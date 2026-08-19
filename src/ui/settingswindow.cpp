#include "settingswindow.h"
#include "floatee.h"
#include "style/elwidgets.h"
#include "style/theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QAbstractItemView>
#include <QFrame>

// 修复 ComboBox popup 黑色直角边框（Windows 系统窗口阴影/边框线）
static void patchComboPopup(QComboBox *combo) {
    if (!combo) return;
    QFrame *view = combo->view();
    if (!view) return;
    view->setFrameShape(QFrame::NoFrame);
    // Windows 专属：popup 窗口右下方的黑色直角边框 = 系统窗口阴影/边框线
    // WA_TranslucentBackground + FramelessWindowHint 去掉系统绘制的边框线
#ifdef Q_OS_WIN
    if (auto *popup = view->window()) {
        popup->setWindowFlags(popup->windowFlags() | Qt::FramelessWindowHint);
        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->setAttribute(Qt::WA_NoSystemBackground);
        popup->update();
    }
#endif
}
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QDebug>
#include <QTimer>

static SettingsWindow *s_settingsWin = nullptr;

SettingsWindow *SettingsWindow::instance(Floatee *floatee)
{
    if (!s_settingsWin)
        s_settingsWin = new SettingsWindow(floatee);
    return s_settingsWin;
}

SettingsWindow::SettingsWindow(Floatee *floatee)
    : QWidget(nullptr, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_floatee(floatee)
{
    // 置顶：主窗口（全屏画布）默认 Always on Top，普通窗口会被盖住
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(720, 500);
    setWindowTitle(QStringLiteral("Floatee Settings"));

auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 20);
    root->setSpacing(0);

    // ── 标题栏（可拖动）──
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(36);
    m_titleBar->setCursor(Qt::SizeAllCursor);
    m_titleBar->installEventFilter(this);
    auto *tbLay = new QHBoxLayout(m_titleBar);
    tbLay->setContentsMargins(6, 0, 0, 0);
    m_titleLabel = new QLabel(QStringLiteral("Settings"), m_titleBar);
    QFont tf = m_titleLabel->font();
    tf.setWeight(QFont::DemiBold);
    tf.setPointSizeF(tf.pointSizeF() * 1.2);
    m_titleLabel->setFont(tf);
    tbLay->addWidget(m_titleLabel);
    tbLay->addStretch();
    // 关闭按钮：QToolButton + objectName 写死 QSS（不受全局 QPushButton 影响）
    m_closeBtn = new QToolButton(m_titleBar);
    m_closeBtn->setObjectName(QStringLiteral("TitleCloseBtn"));
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setFixedSize(30, 30);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("关闭"));
    connect(m_closeBtn, &QToolButton::clicked, this, &SettingsWindow::close);
    tbLay->addWidget(m_closeBtn);
    root->addWidget(m_titleBar);

    // ── 主体：左导航 + 右页面 ──
    auto *body = new QHBoxLayout;
    body->setSpacing(16);
    root->addLayout(body, 1);

    m_nav = new QListWidget(this);
    m_nav->setFixedWidth(150);
    m_nav->setIconSize(QSize(20, 20));
    m_nav->setSpacing(5);              // 项间距，防止 Windows 上重叠
    m_nav->setFocusPolicy(Qt::NoFocus);   // 去掉选中项的焦点虚线框
    const QStringList navItems = {
        QStringLiteral("常规"), QStringLiteral("外观"), QStringLiteral("联机"),
        QStringLiteral("休眠"), QStringLiteral("实例"),
    };
    for (const QString &t : navItems)
        m_nav->addItem(t);
    m_nav->setCurrentRow(0);
    body->addWidget(m_nav);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildGeneralPage());
    m_pages->addWidget(buildAppearancePage());
    m_pages->addWidget(buildNetworkPage());
    m_pages->addWidget(buildSleepPage());
    m_pages->addWidget(buildInstancePage());
    body->addWidget(m_pages, 1);

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);

    connect(Theme::instance(), &Theme::themeChanged, this, [this]() { update(); });

    // ── 运行时状态实时刷新（SizeScale、服务器状态、使用时长）──
    // 定时器：每 500ms 刷新 SizeScale 和使用时长（轻量级，仅更新文本）
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        // 缩放：如果运行时 SizeScale 变化（滚轮/菜单），同步到 Combo
        // 滚轮使用连续值（×1.1/×0.9），Combo 只有离散值（50%~200% 步进 10%）
        // 找最接近的匹配项
        m_sizeCombo->blockSignals(true);
        int bestIdx = 0;
        double bestDiff = std::abs(m_sizeCombo->itemData(0).toDouble() - m_floatee->SizeScale);
        for (int i = 1; i < m_sizeCombo->count(); ++i) {
            const double diff = std::abs(m_sizeCombo->itemData(i).toDouble() - m_floatee->SizeScale);
            if (diff < bestDiff) { bestDiff = diff; bestIdx = i; }
        }
        if (m_sizeCombo->currentIndex() != bestIdx)
            m_sizeCombo->setCurrentIndex(bestIdx);
        m_sizeCombo->blockSignals(false);
        // 房间信息：实时更新
        if (m_roomInfoLabel && m_floatee->m_multi) {
            QString info;
            if (m_floatee->m_multi->inRoom()) {
                const QString id = m_floatee->m_multi->roomId();
                const QString name = m_floatee->m_multi->roomName();
                const QString code = m_floatee->m_multi->joinCode();
                info = QStringLiteral("房间名：%1\n房间号：%2\n密码：%3")
                    .arg(name.isEmpty() ? QStringLiteral("(未命名)") : name,
                         id,
                         code.isEmpty() ? QStringLiteral("(无密码)") : code);
            } else {
                info = QStringLiteral("未加入房间");
            }
            if (m_roomInfoLabel->text() != info)
                m_roomInfoLabel->setText(info);
        }
        // 使用时长
        if (m_usageLabel) {
            const int secs = m_floatee->m_usageSeconds;
            const int mins = secs / 60;
            const int hrs = mins / 60;
            QString text;
            if (hrs > 0)
                text = QStringLiteral("当前使用时长：%1 小时 %2 分钟").arg(hrs).arg(mins % 60);
            else
                text = QStringLiteral("当前使用时长：%1 分钟").arg(mins);
            if (m_usageLabel->text() != text)
                m_usageLabel->setText(text);
        }
    });
    // 窗口可见时启动定时器，隐藏时停止（节省 CPU）
    // visibleChanged 信号在 Qt 5.15+ 才有，改用 showEvent/hideEvent
    // （在 showPage 中启动，closeEvent 中停止）

    // 服务器状态：连接 Multiplayer::statusChanged 实时更新
    if (m_floatee->m_multi) {
        connect(m_floatee->m_multi, &Multiplayer::statusChanged, this,
                [this](const QString &s) {
                    if (m_mpStatus && m_mpStatus->text() != s)
                        m_mpStatus->setText(s);
                });
    }
}

