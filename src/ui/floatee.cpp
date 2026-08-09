#include "floatee.h"
#include "ui_floatee.h"
#include <QIcon>
#include <QPainter>
#include <QActionGroup>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QProcess>
#include <QFileInfo>
#include <QMessageBox>
#include <cmath>

static int CurrentEye = 0;  // 0=Normal, 1=Happy, 2=Angry, 3=Pain, 4=Surprise

// Fixed zoom levels shared by the Size menu and the mouse-wheel zoom.
// 50%..200% in 10% steps (16 levels). Kept in ONE place so the menu and the
// wheel always stay in sync.
static const QVector<double> kZoomLevels = {
    0.50, 0.60, 0.70, 0.80, 0.90, 1.00, 1.10, 1.20,
    1.30, 1.40, 1.50, 1.60, 1.70, 1.80, 1.90, 2.00,
};

// Fixed window size = the largest zoom level (200%): canvas 192 + headroom 83.
// The window geometry NEVER changes while zooming — zooming re-renders the tee
// at the new scale and moves it INSIDE the window (content-anchored). Changing
// geometry + content together on a layered window lets the DWM composite a
// stale "new geometry + old content" frame for ~100ms (the visible jitter);
// pure content updates (eyes, emoticons, dragging) are stable.
static constexpr int kWinW = 192;      // qRound(96 * 2.0)
// Top reserved band for the over-head emoticon: at 200% the bubble needs
// ceil(41.235 * 2.0) = 83px above the tee. The tee is clamped below this band
// so the bubble is never clipped by the window's top edge.
static constexpr int kEmoticonTop = 83;
static constexpr int kWinH = kWinW + kEmoticonTop;   // 275

void Floatee::Loading()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir);

    // Multi-instance support: each instance can run with its own config via
    // `--profile=<name>` (e.g. "Floatee.exe --profile=blue" uses default_blue.json;
    // no argument → the default default.json). Only letters/digits/-/_ are kept
    // so a profile can never escape the config directory.
    QString profile;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i).startsWith(QLatin1String("--profile="))) {
            profile = args.at(i).mid(10).trimmed();
            break;
        }
    }
    m_profile.clear();
    for (const QChar &c : profile) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            m_profile += c;
    }
    Path_Setup = appDataDir + "/default" + (m_profile.isEmpty() ? "" : "_" + m_profile) + ".json";
    Setup = JsonOpt::File2Json(Path_Setup).object();
    if (!Setup["Setup_Existed"].toBool())
    {
        qDebug() << "SetupFileLoss";
        Setup = QJsonObject();
        Setup.insert("Enable_WindowSideHide", true);
        Setup.insert("Enable_TeEyes", true);
        Setup.insert("Always_on_the_Top", true);
        Setup.insert("Setup_Existed", true);
        JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    }
    // ── online 分支：WSH 与 Eye Care 对联机冗余，强制关闭（托盘入口已屏蔽）──
    // （此改动仅存在于 online 分支，勿合回 main；如需恢复删除本注释即可）
    ExecWindowSideHide.Enabled = false;
    ExecTeEyes.Enabled = false;
    qDebug() << "WSH" << ExecWindowSideHide.Enabled;
    qDebug() << "TES" << ExecTeEyes.Enabled;
}

