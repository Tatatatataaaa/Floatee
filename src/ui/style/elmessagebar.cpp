#include "elmessagebar.h"
#include "theme.h"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

QList<QPair<ElMessageBar::Position, ElMessageBar *>> ElMessageBar::s_active;

// ── 静态接口 ──────────────────────────────────────────────

void ElMessageBar::success(Position pos, const QString &title, const QString &text,
                           int displayMsec, QWidget *parent)
{
    new ElMessageBar(pos, Mode::Success, title, text, displayMsec, parent);
}

void ElMessageBar::warning(Position pos, const QString &title, const QString &text,
                           int displayMsec, QWidget *parent)
{
    new ElMessageBar(pos, Mode::Warning, title, text, displayMsec, parent);
}

void ElMessageBar::information(Position pos, const QString &title, const QString &text,
                               int displayMsec, QWidget *parent)
{
    new ElMessageBar(pos, Mode::Information, title, text, displayMsec, parent);
}

void ElMessageBar::error(Position pos, const QString &title, const QString &text,
                         int displayMsec, QWidget *parent)
{
    new ElMessageBar(pos, Mode::Error, title, text, displayMsec, parent);
}

// ── 堆叠管理 ──────────────────────────────────────────────

QList<ElMessageBar *> ElMessageBar::activeBars(Position pos)
{
    QList<ElMessageBar *> out;
    for (const auto &pair : s_active)
        if (pair.first == pos && !pair.second->m_closing)
            out.append(pair.second);
    return out;
}

int ElMessageBar::stackIndex() const
{
    int idx = 0;
    for (const auto &pair : s_active) {
        if (pair.first == m_pos && !pair.second->m_closing) {
            if (pair.second == this)
                return idx;
            ++idx;
        }
    }
    return idx;
}

void ElMessageBar::relayoutStack()
{
    const bool bottom = (m_pos == Position::BottomLeft || m_pos == Position::BottomRight
                         || m_pos == Position::Bottom);
    int y = 0;
    const QList<ElMessageBar *> bars = activeBars(m_pos);
    for (ElMessageBar *bar : bars) {
        QPoint target = bar->anchorPos();
        if (bottom)
            target.ry() -= y;   // 底部锚定：向上堆叠
        else
            target.ry() += y;   // 顶部锚定：向下堆叠
        // 平滑移动
        if (bar->pos() != target) {
            auto *anim = new QPropertyAnimation(bar, "pos", bar);
            anim->setDuration(180);
            anim->setStartValue(bar->pos());
            anim->setEndValue(target);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
        y += bar->height() + bar->m_spacing;
    }
}

QPoint ElMessageBar::anchorPos() const
{
    QWidget *host = parentWidget();
    if (!host) host = QApplication::activeWindow();
    if (!host) {
        // 无宿主：锚定主屏
        const QScreen *scr = QApplication::primaryScreen();
        return QPoint(scr->availableGeometry().width() - width() - m_margin, m_margin);
    }
    const QRect g = host->rect();
    switch (m_pos) {
    case Position::TopLeft:     return QPoint(m_margin, m_margin);
    case Position::Top:         return QPoint(g.width() / 2 - width() / 2, m_margin);
    case Position::BottomLeft:  return QPoint(m_margin, g.height() - m_barHeight - m_margin);
    case Position::BottomRight: return QPoint(g.width() - width() - m_margin,
                                              g.height() - m_barHeight - m_margin);
    case Position::Bottom:      return QPoint(g.width() / 2 - width() / 2,
                                              g.height() - m_barHeight - m_margin);
    case Position::TopRight:
    default:                    return QPoint(g.width() - width() - m_margin, m_margin);
    }
}

// ── 构造 / 生命周期 ───────────────────────────────────────

ElMessageBar::ElMessageBar(Position pos, Mode mode, const QString &title,
                           const QString &text, int displayMsec, QWidget *parent)
    : QWidget(parent), m_pos(pos), m_mode(mode), m_title(title), m_text(text)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground, false);
    setMouseTracking(true);

    // 尺寸：按文字估算宽度
    QFontMetrics fmT(font());
    const int textW = fmT.horizontalAdvance(m_text) + fmT.horizontalAdvance(m_title)
                      + 140 /*图标+边距+关闭*/;
    setFixedSize(qBound(240, textW, 480), m_barHeight);

    QWidget *host = parentWidget();
    if (!host) host = QApplication::activeWindow();
    if (host) {
        setParent(host);
        host->installEventFilter(this);
    }
    raise();
    show();
    s_active.append(qMakePair(pos, this));
    move(anchorPos());
    relayoutStack();

    // 入场：淡入 + 从边缘滑入
    auto *eff = new QGraphicsOpacityEffect(this);
    eff->setOpacity(0);
    setGraphicsEffect(eff);
    auto *fade = new QPropertyAnimation(eff, "opacity", this);
    fade->setDuration(200);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    startLifecycle(displayMsec);
}

ElMessageBar::~ElMessageBar()
{
    s_active.removeAll(qMakePair(m_pos, this));
}