// ═══════════════════════ 页面构建 ═══════════════════════

QWidget *SettingsWindow::buildGeneralPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    // 主题卡片
    auto *themeCard = new ElCard(QStringLiteral("主题"), page);
    auto *themeRow = new QHBoxLayout;
    themeRow->addWidget(new QLabel(QStringLiteral("深浅模式："), page));
    m_themeCombo = new QComboBox(page);
    patchComboPopup(m_themeCombo);
    m_themeCombo->addItem(QStringLiteral("跟随系统"), QStringLiteral("auto"));
    m_themeCombo->addItem(QStringLiteral("浅色"), QStringLiteral("light"));
    m_themeCombo->addItem(QStringLiteral("深色"), QStringLiteral("dark"));
    themeRow->addWidget(m_themeCombo);
    themeRow->addStretch();
    themeCard->contentLayout()->addLayout(themeRow);
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString v = m_themeCombo->itemData(idx).toString();
        Theme::setSavedValue(v);
        Theme::saveToJson(m_floatee->Setup);
        JsonOpt::Json2File(m_floatee->Path_Setup, QJsonDocument(m_floatee->Setup));
    });
    lay->addWidget(themeCard);

    // 窗口卡片
    auto *winCard = new ElCard(QStringLiteral("窗口"), page);
    m_onTopCheck = new QCheckBox(QStringLiteral("总在最前（Always on Top）"), page);
    winCard->contentLayout()->addWidget(m_onTopCheck);
    connect(m_onTopCheck, &QCheckBox::toggled, m_floatee, [this](bool on) {
        // 复用托盘逻辑：同步 action 勾选态 + 槽函数（含持久化）
        if (m_floatee->AlwaysOnTopAction) {
            m_floatee->AlwaysOnTopAction->setChecked(on);
            m_floatee->toggleAlwaysOnTop();
        }
    });
    lay->addWidget(winCard);

    // 颜色调整卡片（打开现有对话框）
    auto *colorCard = new ElCard(QStringLiteral("颜色"), page);
    auto *colorRow = new QHBoxLayout;
    colorRow->addWidget(new QLabel(QStringLiteral("调整 Tee 的色相/饱和度/明度"), page));
    colorRow->addStretch();
    auto *colorBtn = new ElButton(QStringLiteral("Color Adjust..."),
                                  ElButton::Kind::Primary, page);
    connect(colorBtn, &ElButton::clicked, m_floatee, &Floatee::openColorDialog);
    colorRow->addWidget(colorBtn);
    colorCard->contentLayout()->addLayout(colorRow);
    lay->addWidget(colorCard);

    lay->addStretch();
    return page;
}