void Floatee::Initialize()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);

    // Load saved skin preference with per-skin HSL adjustments
    QString savedSkin = Setup.value("Skin").toString();
    QJsonObject skinHsl = Setup.value("SkinHSL").toObject();
    QJsonObject hsl = skinHsl.value(savedSkin).toObject();
    HueShift = hsl.value("HueShift").toInt(0);
    SatFactor = hsl.value("SatFactor").toDouble(1.0);
    LightFactor = hsl.value("LightFactor").toDouble(1.0);
    CurrentEye = qBound(0, Setup.value("Eye").toInt(0), 4);
    if (!savedSkin.isEmpty())
        ExecTeeDrawer.load(savedSkin, HueShift, SatFactor, LightFactor);
    // Apply saved feather strength (edge anti-aliasing) before the first render
    ExecTeeDrawer.setFeatherStrength(qBound(0, Setup.value("Feather").toInt(1), 2));
    // Apply saved zoom before the first render
    SizeScale = qBound(0.5, Setup.value("Size").toDouble(1.0), 2.0);
    ExecTeeDrawer.setRenderScale(SizeScale);
    ExecTeeDrawer.render(CurrentEye, 1.0f, 0.0f);
    LastDirX = 1.0f; LastDirY = 0.0f;
    RenderedEye = CurrentEye;

    // Fixed-size window: zoom never changes the geometry (see paintEvent).
    // The tee starts horizontally centred and vertically at the bottom, below
    // the top band reserved for the over-head emoticon (kEmoticonTop).
    m_teePos = QPointF((kWinW - ExecTeeDrawer.canvasSize()) / 2.0, kEmoticonTop);
    resize(kWinW, kWinH);
    // Multi-instance: nudge non-default profiles so their window doesn't stack
    // exactly on top of the default instance (user can drag it anywhere).
    if (!m_profile.isEmpty())
        move(QPoint(80, 80));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    TrayIcon.setIcon(ExecTeeDrawer.Tee);
    TrayIcon.setToolTip("Floatee");

    TrayMenu = new QMenu();
    AlwaysOnTopAction = TrayMenu->addAction("Always on Top");
    AlwaysOnTopAction->setCheckable(true);
    AlwaysOnTopAction->setChecked(Setup["Always_on_the_Top"].toBool());
    connect(AlwaysOnTopAction, &QAction::triggered, this, &Floatee::toggleAlwaysOnTop);

    // ── online 分支：WSH(Window Side Hide) 与 Eye Care 对联机冗余，屏蔽托盘入口 ──
    // （此改动仅存在于 online 分支，勿合回 main；如需恢复删除本注释块即可）
    // WindowSideHideAction = TrayMenu->addAction("Window Side Hide");
    // WindowSideHideAction->setCheckable(true);
    // WindowSideHideAction->setChecked(Setup["Enable_WindowSideHide"].toBool());
    // connect(WindowSideHideAction, &QAction::triggered, this, &Floatee::toggleWindowSideHide);
    //
    // TeEyesAction = TrayMenu->addAction("Eye Care");
    // TeEyesAction->setCheckable(true);
    // TeEyesAction->setChecked(Setup["Enable_TeEyes"].toBool());
    // connect(TeEyesAction, &QAction::triggered, this, &Floatee::toggleTeEyes);

    // ── Eye submenu ──────────────────────────────────────────────────
    // CurrentEye already loaded from default.json above

    QVector<QPair<QString, int>> eyeTypes = {
        {"Normal", 0}, {"Happy", 1}, {"Angry", 2}, {"Pain", 3}, {"Surprise", 4},
    };

    EyeMenu = new QMenu("Eyes");
    EyeGroup = new QActionGroup(EyeMenu);
    EyeGroup->setExclusive(true);

    for (const auto &[name, idx] : eyeTypes) {
        QAction *action = EyeMenu->addAction(name);
        action->setCheckable(true);
        action->setData(idx);
        action->setChecked(idx == CurrentEye);
        EyeGroup->addAction(action);
    }
    connect(EyeMenu, &QMenu::triggered, this, &Floatee::switchEye);

    // ── Size submenu (zoom) ─────────────────────────────────────────
    // Levels come from the shared kZoomLevels table (same list as the wheel).
    SizeMenu = new QMenu("Size");
    SizeGroup = new QActionGroup(SizeMenu);
    SizeGroup->setExclusive(true);
    for (const double s : kZoomLevels) {
        QAction *action = SizeMenu->addAction(QString::number(qRound(s * 100.0)) + "%");
        action->setCheckable(true);
        action->setData(s);
        action->setChecked(qFuzzyCompare(s, SizeScale));
        SizeGroup->addAction(action);
    }
    connect(SizeMenu, &QMenu::triggered, this, &Floatee::switchSize);

    // ── Feather submenu (edge anti-aliasing strength) ──────────────
    FeatherMenu = new QMenu("Feather");
    FeatherGroup = new QActionGroup(FeatherMenu);
    FeatherGroup->setExclusive(true);
    const QVector<QPair<QString, int>> featherOptions = {
        {"Off", 0}, {"Normal", 1}, {"Strong", 2},
    };
    for (const auto &[name, f] : featherOptions) {
        QAction *action = FeatherMenu->addAction(name);
        action->setCheckable(true);
        action->setData(f);
        action->setChecked(f == ExecTeeDrawer.featherStrength());
        FeatherGroup->addAction(action);
    }
    connect(FeatherMenu, &QMenu::triggered, this, &Floatee::switchFeather);

    QAction *colorAction = TrayMenu->addAction("Color Adjust...");
    connect(colorAction, &QAction::triggered, this, &Floatee::openColorDialog);

    // ── Skin submenu ────────────────────────────────────────────────
    QVector<QPair<QString, QString>> skins = {
        {"Tata",               ":/skins/Tata.png"},
        {"Tataa",              ":/skins/Tataa.png"},
        {"Chinese By Whis",    ":/skins/chinese_by_whis.png"},
        {"Coala Pinky",        ":/skins/coala_pinky.png"},
        {"Mouse",              ":/skins/mouse.png"},
        {"Santa Bluekitty",    ":/skins/santa_bluekitty.png"},
        {"Flower Crown Ghost", ":/skins/flower_crown_ghost.png"},
        {"Ghost Halloween",    ":/skins/ghost_halloween.png"},
        {"Ghost Zeeli",        ":/skins/ghost_zeeli.png"},
    };

    CurrentSkin = Setup.value("Skin").toString(TeeDrawer::defaultSkinPath());
    if (!QFile::exists(CurrentSkin))
        CurrentSkin = TeeDrawer::defaultSkinPath();

    SkinMenu = new QMenu("Skin");
    SkinGroup = new QActionGroup(SkinMenu);
    SkinGroup->setExclusive(true);

    for (const auto &[name, path] : skins) {
        QAction *action = SkinMenu->addAction(name);
        action->setCheckable(true);
        action->setData(path);
        action->setChecked(path == CurrentSkin);
        SkinGroup->addAction(action);
    }

    // ── External skins from local skins/ folder ─────────────────────
    const QString skinsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/skins";
    QDirIterator it(skinsDir, {"*.png"}, QDir::Files);
    bool hasExternal = false;
    while (it.hasNext()) {
        it.next();
        if (!hasExternal) {
            SkinMenu->addSeparator();
            hasExternal = true;
        }
        QAction *action = SkinMenu->addAction(it.fileInfo().completeBaseName());
        action->setCheckable(true);
        action->setData(it.filePath());
        action->setChecked(it.filePath() == CurrentSkin);
        SkinGroup->addAction(action);
    }

    SkinMenu->addSeparator();
    QAction *openSkinFolder = SkinMenu->addAction("Open Skins Folder");
    connect(openSkinFolder, &QAction::triggered, this, [skinsDir]() {
        QDir().mkpath(skinsDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(skinsDir));
    });

    connect(SkinMenu, &QMenu::triggered, this, &Floatee::switchSkin);

    TrayMenu->addMenu(EyeMenu);
    TrayMenu->addMenu(SizeMenu);
    TrayMenu->addMenu(FeatherMenu);
    TrayMenu->addMenu(SkinMenu);

    // ── Instance submenu: configs + multi-instance management ─────
    buildInstanceMenu();

    // ── online 分支：网络通信测试入口（连本地服务器 → hello → create_room）──
    TrayMenu->addSeparator();
    QAction *netTestAction = TrayMenu->addAction("Network Test...");
    connect(netTestAction, &QAction::triggered, this, &Floatee::networkTest);

    TrayMenu->addSeparator();
    QAction *quitAction = TrayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    TrayIcon.setContextMenu(TrayMenu);
    TrayIcon.show();
    connect(&TrayIcon, &QSystemTrayIcon::activated, this, &Floatee::on_systemTrayActivated);

    ExecTeEyes.Tee = this;
    ExecWindowSideHide.Tee = this;

    // Cursor-driven eye follow: re-render the full tee with the look direction
    // pointing at the cursor (the pipeline draws the eyes on the body, so there
    // is no separate eye QLabel anymore).
    EyeFollowTimer = new QTimer(this);
    EyeFollowTimer->setInterval(16);
    connect(EyeFollowTimer, &QTimer::timeout, this, &Floatee::updateEyeFollow);
    EyeFollowTimer->start();

    // Over-head emoticons: standalone logic component + random trigger timer.
    // Its frame is drawn by paintEvent into this (single) window, because a
    // second translucent layered window is never composited on this setup.
    EmoticonWin = new EmoticonWindow(this);
    if (!EmoticonWin->loadAtlas(QPixmap(QStringLiteral(":/main/emoticons.png"))))
        qWarning() << "Floatee: failed to load emoticon atlas";
    connect(EmoticonWin, &EmoticonWindow::frameChanged, this, qOverload<>(&QWidget::update));
    EmoticonRandomTimer = new QTimer(this);
    EmoticonRandomTimer->setInterval(10000);   // check every 10s
    connect(EmoticonRandomTimer, &QTimer::timeout, this, &Floatee::onRandomEmoticonTick);
    EmoticonRandomTimer->start();

    if (Setup["Always_on_the_Top"].toBool())
    {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }
}

