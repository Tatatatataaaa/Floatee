#ifndef EMOTICONWHEEL_H
#define EMOTICONWHEEL_H

#include <QObject>
#include <QPixmap>
#include <QPointF>

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

    // 以屏幕坐标 center 打开 / 关闭
    void open(const QPointF &center) { m_open = true; m_center = center; m_mouse = center; updateSelection(); }
    void close() { m_open = false; }
    bool isOpen() const { return m_open; }
    const QPointF &center() const { return m_center; }

    // 鼠标悬停（屏幕坐标）→ 更新高亮
    void setMousePos(const QPointF &g) { if (m_open) { m_mouse = g; updateSelection(); } }
    // 点击提交（屏幕坐标）→ 命中结果
    Result submitAt(const QPointF &g) const;

    int selectedEmoticon() const { return m_selEmoticon; }
    int selectedEye() const { return m_selEye; }

    // 渲染：emoticonAtlas=表情图集(4×4 网格)，skinAtlas=皮肤图集(256×256)
    void paint(QPainter &p, const QPixmap &emoticonAtlas, const QPixmap &skinAtlas) const;

    // 眼睛图标在皮肤图集中的 X 坐标（256×256 网格，y=96，每格 32×32）。
    // 顺序：NORMAL, HAPPY, ANGRY, PAIN, SURPRISE, BLINK（BLINK=NORMAL 压扁）
    static constexpr int kEyeRegionX[6] = { 64, 160, 96, 128, 224, 64 };
    static constexpr int kEyeRegionY = 96;
    static constexpr int kEyeRegionSize = 32;

private:
    void updateSelection();
    Result hitTest(const QPointF &g) const;

    bool m_open = false;
    QPointF m_center;
    QPointF m_mouse;
    int m_selEmoticon = -1;
    int m_selEye = -1;

    // 半径参数（屏幕像素）
    static constexpr double kCancelR = 40.0;
    static constexpr double kInnerR = 110.0;      // 内环（眼睛）外边界
    static constexpr double kEyeR = 78.0;         // 眼睛项圆心半径
    static constexpr double kOuterItemR = 150.0;  // 表情项圆心半径
    static constexpr double kOuterBgR = 190.0;    // 外背景圆半径
};

#endif // EMOTICONWHEEL_H