QWidget *SettingsWindow::buildAppearancePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    // 皮肤
    auto *skinCard = new ElCard(QStringLiteral("皮肤"), page);
    auto *skinRow = new QHBoxLayout;
    skinRow->addWidget(new QLabel(QStringLiteral("皮肤："), page));
    m_skinCombo = new QComboBox(page);
    patchComboPopup(m_skinCombo);
    skinRow->addWidget(m_skinCombo, 1);
    auto *skinFolderBtn = new ElButton(QStringLiteral("打开皮肤目录"), ElButton::Kind::Standard, page);
    skinRow->addWidget(skinFolderBtn);
    skinCard->contentLayout()->addLayout(skinRow);
    connect(skinFolderBtn, &ElButton::clicked, this, []() {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + QStringLiteral("/skins");
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    connect(m_skinCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString path = m_skinCombo->itemData(idx).toString();
        if (path.isEmpty() || path == m_floatee->CurrentSkin)
            return;
        // 复用托盘 switchSkin：构造带 data 的 QAction
        QAction a(m_skinCombo->itemText(idx));
        a.setData(path);
        m_floatee->switchSkin(&a);
    });
    lay->addWidget(skinCard);

    // 眼睛
    auto *eyeCard = new ElCard(QStringLiteral("眼睛"), page);
    auto *eyeRow = new QHBoxLayout;
    eyeRow->addWidget(new QLabel(QStringLiteral("表情眼睛："), page));
    m_eyeCombo = new QComboBox(page);
    patchComboPopup(m_eyeCombo);
    const QStringList eyes = { QStringLiteral("Normal"), QStringLiteral("Happy"),
                               QStringLiteral("Angry"), QStringLiteral("Pain"),
                               QStringLiteral("Surprise"), QStringLiteral("Blink") };
    for (int i = 0; i < eyes.size(); ++i)
        m_eyeCombo->addItem(eyes.at(i), i);
    eyeRow->addWidget(m_eyeCombo);
    eyeRow->addStretch();
    eyeCard->contentLayout()->addLayout(eyeRow);
    connect(m_eyeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        QAction a;
        a.setData(idx);
        m_floatee->switchEye(&a);
    });
    lay->addWidget(eyeCard);

    // 缩放 + 羽化
    auto *renderCard = new ElCard(QStringLiteral("渲染"), page);
    auto *sizeRow = new QHBoxLayout;
    sizeRow->addWidget(new QLabel(QStringLiteral("缩放："), page));
    m_sizeCombo = new QComboBox(page);
    patchComboPopup(m_sizeCombo);
    for (double s = 0.5; s <= 2.001; s += 0.1)
        m_sizeCombo->addItem(QStringLiteral("%1%").arg(qRound(s * 100)), s);
    sizeRow->addWidget(m_sizeCombo);
    sizeRow->addSpacing(16);
    sizeRow->addWidget(new QLabel(QStringLiteral("边缘羽化："), page));
    m_featherCombo = new QComboBox(page);
    patchComboPopup(m_featherCombo);
    m_featherCombo->addItems({ QStringLiteral("Off"), QStringLiteral("Normal"),
                               QStringLiteral("Strong") });
    sizeRow->addWidget(m_featherCombo);
    sizeRow->addStretch();
    renderCard->contentLayout()->addLayout(sizeRow);
    connect(m_sizeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        QAction a;
        a.setData(m_sizeCombo->itemData(idx));
        m_floatee->switchSize(&a);
    });
    connect(m_featherCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        QAction a;
        a.setData(idx);
        m_floatee->switchFeather(&a);
    });
    lay->addWidget(renderCard);

    // 表情素材
    auto *emoCard = new ElCard(QStringLiteral("表情素材"), page);
    auto *emoRow = new QHBoxLayout;
    emoRow->addWidget(new QLabel(QStringLiteral("图集："), page));
    m_emoticonSetCombo = new QComboBox(page);
    patchComboPopup(m_emoticonSetCombo);
    emoRow->addWidget(m_emoticonSetCombo, 1);
    auto *emoFolderBtn = new ElButton(QStringLiteral("打开素材目录"), ElButton::Kind::Standard, page);
    emoRow->addWidget(emoFolderBtn);
    emoCard->contentLayout()->addLayout(emoRow);
    connect(emoFolderBtn, &ElButton::clicked, this, []() {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + QStringLiteral("/emoticons");
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    connect(m_emoticonSetCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString path = m_emoticonSetCombo->itemData(idx).toString();
        if (path.isEmpty() || path == m_floatee->EmoticonSet)
            return;
        QAction a;
        a.setData(path);
        m_floatee->switchEmoticonSet(&a);
    });
    lay->addWidget(emoCard);

    lay->addStretch();
    return page;
}

