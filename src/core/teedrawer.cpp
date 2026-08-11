#include "teedrawer.h"
#include <QDebug>
#include <QtMath>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
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
    case 5:  return teer::EMOTE_BLINK;   // 压扁闭眼（默认眼垂直压扁）
    default: return teer::EMOTE_NORMAL;
    }
}

// ── Configure sprite regions（按块）──────────────────────────────
// 参考图 256×128：body 块=(0,0,192,96)（含 body fill + outline）、
// feet 块=(192,32,256,96)（foot + foot outline）、eyes 块=(0,96,256,128)。
// 各块裁剪成独立图后，sprite 坐标换算为该块的局部 UV（QMClient sprite 独立
// 裁剪思路，避免整图 mip 时跨部位边缘污染）。

void TeeDrawer::registerRegion(int part, teer::ETeeSprite sprite,
                               float rx0, float ry0, float rx1, float ry1)
{
    const QSize &sz = m_partSize[part];
    if (sz.isEmpty())
        return;
    const float PW = static_cast<float>(sz.width());
    const float PH = static_cast<float>(sz.height());
    // 各块在参考图中的局部尺寸
    constexpr float BLOCK[PartCount][4] = { {0,0,192,96}, {192,32,256,96}, {0,96,256,128} };
    const float bx = BLOCK[part][0], by = BLOCK[part][1];
    const float bw = BLOCK[part][2] - bx, bh = BLOCK[part][3] - by;
    // 参考坐标 → 块局部 UV
    const float u0 = (rx0 - bx) / bw, v0 = (ry0 - by) / bh;
    const float u1 = (rx1 - bx) / bw, v1 = (ry1 - by) / bh;
    const float pw = (rx1 - rx0) * (PW / bw);   // 块内实际像素宽
    const float ph = (ry1 - ry0) * (PH / bh);
    m_renderer.SetSpriteRegion(sprite, teer::SSpriteRegion(u0, v0, u1, v1, pw, ph));
}

void TeeDrawer::configureRegions(int part, float, float)
{
    switch (part) {
    case PartBody:   // body 块：body fill + outline
        registerRegion(part, teer::TEE_SPRITE_BODY,          0,  0, 96, 96);
        registerRegion(part, teer::TEE_SPRITE_BODY_OUTLINE, 96,  0,192, 96);
        break;
    case PartFeet:   // feet 块：foot + outline
        registerRegion(part, teer::TEE_SPRITE_FOOT,        192, 32,256, 64);
        registerRegion(part, teer::TEE_SPRITE_FOOT_OUTLINE,192, 64,256, 96);
        break;
    case PartEyes:   // eyes 块：5 个眼睛（32×32）
        registerRegion(part, teer::TEE_SPRITE_EYES_NORMAL,   64, 96, 96,128);
        registerRegion(part, teer::TEE_SPRITE_EYES_ANGRY,    96, 96,128,128);
        registerRegion(part, teer::TEE_SPRITE_EYES_PAIN,    128, 96,160,128);
        registerRegion(part, teer::TEE_SPRITE_EYES_HAPPY,   160, 96,192,128);
        registerRegion(part, teer::TEE_SPRITE_EYES_SURPRISE,224, 96,256,128);
        break;
    default:
        break;
    }
}

// ── Render to a QPixmap ────────────────────────────────────────────────

void TeeDrawer::renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                               bool drawEyes, bool drawFeet,
                               const teer::CAnimState *pAnim)
{
    int flags = teer::TEE_PREVIEW_LAYER_BODY | teer::TEE_PREVIEW_LAYER_OUTLINE;
    if (drawFeet)  flags |= teer::TEE_PREVIEW_LAYER_FEET;
    if (drawEyes)  flags |= teer::TEE_PREVIEW_LAYER_EYES;
    renderLayers(out, flags, eyeIdx, dirX, dirY, pAnim);
}

void TeeDrawer::renderBody(QPixmap &out)
{
    // Static layer set: body + outline + feet (no eyes). Only re-rendered when
    // the skin changes.
    const int flags = teer::TEE_PREVIEW_LAYER_BODY | teer::TEE_PREVIEW_LAYER_OUTLINE
                    | teer::TEE_PREVIEW_LAYER_FEET;
    renderLayers(out, flags, 0, 0.0f, 1.0f, teer::CAnimState::GetIdle());
}

