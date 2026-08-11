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
    static QString defaultSkinPath() { return QStringLiteral(":/skins/default.png"); }
    static QPixmap adjustHsl(const QPixmap &src, int hueShift, double satFactor, double lightFactor);

    // Render a tee with the given eye type and look direction.
    // eyeIdx: 0=Normal 1=Happy 2=Angry 3=Pain 4=Surprise 5=Blink(压扁闭眼)
    // dir: unit vector pointing where the tee looks (mouse direction)
    // walkPhase: walk-cycle phase in [0,1), or <0 to use the idle pose.
    // eyeOffsetScale: how far the eyes slide within the face (0 = centred,
    // 1 = stock DDNet travel); drive it from cursor distance for a smooth
    // distance-based follow like the original Floatee.
    void render(int eyeIdx, float dirX, float dirY, float walkPhase = -1.0f,
                float eyeOffsetScale = 1.0f);

    // Split-layer rendering for remote peers (online mode): the body layer
    // (body + outline + feet) is static and only re-rendered when the skin
    // changes, while the eyes layer is re-rendered whenever the look direction
    // or eye type changes. Both draw into the same canvas with the same layout,
    // so painting body then eyes composites exactly like a single full render —
    // but each eye update costs only the small eyes region on the device that
    // is rendering the remote peer, instead of a whole tee re-render.
    void renderBody(QPixmap &out);
    void renderEyes(QPixmap &out, int eyeIdx, float dirX, float dirY,
                    float eyeOffsetScale);

    // Set the render scale (1.0 = the base 96×96 canvas / 72px tee). Selects
    // the mip-map level whose body region samples closest to 1:1 for the new
    // size and updates the render info; call before render() after a zoom.
    void setRenderScale(float scale);
    int canvasSize() const { return m_canvasSize; }
    float teeSize() const { return m_teeSize; }
    // 非透明像素包围盒（相对 canvas 左上角）：Tee 身体真正占据的像素范围，
    // 用于把 Tee 钳制在画布内时按实际渲染像素而非正方形碰撞箱。身体层渲染
    // 时计算并缓存；眼睛层在脸内，不改变整体包围盒（高频眼睛渲染零开销）。
    QRect opaqueRect() const { return m_opaqueRect; }

    // Edge anti-aliasing feather strength: 0 = off, 1 = one 3×3 pass,
    // 2 = two passes (stronger). Applied to the rendered tee after downscale.
    // Persisted by the host (default.json "Feather") and adjustable via tray menu.
    void setFeatherStrength(int strength) { m_featherStrength = qBound(0, strength, 2); }
    int featherStrength() const { return m_featherStrength; }
    // Fast mode (no SSAA / no feather): used for remote peers on weak devices,
    // where per-frame full-quality rendering can starve the event loop.
    void setFastMode(bool on) { m_fastMode = on; }
    bool fastMode() const { return m_fastMode; }
    QPixmap SkinFile;
    QPixmap Tee;       // full tee (body + feet + eyes), tee_render layout

private:
    // 独立 texture id：body / feet / eyes 各一块独立图（借鉴 QMClient sprite
    // 独立裁剪+mip，避免整张图集 mip 时不同部位边缘互相污染）
    static constexpr uint32_t BODY_TEX_ID = 1;
    static constexpr uint32_t FEET_TEX_ID = 2;
    static constexpr uint32_t EYES_TEX_ID = 3;
    enum Part { PartBody = 0, PartFeet = 1, PartEyes = 2, PartCount = 3 };
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
    QRect m_opaqueRect;    // 身体层渲染后缓存的实际非透明像素包围盒

    int m_canvasSize = BASE_CANVAS_SIZE;
    float m_teeSize = BASE_TEE_SIZE;
    int m_featherStrength = 1;
    bool m_fastMode = false;

    // 每块独立 mip 链（PartBody/PartFeet/PartEyes），m_mips[p][0] = 最大层。
    // 各块按“块内最大 sprite 采样比 ≤1 且最接近 1:1”选层（CPU 版 mipmap）。
    QVector<QPixmap> m_mips[PartCount];
    QSize m_partSize[PartCount];   // 各块实际像素尺寸

    void buildMipChain(const QPixmap &src, int part);
    // 为第 part 块选 mip 层：使块内最大 sprite 的采样像素 ≥ 其渲染像素
    // （renderSize = 该 sprite 的渲染尺寸，SSAA 时已放大）。
    void selectMip(int part, float renderSize);
    void configureRegions(int part, float partW, float partH);
    // 把参考图(256×128)坐标换算为某块的局部坐标并注册 sprite region。
    void registerRegion(int part, teer::ETeeSprite sprite,
                        float rx0, float ry0, float rx1, float ry1);
    // Post-process: feather the alpha edge (3×3 neighbourhood mean applied only
    // to semi-transparent pixels, keeping opaque interiors crisp). Small zoom
    // levels have only ~0.5px of alpha transition (the atlas edge is 1px and
    // resampling conserves information), which looks jaggy — feathering widens
    // the transition to ~1–2px for a smooth, anti-aliased edge.
    // `strength` = number of 3×3 passes (0 = none, 2 = stronger).
    static QPixmap featherAlpha(const QPixmap &src, int strength);
    // Compute the bounding box of non-transparent pixels (relative to the
    // pixmap origin); empty rect when fully transparent.
    static QRect computeOpaqueRect(const QPixmap &pm);
    static teer::EEmote mapEye(int eyeIdx);
    // Render the given layer set (TEE_PREVIEW_LAYER_*) into `out`.
    void renderLayers(QPixmap &out, int flags, int eyeIdx, float dirX, float dirY,
                      const teer::CAnimState *pAnim = nullptr);
    void renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                        bool drawEyes, bool drawFeet,
                        const teer::CAnimState *pAnim = nullptr);
};

#endif // TEEDRAWER_H
