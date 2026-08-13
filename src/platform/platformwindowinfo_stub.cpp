#include "platformwindowinfo.h"

// No-op platform layer for platforms without window-manager integration
// (Android, or any platform not covered by the Win/macOS/X11 backends).
// All operations degrade gracefully: nothing can be embedded or brought to
// topmost, and the foreground-window queries return empty.
class PlatformWindowInfoStub : public PlatformWindowInfo
{
public:
    QString foregroundWindowTitle() override { return {}; }
    QString foregroundWindowClass() override { return {}; }
    bool findAndEmbedWindow(const QString &, const QString &, void *&) override
    {
        return false;
    }
    void setWindowTopmost(void *) override {}
};

PlatformWindowInfo *PlatformWindowInfo::create()
{
    return new PlatformWindowInfoStub();
}
