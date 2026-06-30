#include "windowsidehide.h"
#include "ui/floatee.h"
#include <QMouseEvent>
#include <QWindow>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QTimer>
#include <QCursor>
#include <QCoreApplication>
#include <QVariantAnimation>

// ─── Windows implementation ───────────────────────────────────────────
#if defined(Q_OS_WIN)
#include <windows.h>

static bool s_leftPressed = false;

static int mouseButtonsFromWin32()
{
    int btns = 0;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) btns |= Qt::LeftButton;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) btns |= Qt::RightButton;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) btns |= Qt::MiddleButton;
    return btns;
}

static void pollGlobalMouse(WindowSideHide *self)
{
    QPoint pos = QCursor::pos();
    int buttons = mouseButtonsFromWin32();
    bool leftNow = (buttons & Qt::LeftButton) != 0;

    QCoreApplication::postEvent(self,
        new QMouseEvent(QEvent::MouseMove, QPointF(pos), QPointF(pos),
                        Qt::NoButton, Qt::MouseButtons(buttons), Qt::NoModifier));

    if (leftNow && !s_leftPressed) {
        QCoreApplication::postEvent(self,
            new QMouseEvent(QEvent::MouseButtonPress, QPointF(pos), QPointF(pos),
                            Qt::LeftButton, Qt::MouseButtons(buttons | Qt::LeftButton),
                            Qt::NoModifier));
    } else if (!leftNow && s_leftPressed) {
        QCoreApplication::postEvent(self,
            new QMouseEvent(QEvent::MouseButtonRelease, QPointF(pos), QPointF(pos),
                            Qt::LeftButton, Qt::MouseButtons(buttons | Qt::LeftButton),
                            Qt::NoModifier));
    }
    s_leftPressed = leftNow;
}

static QPropertyAnimation* startAnim(QWindow *w, int from, int to, int duration,
                                       QObject *parent)
{
    auto *old = w->findChild<QPropertyAnimation*>(QString(), Qt::FindDirectChildrenOnly);
    while (old) {
        old->stop();
        old->deleteLater();
        old = w->findChild<QPropertyAnimation*>(QString(), Qt::FindDirectChildrenOnly);
    }
    auto *ani = new QPropertyAnimation(w, "x", parent);
    ani->setDuration(duration);
    ani->setStartValue(from);
    ani->setEndValue(to);
    ani->setEasingCurve(QEasingCurve::Linear);
    QObject::connect(ani, &QPropertyAnimation::finished, ani, &QObject::deleteLater);
    ani->start();
    return ani;
}

static HWND getForegroundHWND() { return GetForegroundWindow(); }

// ─── macOS implementation ──────────────────────────────────────────────
#elif defined(Q_OS_MACOS)
#import <AppKit/AppKit.h>

static bool s_leftPressed_mac = false;

static int mouseButtonsFromMac()
{
    int btns = 0;
    if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft))
        btns |= Qt::LeftButton;
    if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight))
        btns |= Qt::RightButton;
    return btns;
}

static void pollGlobalMouseMac(WindowSideHide *self)
{
    @autoreleasepool {
        NSPoint nsPos = [NSEvent mouseLocation];
        QPoint pos(nsPos.x, nsPos.y);
        int buttons = mouseButtonsFromMac();
        bool leftNow = (buttons & Qt::LeftButton) != 0;

        QCoreApplication::postEvent(self,
            new QMouseEvent(QEvent::MouseMove, QPointF(pos), QPointF(pos),
                            Qt::NoButton, Qt::MouseButtons(buttons), Qt::NoModifier));

        if (leftNow && !s_leftPressed_mac) {
            QCoreApplication::postEvent(self,
                new QMouseEvent(QEvent::MouseButtonPress, QPointF(pos), QPointF(pos),
                                Qt::LeftButton, Qt::MouseButtons(buttons | Qt::LeftButton),
                                Qt::NoModifier));
        } else if (!leftNow && s_leftPressed_mac) {
            QCoreApplication::postEvent(self,
                new QMouseEvent(QEvent::MouseButtonRelease, QPointF(pos), QPointF(pos),
                                Qt::LeftButton, Qt::MouseButtons(buttons | Qt::LeftButton),
                                Qt::NoModifier));
        }
        s_leftPressed_mac = leftNow;
    }
}