QWidget *SettingsWindow::buildNetworkPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto *srvCard = new ElCard(QStringLiteral("服务器"), page);
    auto *srvRow = new QHBoxLayout;
    srvRow->addWidget(new QLabel(QStringLiteral("地址："), page));
    m_serverEdit = new QLineEdit(page);
    m_serverEdit->setPlaceholderText(QStringLiteral("host:port（如 127.0.0.1:8764）"));
    srvRow->addWidget(m_serverEdit, 1);
    m_connectBtn = new ElButton(QStringLiteral("连接"), ElButton::Kind::Primary, page);
    srvRow->addWidget(m_connectBtn);
    m_disconnectBtn = new ElButton(QStringLiteral("断开"), ElButton::Kind::Standard, page);
    srvRow->addWidget(m_disconnectBtn);
    srvCard->contentLayout()->addLayout(srvRow);
    lay->addWidget(srvCard);

    auto *statusCard = new ElCard(QStringLiteral("状态"), page);
    m_mpStatus = new QLabel(QStringLiteral("离线"), page);
    statusCard->contentLayout()->addWidget(m_mpStatus);
    lay->addWidget(statusCard);

    // 房间信息卡片
    auto *roomCard = new ElCard(QStringLiteral("房间"), page);
    m_roomInfoLabel = new QLabel(QStringLiteral("未加入房间"), page);
    m_roomInfoLabel->setWordWrap(true);
    roomCard->contentLayout()->addWidget(m_roomInfoLabel);
    auto *roomBtnRow = new QHBoxLayout;
    auto *createBtn = new ElButton(QStringLiteral("创建房间"), ElButton::Kind::Primary, page);
    auto *joinBtn = new ElButton(QStringLiteral("加入房间"), ElButton::Kind::Standard, page);
    auto *leaveBtn = new ElButton(QStringLiteral("离开房间"), ElButton::Kind::Standard, page);
    m_roomListBtn = new ElButton(QStringLiteral("房间列表"), ElButton::Kind::Standard, page);
    roomBtnRow->addWidget(createBtn);
    roomBtnRow->addWidget(joinBtn);
    roomBtnRow->addWidget(leaveBtn);
    roomBtnRow->addWidget(m_roomListBtn);
    roomBtnRow->addStretch();
    roomCard->contentLayout()->addLayout(roomBtnRow);
    lay->addWidget(roomCard);

    // 房间操作：复用托盘菜单的 mpCreateRoom / mpJoinRoom / mpRoomList
    connect(createBtn, &ElButton::clicked, m_floatee, &Floatee::mpCreateRoom);
    connect(joinBtn, &ElButton::clicked, m_floatee, &Floatee::mpJoinRoom);
    connect(leaveBtn, &ElButton::clicked, m_floatee, [this]() {
        if (m_floatee->m_multi) m_floatee->m_multi->leaveRoom();
    });
    connect(m_roomListBtn, &ElButton::clicked, m_floatee, &Floatee::mpRoomList);

    // 连接/断开：直接用输入框地址（复用 mpConnect 的解析与持久化规则）
    connect(m_connectBtn, &ElButton::clicked, this, [this]() {
        const QString s = m_serverEdit->text().trimmed();
        if (s.isEmpty())
            return;
        const int colon = s.lastIndexOf(QLatin1Char(':'));
        QString host = s;
        quint16 port = 8764;
        if (colon > 0) {
            host = s.left(colon);
            const quint16 p = s.mid(colon + 1).toUShort();
            port = p != 0 ? p : 8764;
        }
        QJsonObject mp = m_floatee->Setup.value("multiplayer").toObject();
        mp.insert("server", s);
        m_floatee->Setup.insert("multiplayer", mp);
        saveSetup();
        m_floatee->m_multi->connectTo(host, port);
    });
    connect(m_disconnectBtn, &ElButton::clicked, m_floatee, &Floatee::mpDisconnect);

    lay->addStretch();
    return page;
}

