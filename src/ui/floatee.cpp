#include "floatee.h"
#include "ui_floatee.h"
#include <QIcon>
#include <QActionGroup>
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
#include <cmath>

static int CurrentEye = 0;  // 0=Normal, 1=Happy, 2=Angry, 3=Pain, 4=Surprise

void Floatee::Loading()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir);
    Path_Setup = appDataDir + "/setup.json";
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
    ExecWindowSideHide.Enabled = Setup["Enable_WindowSideHide"].toBool();
    ExecTeEyes.Enabled = Setup["Enable_TeEyes"].toBool();
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    ExecWindowSideHide.Enabled = false;
#endif
    qDebug() << "WSH" << ExecWindowSideHide.Enabled;
    qDebug() << "TES" << ExecTeEyes.Enabled;
}

void Floatee::Initialize()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
    resize(96, 96);

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
    ExecTeeDrawer.render(CurrentEye, 1.0f, 0.0f);
    LastDirX = 1.0f; LastDirY = 0.0f;
    RenderedEye = CurrentEye;

    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    BodyLabel = new QLabel(this);
    BodyLabel->setGeometry(0, 0, 96, 96);
    BodyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    BodyLabel->setPixmap(ExecTeeDrawer.Tee);

    TrayIcon.setIcon(ExecTeeDrawer.Tee);
    TrayIcon.setToolTip("Floatee");

    TrayMenu = new QMenu();
    AlwaysOnTopAction = TrayMenu->addAction("Always on Top");
    AlwaysOnTopAction->setCheckable(true);
    AlwaysOnTopAction->setChecked(Setup["Always_on_the_Top"].toBool());
    connect(AlwaysOnTopAction, &QAction::triggered, this, &Floatee::toggleAlwaysOnTop);

    WindowSideHideAction = TrayMenu->addAction("Window Side Hide");
    WindowSideHideAction->setCheckable(true);
    WindowSideHideAction->setChecked(Setup["Enable_WindowSideHide"].toBool());
    connect(WindowSideHideAction, &QAction::triggered, this, &Floatee::toggleWindowSideHide);

    TeEyesAction = TrayMenu->addAction("Eye Care");
    TeEyesAction->setCheckable(true);
    TeEyesAction->setChecked(Setup["Enable_TeEyes"].toBool());
    connect(TeEyesAction, &QAction::triggered, this, &Floatee::toggleTeEyes);

    // ── Eye submenu ──────────────────────────────────────────────────
    // CurrentEye already loaded from setup.json above

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
    TrayMenu->addMenu(SkinMenu);
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
    }
    else if (event->button() == Qt::RightButton) {
        CurrentEye = (CurrentEye + 1) % 5;
        RenderedEye = -1;             // force re-render with the new eye
        updateEyeFollow();
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
    }
    QMainWindow::mouseMoveEvent(event);
}

void Floatee::mouseReleaseEvent(QMouseEvent *event)
{
    MousePress = false;
    QMainWindow::mouseReleaseEvent(event);
}

void Floatee::updateEyeFollow()
{
    // Look direction: from the tee window center toward the cursor.
    const QPoint g = QCursor::pos();
    const QPointF c = geometry().center();
    QPointF d = QPointF(g) - c;
    const float len = std::hypot(d.x(), d.y());
    float dirX = 1.0f, dirY = 0.0f;
    if (len > 1.0f) {
        dirX = d.x() / len;
        dirY = d.y() / len;
    }

    int eye = CurrentEye;
    // Classic Floatee behaviour: when the cursor hovers near the top of the tee
    // and the default eyes are active, switch to a happy face.
    if (eye == 0 && len < 45.0f && d.y() < 0.0f)
        eye = 1;

    // Skip re-render when nothing (eye or direction) changed.
    if (eye == RenderedEye &&
        std::abs(dirX - LastDirX) < 0.04f &&
        std::abs(dirY - LastDirY) < 0.04f)
        return;

    ExecTeeDrawer.render(eye, dirX, dirY);
    RenderedEye = eye;
    LastDirX = dirX;
    LastDirY = dirY;
    BodyLabel->setPixmap(ExecTeeDrawer.Tee);
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

    Setup["Eye"] = idx;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
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
