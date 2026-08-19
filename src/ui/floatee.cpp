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
#include <QInputMethod>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QFileInfo>
#include <QMessageBox>
#include <QUuid>
#include <QDateTime>
#include <QTextStream>
#include <cmath>

#include "style/theme.h"
#include "style/elwidgets.h"
#include "style/elmessagebar.h"
#include "settingswindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

// ── 临时诊断：窗口操作日志（定位任务栏遮挡问题，定位后移除）──
static void dbgWin(const QString &msg)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QLatin1String("/window_debug.log");
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentMSecsSinceEpoch() << QLatin1Char(' ') << msg << Qt::endl;
        f.close();
    }
}

static QString rectStr(double x, double y, double w, double h)
{
    return QStringLiteral("(%1,%2 %3x%4)").arg(x).arg(y).arg(w).arg(h);
}

#ifdef Q_OS_WIN
static QString exStyleStr(LONG_PTR ex)
{
    QString s;
    if (ex & WS_EX_TOPMOST) s += QStringLiteral(" TOPMOST");
    if (ex & WS_EX_LAYERED) s += QStringLiteral(" LAYERED");
    if (ex & WS_EX_TRANSPARENT) s += QStringLiteral(" TRANSPARENT");
    if (ex & WS_EX_TOOLWINDOW) s += QStringLiteral(" TOOLWINDOW");
    return s.trimmed().isEmpty() ? QStringLiteral("none") : s.trimmed();
}
#endif

static int CurrentEye = 0;  // 0=Normal, 1=Happy, 2=Angry, 3=Pain, 4=Surprise, 5=Blink

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
    // NoDropShadowWindowHint：macOS 透明窗口的阴影（围绕非透明内容生成）
    // 在内容移动后可能残留（WindowServer 合成缓存旧帧 → 灰色 Tee 轮廓残影）。
    // 仅 macOS 需要；Windows 的阴影由 DWM 提供且无此合成残留问题，保持原有
    // flags 以免改变窗口外观/行为。
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint);
    dbgWin(QStringLiteral("[init] setWindowFlags FramelessWindowHint|Tool|NoDropShadow"));
