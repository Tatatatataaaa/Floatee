#include "ui/floatee.h"

#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <timeapi.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Raise Windows timer resolution from default 15.6ms to 1ms so that
    // Qt animations and polling timers are not bottlenecked by the OS.
    timeBeginPeriod(1);
#endif

    QApplication a(argc, argv);
    a.setApplicationName("Floatee");
    Floatee w;
    w.show();
    int ret = a.exec();

#ifdef Q_OS_WIN
    timeEndPeriod(1);
#endif

    return ret;
}
