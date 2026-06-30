#include "teedrawer.h"
#include <QDebug>
#include <QFile>
#include <QTransform>
#include <QtMath>

// ── Hue shift ──────────────────────────────────────────────────────────

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

// ── Constructor ─────────────────────────────────────────────────────────

TeeDrawer::TeeDrawer(const QString &skinPath)
{
    load(skinPath);
}

// ── Load ────────────────────────────────────────────────────────────────

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

    // ── Proportional scale factors (base = 256×128 standard sheet) ──
    double sx = static_cast<double>(SkinFile.width())  / 256.0;
    double sy = static_cast<double>(SkinFile.height()) / 128.0;

    auto copy = [&](int x, int y, int w, int h) {
        return SkinFile.copy(
            qRound(x * sx), qRound(y * sy),
            qRound(w * sx), qRound(h * sy));
    };

    // ── Body (head) ─────────────────────────────────────────────────
    // Standard: top-left 96×96 region  (1536×1536 at 4K → 96×96 at 256)
    TeeBody = copy(0, 0, 96, 96);

    // ── Eyes ────────────────────────────────────────────────────────
    // Standard: each eye region is 32×32  (512×512 at 4K → 32×32 at 256)
    // Right eye = horizontally mirrored left eye (not a copy)
    //
    // Eye source positions on a standard 256×128 sheet:
    //   Normal:  (64, 96)    Angry:  (96, 96)
    //   Clumsy: (128, 96)    Happy: (160, 96)
    //
    // Display: each 32×32 source eye → 20×20 on screen (keepAspectRatio)
    // Eye-pair canvas: 38×27, left eye at (0,3), mirrored right at (18,3)

    constexpr int kEyeSrcW = 32, kEyeSrcH = 32;
    constexpr int kEyeDispW = 20, kEyeDispH = 20;
    constexpr int kEyesCanvasW = 38, kEyesCanvasH = 27;
    constexpr int kEyePairOffset = 18;

    auto buildEyes = [&](int srcX, int srcY) {
        QPixmap eyeSrc  = copy(srcX, srcY, kEyeSrcW, kEyeSrcH);
        QPixmap eyeLeft = eyeSrc.scaled(kEyeDispW, kEyeDispH,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
        QPixmap eyeRight = eyeLeft.transformed(QTransform::fromScale(-1, 1),
                                               Qt::SmoothTransformation);

        QPixmap eyes(kEyesCanvasW, kEyesCanvasH);
        eyes.fill(Qt::transparent);
        QPainter p(&eyes);
        int yOff = (kEyesCanvasH - kEyeDispH) / 2;   // 3px – vertically center
        p.drawPixmap(0, yOff, eyeLeft);
        p.drawPixmap(kEyePairOffset, yOff, eyeRight);
        p.end();
        return eyes;
    };

    TeeEye         = copy(64, 96, kEyeSrcW, kEyeSrcH);
    TeeEyes        = buildEyes(64, 96);     // normal
    TeeEyes_Angry  = buildEyes(96, 96);     // angry
    TeeEyes_Clever = buildEyes(128, 96);    // clumsy → clever
    TeeEyes_Happy  = buildEyes(160, 96);    // happy
    TeeEyes_Close  = buildEyes(64, 96);     // fallback to normal

    // ── Foot ────────────────────────────────────────────────────────
    // Standard: left half of the foot region → 32×32 source
    //   (1024×512 at 4K → 64×32 at 256; single foot = left 32×32)
    // Display: scaled up for visibility on the 96×96 canvas
    QPixmap rawFoot = copy(192, 32, 32, 32);
    TeeFoot = rawFoot.scaled(44, 44,
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

    // ── Compose TeeBare (body + feet, no eyes) ──────────────────────
    TeeBare = QPixmap(96, 96);
    TeeBare.fill(Qt::transparent);
    {
        QPainter painter(&TeeBare);
        painter.drawPixmap(14, 55, TeeFoot);   // left foot
        painter.drawPixmap(0,  0,  TeeBody);   // body
        painter.drawPixmap(38, 55, TeeFoot);   // right foot
    }

    // ── Compose Tee (TeeBare + eyes) ────────────────────────────────
    Tee = TeeBare;
    {
        QPainter painter(&Tee);
        painter.drawPixmap(40, 32, TeeEyes);   // eyes on face
    }

    // ── Hue-shifted icon ────────────────────────────────────────────
    cTee = changeHue(Tee, 0);
    cTee = cTee.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    return true;
}
