#ifndef FLOATEE_ELMESSAGEBAR_H
#define FLOATEE_ELMESSAGEBAR_H

#include <QWidget>

// ── ElMessageBar：边缘弹出信息栏（参考 ElaMessageBar 视觉，纯自研）──
// success / warning / information / error 四种模式；
// 同一位置多条自动纵向堆叠，关闭后下方条上移补位。
class ElMessageBar : public QWidget
{
    Q_OBJECT
public:
    enum class Mode {
        Success,
        Warning,
        Information,
        Error,
    };
    enum class Position {
        TopRight,   // 推荐：右上角
        TopLeft,
        BottomRight,
        BottomLeft,
        Top,        // 顶部居中
        Bottom,     // 底部居中
    };

    // 静态快捷接口：displayMsec <= 0 表示常驻（手动关闭）
    static void success(Position pos, const QString &title, const QString &text,
                        int displayMsec = 3000, QWidget *parent = nullptr);
    static void warning(Position pos, const QString &title, const QString &text,
                        int displayMsec = 4000, QWidget *parent = nullptr);
    static void information(Position pos, const QString &title, const QString &text,
                            int displayMsec = 3000, QWidget *parent = nullptr);
    static void error(Position pos, const QString &title, const QString &text,
                      int displayMsec = 5000, QWidget *parent = nullptr);

    explicit ElMessageBar(Position pos, Mode mode, const QString &title,
                          const QString &text, int displayMsec, QWidget *parent = nullptr);
    ~ElMessageBar() override;

signals:
    void barClosed(ElMessageBar *bar);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void startLifecycle(int displayMsec);
    void closeBar();
    void relayoutStack();          // 重新排布同位置所有条
    int stackIndex() const;        // 本条在堆叠中的序号
    QColor modeColor() const;      // 模式主题色（图标+左边条）
    QString modeGlyph() const;     // 模式图标字符
    QPoint anchorPos() const;      // 根据策略计算落点

    Position m_pos;
    Mode m_mode;
    QString m_title;
    QString m_text;
    int m_margin = 12;             // 距边缘
    int m_spacing = 8;             // 条间距
    int m_barHeight = 56;
    bool m_closing = false;

    static QList<ElMessageBar *> activeBars(Position pos);
    static QList<QPair<Position, ElMessageBar *>> s_active; // 全部活跃条
};

#endif // FLOATEE_ELMESSAGEBAR_H