Floatee::Floatee(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Floatee)
{
    ui->setupUi(this);
    Loading();
    Initialize();
}

Floatee::~Floatee()
{
    delete ui;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        MousePress = true;
        MousePoint = event->globalPosition().toPoint() - this->pos();
        // Start walking while dragging (phase from horizontal position)
        WalkPhase = std::fmod(double(this->pos().x()), 100.0) / 100.0f;
        if (WalkPhase < 0.0f) WalkPhase += 1.0f;
        LastWalkPhase = -2.0f;   // force re-render
        updateEyeFollow();
        triggerRandomEmoticon();   // drag started
    }
    else if (event->button() == Qt::RightButton) {
        CurrentEye = (CurrentEye + 1) % 5;
        RenderedEye = -1;             // force re-render with the new eye
        updateEyeFollow();
        triggerRandomEmoticon();      // eye switched
        // Sync menu checkmark
        if (EyeGroup && EyeGroup->actions().size() > CurrentEye)
            EyeGroup->actions()[CurrentEye]->setChecked(true);
    }
    QMainWindow::mousePressEvent(event);
}

void Floatee::mouseMoveEvent(QMouseEvent *event)
{
    if (MousePress) {
        move(event->globalPosition().toPoint() - MousePoint);
        // Walk-cycle phase follows the horizontal position (DDNet formula).
        WalkPhase = std::fmod(double(this->pos().x()), 100.0) / 100.0f;
        if (WalkPhase < 0.0f) WalkPhase += 1.0f;
        LastWalkPhase = -2.0f;   // force re-render with the new phase
        updateEyeFollow();
    }
    QMainWindow::mouseMoveEvent(event);
}