#else
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    dbgWin(QStringLiteral("[init] setWindowFlags FramelessWindowHint|Tool"));
#endif
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
    // 消息输入框需要键盘焦点 + 系统输入法（IME）：允许窗口成为键盘焦点目标。
    // QMainWindow 默认 NoFocus，无焦点时 Qt 不会启用 TSF 输入法（无法唤出中文输入法）。
    setFocusPolicy(Qt::StrongFocus);

    // Load saved skin preference with per-skin HSL adjustments
    QString savedSkin = Setup.value("Skin").toString();
    QJsonObject skinHsl = Setup.value("SkinHSL").toObject();
    QJsonObject hsl = skinHsl.value(savedSkin).toObject();
    HueShift = hsl.value("HueShift").toInt(0);
    SatFactor = hsl.value("SatFactor").toDouble(1.0);
    LightFactor = hsl.value("LightFactor").toDouble(1.0);
    CurrentEye = qBound(0, Setup.value("Eye").toInt(0), 5);
    if (!savedSkin.isEmpty())
        ExecTeeDrawer.load(savedSkin, HueShift, SatFactor, LightFactor);
    // Apply saved feather strength (edge anti-aliasing) before the first render
    ExecTeeDrawer.setFeatherStrength(qBound(0, Setup.value("Feather").toInt(1), 2));
    // Apply saved zoom before the first render
    SizeScale = qBound(0.5, Setup.value("Size").toDouble(1.0), 2.0);
    ExecTeeDrawer.setRenderScale(SizeScale);
    // HiDPI：首次渲染前就按屏幕 DPI 设置渲染像素比（1 渲染像素 = 1 物理像素）
    if (QScreen *scr = screen())
        ExecTeeDrawer.setDevicePixelRatio(scr->devicePixelRatio());
    ExecTeeDrawer.render(CurrentEye, 1.0f, 0.0f);
    LastDirX = 1.0f; LastDirY = 0.0f;
    RenderedEye = CurrentEye;

    // Fixed-size window: zoom never changes the geometry (see paintEvent).
    // The tee starts horizontally centred and vertically at the bottom, below
    // the top band reserved for the over-head emoticon (kEmoticonTop).
    m_teePos = QPointF((kWinW - ExecTeeDrawer.canvasSize()) / 2.0, kEmoticonTop);
    resize(kWinW, kWinH);
    dbgWin(QStringLiteral("[init] resize %1x%2").arg(kWinW).arg(kWinH));
    // ── 临时调试：打印屏幕参数（用户分析多屏问题用，定位后移除）──
    const auto screens = QGuiApplication::screens();
    QScreen *pri = QGuiApplication::primaryScreen();
    qDebug() << "[ScreenDebug] screen count =" << screens.size();
    qDebug() << "[ScreenDebug] primary =" << (pri ? pri->name() : QStringLiteral("null"))
             << "vgeo =" << (pri ? pri->virtualGeometry() : QRect());
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *s = screens[i];
        qDebug() << "[ScreenDebug]   [" << i << "]" << s->name()
                 << "geo=" << s->geometry()
                 << "avail=" << s->availableGeometry()
                 << "dpr=" << s->devicePixelRatio()
                 << "primary=" << (s == pri);
    }
    qDebug() << "[ScreenDebug] window pos=" << pos() << "size=" << size();
    // ── 调试结束 ──
    // Multi-instance: nudge non-default profiles so their window doesn't stack
    // exactly on top of the default instance (user can drag it anywhere).
    if (!m_profile.isEmpty())
        move(QPoint(80, 80));
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    TrayIcon.setIcon(makeTrayIcon());
    TrayIcon.setToolTip("Floatee");

    TrayMenu = new FloateeMenu();
    // ── Step1 设计系统：全局 Fluent 风格 QSS 由 Theme 单例统一生成 ──
    // （浅/深两套；菜单/弹窗保留半透明玻璃感，卡片/按钮走 Ela 配色；
    //   深浅模式持久化在 default.json["Theme"]，设置窗口可切换）
    Theme::loadFromJson(Setup);
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
        {"Blink", 5},
    };

    EyeMenu = new FloateeMenu("Eyes");
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
    SizeMenu = new FloateeMenu("Size");
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
    FeatherMenu = new FloateeMenu("Feather");
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

    // ── Step3 托盘精简（方案 A）：外观类子菜单（Eye/Size/Feather/Skin/
    //    EmoticonSet）迁移到设置窗口对应页；托盘保留高频入口 ──
    // 子菜单仍构建（保持成员与既有逻辑可用），但不挂到托盘。
    // 详见 SettingsWindow 外观页（showPage(1)）。
    // Color Adjust 与 Skin Settings 已移入 Skin 子菜单（见下）。

    // ── Skin submenu ────────────────────────────────────────────────
    // 内置皮肤只有 default（真正的默认皮肤）；其余通过外部 skins/ 目录动态加载
    QVector<QPair<QString, QString>> skins = {
        {"Default", ":/skins/default.png"},
    };

    CurrentSkin = Setup.value("Skin").toString(TeeDrawer::defaultSkinPath());
    if (!QFile::exists(CurrentSkin))
        CurrentSkin = TeeDrawer::defaultSkinPath();

    SkinMenu = new FloateeMenu("Skin");
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

    // Skin 子菜单底部操作项（顺序：Color Adjust → Open Skin Folder → Skin Settings）
    SkinMenu->addSeparator();
    QAction *colorAdjust = SkinMenu->addAction("Color Adjust");
    connect(colorAdjust, &QAction::triggered, this, &Floatee::openColorDialog);
    QAction *openSkinFolder = SkinMenu->addAction("Open Skin Folder");
    connect(openSkinFolder, &QAction::triggered, this, [skinsDir]() {
        QDir().mkpath(skinsDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(skinsDir));
    });
    QAction *skinSettings = SkinMenu->addAction("Skin Settings");
    connect(skinSettings, &QAction::triggered, this, [this]() {
        SettingsWindow::instance(this)->showPage(1);   // 外观页
    });

    connect(SkinMenu, &QMenu::triggered, this, &Floatee::switchSkin);
    // 保留快捷换皮肤：Skin 子菜单挂在托盘（高频操作）
    TrayMenu->addMenu(SkinMenu);
    // Eye/Size/Feather 子菜单不再挂托盘（迁移到设置窗口外观页）

    // ── Emoticon Set submenu（自选表情素材，类似 Skin；纯本地不参与联网）──
    // 每个表情素材 = 一张 4×4 网格图集 PNG（含 16 个表情）；内置默认 + 外部
    // emoticons/ 目录（%APPDATA%\Floatee\emoticons\*.png）。切换只改本地图集，
    // 不向服务器同步。
    EmoticonSetMenu = new FloateeMenu("Emoticon Set");
    QActionGroup *emoSetGroup = new QActionGroup(EmoticonSetMenu);
    emoSetGroup->setExclusive(true);
    const QString defaultEmo = QStringLiteral(":/main/emoticons.png");
    EmoticonSet = Setup.value("EmoticonSet").toString(defaultEmo);
    if (!QFile::exists(EmoticonSet))
        EmoticonSet = defaultEmo;
    {
        QAction *a = EmoticonSetMenu->addAction(QStringLiteral("Default"));
        a->setCheckable(true);
        a->setData(defaultEmo);
        a->setChecked(EmoticonSet == defaultEmo);
        emoSetGroup->addAction(a);
    }
    const QString emoDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/emoticons");
    QDirIterator eit(emoDir, {"*.png"}, QDir::Files);
    bool hasExternalEmo = false;
    while (eit.hasNext()) {
        eit.next();
        if (!hasExternalEmo) { EmoticonSetMenu->addSeparator(); hasExternalEmo = true; }
        QAction *a = EmoticonSetMenu->addAction(eit.fileInfo().completeBaseName());
        a->setCheckable(true);
        a->setData(eit.filePath());
        a->setChecked(eit.filePath() == EmoticonSet);
        emoSetGroup->addAction(a);
    }
    EmoticonSetMenu->addSeparator();
    QAction *openEmoFolder = EmoticonSetMenu->addAction("Open Emoticons Folder");
    connect(openEmoFolder, &QAction::triggered, this, [emoDir]() {
        QDir().mkpath(emoDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(emoDir));
    });
    connect(EmoticonSetMenu, &QMenu::triggered, this, &Floatee::switchEmoticonSet);
    // Step3：EmoticonSet 子菜单不再挂托盘（迁移到设置窗口外观页）

    // Emoticon 托盘菜单已移除（M4 已被表情圆盘替代，见 EmoticonWheel）

    // ── 聊天：发送消息入口（打开自绘输入框，与消息框同款样式）──
    QAction *chatAction = TrayMenu->addAction("Send Message...");
    connect(chatAction, &QAction::triggered, this, &Floatee::openChatInput);

    // ── Instance submenu: configs + multi-instance management ─────
    buildInstanceMenu();

    // ── online 分支：Online 子菜单（原名 Multiplayer）──
    MpMenu = new FloateeMenu("Online");
    QAction *mpConnectAction = MpMenu->addAction("Connect...");
    connect(mpConnectAction, &QAction::triggered, this, &Floatee::mpConnect);
    QAction *mpDisconnectAction = MpMenu->addAction("Disconnect");
    connect(mpDisconnectAction, &QAction::triggered, this, &Floatee::mpDisconnect);
    MpMenu->addSeparator();
    QAction *mpCreateAction = MpMenu->addAction("Create Room");
    connect(mpCreateAction, &QAction::triggered, this, &Floatee::mpCreateRoom);
    QAction *mpJoinAction = MpMenu->addAction("Join Room...");
    connect(mpJoinAction, &QAction::triggered, this, &Floatee::mpJoinRoom);
    QAction *mpCodeAction = MpMenu->addAction("Show Password");
    connect(mpCodeAction, &QAction::triggered, this, &Floatee::mpShowJoinCode);
    QAction *mpListAction = MpMenu->addAction("Room List");
    connect(mpListAction, &QAction::triggered, this, &Floatee::mpRoomList);
    QAction *mpLeaveAction = MpMenu->addAction("Leave Room");
    connect(mpLeaveAction, &QAction::triggered, this, [this]() {
        if (m_multi) m_multi->leaveRoom();
    });
    MpMenu->addSeparator();
    MpStatusAction = MpMenu->addAction("Status: 离线");
    MpStatusAction->setEnabled(false);
    TrayMenu->addMenu(MpMenu);

    // M7：休眠 + 使用时长提醒 菜单
    SleepBreakMenu = new FloateeMenu("Sleep & Break");
    // 当前累计使用时长（分钟）显示：禁用态、实时由 onAfkTick 刷新
    m_usageDisplayAction = SleepBreakMenu->addAction(QStringLiteral("Usage: 0 min"));
    m_usageDisplayAction->setEnabled(false);
    SleepBreakMenu->addSeparator();
    QAction *sleepGoAction = SleepBreakMenu->addAction("Go to Sleep");
    connect(sleepGoAction, &QAction::triggered, this, [this]() { enterSleep(); });
    QAction *sleepWakeAction = SleepBreakMenu->addAction("Wake Up");
    connect(sleepWakeAction, &QAction::triggered, this, [this]() { noteActivity(); });
    SleepBreakMenu->addSeparator();
    QAction *sleepTimeoutAction = SleepBreakMenu->addAction("Sleep Timeout (s)...");
    connect(sleepTimeoutAction, &QAction::triggered, this, [this]() {
        bool ok = false;
        const int v = QInputDialog::getInt(this, QStringLiteral("Sleep Timeout"),
            QStringLiteral("无操作多少秒后休眠（0=禁用）："), m_sleepTimeoutSec, 0, 86400, 5, &ok);
        if (!ok) return;
        m_sleepTimeoutSec = v;
        Setup["SleepTimeout"] = v;
        JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    });
    QAction *remindAction = SleepBreakMenu->addAction("Break Reminder (min)...");
    connect(remindAction, &QAction::triggered, this, [this]() {
        bool ok = false;
        const int v = QInputDialog::getInt(this, QStringLiteral("Break Reminder"),
            QStringLiteral("每多少分钟提醒休息（0=禁用）："), m_breakReminderMin, 0, 1440, 5, &ok);
        if (!ok) return;
        m_breakReminderMin = v;
        m_usageSeconds = 0;   // 重置本轮计时
        Setup["BreakReminder"] = v;
        JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    });
    QAction *resetAction = SleepBreakMenu->addAction("Reset After Sleep (min)...");
    connect(resetAction, &QAction::triggered, this, [this]() {
        bool ok = false;
        const int v = QInputDialog::getInt(this, QStringLiteral("Reset After Sleep"),
            QStringLiteral("单次休眠超过多少分钟视为新会话（清空使用计时）："),
            m_resetAfterSleepMin, 0, 43200, 10, &ok);
        if (!ok) return;
        m_resetAfterSleepMin = v;
        Setup["ResetAfterSleep"] = v;
        JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    });
    TrayMenu->addMenu(SleepBreakMenu);

    // ── online 分支：联机控制器初始化（deviceId 首次生成并持久化）──
    {
        QString deviceId;
        const QString devFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/device.json";
        const QJsonObject dev = JsonOpt::File2Json(devFile).object();
        deviceId = dev.value("deviceId").toString();
        if (deviceId.isEmpty()) {
            deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QJsonObject d;
            d.insert("deviceId", deviceId);
            JsonOpt::Json2File(devFile, QJsonDocument(d));
        }
        m_multi = new Multiplayer(this);
        // clientId 需全局唯一：profile + 设备标识前 8 位（不同设备唯一；
        // 同设备因 maxConnsPerDevice=1 同时仅一个联机连接，不会冲突）
        const QString clientId = (m_profile.isEmpty() ? QStringLiteral("floatee") : m_profile)
                                 + QLatin1Char('-') + deviceId.left(8);
        m_multi->init(clientId, deviceId,
                      Setup.value("multiplayer").toObject().value("server").toString());
        connect(m_multi, &Multiplayer::notify, this, [this](const QString &t, const QString &x, bool warn) {
            // Step3：通知改 ElMessageBar 边缘弹出（非模态，自动消失）
            if (warn)
                ElMessageBar::warning(ElMessageBar::Position::TopRight, t, x, 4000, this);
            else
                ElMessageBar::information(ElMessageBar::Position::TopRight, t, x, 3000, this);
        });
        connect(m_multi, &Multiplayer::statusChanged, this, [this](const QString &s) {
            if (MpStatusAction) MpStatusAction->setText(s);
        });
        connect(m_multi, &Multiplayer::roomListReceived, this,
                &Floatee::showRoomListDialog);
        // M2：进出房间切换全屏画布；Tee 表变化刷新渲染；M4 表情接收
        connect(m_multi, &Multiplayer::roomChanged, this, &Floatee::onRoomChangedMp);
        connect(m_multi, &Multiplayer::peersChanged, this, &Floatee::onPeersChangedMp);
        connect(m_multi, &Multiplayer::emoticonReceived, this, &Floatee::onEmoticonReceivedMp);
        connect(m_multi, &Multiplayer::chatReceived, this, &Floatee::onChatReceivedMp);
        m_hitTestTimer = new QTimer(this);
        m_hitTestTimer->setInterval(16);
        connect(m_hitTestTimer, &QTimer::timeout, this, &Floatee::onHitTestTick);
        // 启动时自动连接上次使用的服务器，免手动连接
        tryAutoConnectLastServer();
    }

    // ── Step2：设置窗口入口（隐藏复用单例；5 页设置 + 主题切换）──
    QAction *settingsAction = TrayMenu->addAction("Settings...");
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsWindow::instance(this)->showPage(0);
    });

    // 多屏幕兜底：把窗口放回主屏中心（防止拖到屏幕间隙或拔屏后丢失）
    QAction *resetPosAction = TrayMenu->addAction("Reset Position");
    connect(resetPosAction, &QAction::triggered, this, &Floatee::resetWindowPosition);

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
    // 表情气泡与 Tee 共用同一 Feather 菜单（边缘羽化）
    EmoticonWin->setFeatherStrength(ExecTeeDrawer.featherStrength());
    // 表情素材：按配置加载（内置默认或外部 emoticons/ 目录），纯本地
    {
        const QString defaultEmo = QStringLiteral(":/main/emoticons.png");
        QString emoPath = Setup.value("EmoticonSet").toString(defaultEmo);
        if (!QFile::exists(emoPath))
            emoPath = defaultEmo;
        if (!EmoticonWin->loadAtlas(QPixmap(emoPath)))
            qWarning() << "Floatee: failed to load emoticon atlas" << emoPath;
    }
    connect(EmoticonWin, &EmoticonWindow::frameChanged, this, qOverload<>(&QWidget::update));
    m_emoticonWheel = new EmoticonWheel(this);   // M4：表情圆盘（全屏画布 overlay）
    connect(m_emoticonWheel, &EmoticonWheel::frameChanged, this, qOverload<>(&QWidget::update));
    // 聊天气泡过期检查（10s 自动消失）/ 输入框光标闪烁 / 窗口扩展收缩
    m_chatTimer = new QTimer(this);
    m_chatTimer->setInterval(300);
    connect(m_chatTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool removed = false;
        for (int i = m_chatBubbles.size() - 1; i >= 0; --i) {
            if (now - m_chatBubbles[i].startMs > 10000) {
                m_chatBubbles.removeAt(i);
                removed = true;
            }
        }
        if (m_chatInput) {
            m_chatCursorVisible = !m_chatCursorVisible;
            // 输入框空白且 5s 无输入 → 自动关闭（同时收起非全屏窗口扩展）
            const bool empty =
                (m_imeEdit ? m_imeEdit->text().isEmpty() : m_chatInputText.isEmpty())
                && m_chatPreedit.isEmpty();
            if (empty && now - m_chatLastInputMs > 5000) {
                closeChatInput();
                return;
            }
        }
        if (removed)
            updateChatWindowExtend();
        if (removed || m_chatInput || !m_chatBubbles.isEmpty())
            update();
    });
    m_chatTimer->start();
    // M7 休眠 + 使用时长提醒：配置 + 每秒 tick（借鉴 DDNet AFK 模型）
    m_sleepTimeoutSec = qBound(0, Setup.value("SleepTimeout").toInt(60), 86400);
    m_breakReminderMin = qBound(0, Setup.value("BreakReminder").toInt(20), 1440);
    m_resetAfterSleepMin = qBound(0, Setup.value("ResetAfterSleep").toInt(120), 43200);
    m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    m_lastCursorPos = QCursor::pos();
    m_lastTickWallMs = QDateTime::currentMSecsSinceEpoch();
    m_afkTimer = new QTimer(this);
    m_afkTimer->setInterval(1000);
    connect(m_afkTimer, &QTimer::timeout, this, &Floatee::onAfkTick);
    m_afkTimer->start();
    // 运行时 log（验证使用时长表现）
    initRuntimeLog();
    // M6 IME 代理：Qt 只在标准文本控件（QLineEdit 等）上可靠地启用系统输入法
    // （Windows TSF）。自绘 QMainWindow 即使 ImEnabled=true 也常唤不起输入法；
    // 改用完全透明、置于输入框位置的小 QLineEdit 承载输入法焦点，文本/组合/回车
    // 经信号同步到自绘输入框，保证中文输入法可正常唤起、候选框定位正确。
    m_imeEdit = new QLineEdit(this);
    m_imeEdit->setFrame(false);
    m_imeEdit->setFocusPolicy(Qt::StrongFocus);
    m_imeEdit->setStyleSheet(
        "QLineEdit { background: transparent; border: none;"
        " color: transparent; selection-background-color: transparent; }");
    m_imeEdit->installEventFilter(this);   // 捕获组合文本（见 eventFilter）
    connect(m_imeEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_chatInputText = t;
        m_chatLastInputMs = QDateTime::currentMSecsSinceEpoch();
        update();
    });
    connect(m_imeEdit, &QLineEdit::returnPressed, this, [this]() {
        submitChatInput();
    });
    EmoticonRandomTimer = new QTimer(this);
    EmoticonRandomTimer->setInterval(10000);   // check every 10s
    connect(EmoticonRandomTimer, &QTimer::timeout, this, &Floatee::onRandomEmoticonTick);
    EmoticonRandomTimer->start();

    if (Setup["Always_on_the_Top"].toBool())
    {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        dbgWin(QStringLiteral("[init] setWindowFlag StaysOnTop=1"));
    }
    // 全屏画布在 showEvent 首次显示后进入（窗口已显示，screen()/availableGeometry
    // 正确排除任务栏，置顶 flag 已应用），避免构造阶段几何/置顶异常。
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
    if (m_logFile.isOpen()) {
        m_logFile.write(QStringLiteral("=== Floatee runtime log end ===\n").toUtf8());
        m_logFile.flush();
        m_logFile.close();
    }
    delete ui;
    Theme::saveToJson(Setup);   // Step1：主题取值随配置一并落盘
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::mousePressEvent(QMouseEvent *event)
{
    noteActivity();   // M7：点击算活动（休眠时唤醒）
    // M6 便捷入口：记录点击命中本地 Tee（随后按回车即唤起输入框）。
    // 全屏：用 hitTestTee（屏幕坐标）；非全屏：窗口即 Tee，点击窗口即命中。
    if (m_fullscreenCanvas) {
        const QPoint g = event->globalPosition().toPoint();
        QString roleId;
        m_teeClicked = hitTestTee(g, &roleId) && roleId.isEmpty();
    } else {
        m_teeClicked = true;
    }
    // M2 全屏画布：左键拖拽命中的 Tee（本地或远端，仅本地摆放）；右键 M4 表情圆盘
    if (m_fullscreenCanvas) {
        if (event->button() == Qt::RightButton) {            // M4：右键本地 Tee → 表情圆盘；右键远端 Tee → 管理菜单
            const QPoint g = event->globalPosition().toPoint();
            QString roleId;
            if (hitTestTee(g, &roleId)) {
                if (roleId.isEmpty())
                    openEmoticonWheel();
                else
                    showPeerContextMenu(roleId, g);
            }
        } else if (event->button() == Qt::LeftButton &&
                   !(m_emoticonWheel && m_emoticonWheel->isOpen())) {
            const QPoint g = event->globalPosition().toPoint();
            QString roleId;
            if (hitTestTee(g, &roleId)) {
                m_dragging = true;      // 本地 Tee 的 roleId 为空串，需独立标志
                m_dragRoleId = roleId;
                const QPointF teePos = roleId.isEmpty() ? m_localTeePos
                                                        : m_peersRender.value(roleId).pos;
                m_dragOffset = QPointF(g) - teePos;
                if (roleId.isEmpty()) {
                    // 开始拖拽本地 Tee → 进入走路（相位随 Tee 水平位置）
                    WalkPhase = std::fmod(double(m_localTeePos.x()), 100.0) / 100.0f;
                    if (WalkPhase < 0.0f) WalkPhase += 1.0f;
                    LastWalkPhase = -2.0f;   // force re-render
                    updateEyeFollow();
                } else {
                    // 开始拖拽远端 Tee → 进入走路（与本地一致）
                    auto &pr = m_peersRender[roleId];
                    if (pr.drawer) {
                        pr.walkPhase = std::fmod(double(pr.pos.x()), 100.0) / 100.0f;
                        if (pr.walkPhase < 0.0f) pr.walkPhase += 1.0f;
                        pr.drawer->render(pr.eye, pr.dir.x(), pr.dir.y(),
                                          pr.walkPhase, pr.eyeScale);
                        pr.walkPixmap = pr.drawer->Tee;
                    }
                    update();
                }
            }
        }
        return;
    }
    if (event->button() == Qt::LeftButton &&
        !(m_emoticonWheel && m_emoticonWheel->isOpen())) {
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
        // M4：单机右键也用圆盘（与联机保持一致），替代原来的右键循环切眼
        openEmoticonWheel();
    }
    QMainWindow::mousePressEvent(event);
}

void Floatee::mouseMoveEvent(QMouseEvent *event)
{
    noteActivity();   // M7：鼠标移动算活动
    m_lastCursorPos = event->globalPosition().toPoint();
    // M2 全屏画布：拖拽 Tee（本地/远端）仅改变本地摆放
    if (m_fullscreenCanvas) {
        if (m_emoticonWheel && m_emoticonWheel->isOpen()) {
            // M4：圆盘打开时更新悬停高亮
            m_emoticonWheel->setMousePos(QPointF(event->globalPosition().toPoint()));
            update();
        } else if (m_dragging) {
            const QPointF np = QPointF(event->globalPosition().toPoint()) - m_dragOffset;
            if (m_dragRoleId.isEmpty()) {
                m_localTeePos = clampTeePos(np, ExecTeeDrawer.opaqueRect());
                // 拖拽走路动画：相位随 Tee 水平位置（DDNet 公式，与单机一致）
                WalkPhase = std::fmod(double(m_localTeePos.x()), 100.0) / 100.0f;
                if (WalkPhase < 0.0f) WalkPhase += 1.0f;
                LastWalkPhase = -2.0f;   // force re-render with the new phase
                RenderedEye = -1;
                updateEyeFollow();
            } else {
                auto &pr = m_peersRender[m_dragRoleId];
                pr.pos = clampTeePos(np, pr.drawer ? pr.drawer->opaqueRect() : QRect());
                if (pr.drawer) {
                    // 远端拖拽走路动画：相位随 Tee 水平位置（与本地一致）
                    pr.walkPhase = std::fmod(double(pr.pos.x()), 100.0) / 100.0f;
                    if (pr.walkPhase < 0.0f) pr.walkPhase += 1.0f;
                    pr.drawer->render(pr.eye, pr.dir.x(), pr.dir.y(),
                                      pr.walkPhase, pr.eyeScale);
                    pr.walkPixmap = pr.drawer->Tee;
                }
                update();
            }
        }
        return;
    }
    if (MousePress) {
        move(event->globalPosition().toPoint() - MousePoint);
        // Walk-cycle phase follows the horizontal position (DDNet formula).
        WalkPhase = std::fmod(double(this->pos().x()), 100.0) / 100.0f;
        if (WalkPhase < 0.0f) WalkPhase += 1.0f;
        LastWalkPhase = -2.0f;   // force re-render with the new phase
        updateEyeFollow();
    } else if (m_emoticonWheel && m_emoticonWheel->isOpen()) {
        // M4：圆盘打开时更新悬停高亮（widget 坐标）
        m_emoticonWheel->setMousePos(QPointF(event->position()));
        update();
    }
    QMainWindow::mouseMoveEvent(event);
}

