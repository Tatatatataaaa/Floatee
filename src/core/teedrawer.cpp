#include "teedrawer.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>

// ── Working-atlas helpers ──────────────────────────────────────────────

// Repeatedly halve the pixmap (2× bilinear each step) until its largest side
// is <= maxDim. Halving in steps acts as a low-pass filter — a cheap CPU
// substitute for mipmap generation — so that sampling the atlas into the small
// 96×96 canvas stays near 1:1. A single one-shot bilinear downscale (e.g.
// 4096→96, ~16× for the body region) aliases badly and produces jaggies.
static QPixmap downscaleToMaxDim(const QPixmap &src, int maxDim)
{
    QPixmap cur = src;
    while (qMax(cur.width(), cur.height()) > maxDim) {
        cur = cur.scaled(qMax(1, cur.width() / 2), qMax(1, cur.height() / 2),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return cur;
}

// ── HSL adjustment ────────────────────────────────────────────────────

QPixmap TeeDrawer::adjustHsl(const QPixmap &src, int hueShift,
                             double satFactor, double lightFactor)
{
    // Use non-premultiplied RGBA8888 (Qt6 canonical format)
    QImage image = src.toImage().convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QColor color = QColor::fromRgba(line[x]);
            if (color.alpha() == 0)
                continue;
            float h, s, l, a;
            color.getHslF(&h, &s, &l, &a);
            if (h >= 0) {
                h = std::fmod(h + hueShift / 360.0f, 1.0f);
                s = std::clamp(s * static_cast<float>(satFactor), 0.0f, 1.0f);
            } else if (hueShift != 0) {
                // Inject hue into achromatic pixels so shift is visible
                h = std::fmod(hueShift / 360.0f, 1.0f);
                s = std::clamp(0.5f * static_cast<float>(satFactor), 0.0f, 1.0f);
            }
            if (h < 0) h += 1.0f;
            l = std::clamp(l * static_cast<float>(lightFactor), 0.0f, 1.0f);
            color.setHslF(h, s, l, a);
            line[x] = color.rgba();
        }
    }
    return QPixmap::fromImage(image);
}

// ── Constructor ─────────────────────────────────────────────────────────

TeeDrawer::TeeDrawer(const QString &skinPath)
    : m_renderer(&m_backend)
{
    load(skinPath, 0, 1.0, 1.0);
}

// ── Eye index → tee_render EMOTE mapping ───────────────────────────────

teer::EEmote TeeDrawer::mapEye(int eyeIdx)
{
    switch (eyeIdx) {
    case 1:  return teer::EMOTE_HAPPY;
    case 2:  return teer::EMOTE_ANGRY;
    case 3:  return teer::EMOTE_PAIN;
    case 4:  return teer::EMOTE_SURPRISE;
    default: return teer::EMOTE_NORMAL;
    }
}

// ── Configure sprite regions for the skin atlas ───────────────────────
// Coordinates are given in the standard 256×128 reference sheet and are
// scaled to the actual skin dimensions, so this works for both 256×128 and
// 4K (4096×2048) skins. All regions are always configured; empty regions
// render as transparent (no effect).

void TeeDrawer::configureRegions(float skinW, float skinH)
{
    // Scale from the 256×128 reference sheet to the actual skin atlas.
    const float sx = skinW / 256.0f;
    const float sy = skinH / 128.0f;
    const float TW = skinW, TH = skinH;

    auto region = [&](float x0, float y0, float x1, float y1) {
        return teer::SSpriteRegion(x0 * sx / TW, y0 * sy / TH,
                                   x1 * sx / TW, y1 * sy / TH,
                                   (x1 - x0) * sx, (y1 - y0) * sy);
    };

    // Body base (A region): top-left
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_BODY, region(0, 0, 96, 96));
    // Body outline (B region): next 96×96 to the right
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_BODY_OUTLINE, region(96, 0, 192, 96));
    // Foot base (E region)
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_FOOT, region(192, 32, 256, 64));
    // Foot outline (F region)
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_FOOT_OUTLINE, region(192, 64, 256, 96));
    // Eyes (G1~G4 + H)
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_EYES_NORMAL,   region(64, 96, 96, 128));
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_EYES_ANGRY,    region(96, 96, 128, 128));
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_EYES_PAIN,     region(128, 96, 160, 128));
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_EYES_HAPPY,    region(160, 96, 192, 128));
    m_renderer.SetSpriteRegion(teer::TEE_SPRITE_EYES_SURPRISE, region(224, 96, 256, 128));
}

// ── Render to a QPixmap ────────────────────────────────────────────────