void Floatee::mouseReleaseEvent(QMouseEvent *event)
{
    MousePress = false;
    WalkPhase = -1.0f;   // stop walking, back to idle
    LastWalkPhase = -2.0f;
    updateEyeFollow();
    QMainWindow::mouseReleaseEvent(event);
}

void Floatee::changeEvent(QEvent *event)
{
    // On Windows, translucent (WA_TranslucentBackground) frameless tool windows
    // can have their alpha compositing go stale — semi-transparent skin pixels
    // appear faded/washed out — after the window loses (or regains) focus.
    // Refresh only when the real activation state flips: some apps (Chromium /
    // Electron, e.g. VS Code) periodically emit activation-related events that
    // would otherwise trigger this repeatedly and cause flickering.
    if (event->type() == QEvent::ActivationChange) {
        const bool active = isActiveWindow();
        if (active != m_wasActive) {
            m_wasActive = active;
            refreshTranslucentDisplay();
        }
    }
    QMainWindow::changeEvent(event);
}

void Floatee::refreshTranslucentDisplay()
{
    RenderedEye = -1;          // force a fresh render + repaint
    updateEyeFollow();
    // Lightweight refresh: repaint re-syncs the layered-window alpha to the DWM.
    // Deliberately does NOT re-assert WA_TranslucentBackground — re-applying it
    // on an already-translucent window re-styles the native WS_EX_LAYERED flag
    // and can make the window flicker between transparent and opaque.
    repaint();
}

void Floatee::paintEvent(QPaintEvent *event)
{
    // Paint the tee directly into the translucent top-level window — the
    // canonical reliable pattern for alpha compositing. The window has a FIXED
    // size; the tee is drawn at m_teePos (content-anchored zoom), so zooming
    // never changes the window geometry — only the tee's in-window size and
    // position — a pure content update that is stable on a layered window.
    const int cs = ExecTeeDrawer.canvasSize();
    const float ts = ExecTeeDrawer.teeSize();
    QPainter p(this);
    p.drawPixmap(qRound(m_teePos.x()), qRound(m_teePos.y()), ExecTeeDrawer.Tee);
    if (EmoticonWin && EmoticonWin->isActive()) {
        QPixmap frame(width(), height());
        frame.fill(Qt::transparent);
        const QPointF teeCenter = m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
        const bool ok = EmoticonWin->renderFrame(frame, teeCenter);
        if (ok)
            p.drawPixmap(0, 0, frame);
    }
    QMainWindow::paintEvent(event);
}

void Floatee::updateEyeFollow()
{
    // Look direction: from the tee area center toward the cursor (the window is
    // taller than the tee canvas to leave headroom for the over-head bubble).
    const QPoint g = QCursor::pos();
    const QPointF c(pos().x() + m_teePos.x() + ExecTeeDrawer.canvasSize() / 2.0,
                    pos().y() + m_teePos.y() + ExecTeeDrawer.canvasSize() / 2.0);
    QPointF d = QPointF(g) - c;
    const float len = std::hypot(d.x(), d.y());
    float dirX = 1.0f, dirY = 0.0f;
    if (len > 1.0f) {
        dirX = d.x() / len;
        dirY = d.y() / len;
    }

    // Distance-based eye travel. Outside the tee it keeps the original
    // sublinear (√-like) response that saturates at ~800px. New: inside/on the
    // tee the eyes are NO longer perfectly centred — they track the cursor with
    // a gentle ramp from 0 (cursor at the exact centre) up to kNearTravel at
    // the tee edge, so the pet stays "alive" even while the cursor is on it.
    // The "tee" radius scales with the pet zoom; the eye travel itself scales
    // with the tee size in the pipeline, so the look stays consistent at every
    // zoom level.
    float eyeScale = 0.0f;
    const float kOnRadius = 35.0f * SizeScale;   // tee radius (near-zone ramp)
    constexpr float kTravelC = 6.0f;             // px travel ≈ sqrt(dist/kTravelC)
    constexpr float kMaxTravel = 1.2f;           // ≈ 0.125*72*1.2 ≈ 10.8px cap
    constexpr float kNearTravel = 0.45f;         // eyeScale at the tee edge (~4px)
    if (len <= kOnRadius) {
        // Cursor on the tee: ramp 0 → kNearTravel so the eyes still follow.
        eyeScale = kNearTravel * (len / kOnRadius);
    } else {
        // Outside the tee: same √-like growth as before, continuing from the
        // near-zone value instead of from 0.
        const float travelPx = std::sqrt((len - kOnRadius) / kTravelC);
        eyeScale = kNearTravel + travelPx / 9.0f;
        if (eyeScale > kMaxTravel)
            eyeScale = kMaxTravel;
    }

    int eye = CurrentEye;
    // Classic Floatee behaviour: when the cursor hovers near the top of the tee
    // and the default eyes are active, switch to a happy face.
    const bool petting = (eye == 0 && len < 45.0f * SizeScale && d.y() < 0.0f);
    if (petting) {
        eye = 1;
        // Edge-trigger the hearts emoticon once when the cursor starts petting.
        if (!m_petting)
            showEmoticonOnTee(teer::EMOTICON_HEARTS);
    }
    m_petting = petting;

    // Skip re-render when nothing (eye, direction, eye travel or walk phase) changed.
    if (eye == RenderedEye &&
        std::abs(dirX - LastDirX) < 0.04f &&
        std::abs(dirY - LastDirY) < 0.04f &&
        std::abs(eyeScale - LastEyeScale) < 0.02f &&
        std::abs(WalkPhase - LastWalkPhase) < 0.004f)
        return;

    ExecTeeDrawer.render(eye, dirX, dirY, WalkPhase, eyeScale);
    RenderedEye = eye;
    LastDirX = dirX;
    LastDirY = dirY;
    LastEyeScale = eyeScale;
    LastWalkPhase = WalkPhase;
    update();
}

