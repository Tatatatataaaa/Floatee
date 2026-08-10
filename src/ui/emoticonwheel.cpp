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
    m_closeTimer.setSingleShot(true);
    connect(&m_closeTimer, &QTimer::timeout, this, &EmoticonWheel::startClose);
}

void EmoticonWheel::open(const QPointF &center)
{
    m_open = true;
    m_closing = false;
    m_center = center;
    m_mouse = center;
    updateSelection();
    // 启动弹出回弹动画 + 5s 空闲超时
    m_animClock.start();
    m_animTimer.start();
    m_closeTimer.start(kIdleTimeoutMs);
    emit frameChanged();
}

void EmoticonWheel::close()
{
    m_open = false;
    m_closing = false;
    m_animTimer.stop();
    m_closeTimer.stop();
}

void EmoticonWheel::startClose()
{
    if (!m_open || m_closing)
        return;
    m_closing = true;
    m_closeClock.start();
    m_animTimer.start();   // 驱动收回动画重绘
    m_closeTimer.stop();
    emit frameChanged();
}

void EmoticonWheel::onAnimTick()
{
    if (m_closing) {
        // 收回动画完成 → 真正关闭
        if (m_closeClock.elapsed() >= kCloseDurMs) {
            close();
            return;
        }
        emit frameChanged();
        return;
    }
    if (!m_open)
        return;
    // 全部 item 动画完成（表情起始 + 最大延迟 + 时长）后停止
    const qint64 total = kItemStartMs + kMaxCount * kItemDelayMs + kItemDurMs;
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

    // 收回动画：统一反向系数（scale/alpha 衰减到 0，easeIn）
    double closeRev = 1.0;
    if (m_closing) {
        const double t = qMin(1.0, double(m_closeClock.elapsed()) / double(kCloseDurMs));
        closeRev = 1.0 - t * t;
    }

    // ── 背景：外环 + 内环分隔（半透明），弹出回弹（先于表情）──
    // 背景圆半径从 0 弹到 ~108% 再回落 100%，带淡入；完成后表情再逐个弹出。
    const double bgReveal = easeOutBack(double(now) / double(kBgDurMs), 1.08) * closeRev;
    const double bgAlpha = qMin(1.0, double(now) / 120.0) * closeRev;
    const double outerR = kOuterBgR * scl * bgReveal;
    const double innerR = kInnerR * scl * bgReveal;
    if (outerR > 0.5) {
        QPainterPath path;
        path.addEllipse(c, outerR, outerR);
        p.fillPath(path, QColor(30, 30, 40, qRound(180 * bgAlpha)));
        if (innerR > 0.5) {
            QPainterPath inner;
            inner.addEllipse(c, innerR, innerR);
            p.fillPath(inner, QColor(60, 60, 80, qRound(200 * bgAlpha)));
        }
    }

    // ── 中心取消（随背景弹出/收回）──
    p.setOpacity(bgAlpha);
    p.setPen(QPen(QColor(255, 255, 255, 220), 4.0 * scl));
    const double s = 14.0 * scl;
    p.drawLine(c + QPointF(-s, -s), c + QPointF(s, s));
    p.drawLine(c + QPointF(-s, s), c + QPointF(s, -s));
    p.setOpacity(1.0);

    // ── 外环：16 个表情（从 emoticons.png 4×4 裁取，弹出回弹动画）──
    if (!emoticonAtlas.isNull()) {
        const double cell = double(emoticonAtlas.width()) / 4.0;
        for (int i = 0; i < NUM_EMOTICONS; ++i) {
            const double angle = 2.0 * M_PI * i / NUM_EMOTICONS - M_PI / 2.0; // 从顶部开始
            const QPointF pos = c + QPointF(std::cos(angle), std::sin(angle)) * (kOuterItemR * scl);
            const bool hover = (i == m_selEmoticon);
            // 弹出回弹：先放大到 ~110% 再回落 100%；item 错开 stagger 延迟（背景未完即开始）
            const double reveal = itemReveal(now, kItemStartMs + i * kItemDelayMs) * closeRev;
            if (reveal <= 0.001)
                continue;
            const double size = (hover ? 74.0 : 52.0) * scl * reveal;
            const double alpha = qMin(1.0, double(qMax<qint64>(0, now - (kItemStartMs + i * kItemDelayMs))) / 80.0) * closeRev;
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
            // 眼睛也参与弹出动画（表情加载一半时开始），尺寸/淡入同表情
            const double reveal = itemReveal(now, kEyeStartMs + i * kItemDelayMs) * closeRev;
            if (reveal <= 0.001)
                continue;
            const double size = (hover ? 56.0 : 40.0) * scl * reveal;
            const double alpha = qMin(1.0, double(qMax<qint64>(0, now - (kEyeStartMs + i * kItemDelayMs))) / 80.0) * closeRev;
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
