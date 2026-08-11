#include "tee_qt_backend.h"
#include <QPainter>
#include <cmath>

void QPixmapBackend::DrawQuad(const teer::STeeQuad &quad)
{
    auto it = textures.find(quad.m_Texture.m_Id);
    if (it == textures.end() || it->isNull())
        return;

    const QPixmap &src = it.value();
    const float tw = static_cast<float>(src.width());
    const float th = static_cast<float>(src.height());

    // Source rect in texture pixels (UV → pixel coords). Inset by half a texel
    // so bilinear sampling never reaches past a sprite's own pixels into the
    // adjacent sprite that shares the same block texture (body fill ↔ outline,
    // neighbouring eyes, foot ↔ foot outline). Without this the sprite edges
    // bleed each other's colour in and the silhouette gets dark/jagged lines.
    const float sx0 = quad.m_U0 * tw, sy0 = quad.m_V0 * th;
    const float sx1 = quad.m_U1 * tw, sy1 = quad.m_V1 * th;
    QRectF srcRect(sx0 + 0.5f, sy0 + 0.5f,
                   qMax(1.0f, sx1 - sx0 - 1.0f),
                   qMax(1.0f, sy1 - sy0 - 1.0f));

    // Destination rect centered at quad position
    QRectF destRect(
        quad.m_Position.x - quad.m_Width  / 2.0f,
        quad.m_Position.y - quad.m_Height / 2.0f,
        quad.m_Width,
        quad.m_Height);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Apply color modulation via composition mode (multiply for color, source-over for alpha)
    // tee_render uses color as a multiply tint; white = no tint.
    // For simplicity we use QPainter's opacity for alpha and skip RGB multiply
    // (most skins use white color anyway).
    if (quad.m_Color.a < 1.0f)
        painter.setOpacity(quad.m_Color.a);

    painter.save();
    painter.translate(quad.m_Position.x, quad.m_Position.y);
    painter.rotate(quad.m_Rotation * 180.0f / teer::PI);
    if (quad.m_FlipX)
        painter.scale(-1.0, 1.0);

    QRectF centeredDest(-quad.m_Width / 2.0f, -quad.m_Height / 2.0f,
                        quad.m_Width, quad.m_Height);
    painter.drawPixmap(centeredDest, src, srcRect);

    painter.restore();
}