void Floatee::triggerRandomEmoticon()
{
    const int idx = QRandomGenerator::global()->bounded(teer::NUM_EMOTICONS);
    showEmoticonOnTee(idx);
}

void Floatee::showEmoticonOnTee(int index)
{
    if (!EmoticonWin)
        return;
    const int cs = ExecTeeDrawer.canvasSize();
    const float ts = ExecTeeDrawer.teeSize();
    const QPointF teeCenter = m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
    EmoticonWin->showEmoticon(index, ts, teeCenter);
}

void Floatee::onRandomEmoticonTick()
{
    // Every 10s, with ~50% probability, show a random emoticon.
    if (QRandomGenerator::global()->bounded(100) < 50)
        triggerRandomEmoticon();
}

void Floatee::on_systemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::DoubleClick:
        qApp->quit();
        break;
    default:
        break;
    }
}

void Floatee::toggleAlwaysOnTop()
{
    bool on = AlwaysOnTopAction->isChecked();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    show();

    Setup["Always_on_the_Top"] = on;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::toggleWindowSideHide()
{
    bool on = WindowSideHideAction->isChecked();
    ExecWindowSideHide.Enabled = on;
    Setup["Enable_WindowSideHide"] = on;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::toggleTeEyes()
{
    bool on = TeEyesAction->isChecked();
    ExecTeEyes.Enabled = on;
    Setup["Enable_TeEyes"] = on;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::switchSkin(QAction *action)
{
    QString path = action->data().toString();
    if (path.isEmpty() || path == CurrentSkin)
        return;

    // Load per-skin HSL for the new skin
    QJsonObject skinHsl = Setup.value("SkinHSL").toObject();
    QJsonObject hsl = skinHsl.value(path).toObject();
    HueShift = hsl.value("HueShift").toInt(0);
    SatFactor = hsl.value("SatFactor").toDouble(1.0);
    LightFactor = hsl.value("LightFactor").toDouble(1.0);

    ExecTeeDrawer.load(path, HueShift, SatFactor, LightFactor);
    CurrentSkin = path;

    const bool wasVisible = isVisible();
    if (wasVisible)
        hide();

    RenderedEye = -1;
    updateEyeFollow();   // re-renders the full tee with current eye + cursor dir
    TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    if (wasVisible)
        show();

    Setup["Skin"] = path;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::switchEye(QAction *action)
{
    int idx = action->data().toInt();
    if (idx == CurrentEye)
        return;

    CurrentEye = idx;
    RenderedEye = -1;
    updateEyeFollow();
    triggerRandomEmoticon();   // eye switched

    Setup["Eye"] = idx;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::switchSize(QAction *action)
{
    if (applySizeScale(action->data().toDouble()))
        triggerRandomEmoticon();   // size switched (menu path only)
}

void Floatee::switchFeather(QAction *action)
{
    const int f = qBound(0, action->data().toInt(), 2);
    if (f == ExecTeeDrawer.featherStrength())
        return;
    ExecTeeDrawer.setFeatherStrength(f);
    RenderedEye = -1;
    updateEyeFollow();           // re-render with the new feather strength
    TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    Setup["Feather"] = f;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::networkTest()
{
    // online 分支：连本地服务器(TCP 8764) → hello → create_room，弹窗显示结果。
    if (!m_net) {
        m_net = new NetClient(this);
        connect(m_net, &NetClient::connected, this, [this]() {
            m_net->sendJson(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("hello")},
                {QStringLiteral("clientId"), QStringLiteral("floatee-test")},
                {QStringLiteral("deviceId"), QStringLiteral("floatee-test-device")},
                {QStringLiteral("displayName"), QStringLiteral("FloateeTest")},
            });
        });
        connect(m_net, &NetClient::messageReceived, this, [this](const QJsonObject &msg) {
            const QString type = msg.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("welcome")) {
                m_net->sendJson(QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("create_room")},
                    {QStringLiteral("roomName"), QStringLiteral("floatee-test-room")},
                });
            } else if (type == QLatin1String("room_created")) {
                QMessageBox::information(this, QStringLiteral("Network Test"),
                    QStringLiteral("已连接服务器并创建房间:\n房间号: %1\n邀请码: %2")
                        .arg(msg.value(QStringLiteral("roomId")).toString(),
                             msg.value(QStringLiteral("joinCode")).toString()));
                m_net->disconnectFromServer();
            } else if (type == QLatin1String("error")) {
                QMessageBox::warning(this, QStringLiteral("Network Test"),
                    QStringLiteral("服务器错误: %1").arg(msg.value(QStringLiteral("message")).toString()));
            }
        });
        connect(m_net, &NetClient::errorOccurred, this, [this](const QString &e) {
            QMessageBox::warning(this, QStringLiteral("Network Test"),
                QStringLiteral("连接失败: %1").arg(e));
        });
    }
    m_net->connectToServer(QStringLiteral("127.0.0.1"), 8764);
}

void Floatee::launchNewInstance()
{
    // One-click multi-instance: launch a NEW PROCESS with the DEFAULT config
    // (no --profile → default.json), so users can quickly open another pet
    // side by side. Creating a fresh profile has its own entry (Custom Profile...).
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
}

void Floatee::openNewInstance()
{
    // Launch a fresh Floatee with its own profile config from the tray menu,
    // so no command line is needed. Suggest a unique default name; any running
    // instance can create more (recursive multi-instance).
    const QString defaultName = QStringLiteral("inst%1")
                                    .arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("New Floatee Instance"),
        QStringLiteral("Instance name (letters / digits / - / _):"),
        QLineEdit::Normal, defaultName, &ok);
    if (!ok)
        return;
    // Sanitize exactly like Loading() so the profile can't escape the config dir.
    QString clean;
    for (const QChar &c : name.trimmed()) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            clean += c;
    }
    if (clean.isEmpty())
        return;
    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            QStringList{QStringLiteral("--profile=") + clean});
}

