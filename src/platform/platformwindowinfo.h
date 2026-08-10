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
};

#endif // PLATFORMWINDOWINFO_H
