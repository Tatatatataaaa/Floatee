#ifndef FLOATEE_THEME_H
#define FLOATEE_THEME_H

#include <QColor>
#include <QObject>

// ── Floatee 设计系统主题单例（参考 ElaWidgetTools 视觉语言，纯自研）──
// 深浅主题 + 主题色 + 圆角/间距体系；持久化到 default.json["Theme"]。
// Theme 只作用于控件 QSS 与自绘 UI；Tee 渲染层（TeeDrawer）不依赖主题。
class Theme : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        Light = 0,
        Dark = 1,
    };
    Q_ENUM(Mode)

    // 持久化取值："auto" / "light" / "dark"（默认 auto：跟随系统）
    static Theme *instance();

    static Mode currentMode();                       // 解析后的实际模式
    static QString savedValue();                     // 原始持久化值
    static void setSavedValue(const QString &v);     // "auto"/"light"/"dark"，立即应用并持久化

    // ── 设计令牌（浅/深两套，源自 Ela 源码实测配色）──
    static QColor windowGradientTop();     // 窗口渐变起点
    static QColor windowGradientBottom();  // 窗口渐变终点
    static QColor cardBackground();        // 卡片底色
    static QColor cardBorder();            // 卡片描边
    static QColor textPrimary();           // 主文字
    static QColor textSecondary();         // 次要文字
    static QColor accent();                // 主题色（Fluent 蓝）
    static QColor shadowColor();           // 阴影色

    static int radiusCard();   // 卡片圆角 6px
    static int radiusButton(); // 按钮圆角 3px
    static int spacing();      // 标准间距 12px

    // 全局 Fluent 风格样式表（浅/深两套，替换现有 qApp->setStyleSheet）
    static QString styleSheet();

    // 持久化钩子（由 Floatee 在加载/保存 Setup 时调用，避免 Theme 依赖 JsonOpt 路径）
    static void loadFromJson(const class QJsonObject &setup);
    static void saveToJson(class QJsonObject &setup);

signals:
    void themeChanged(Mode mode);

private:
    explicit Theme(QObject *parent = nullptr);
    void apply();

    static Mode resolveMode();            // auto → 跟随系统深浅
    static bool systemDarkHint();

    QString m_saved = QStringLiteral("auto");
    Mode m_mode = Mode::Light;
};

#endif // FLOATEE_THEME_H
