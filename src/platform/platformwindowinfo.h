#ifndef PLATFORMWINDOWINFO_H
#define PLATFORMWINDOWINFO_H

#include <QString>

class PlatformWindowInfo {
public:
    static PlatformWindowInfo* create();
    virtual ~PlatformWindowInfo() = default;

    virtual QString foregroundWindowTitle() = 0;
    virtual QString foregroundWindowClass() = 0;
    virtual bool findAndEmbedWindow(const QString &title, const QString &className,
                                     void*& outHandle) = 0;
    virtual void setWindowTopmost(void* handle) = 0;

    // 点击穿透（鼠标事件是否落到后面窗口）。默认 no-op：Qt 的
    // WA_TransparentForMouseEvents 在非 macOS 平台由宿主直接设置；
    // macOS 上该 attribute 不参与 native 鼠标穿透（QNSView::
    // isTransparentForUserInput 只检查 Qt::WindowTransparentForInput flag，
    // 而 ignoresMouseEvents 运行时切回不可靠），因此由平台层直接操作
    // NSWindow.ignoresMouseEvents。
    virtual void setWindowClickThrough(void* handle, bool on)
    {
        Q_UNUSED(handle)
        Q_UNUSED(on)
    }
};

#endif // PLATFORMWINDOWINFO_H