void Floatee::openConfigFolder()
{
    const QString dir = QFileInfo(Path_Setup).absolutePath();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void Floatee::buildInstanceMenu()
{
    if (!InstanceMenu) {
        InstanceMenu = new QMenu("Instance");
        TrayMenu->addMenu(InstanceMenu);
    }
    disconnect(InstanceMenu, nullptr, this, nullptr);   // drop stale connections
    InstanceMenu->clear();
    delete InstanceGroup;
    InstanceGroup = new QActionGroup(InstanceMenu);
    InstanceGroup->setExclusive(true);

    // One checkable entry per config file (both the current default*.json
    // naming and the legacy setup*.json are listed, so old configs can still
    // be read/switched without any special handling); the one this process
    // uses is checked.
    const QString dir = QFileInfo(Path_Setup).absolutePath();
    const QString cur = QFileInfo(Path_Setup).fileName();
    const QStringList files =
        QDir(dir).entryList({QStringLiteral("default*.json"), QStringLiteral("setup*.json")},
                            QDir::Files, QDir::Name);
    if (files.isEmpty())
        InstanceMenu->addAction(QStringLiteral("(no configs)"))->setEnabled(false);
    for (const QString &f : files) {
        QAction *a = InstanceMenu->addAction(f);
        a->setCheckable(true);
        a->setData(f);
        a->setChecked(f == cur);
        InstanceGroup->addAction(a);
    }

    InstanceMenu->addSeparator();
    QAction *launchAction = InstanceMenu->addAction("Launch New Instance");
    connect(launchAction, &QAction::triggered, this, &Floatee::launchNewInstance);
    QAction *customAction = InstanceMenu->addAction("Custom Profile...");
    connect(customAction, &QAction::triggered, this, &Floatee::openNewInstance);
    QAction *cfgFolderAction = InstanceMenu->addAction("Open Config Folder");
    connect(cfgFolderAction, &QAction::triggered, this, &Floatee::openConfigFolder);
    connect(InstanceMenu, &QMenu::triggered, this, &Floatee::switchConfig);
}

void Floatee::switchConfig(QAction *action)
{
    const QString fileName = action->data().toString();
    if (fileName.isEmpty())
        return;                    // separator / New Instance / Open Config Folder
    const QString newPath = QFileInfo(Path_Setup).absolutePath() + "/" + fileName;
    if (QFileInfo(newPath) == QFileInfo(Path_Setup))
        return;                    // already using this config
    // Load the clicked config into THIS process and re-apply all settings.
    Setup = JsonOpt::File2Json(newPath).object();
    Path_Setup = newPath;
    applyLiveConfig();
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));   // ensure the file exists
    buildInstanceMenu();           // refresh the checkmark
}