static AXUIElementRef getFrontmostWindowAX(int *outPid = nullptr,
                                             CGRect *outBounds = nullptr)
{
    NSRunningApplication *frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (!frontApp) return nullptr;
    pid_t pid = frontApp.processIdentifier;
    if (outPid) *outPid = pid;

    AXUIElementRef appEl = AXUIElementCreateApplication(pid);
    if (!appEl) return nullptr;

    AXUIElementRef window = nullptr;
    AXError err = AXUIElementCopyAttributeValue(appEl, kAXFocusedWindowAttribute,
                                                 (CFTypeRef*)&window);
    CFRelease(appEl);
    if (err != kAXErrorSuccess || !window) return nullptr;

    if (outBounds) {
        CFTypeRef posVal = nullptr, sizeVal = nullptr;
        CGPoint pos = {0, 0};
        CGSize size = {0, 0};
        if (AXUIElementCopyAttributeValue(window, kAXPositionAttribute, &posVal) == kAXErrorSuccess) {
            AXValueGetValue((AXValueRef)posVal, kAXValueTypeCGPoint, &pos);
            CFRelease(posVal);
        }
        if (AXUIElementCopyAttributeValue(window, kAXSizeAttribute, &sizeVal) == kAXErrorSuccess) {
            AXValueGetValue((AXValueRef)sizeVal, kAXValueTypeCGSize, &size);
            CFRelease(sizeVal);
        }
        *outBounds = CGRectMake(pos.x, pos.y, size.width, size.height);
    }
    return window;
}

static void startAnimMac(AXUIElementRef axWindow, int fromX, int toX, int duration,
                          QObject *parent, int windowWidth)
{
    auto *ani = new QVariantAnimation(parent);
    ani->setDuration(duration);
    ani->setStartValue(fromX);
    ani->setEndValue(toX);
    ani->setEasingCurve(QEasingCurve::Linear);

    QObject::connect(ani, &QVariantAnimation::valueChanged, parent,
        [axWindow](const QVariant &val) {
            CFTypeRef posVal = nullptr;
            if (AXUIElementCopyAttributeValue(axWindow, kAXPositionAttribute, &posVal)
                == kAXErrorSuccess) {
                CGPoint pos;
                if (AXValueGetValue((AXValueRef)posVal, kAXValueTypeCGPoint, &pos)) {
                    pos.x = val.toDouble();
                    AXValueRef newPos = AXValueCreate(kAXValueTypeCGPoint, &pos);
                    AXUIElementSetAttributeValue(axWindow, kAXPositionAttribute, newPos);
                    CFRelease(newPos);
                }
                CFRelease(posVal);
            }
        });

    QObject::connect(ani, &QVariantAnimation::finished, parent, [ani]() {
        ani->deleteLater();
    });
    ani->start();
}
#endif

// ─── Common constructor / destructor ───────────────────────────────────
WindowSideHide::WindowSideHide(QWidget *parent)
    : QMainWindow{parent}
{
    QScreen *screen = QGuiApplication::primaryScreen();
    Desktop = screen->geometry();
    setMouseTracking(true);

#if defined(Q_OS_WIN)
    QTimer *pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { pollGlobalMouse(this); });
    pollTimer->start(33);
#elif defined(Q_OS_MACOS)
    QTimer *pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { pollGlobalMouseMac(this); });
    pollTimer->start(33);
#endif
}

WindowSideHide::~WindowSideHide() {}

// ─── Platform implementations ──────────────────────────────────────────
#if defined(Q_OS_WIN)

void WindowSideHide::mouseReleaseEvent(QMouseEvent *event)
{
    if (!Enabled) return;
    if (event->buttons() & Qt::LeftButton)  MouseButtonKeep[0] = false;
    if (event->buttons() & Qt::RightButton) MouseButtonKeep[1] = false;
    if (event->buttons() & Qt::MiddleButton) MouseButtonKeep[2] = false;
    if (event->button() == Qt::LeftButton) {
        HWND h = GetForegroundWindow();
        QWindow *w = QWindow::fromWinId(reinterpret_cast<WId>(h));
        QRect r = w->geometry();
        if (r.x() < 0 && -r.x() > (r.x() + r.width())) {
            startAnim(w, r.x(), -r.width() - 1, AnimaDuration, this);
            HidedWindowsLeft.emplace_back(w, h, w->y(), w->y() + w->height());
        }
        if (r.right() > Desktop.width()
            && r.right() - Desktop.width() > (-r.x() + Desktop.width())) {
            startAnim(w, r.x(), Desktop.width() + 1, AnimaDuration, this);
            HidedWindowsRight.emplace_back(w, h, w->y(), w->y() + w->height());
        }
        if (r.x() > 0) {
            for (auto i = HidedWindowsLeft.begin(); i != HidedWindowsLeft.end(); )
                i = (i->hwnd == h) ? HidedWindowsLeft.erase(i) : ++i;
        }
        if (r.right() < Desktop.width() - 1) {
            for (auto i = HidedWindowsRight.begin(); i != HidedWindowsRight.end(); )
                i = (i->hwnd == h) ? HidedWindowsRight.erase(i) : ++i;
        }
    }
}