void Floatee::mouseReleaseEvent(QMouseEvent *event)
{
    noteActivity();   // M7：松开鼠标算活动
    if (m_fullscreenCanvas) {
        if (m_emoticonWheel && m_emoticonWheel->isOpen() && event->button() == Qt::LeftButton) {
            // 全屏圆盘中心是全局坐标，提交也必须用全局坐标（用窗口内坐标会
            // 与中心错位 44px，导致下方表情点不中/被穿透）
            submitEmoticonWheel(QPointF(event->globalPosition().toPoint()));
            return;
        }
        const bool wasDraggingLocal = m_dragging && m_dragRoleId.isEmpty();
        const QString dragRole = m_dragRoleId;
        m_dragging = false;
        m_dragRoleId.clear();
        if (wasDraggingLocal) {
            // 停止拖拽本地 Tee → 回 idle 姿态
            WalkPhase = -1.0f;
            LastWalkPhase = -2.0f;
            updateEyeFollow();
        } else if (!dragRole.isEmpty()) {
            // 松开远端 Tee → 恢复分离渲染（静态 body + 眼睛）
            auto it = m_peersRender.find(dragRole);
            if (it != m_peersRender.end()) {
                it->walkPhase = -1.0f;
                it->walkPixmap = QPixmap();
                update();
            }
        }
        return;
    }
    if (m_emoticonWheel && m_emoticonWheel->isOpen() && event->button() == Qt::LeftButton) {
        submitEmoticonWheel(QPointF(event->position()));
        return;
    }
    MousePress = false;
    WalkPhase = -1.0f;   // stop walking, back to idle
    LastWalkPhase = -2.0f;
    updateEyeFollow();
    QMainWindow::mouseReleaseEvent(event);
}

void Floatee::keyPressEvent(QKeyEvent *event)
{
    noteActivity();   // M7：按键算活动（输入中同样算）
    // ── M6 输入框打开：所有按键进入输入逻辑 ──
    if (m_chatInput) {
        // 文本输入由 IME 代理 QLineEdit 处理（含系统输入法）；这里只兜底：
        // Esc 取消、Enter 发送（焦点不在代理上时也能用）。isAutoRepeat 的回车
        // 忽略，防止按住回车导致“发送→自动打开”抖动。
        if (event->key() == Qt::Key_Escape) {
            closeChatInput();
        } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
                   && !event->isAutoRepeat()) {
            submitChatInput();
        }
        event->accept();
        return;
    }
    // 便捷入口：点击本地 Tee 后按回车 → 打开输入框（忽略按住产生的重复回车）
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && m_teeClicked && !event->isAutoRepeat()) {
        // 关闭后 100ms 冷却：抑制发送/关闭的回车残留或快速二次按键立即重开
        // 输入框（表现为“自动打开下一个/空输入框关不掉”）。手动再按回车
        // （间隔 >100ms）正常唤起。
        if (QDateTime::currentMSecsSinceEpoch() - m_chatCloseMs < 100) {
            event->accept();
            return;
        }
        openChatInput();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        // Esc 关闭圆盘（全屏/非全屏通用，触发收回动画）
        if (m_emoticonWheel && m_emoticonWheel->isOpen()) {
            m_emoticonWheel->startClose();
            unsetCursor();
            update();
        }
        event->accept();
        return;
    }
    if (m_fullscreenCanvas) {
        // M4：数字键 0-9 → 表情 index 0-9（附加快捷）
        const int idx = event->key() - Qt::Key_0;
        if (idx >= 0 && idx <= 9) {
            sendLocalEmoticon(idx);
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void Floatee::changeEvent(QEvent *event)
{
    // 窗口被拖到不同 DPI 的屏幕时，更新渲染像素比并重渲染（HiDPI 适配）
    if (event->type() == QEvent::ScreenChangeInternal)
        applyTeeDpr();
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
            // 输入框打开期间窗口重新激活时，确保系统输入法仍被唤起
            if (active && m_chatInput) {
                if (QInputMethod *im = QGuiApplication::inputMethod())
                    im->show();
            }
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
    // M2：联机全屏画布 —— 本地 + 远端 Peer 的 Tee 都画在屏幕上（屏幕坐标=画布坐标）
    if (m_fullscreenCanvas) {
        QPainter p(this);
        // 全屏画布的所有位置（m_localTeePos / peer pos / 表情 / 气泡 / 圆盘）
        // 都是全局（屏幕）坐标；macOS 上窗口不在原点（availableGeometry 从
        // (0,44) 开始），必须把 painter 平移到全局坐标系，绘制才与实际命中
        // 区域对齐（否则 Tee/圆盘显示位置比可点击区域低 44px）。
        p.translate(-QPointF(pos()));
        p.drawPixmap(qRound(m_localTeePos.x()), qRound(m_localTeePos.y()), ExecTeeDrawer.Tee);
        for (const auto &pr : m_peersRender) {
            if (!pr.drawer || pr.drawer->canvasSize() <= 0 || pr.hidden)
                continue;
            if (pr.walkPhase >= 0.0f && !pr.walkPixmap.isNull()) {
                // 拖拽走路中：用完整渲染（含 walk 姿态）
                p.drawPixmap(qRound(pr.pos.x()), qRound(pr.pos.y()), pr.walkPixmap);
            } else {
                // 分离渲染：body 静态（皮肤变化才重建），eyes 在数据变化时重渲染
                // （onPeersChangedMp），这里只贴图
                if (!pr.body.isNull())
                    p.drawPixmap(qRound(pr.pos.x()), qRound(pr.pos.y()), pr.body);
                if (!pr.eyes.isNull())
                    p.drawPixmap(qRound(pr.pos.x()), qRound(pr.pos.y()), pr.eyes);
            }
        }
        // M4：多 Tee 并行表情（本地 + 每个远端 Tee）
        if (EmoticonWin && EmoticonWin->hasAny())
            paintEmoticonsFullscreen(p);
        // 聊天：本地 + 远端 Tee 文本气泡
        paintChatBubbles(p);
        // M7：本地 Tee 休眠 zzz（全局坐标）
        if (m_sleeping && EmoticonWin) {
            const int zcs = ExecTeeDrawer.canvasSize();
            const float zts = ExecTeeDrawer.teeSize();
            paintAfkZzz(p, m_localTeePos + QPointF(zcs / 2.0, zcs / 2.0), zts);
        }
        // M4：表情圆盘 overlay
        if (m_emoticonWheel && m_emoticonWheel->isOpen())
            m_emoticonWheel->paint(p, EmoticonWin ? EmoticonWin->atlas() : QPixmap(),
                                   ExecTeeDrawer.SkinFile);
        // 子控件/背景按窗口内坐标绘制，恢复变换后再交给 QMainWindow
        p.resetTransform();
        QMainWindow::paintEvent(event);
        return;
    }

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
        // HiDPI：气泡帧按屏幕物理像素渲染（像素 = 逻辑 × dpr），避免放大失真
        const qreal fdpr = ExecTeeDrawer.devicePixelRatio();
        QPixmap frame(qRound(width() * fdpr), qRound(height() * fdpr));
        frame.setDevicePixelRatio(fdpr);
        frame.fill(Qt::transparent);
        const QPointF teeCenter = m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
        const bool ok = EmoticonWin->renderFrame(frame, teeCenter);
        if (ok)
            p.drawPixmap(0, 0, frame);
    }
    // M6：消息区域（输入框 + 纵向消息列表，位于 Tee 上方）
    if (m_chatInput || !m_chatBubbles.isEmpty())
        paintChatAreaFor(p, QString(),
                         m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts),
                         ts / 64.0f);
    // M7：休眠 zzz（Tee 右上）
    if (m_sleeping && EmoticonWin)
        paintAfkZzz(p, m_teePos + QPointF(cs / 2.0, cs / 2.0), ts);
    // M4：表情圆盘 overlay（非全屏也支持，右键本地 Tee 打开）
    if (m_emoticonWheel && m_emoticonWheel->isOpen())
        m_emoticonWheel->paint(p, EmoticonWin ? EmoticonWin->atlas() : QPixmap(),
                               ExecTeeDrawer.SkinFile);
    QMainWindow::paintEvent(event);
}

QIcon Floatee::makeTrayIcon() const
{
    // 托盘图标总尺寸由系统固定，放大分辨率无用 → 关键是**裁剪掉透明边距**，
    // 让 Tee 实际像素占满整个图标。用 opaqueRect（非透明像素包围盒）裁出
    // Tee 本体，再提供多尺寸让系统按 DPI 选择。
    // opaqueRect 是物理像素（相对 canvas），而带 dpr 的 QPixmap 坐标是逻辑的。
    // 把 Tee 的 dpr 标记临时置 1，让逻辑=物理，裁剪坐标才统一（HiDPI 下不会
    // 裁错导致 Tee 变小/偏移左上）。
    QPixmap src = ExecTeeDrawer.Tee;   // 值复制（隐式共享），setDevicePixelRatio 会 detach
    if (src.isNull())
        return QIcon();
    src.setDevicePixelRatio(1.0f);
    QPixmap cropped = src;
    const QRect box = ExecTeeDrawer.opaqueRect();
    if (!box.isNull() && !box.isEmpty()) {
        const QRect clamp = box.intersected(src.rect());
        if (!clamp.isEmpty())
            cropped = src.copy(clamp);
    }
    // 保持原始宽高比：Tee 本体是竖长方形，直接忽略比例缩放到正方形会被压扁。
    // 以长边为边长、短边居中拓展出正方形（透明背景），再等比例缩放到图标尺寸。
    const int side = qMax(cropped.width(), cropped.height());
    if (side > 0 && cropped.width() != cropped.height()) {
        QPixmap sq(side, side);
        sq.fill(Qt::transparent);
        QPainter p(&sq);
        p.drawPixmap((side - cropped.width()) / 2, (side - cropped.height()) / 2, cropped);
        p.end();
        cropped = sq;
    }
    QIcon icon;
    const QList<int> sizes = { 16, 20, 24, 32, 48, 64, 96, 128 };
    for (int s : sizes) {
        QPixmap px = cropped.scaled(s, s, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        px.setDevicePixelRatio(1.0);   // 图标按逻辑尺寸固定，避免 Tee 的 dpr 使其偏小
        icon.addPixmap(px);
    }
    return icon;
}

void Floatee::updateEyeFollow()
{
    // Look direction: from the tee area center toward the cursor. In fullscreen
    // canvas mode the tee centre is in screen coordinates (m_localTeePos).
    const QPoint g = QCursor::pos();
    const int cs = ExecTeeDrawer.canvasSize();
    const QPointF c = m_fullscreenCanvas
        ? m_localTeePos + QPointF(cs / 2.0, cs / 2.0)
        : QPointF(pos()) + m_teePos + QPointF(cs / 2.0, cs / 2.0);
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

    int eye = m_sleeping ? 5 : CurrentEye;   // M7 休眠：强制 EMOTE_BLINK 压扁闭眼
    // Classic Floatee behaviour: when the cursor hovers near the top of the tee
    // and the default eyes are active, switch to a happy face.
    const bool petting = !m_sleeping && (eye == 0 && len < 45.0f * SizeScale && d.y() < 0.0f);
    if (petting) {
        eye = 1;
        // Edge-trigger the hearts emoticon once when the cursor starts petting.
        if (!m_petting)
            showEmoticonOnTee(teer::EMOTICON_HEARTS);
    }
    m_petting = petting;

    // M2：联机时上报眼睛状态（鼠标相对本地 Tee 画布中心偏移 + 眼睛类型）
    if (m_multi && m_multi->isConnected() && m_multi->inRoom()) {
        const float ts = ExecTeeDrawer.teeSize();
        const QPointF teeCenter = m_fullscreenCanvas
            ? m_localTeePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts)
            : QPointF(pos()) + m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
        const QPointF off = QPointF(g) - teeCenter;
        m_multi->updateLocalMouse(float(off.x()), float(off.y()), eye, eyeScale);
    }

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
    // M4：随机表情也转发给房间其他人（每 10s 一次且 50% 概率，远低于
    // 服务器 emoticonPerSec=5 上限）
    if (m_multi && m_multi->isConnected() && m_multi->inRoom())
        m_multi->sendEmoticon(idx);
}

void Floatee::showEmoticonOnTee(int index, const QString &key)
{
    if (!EmoticonWin)
        return;
    QPointF teeCenter;
    float teeSize = ExecTeeDrawer.teeSize();
    if (key.isEmpty()) {
        const int cs = ExecTeeDrawer.canvasSize();
        // 全屏用全局 m_localTeePos，非全屏用窗口内 m_teePos（与 paint 一致）
        teeCenter = (m_fullscreenCanvas ? m_localTeePos : m_teePos)
                  + QPointF(cs / 2.0, cs / 2.0 + 0.12 * teeSize);
    } else {
        const auto it = m_peersRender.constFind(key);
        if (it == m_peersRender.constEnd() || !it->drawer)
            return;
        const int pcs = it->drawer->canvasSize();
        const float pts = it->drawer->teeSize();
        teeSize = pts;
        teeCenter = it->pos + QPointF(pcs / 2.0, pcs / 2.0 + 0.12 * pts);
    }
    EmoticonWin->showEmoticon(key, index, teeSize, teeCenter);
}

