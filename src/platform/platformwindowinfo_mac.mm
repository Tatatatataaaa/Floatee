#include "platformwindowinfo.h"

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

class PlatformWindowInfoMac : public PlatformWindowInfo {
public:
    QString foregroundWindowTitle() override
    {
        @autoreleasepool {
            NSRunningApplication *app = [NSWorkspace sharedWorkspace].frontmostApplication;
            if (!app) return {};
            NSString *name = app.localizedName;
            return name ? QString::fromNSString(name) : QString();
        }
    }

    QString foregroundWindowClass() override
    {
        @autoreleasepool {
            NSRunningApplication *app = [NSWorkspace sharedWorkspace].frontmostApplication;
            if (!app) return {};
            NSString *bundleId = app.bundleIdentifier;
            return bundleId ? QString::fromNSString(bundleId) : QString();
        }
    }

    bool findAndEmbedWindow(const QString &title, const QString &className,
                             void*& outHandle) override
    {
        Q_UNUSED(title)
        Q_UNUSED(className)
        // macOS does not support embedding external application windows.
        // The caller should fall back to fullscreen overlay mode.
        outHandle = nullptr;
        return false;
    }

    void setWindowTopmost(void* handle) override
    {
        Q_UNUSED(handle)
        // Not needed on macOS — the fallback fullscreen overlay handles this via Qt flags.
    }

    void setWindowClickThrough(void* handle, bool on) override
    {
        // winId() on macOS returns the QNSView*; its NSWindow drives hit-testing.
        // Setting ignoresMouseEvents=YES makes the whole window pass clicks
        // through to windows below; NO makes it capture them (tee interactable).
        NSView *view = (__bridge NSView *)handle;
        NSWindow *window = [view window];
        if (window)
            window.ignoresMouseEvents = on ? YES : NO;
    }
};

PlatformWindowInfo* PlatformWindowInfo::create()
{
    return new PlatformWindowInfoMac();
}
