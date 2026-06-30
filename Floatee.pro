QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/core/teedrawer.cpp \
    src/core/jsonopt.cpp \
    src/ui/floatee.cpp \
    src/ui/teeyes.cpp \
    src/ui/windowsidehide.cpp \
    src/platform/platformwindowinfo_win.cpp

HEADERS += \
    src/core/teedrawer.h \
    src/core/jsonopt.h \
    src/ui/floatee.h \
    src/ui/teeyes.h \
    src/ui/windowsidehide.h \
    src/platform/platformwindowinfo.h

FORMS += \
    src/ui/floatee.ui \
    src/ui/teeyes.ui

INCLUDEPATH += src

RESOURCES += \
    Floatee.qrc

DISTFILES += \
    assets/bg/green.png \
    assets/bg/neko.jpg \
    assets/bg/purple.jpg \
    assets/bg/sky.jpg \
    assets/main/Icon.png \
    assets/main/_ghostjtj.png \
    assets/main/emoticons.png \
    assets/main/eyes.png \
    assets/main/eyes_clever.png \
    assets/main/eyes_close.png \
    assets/main/eyes_happy.png \
    assets/main/ghostjtj.png \
    assets/skin/Tata.png \
    assets/skin/Tataa.png \
    assets/skin/chinese_by_whis.png \
    assets/skin/coala_pinky.png \
    assets/skin/mouse.png \
    assets/skin/santa_bluekitty.png
