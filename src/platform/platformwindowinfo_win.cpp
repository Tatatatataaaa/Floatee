#include "platformwindowinfo.h"

#include <windows.h>
#include <QDebug>
#include <string>

class PlatformWindowInfoWin : public PlatformWindowInfo {
public:
    QString foregroundWindowTitle() override
    {
        HWND h = GetForegroundWindow();
        if (!h) return {};
        int len = GetWindowTextLengthW(h);
        if (len <= 0) return {};
        QVector<WCHAR> buf(len + 1);
        GetWindowTextW(h, buf.data(), len + 1);
        return QString::fromWCharArray(buf.data());
    }

    QString foregroundWindowClass() override
    {
        HWND h = GetForegroundWindow();
        if (!h) return {};
        WCHAR buf[256];
        int len = GetClassNameW(h, buf, 256);
        if (len <= 0) return {};
        return QString::fromWCharArray(buf);
    }

    bool findAndEmbedWindow(const QString &title, const QString &className,
                             void*& outHandle) override
    {
        std::wstring classW = className.toStdWString();
        std::wstring titleW = title.toStdWString();

        HWND h = FindWindowW(
            classW.empty() ? nullptr : reinterpret_cast<LPCWSTR>(classW.c_str()),
            titleW.empty() ? nullptr : reinterpret_cast<LPCWSTR>(titleW.c_str()));
        if (!h) {
            outHandle = nullptr;
            return false;
        }
        SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        outHandle = reinterpret_cast<void*>(h);
        return true;
    }

    void setWindowTopmost(void* handle) override
    {
        if (!handle) return;
        SetWindowPos(reinterpret_cast<HWND>(handle), HWND_TOPMOST,
                      0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
};

PlatformWindowInfo* PlatformWindowInfo::create()
{
    return new PlatformWindowInfoWin();
}