void Floatee::sendLocalEmoticon(int index)
{
    if (index < 0 || index >= teer::NUM_EMOTICONS)
        return;
    showEmoticonOnTee(index);   // 本地显示
    // M4：联机广播（服务器 emoticonPerSec=5 限流，超限静默）
    if (m_multi && m_multi->isConnected() && m_multi->inRoom())
        m_multi->sendEmoticon(index);
}

void Floatee::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示后进入始终全屏画布（单机/联机统一）。延迟到事件循环空闲再
    // 执行：窗口完全映射后 screen()/availableGeometry()（排除任务栏）才稳定、
    // setGeometry 才真正生效——直接在 showEvent 中设置会被 Qt 初始几何覆盖，
    // 导致窗口盖住任务栏（之前用 availableGeometry 修复过，此处恢复正确时机）。
    if (!m_fullscreenEntered) {
        m_fullscreenEntered = true;
        dbgWin(QStringLiteral("[showEvent] scheduling enterFullscreenCanvas"));
        QTimer::singleShot(0, this, &Floatee::enterFullscreenCanvas);
    }
}

void Floatee::enterFullscreenCanvas()
{
    dbgWin(QStringLiteral("[fullscreen] enter"));
    QScreen *s = screen();
    if (!s) s = QGuiApplication::primaryScreen();
    QRect scr = s ? s->availableGeometry() : QRect();
    dbgWin(QStringLiteral("[fullscreen] screen=%1 available=%2")
        .arg(s ? s->name() : QStringLiteral("null"))
        .arg(rectStr(scr.x(), scr.y(), scr.width(), scr.height())));
    if (scr.isNull() || scr.isEmpty())
        scr = QRect(0, 0, 1920, 1080);   // 极端兜底
#ifdef Q_OS_WIN
    {
        APPBARDATA abd;
        memset(&abd, 0, sizeof(abd));
        abd.cbSize = sizeof(abd);
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            const RECT &tb = abd.rc;
            dbgWin(QStringLiteral("[fullscreen] taskbar rect(px)=%1x%2+%3+%4")
                .arg(tb.right - tb.left).arg(tb.bottom - tb.top).arg(tb.left).arg(tb.top));
            // AppBar 返回物理像素，Qt 几何是逻辑像素 → 按屏幕 DPI 换算
            const double dpr = s ? s->devicePixelRatio() : 1.0;
            const QRectF tbf(tb.left / dpr, tb.top / dpr,
                             (tb.right - tb.left) / dpr, (tb.bottom - tb.top) / dpr);
            dbgWin(QStringLiteral("[fullscreen] taskbar rect(log)=%1")
                .arg(rectStr(tbf.x(), tbf.y(), tbf.width(), tbf.height())));
            const LONG m = 4;   // 容差（逻辑 px）
            // 用「完整屏幕几何」（s->geometry()，含任务栏）判断任务栏贴哪条边，
            // 再从 availableGeometry 裁掉任务栏所在条带。
            //  - fixed 任务栏：availableGeometry 已排除任务栏，再裁同条带无影响
            //  - auto-hide 任务栏：availableGeometry = 全屏（隐藏的任务栏不占
            //    工作区），必须用完整屏判边 + 裁掉条带，否则 Floatee 全屏覆盖
            //    会遮挡弹出的任务栏（表现为"任务栏不置顶"）
            if (s) {
                const QRect full = s->geometry();
                // 容差 = 任务栏厚度（min 宽/高）：底部/顶部任务栏厚=高，
                // 左/右任务栏厚=宽。不能用 max（横跨全屏的任务栏宽=屏宽，
                // 容差会过大导致误判所有边）。
                const int thick = qMax(16, qRound(qMin(tbf.width(), tbf.height())));
                // 横跨全宽 → 底/顶任务栏；纵跨全高 → 左/右任务栏。避免竖条
                // 任务栏的 top/bottom 也贴屏幕 top/bottom 导致误判 atBottom。
                const bool spansWidth  = std::abs(tbf.width() - full.width()) <= 4;
                const bool spansHeight = std::abs(tbf.height() - full.height()) <= 4;
                const bool atBottom = spansWidth  && std::abs(tbf.top() - full.bottom()) <= thick;
                const bool atTop    = spansWidth  && std::abs(tbf.bottom() - full.top()) <= thick;
                const bool atLeft   = spansHeight && std::abs(tbf.right() - full.left()) <= thick;
                const bool atRight  = spansHeight && std::abs(tbf.left() - full.right()) <= thick;
                dbgWin(QStringLiteral("[fullscreen] edge B=%1 T=%2 L=%3 R=%4")
                    .arg(atBottom).arg(atTop).arg(atLeft).arg(atRight));
                const QRect origScr = scr;   // 保护：记录原始可用区域
                if (atBottom)      scr.setBottom(qMin(scr.bottom(), full.bottom() - qRound(tbf.height())));
                else if (atTop)    scr.setTop(qMax(scr.top(), full.top() + qRound(tbf.height())));
                else if (atLeft)   scr.setLeft(qMax(scr.left(), full.left() + qRound(tbf.width())));
                else if (atRight)  scr.setRight(qMin(scr.right(), full.right() - qRound(tbf.width())));
                // 保护：多显示器/异常布局下裁切可能出错位（0 宽/过小），回退原始
                // 可用区域，避免窗口错位导致 Tee 不显示。
                if (scr.width() < 100 || scr.height() < 100)
                    scr = origScr;
            }
        } else {
            dbgWin(QStringLiteral("[fullscreen] ABM_GETTASKBARPOS failed"));
        }
    }
#endif
    dbgWin(QStringLiteral("[fullscreen] final scr=%1")
        .arg(rectStr(scr.x(), scr.y(), scr.width(), scr.height())));
    setGeometry(scr);
    applyTeeDpr();   // HiDPI：按全屏所在屏幕更新渲染像素比
    // 必须在 setGeometry 之后才 clamp：clampTeePos 依赖 width()/height()，
    // 若在放大前调用会按初始化窗口 192×275 把 Tee 钳到左上角（m_localTeePos
    // 变成 (46,120)），Tee 显示错位且永远进不了屏幕中心。
    const QPointF center = QRectF(scr).center();
    const int cs = ExecTeeDrawer.canvasSize();
    m_localTeePos = clampTeePos(center - QPointF(cs / 2.0, cs / 2.0), ExecTeeDrawer.opaqueRect());
    dbgWin(QStringLiteral("[fullscreen] after setGeometry geom=%1")
        .arg(rectStr(geometry().x(), geometry().y(), geometry().width(), geometry().height())));
    m_fullscreenCanvas = true;
    m_transparent = true;
    if (!m_platformInfo)
        m_platformInfo = PlatformWindowInfo::create();
#ifdef Q_OS_MACOS
    if (m_platformInfo)
        m_platformInfo->setWindowClickThrough(
            reinterpret_cast<void *>(winId()), true);
#else
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
#endif
    m_hitTestTimer->start();
    if (EyeFollowTimer) EyeFollowTimer->setInterval(33);   // 全屏整屏重绘降频
    // Floatee 置顶顶层（Win32 HWND_TOPMOST）：保证在普通窗口之上。
    // 注意：不周期强制任务栏置顶——那会盖住托盘菜单。任务栏不被 Floatee
    // 遮挡的保障是上面的 AppBar 区域裁切（Floatee 避开任务栏区域），任务栏
    // 保持系统默认 topmost 即可。
    if (m_platformInfo)
        m_platformInfo->setWindowTopmost(reinterpret_cast<void *>(winId()));
    RenderedEye = -1;
    updateEyeFollow();
    update();
    // 延迟检查窗口与任务栏的最终状态
    QTimer::singleShot(600, this, &Floatee::logWindowState);
}

void Floatee::logWindowState()
{
#ifdef Q_OS_WIN
    const HWND hw = reinterpret_cast<HWND>(winId());
    RECT rc{};
    ::GetWindowRect(hw, &rc);
    const LONG_PTR ex = ::GetWindowLongPtrW(hw, GWL_EXSTYLE);
    dbgWin(QStringLiteral("[state] Floatee rect=%1x%2+%3+%4 ex=%5")
        .arg(rc.right - rc.left).arg(rc.bottom - rc.top).arg(rc.left).arg(rc.top)
        .arg(exStyleStr(ex)));
    const HWND htb = ::FindWindowW(L"Shell_TrayWnd", nullptr);
    if (htb) {
        RECT tr{};
        ::GetWindowRect(htb, &tr);
        const LONG_PTR tex = ::GetWindowLongPtrW(htb, GWL_EXSTYLE);
        dbgWin(QStringLiteral("[state] Taskbar rect=%1x%2+%3+%4 ex=%5")
            .arg(tr.right - tr.left).arg(tr.bottom - tr.top).arg(tr.left).arg(tr.top)
            .arg(exStyleStr(tex)));
    } else {
        dbgWin(QStringLiteral("[state] Shell_TrayWnd not found"));
    }
    dbgWin(QStringLiteral("[state] Floatee Qt geom=%1 flags=0x%2")
        .arg(rectStr(geometry().x(), geometry().y(), geometry().width(), geometry().height()))
        .arg(quintptr(windowFlags()), 0, 16));
#endif
}

void Floatee::openEmoticonWheel()
{
    if (!m_emoticonWheel)
        return;
    const int cs = ExecTeeDrawer.canvasSize();
    const float ts = ExecTeeDrawer.teeSize();
    // 中心 = 本地 Tee 中心（全屏画布，widget 坐标 == 屏幕坐标）
    QPointF teeCenter = m_localTeePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
    // 圆盘随 Tee 缩放同步，但以 100%（scale=1.0）为上限：Tee 缩放到 >100%
    // 时圆盘保持 100% 大小（避免过大），Tee <100% 时圆盘同步缩小。
    m_emoticonWheel->setScale(qMin(1.0, SizeScale));
    // Tee 贴近屏幕边缘时圆盘内移，不被屏幕边界裁断
    const double R = 190.0 * m_emoticonWheel->scale();
    // 全屏：teeCenter 是全局坐标，钳制在全屏窗口的全局范围内；
    // 非全屏：teeCenter 是窗口内坐标，钳制在窗口矩形内。
    const QRect bound = m_fullscreenCanvas ? geometry() : rect();
    teeCenter.setX(qBound(R, teeCenter.x(), qMax(R, double(bound.right()) - R)));
    teeCenter.setY(qBound(R, teeCenter.y(), qMax(R, double(bound.bottom()) - R)));
    m_emoticonWheel->open(teeCenter);
    setCursor(Qt::CrossCursor);
    update();
}

void Floatee::submitEmoticonWheel(const QPointF &widgetPos)
{
    if (!m_emoticonWheel || !m_emoticonWheel->isOpen())
        return;
    const EmoticonWheel::Result r = m_emoticonWheel->submitAt(widgetPos);
    m_emoticonWheel->startClose();   // 触发收回动画后关闭
    unsetCursor();
    switch (r.hit) {
    case EmoticonWheel::Hit::Emoticon:
        sendLocalEmoticon(r.index);
        break;
    case EmoticonWheel::Hit::Eye: {
        // 内环 6 眼睛：NORMAL/HAPPY/ANGRY/PAIN/SURPRISE/BLINK
        static const int kEyeMap[6] = { 0, 1, 2, 3, 4, 5 };
        CurrentEye = kEyeMap[r.index];
        RenderedEye = -1;
        updateEyeFollow();   // 眼睛变化随 mouse 消息同步到远端（联机时）
        if (EyeGroup && EyeGroup->actions().size() > CurrentEye)
            EyeGroup->actions()[CurrentEye]->setChecked(true);
        // 持久化：圆盘选的眼睛也要保存（否则重启后加载旧的 Setup["Eye"]，
        // 表现为"每次启动自动切回之前保存的眼睛"）
        Setup["Eye"] = CurrentEye;
        JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
        break;
    }
    default:
        break;   // Cancel / None → 仅关闭
    }
    update();
}

void Floatee::onEmoticonReceivedMp(const QString &roleId, int index)
{
    // 全屏画布内，在对应远端 Tee 上方播放表情
    if (m_fullscreenCanvas && EmoticonWin)
        showEmoticonOnTee(index, roleId);
}

void Floatee::onChatReceivedMp(const QString &roleId, const QString &text)
{
    addChatMessage(roleId, text);
}

void Floatee::addChatMessage(const QString &roleId, const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    ChatBubble b;
    b.roleId = roleId;
    b.text = text;
    b.startMs = QDateTime::currentMSecsSinceEpoch();
    m_chatBubbles.append(b);
    if (m_chatBubbles.size() > 8)
        m_chatBubbles.removeFirst();
    updateChatWindowExtend();
    update();
}

void Floatee::openChatInput()
{
    if (m_chatInput)
        return;
    m_chatInput = true;
    m_chatInputText.clear();
    m_chatPreedit.clear();
    m_chatCursor = 0;
    m_chatCursorVisible = true;
    m_chatLastInputMs = QDateTime::currentMSecsSinceEpoch();
    updateChatWindowExtend();
    update();
    // 聚焦窗口并把键盘焦点交给 IME 代理（QLineEdit），确保系统输入法可唤起。
    // activateWindow 在 Windows 上是异步的，等事件循环处理完再 setFocus + show。
    if (m_imeEdit)
        m_imeEdit->clear();
    activateWindow();
    raise();
    QTimer::singleShot(0, this, [this]() {
        if (!m_chatInput)
            return;
        if (m_imeEdit) {
            m_imeEdit->setFocus(Qt::OtherFocusReason);
            m_imeEdit->setCursorPosition(m_imeEdit->text().size());
        } else {
            setFocus(Qt::OtherFocusReason);
        }
        if (QInputMethod *im = QGuiApplication::inputMethod())
            im->show();
    });
}

