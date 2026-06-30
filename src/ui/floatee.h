#ifndef FLOATEE_H
#define FLOATEE_H

#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
#include <QCursor>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include <QPropertyAnimation>
#include <QScreen>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QProcess>

#include "ui/windowsidehide.h"
#include "ui/teeyes.h"
#include "core/jsonopt.h"
#include "core/teedrawer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Floatee;
}
QT_END_NAMESPACE

class Floatee : public QMainWindow
{
    Q_OBJECT

public:
    Floatee(QWidget *parent = nullptr);
    ~Floatee();
    QRect GetTeePos();
    void Loading();
    void Initialize();
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    class Eyes : public QLabel
    {
        public:
        void MouseMoveEvent(QMouseEvent *e);
        Eyes(QWidget *parent = nullptr) : QLabel(parent)
        {
            setMouseTracking(true);
            QTimer *cursorTimer = new QTimer(this);
            connect(cursorTimer, &QTimer::timeout, this, [this]() {
                QPoint globalPos = QCursor::pos();
                QMouseEvent fakeEvent(QEvent::MouseMove,
                    mapFromGlobal(globalPos),
                    globalPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                MouseMoveEvent(&fakeEvent);
            });
            cursorTimer->start(16);
        }
        ~Eyes() {}
    } TeeEyes;

    QSystemTrayIcon TrayIcon;
    QMenu *TrayMenu = nullptr;
    QAction *AlwaysOnTopAction = nullptr;
    QLabel *BodyLabel = nullptr;
    QRect EyesPos;
    QPoint MousePoint;
    bool MousePress;

    WindowSideHide ExecWindowSideHide;
    TeEyes ExecTeEyes;
    TeeDrawer ExecTeeDrawer;

    QJsonObject Setup;
    QString Path_Setup;

protected slots:
    void on_systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAlwaysOnTop();

private:
    Ui::Floatee *ui;
};

#endif // FLOATEE_H
