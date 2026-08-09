#ifndef TEEDRAWER_H
#define TEEDRAWER_H

#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QImage>
#include <QString>
#include <QVector>
#include "tee_qt_backend.h"
#include "tee_renderer.h"
#include "tee_render_info.h"
#include "tee_anim.h"

class TeeDrawer
{
public:
    explicit TeeDrawer(const QString &skinPath = defaultSkinPath());

    bool load(const QString &skinPath, int hueShift = 0, double satFactor = 1.0, double lightFactor = 1.0);
    static QString defaultSkinPath() { return QStringLiteral(":/skins/Tata.png"); }
    static QPixmap adjustHsl(const QPixmap &src, int hueShift, double satFactor, double lightFactor);

    // Render a tee with the given eye type and look direction.
    // eyeIdx: 0=Normal 1=Happy 2=Angry 3=Pain 4=Surprise
    // dir: unit vector pointing where the tee looks (mouse direction)
    // walkPhase: walk-cycle phase in [0,1), or <0 to use the idle pose.
    // eyeOffsetScale: how far the eyes slide within the face (0 = centred,
    // 1 = stock DDNet travel); drive it from cursor distance for a smooth
    // distance-based follow like the original Floatee.
    void render(int eyeIdx, float dirX, float dirY, float walkPhase = -1.0f,
                float eyeOffsetScale = 1.0f);

    // Set the render scale (1.0 = the base 96×96 canvas / 72px tee). Selects
    // the mip-map level whose body region samples closest to 1:1 for the new
    // size and updates the render info; call before render() after a zoom.
    void setRenderScale(float scale);
    int canvasSize() const { return m_canvasSize; }
    float teeSize() const { return m_teeSize; }

    QPixmap SkinFile;
    QPixmap Tee;       // full tee (body + feet + eyes), tee_render layout

private:
    static constexpr uint32_t SKIN_TEX_ID = 1;
    // Base (scale 1.0) canvas / tee size.
    static constexpr int BASE_CANVAS_SIZE = 96;
    static constexpr float BASE_TEE_SIZE = 72.0f;
    // Mip chain bounds (largest atlas dimension / smallest level).
    static constexpr int MIP_MAX_DIM = 1024;
    static constexpr int MIP_MIN_DIM = 32;

    QPixmapBackend m_backend;
    teer::CTeeRenderer m_renderer;
    teer::STeeRenderInfo m_info;

    int m_canvasSize = BASE_CANVAS_SIZE;
    float m_teeSize = BASE_TEE_SIZE;

    // Mip-map chain of the skin atlas (m_mips[0] = largest). Each level is a
    // clean 2× low-pass of the previous one; the renderer samples the level
    // whose body region is closest to 1:1 with the current render size, so
    // zooming in/out never aliases (CPU analogue of GPU mipmaps; trilinear
    // blending between adjacent levels is optional and not implemented).
    QVector<QPixmap> m_mips;

    void buildMipChain(const QPixmap &src);
    void selectMip();
    void configureRegions(float skinW, float skinH);
    static teer::EEmote mapEye(int eyeIdx);
    void renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                        bool drawEyes, bool drawFeet,
                        const teer::CAnimState *pAnim = nullptr);
};

#endif // TEEDRAWER_H