void Floatee::closeChatInput()
{
    if (!m_chatInput)
        return;
    m_chatInput = false;
    m_chatInputText.clear();
    m_chatPreedit.clear();
    m_chatCursor = 0;
    m_chatCloseMs = QDateTime::currentMSecsSinceEpoch();   // 防自动重开：短暂抑制回车唤起
    // 注意：不清除 m_teeClicked —— 点击过 Tee 后，任何关闭方式（空白回车/发送/Esc/超时）
    // 之后都能再按回车重新唤起输入框。防抖由 keyPressEvent 忽略 isAutoRepeat 的回车负责。
    // 收回输入法焦点（清空/取消聚焦 IME 代理，避免残留候选框）
    if (m_imeEdit) {
        m_imeEdit->clear();
        m_imeEdit->clearFocus();
        setFocus(Qt::OtherFocusReason);
    }
    updateChatWindowExtend();
    update();
}

void Floatee::submitChatInput()
{
    if (!m_chatInput)
        return;
    const QString text = m_imeEdit ? m_imeEdit->text().trimmed() : m_chatInputText.trimmed();
    if (text.isEmpty()) {
        // 空内容 → 只关闭输入框
        closeChatInput();
        return;
    }
    // 联机时广播给同房间；未联机仅本地显示（自娱自乐）
    if (m_multi && m_multi->inRoom())
        m_multi->sendChat(text);
    addChatMessage(QString(), text);
    closeChatInput();
}

QPointF Floatee::localTeeCenterGlobal() const
{
    const int cs = ExecTeeDrawer.canvasSize();
    const float ts = ExecTeeDrawer.teeSize();
    return m_fullscreenCanvas
        ? m_localTeePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts)
        : QPointF(pos()) + m_teePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
}

QRectF Floatee::chatInputScreenRect() const
{
    if (!m_chatInput)
        return QRectF();
    const QPointF anchor = localTeeCenterGlobal();
    const float scale = ExecTeeDrawer.teeSize() / 64.0f;
    QFont f = font();
    f.setPixelSize(qRound(14.0 * scale));
    QFontMetrics fm(f);
    const int maxW = qRound(250.0 * scale);
    const int padX = qRound(8.0 * scale), padY = qRound(5.0 * scale);
    const QString disp = m_chatInputText + m_chatPreedit + QLatin1Char(' ');
    const QRect tr = fm.boundingRect(QRect(0, 0, maxW, 1000), Qt::TextWordWrap, disp);
    const double w = tr.width() + padX * 2.0;
    const double h = tr.height() + padY * 2.0;
    const double bottomY = anchor.y() - qRound(46.0 * scale);   // 与绘制一致（上移避开 Tee 身体）
    return QRectF(anchor.x() - w / 2.0, bottomY - h, w, h);
}

void Floatee::applyTeeDpr()
{
    QScreen *scr = screen();
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    if (!scr)
        return;
    const float dpr = scr->devicePixelRatio();
    if (qFuzzyCompare(ExecTeeDrawer.devicePixelRatio(), dpr))
        return;
    ExecTeeDrawer.setDevicePixelRatio(dpr);
    // 强制按新像素比重渲染本地 Tee（眼睛/方向跟随下一帧生效）
    RenderedEye = -1;
    updateEyeFollow();
    // peers 在同一屏幕，dpr 一致；仅更新其 drawer，渲染由其自身逻辑触发
    for (auto &pr : m_peersRender) {
        if (pr.drawer)
            pr.drawer->setDevicePixelRatio(dpr);
    }
}

void Floatee::updateChatWindowExtend()
{
    if (m_fullscreenCanvas)
        return;
    const bool need = m_chatInput || !m_chatBubbles.isEmpty();
    const int wantH = need ? 210 : 0;   // 消息区高度（非全屏固定值）
    if (wantH == m_chatAreaH)
        return;
    const int delta = wantH - m_chatAreaH;
    m_chatAreaH = wantH;
    // 窗口向上扩展/收缩：Tee 在窗口内同步下移/上移，保持屏幕位置不变
    move(pos().x(), pos().y() - delta);
    resize(kWinW, kWinH + m_chatAreaH);
    m_teePos.setY(kEmoticonTop + m_chatAreaH);
    update();
}

// 渲染一个 Tee 的消息区：输入框（仅本地）在底部，消息列表越早越靠上。
// anchor = Tee 中心（全屏=屏幕坐标，非全屏=窗口坐标）。
void Floatee::paintChatAreaFor(QPainter &p, const QString &roleId,
                               const QPointF &anchor, float scale)
{
    // 收集该 Tee 的消息（倒序 = 新→旧）
    QVector<int> idxs;
    for (int i = m_chatBubbles.size() - 1; i >= 0; --i) {
        if (m_chatBubbles[i].roleId == roleId)
            idxs.append(i);
    }
    const bool hasInput = (roleId.isEmpty() && m_chatInput);
    if (idxs.isEmpty() && !hasInput)
        return;

    // 计算与绘制必须用同一字体：先设置到 painter，框宽才能精确包裹显示文本
    p.save();
    QFont f = p.font();
    f.setPixelSize(qRound(14.0 * scale));
    p.setFont(f);
    QFontMetrics fm(f);
    const int maxW = qRound(250.0 * scale);
    const int padX = qRound(8.0 * scale), padY = qRound(5.0 * scale);
    const int gap = qRound(4.0 * scale);
    const double bottomY = anchor.y() - qRound(46.0 * scale);   // 底部元素底部（上移避开 Tee 身体）

    // 输入框（最底，紧贴 Tee 上方）
    QSize inSize;
    if (hasInput) {
        const QString disp = m_chatInputText + m_chatPreedit + QLatin1Char(' ');
        const QRect tr = fm.boundingRect(QRect(0, 0, maxW, 1000), Qt::TextWordWrap, disp);
        inSize = QSize(tr.width() + padX * 2, tr.height() + padY * 2);
    }
    // 消息尺寸（0=最新）
    QVector<QSize> sizes;
    for (int i = 0; i < idxs.size(); ++i) {
        const QRect tr = fm.boundingRect(QRect(0, 0, maxW, 1000),
                                         Qt::TextWordWrap, m_chatBubbles[idxs[i]].text);
        sizes.append(QSize(tr.width() + padX * 2, tr.height() + padY * 2));
    }
    // 布局：从底部向上排（输入框 → 最新消息 → ... → 最旧）
    double y = bottomY;
    QRectF inputRect;
    if (!inSize.isEmpty()) {
        inputRect = QRectF(anchor.x() - inSize.width() / 2.0,
                           y - inSize.height(), inSize.width(), inSize.height());
        y -= inSize.height() + gap;
    }
    QVector<QRectF> rects;
    for (int i = 0; i < sizes.size(); ++i) {
        rects.append(QRectF(anchor.x() - sizes[i].width() / 2.0,
                            y - sizes[i].height(), sizes[i].width(), sizes[i].height()));
        y -= sizes[i].height() + gap;
    }

    // 绘制消息框（深色圆角，最后 2s 淡出）
    // rects 自底向上：0=最新（紧贴输入框/Tee 上方），与 idxs 一一对应
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    p.setPen(Qt::NoPen);
    for (int i = 0; i < rects.size(); ++i) {
        const auto &b = m_chatBubbles[idxs[i]];
        const qint64 age = now - b.startMs;
        int alpha = 210;
        if (age > 8000)
            alpha = qMax(0, qRound(210.0 * (10000.0 - age) / 2000.0));
        p.setBrush(QColor(30, 30, 40, alpha));
        p.drawRoundedRect(rects[i], 7.0 * scale, 7.0 * scale);
        p.setPen(QColor(255, 255, 255, alpha));
        p.drawText(rects[i].adjusted(padX, padY, -padX, -padY),
                   Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, b.text);
        p.setPen(Qt::NoPen);
    }
    // 绘制输入框（空白消息框 + 文本 + 闪烁光标）
    if (hasInput && !inputRect.isNull()) {
        p.setBrush(QColor(30, 30, 40, 210));
        p.drawRoundedRect(inputRect, 7.0 * scale, 7.0 * scale);
        const QString disp = m_chatInputText + m_chatPreedit;
        p.setPen(Qt::white);
        p.drawText(inputRect.adjusted(padX, padY, -padX, -padY),
                   Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, disp);
        if (m_chatCursorVisible) {
            const int w = fm.horizontalAdvance(disp);
            const int cx = qRound(inputRect.left() + padX + w) + 1;
            const int cy = qRound(inputRect.center().y());
            p.fillRect(QRect(cx, cy - qRound(8.0 * scale),
                             qMax(1, qRound(1.5 * scale)), qRound(16.0 * scale)),
                       QColor(255, 255, 255, 230));
        }
        // 记录输入框矩形（屏幕坐标），供 inputMethodQuery 定位 IME 候选框
        m_chatInputRect = inputRect;
        if (!m_fullscreenCanvas)
            m_chatInputRect.translate(QPointF(pos()));
        // 同步 IME 代理的位置到输入框（透明，仅承载输入法焦点与候选框定位）
        if (m_imeEdit) {
            const QPointF tl = m_fullscreenCanvas
                ? inputRect.topLeft() - QPointF(pos())
                : inputRect.topLeft();
            const QRect g(tl.toPoint(), inputRect.size().toSize());
            if (m_imeEdit->geometry() != g)
                m_imeEdit->setGeometry(g);
        }
    }
    p.restore();
}

void Floatee::paintChatBubbles(QPainter &p)
{
    // 全屏画布（painter 已平移到全局坐标）：为本地 + 每个远端 Tee 绘制各自消息区
    {
        const int cs = ExecTeeDrawer.canvasSize();
        const float ts = ExecTeeDrawer.teeSize();
        const QPointF anchor = m_localTeePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * ts);
        paintChatAreaFor(p, QString(), anchor, ts / 64.0f);
    }
    for (auto it = m_peersRender.constBegin(); it != m_peersRender.constEnd(); ++it) {
        if (!it->drawer || it->hidden)
            continue;
        const int pcs = it->drawer->canvasSize();
        const float pts = it->drawer->teeSize();
        const QPointF anchor = it->pos + QPointF(pcs / 2.0, pcs / 2.0 + 0.12 * pts);
        paintChatAreaFor(p, it.key(), anchor, pts / 64.0f);
    }
}

bool Floatee::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_imeEdit) {
        if (event->type() == QEvent::InputMethod) {
            // 捕获 IME 代理（QLineEdit）上的组合文本：Qt 6 的 QInputMethod 不提供
            // preeditString()，改为在事件层面读取 QInputMethodEvent，实时同步到自绘输入框。
            auto *ime = static_cast<QInputMethodEvent *>(event);
            m_chatPreedit = ime->preeditString();
            m_chatLastInputMs = QDateTime::currentMSecsSinceEpoch();   // 组合中也算输入活动
            update();
        } else if (event->type() == QEvent::Paint) {
            // 完全阻止 QLineEdit 绘制：其内置文本光标（caret）/选中高亮/背景全部由
            // 自绘输入框接管，否则会出现一个多余的小闪烁光标条。QLineEdit 仅承载
            // 输入法焦点与候选框定位，无需任何可见绘制。
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);   // 不拦截其余事件，QLineEdit 正常处理
}

void Floatee::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_chatInput) {
        QMainWindow::inputMethodEvent(event);
        return;
    }
    // IME：更新组合中文本（preedit）并提交确认文本（commit）
    m_chatPreedit = event->preeditString();
    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        m_chatInputText.insert(m_chatCursor, commit);
        m_chatCursor += commit.size();
    }
    // 处理组合期间的退格/删除（replacementLength）
    if (event->replacementLength() > 0) {
        m_chatCursor = qMax(0, m_chatCursor - event->replacementLength());
        m_chatInputText.remove(m_chatCursor, event->replacementLength());
    }
    // 光标属性（IME 请求把光标移到某个位置）
    for (const QInputMethodEvent::Attribute &attr : event->attributes()) {
        if (attr.type == QInputMethodEvent::Cursor) {
            m_chatCursor = qBound(0, attr.start, m_chatInputText.size());
            break;
        }
    }
    update();
    event->accept();
}

QVariant Floatee::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return m_chatInput;
    case Qt::ImFont: {
        QFont f = font();
        f.setPixelSize(14);
        return f;
    }
    case Qt::ImCursorRectangle: {
        // 返回输入框内光标位置（屏幕坐标），供输入法候选框定位
        const QRectF ir = chatInputScreenRect();
        if (ir.isNull())
            return QRect();
        QFont f = font();
        f.setPixelSize(14);
        const int w = QFontMetrics(f).horizontalAdvance(m_chatInputText + m_chatPreedit);
        return QRect(qRound(ir.left() + 8 + w), qRound(ir.center().y()) - 8, 2, 16);
    }
    default:
        break;
    }
    return QMainWindow::inputMethodQuery(query);
}

