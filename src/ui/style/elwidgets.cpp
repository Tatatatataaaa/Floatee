#include "elwidgets.h"
#include "theme.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

// ═════════════════════════ ElCard ═════════════════════════

ElCard::ElCard(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ElCard"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);
    m_content = new QVBoxLayout;   // 无父级：由 outer 接管
    m_content->setContentsMargins(12, 10, 12, 10);
    m_content->setSpacing(8);
    if (!title.isEmpty()) {
        m_titleLabel = new QLabel(title, this);
        QFont f = m_titleLabel->font();
        f.setWeight(QFont::DemiBold);
        m_titleLabel->setFont(f);
        m_content->addWidget(m_titleLabel);
    }
    outer->addLayout(m_content);
    connect(Theme::instance(), &Theme::themeChanged, this,
            qOverload<>(&QWidget::update));
}

void ElCard::setTitle(const QString &title)
{
    if (!m_titleLabel) {
        m_titleLabel = new QLabel(this);
        QFont f = m_titleLabel->font();
        f.setWeight(QFont::DemiBold);
        m_titleLabel->setFont(f);
        m_content->insertWidget(0, m_titleLabel);
    }
    m_titleLabel->setText(title);
    m_titleLabel->setVisible(!title.isEmpty());
}

void ElCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Theme::cardBorder(), 1));
    p.setBrush(Theme::cardBackground());
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), Theme::radiusCard(),
                      Theme::radiusCard());
}

// ═════════════════════════ ElButton ═════════════════════════

ElButton::ElButton(const QString &text, Kind kind, QWidget *parent)
    : QPushButton(text, parent), m_kind(kind)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(30);
    connect(Theme::instance(), &Theme::themeChanged, this,
            qOverload<>(&QWidget::update));
}

void ElButton::setKind(Kind kind)
{
    if (m_kind == kind) return;
    m_kind = kind;
    update();
}

void ElButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int r = Theme::radiusButton() + 3; // 视觉上与 QSS 按钮(6px)接近
    QRectF rc = rect().adjusted(1, 1, -1, -1);
    QColor bg, fg, border;
    const bool dark = Theme::currentMode() == Theme::Mode::Dark;
    if (m_kind == Kind::Primary) {
        bg = Theme::accent();
        fg = Qt::white;
        border = Theme::accent().darker(110);
        if (isDown())            bg = bg.darker(115);
        else if (underMouse())   bg = bg.lighter(110);
    } else {
        // Ela 中性按钮配色
        bg  = dark ? QColor(0x3E, 0x3E, 0x3E) : QColor(0xFE, 0xFE, 0xFE);
        fg  = dark ? Qt::white : Qt::black;
        border = Theme::cardBorder();
        if (isDown())            bg = dark ? QColor(0x1C, 0x1C, 0x1C) : QColor(0xF2, 0xF2, 0xF2);
        else if (underMouse())   bg = dark ? QColor(0x4F, 0x4F, 0x4F) : QColor(0xF6, 0xF6, 0xF6);
    }
    if (!isEnabled()) {
        bg = dark ? QColor(0x32, 0x32, 0x36) : QColor(0xEE, 0xEE, 0xF0);
        fg = dark ? QColor(0x9A, 0x9A, 0xA4) : QColor(0x9A, 0x9A, 0xA4);
        border = Theme::cardBorder();
    }
    p.setPen(QPen(border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(rc, r, r);
    p.setPen(fg);
    p.setFont(font());
    p.drawText(rect(), Qt::AlignCenter, text());
}

// ═════════════════════════ ElDialog ═════════════════════════

ElDialog::ElDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ElDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(420, 240);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 16);
    root->setSpacing(10);

    // 标题栏（可拖动）
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(32);
    m_titleBar->setCursor(Qt::SizeAllCursor);
    m_titleBar->installEventFilter(this);
    auto *tbLay = new QHBoxLayout(m_titleBar);
    tbLay->setContentsMargins(4, 0, 0, 0);
    tbLay->setSpacing(0);
    m_titleLabel = new QLabel(title, m_titleBar);
    QFont f = m_titleLabel->font();
    f.setWeight(QFont::DemiBold);
    f.setPointSizeF(f.pointSizeF() * 1.1);
    m_titleLabel->setFont(f);
    tbLay->addWidget(m_titleLabel);
    tbLay->addStretch();
    m_closeBtn = new QToolButton(m_titleBar);
    m_closeBtn->setObjectName(QStringLiteral("TitleCloseBtn"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("关闭"));
    connect(m_closeBtn, &QToolButton::clicked, this, &ElDialog::reject);
    tbLay->addWidget(m_closeBtn);
    root->addWidget(m_titleBar);

    // 内容区
    m_content = new QVBoxLayout;
    m_content->setContentsMargins(4, 0, 4, 0);
    m_content->setSpacing(8);
    root->addLayout(m_content);

    // 阴影
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(Theme::shadowColor());
    setGraphicsEffect(shadow);

    connect(Theme::instance(), &Theme::themeChanged, this, &ElDialog::onThemeChanged);
}

void ElDialog::setContent(QWidget *w)
{
    m_content->addWidget(w);
}

void ElDialog::onThemeChanged()
{
    if (auto *eff = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect()))
        eff->setColor(Theme::shadowColor());
    update();
}

void ElDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // 窗口渐变底（Ela 风格）
    QLinearGradient g(0, 0, width(), height());
    g.setColorAt(0, Theme::windowGradientTop());
    g.setColorAt(1, Theme::windowGradientBottom());
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(8, 8, -8, -8), 8, 8);
    p.fillPath(path, g);
    p.setPen(QPen(Theme::cardBorder(), 1));
    p.drawPath(path);
}

bool ElDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
            }
            break;
        }
        case QEvent::MouseMove: {
            auto *e = static_cast<QMouseEvent *>(event);
            if (m_dragging)
                move(e->globalPosition().toPoint() - m_dragOffset);
            break;
        }
        case QEvent::MouseButtonRelease:
            m_dragging = false;
            break;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ElDialog::mousePressEvent(QMouseEvent *e)      { QDialog::mousePressEvent(e); }
void ElDialog::mouseMoveEvent(QMouseEvent *e)       { QDialog::mouseMoveEvent(e); }
void ElDialog::mouseReleaseEvent(QMouseEvent *e)    { QDialog::mouseReleaseEvent(e); }