void WindowSideHide::mouseMoveEvent(QMouseEvent *event)
{
    if (!Enabled) return;
    if (MouseButtonKeep[0] | MouseButtonKeep[1] | MouseButtonKeep[2]) return;

    if (event->position().x() <= 5) {
        for (auto &i : HidedWindowsLeft) {
            if (i.t <= event->position().y() && i.b >= event->position().y()
                && i.w->x() < 0)
                startAnim(i.w, i.w->x(), 0, AnimaDuration, this);
        }
    }
    if (event->position().x() >= Desktop.right() - 5) {
        for (auto &i : HidedWindowsRight) {
            if (i.t <= event->position().y() && i.b >= event->position().y()
                && i.w->x() > Desktop.width() - i.w->width())
                startAnim(i.w, i.w->x(), Desktop.width() - i.w->width(),
                          AnimaDuration, this);
        }
    }
    if (event->position().x() > 5) {
        for (auto &i : HidedWindowsLeft) {
            int cx = i.w->x();
            if ((event->position().x() < cx || event->position().x() > cx + i.w->width())
                && cx > -i.w->width())
                startAnim(i.w, cx, -i.w->width() - 1, AnimaDuration, this);
        }
    }
    if (event->position().x() < Desktop.right() - 5) {
        for (auto &i : HidedWindowsRight) {
            int cx = i.w->x();
            if (cx < Desktop.right()
                && (event->position().x() < cx || event->position().x() > cx + i.w->width())
                && cx < Desktop.width())
                startAnim(i.w, cx, Desktop.width() + 1, AnimaDuration, this);
        }
    }
}

void WindowSideHide::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)   MouseButtonKeep[0] = true;
    if (event->buttons() & Qt::RightButton)  MouseButtonKeep[1] = true;
    if (event->buttons() & Qt::MiddleButton) MouseButtonKeep[2] = true;
    mouseMoveEvent(event);
}

QRect WindowSideHide::GetGeometryFromHWND(void *hwnd)
{
    return QWindow::fromWinId(reinterpret_cast<WId>(hwnd))->geometry();
}

// ─── macOS implementation ──────────────────────────────────────────────
#elif defined(Q_OS_MACOS)

void WindowSideHide::mouseReleaseEvent(QMouseEvent *event)
{
    if (!Enabled) return;
    if (event->buttons() & Qt::LeftButton)  MouseButtonKeep[0] = false;
    if (event->buttons() & Qt::RightButton) MouseButtonKeep[1] = false;
    if (event->buttons() & Qt::MiddleButton) MouseButtonKeep[2] = false;
    if (event->button() != Qt::LeftButton) return;

    // Check accessibility permission
    if (!AXIsProcessTrusted()) return;

    int pid = 0;
    CGRect bounds = {0, 0, 0, 0};
    AXUIElementRef axWindow = getFrontmostWindowAX(&pid, &bounds);
    if (!axWindow) return;

    // The screen coordinate system on macOS has origin at bottom-left.
    // Convert to our top-left coordinate system for consistency.
    CGFloat screenH = Desktop.height();
    CGFloat winX = bounds.origin.x;
    CGFloat winY = screenH - bounds.origin.y - bounds.size.height;
    int winW = (int)bounds.size.width;
    int winH = (int)bounds.size.height;

    QWindow *w = nullptr; // QWindow::fromWinId unreliable for foreign macOS windows

    if (winX < 0 && -winX > (winX + winW)) {
        HidedWindowsLeft.emplace_back(w, axWindow, (int)winY, (int)(winY + winH));
        startAnimMac(axWindow, (int)winX, -winW - 1, AnimaDuration, this, winW);
    } else if (winX + winW > Desktop.width()
               && (winX + winW - Desktop.width()) > (-winX + Desktop.width())) {
        HidedWindowsRight.emplace_back(w, axWindow, (int)winY, (int)(winY + winH));
        startAnimMac(axWindow, (int)winX, Desktop.width() + 1, AnimaDuration, this, winW);
    } else {
        // Window is fully on screen — remove from hidden lists
        for (auto i = HidedWindowsLeft.begin(); i != HidedWindowsLeft.end(); ) {
            i = (CFEqual(i->axWindow, axWindow)) ? HidedWindowsLeft.erase(i) : ++i;
        }
        for (auto i = HidedWindowsRight.begin(); i != HidedWindowsRight.end(); ) {
            i = (CFEqual(i->axWindow, axWindow)) ? HidedWindowsRight.erase(i) : ++i;
        }
        CFRelease(axWindow);
        return;
    }
    // axWindow is now owned by HidedWindow (CFRetained in constructor)
    CFRelease(axWindow);
}