QWidget *SettingsWindow::buildSleepPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto *card = new ElCard(QStringLiteral("休眠与休息提醒"), page);
    auto *grid = new QVBoxLayout;
    grid->setSpacing(10);

    // 自由输入：QLineEdit + QIntValidator，提交（editingFinished）时校验+保存
    auto makeRow = [&](const QString &labelText, QLineEdit *&edit,
                       const QString &placeholder, int min, int max) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(labelText, page));
        edit = new QLineEdit(page);
        edit->setPlaceholderText(placeholder);
        edit->setValidator(new QIntValidator(min, max, edit));
        edit->setMaximumWidth(120);
        // 占位提示单位
        row->addWidget(edit);
        auto *unit = new QLabel(QStringLiteral("(%1-%2)")
                                    .arg(min == 0 ? QStringLiteral("0=禁用") : QString::number(min))
                                    .arg(max),
                                page);
        row->addWidget(unit);
        row->addStretch();
        grid->addLayout(row);
    };
    makeRow(QStringLiteral("无操作多少秒后休眠："),
            m_sleepTimeoutEdit, QStringLiteral("0-86400"), 0, 86400);
    makeRow(QStringLiteral("每多少分钟提醒休息："),
            m_breakRemindEdit, QStringLiteral("0-1440"), 0, 1440);
    makeRow(QStringLiteral("单次休眠超过多少分钟视为新会话："),
            m_resetAfterEdit, QStringLiteral("0-43200"), 0, 43200);

    // 当前累计使用时长（实时更新，与托盘菜单一致）
    auto *usageRow = new QHBoxLayout;
    m_usageLabel = new QLabel(QStringLiteral("当前使用时长：0 分钟"), page);
    m_usageLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;")
        .arg(Theme::textSecondary().name()));
    usageRow->addWidget(m_usageLabel);
    usageRow->addStretch();
    grid->addLayout(usageRow);

    card->contentLayout()->addLayout(grid);
    lay->addWidget(card);

    auto commit = [&](QLineEdit *edit, int min, int max,
                      const QString &key, std::function<void(int)> apply) {
        int v = edit->text().toInt();
        if (v < min) v = min;
        if (v > max) v = max;
        if (QString::number(v) != edit->text())
            edit->setText(QString::number(v));
        QJsonObject &s = m_floatee->Setup;
        s[key] = v;
        if (apply) apply(v);
        saveSetup();
    };
    connect(m_sleepTimeoutEdit, &QLineEdit::editingFinished, this, [this, commit]() {
        commit(m_sleepTimeoutEdit, 0, 86400,
               QStringLiteral("SleepTimeout"),
               [this](int v) { m_floatee->m_sleepTimeoutSec = v; });
    });
    connect(m_breakRemindEdit, &QLineEdit::editingFinished, this, [this, commit]() {
        commit(m_breakRemindEdit, 0, 1440,
               QStringLiteral("BreakReminder"),
               [this](int v) {
                   m_floatee->m_breakReminderMin = v;
                   m_floatee->m_usageSeconds = 0;
               });
    });
    connect(m_resetAfterEdit, &QLineEdit::editingFinished, this, [this, commit]() {
        commit(m_resetAfterEdit, 0, 43200,
               QStringLiteral("ResetAfterSleep"),
               [this](int v) { m_floatee->m_resetAfterSleepMin = v; });
    });

    lay->addStretch();
    return page;
}

