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

    // Edge anti-aliasing feather strength: 0 = off, 1 = one 3×3 pass,
    // 2 = two passes (stronger). Applied to the rendered tee after downscale.
    // Persisted by the host (default.json "Feather") and adjustable via tray menu.
    void setFeatherStrength(int strength) { m_featherStrength = qBound(0, strength, 2); }
    int featherStrength() const { return m_featherStrength; }

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
    // Supersampling factor: the tee is rendered into a RENDER_SSAA× larger
    // canvas and then bilinearly downscaled to the target size — the CPU
    // analogue of MSAA. This removes the visible jaggies at small zoom levels
    // (large levels already look smooth because the alpha edges span more
    // pixels). Cost: RENDER_SSAA²× pixels per render.
    static constexpr int RENDER_SSAA = 2;

    QPixmapBackend m_backend;
    teer::CTeeRenderer m_renderer;
    teer::STeeRenderInfo m_info;

    int m_canvasSize = BASE_CANVAS_SIZE;
    float m_teeSize = BASE_TEE_SIZE;
    int m_featherStrength = 1;

    // Mip-map chain of the skin atlas (m_mips[0] = largest). Each level is a
    // clean 2× low-pass of the previous one; the renderer samples the level
    // whose body region is closest to 1:1 with the current render size, so
    // zooming in/out never aliases (CPU analogue of GPU mipmaps; trilinear
    // blending between adjacent levels is optional and not implemented).
    QVector<QPixmap> m_mips;

    void buildMipChain(const QPixmap &src);
    // Select the mip level for the given render tee size (the supersampled
    // size when SSAA is active, so a higher-resolution atlas is used).
    void selectMip(float renderTeeSize);
    void configureRegions(float skinW, float skinH);
    // Post-process: feather the alpha edge (3×3 neighbourhood mean applied only
    // to semi-transparent pixels, keeping opaque interiors crisp). Small zoom
    // levels have only ~0.5px of alpha transition (the atlas edge is 1px and
    // resampling conserves information), which looks jaggy — feathering widens
    // the transition to ~1–2px for a smooth, anti-aliased edge.
    // `strength` = number of 3×3 passes (0 = none, 2 = stronger).
    static QPixmap featherAlpha(const QPixmap &src, int strength);
    static teer::EEmote mapEye(int eyeIdx);
    void renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                        bool drawEyes, bool drawFeet,
                        const teer::CAnimState *pAnim = nullptr);
};

#endif // TEEDRAWER_H
