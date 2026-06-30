#include "teedrawer.h"
#include <QDebug>
#include <QFile>

QPixmap TeeDrawer::changeHue(const QPixmap &pixmap, int hueShift)
{
    QImage image = pixmap.toImage();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QColor color = QColor(image.pixel(x, y));
            if (color.lightness() == 0)
                continue;
            if (y == 0 && x <= 5)
                continue;
            int h, s, l, a;
            color.getHsl(&h, &s, &l, &a);
            h = (h + hueShift) % 360;
            color.setHsl(h, s, l, a);
            image.setPixel(x, y, color.rgb());
        }
    }
    return QPixmap::fromImage(image);
}

TeeDrawer::TeeDrawer(const QString &skinPath)
{
    load(skinPath);
}

bool TeeDrawer::load(const QString &skinPath)
{
    QPixmap loaded;
    bool ok = loaded.load(skinPath);
    if (!ok || loaded.isNull()) {
        qWarning() << "TeeDrawer: failed to load skin" << skinPath
                   << "— falling back to" << defaultSkinPath();
        if (skinPath != defaultSkinPath())
            ok = loaded.load(defaultSkinPath());
    }
    if (!ok || loaded.isNull()) {
        qWarning() << "TeeDrawer: default skin missing, drawer is empty";
        return false;
    }
    SkinFile = loaded;

    TeeBody = SkinFile.copy(0, 0, 96, 96);

    auto buildEyes = [this](int srcX, int srcY, int srcW, int srcH, int rightOffset) {
        QPixmap eyes(30, 27);
        eyes.fill(Qt::transparent);
        QPainter p(&eyes);
        QPixmap eye = SkinFile.copy(srcX, srcY, srcW, srcH);
        p.drawPixmap(0, 0, eye);
        p.drawPixmap(rightOffset, 0, eye);
        return eyes;
    };

    TeeEye         = SkinFile.copy(72, 99, 17, 27);
    TeeEyes        = buildEyes(72, 99, 17, 27, 13);
    TeeEyes_Happy  = buildEyes(169, 98, 15, 27, 15);
    TeeEyes_Angry  = buildEyes(104, 98, 17, 27, 13);
    TeeEyes_Clever = buildEyes(230, 98, 17, 27, 13);

    QPixmap rawFoot = SkinFile.copy(208, 37, 32, 20);
    TeeFoot = QPixmap::fromImage(rawFoot.toImage().scaled(
        int(32 * 1.4), int(20 * 1.4),
        Qt::KeepAspectRatio, Qt::SmoothTransformation));

    TeeBare = QPixmap(96, 96);
    TeeBare.fill(Qt::transparent);
    {
        QPainter painter(&TeeBare);
        painter.drawPixmap(14, 55, TeeFoot);
        painter.drawPixmap(0, 0, TeeBody);
        painter.drawPixmap(38, 55, TeeFoot);
    }

    Tee = TeeBare;
    {
        QPainter painter(&Tee);
        painter.drawPixmap(40, 32, TeeEyes);
    }

    cTee = changeHue(Tee, 0);
    cTee = cTee.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    return true;
}
