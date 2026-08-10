#include "platformwindowinfo.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <QDebug>
#include <QGuiApplication>

class PlatformWindowInfoX11 : public PlatformWindowInfo {
public:
    PlatformWindowInfoX11()
    {
        m_display = nullptr;
        if (QGuiApplication::platformName() == QLatin1String("xcb")) {
            m_display = XOpenDisplay(nullptr);
        }
    }

    ~PlatformWindowInfoX11() override
    {
        if (m_display)
            XCloseDisplay(m_display);
    }

    QString foregroundWindowTitle() override
    {
        if (!m_display) return {};
        Window w = getActiveWindow();
        if (!w) return {};
        return getWindowProperty(w, "_NET_WM_NAME");
    }

    QString foregroundWindowClass() override
    {
        if (!m_display) return {};
        Window w = getActiveWindow();
        if (!w) return {};
        return getWindowProperty(w, "WM_CLASS");
    }

    bool findAndEmbedWindow(const QString &title, const QString &className,
                             void*& outHandle) override
    {
        Q_UNUSED(title)
        Q_UNUSED(className)
        // Embedding foreign windows via XReparentWindow is risky and
        // not supported on Wayland. Fall back to fullscreen overlay.
        outHandle = nullptr;
        return false;
    }

    void setWindowTopmost(void* handle) override
    {
        Q_UNUSED(handle)
        // Not needed — the fallback fullscreen overlay handles this via Qt flags.
    }

private:
    Display* m_display;

    Window getActiveWindow()
    {
        Atom netActive = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", False);
        Atom type;
        int format;
        unsigned long nitems, bytesAfter;
        unsigned char *data = nullptr;
        Window root = DefaultRootWindow(m_display);
        int status = XGetWindowProperty(m_display, root, netActive,
                                         0, 1, False, XA_WINDOW,
                                         &type, &format, &nitems, &bytesAfter, &data);
        if (status != Success || !data) return 0;
        Window w = *reinterpret_cast<Window*>(data);
        XFree(data);
        return w;
    }

    QString getWindowProperty(Window w, const char* propName)
    {
        Atom prop = XInternAtom(m_display, propName, False);
        Atom type;
        int format;
        unsigned long nitems, bytesAfter;
        unsigned char *data = nullptr;
        int status = XGetWindowProperty(m_display, w, prop,
                                         0, 1024, False, AnyPropertyType,
                                         &type, &format, &nitems, &bytesAfter, &data);
        if (status != Success || !data) return {};
        QString result;
        if (type == XA_STRING || type == XInternAtom(m_display, "UTF8_STRING", False)) {
            result = QString::fromUtf8(reinterpret_cast<char*>(data));
        }
        XFree(data);
        return result;
    }
};

PlatformWindowInfo* PlatformWindowInfo::create()
{
    return new PlatformWindowInfoX11();
}
