#ifndef FLOATEE_SETTINGSWINDOW_H
#define FLOATEE_SETTINGSWINDOW_H

#include <QWidget>

class QListWidget;
class QStackedWidget;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QToolButton;
class QLabel;

class Floatee;

// ── Step2 独立设置窗口（ElaWindow 风格：无边框圆角 + 左侧导航 + 页面栈）──
// 页面：常规 / 外观 / 联机 / 休眠 / 实例。隐藏复用（单例式 show/raise）。
// 所有设置项直接读写 Floatee 的 Setup 并复用其既有槽函数，键名不变。
class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    // 单例式访问：首次创建后隐藏复用（保留页面状态）
    static SettingsWindow *instance(Floatee *floatee);

    void showPage(int index);   // 0=常规 1=外观 2=联机 3=休眠 4=实例

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit SettingsWindow(Floatee *floatee);

    QWidget *buildGeneralPage();
    QWidget *buildAppearancePage();
    QWidget *buildNetworkPage();
    QWidget *buildSleepPage();
    QWidget *buildInstancePage();

    void refreshFromSetup();    // 重新从 Setup 同步控件状态（打开/切换配置后）
    void saveSetup();           // Setup → 磁盘

    Floatee *m_floatee;
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_titleBar = nullptr;
    QLabel *m_titleLabel = nullptr;
    QToolButton *m_closeBtn = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;

    // 常规页
    QComboBox *m_themeCombo = nullptr;
    QCheckBox *m_onTopCheck = nullptr;

    // 外观页
    QComboBox *m_skinCombo = nullptr;
    QComboBox *m_eyeCombo = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    QComboBox *m_featherCombo = nullptr;
    QComboBox *m_emoticonSetCombo = nullptr;

    // 联机页
    QLineEdit *m_serverEdit = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QLabel *m_mpStatus = nullptr;

    // 休眠页（自由输入：QLineEdit + QIntValidator，避免 SpinBox 上下按钮样式异常）
    QLineEdit *m_sleepTimeoutEdit = nullptr;
    QLineEdit *m_breakRemindEdit = nullptr;
    QLineEdit *m_resetAfterEdit = nullptr;

    // 实例页
    QListWidget *m_instanceList = nullptr;
};

#endif // FLOATEE_SETTINGSWINDOW_H