QWidget *SettingsWindow::buildInstancePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto *card = new ElCard(QStringLiteral("多实例"), page);
    auto *v = card->contentLayout();

    m_instanceList = new QListWidget(page);
    m_instanceList->setMinimumHeight(160);
    v->addWidget(m_instanceList);

    auto *btnRow = new QHBoxLayout;
    auto *launchBtn = new ElButton(QStringLiteral("启动新实例"), ElButton::Kind::Primary, page);
    auto *customBtn = new ElButton(QStringLiteral("自定义配置..."), ElButton::Kind::Standard, page);
    auto *folderBtn = new ElButton(QStringLiteral("打开配置目录"), ElButton::Kind::Standard, page);
    btnRow->addWidget(launchBtn);
    btnRow->addWidget(customBtn);
    btnRow->addWidget(folderBtn);
    btnRow->addStretch();
    v->addLayout(btnRow);

    connect(launchBtn, &ElButton::clicked, m_floatee, &Floatee::launchNewInstance);
    connect(customBtn, &ElButton::clicked, m_floatee, &Floatee::openNewInstance);
    connect(folderBtn, &ElButton::clicked, m_floatee, &Floatee::openConfigFolder);
    // 双击切换配置（与托盘 Instance 菜单一致）
    connect(m_instanceList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QAction a;
        a.setData(item->data(Qt::UserRole));
        m_floatee->switchConfig(&a);
        refreshFromSetup();
    });

    lay->addWidget(card);
    lay->addStretch();
    return page;
}

// ═══════════════════════ 数据同步 ═══════════════════════

void SettingsWindow::saveSetup()
{
    JsonOpt::Json2File(m_floatee->Path_Setup, QJsonDocument(m_floatee->Setup));
}