void ElMessageBar::startLifecycle(int displayMsec)
{
    if (displayMsec > 0)
        QTimer::singleShot(displayMsec, this, &ElMessageBar::closeBar);
}

void ElMessageBar::closeBar()
{
    if (m_closing) return;
    m_closing = true;
    // 淡出后关闭并让堆叠补位
    auto *eff = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
    if (!eff) {
        eff = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(eff);
    }
    auto *fade = new QPropertyAnimation(eff, "opacity", this);
    fade->setDuration(220);
    fade->setStartValue(eff->opacity());
    fade->setEndValue(0.0);
    connect(fade, &QPropertyAnimation::finished, this, [this]() {
        emit barClosed(this);
        close(); // WA_DeleteOnClose → 析构 → s_active 移除
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
    // 立即让其余条补位（本条标记 closing 后不再计入）
    relayoutStack();
}

// ── 绘制 ──────────────────────────────────────────────────

QColor ElMessageBar::modeColor() const
{
    switch (m_mode) {
    case Mode::Success:      return QColor(0x2E, 0xC2, 0x7E);
    case Mode::Warning:      return QColor(0xF5, 0xA6, 0x23);
    case Mode::Error:        return QColor(0xE5, 0x48, 0x4D);
    case Mode::Information:
    default:                 return Theme::accent();
    }
}

QString ElMessageBar::modeGlyph() const
{
    switch (m_mode) {
    case Mode::Success:      return QStringLiteral("✓");
    case Mode::Warning:      return QStringLiteral("!");
    case Mode::Error:        return QStringLiteral("✕");
    case Mode::Information:
    default:                 return QStringLiteral("i");
    }
}

void ElMessageBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    // 手绘柔和阴影（Ela 做法：多层圆角递增 alpha）
    const QColor sc = Theme::shadowColor();
    for (int i = 0; i < 6; ++i) {
        QPainterPath sp;
        sp.addRoundedRect(rect().adjusted(6 - i, 6 - i, -(6 - i), -(6 - i)), 6 + i, 6 + i);
        QColor c = sc;
        c.setAlpha(qMin(255, 7 * (6 - i + 1)));
        p.setPen(QPen(c, 1));
        p.drawPath(sp);
    }

    // 主体卡片
    QPainterPath body;
    body.addRoundedRect(rect().adjusted(6, 6, -6, -6), 6, 6);
    p.fillPath(body, Theme::cardBackground());
    p.setPen(QPen(Theme::cardBorder(), 1));
    p.drawPath(body);

    // 左侧模式色条
    const QColor mc = modeColor();
    QPainterPath stripe;
    stripe.addRoundedRect(QRectF(7, 7, 5, height() - 14), 2.5, 2.5);
    p.fillPath(stripe, mc);

    // 模式图标（圆形底 + 字符）
    p.setPen(Qt::NoPen);
    p.setBrush(mc);
    p.drawEllipse(QRectF(26, height() / 2 - 11, 22, 22));
    p.setPen(Qt::white);
    QFont iconFont = font();
    iconFont.setBold(true);
    iconFont.setPixelSize(13);
    p.setFont(iconFont);
    p.drawText(QRect(26, height() / 2 - 11, 22, 22), Qt::AlignCenter, modeGlyph());

    // 标题（Bold）+ 正文（同行，Ela 布局）
    const int textX = 58;
    QFont tf = font();
    tf.setWeight(QFont::DemiBold);
    p.setFont(tf);
    p.setPen(Theme::textPrimary());
    int titleW = p.fontMetrics().horizontalAdvance(m_title);
    const int maxTitleW = 110;
    if (titleW > maxTitleW) titleW = maxTitleW;
    p.drawText(QRect(textX, 6, titleW, height() - 12),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, m_title);
    QFont bf = font();
    bf.setWeight(QFont::Normal);
    p.setFont(bf);
    p.setPen(Theme::textSecondary());
    p.drawText(QRect(textX + titleW + 10, 6, width() - textX - titleW - 10 - 34, height() - 12),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, m_text);

    // 关闭按钮（悬停可见）
    if (underMouse()) {
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::cardBorder());
        p.drawEllipse(QRectF(width() - 30, height() / 2 - 11, 22, 22));
    }
    p.setPen(Theme::textSecondary());
    p.setFont(iconFont);
    p.drawText(QRect(width() - 30, height() / 2 - 11, 22, 22), Qt::AlignCenter,
               QStringLiteral("✕"));
}

void ElMessageBar::mousePressEvent(QMouseEvent *event)
{
    // 点击关闭按钮区域 → 关闭；其余区域忽略
    const QRect closeRect(width() - 32, height() / 2 - 13, 26, 26);
    if (event->button() == Qt::LeftButton && closeRect.contains(event->pos())) {
        closeBar();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool ElMessageBar::eventFilter(QObject *watched, QEvent *event)
{
    // 宿主窗口移动/缩放时重新锚定
    if (watched == parentWidget()
        && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        relayoutStack();
    }
    return QWidget::eventFilter(watched, event);
}
