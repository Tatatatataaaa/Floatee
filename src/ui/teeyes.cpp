#include "teeyes.h"
#include "ui_teeyes.h"
#include "ui/floatee.h"
#include <QFile>
#include <QJsonArray>
#include <QKeyEvent>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void TeEyes::Loading()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir);
    Path_Data = appDataDir + "/teeyes.json";
    Data = JsonOpt::File2Json(Path_Data).object();
    if (Data.empty())
    {
        qDebug() << "DataFileLoss";
        Data = QJsonObject();
        Data.insert("DataExisted", true);
        Data.insert("Interval", 600);
        Data.insert("Duration", 10);
        Data.insert("Reminder", true);
        Data.insert("Background", ":/ProtectImg/assets/bg/neko.jpg");
        QJsonArray Pl;
        Data.insert("List", Pl);
        JsonOpt::Json2File(Path_Data, QJsonDocument(Data));
    }
    Interval = Data["Interval"].toInt();
    Duration = Data["Duration"].toInt();
    Reminder = Data["Reminder"].toBool();
    Background = Data["Background"].toString();
    if ((Data["List"].toArray()).contains("Floatee"))
        qDebug() << "Pass!";
}

void TeEyes::Initiation()
{
    scr = QGuiApplication::primaryScreen();
    this->setGeometry(scr->geometry());
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);

    QFrame *frame = new QFrame(this);
    frame->setObjectName("myframe");
    frame->setGeometry(this->geometry());
    frame->resize(width(), height());
    frame->setStyleSheet("QFrame#myframe{border-image:url(" + Background + ")}");

    State = 0;
    Current = 0;
    Total = 0;

    m_platformInfo = PlatformWindowInfo::create();

    Id = startTimer(Interval * 1000);
}

TeEyes::TeEyes(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TeEyes)
    , Todo(nullptr)
    , m_platformInfo(nullptr)
{
    ui->setupUi(this);
    Loading();
    Initiation();
}

TeEyes::~TeEyes()
{
    delete ui;
    delete m_platformInfo;
    JsonOpt::Json2File(Path_Data, QJsonDocument(Data));
}

int TeEyes::PassCheck()
{
    qDebug() << "PassCheck";
    QString title = m_platformInfo->foregroundWindowTitle();
    QString className = m_platformInfo->foregroundWindowClass();

    if (Data["List"].toArray().contains(title))
        return 1;
    if (Data["List"].toArray().contains(className))
        return 1;
    return 0;
}

void TeEyes::timerEvent(QTimerEvent *event)
{
    if (!Enabled)
        return;
    if (event->timerId() == Id)
    {
        if (State)
        {
            Stop();
            return;
        }

        int c = PassCheck();
        if (!c)
        {
            void *extHandle = nullptr;
            State = 1;
            bool embedded = false;

            // Stop interval timer and start duration timer
            killTimer(Id);
            Id = startTimer(Duration * 1000);
            qDebug() << "Duration" << Duration;

            if (Reminder)
            {
                embedded = m_platformInfo->findAndEmbedWindow(
                    "Microsoft To Do", "ApplicationFrameWindow", extHandle);
#ifdef Q_OS_WIN
                if (!embedded)
                {
                    QString todoPath = "explorer.exe shell:AppsFolder\\Microsoft.Todos_8wekyb3d8bbwe!App";
                    QProcess TodoProcess;
                    TodoProcess.start(todoPath);
                    TodoProcess.waitForStarted(2000);
                    Sleep(100);
                    embedded = m_platformInfo->findAndEmbedWindow(
                        "Microsoft To Do", "ApplicationFrameWindow", extHandle);
                }
#endif
            }

            if (embedded && extHandle)
            {
#ifdef Q_OS_WIN
                Todo = QWindow::fromWinId(reinterpret_cast<WId>(extHandle));
                QWidget *w = QWidget::createWindowContainer(Todo);
                w->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
                Todo->setFlags(Qt::WindowStaysOnTopHint | Qt::Tool);
                w->setGeometry(-scr->geometry().width() - 1, 0,
                               scr->geometry().width(), scr->geometry().height());
#endif
            }
            else
            {
                qDebug() << "Protect";
                setWindowState(Qt::WindowFullScreen);
                setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
                this->show();
            }
        }
    }
}

void TeEyes::Stop()
{
    State = 0;
    killTimer(Id);
    Id = startTimer(Interval * 1000);
    qDebug() << "Interval" << Interval;

    void *extHandle = nullptr;

    if (Reminder)
    {
        m_platformInfo->findAndEmbedWindow(
            "Microsoft To Do", "ApplicationFrameWindow", extHandle);
    }

    if (extHandle)
    {
#ifdef Q_OS_WIN
        Todo = QWindow::fromWinId(reinterpret_cast<WId>(extHandle));
        QPropertyAnimation *Ani = new QPropertyAnimation(Todo, "x");
        QRect r = Todo->geometry();
        Ani->setDuration(((Floatee*)Tee)->ExecWindowSideHide.AnimaDuration);
        Ani->setStartValue(r.x());
        Ani->setEndValue(-r.width() - 1);
        Ani->start();

        if (((Floatee*)Tee)->Setup["Always_on_the_Top"].toBool())
        {
            Tee->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        }
#endif
    }
    else
    {
        if (((Floatee*)Tee)->Setup["Always_on_the_Top"].toBool())
        {
            Tee->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        }
        this->hide();
    }
}

void TeEyes::keyPressEvent(QKeyEvent *event)
{
    if (!Enabled)
        return;
    if (!State)
        return;
    if (event->key() == Qt::Key_Space)
    {
        Stop();
        QString title = m_platformInfo->foregroundWindowTitle();
        QString className = m_platformInfo->foregroundWindowClass();
        QJsonArray Pl = Data["List"].toArray();
        Pl.append(title);
        Pl.append(className);
        qDebug() << Pl;
        Data["List"] = Pl;
    }
    if (event->key() == Qt::Key_Escape)
    {
        Stop();
    }
}
