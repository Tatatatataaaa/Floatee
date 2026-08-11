#ifndef EMOTICONWINDOW_H
#define EMOTICONWINDOW_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QPixmap>
#include <QHash>
#include "core/tee_qt_backend.h"
#include "core/teedrawer.h"
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
 *
 * M4 扩展：多 Tee 并行表情 —— 本地 + 每个远端 Tee 可同时播放各自的表情
 * 动画，用 key（远端 roleId；本地 Tee 用空字符串）区分。每个表情 2 秒
 * 生命周期，独立时钟。
 */
class EmoticonWindow : public QObject
{
    Q_OBJECT
public:
    explicit EmoticonWindow(QObject *parent = nullptr);

    // Register the emoticon atlas (emoticons.png, a 4×4 grid) as texture id 2.
    bool loadAtlas(const QPixmap &atlas);
    // 暴露图集（供表情圆盘绘制图标用）
    const QPixmap &atlas() const { return m_atlas; }

    // Trigger a 2-second emoticon animation for the given tee.
    //   key    : remote roleId; local tee uses the empty string
    //   index  : 0..NUM_EMOTICONS-1 (see teer::EEmoticonSprite)
    //   teeSize: current tee render size (for SetTeeSize scaling)
    //   teePos : the tee's TeePos in the square tee-canvas coordinates
    void showEmoticon(const QString &key, int index, float teeSize, const QPointF &teePos);
    void showEmoticon(int index, float teeSize, const QPointF &teePos);   // local tee
    void hideEmoticon(const QString &key);
    void hideEmoticon();
    bool isActive(const QString &key) const { return m_active.contains(key); }
    bool isActive() const { return isActive(QString()); }
    bool hasAny() const { return !m_active.isEmpty(); }
    QList<QString> activeKeys() const { return m_active.keys(); }

    // Render the current animation frame of `key` into target (sized by the
    // host to its window). teePosInTarget = the tee's TeePos in the host
    // window's coordinates. Returns false when key has no active emoticon.
    // teeSize = 当前 tee 渲染尺寸：表情尺寸跟随实时缩放，保证与宿主按当前
    // teeSize 计算的渲染缓冲一致（播放中缩小 Tee 不裁断）。
    bool renderFrame(QPixmap &target, const QString &key, const QPointF &teePosInTarget,
                     float teeSize);
    bool renderFrame(QPixmap &target, const QString &key, const QPointF &teePosInTarget);
    bool renderFrame(QPixmap &target, const QPointF &teePosInTarget);     // local

    // 表情气泡边缘羽化（与 Tee 共用同一 Feather 托盘菜单）：
    // 0=Off 1=Normal 2=Strong，渲染每帧气泡后应用 TeeDrawer::featherAlpha。
    void setFeatherStrength(int strength) { m_featherStrength = qBound(0, strength, 2); }
    int featherStrength() const { return m_featherStrength; }

signals:
    void frameChanged();   // host should repaint (new animation frame / ended)

private slots:
    void updateFrame();

private:
    struct Active {
        int index = -1;
        float teeSize = 64.0f;
        QPointF teePos;    // TeePos in the square tee-canvas coordinates
        QElapsedTimer clock;
    };
    static constexpr uint32_t EMOTICON_TEX_ID = 2;

    QPixmapBackend m_backend;
    QPixmap m_atlas;                       // 表情图集副本（供圆盘裁图标）
    teer::CEmoticonRenderer m_renderer{&m_backend, teer::STextureHandle(EMOTICON_TEX_ID)};
    QTimer m_frameTimer;
    QHash<QString, Active> m_active;   // key -> active emoticon
    int m_featherStrength = 1;             // 与 TeeDrawer 默认一致（Normal）
};

#endif // EMOTICONWINDOW_H
