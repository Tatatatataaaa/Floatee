#ifndef TEE_QT_BACKEND_H
#define TEE_QT_BACKEND_H

#include <QPixmap>
#include <QHash>
#include "tee_backend.h"

/**
 * Qt software rasterizer backend for the tee_render pipeline.
 *
 * Each STeeQuad is drawn onto a QPixmap via QPainter.  Texture handles
 * (uint32_t) are mapped to source QPixmaps registered by the host.
 *
 * The output pixmap is accessible via `target` after each render call.
 */
class QPixmapBackend : public teer::ITeeRenderBackend
{
public:
    QPixmap target;                                   // output canvas
    QHash<uint32_t, QPixmap> textures;                // texture-id → source pixmap

    void registerTexture(uint32_t id, const QPixmap &pix) { textures[id] = pix; }

    void BeginTee() override {}
    void EndTee() override {}

    void DrawQuad(const teer::STeeQuad &quad) override;
};

#endif // TEE_QT_BACKEND_H