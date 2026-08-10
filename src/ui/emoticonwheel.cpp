#include "emoticonwheel.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
constexpr int NUM_EMOTICONS = 16;
constexpr int NUM_EYES = 6;

// PositiveMod: non-negative fmod
double positiveMod(double x, double y)
{
    return std::fmod(x + y, y);
}

int indexFromAngle(double angle, int count)
{
    // 归一到 [0, 2π) 后按扇区四舍五入
    double a = positiveMod(angle, 2.0 * M_PI);
    return int(std::round(a / (2.0 * M_PI) * count)) % count;
}
} // namespace

EmoticonWheel::EmoticonWheel(QObject *parent)
    : QObject(parent)
{
    m_animTimer.setInterval(16);
    connect(&m_animTimer, &QTimer::timeout, this, &EmoticonWheel::onAnimTick);
}

void EmoticonWheel::open(const QPointF &center)
{
    m_open = true;
    m_center = center;
    m_mouse = center;
    updateSelection();
    // 启动弹出回弹动画
    m_animClock.start();
    m_animTimer.start();
    emit frameChanged();
}

void EmoticonWheel::close()
{
    m_open = false;
    m_animTimer.stop();
}

void EmoticonWheel::onAnimTick()
{
    if (!m_open)
        return;
    // 全部 item 动画完成（最大延迟 + 时长）后停止
    const qint64 total = kMaxCount * kItemDelayMs + kItemDurMs;
    if (m_animClock.elapsed() >= total) {
        m_animTimer.stop();
        return;
    }
    emit frameChanged();
}

// easeOutBack：0→峰值(overshoot≈1.1)→1，带回弹超调
double EmoticonWheel::easeOutBack(double t, double overshoot)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    const double c1 = overshoot * 1.70158 / 1.1;   // 峰值≈overshoot
    const double c3 = c1 + 1.0;
    const double u = t - 1.0;
    return 1.0 + c3 * u * u * u + c1 * u * u;
}

double EmoticonWheel::itemReveal(qint64 at, qint64 delayMs) const
{
    const qint64 local = at - delayMs;
    if (local < 0)
        return 0.0;
    return easeOutBack(double(local) / double(kItemDurMs), 1.1);
}

void EmoticonWheel::updateSelection()
{
    m_selEmoticon = -1;
    m_selEye = -1;
    if (!m_open)
        return;
    const QPointF d = m_mouse - m_center;
    const double dist = std::hypot(d.x(), d.y());
    if (dist <= kCancelR * m_scale)
        return;                                   // 中心：取消
    // 命中角度与渲染基准对齐：渲染 item i 在 2π*i/N - π/2（顶部起始），
    // 故鼠标角度也加 π/2 使 0 扇区对准顶部（否则差 90°）
    const double angle = std::atan2(d.y(), d.x()) + M_PI / 2.0;
    if (dist <= kInnerR * m_scale)
        m_selEye = indexFromAngle(angle, NUM_EYES);
    else
        m_selEmoticon = indexFromAngle(angle, NUM_EMOTICONS);
}

EmoticonWheel::Result EmoticonWheel::submitAt(const QPointF &g) const
{
    return hitTest(g);
}

EmoticonWheel::Result EmoticonWheel::hitTest(const QPointF &g) const
{
    Result r;
    const QPointF d = g - m_center;
    const double dist = std::hypot(d.x(), d.y());
    if (dist <= kCancelR * m_scale) {
        r.hit = Hit::Cancel;
        return r;
    }
    // 与渲染基准对齐（顶部为 0 扇区）
    const double angle = std::atan2(d.y(), d.x()) + M_PI / 2.0;
    if (dist <= kInnerR * m_scale) {
        r.hit = Hit::Eye;
        r.index = indexFromAngle(angle, NUM_EYES);
    } else if (dist <= kOuterBgR * m_scale) {
        r.hit = Hit::Emoticon;
        r.index = indexFromAngle(angle, NUM_EMOTICONS);
    }
    return r;
}

