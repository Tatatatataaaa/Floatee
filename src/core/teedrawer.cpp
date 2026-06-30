#include "teedrawer.h"
#include <QDebug>

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
    TeeBody = copy(0, 0, 96, 96).scaled(96, 96,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);

    // ── Eyes ────────────────────────────────────────────────────────
    // Standard: each eye region is 32×32  (512×512 at 4K → 32×32 at 256)
    // Right eye = horizontally mirrored left eye (not a copy)
    //
    // Eye source positions on a standard 256×128 sheet:
    //   Normal:  (64, 96)    Angry:  (96, 96)
    //   Clumsy: (128, 96)    Happy: (160, 96)
    //
    // Display: 1:1 from source (32×32 per eye), same as body 96→96
    // Eye-pair canvas: 52×32, left eye at (0,0), mirrored right at (20,0)

    constexpr int kEyeSrcW = 32, kEyeSrcH = 32;
    constexpr int kEyeDispW = 32, kEyeDispH = 32;
    constexpr int kEyesCanvasW = 52, kEyesCanvasH = 32;
    constexpr int kEyePairOffset = 16;

    auto buildEyes = [&](int srcX, int srcY) {
        QPixmap eyeSrc  = copy(srcX, srcY, kEyeSrcW, kEyeSrcH);
        QPixmap eyeLeft = eyeSrc.scaled(kEyeDispW, kEyeDispH,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
        QPixmap eyeRight = QPixmap::fromImage(
            eyeLeft.toImage().flipped(Qt::Horizontal));

        QPixmap eyes(kEyesCanvasW, kEyesCanvasH);
        eyes.fill(Qt::transparent);
        QPainter p(&eyes);
        p.drawPixmap(0, 0, eyeLeft);
        p.drawPixmap(kEyePairOffset, 0, eyeRight);
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
    // E zone: full 64×32 foot region at standard 256×128
    QPixmap rawFoot = copy(192, 32, 64, 32);
    TeeFoot = rawFoot.scaled(64, 32,
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    QPixmap rightFoot = QPixmap::fromImage(
        TeeFoot.toImage().flipped(Qt::Horizontal));

    // ── Compose TeeBare (body + feet, no eyes) ──────────────────────
    TeeBare = QPixmap(96, 96);
    TeeBare.fill(Qt::transparent);
    {
        QPainter painter(&TeeBare);
        painter.drawPixmap(0,  56, TeeFoot);    // left foot
        painter.drawPixmap(0,  0,  TeeBody);    // body on top
        painter.drawPixmap(34, 56, rightFoot);  // right foot (mirrored)
    }

    // ── Compose Tee (TeeBare + eyes, for tray/window icon) ──────────
    Tee = TeeBare;
    {
        QPainter painter(&Tee);
        painter.drawPixmap(20, 28, TeeEyes);   // eyes slightly left for icon view
    }

    return true;
}
