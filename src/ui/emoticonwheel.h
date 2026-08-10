#ifndef EMOTICONWHEEL_H
#define EMOTICONWHEEL_H

#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QElapsedTimer>
#include <QTimer>

/**
 * M4 表情圆盘：全屏画布内的环形选择器（DDNet CEmoticon 方案）。
 * 外环 16 个头顶表情 + 内环 6 个眼睛 + 中心取消。
 * 纯 UI overlay —— 由 Floatee 的 paintEvent 渲染、鼠标事件驱动，
 * 不开第二窗口（本机第二个图层窗口不会被 DWM 合成）。
 *
 * 半径分层（与参考一致）：
 *   ≤ 40   中心 → 取消
 *   40~110 内环 → 6 个眼睛
 *   > 110  外环 → 16 个表情
 * 选择公式：Index = round(angle / (2π) * Count) mod Count
 */
class EmoticonWheel : public QObject
{
    Q_OBJECT
public:
    enum class Hit { None, Cancel, Eye, Emoticon };
    struct Result { Hit hit = Hit::None; int index = -1; };

    explicit EmoticonWheel(QObject *parent = nullptr);

    // 以屏幕坐标 center 打开 / 关闭；setScale 先设（非全屏小窗口缩小圆盘）
    void open(const QPointF &center);
    void close();
    bool isOpen() const { return m_open; }
    const QPointF &center() const { return m_center; }
    // 整体缩放（0.4~2.0）：全屏=1.0，非全屏跟随 Tee 缩放（基础×SizeScale）
    void setScale(double s) { m_scale = qBound(0.4, s, 2.0); }
    double scale() const { return m_scale; }

    // 鼠标悬停（屏幕坐标）→ 更新高亮
    void setMousePos(const QPointF &g) { if (m_open) { m_mouse = g; updateSelection(); } }
    // 点击提交（屏幕坐标）→ 命中结果
    Result submitAt(const QPointF &g) const;

    int selectedEmoticon() const { return m_selEmoticon; }
    int selectedEye() const { return m_selEye; }

    // 渲染：emoticonAtlas=表情图集(4×4 网格)，skinAtlas=皮肤图集(256×256)
    void paint(QPainter &p, const QPixmap &emoticonAtlas, const QPixmap &skinAtlas) const;

signals:
    void frameChanged();   // 弹出动画进行中，host 应 repaint

private slots:
    void onAnimTick();

private:
    void updateSelection();
    Result hitTest(const QPointF &g) const;
    // 弹出回弹曲线：easeOutBack（0→~1.1→1），item 错开 stagger 延迟
    static double easeOutBack(double t, double overshoot = 1.1);
    // 单个 item 的 reveal 系数（含淡入）；delayMs=该 item 延迟，at 为当前
    // elapsed（ms）；返回 0=不可见，~1.1=峰值超调，1=稳定
    double itemReveal(qint64 at, qint64 delayMs) const;

    // 眼睛图标在皮肤图集中的参考图 X 坐标（256×128 参考图，y=96，每格 32×32），
    // paint 时按实际皮肤尺寸缩放。顺序：NORMAL, HAPPY, ANGRY, PAIN, SURPRISE, BLINK
    // （BLINK=NORMAL 压扁）
    static constexpr int kEyeRegionX[6] = { 64, 160, 96, 128, 224, 64 };
    static constexpr int kEyeRegionY = 96;
    static constexpr int kEyeRegionSize = 32;

    bool m_open = false;
    QPointF m_center;
    QPointF m_mouse;
    int m_selEmoticon = -1;
    int m_selEye = -1;
    double m_scale = 1.0;

    // 弹出动画：打开时启动，每 item 错开延迟 + 回弹缩放
    QElapsedTimer m_animClock;
    QTimer m_animTimer;
    static constexpr qint64 kBgDurMs = 200;       // 背景圆弹出时长（先于表情）
    static constexpr qint64 kItemDelayMs = 24;    // 相邻 item 错开
    static constexpr qint64 kItemDurMs = 320;     // 单个 item 动画时长
    static constexpr qint64 kMaxCount = 16;       // 外环表情数（延迟最大档）

    // 半径参数（屏幕像素）
    static constexpr double kCancelR = 40.0;
    static constexpr double kInnerR = 110.0;      // 内环（眼睛）外边界
    static constexpr double kEyeR = 78.0;         // 眼睛项圆心半径
    static constexpr double kOuterItemR = 150.0;  // 表情项圆心半径
    static constexpr double kOuterBgR = 190.0;    // 外背景圆半径
};

#endif // EMOTICONWHEEL_H
