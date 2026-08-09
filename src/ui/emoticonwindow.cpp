#include "emoticonwindow.h"
#include <cmath>

EmoticonWindow::EmoticonWindow(QObject *parent)
    : QObject(parent)
{
    m_frameTimer.setInterval(16);   // ~60 fps animation
    connect(&m_frameTimer, &QTimer::timeout, this, &EmoticonWindow::updateFrame);
}

bool EmoticonWindow::loadAtlas(const QPixmap &atlas)
{
    if (atlas.isNull())
        return false;
    m_backend.registerTexture(EMOTICON_TEX_ID, atlas);
    m_renderer.ConfigureEmoticonGrid(4, 4);   // emoticons.png is a 4×4 grid
    return true;
}

void EmoticonWindow::showEmoticon(int index, float teeSize, const QPointF &teePos)
{
    if (index < 0 || index >= teer::NUM_EMOTICONS)
        return;
    m_index = index;
    m_teeSize = teeSize;
    m_teePos = teePos;
    m_renderer.SetTeeSize(m_teeSize);
    m_clock.restart();
    updateFrame();
    m_frameTimer.start();
}

void EmoticonWindow::hideEmoticon()
{
    m_frameTimer.stop();
    m_index = -1;
    emit frameChanged();
}

void EmoticonWindow::updateFrame()
{
    if (m_index < 0)
        return;
    if (m_clock.elapsed() / 1000.0f >= 2.0f) {   // 2-second lifetime
        hideEmoticon();
        return;
    }
    emit frameChanged();
}

bool EmoticonWindow::renderFrame(QPixmap &target, const QPointF &teePosInTarget)
{
    if (m_index < 0)
        return false;
    const float elapsed = m_clock.elapsed() / 1000.0f;
    if (elapsed >= 2.0f) {
        hideEmoticon();
        return false;
    }

    // Render the current animation frame via the pipeline.
    target.fill(Qt::transparent);
    m_backend.target = target;
    m_renderer.RenderEmoticon(teer::vec2(teePosInTarget.x(), teePosInTarget.y()),
                              m_index, elapsed, 1.0f);
    // QPixmap implicit sharing: painting into backend.target may have detached
    // it from target, so read the painted pixmap back (same pattern as
    // TeeDrawer's `out = m_backend.target`).
    target = m_backend.target;
    return true;
}