void SettingsWindow::refreshFromSetup()
{
    const QJsonObject &s = m_floatee->Setup;

    // 主题
    const QString theme = Theme::savedValue();
    for (int i = 0; i < m_themeCombo->count(); ++i)
        if (m_themeCombo->itemData(i).toString() == theme) {
            m_themeCombo->setCurrentIndex(i);
            break;
        }

    // 置顶
    if (m_floatee->AlwaysOnTopAction)
        m_onTopCheck->setChecked(m_floatee->AlwaysOnTopAction->isChecked());

    // 皮肤（内置 + 外部 skins/）
    m_skinCombo->blockSignals(true);
    m_skinCombo->clear();
    m_skinCombo->addItem(QStringLiteral("Default"),
                         QStringLiteral(":/skins/default.png"));
    const QString skinsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/skins");
    QDirIterator sit(skinsDir, {QStringLiteral("*.png")}, QDir::Files);
    while (sit.hasNext()) {
        sit.next();
        m_skinCombo->addItem(sit.fileInfo().completeBaseName(), sit.filePath());
    }
    for (int i = 0; i < m_skinCombo->count(); ++i)
        if (m_skinCombo->itemData(i).toString() == m_floatee->CurrentSkin) {
            m_skinCombo->setCurrentIndex(i);
            break;
        }
    m_skinCombo->blockSignals(false);

    // 眼睛 / 缩放 / 羽化
    m_eyeCombo->blockSignals(true);
    m_eyeCombo->setCurrentIndex(qBound(0, s.value(QStringLiteral("Eye")).toInt(0), 5));
    m_eyeCombo->blockSignals(false);

    m_sizeCombo->blockSignals(true);
    for (int i = 0; i < m_sizeCombo->count(); ++i)
        if (qFuzzyCompare(m_sizeCombo->itemData(i).toDouble(), m_floatee->SizeScale)) {
            m_sizeCombo->setCurrentIndex(i);
            break;
        }
    m_sizeCombo->blockSignals(false);

    m_featherCombo->blockSignals(true);
    m_featherCombo->setCurrentIndex(qBound(0, s.value(QStringLiteral("Feather")).toInt(1), 2));
    m_featherCombo->blockSignals(false);

    // 表情素材
    m_emoticonSetCombo->blockSignals(true);
    m_emoticonSetCombo->clear();
    const QString defaultEmo = QStringLiteral(":/main/emoticons.png");
    m_emoticonSetCombo->addItem(QStringLiteral("Default"), defaultEmo);
    const QString emoDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/emoticons");
    QDirIterator eit(emoDir, {QStringLiteral("*.png")}, QDir::Files);
    while (eit.hasNext()) {
        eit.next();
        m_emoticonSetCombo->addItem(eit.fileInfo().completeBaseName(), eit.filePath());
    }
    for (int i = 0; i < m_emoticonSetCombo->count(); ++i)
        if (m_emoticonSetCombo->itemData(i).toString() == m_floatee->EmoticonSet) {
            m_emoticonSetCombo->setCurrentIndex(i);
            break;
        }
    m_emoticonSetCombo->blockSignals(false);

    // 联机
    m_serverEdit->setText(s.value(QStringLiteral("multiplayer")).toObject()
                              .value(QStringLiteral("server"))
                              .toString(QStringLiteral("127.0.0.1:8764")));
    // 服务器状态：从 Multiplayer 实例获取当前状态
    if (m_floatee->m_multi && m_mpStatus) {
        const QString status = m_floatee->m_multi->isConnected()
                               ? QStringLiteral("已连接")
                               : QStringLiteral("离线");
        if (m_mpStatus->text() != status)
            m_mpStatus->setText(status);
    }

    // 休眠
    m_sleepTimeoutEdit->setText(QString::number(s.value(QStringLiteral("SleepTimeout")).toInt(60)));
    m_breakRemindEdit->setText(QString::number(s.value(QStringLiteral("BreakReminder")).toInt(20)));
    m_resetAfterEdit->setText(QString::number(s.value(QStringLiteral("ResetAfterSleep")).toInt(120)));

    // 实例列表
    m_instanceList->clear();
    const QString dir = QFileInfo(m_floatee->Path_Setup).absolutePath();
    const QString cur = QFileInfo(m_floatee->Path_Setup).fileName();
    const QStringList files =
        QDir(dir).entryList({QStringLiteral("default*.json"), QStringLiteral("setup*.json")},
                            QDir::Files, QDir::Name);
    for (const QString &f : files) {
        auto *item = new QListWidgetItem(f, m_instanceList);
        item->setData(Qt::UserRole, f);
        if (f == cur)
            item->setText(f + QStringLiteral("  ✓"));
    }
}

// ═══════════════════════ 窗口行为 ═══════════════════════

void SettingsWindow::showPage(int index)
{
    refreshFromSetup();
    m_nav->setCurrentRow(index);
    show();
    raise();
    activateWindow();
    // 启动定时刷新（SizeScale、使用时长等运行时状态）
    if (m_refreshTimer && !m_refreshTimer->isActive())
        m_refreshTimer->start();
}

void SettingsWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Ela 渐变底 + 圆角裁剪
    QLinearGradient g(0, 0, width(), height());
    g.setColorAt(0, Theme::windowGradientTop());
    g.setColorAt(1, Theme::windowGradientBottom());
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(10, 10, -10, -10), 8, 8);
    p.fillPath(path, g);
    p.setPen(QPen(Theme::cardBorder(), 1));
    p.drawPath(path);
}

void SettingsWindow::closeEvent(QCloseEvent *event)
{
    // 停止定时刷新
    if (m_refreshTimer)
        m_refreshTimer->stop();
    // 隐藏复用：关闭仅隐藏，不销毁
    event->ignore();
    hide();
}

bool SettingsWindow::eventFilter(QObject *watched, QEvent *event)
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
    return QWidget::eventFilter(watched, event);
}