void TeeDrawer::renderEyes(QPixmap &out, int eyeIdx, float dirX, float dirY,
                           float eyeOffsetScale)
{
    // Eyes layer only — a couple of small quads, re-rendered on every eye/dir
    // change instead of re-rendering the whole tee.
    m_info.m_Skin6EyeOffsetScale = eyeOffsetScale;
    renderLayers(out, teer::TEE_PREVIEW_LAYER_EYES, eyeIdx, dirX, dirY,
                 teer::CAnimState::GetIdle());
}

void TeeDrawer::renderLayers(QPixmap &out, int flags, int eyeIdx, float dirX,
                             float dirY, const teer::CAnimState *pAnim)
{
    // 2×2 supersampling (CPU analogue of MSAA): render into a RENDER_SSAA×
    // larger canvas with the tee also scaled up, then bilinearly downscale to
    // the target size. This smooths the alpha edges — small zoom levels look
    // jaggy because the edge transition spans only 1–2 px, large levels look
    // smooth because it spans many more.
    const int ssaa = m_fastMode ? 1 : RENDER_SSAA;
    const float renderTee = m_teeSize * ssaa;
    const int renderCs = m_canvasSize * ssaa;

    QPixmap big(renderCs, renderCs);
    big.fill(Qt::transparent);
    m_backend.target = big;

    const float savedSize = m_info.m_Size;
    m_info.m_Size = renderTee;
    // Supersampled render samples a bigger tee → pick higher-resolution mips.
    // 三块各自独立选层（QMClient sprite 独立 mip）。
    selectMip(PartBody, renderTee);
    selectMip(PartFeet, renderTee * 0.8f);
    selectMip(PartEyes, renderTee * 0.6f);

    if (pAnim == nullptr)
        pAnim = teer::CAnimState::GetIdle();

    // Configure render flags: control which layers are drawn
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
    // Fast mode skips it (remote peers on weak devices).
    if (m_featherStrength > 0 && !m_fastMode)
        out = featherAlpha(out, m_featherStrength);

    // 身体层（含 outline/feet）渲染后缓存实际非透明像素包围盒，供画布内
    // 钳制用（按真实渲染像素而非正方形碰撞箱）。眼睛层在脸内，不改变整体
    // 包围盒，因此 eyes-only 渲染不更新 —— 高频眼睛渲染零额外开销。
    if (flags & teer::TEE_PREVIEW_LAYER_BODY)
        m_opaqueRect = computeOpaqueRect(out);

    m_info.m_Size = savedSize;
}

QRect TeeDrawer::computeOpaqueRect(const QPixmap &pm)
{
    if (pm.isNull())
        return QRect();
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(line[x]) > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0)
        return QRect();   // 全透明
    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
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

// ── Mip-map chain + render scale（按块独立）──────────────────────