void TeeDrawer::renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                               bool drawEyes, bool drawFeet,
                               const teer::CAnimState *pAnim)
{
    // 2×2 supersampling (CPU analogue of MSAA): render into a RENDER_SSAA×
    // larger canvas with the tee also scaled up, then bilinearly downscale to
    // the target size. This smooths the alpha edges — small zoom levels look
    // jaggy because the edge transition spans only 1–2 px, large levels look
    // smooth because it spans many more.
    const float renderTee = m_teeSize * RENDER_SSAA;
    const int renderCs = m_canvasSize * RENDER_SSAA;

    QPixmap big(renderCs, renderCs);
    big.fill(Qt::transparent);
    m_backend.target = big;

    const float savedSize = m_info.m_Size;
    m_info.m_Size = renderTee;
    // Supersampled render samples a bigger tee → pick a higher-resolution mip
    // so the upsample stays crisp.
    selectMip(renderTee);

    if (pAnim == nullptr)
        pAnim = teer::CAnimState::GetIdle();

    // Configure render flags: control which layers are drawn
    int flags = teer::TEE_PREVIEW_LAYER_BODY | teer::TEE_PREVIEW_LAYER_OUTLINE;
    if (drawFeet)  flags |= teer::TEE_PREVIEW_LAYER_FEET;
    if (drawEyes)  flags |= teer::TEE_PREVIEW_LAYER_EYES;
    m_info.m_TeeRenderFlags = flags;

    // Authentic tee_render layout: GetRenderTeeOffsetToRenderedTee returns the
    // offset that makes the whole rendered tee (body + feet) center on Pos, so
    // the tee is centered in the canvas with the feet hanging below the body.
    teer::vec2 offset;
    teer::CTeeRenderer::GetRenderTeeOffsetToRenderedTee(pAnim, &m_info, offset);
    const teer::vec2 pos(renderCs / 2.0f, renderCs / 2.0f + offset.y);

    m_renderer.RenderTee(pAnim, &m_info, mapEye(eyeIdx),
                         teer::vec2(dirX, dirY), pos, 1.0f);

    out = m_backend.target;
    out = out.scaled(m_canvasSize, m_canvasSize,
                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Feather the alpha edge so small sizes render with smooth, anti-aliased
    // outlines instead of hard ~0.5px jaggies (opaque interiors stay crisp).
    if (m_featherStrength > 0)
        out = featherAlpha(out, m_featherStrength);

    m_info.m_Size = savedSize;
}

QPixmap TeeDrawer::featherAlpha(const QPixmap &src, int strength)
{
    QPixmap cur = src;
    for (int pass = 0; pass < strength; ++pass) {
        QImage img = cur.toImage().convertToFormat(QImage::Format_RGBA8888); // non-premultiplied
        const int w = img.width(), h = img.height();
        const int stride = img.bytesPerLine();
        const uchar *bits = img.constBits();
        QImage out = img.copy();
        uchar *ob = out.bits();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const uchar *p = bits + y * stride + x * 4;
                const int a = p[3];
                if (a == 0 || a == 255)
                    continue;                // skip interior/fully-transparent
                // 3×3 box mean of alpha (edge pixel only) — feathered outward.
                int sum = 0, n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = x + dx, ny = y + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                            continue;
                        sum += bits[ny * stride + nx * 4 + 3];
                        ++n;
                    }
                }
                uchar *op = ob + y * stride + x * 4;
                op[3] = uchar(qMax(a, sum / n)); // grow the edge outward smoothly
            }
        }
        cur = QPixmap::fromImage(out);
    }
    return cur;
}

// ── Mip-map chain + render scale ───────────────────────────────────────

