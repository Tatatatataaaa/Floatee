#ifndef WINDOWSIDEHIDE_H
#define WINDOWSIDEHIDE_H

#include <QMainWindow>
#include <QObject>
#include <QWidget>
#include <list>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#endif

class WindowSideHide : public QMainWindow
{
    Q_OBJECT
public:
    class HidedWindow
    {
    public:
        QWindow *w;
#ifdef Q_OS_WIN
        HWND hwnd;
        HidedWindow(QWindow *_w, HWND h, int _t, int _b)
            : w(_w), hwnd(h), t(_t), b(_b) {}
#elif defined(Q_OS_MACOS)
        AXUIElementRef axWindow;
        HidedWindow(QWindow *_w, AXUIElementRef ax, int _t, int _b)
            : w(_w), axWindow(ax), t(_t), b(_b) { if (ax) CFRetain(ax); }
        HidedWindow(const HidedWindow &other)
            : w(other.w), axWindow(other.axWindow), t(other.t), b(other.b)
            { if (axWindow) CFRetain(axWindow); }
        ~HidedWindow() { if (axWindow) CFRelease(axWindow); }
#else
        void *hwnd;
        HidedWindow(QWindow *_w, void *h, int _t, int _b)
            : w(_w), hwnd(h), t(_t), b(_b) {}
#endif
        int t, b;
    };
    std::list<HidedWindow> HidedWindowsLeft, HidedWindowsRight;
    explicit WindowSideHide(QWidget *parent = nullptr);
    ~WindowSideHide();
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    bool Enabled = true;
    bool MouseButtonKeep[3] = {false, false, false};
    QRect GetGeometryFromHWND(void *hwnd);
    int AnimaDuration = 300;
    QRect Desktop;
    QWidget* Tee;
};

#endif // WINDOWSIDEHIDE_H
