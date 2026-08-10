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
    m_atlas = atlas;
    m_backend.registerTexture(EMOTICON_TEX_ID, atlas);
    m_renderer.ConfigureEmoticonGrid(4, 4);   // emoticons.png is a 4×4 grid
    return true;
}

void EmoticonWindow::showEmoticon(const QString &key, int index, float teeSize,
                                  const QPointF &teePos)
{
    if (index < 0 || index >= teer::NUM_EMOTICONS)
        return;
    Active a;
    a.index = index;
    a.teeSize = teeSize;
    a.teePos = teePos;
    a.clock.start();
    m_active.insert(key, a);
    if (!m_frameTimer.isActive())
        m_frameTimer.start();
    emit frameChanged();
}

void EmoticonWindow::showEmoticon(int index, float teeSize, const QPointF &teePos)
{
    showEmoticon(QString(), index, teeSize, teePos);
}

void EmoticonWindow::hideEmoticon(const QString &key)
{
    if (m_active.remove(key) != 0)
        emit frameChanged();
}

void EmoticonWindow::hideEmoticon()
{
    if (m_active.isEmpty())
        return;
    m_active.clear();
    m_frameTimer.stop();
    emit frameChanged();
}

void EmoticonWindow::updateFrame()
{
    bool changed = false;
    for (auto it = m_active.begin(); it != m_active.end();) {
        if (it->clock.elapsed() / 1000.0f >= 2.0f) {   // 2-second lifetime
            it = m_active.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (m_active.isEmpty()) {
        m_frameTimer.stop();
        if (changed)
            emit frameChanged();
        return;
    }
    emit frameChanged();
}

bool EmoticonWindow::renderFrame(QPixmap &target, const QString &key,
                                 const QPointF &teePosInTarget, float teeSize)
{
    const auto it = m_active.constFind(key);
    if (it == m_active.constEnd())
        return false;
    const float elapsed = it->clock.elapsed() / 1000.0f;
    if (elapsed >= 2.0f) {
        hideEmoticon(key);
        return false;
    }

    // Render the current animation frame via the pipeline, sized to the
    // CURRENT tee size (tracks live zoom changes so the bubble never exceeds
    // the host's buffer computed from the same size).
    target.fill(Qt::transparent);
    m_backend.target = target;
    m_renderer.SetTeeSize(teeSize);
    m_renderer.RenderEmoticon(teer::vec2(teePosInTarget.x(), teePosInTarget.y()),
                              it->index, elapsed, 1.0f);
    // QPixmap implicit sharing: painting into backend.target may have detached
    // it from target, so read the painted pixmap back (same pattern as
    // TeeDrawer's `out = m_backend.target`).
    target = m_backend.target;
    return true;
}

bool EmoticonWindow::renderFrame(QPixmap &target, const QString &key,
                                 const QPointF &teePosInTarget)
{
    // 无 teeSize 重载：用播放开始时的初始尺寸（旧调用兼容）
    const auto it = m_active.constFind(key);
    if (it == m_active.constEnd())
        return false;
    return renderFrame(target, key, teePosInTarget, it->teeSize);
}

bool EmoticonWindow::renderFrame(QPixmap &target, const QPointF &teePosInTarget)
{
    return renderFrame(target, QString(), teePosInTarget);
}