void Floatee::paintEmoticonsFullscreen(QPainter &p)
{
    for (const QString &key : EmoticonWin->activeKeys()) {
        QPointF teeCenter;
        float teeSize;
        if (key.isEmpty()) {
            const int cs = ExecTeeDrawer.canvasSize();
            teeSize = ExecTeeDrawer.teeSize();
            teeCenter = m_localTeePos + QPointF(cs / 2.0, cs / 2.0 + 0.12 * teeSize);
        } else {
            const auto it = m_peersRender.constFind(key);
            if (it == m_peersRender.constEnd() || !it->drawer || it->hidden)
                continue;
            const int pcs = it->drawer->canvasSize();
            teeSize = it->drawer->teeSize();
            teeCenter = it->pos + QPointF(pcs / 2.0, pcs / 2.0 + 0.12 * teeSize);
        }
        // 气泡 quad：中心在 Tee 上方 55*Scale、尺寸 64*Scale（Scale=teeSize/64）。
        // 渲染缓冲只包住气泡（±40*Scale），锚点使气泡中心落在缓冲中央。
        // 表情允许超出画布边界（Tee 身体已被 clampTeePos 限制在画布内，
        // 气泡在 Tee 上方越界是预期行为，即使被屏幕/窗口边缘裁断）。
        const float scale = teeSize / 64.0f;
        const double half = 40.0 * scale;
        const int box = qCeil(half * 2.0);
        const QPointF bubble(teeCenter.x(), teeCenter.y() - 55.0 * scale);
        // HiDPI：气泡帧按屏幕物理像素渲染（像素 = 逻辑 × dpr），避免放大失真
        const qreal fdpr = ExecTeeDrawer.devicePixelRatio();
        QPixmap frame(qRound(box * fdpr), qRound(box * fdpr));
        frame.setDevicePixelRatio(fdpr);
        // frame 内 TeePos：使气泡中心落在 frame 中央 (half, half)；表情尺寸
        // 跟随当前 teeSize，与缓冲盒（同一 teeSize 计算）一致，不裁断
        if (EmoticonWin->renderFrame(frame, key, QPointF(half, half + 55.0 * scale), teeSize))
            p.drawPixmap(qRound(bubble.x() - half), qRound(bubble.y() - half), frame);
    }
}

void Floatee::showPeerContextMenu(const QString &roleId, const QPoint &g)
{
    auto it = m_peersRender.find(roleId);
    if (it == m_peersRender.end())
        return;
    FloateeMenu menu(this);
    const bool isOwner = m_multi && !m_multi->ownerToken().isEmpty();
    const QString clientId = roleId.left(roleId.indexOf(QLatin1Char('/')));   // roleId: <clientId>/0
    QAction *hideAct = menu.addAction(it->hidden ? QStringLiteral("显示") : QStringLiteral("隐藏"));
    QAction *resetAct = menu.addAction(QStringLiteral("重置位置"));
    QAction *kickAct = isOwner ? menu.addAction(QStringLiteral("踢出")) : nullptr;
    QAction *sel = menu.exec(g);
    if (sel == hideAct) {
        it->hidden = !it->hidden;
        update();
    } else if (sel == resetAct) {
        const QPointF center = QRectF(screen()->availableGeometry()).center();
        if (it->drawer) {
            const int cs = it->drawer->canvasSize();
            it->pos = clampTeePos(center - QPointF(cs / 2.0, cs / 2.0), it->drawer->opaqueRect());
        }
        update();
    } else if (sel == kickAct && m_multi && !clientId.isEmpty()) {
        m_multi->kickMember(clientId);   // 需房主权限（菜单项仅房主显示）
    }
}

void Floatee::onRandomEmoticonTick()
{
    if (m_sleeping)   // M7：休眠中不随机冒表情
        return;
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

// 多屏幕兜底：把窗口放回真实可见区域，防止上下屏间隙/拔屏后窗口丢失。
// 实现要点（macOS 多屏布局下特别重要）：
//   1. 用 screen->geometry() 而非 availableGeometry()——后者排除任务栏/Dock，
//      多屏上下布局下任务栏常驻其中一块屏，会把可用区域切得极小，
//      居中后窗口反而跨进间隙或落到底部不可见区域。
//   2. 选屏：优先选与当前窗口重叠面积最大的屏；无重叠则按 curCenter 欧氏距离最近。
//   3. setGeometry 而非 move——保证位置+尺寸原子提交，避免 WM 二次调整。
//   4. move 后验证：结果必须完全落在某块屏的 geometry 内，否则改用主屏几何
//      强制居中（兜底）。
void Floatee::resetWindowPosition()
{
    const QSize sz = size();
    const QRect curRect(pos(), sz);
    const QPoint curCenter = curRect.center();
    const auto screens = QGuiApplication::screens();
    // 临时调试：始终打印（用户分析用，定位后移除）
    qDebug() << "[ResetDebug] trigger curRect=" << curRect
             << "m_fullscreenCanvas=" << m_fullscreenCanvas
             << "frameGeometry=" << frameGeometry();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *s = screens[i];
        qDebug() << "[ResetDebug]   screen" << i << s->name()
                 << "geo=" << s->geometry() << "avail=" << s->availableGeometry();
    }

    // ── 全屏画布模式：窗口本身覆盖某块屏，问题不在窗口位置，而在画布内
    //   Tee 的 m_localTeePos 跑到屏外/极端位置。直接移动 Tee 到窗口中心。
    // 关键：m_localTeePos 是全局屏幕坐标，渲染时通过 painter.translate(-pos())
    // 偏移回窗口内，所以 Tee 显示位置 = m_localTeePos - pos()。
    // 因此 target 必须用窗口自身 geometry（=当前所在屏幕），而非 primaryScreen：
    //   - 若窗口在 secondary（pos=(-401,-1415)），用 secondary 几何
    //   - 若窗口在 primary，用 primary 几何
    // 否则在 secondary 屏时会把 m_localTeePos 设为 primary 中心，减去 secondary
    // pos 后实际显示坐标远在窗口外（用户反馈"看不见 Tee"）。
    if (m_fullscreenCanvas) {
        const QRect winGeo = geometry();   // 当前窗口（=当前所在屏幕）
        const QPointF winCenter(winGeo.center());
        m_localTeePos = QPointF(winCenter.x() - kWinW / 2.0,
                                winCenter.y() - kWinH / 2.0);
        clampTeePos(m_localTeePos,
                    QRect(0, 0, ExecTeeDrawer.canvasSize(), ExecTeeDrawer.canvasSize()));
        update();
        qDebug() << "[ResetDebug] fullscreen mode, winGeo=" << winGeo
                 << "moved Tee to" << m_localTeePos;
        raise();
        activateWindow();
        ElMessageBar::success(ElMessageBar::Position::TopRight,
                              QStringLiteral("位置"),
                              QStringLiteral("已重置 Tee 到屏幕中心"), 2000, this);
        return;
    }

    // ── 小窗口模式：按"重叠面积最大优先，否则最近"选屏，居中+clamp ──
    QScreen *scr = nullptr;
    int bestOverlap = 0;
    for (QScreen *s : screens) {
        const QRect g = s->geometry();
        const QRect inter = g.intersected(curRect);
        const int area = inter.width() * inter.height();
        if (area > bestOverlap) { bestOverlap = area; scr = s; }
    }
    if (bestOverlap <= 0) {
        qint64 bestD = std::numeric_limits<qint64>::max();
        for (QScreen *s : screens) {
            const QPoint sc = s->geometry().center();
            const qint64 dx = qint64(sc.x()) - curCenter.x();
            const qint64 dy = qint64(sc.y()) - curCenter.y();
            const qint64 d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; scr = s; }
        }
    }
    if (!scr) {
        scr = QGuiApplication::primaryScreen();
        if (!scr) return;
    }
    const QRect g = scr->geometry();
    QPoint p(g.center().x() - sz.width()  / 2,
             g.center().y() - sz.height() / 2);
    if (!m_profile.isEmpty())
        p += QPoint(60, 60);
    p.setX(qBound(g.left(),  p.x(), qMax(g.left(),  g.right()  - sz.width()  + 1)));
    p.setY(qBound(g.top(),   p.y(), qMax(g.top(),   g.bottom() - sz.height() + 1)));
    qDebug() << "[ResetDebug] chose screen" << scr->name()
             << "geo=" << g << "picked p=" << p << "sz=" << sz;
    setGeometry(QRect(p, sz));
    qDebug() << "[ResetDebug] after setGeometry pos=" << pos() << "size=" << size();
    bool fullyVisible = false;
    for (QScreen *s : QGuiApplication::screens()) {
        if (s->geometry().intersected(QRect(p, sz)) == QRect(p, sz)) {
            fullyVisible = true; break;
        }
    }
    if (!fullyVisible) {
        qDebug() << "[ResetDebug] NOT fully visible, fallback to primary";
        QScreen *pri = QGuiApplication::primaryScreen();
        if (pri) {
            const QRect pg = pri->geometry();
            const QPoint pp(pg.center().x() - sz.width()  / 2,
                            pg.center().y() - sz.height() / 2);
            setGeometry(QRect(pp, sz));
            qDebug() << "[ResetDebug] fallback pos=" << pos() << "size=" << size();
        }
    }
    raise();
    activateWindow();
    ElMessageBar::success(ElMessageBar::Position::TopRight,
                          QStringLiteral("位置"),
                          QStringLiteral("已重置窗口位置"), 2000, this);
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
    // load() keeps the current render scale, but re-assert it anyway so the
    // drawer and SizeScale can never desync — otherwise wheel zoom-in appears
    // dead right after a skin switch until the user zooms out once.
    ExecTeeDrawer.setRenderScale(SizeScale);
    CurrentSkin = path;

    // M2：联机时上报皮肤名（远端回落 default）
    if (m_multi && m_multi->isConnected() && m_multi->inRoom())
        m_multi->updateLocalSkin(currentSkinName());

    const bool wasVisible = isVisible();
    if (wasVisible)
        hide();

    RenderedEye = -1;
    updateEyeFollow();   // re-renders the full tee with current eye + cursor dir
    TrayIcon.setIcon(makeTrayIcon());
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    if (wasVisible)
        show();

    Setup["Skin"] = path;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

void Floatee::switchEmoticonSet(QAction *action)
{
    QString path = action->data().toString();
    if (path.isEmpty() || path == EmoticonSet)
        return;
    if (!EmoticonWin || !EmoticonWin->loadAtlas(QPixmap(path)))
        return;   // 图集加载失败则不切换
    EmoticonSet = path;
    Setup["EmoticonSet"] = path;
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
    if (EmoticonWin)
        EmoticonWin->setFeatherStrength(f);   // 表情气泡与 Tee 共用同一 Feather 设置
    RenderedEye = -1;
    updateEyeFollow();           // re-render with the new feather strength
    TrayIcon.setIcon(makeTrayIcon());
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    Setup["Feather"] = f;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}

// ── online 分支：Multiplayer 托盘菜单 ────────────────────────────────

void Floatee::mpConnect()
{
    if (!m_multi) return;
    const QString cur = Setup.value("multiplayer").toObject()
                            .value("server").toString(QStringLiteral("127.0.0.1:8764"));
    bool ok = false;
    const QString server = QInputDialog::getText(this, QStringLiteral("Connect"),
        QStringLiteral("服务器地址 (host:port):"), QLineEdit::Normal, cur, &ok);
    if (!ok || server.trimmed().isEmpty())
        return;
    const QString s = server.trimmed();
    const int colon = s.lastIndexOf(QLatin1Char(':'));
    QString host = s;
    quint16 port = 8764;
    if (colon > 0) {
        host = s.left(colon);
        const quint16 p = s.mid(colon + 1).toUShort();
        port = p != 0 ? p : 8764;
    }
    // 记住服务器地址
    QJsonObject mp = Setup.value("multiplayer").toObject();
    mp.insert("server", s);
    Setup.insert("multiplayer", mp);
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
    m_multi->connectTo(host, port);
}

void Floatee::mpDisconnect()
{
    if (m_multi) m_multi->disconnect();
}

void Floatee::tryAutoConnectLastServer()
{
    if (!m_multi)
        return;
    const QJsonObject mp = Setup.value("multiplayer").toObject();
    if (!mp.contains(QStringLiteral("server")))
        return;                       // 从未连接过 → 不自动连接
    const QString s = mp.value(QStringLiteral("server")).toString().trimmed();
    if (s.isEmpty())
        return;
    // 解析 host:port（与 mpConnect 相同规则）
    const int colon = s.lastIndexOf(QLatin1Char(':'));
    QString host = s;
    quint16 port = 8764;
    if (colon > 0) {
        host = s.left(colon);
        const quint16 p = s.mid(colon + 1).toUShort();
        port = p != 0 ? p : 8764;
    }
    m_multi->connectTo(host, port);
    m_autoConnecting = true;   // 自动连接结果弹窗 2s 自动关闭
}