void Floatee::applyLiveConfig()
{
    // Re-apply every setting from the current Setup at runtime (used when the
    // active config file is switched from the Instance menu).
    const QString skin = Setup.value("Skin").toString();
    QJsonObject skinHsl = Setup.value("SkinHSL").toObject();
    QJsonObject hsl = skinHsl.value(skin).toObject();
    HueShift = hsl.value("HueShift").toInt(0);
    SatFactor = hsl.value("SatFactor").toDouble(1.0);
    LightFactor = hsl.value("LightFactor").toDouble(1.0);
    if (!skin.isEmpty() && QFile::exists(skin)) {
        CurrentSkin = skin;
        ExecTeeDrawer.load(skin, HueShift, SatFactor, LightFactor);
    }
    CurrentEye = qBound(0, Setup.value("Eye").toInt(0), 4);
    ExecTeeDrawer.setFeatherStrength(qBound(0, Setup.value("Feather").toInt(1), 2));
    SizeScale = qBound(0.5, Setup.value("Size").toDouble(1.0), 2.0);
    ExecTeeDrawer.setRenderScale(SizeScale);
    m_teePos = QPointF((kWinW - ExecTeeDrawer.canvasSize()) / 2.0, kEmoticonTop);

    ExecWindowSideHide.Enabled = Setup.value("Enable_WindowSideHide").toBool();
    ExecTeEyes.Enabled = Setup.value("Enable_TeEyes").toBool();
    const bool onTop = Setup.value("Always_on_the_Top").toBool();
    setWindowFlag(Qt::WindowStaysOnTopHint, onTop);

    // Sync tray menu checkmarks
    if (AlwaysOnTopAction) AlwaysOnTopAction->setChecked(onTop);
    if (WindowSideHideAction) WindowSideHideAction->setChecked(ExecWindowSideHide.Enabled);
    if (TeEyesAction) TeEyesAction->setChecked(ExecTeEyes.Enabled);
    if (SkinGroup) {
        for (QAction *a : SkinGroup->actions())
            a->setChecked(a->data().toString() == CurrentSkin);
    }
    if (EyeGroup && EyeGroup->actions().size() > CurrentEye)
        EyeGroup->actions()[CurrentEye]->setChecked(true);
    if (SizeGroup) {
        for (QAction *a : SizeGroup->actions())
            a->setChecked(qFuzzyCompare(a->data().toDouble(), SizeScale));
    }
    if (FeatherGroup) {
        for (QAction *a : FeatherGroup->actions())
            a->setChecked(a->data().toInt() == ExecTeeDrawer.featherStrength());
    }

    // Re-render + icons
    RenderedEye = -1;
    updateEyeFollow();
    TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));
}

bool Floatee::applySizeScale(double scale, bool anchorAtCursor)
{
    const double s = qBound(0.5, scale, 2.0);
    if (qFuzzyCompare(s, SizeScale))
        return false;

    // Content-anchored zoom: the window geometry NEVER changes (fixed kWinW×
    // kWinH) — zooming only re-renders the tee at the new scale and moves it
    // INSIDE the window so the tee point under the cursor stays under it. This
    // is a pure content update: stable on a layered window (unlike geometry +
    // content changes, which let the DWM composite a ~100ms stale frame).
    const int oldCs = ExecTeeDrawer.canvasSize();
    const QPointF winPos = QPointF(pos());
    const QPointF mouse = QCursor::pos();

    SizeScale = s;
    ExecTeeDrawer.setRenderScale(SizeScale);
    const int newCs = ExecTeeDrawer.canvasSize();

    if (anchorAtCursor) {
        // Normalised anchor inside the (old) tee canvas, then place the new
        // canvas so that same tee point stays under the cursor. Clamp so the
        // tee never leaves the window and never enters the top band reserved
        // for the over-head emoticon (kEmoticonTop) — otherwise the bubble
        // would be clipped by the window's top edge.
        const QPointF anchorRel = (mouse - (winPos + m_teePos)) / oldCs;
        QPointF newPos = (mouse - winPos) - anchorRel * newCs;
        newPos.setX(qBound(0.0, newPos.x(), double(kWinW - newCs)));
        newPos.setY(qBound(double(kEmoticonTop), newPos.y(),
                           double(kWinH - newCs)));
        m_teePos = newPos;
    } else {
        // Menu path: horizontally centred, vertically at the bottom below the
        // emoticon band.
        m_teePos = QPointF((kWinW - newCs) / 2.0, kEmoticonTop);
    }

    RenderedEye = -1;
    updateEyeFollow();           // re-render the tee at the new scale
    repaint();                   // content-only update, painted synchronously

    TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    // Sync the Size menu checkmark (a menu click relies on the QActionGroup's
    // exclusivity; wheel-zoom needs the check updated manually).
    if (SizeGroup) {
        for (QAction *a : SizeGroup->actions())
            a->setChecked(qFuzzyCompare(a->data().toDouble(), s));
    }

    Setup["Size"] = SizeScale;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    return true;
}

