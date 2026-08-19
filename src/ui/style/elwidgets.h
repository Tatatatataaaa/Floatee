#ifndef FLOATEE_ELWIDGETS_H
#define FLOATEE_ELWIDGETS_H

#include <QDialog>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;

// ── ElCard：圆角卡片容器（ElaScrollPageArea 风格：圆角 6 + 描边 + 卡片底色）──
// 用法：new ElCard(parent) → setLayout() 放入内容；标题可选。
class ElCard : public QWidget
{
    Q_OBJECT
public:
    explicit ElCard(const QString &title = QString(), QWidget *parent = nullptr);

    // 内容布局（卡片内边距已设置好，直接 addWidget）
    QVBoxLayout *contentLayout() const { return m_content; }
    void setTitle(const QString &title);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVBoxLayout *m_content = nullptr;
    QLabel *m_titleLabel = nullptr;
};

// ── ElButton：主题色按钮（ElaPushButton 风格 + Floatee 主题蓝）──
class ElButton : public QPushButton
{
    Q_OBJECT
public:
    enum class Kind {
        Primary,   // 主题色填充
        Standard,  // 中性底
    };
    explicit ElButton(const QString &text = QString(), Kind kind = Kind::Standard,
                      QWidget *parent = nullptr);
    void setKind(Kind kind);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Kind m_kind = Kind::Standard;
};

// ── ElDialog：无边框圆角对话框（可拖动标题栏 + 关闭按钮 + 阴影）──
// 用法：继承或实例化后 setContent(widget)；标题栏自动拖动移动窗口。
class ElDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ElDialog(const QString &title, QWidget *parent = nullptr);

    // 内容区布局（标题栏下方，内边距已设置）
    QVBoxLayout *contentLayout() const { return m_content; }
    void setContent(QWidget *w);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QVBoxLayout *m_content = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // FLOATEE_ELWIDGETS_H