void EmoticonWheel::paint(QPainter &p, const QPixmap &emoticonAtlas,
                          const QPixmap &skinAtlas) const
{
    if (!m_open)
        return;
    const QPointF c = m_center;
    const double scl = m_scale;
    const qint64 now = m_animClock.isValid() ? m_animClock.elapsed() : 0;

    // ── 背景：外环 + 内环分隔（半透明）──
    QPainterPath path;
    path.addEllipse(c, kOuterBgR * scl, kOuterBgR * scl);
    p.fillPath(path, QColor(30, 30, 40, 180));
    QPainterPath inner;
    inner.addEllipse(c, kInnerR * scl, kInnerR * scl);
    p.fillPath(inner, QColor(60, 60, 80, 200));

    // ── 中心取消 ──
    p.setPen(QPen(QColor(255, 255, 255, 220), 4.0 * scl));
    const double s = 14.0 * scl;
    p.drawLine(c + QPointF(-s, -s), c + QPointF(s, s));
    p.drawLine(c + QPointF(-s, s), c + QPointF(s, -s));

    // ── 外环：16 个表情（从 emoticons.png 4×4 裁取，弹出回弹动画）──
    if (!emoticonAtlas.isNull()) {
        const double cell = double(emoticonAtlas.width()) / 4.0;
        for (int i = 0; i < NUM_EMOTICONS; ++i) {
            const double angle = 2.0 * M_PI * i / NUM_EMOTICONS - M_PI / 2.0; // 从顶部开始
            const QPointF pos = c + QPointF(std::cos(angle), std::sin(angle)) * (kOuterItemR * scl);
            const bool hover = (i == m_selEmoticon);
            // 弹出回弹：先放大到 ~110% 再回落 100%；item 错开 stagger 延迟
            const double reveal = itemReveal(now, i * kItemDelayMs);
            if (reveal <= 0.001)
                continue;
            const double size = (hover ? 74.0 : 52.0) * scl * reveal;
            const double alpha = qMin(1.0, double(qMax<qint64>(0, now - i * kItemDelayMs)) / 80.0);
            p.setOpacity(alpha);
            const int col = i % 4, row = i / 4;
            const QRectF src(col * cell, row * cell, cell, cell);
            p.drawPixmap(QRectF(pos.x() - size / 2, pos.y() - size / 2, size, size),
                         emoticonAtlas, src);
        }
        p.setOpacity(1.0);
    }

    // ── 内环：6 个眼睛（从皮肤图集裁取；BLINK=NORMAL 压扁）──
    if (!skinAtlas.isNull()) {
        // 眼睛区域坐标按 256×128 参考图缩放（与 configureRegions 一致），
        // 兼容标准 256×128 与 4K(4096×2048) 皮肤
        const double sx = double(skinAtlas.width()) / 256.0;
        const double sy = double(skinAtlas.height()) / 128.0;
        for (int i = 0; i < NUM_EYES; ++i) {
            const double angle = 2.0 * M_PI * i / NUM_EYES - M_PI / 2.0;
            const QPointF pos = c + QPointF(std::cos(angle), std::sin(angle)) * (kEyeR * scl);
            const bool hover = (i == m_selEye);
            // 眼睛也参与弹出动画（错开在表情之后），尺寸/淡入同表情
            const double reveal = itemReveal(now, (NUM_EMOTICONS + i) * kItemDelayMs);
            if (reveal <= 0.001)
                continue;
            const double size = (hover ? 56.0 : 40.0) * scl * reveal;
            const double alpha = qMin(1.0, double(qMax<qint64>(0, now - (NUM_EMOTICONS + i) * kItemDelayMs)) / 80.0);
            p.setOpacity(alpha);
            const QRectF src(kEyeRegionX[i] * sx, kEyeRegionY * sy,
                             kEyeRegionSize * sx, kEyeRegionSize * sy);
            if (i == 5) {
                // BLINK：垂直压扁
                const QPixmap normal = skinAtlas.copy(src.toRect());
                const QPixmap blink = normal.scaled(qRound(size), qRound(size * 0.5),
                                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                p.drawPixmap(QPointF(pos.x() - blink.width() / 2.0,
                                     pos.y() - blink.height() / 2.0), blink);
            } else {
                p.drawPixmap(QRectF(pos.x() - size / 2, pos.y() - size / 2, size, size),
                             skinAtlas, src);
            }
        }
        p.setOpacity(1.0);
    }
}
