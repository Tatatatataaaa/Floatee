#include "floatee.h"
#include "ui_floatee.h"
#include <QIcon>
#include <QActionGroup>
#include <QStandardPaths>
#include <QDir>
#include <cmath>

static int CurrentEye = 0;  // 0=Normal, 1=Happy, 2=Angry, 3=Clever, 4=Close

static QPixmap eyePixmap(const TeeDrawer &d, int idx) {
    switch (idx) {
    case 1: return d.TeeEyes_Happy;
    case 2: return d.TeeEyes_Angry;
    case 3: return d.TeeEyes_Clever;
    case 4: return d.TeeEyes_Close;
    default: return d.TeeEyes;
    }
}

void Floatee::Loading()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Floatee";
    QDir().mkpath(dataDir);
    Path_Setup = dataDir + "/setup.json";
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

    // Load saved skin preference
    QString savedSkin = Setup.value("Skin").toString();
    if (!savedSkin.isEmpty() && savedSkin.startsWith(":/"))
        ExecTeeDrawer.load(savedSkin);

    setWindowIcon(QIcon(ExecTeeDrawer.Tee));

    BodyLabel = new QLabel(this);
    BodyLabel->setGeometry(0, 0, 96, 96);
    BodyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    BodyLabel->setPixmap(ExecTeeDrawer.TeeBare);

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
    CurrentEye = qBound(0, Setup.value("Eye").toInt(0), 4);

    QVector<QPair<QString, int>> eyeTypes = {
        {"Normal", 0}, {"Happy", 1}, {"Angry", 2}, {"Clever", 3}, {"Close", 4},
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
    if (!CurrentSkin.startsWith(":/"))
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

    TeeEyes.setParent(this);
    TeeEyes.resize(52, 32);
    TeeEyes.move(24, 28);
    TeeEyes.setAttribute(Qt::WA_TransparentForMouseEvents);
    TeeEyes.setPixmap(eyePixmap(ExecTeeDrawer, CurrentEye));
    TeeEyes.raise();
    TeeEyes.show();

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

QRect Floatee::GetTeePos()
{
    return geometry();
}

void Floatee::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        MousePress = true;
        MousePoint = event->globalPosition().toPoint() - this->pos();
    }
    else if (event->button() == Qt::RightButton) {
        CurrentEye = (CurrentEye + 1) % 5;
        TeeEyes.setPixmap(eyePixmap(ExecTeeDrawer, CurrentEye));
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

void Floatee::Eyes::MouseMoveEvent(QMouseEvent *e)
{
    QPointF pG = e->globalPosition();
    Floatee *parentTee = static_cast<Floatee*>(parent());
    QRect TeePos = parentTee->GetTeePos();
    int dx = pG.x() - TeePos.x() - 57;
    int dy = pG.y() - TeePos.y() - 44;
    if (CurrentEye == 0)
    {
        if (std::abs(dx) <= 30 && dy >= -30 && dy <= 0)
            setPixmap(parentTee->ExecTeeDrawer.TeeEyes_Happy);
        else
            setPixmap(parentTee->ExecTeeDrawer.TeeEyes);
    }
    int t = 0;
    if (dx > 0)
        for (; dx > 0; t++)
            dx -= t * 12;
    else
        for (; dx < 0; t--)
            dx -= t * 12;
    dx = t;
    t = 0;
    if (dy > 0)
        for (; dy > 0; t++)
            dy -= t * 8;
    else
        for (; dy < 0; t--)
            dy -= t * 8;
    dy = t;

    dx = std::min(dx, 15);
    dx = std::max(dx, -15);
    dy = std::min(dy, 15);
    dy = std::max(dy, -15);
    double l = std::sqrt(dx * dx + dy * dy);
    if (l > 15)
    {
        dx = dx * 15 / l;
        dy = dy * 15 / l;
    }
    dy = dy * 2 / 3;
    if (dx >= 15)
        dx--;
    if (dx <= -15)
        dx++;
    if (dy == 10)
        dy--;
    if (dy == -10)
        dy++;

    setGeometry(24 + dx, 28 + dy, 52, 32);
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
    if (path == CurrentSkin)
        return;

    ExecTeeDrawer.load(path);
    CurrentSkin = path;

    const bool wasVisible = isVisible();
    if (wasVisible)
        hide();

    BodyLabel->setPixmap(ExecTeeDrawer.TeeBare);
    TeeEyes.setPixmap(eyePixmap(ExecTeeDrawer, CurrentEye));
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
    TeeEyes.setPixmap(eyePixmap(ExecTeeDrawer, CurrentEye));

    Setup["Eye"] = idx;
    JsonOpt::Json2File(Path_Setup, QJsonDocument(Setup));
}