void Floatee::zoomSize(int step)
{
    // Snap the current scale to the nearest shared level, then move one step —
    // so a wheel event always lands on an exact menu level, never in between.
    int idx = 0;
    double best = 1e9;
    for (int i = 0; i < kZoomLevels.size(); ++i) {
        const double d = std::abs(kZoomLevels[i] - SizeScale);
        if (d < best) { best = d; idx = i; }
    }
    const int next = idx + step;
    if (next < 0 || next >= kZoomLevels.size())
        return;                  // already at the min/max level
    applySizeScale(kZoomLevels[next], /*anchorAtCursor=*/true);
}

void Floatee::wheelEvent(QWheelEvent *event)
{
    // Mouse-wheel zoom. Deliberately NOT "delta / 120 steps": one wheel event
    // moves exactly one level (even if a fast/high-resolution wheel bundles
    // several notches of delta), so the zoom stays calm and predictable.
    const int dy = event->angleDelta().y();
    if (dy > 0)
        zoomSize(+1);            // scroll up → zoom in
    else if (dy < 0)
        zoomSize(-1);            // scroll down → zoom out
    event->accept();
}

void Floatee::openColorDialog()
{
    const int origHue = HueShift;
    const double origSat = SatFactor;
    const double origLight = LightFactor;

    QDialog dlg(this);
    dlg.setWindowTitle("Color Adjust");

    auto *hueSlider = new QSlider(Qt::Horizontal);
    hueSlider->setRange(-180, 180);
    hueSlider->setValue(HueShift);
    auto *hueLabel = new QLabel(QString("Hue: %1").arg(HueShift));

    auto *satSlider = new QSlider(Qt::Horizontal);
    satSlider->setRange(0, 200);
    satSlider->setValue(qRound(SatFactor * 100));
    auto *satLabel = new QLabel(QString("Saturation: %1%").arg(qRound(SatFactor * 100)));

    auto *lightSlider = new QSlider(Qt::Horizontal);
    lightSlider->setRange(0, 200);
    lightSlider->setValue(qRound(LightFactor * 100));
    auto *lightLabel = new QLabel(QString("Lightness: %1%").arg(qRound(LightFactor * 100)));

    auto updateLabels = [&]() {
        hueLabel->setText(QString("Hue: %1").arg(hueSlider->value()));
        satLabel->setText(QString("Saturation: %1%").arg(satSlider->value()));
        lightLabel->setText(QString("Lightness: %1%").arg(lightSlider->value()));
    };
    connect(hueSlider, &QSlider::valueChanged, this, updateLabels);
    connect(satSlider, &QSlider::valueChanged, this, updateLabels);
    connect(lightSlider, &QSlider::valueChanged, this, updateLabels);

    auto apply = [&]() {
        int h = hueSlider->value();
        double s = satSlider->value() / 100.0;
        double l = lightSlider->value() / 100.0;
        ExecTeeDrawer.load(CurrentSkin, h, s, l);
        RenderedEye = -1;
        updateEyeFollow();
        TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
        setWindowIcon(QIcon(ExecTeeDrawer.Tee));
    };

    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(hueLabel);
    layout->addWidget(hueSlider);
    layout->addWidget(satLabel);
    layout->addWidget(satSlider);
    layout->addWidget(lightLabel);
    layout->addWidget(lightSlider);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *applyBtn = btnBox->addButton("Apply", QDialogButtonBox::ApplyRole);
    layout->addWidget(btnBox);
    connect(applyBtn, &QPushButton::clicked, this, apply);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        // Revert to original
        ExecTeeDrawer.load(CurrentSkin, origHue, origSat, origLight);
        RenderedEye = -1;
        updateEyeFollow();
        TrayIcon.setIcon(QIcon(ExecTeeDrawer.Tee));
        setWindowIcon(QIcon(ExecTeeDrawer.Tee));
        return;
    }

    HueShift = hueSlider->value();
    SatFactor = satSlider->value() / 100.0;
    LightFactor = lightSlider->value() / 100.0;
    apply();

    // Save per-skin HSL
    QJsonObject skinHsl = Setup.value("SkinHSL").toObject();
    QJsonObject hsl;
    hsl["HueShift"] = HueShift;
    hsl["SatFactor"] = SatFactor;
    hsl["LightFactor"] = LightFactor;
    skinHsl[CurrentSkin] = hsl;
    Setup["SkinHSL"] = skinHsl;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}
