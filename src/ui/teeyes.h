#ifndef TEEYES_H
#define TEEYES_H

#include <QWidget>
#include <QTimer>
#include <QWindow>
#include <QScreen>
#include <QEvent>
#include <QKeyEvent>
#include "platform/platformwindowinfo.h"
#include "core/jsonopt.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class TeEyes;
}
QT_END_NAMESPACE

class TeEyes : public QWidget
{
    Q_OBJECT

public:
    TeEyes(QWidget *parent = nullptr);
    ~TeEyes();
    int Id;
    void Stop();
    void keyPressEvent(QKeyEvent *event);
    void Loading();
    void Initiation();
    int PassCheck();
    QJsonObject Data;
    int Interval;
    int Duration;
    int Total;
    int Current;
    int State;
    bool Reminder;
    QString Background;
    QWindow *Todo;
    QScreen *scr;
    QWidget* Tee;
    bool Enabled = true;
    QString Path_Data;

private:
    Ui::TeEyes *ui;
    PlatformWindowInfo *m_platformInfo;
    void timerEvent(QTimerEvent *event);
};

#endif // TEEYES_H
