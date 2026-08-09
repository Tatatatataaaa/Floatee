#ifndef EMOTICONWINDOW_H
#define EMOTICONWINDOW_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QPixmap>
#include "core/tee_qt_backend.h"
#include "tee_emoticon.h"

/**
 * Standalone over-head emoticon (bubble) animation component.
 *
 * On this Windows/Qt setup a SECOND translucent layered window is never
 * composited by the DWM (its backing store has the content — confirmed via
 * PrintWindow — but nothing reaches the screen, whether it is shown at startup
 * or dynamically). So this class keeps ALL the emoticon logic (atlas, renderer,
 * animation clock, trigger API) separate, and renders its current animation
 * frame into a target pixmap that the host (Floatee) draws in its own window's
 * paintEvent — the tee window composites correctly, so the bubble actually
 * shows, drawn above the tee.
 */
class EmoticonWindow : public QObject
{
    Q_OBJECT
public:
    explicit EmoticonWindow(QObject *parent = nullptr);

    // Register the emoticon atlas (emoticons.png, a 4×4 grid) as texture id 2.
    bool loadAtlas(const QPixmap &atlas);

    // Trigger a 2-second emoticon animation.
    //   index  : 0..NUM_EMOTICONS-1 (see teer::EEmoticonSprite)
    //   teeSize: current tee render size (for SetTeeSize scaling)
    //   teePos : the tee's TeePos in the square tee-canvas coordinates
    void showEmoticon(int index, float teeSize, const QPointF &teePos);
    void hideEmoticon();

    bool isActive() const { return m_index >= 0; }

    // Render the current animation frame into target (sized by the host to its
    // window). teePosInTarget = the tee's TeePos in the host window's
    // coordinates (the tee canvas shifted down by the headroom). Returns false
    // when no emoticon is active.
    bool renderFrame(QPixmap &target, const QPointF &teePosInTarget);

signals:
    void frameChanged();   // host should repaint (new animation frame / ended)

private slots:
    void updateFrame();

private:
    static constexpr uint32_t EMOTICON_TEX_ID = 2;

    QPixmapBackend m_backend;
    teer::CEmoticonRenderer m_renderer{&m_backend, teer::STextureHandle(EMOTICON_TEX_ID)};
    QTimer m_frameTimer;
    QElapsedTimer m_clock;
    int m_index = -1;     // -1 = no active emoticon
    float m_teeSize = 64.0f;
    QPointF m_teePos;     // TeePos in the square tee-canvas coordinates
};

#endif // EMOTICONWINDOW_H
