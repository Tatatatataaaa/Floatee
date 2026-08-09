#ifndef TEEDRAWER_H
#define TEEDRAWER_H

#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QImage>
#include <QString>
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
    void render(int eyeIdx, float dirX, float dirY, float walkPhase = -1.0f);

    QPixmap SkinFile;
    QPixmap Tee;       // full tee (body + feet + eyes), tee_render layout

private:
    static constexpr uint32_t SKIN_TEX_ID = 1;
    static constexpr int CANVAS_SIZE = 96;
    // tee_render base size. With the authentic GetRenderTeeOffsetToRenderedTee
    // centering, this is the largest size that keeps the whole tee (body +
    // feet + outline) inside the 96×96 canvas.
    static constexpr float TEE_SIZE = 72.0f;

    QPixmapBackend m_backend;
    teer::CTeeRenderer m_renderer;
    teer::STeeRenderInfo m_info;

    // Cleanly-downscaled skin atlas actually used for rendering. Large (4K)
    // atlases are halved repeatedly so sampling into the 96×96 canvas stays
    // near 1:1 and never aliases (bilinear from 4096px produces jaggies).
    QPixmap m_workingSkin;

    void configureRegions(float skinW, float skinH);
    static teer::EEmote mapEye(int eyeIdx);
    void renderToPixmap(QPixmap &out, int eyeIdx, float dirX, float dirY,
                        bool drawEyes, bool drawFeet,
                        const teer::CAnimState *pAnim = nullptr);
};

#endif // TEEDRAWER_H
