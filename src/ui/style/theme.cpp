#include "theme.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyleHints>

Theme::Theme(QObject *parent)
    : QObject(parent)
{
}

Theme *Theme::instance()
{
    static Theme inst;
    return &inst;
}

// ── 模式解析 ──────────────────────────────────────────────

bool Theme::systemDarkHint()
{
    // Qt 6.5+：查询系统颜色方案
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

Theme::Mode Theme::resolveMode()
{
    const QString v = instance()->m_saved;
    if (v == QLatin1String("light"))
        return Mode::Light;
    if (v == QLatin1String("dark"))
        return Mode::Dark;
    return systemDarkHint() ? Mode::Dark : Mode::Light; // auto
}

Theme::Mode Theme::currentMode()
{
    return instance()->m_mode;
}

QString Theme::savedValue()
{
    return instance()->m_saved;
}

void Theme::setSavedValue(const QString &v)
{
    QString nv = v.toLower().trimmed();
    if (nv != QLatin1String("auto") && nv != QLatin1String("light") && nv != QLatin1String("dark"))
        nv = QStringLiteral("auto");
    Theme *t = instance();
    if (t->m_saved == nv)
        return;
    t->m_saved = nv;
    t->apply();
}

void Theme::apply()
{
    const Mode m = resolveMode();
    if (m == m_mode && !qApp->styleSheet().isEmpty())
        return;
    m_mode = m;
    qApp->setStyleSheet(styleSheet());
    emit themeChanged(m_mode);
}

// ── 持久化钩子 ────────────────────────────────────────────

void Theme::loadFromJson(const QJsonObject &setup)
{
    Theme *t = instance();
    t->m_saved = setup.value(QLatin1String("Theme")).toString(
        QStringLiteral("auto"));
    if (t->m_saved != QLatin1String("light") && t->m_saved != QLatin1String("dark"))
        t->m_saved = QStringLiteral("auto");
    t->m_mode = resolveMode();
    qApp->setStyleSheet(styleSheet());
    emit t->themeChanged(t->m_mode);
}

void Theme::saveToJson(QJsonObject &setup)
{
    setup.insert(QLatin1String("Theme"), instance()->m_saved);
}

// ── 设计令牌 ──────────────────────────────────────────────

QColor Theme::windowGradientTop()
{
    return currentMode() == Mode::Dark ? QColor(0x20, 0x20, 0x24)
                                       : QColor(0xF2, 0xF2, 0xF9);
}

QColor Theme::windowGradientBottom()
{
    return currentMode() == Mode::Dark ? QColor(0x1A, 0x1D, 0x24)
                                       : QColor(0xF9, 0xEF, 0xF6);
}

QColor Theme::cardBackground()
{
    return currentMode() == Mode::Dark ? QColor(0x26, 0x2C, 0x36)
                                       : QColor(0xFB, 0xFB, 0xFD);
}

QColor Theme::cardBorder()
{
    return currentMode() == Mode::Dark ? QColor(0x37, 0x37, 0x37)
                                       : QColor(0xDF, 0xDF, 0xDF);
}

QColor Theme::textPrimary()
{
    return currentMode() == Mode::Dark ? QColor(0xE8, 0xE8, 0xEC)
                                       : QColor(0x1A, 0x1A, 0x1A);
}

QColor Theme::textSecondary()
{
    return currentMode() == Mode::Dark ? QColor(0x9A, 0x9A, 0xA4)
                                       : QColor(0x5A, 0x5A, 0x64);
}

QColor Theme::accent()
{
    return QColor(0x78, 0xA5, 0xFF); // 沿用 Floatee 现有主题蓝
}

QColor Theme::shadowColor()
{
    return currentMode() == Mode::Dark ? QColor(0, 0, 0, 160)
                                       : QColor(0xDA, 0xDA, 0xDA);
}

int Theme::radiusCard()   { return 6; }
int Theme::radiusButton() { return 3; }
int Theme::spacing()      { return 12; }

// ── 全局 QSS（浅/深两套变量式生成）────────────────────────

static QString rgba(const QColor &c, int alpha = 255)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

QString Theme::styleSheet()
{
    const bool dark = currentMode() == Mode::Dark;
    // 菜单/弹窗保留 Floatee 半透明玻璃感；卡片/按钮走 Ela 配色
    const QString menuBg   = dark ? QStringLiteral("rgba(32,32,38,225)")
                                  : QStringLiteral("rgba(250,250,253,220)");
    const QString menuFg   = dark ? QStringLiteral("#E8E8EC") : QStringLiteral("#141414");
    const QString menuDis  = dark ? QStringLiteral("rgba(232,232,236,110)")
                                  : QStringLiteral("rgba(20,20,20,110)");
    const QString menuBrd  = dark ? QStringLiteral("rgba(90,90,104,90)")
                                  : QStringLiteral("rgba(150,150,168,80)");
    const QString dlgBg    = dark ? QStringLiteral("rgba(30,32,38,245)")
                                  : QStringLiteral("rgba(248,248,252,245)");
    const QString inputBg  = dark ? QStringLiteral("rgba(38,44,54,235)")
                                  : QStringLiteral("rgba(255,255,255,235)");
    const QString inputBrd = dark ? QStringLiteral("rgba(110,110,130,140)")
                                  : QStringLiteral("rgba(140,140,160,120)");
    const QString fg       = dark ? QStringLiteral("#E8E8EC") : QStringLiteral("#1A1A1A");
    const QString fgStrong = dark ? QStringLiteral("#F2F2F6") : QStringLiteral("#141414");
    const QString accent   = QStringLiteral("rgba(120,165,255");
    const QString listSel  = dark ? QStringLiteral("rgba(120,165,255,50)")
                                  : QStringLiteral("rgba(120,165,255,70)");
    const QString scroll   = dark ? QStringLiteral("rgba(120,120,136,120)")
                                  : QStringLiteral("rgba(150,150,168,110)");

    return QStringLiteral(
        "QMenu {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 5px;"
        "}"
        "QMenu::item {"
        "  color: %3;"
        "  padding: 6px 26px 6px 18px;"
        "  border-radius: 5px;"
        "  margin: 1px 5px;"
        "  background: transparent;"
        "}"
        "QMenu::item:selected { background-color: %4; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %2; margin: 4px 12px; }"
        "QMenu::indicator { width: 14px; height: 14px; margin-left: 4px; }"
        "/* 对话框 / 弹窗 */"
        "QMessageBox, QInputDialog, QDialog {"
        "  background-color: %6;"
        "  color: %7;"
        "}"
        "QLabel { color: %7; background: transparent; }"
        "QCheckBox { color: %7; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QRadioButton { color: %7; }"
        "QGroupBox { color: %7; border: 1px solid %2; border-radius: 6px; margin-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: %7; }"
        "/* 输入框 */"
        "QLineEdit {"
        "  background: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  color: %10;"
        "  selection-background-color: %11;"
        "}"
        "QLineEdit:focus { border: 1px solid %12; }"
        "QTextEdit, QPlainTextEdit {"
        "  background: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  color: %10;"
        "}"
        "QComboBox {"
        "  background: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  padding: 4px 24px 4px 10px;"
        "  color: %10;"
        "  min-height: 22px;"
        "}"
        "QComboBox:hover { border: 1px solid %11; }"
        "QComboBox:focus { border: 1px solid %11; }"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 24px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(:/main/dropdown-arrow.svg);"
        "  width: 12px; height: 12px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %13;"
        "  border: 1px solid %2;"
        "  color: %7;"
        "  selection-background-color: %4;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QSpinBox, QDoubleSpinBox {"
        "  background: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  color: %10;"
        "}"
        "/* 按钮 */"
        "QPushButton {"
        "  background: %14;"
        "  border: 1px solid %15;"
        "  border-radius: 6px;"
        "  padding: 5px 16px;"
        "  color: %16;"
        "}"
        "QPushButton:hover { background: %17; }"
        "QPushButton:pressed { background: %18; }"
        "QPushButton:disabled { color: %19; background: %20; }"
        "QPushButton::menu-indicator { subcontrol-position: right center; right: 6px; }"
        "/* 列表 */"
        "QListWidget, QListView, QTreeWidget, QTreeView, QTableView {"
        "  background: %13;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  color: %7;"
        "}"
        "QListWidget::item, QListView::item { padding: 5px 8px; border-radius: 4px; }"
        "QListWidget::item:selected, QListView::item:selected { background: %4; color: %7; border: 1px solid %11; }"
        "QListWidget::item:hover, QListView::item:hover { background: %21; }"
        "QHeaderView::section {"
        "  background: %13;"
        "  border: none;"
        "  border-bottom: 1px solid %2;"
        "  padding: 4px 8px;"
        "  color: %7;"
        "}"
        "/* 滑块 */"
        "QSlider::groove:horizontal { height: 6px; background: %22; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: rgba(120,165,255,160); border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 14px; margin: -4px 0; border-radius: 7px; background: rgba(120,165,255,210); }"
        "/* 滚动条 */"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %22; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(120,165,255,160); }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: %22; border-radius: 4px; min-width: 24px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
        /* ElCard / ElDialog 自绘容器 */
        "#ElCard, #ElDialog { border-radius: 6px; }"
        /* 设置窗口专用 */
        "#TitleCloseBtn {"
        "  background: transparent;"
        "  color: %7;"
        "  border: 1px solid transparent;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "#TitleCloseBtn:hover { background: rgba(232,60,60,40); color: #C42B1C; border: 1px solid rgba(196,43,28,80); }"
        "#TitleCloseBtn:pressed { background: rgba(196,43,28,60); }"
    )
        .arg(menuBg, menuBrd, menuFg, listSel, menuDis,
             dlgBg, fg,
             inputBg, inputBrd, fgStrong, accent + QStringLiteral(",140)"),
             accent + QStringLiteral(",210)"),
             cardBackground().name(),
             accent + QStringLiteral(",55)"),
             accent + QStringLiteral(",130)"),
             fgStrong,
             accent + QStringLiteral(",105)"),
             accent + QStringLiteral(",150)"),
             menuDis,
             accent + QStringLiteral(",30)"),
             accent + QStringLiteral(",35)"),
             scroll);
}