void Floatee::mpCreateRoom()
{
    if (!m_multi) return;
    // Step3：Fluent 风格对话框（无边框圆角 + 可拖动标题栏 + 阴影）
    ElDialog dlg(QStringLiteral("创建房间"), this);
    dlg.resize(420, 240);
    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(QStringLiteral("房间名（可选）"));
    auto *pubCheck = new QCheckBox(QStringLiteral("公共房间（出现在房间列表，可被直接加入）"), &dlg);
    auto *pwdEdit = new QLineEdit(&dlg);
    pwdEdit->setPlaceholderText(QStringLiteral("密码（可选，留空则无密码）"));
    pwdEdit->setEchoMode(QLineEdit::Password);
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *okBtn = new ElButton(QStringLiteral("创建"), ElButton::Kind::Primary, &dlg);
    auto *cancelBtn = new ElButton(QStringLiteral("取消"), ElButton::Kind::Standard, &dlg);
    connect(okBtn, &ElButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &ElButton::clicked, &dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    dlg.contentLayout()->addWidget(nameEdit);
    dlg.contentLayout()->addWidget(pubCheck);
    dlg.contentLayout()->addWidget(pwdEdit);
    dlg.contentLayout()->addStretch();
    dlg.contentLayout()->addLayout(btnRow);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_multi->createRoom(nameEdit->text().trimmed(), pubCheck->isChecked(),
                        pwdEdit->text());
}

void Floatee::mpJoinRoom()
{
    if (!m_multi) return;
    bool ok1 = false;
    const QString rid = QInputDialog::getText(this, QStringLiteral("Join Room"),
        QStringLiteral("房间号:"), QLineEdit::Normal, QString(), &ok1);
    if (!ok1 || rid.trimmed().isEmpty())
        return;
    bool ok2 = false;
    const QString pw = QInputDialog::getText(this, QStringLiteral("Join Room"),
        QStringLiteral("密码（可选，无密码可留空）:"), QLineEdit::Password, QString(), &ok2);
    if (!ok2) return;
    m_multi->joinRoom(rid.trimmed(), pw);
}

void Floatee::mpShowJoinCode()
{
    if (!m_multi || !m_multi->inRoom()) {
        ElMessageBar::information(ElMessageBar::Position::TopRight,
                                  QStringLiteral("Multiplayer"),
                                  QStringLiteral("当前不在房间中"), 3000, this);
        return;
    }
    const QString pw = m_multi->joinCode();
    ElMessageBar::success(ElMessageBar::Position::TopRight,
                          QStringLiteral("房间信息"),
                          QStringLiteral("房间号: %1  密码: %2")
                              .arg(m_multi->roomId(),
                                   pw.isEmpty() ? QStringLiteral("（无密码，凭房间号即可加入）") : pw),
                          6000, this);
}

void Floatee::mpRoomList()
{
    if (m_multi) m_multi->listRooms();
}

void Floatee::showRoomListDialog(const QList<QJsonObject> &rooms)
{
    if (!m_multi) return;
    // Step3：Fluent 风格房间列表（卡片式列表 + 双击加入）
    ElDialog dlg(QStringLiteral("公共房间"), this);
    dlg.resize(480, 380);
    auto *list = new QListWidget(&dlg);
    for (const QJsonObject &r : rooms) {
        const QString label = QStringLiteral("%1  %2  (%3/%4)  %5")
            .arg(r.value("roomId").toString(), r.value("roomName").toString())
            .arg(r.value("members").toInt()).arg(r.value("capacity").toInt())
            .arg(r.value("hasPassword").toBool() ? QStringLiteral("🔒 需密码") : QStringLiteral("公开"));
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, r.value("roomId").toString());
        item->setData(Qt::UserRole + 1, r.value("hasPassword").toBool());
    }
    if (rooms.isEmpty())
        new QListWidgetItem(QStringLiteral("（当前没有公共房间）"), list);
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *refreshBtn = new ElButton(QStringLiteral("刷新"), ElButton::Kind::Standard, &dlg);
    auto *closeBtn = new ElButton(QStringLiteral("关闭"), ElButton::Kind::Standard, &dlg);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(closeBtn);
    dlg.contentLayout()->addWidget(new QLabel(QStringLiteral("双击公共房间加入；带 🔒 需输入密码"), &dlg));
    dlg.contentLayout()->addWidget(list, 1);
    dlg.contentLayout()->addLayout(btnRow);

    // 双击加入：有密码则提示输入，否则直接加入
    connect(list, &QListWidget::itemDoubleClicked, &dlg, [this, &dlg, list](QListWidgetItem *item) {
        const QString roomId = item->data(Qt::UserRole).toString();
        const bool hasPwd = item->data(Qt::UserRole + 1).toBool();
        dlg.done(QDialog::Accepted);
        if (hasPwd) {
            bool ok = false;
            const QString pw = QInputDialog::getText(this, QStringLiteral("加入公共房间"),
                QStringLiteral("该房间需要密码："), QLineEdit::Password, QString(), &ok);
            if (!ok) return;
            m_multi->joinRoom(roomId, pw);
        } else {
            m_multi->joinRoom(roomId);
        }
    });
    // 刷新：关闭当前对话框并重新查询（回调会再次弹出）
    connect(refreshBtn, &ElButton::clicked, &dlg, [this, &dlg]() {
        dlg.done(QDialog::Accepted);
        m_multi->listRooms();
    });
    connect(closeBtn, &ElButton::clicked, &dlg, &QDialog::reject);
    dlg.exec();
}

// ── M7：休眠 + 使用时长提醒（借鉴 DDNet AFK 模型）──────────────────

void Floatee::noteActivity()
{
    m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    if (m_sleeping)
        wakeUp();
}

void Floatee::enterSleep()
{
    if (m_sleeping)
        return;
    m_sleeping = true;
    m_sleepStartMs = QDateTime::currentMSecsSinceEpoch();
    m_zzzPhaseMs = 0;
    // 省电：休眠中无需高频重绘，跟眼降到 2s
    if (EyeFollowTimer)
        EyeFollowTimer->setInterval(2000);
    RenderedEye = -1;
    updateEyeFollow();   // 闭眼（EMOTE_BLINK）渲染
    update();
}

void Floatee::wakeUp()
{
    if (!m_sleeping)
        return;
    m_sleeping = false;
    // 长休眠 = 新会话：清空累计使用时长，重新计时（夜间睡眠自然重置）
    if (m_resetAfterSleepMin > 0) {
        const qint64 dur = QDateTime::currentMSecsSinceEpoch() - m_sleepStartMs;
        if (dur >= qint64(m_resetAfterSleepMin) * 60 * 1000)
            m_usageSeconds = 0;
    }
    // 恢复跟眼频率（全屏 33ms / 非全屏 16ms）
    if (EyeFollowTimer)
        EyeFollowTimer->setInterval(m_fullscreenCanvas ? 33 : 16);
    RenderedEye = -1;
    updateEyeFollow();
    update();
}

void Floatee::onAfkTick()
{
    writeRuntimeLog();   // 每分钟状态 / 每 10 分钟系统时间

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 dt = now - m_lastTickWallMs;
    m_lastTickWallMs = now;
    // 系统睡眠/合盖：tick 间隔异常大（正常 1s）。唤醒后系统时间已校正回真实时间，
    // dt 包含睡眠时长 —— 达到阈值视为「新会话」，清空累计使用时长重新计时。
    // 也覆盖 debugger 长时间挂起等场景。
    if (m_resetAfterSleepMin > 0 && dt >= qint64(m_resetAfterSleepMin) * 60 * 1000)
        m_usageSeconds = 0;

    // 光标位置变化也算活动（全屏穿透窗口收不到 mouseMoveEvent）
    const QPoint cp = QCursor::pos();
    if (cp != m_lastCursorPos) {
        m_lastCursorPos = cp;
        noteActivity();
    }
    if (m_sleeping) {
        m_zzzPhaseMs += 1000;   // zzz 呼吸动画相位（驱动 update 重绘）
        update();
        return;
    }
    // 休眠检测：无活动超过阈值 → 休眠
    if (m_sleepTimeoutSec > 0
        && now - m_lastActivityMs >= qint64(m_sleepTimeoutSec) * 1000) {
        enterSleep();
        return;
    }
    // 使用时长统计（非休眠时每秒累加；提醒关闭也照常记录）+
    // 定期休息提醒（达间隔归零）
    if (!m_sleeping) {
        m_usageSeconds++;
        if (m_breakReminderMin > 0 && m_usageSeconds >= m_breakReminderMin * 60) {
            m_usageSeconds = 0;
            showBreakReminder();
        }
    }
    // 刷新 Sleep & Break 菜单里的累计使用时长（分钟）
    if (m_usageDisplayAction) {
        const int mins = m_usageSeconds / 60;
        const QString label = QStringLiteral("Usage: %1 min%2")
            .arg(mins).arg(mins == 1 ? QString() : QStringLiteral("s"));
        if (m_usageDisplayAction->text() != label)
            m_usageDisplayAction->setText(label);
    }
}

void Floatee::initRuntimeLog()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logDir = dir + QLatin1String("/logs");
    QDir().mkpath(logDir);
    m_logFile.setFileName(logDir + QLatin1String("/floatee_runtime.log"));
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        qWarning() << "Floatee: cannot open runtime log" << m_logFile.fileName();
    else {
        m_logFile.write(QStringLiteral("=== Floatee runtime log start ===\n").toUtf8());
        m_logFile.flush();   // 立即落盘，便于随时查看
    }
}

void Floatee::writeRuntimeLog()
{
    if (!m_logFile.isOpen())
        return;
    ++m_logTick;
    const QDateTime now = QDateTime::currentDateTime();
    const bool doMinute = (m_logTick % 60 == 0);
    if (doMinute) {
        // 每 1 分钟：当前运行状态（是否休眠 + 已累计使用秒数）
        m_logFile.write(QStringLiteral("[%1] sleeping=%2 usageSeconds=%3\n")
            .arg(now.toString(QStringLiteral("HH:mm:ss")),
                 m_sleeping ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(m_usageSeconds).toUtf8());
    }
    if (m_logTick % 600 == 0) {
        // 每 10 分钟：当前系统时间
        m_logFile.write(QStringLiteral("[%1] system time: %2\n")
            .arg(now.toString(QStringLiteral("HH:mm:ss")),
                 now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))).toUtf8());
    }
    if (doMinute)
        m_logFile.flush();
}

void Floatee::showBreakReminder()
{
    addChatMessage(QString(),
                   QStringLiteral("已使用 %1 分钟，起来休息一下吧 😴").arg(m_breakReminderMin));
}

void Floatee::paintAfkZzz(QPainter &p, const QPointF &teeCenter, float teeSize)
{
    const float scale = teeSize / 64.0f;
    // zzz 位于 TeePos + (24,-40)*scale，加上 zzz 自身半高(~32*scale)与上浮动画
    // (~5*scale)，顶部最大到 -77*scale → 缓冲半宽取 96*scale 防截断
    const double half = 96.0 * scale;
    const int box = qCeil(half * 2.0);
    const qreal fdpr = ExecTeeDrawer.devicePixelRatio();
    QPixmap frame(qRound(box * fdpr), qRound(box * fdpr));
    frame.setDevicePixelRatio(fdpr);
    // frame 内 TeePos = 缓冲中心；RenderAfkZzz 在 TeePos + (24,-40)*scale 画 zzz
    if (EmoticonWin->renderAfkZzzFrame(frame, QPointF(half, half), teeSize,
                                       m_zzzPhaseMs / 1000.0f))
        p.drawPixmap(qRound(teeCenter.x() - half), qRound(teeCenter.y() - half), frame);
}

// ── M2：全屏画布 / Peer 渲染 / 交互 ────────────────────────────────

QString Floatee::currentSkinName() const
{
    return QFileInfo(CurrentSkin).fileName();
}

// 把远端皮肤文件名解析为本地可加载路径（内置/皮肤库/应用目录，找不到回落 default）
static QString resolveSkinPath(const QString &name)
{
    if (name.isEmpty())
        return TeeDrawer::defaultSkinPath();
    const QString qrc = QStringLiteral(":/skins/") + name;
    if (QFile::exists(qrc))
        return qrc;
    const QString lib = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/skins/") + name;
    if (QFile::exists(lib))
        return lib;
    const QString app = QCoreApplication::applicationDirPath() + QStringLiteral("/skins/") + name;
    if (QFile::exists(app))
        return app;
    return TeeDrawer::defaultSkinPath();
}

void Floatee::onRoomChangedMp()
{
    if (!m_multi)
        return;
    if (m_multi->inRoom() && m_multi->isConnected()) {
        // 进房间：保持全屏画布（启动即全屏），注册本地角色；远端 Tee 由
        // onPeersChangedMp 渲染。
        if (EyeFollowTimer) EyeFollowTimer->setInterval(33);
        m_peersRender.clear();
        m_multi->addLocalRole(currentSkinName(), HueShift, SatFactor, LightFactor);   // 注册本地角色（含皮肤名 + HSL）
        update();
    } else {
        // 离开房间：保持全屏画布（单机也全屏），清理远端渲染
        if (EmoticonWin) EmoticonWin->hideEmoticon();
        if (m_emoticonWheel) { m_emoticonWheel->close(); unsetCursor(); }
        if (EyeFollowTimer) EyeFollowTimer->setInterval(33);
        m_peersRender.clear();
        m_dragRoleId.clear();
        update();
    }
}

void Floatee::onPeersChangedMp()
{
    if (!m_multi)
        return;
    const auto &peers = m_multi->peers();
    bool changed = false;   // 是否有实际视觉变化（避免每次 peer_mouse 都整屏重绘）
    // 移除已消失的角色
    for (auto it = m_peersRender.begin(); it != m_peersRender.end();) {
        if (peers.contains(it.key()))
            ++it;
        else {
            it = m_peersRender.erase(it);
            changed = true;
        }
    }
    // 新增/更新
    const QPointF center = QRectF(screen()->availableGeometry()).center();
    int idx = 0;
    for (auto it = peers.constBegin(); it != peers.constEnd(); ++it, ++idx) {
        const Multiplayer::PeerInfo &info = it.value();
        auto r = m_peersRender.find(it.key());
        if (r == m_peersRender.end()) {
            PeerRender pr;
            // 只创建一次（shared_ptr），避免 TeeDrawer 浅拷贝悬垂；
            // 用远端同步的皮肤名 + HSL 调整加载（新 Tee 加入时同步 HSL）
            pr.drawer = std::make_shared<TeeDrawer>(resolveSkinPath(info.skin));
            if (info.hue != 0 || info.sat != 1.0 || info.light != 1.0)
                pr.drawer->load(resolveSkinPath(info.skin), info.hue, info.sat, info.light);
            pr.drawer->setFastMode(true);   // 远端轻量渲染，避免弱设备事件循环饿死
            pr.drawer->setDevicePixelRatio(ExecTeeDrawer.devicePixelRatio());  // HiDPI 一致
            pr.skin = info.skin;
            pr.eye = info.eye;
            pr.eyeScale = info.es;
            const float len = std::hypot(info.dx, info.dy);
            if (len > 1.0f)
                pr.dir = QPointF(info.dx / len, info.dy / len);
            // 先渲染 body（更新实际像素包围盒），再按包围盒钳制初始位置
            if (pr.drawer) {
                pr.drawer->renderBody(pr.body);
                QPointF np = center - QPointF(pr.drawer->canvasSize() / 2.0,
                                              pr.drawer->canvasSize() / 2.0);
                np += QPointF(40.0 * idx, 40.0 * idx);   // 错开避免完全重叠
                pr.pos = clampTeePos(np, pr.drawer->opaqueRect());
                pr.drawer->renderEyes(pr.eyes, pr.eye, pr.dir.x(), pr.dir.y(), pr.eyeScale);
                pr.lastEye = pr.eye;
                pr.lastEyeScale = pr.eyeScale;
                pr.lastDir = pr.dir;
            }
            m_peersRender.insert(it.key(), pr);
            changed = true;   // 新 peer 出现 → 需要重绘
        } else {
            r->eye = info.eye;
            r->eyeScale = info.es;   // es=0 时眼睛回到中心
            const float len = std::hypot(info.dx, info.dy);
            if (len > 1.0f)
                r->dir = QPointF(info.dx / len, info.dy / len);
            if (r->drawer) {
                // 皮肤变化：重载皮肤并重建静态 body 层（强制重建眼睛层）
                if (r->skin != info.skin) {
                    r->skin = info.skin;
                    r->drawer->load(resolveSkinPath(info.skin));
                    r->drawer->renderBody(r->body);
                    r->lastEye = -1;   // 眼睛颜色/纹理来自皮肤，强制重建
                    changed = true;    // body 重建 → 需要重绘
                }
                // 仅数据变化时重渲染眼睛层（带容差，避免鼠标微动反复重渲染）
                const float dirDelta = std::hypot(r->dir.x() - r->lastDir.x(),
                                                  r->dir.y() - r->lastDir.y());
                if (r->eye != r->lastEye ||
                    std::abs(r->eyeScale - r->lastEyeScale) > 0.02f ||
                    dirDelta > 0.04f) {
                    r->drawer->renderEyes(r->eyes, r->eye, r->dir.x(), r->dir.y(),
                                          r->eyeScale);
                    // 若该远端 Tee 正在走路，同步更新走路帧的眼睛
                    if (r->walkPhase >= 0.0f) {
                        r->drawer->render(r->eye, r->dir.x(), r->dir.y(),
                                          r->walkPhase, r->eyeScale);
                        r->walkPixmap = r->drawer->Tee;
                    }
                    r->lastEye = r->eye;
                    r->lastEyeScale = r->eyeScale;
                    r->lastDir = r->dir;
                    changed = true;    // 眼睛重渲染 → 需要重绘
                }
            }
        }
    }
    // 仅在有实际视觉变化时重绘：peer_mouse 高频到达时，若眼睛在容差内
    // （微动）或数据未变，跳过整屏重绘可显著降低弱设备负载
    if (changed)
        update();
}

