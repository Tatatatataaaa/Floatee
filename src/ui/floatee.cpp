#include "floatee.h"
#include "ui_floatee.h"
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <cmath>

bool EyesSwitch = true;

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
    setWindowIcon(QIcon(ExecTeeDrawer.Tee));
    resize(96, 96);

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
    TeeEyes.move(22, 28);
    TeeEyes.setAttribute(Qt::WA_TransparentForMouseEvents);
    TeeEyes.setPixmap(ExecTeeDrawer.TeeEyes);
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
        EyesSwitch = !EyesSwitch;
        TeeEyes.setPixmap(EyesSwitch ? ExecTeeDrawer.TeeEyes
                                     : ExecTeeDrawer.TeeEyes_Clever);
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
    if (EyesSwitch)
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

    setGeometry(22 + dx, 28 + dy, 52, 32);
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