void TeeDrawer::buildMipChain(const QPixmap &src, int part)
{
    m_mips[part].clear();
    // Start from a cleanly-low-passed block (≤ MIP_MAX_DIM) and halve it down
    // to MIP_MIN_DIM; each level is a 2× bilinear low-pass of the previous one.
    QPixmap cur = downscaleToMaxDim(src, MIP_MAX_DIM);
    m_partSize[part] = cur.size();
    while (true) {
        m_mips[part].push_back(cur);
        if (qMax(cur.width(), cur.height()) <= MIP_MIN_DIM)
            break;
        cur = cur.scaled(qMax(1, cur.width() / 2), qMax(1, cur.height() / 2),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
}

void TeeDrawer::selectMip(int part, float renderSize)
{
    auto &chain = m_mips[part];
    if (chain.isEmpty())
        return;
    // 块内最大 sprite 参考宽 / 块参考宽 的比例（决定采样比）
    constexpr float SPRITE_RATIO[PartCount] = { 96.0f / 192.0f,  // body: body sprite 96/块 192
                                                64.0f /  64.0f,  // feet: foot 64/块 64
                                                32.0f / 256.0f };// eyes: eye 32/块 256
    const float ratio = SPRITE_RATIO[part];
    // 选最小层使 spritePx(=ratio×块宽) ≥ renderSize（采样比≤1，尽量接近1:1）
    int best = 0;
    for (int i = chain.size() - 1; i >= 0; --i) {
        if (ratio * chain[i].width() >= renderSize) {
            best = i;
            break;
        }
    }
    const uint32_t id = (part == PartBody) ? BODY_TEX_ID
                       : (part == PartFeet) ? FEET_TEX_ID : EYES_TEX_ID;
    m_backend.registerTexture(id, chain[best]);
}

void TeeDrawer::setRenderScale(float scale)
{
    m_canvasSize = qMax(32, qRound(BASE_CANVAS_SIZE * scale));
    m_teeSize = BASE_TEE_SIZE * scale;
    m_info.m_Size = m_teeSize;
    // 三块各自选 mip（body 渲染尺寸≈tee；feet≈0.8×tee；eyes≈0.6×tee）
    selectMip(PartBody, m_teeSize);
    selectMip(PartFeet, m_teeSize * 0.8f);
    selectMip(PartEyes, m_teeSize * 0.6f);
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
    // 校验：加载"成功"但全透明/尺寸过小的坏皮肤（如残留的损坏或空测试皮肤）
    // 渲染出来会完全不可见 → 同样视为失败并回落到 default，避免 Tee 空白。
    if (ok && !loaded.isNull()
        && (loaded.width() < 8 || loaded.height() < 8
            || computeOpaqueRect(loaded).isEmpty()))
        ok = false;
    if (!ok || loaded.isNull()) {
        qWarning() << "TeeDrawer: failed to load skin" << skinPath
                   << "— falling back to" << defaultSkinPath();
        if (skinPath != defaultSkinPath())
            ok = loaded.load(defaultSkinPath());
    }
    if (!ok || loaded.isNull()) {
        // 终极兜底：qrc 资源缺失（如旧构建 exe 未内嵌资源）时，尝试从应用
        // 数据目录 skins/ 加载同名 default 副本（用户/部署可手动放置），避免
        // Tee 完全空白。
        const QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                 + QStringLiteral("/skins/") + QFileInfo(defaultSkinPath()).fileName();
        if (QFile::exists(fallback)) {
            qWarning() << "TeeDrawer: qrc default missing, trying" << fallback;
            ok = loaded.load(fallback);
        }
    }
    if (!ok || loaded.isNull()) {
        qWarning() << "TeeDrawer: default skin missing, drawer is empty";
        return false;
    }
    SkinFile = loaded;

    if (hueShift != 0 || satFactor != 1.0 || lightFactor != 1.0)
        SkinFile = adjustHsl(SkinFile, hueShift, satFactor, lightFactor);

    // 从整图裁剪三块独立图（body/feet/eyes），各自建 mip 链（QMClient sprite
    // 独立裁剪思路，避免整图 mip 时跨部位边缘污染）。坐标按实际皮肤尺寸缩放。
    const int skinW = SkinFile.width(), skinH = SkinFile.height();
    const auto px = [&](float v, int full) { return qRound(v * full / 256.0f); };
    const auto py = [&](float v, int full) { return qRound(v * full / 128.0f); };
    const QPixmap bodyPix = SkinFile.copy(QRect(px(0, skinW), py(0, skinH),
                                                px(192, skinW), py(96, skinH)));
    const QPixmap feetPix = SkinFile.copy(QRect(px(192, skinW), py(32, skinH),
                                                px(64, skinW), py(64, skinH)));
    const QPixmap eyesPix = SkinFile.copy(QRect(px(0, skinW), py(96, skinH),
                                                px(256, skinW), py(32, skinH)));
    buildMipChain(bodyPix, PartBody);
    buildMipChain(feetPix, PartFeet);
    buildMipChain(eyesPix, PartEyes);
    configureRegions(PartBody, 0, 0);
    configureRegions(PartFeet, 0, 0);
    configureRegions(PartEyes, 0, 0);

    // Set up render info for protocol-7 six-part skin
    m_info.Reset();
    setRenderScale(1.0f);   // default: canvas 96, tee 72, picks the best mip
    m_info.m_GotAirJump = true;

    teer::SSixupSkin &sixup = m_info.m_aSixup[0];
    sixup.Reset();
    // body/feet/eyes 各用独立纹理 id（对应三块独立 mip 图）
    sixup.m_aOriginalTextures[teer::SKINPART_BODY] = teer::STextureHandle(BODY_TEX_ID);
    sixup.m_aOriginalTextures[teer::SKINPART_FEET] = teer::STextureHandle(FEET_TEX_ID);
    sixup.m_aOriginalTextures[teer::SKINPART_EYES] = teer::STextureHandle(EYES_TEX_ID);
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