QPointF Floatee::clampTeePos(QPointF pos, const QRect &opaqueBox) const
{
    if (opaqueBox.isNull() || opaqueBox.isEmpty())
        return pos;   // 无渲染像素 → 不钳制
    // opaqueBox 相对 canvas 左上角；实际渲染像素范围 = [pos+left, pos+right]
    // （QRect left/right 均含）。要求完全落在画布 [0, W-1]×[0, H-1] 内。
    // 全屏画布：pos 是全局（屏幕）坐标，钳制在「窗口全局位置 + 尺寸」范围内；
    // 非全屏：pos 是窗口内坐标，钳制在窗口矩形内。
    // opaqueBox 是渲染像素坐标，而 pos/width/height 是逻辑坐标：按 dpr 换算成逻辑。
    const double dpr = ExecTeeDrawer.devicePixelRatio();
    const double l = opaqueBox.left() / dpr, t = opaqueBox.top() / dpr;
    const double r = opaqueBox.right() / dpr, b = opaqueBox.bottom() / dpr;
    double minX, minY, maxX, maxY;
    if (m_fullscreenCanvas) {
        const double ox = this->pos().x(), oy = this->pos().y();
        minX = ox - l;
        minY = oy - t;
        maxX = ox + double(width()) - 1 - r;
        maxY = oy + double(height()) - 1 - b;
    } else {
        minX = -l;
        minY = -t;
        maxX = double(width()) - 1 - r;
        maxY = double(height()) - 1 - b;
    }
    pos.setX(qBound(minX, pos.x(), qMax(minX, maxX)));
    pos.setY(qBound(minY, pos.y(), qMax(minY, maxY)));
    return pos;
}

bool Floatee::hitTestTee(const QPoint &g, QString *outRoleId, int pad) const
{
    const QPointF gf(g);
    if (m_fullscreenCanvas) {
        // 用 Tee 实际不透明像素包围盒（opaqueRect，相对 canvas）做命中判定，
        // 而不是整个 canvas 正方形——否则 canvas 内 Tee 外的透明区域也会被
        // 判定为"命中"（交互区域变成一个正方形，点击 Tee 周围空白被窗口
        // 拦截而无法穿透到后面窗口）。pad 只做边缘微容差。
        const QRect opaque = ExecTeeDrawer.opaqueRect();
        if (!opaque.isNull() && !opaque.isEmpty()) {
            // 像素包围盒 → 逻辑坐标（HiDPI 下 opaque 是物理像素）
            const double dpr = ExecTeeDrawer.devicePixelRatio();
            const QRectF o(opaque.x() / dpr, opaque.y() / dpr,
                           opaque.width() / dpr, opaque.height() / dpr);
            if (o.adjusted(-pad, -pad, pad, pad).contains(gf - m_localTeePos)) {
                if (outRoleId) outRoleId->clear();   // 空 = 本地 Tee
                return true;
            }
        } else {
            // 兜底：无渲染像素时退回 canvas 矩形
            const int lcs = ExecTeeDrawer.canvasSize();
            if (QRectF(m_localTeePos, QSizeF(lcs + 2 * pad, lcs + 2 * pad))
                    .adjusted(-pad, -pad, pad, pad).contains(gf)) {
                if (outRoleId) outRoleId->clear();
                return true;
            }
        }
    }
    for (auto it = m_peersRender.constBegin(); it != m_peersRender.constEnd(); ++it) {
        if (!it->drawer || it->hidden) continue;
        // 远端 Tee 同样用各自 drawer 的 opaqueRect（贴合形状，而非 canvas 正方形）
        const QRect opaque = it->drawer->opaqueRect();
        if (opaque.isNull() || opaque.isEmpty()) {
            const int cs = it->drawer->canvasSize();
            if (cs > 0 && QRectF(it->pos, QSizeF(cs, cs)).adjusted(-pad, -pad, pad, pad).contains(gf)) {
                if (outRoleId) *outRoleId = it.key();
                return true;
            }
        } else {
            // 像素包围盒 → 逻辑坐标（HiDPI 下 opaque 是物理像素）
            const double dpr = it->drawer->devicePixelRatio();
            const QRectF o(opaque.x() / dpr, opaque.y() / dpr,
                           opaque.width() / dpr, opaque.height() / dpr);
            if (o.adjusted(-pad, -pad, pad, pad).contains(gf - it->pos)) {
                if (outRoleId) *outRoleId = it.key();
                return true;
            }
        }
    }
    return false;
}

void Floatee::onHitTestTick()
{
    if (!m_fullscreenCanvas)
        return;
    // 计算期望的穿透状态；拖拽中锁定为可交互（否则鼠标移出命中框就会
    // 重新穿透，窗口收不到 mouseMove 导致拖拽中断）。
    bool want;
    if (m_dragging) {
        want = false;
    } else if (m_emoticonWheel && m_emoticonWheel->isOpen()) {
        // 表情圆盘打开：命中 = Tee 身体 ∪ 圆盘范围（否则圆盘大部分区域在
        // Tee 身体之外，会被判定为穿透而无法点击表情）。
        const QPoint cp = QCursor::pos();
        want = !(hitTestTee(cp, nullptr, 14) || m_emoticonWheel->contains(cp));
    } else {
        want = !hitTestTee(QCursor::pos(), nullptr, 14);
    }
    // 仅在状态真正变化时应用穿透：反复设置同一值会让 Qt 频繁做窗口
    // 系统样式操作（WS_EX_LAYERED 上尤其重），是主线程挂起(Application Hang)
    // 的重要来源。
    if (want != m_transparent) {
        m_transparent = want;
        const QPoint cp = QCursor::pos();
        dbgWin(QStringLiteral("[hit] transparent=%1 apply mouse=%2x%3 m_localTeePos=%4x%5 canvas=%6 opaque=%7x%8+%9+%10")
            .arg(want).arg(cp.x()).arg(cp.y())
            .arg(m_localTeePos.x()).arg(m_localTeePos.y())
            .arg(ExecTeeDrawer.canvasSize())
            .arg(ExecTeeDrawer.opaqueRect().width()).arg(ExecTeeDrawer.opaqueRect().height())
            .arg(ExecTeeDrawer.opaqueRect().x()).arg(ExecTeeDrawer.opaqueRect().y()));
        // macOS：WA_TransparentForMouseEvents / WindowTransparentForInput 的
        // native 运行时切换不可靠（QNSView 的 isTransparentForUserInput 只
        // 看 window flag，而 ignoresMouseEvents 设置 false 后不会自动恢复
        // 接收），因此由平台层直接操作 NSWindow.ignoresMouseEvents。其余平台
        // 沿用 widget attribute。
#ifdef Q_OS_MACOS
        if (!m_platformInfo)
            m_platformInfo = PlatformWindowInfo::create();
        if (m_platformInfo)
            m_platformInfo->setWindowClickThrough(
                reinterpret_cast<void *>(winId()), want);
#else
        setAttribute(Qt::WA_TransparentForMouseEvents, want);
#endif
    }
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
        InstanceMenu = new FloateeMenu("Instance");
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
    CurrentEye = qBound(0, Setup.value("Eye").toInt(0), 5);
    ExecTeeDrawer.setFeatherStrength(qBound(0, Setup.value("Feather").toInt(1), 2));
    if (EmoticonWin)
        EmoticonWin->setFeatherStrength(ExecTeeDrawer.featherStrength());
    SizeScale = qBound(0.5, Setup.value("Size").toDouble(1.0), 2.0);
    ExecTeeDrawer.setRenderScale(SizeScale);
    // 非全屏消息区扩展时保留 m_chatAreaH 的垂直偏移
    m_teePos = QPointF((kWinW - ExecTeeDrawer.canvasSize()) / 2.0, kEmoticonTop + m_chatAreaH);

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
    TrayIcon.setIcon(makeTrayIcon());
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
        // 非全屏消息区扩展时，Tee 的窗口内下边界下移 m_chatAreaH
        newPos.setY(qBound(double(kEmoticonTop + m_chatAreaH), newPos.y(),
                           double(kWinH + m_chatAreaH - newCs)));
        m_teePos = newPos;
    } else {
        // Menu path: horizontally centred, vertically at the bottom below the
        // emoticon band.
        m_teePos = QPointF((kWinW - newCs) / 2.0, kEmoticonTop + m_chatAreaH);
    }

    RenderedEye = -1;
    updateEyeFollow();           // re-render the tee at the new scale
    repaint();                   // content-only update, painted synchronously

    TrayIcon.setIcon(makeTrayIcon());
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    // Sync the Size menu checkmark (a menu click relies on the QActionGroup's
    // exclusivity; wheel-zoom needs the check updated manually).
    if (SizeGroup) {
        for (QAction *a : SizeGroup->actions())
            a->setChecked(qFuzzyCompare(a->data().toDouble(), s));
    }

    applyTeeDpr();   // HiDPI：缩放后同步渲染像素比（跨屏时 dpr 可能已变）

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
    noteActivity();   // M7：滚轮算活动
    // M2 全屏画布：滚轮缩放光标悬停的 Tee，**以鼠标为锚点**（鼠标指向的 Tee 点保持不动）
    if (m_fullscreenCanvas) {
        const QPoint g = QCursor::pos();
        QString roleId;
        if (hitTestTee(g, &roleId)) {
            const double f = event->angleDelta().y() > 0 ? 1.1 : 0.9;
            if (roleId.isEmpty()) {
                const int oldCs = ExecTeeDrawer.canvasSize();
                const QPointF anchor = QPointF(g) - m_localTeePos;   // 鼠标相对 Tee 左上角
                const double ns = qBound(0.5, SizeScale * f, 2.0);
                if (!qFuzzyCompare(ns, SizeScale)) {
                    SizeScale = ns;
                    ExecTeeDrawer.setRenderScale(SizeScale);
                    const int newCs = ExecTeeDrawer.canvasSize();
                    // 先按新尺寸渲染拿到实际像素包围盒，再钳制位置（否则包围盒
                    // 还是旧尺寸，钳制不准确）
                    ExecTeeDrawer.render(CurrentEye, LastDirX, LastDirY, WalkPhase, LastEyeScale);
                    RenderedEye = CurrentEye;
                    if (oldCs > 0)
                        m_localTeePos = clampTeePos(QPointF(g) - anchor * (double(newCs) / oldCs),
                                                    ExecTeeDrawer.opaqueRect());
                    RenderedEye = -1;
                    updateEyeFollow();
                    if (SizeGroup) {
                        for (QAction *a : SizeGroup->actions())
                            a->setChecked(qFuzzyCompare(a->data().toDouble(), SizeScale));
                    }
                    Setup["Size"] = SizeScale;
                    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
                }
            } else {
                auto &pr = m_peersRender[roleId];
                if (pr.drawer) {
                    const int oldCs = pr.drawer->canvasSize();
                    const QPointF anchor = QPointF(g) - pr.pos;
                    pr.scale = qBound(0.5f, pr.scale * float(f), 2.0f);
                    pr.drawer->setRenderScale(pr.scale);
                    const int newCs = pr.drawer->canvasSize();
                    // 先重渲染 body 拿新尺寸的实际像素包围盒，再钳制位置
                    pr.drawer->renderBody(pr.body);
                    if (oldCs > 0)
                        pr.pos = clampTeePos(QPointF(g) - anchor * (double(newCs) / oldCs),
                                             pr.drawer->opaqueRect());
                    pr.drawer->renderEyes(pr.eyes, pr.eye, pr.dir.x(), pr.dir.y(),
                                          pr.eyeScale);
                    pr.lastEye = pr.eye;
                    pr.lastEyeScale = pr.eyeScale;
                    pr.lastDir = pr.dir;
                }
                update();
            }
        }
        event->accept();
        return;
    }

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
        TrayIcon.setIcon(makeTrayIcon());
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
        TrayIcon.setIcon(makeTrayIcon());
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