void WindowSideHide::mouseMoveEvent(QMouseEvent *event)
{
    if (!Enabled) return;
    if (MouseButtonKeep[0] | MouseButtonKeep[1] | MouseButtonKeep[2]) return;
    if (!AXIsProcessTrusted()) return;

    CGFloat screenH = Desktop.height();

    auto getMacWindowX = [screenH](const HidedWindow &i) -> int {
        if (!i.axWindow) return 0;
        CFTypeRef posVal = nullptr;
        int x = 0;
        if (AXUIElementCopyAttributeValue(i.axWindow, kAXPositionAttribute, &posVal)
            == kAXErrorSuccess) {
            CGPoint pos;
            if (AXValueGetValue((AXValueRef)posVal, kAXValueTypeCGPoint, &pos))
                x = (int)pos.x;
            CFRelease(posVal);
        }
        return x;
    };

    auto getMacWindowWidth = [](const HidedWindow &i) -> int {
        if (!i.axWindow) return 0;
        CFTypeRef sizeVal = nullptr;
        int w = 0;
        if (AXUIElementCopyAttributeValue(i.axWindow, kAXSizeAttribute, &sizeVal)
            == kAXErrorSuccess) {
            CGSize sz;
            if (AXValueGetValue((AXValueRef)sizeVal, kAXValueTypeCGSize, &sz))
                w = (int)sz.width;
            CFRelease(sizeVal);
        }
        return w;
    };

    if (event->position().x() <= 5) {
        for (auto &i : HidedWindowsLeft) {
            if (i.t <= event->position().y() && i.b >= event->position().y()) {
                int cx = getMacWindowX(i);
                int cw = getMacWindowWidth(i);
                if (cx < 0)
                    startAnimMac(i.axWindow, cx, 0, AnimaDuration, this, cw);
            }
        }
    }
    if (event->position().x() >= Desktop.right() - 5) {
        for (auto &i : HidedWindowsRight) {
            if (i.t <= event->position().y() && i.b >= event->position().y()) {
                int cx = getMacWindowX(i);
                int cw = getMacWindowWidth(i);
                if (cx > Desktop.width() - cw)
                    startAnimMac(i.axWindow, cx, Desktop.width() - cw,
                                 AnimaDuration, this, cw);
            }
        }
    }
    if (event->position().x() > 5) {
        for (auto &i : HidedWindowsLeft) {
            int cx = getMacWindowX(i);
            int cw = getMacWindowWidth(i);
            if ((event->position().x() < cx || event->position().x() > cx + cw)
                && cx > -cw)
                startAnimMac(i.axWindow, cx, -cw - 1, AnimaDuration, this, cw);
        }
    }
    if (event->position().x() < Desktop.right() - 5) {
        for (auto &i : HidedWindowsRight) {
            int cx = getMacWindowX(i);
            int cw = getMacWindowWidth(i);
            if (cx < Desktop.right()
                && (event->position().x() < cx || event->position().x() > cx + cw)
                && cx < Desktop.width())
                startAnimMac(i.axWindow, cx, Desktop.width() + 1,
                             AnimaDuration, this, cw);
        }
    }
}

void WindowSideHide::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)   MouseButtonKeep[0] = true;
    if (event->buttons() & Qt::RightButton)  MouseButtonKeep[1] = true;
    if (event->buttons() & Qt::MiddleButton) MouseButtonKeep[2] = true;
    mouseMoveEvent(event);
}

QRect WindowSideHide::GetGeometryFromHWND(void *) { return QRect(); }

// ─── Stubs for unsupported platforms ──────────────────────────────────
#else

void WindowSideHide::mouseReleaseEvent(QMouseEvent *) {}
void WindowSideHide::mouseMoveEvent(QMouseEvent *) {}
void WindowSideHide::mousePressEvent(QMouseEvent *) {}
QRect WindowSideHide::GetGeometryFromHWND(void *) { return QRect(); }

#endif
