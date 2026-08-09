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

    bool load(const QString &skinPath, int hueShift = 0, double satFactor = 1.0, double lightFactor = 1.0);
    static QString defaultSkinPath() { return QStringLiteral(":/skins/Tata.png"); }
    static QPixmap adjustHsl(const QPixmap &src, int hueShift, double satFactor, double lightFactor);

    QPixmap SkinFile;
    QPixmap Tee;       // full tee with eyes (used as window / tray icon)
    QPixmap TeeBare;   // tee without eyes (used as draggable body background)
    QPixmap TeeBody;
    QPixmap TeeEyes;
    QPixmap TeeEyes_Happy;
    QPixmap TeeEyes_Angry;
    QPixmap TeeEyes_Surprise; // mapped from UI "Dazed" (tee_render EMOTE_SURPRISE)
    QPixmap TeeEyes_Pain;     // mapped from UI "Clever" (tee_render EMOTE_PAIN)
    QPixmap TeeEye;
    QPixmap TeeFoot;
};

#endif // TEEDRAWER_H
