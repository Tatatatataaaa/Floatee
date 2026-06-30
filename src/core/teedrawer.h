#ifndef TEEDRAWER_H
#define TEEDRAWER_H

#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QImage>
#include <QString>

class TeeDrawer
{
public:
    explicit TeeDrawer(const QString &skinPath = defaultSkinPath());

    bool load(const QString &skinPath);
    static QString defaultSkinPath() { return QStringLiteral(":/skins/Tata.png"); }

    QPixmap changeHue(const QPixmap &pixmap, int hueShift);

    QPixmap SkinFile;
    QPixmap Tee;       // full tee with eyes (used as window / tray icon)
    QPixmap TeeBare;   // tee without eyes (used as draggable body background)
    QPixmap cTee;
    QPixmap TeeBody;
    QPixmap TeeEyes;
    QPixmap TeeEyes_Happy;
    QPixmap TeeEyes_Angry;
    QPixmap TeeEyes_Close;
    QPixmap TeeEyes_Clever;
    QPixmap TeeEye;
    QPixmap TeeFoot;
};

#endif // TEEDRAWER_H