void TeeDrawer::buildMipChain(const QPixmap &src)
{
    m_mips.clear();
    // Start from a cleanly-low-passed atlas (≤ MIP_MAX_DIM) and halve it down
    // to MIP_MIN_DIM; each level is a 2× bilinear low-pass of the previous one.
    QPixmap cur = downscaleToMaxDim(src, MIP_MAX_DIM);
    while (true) {
        m_mips.push_back(cur);
        if (qMax(cur.width(), cur.height()) <= MIP_MIN_DIM)
            break;
        cur = cur.scaled(qMax(1, cur.width() / 2), qMax(1, cur.height() / 2),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
}

void TeeDrawer::selectMip(float renderTeeSize)
{
    if (m_mips.isEmpty())
        return;
    // Body region width at a mip level = 96/256 × atlas width. m_mips is ordered
    // largest → smallest (index 0 = largest). We want the SMALLEST level whose
    // body region is still >= the render body size: sampling ratio ≤ 1 (crisp
    // downscale) while staying as close to 1:1 as possible. Scan from the
    // smallest level upward so large atlases are not over-sampled (a 1024 atlas
    // sampled into a 72px body would alias ~5×). If every level is smaller
    // (zoomed in beyond the chain), fall back to the largest mip (index 0).
    int best = 0;
    for (int i = m_mips.size() - 1; i >= 0; --i) {
        const float bodyPx = (BASE_CANVAS_SIZE * m_mips[i].width()) / 256.0f;
        if (bodyPx >= renderTeeSize) {
            best = i;
            break;
        }
    }
    m_backend.registerTexture(SKIN_TEX_ID, m_mips[best]);
}

void TeeDrawer::setRenderScale(float scale)
{
    m_canvasSize = qMax(32, qRound(BASE_CANVAS_SIZE * scale));
    m_teeSize = BASE_TEE_SIZE * scale;
    m_info.m_Size = m_teeSize;
    selectMip(m_teeSize);
}

// ── Public render entry ────────────────────────────────────────────────

void TeeDrawer::render(int eyeIdx, float dirX, float dirY, float walkPhase,
                       float eyeOffsetScale)
{
    // The complete tee (body + feet + eyes) in tee_render's authentic layout,
    // rendered as a single image. The eyes follow the look direction (dirX/Y),
    // which is driven by the cursor by the host.
    //
    // walkPhase in [0,1) selects the walk cycle (base pose + walk keyframes,
    // exactly how DDNet drives the feet by movement distance); <0 = idle.
    const teer::CAnimState *pAnim = teer::CAnimState::GetIdle();
    teer::CAnimState walkState;
    if (walkPhase >= 0.0f)
    {
        walkState.Set(&teer::s_aAnimations[teer::ANIM_BASE], 0.0f);
        walkState.Add(&teer::s_aAnimations[teer::ANIM_WALK], walkPhase, 1.0f);
        pAnim = &walkState;
    }

    m_info.m_Skin6EyeOffsetScale = eyeOffsetScale;
    renderToPixmap(Tee, eyeIdx, dirX, dirY, true, true, pAnim);
}

// ── Load ────────────────────────────────────────────────────────────────

bool TeeDrawer::load(const QString &skinPath,
                  int hueShift, double satFactor, double lightFactor)
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

    if (hueShift != 0 || satFactor != 1.0 || lightFactor != 1.0)
        SkinFile = adjustHsl(SkinFile, hueShift, satFactor, lightFactor);

    // Build the mip-map chain (each level a clean 2× low-pass of the previous)
    // and register the level best matching the current render scale. This is
    // the CPU analogue of GPU mipmaps: sampling stays near 1:1 so zooming in
    // or out never aliases (a single bilinear pass from a huge atlas would).
    buildMipChain(SkinFile);

    // Configure sprite regions (normalized UVs are resolution-independent, so
    // the same regions are valid for every mip level)
    configureRegions(static_cast<float>(m_mips.first().width()),
                     static_cast<float>(m_mips.first().height()));

    // Set up render info for protocol-7 six-part skin
    m_info.Reset();
    setRenderScale(1.0f);   // default: canvas 96, tee 72, picks the best mip
    m_info.m_GotAirJump = true;

    teer::SSixupSkin &sixup = m_info.m_aSixup[0];
    sixup.Reset();
    // All parts (body, eyes, feet) come from the same skin atlas (texture id 1)
    sixup.m_aOriginalTextures[teer::SKINPART_BODY] = teer::STextureHandle(SKIN_TEX_ID);
    sixup.m_aOriginalTextures[teer::SKINPART_FEET] = teer::STextureHandle(SKIN_TEX_ID);
    sixup.m_aOriginalTextures[teer::SKINPART_EYES] = teer::STextureHandle(SKIN_TEX_ID);
    // Use white colors (texture provides the actual colors)
    sixup.m_aColors[teer::SKINPART_BODY] = teer::ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    sixup.m_aColors[teer::SKINPART_FEET] = teer::ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    sixup.m_aColors[teer::SKINPART_EYES] = teer::ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

    // Skin6 eye pair mode: two eyes from one texture tile, second mirrored
    m_info.m_Skin6EyePair = true;
    m_info.m_Skin6EyeSeparationScale = 1.0f;
    // Constant eye spacing regardless of look direction: the host drives the
    // eye offset from cursor distance, so the stock DDNet convergence term
    // (which depends on |Dir.x|) would make the spacing wobble with the cursor
    // (e.g. jump by 0.72px when the cursor crosses the tee centre).
    m_info.m_Skin6EyeSeparationDirectionScale = 0.0f;

    // Initial render with default eye (Normal) looking right
    render(0, 1.0f, 0.0f);

    return true;
}
