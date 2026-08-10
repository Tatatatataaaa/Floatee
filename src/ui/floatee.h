#ifndef FLOATEE_H
#define FLOATEE_H

#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QTimer>
#include <QDebug>
#include <QTime>
#include <QPropertyAnimation>
#include <QScreen>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QProcess>
#include <memory>

#include "ui/windowsidehide.h"
#include "ui/teeyes.h"
#include "ui/emoticonwindow.h"
#include "ui/emoticonwheel.h"
#include "multiplayer/multiplayer.h"
#include "core/jsonopt.h"
#include "core/teedrawer.h"
#include "platform/platformwindowinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Floatee;
}
QT_END_NAMESPACE

class Floatee : public QMainWindow
{
    Q_OBJECT

public:
    Floatee(QWidget *parent = nullptr);
    ~Floatee();
    void Loading();
    void Initialize();
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    QSystemTrayIcon TrayIcon;
    QMenu *TrayMenu = nullptr;
    QMenu *SkinMenu = nullptr;
    QMenu *EyeMenu = nullptr;
    QMenu *SizeMenu = nullptr;
    QMenu *FeatherMenu = nullptr;
    QMenu *EmoticonMenu = nullptr;   // M4：16 表情托盘子菜单
    QMenu *InstanceMenu = nullptr;
    QActionGroup *SkinGroup = nullptr;
    QActionGroup *EyeGroup = nullptr;
    QActionGroup *SizeGroup = nullptr;
    QActionGroup *FeatherGroup = nullptr;
    QActionGroup *InstanceGroup = nullptr;
    QAction *AlwaysOnTopAction = nullptr;
    QAction *WindowSideHideAction = nullptr;
    QAction *TeEyesAction = nullptr;
    QString CurrentSkin;
    QPoint MousePoint;
    bool MousePress;

    // Pet zoom (1.0 = base 96×96 window). Drives TeeDrawer::setRenderScale;
    // persisted to default.json "Size".
    double SizeScale = 1.0;

    // Tee position INSIDE the fixed-size window (top-left). Zooming re-renders
    // the tee at the new scale and moves this offset (content-anchored zoom);
    // the window geometry never changes, so zooming is a pure content update
    // and never triggers the layered-window "geometry+content" stale frame.
    QPointF m_teePos;

    // Cursor-driven eye follow (tee_render layout): the full tee is re-rendered
    // with the look direction pointing at the cursor, so the eyes slide inside
    // the face — the authentic tee_render behaviour replaces the old eye QLabel.
    QTimer *EyeFollowTimer = nullptr;
    float LastDirX = 1.0f;
    float LastDirY = 0.0f;
    int RenderedEye = -1;

    // Walk animation: while the tee is dragged the walk-cycle phase in [0,1)
    // is driven by its horizontal position (DDNet formula fmod(x,100)/100);
    // -1 = idle pose (not dragging).
    float WalkPhase = -1.0f;
    float LastWalkPhase = -1.0f;

    // Distance-based eye travel: how far the eyes slide inside the face
    // (0 = centred, ~1.2 = full travel), derived from cursor distance with a
    // nonlinear (√-like) response.
    float LastEyeScale = -1.0f;

    // Over-head emoticon: independent floating window + random trigger timer.
    EmoticonWindow *EmoticonWin = nullptr;
    QTimer *EmoticonRandomTimer = nullptr;
    // 联机控制器（online 分支）：连接/房间管理。
    Multiplayer *m_multi = nullptr;
    QMenu *MpMenu = nullptr;
    QAction *MpStatusAction = nullptr;

    // ── M2 多人渲染：全屏画布 + Peer ──
    // 注意：TeeDrawer 的 m_renderer 持有 &m_backend（裸指针），浅拷贝会悬垂。
    // 因此 PeerRender 用 shared_ptr<TeeDrawer>（只创建一次、QHash 拷贝安全）。
    struct PeerRender {
        std::shared_ptr<TeeDrawer> drawer;   // 创建时按皮肤加载
        QPointF pos;               // 屏幕坐标（画布左上角）
        float scale = 1.0f;
        QString skin;              // 当前已加载的皮肤名（变化时重建 body 层）
        int eye = 0;
        QPointF dir{0.0f, -1.0f};
        float eyeScale = 0.0f;     // 远端眼睛偏移幅度（同步自对端）
        // 分离渲染（弱设备关键优化）：body（身体+轮廓+脚）静态，仅在皮肤变化
        // 时重渲染；eyes（眼睛层）每次眼睛/方向变化只重渲染眼睛小区域。
        QPixmap body;
        QPixmap eyes;
        bool hidden = false;          // M4：右键隐藏（仅本地摆放，不发送）
        // 渲染缓存：数据未变化（含容差）时跳过重渲染（降低 CPU）
        int lastEye = -1;
        float lastEyeScale = -1.0f;
        QPointF lastDir{0.0f, 0.0f};
    };
    QHash<QString, PeerRender> m_peersRender;   // roleId -> 远端 Tee 渲染
    EmoticonWheel *m_emoticonWheel = nullptr;   // M4：表情圆盘（全屏画布 overlay）
    bool m_fullscreenEntered = false;           // 首次 show 后进入全屏（防重复）
    PlatformWindowInfo *m_platformInfo = nullptr; // 平台层（Win32 顶层置顶）
    QTimer *m_taskbarTimer = nullptr;           // 周期把任务栏提到最顶层
    bool m_fullscreenCanvas = false;            // 联机全屏画布模式
    QPoint m_preFullscreenPos;                  // 进入全屏前的窗口位置
    QPointF m_localTeePos;                      // 全屏时本地 Tee 的屏幕坐标（左上角）
    QString m_dragRoleId;                       // 拖拽目标 roleId（空 = 本地 Tee）
    bool m_dragging = false;                    // 是否正在拖拽（本地也用空 roleId，需独立标志）
    QPointF m_dragOffset;                       // 按下点到 Tee 左上角偏移
    QTimer *m_hitTestTimer = nullptr;           // 动态点击穿透检测
    bool m_transparent = false;                 // 当前穿透状态（仅在变化时 setAttribute）
    // True while the cursor is hovering the tee (petting); used to edge-trigger
    // the hearts emoticon once per entry instead of spamming every frame.
    bool m_petting = false;

    WindowSideHide ExecWindowSideHide;
    TeEyes ExecTeEyes;
    TeeDrawer ExecTeeDrawer;
    int HueShift = 0;
    double SatFactor = 1.0;
    double LightFactor = 1.0;

    QJsonObject Setup;
    QString Path_Setup;
    // Multi-instance profile from `--profile=<name>` (empty = the default
    // config). Each profile uses its own default_<name>.json so several
    // Floatee instances can run side by side with different skins/settings
    // while the shared skin library (AppDataLocation/skins) stays common to all.
    QString m_profile;

    // Last known activation state; used to refresh the translucent display only
    // when the window really gains/loses focus (filters spurious activation
    // events that some apps like Chromium/Electron emit periodically).
    bool m_wasActive = false;

protected slots:
    void on_systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAlwaysOnTop();
    void toggleWindowSideHide();
    void toggleTeEyes();
    void refreshTranslucentDisplay();
    void switchSkin(QAction *action);
    void switchEye(QAction *action);
    void switchSize(QAction *action);
    void switchFeather(QAction *action);
    // ── online 分支：Multiplayer 托盘菜单 ──
    void mpConnect();
    void mpDisconnect();
    void mpCreateRoom();
    void mpJoinRoom();
    void mpShowJoinCode();
    void mpRoomList();
    // 启动时自动连接上次使用的服务器（default.json multiplayer.server）
    void tryAutoConnectLastServer();

    // ── M2：全屏画布 / Peer 渲染 / 交互 ──
    void onRoomChangedMp();     // 进出房间 → 切换全屏画布
    void onPeersChangedMp();    // Peer 表变化 → 同步渲染
    void onHitTestTick();       // 动态点击穿透检测
    QString currentSkinName() const;                 // 皮肤文件名（用于同步）
    // 命中检测；pad 为命中框外扩像素（穿透轮询用大 pad，交互用 0）
    bool hitTestTee(const QPoint &g, QString *outRoleId, int pad = 0) const;

    // One-click multi-instance: launch a NEW PROCESS with an auto-generated
    // unique profile name (no dialog) for quick side-by-side pets.
    void launchNewInstance();
    // Launch a new process with a user-chosen profile name (dialog).
    void openNewInstance();
    // Open the config directory (AppDataLocation: setup files + shared skins).
    void openConfigFolder();
    // (Re)build the Instance submenu: one checkable entry per default*.json in
    // the config dir (current one checked) + Launch/New Config/Open Folder.
    void buildInstanceMenu();
    // Tray action: switch THIS process to the clicked config file.
    void switchConfig(QAction *action);
    // Re-apply every setting from the current Setup at runtime (used after
    // switching the active config file).
    void applyLiveConfig();
    // Core zoom: applies a scale (resize window + re-render + persist + menu
    // sync) shared by the Size menu and the mouse-wheel zoom. Returns true if
    // the scale actually changed.
    // anchorAtCursor: when true (wheel zoom), the window is moved after the
    // resize so the tee point under the cursor stays under the cursor — a plain
    // resize keeps the top-left corner fixed, so shrinking would slide the
    // cursor out of the tee and break rapid consecutive zooms.
    bool applySizeScale(double scale, bool anchorAtCursor = false);
    // Wheel zoom: move one step (+1 = zoom in, -1 = zoom out) along the fixed
    // Size menu levels, snapping from the current scale to the nearest level.
    void zoomSize(int step);
    void openColorDialog();
    void updateEyeFollow();
    void triggerRandomEmoticon();
    void onRandomEmoticonTick();
    // 在指定 Tee 上显示表情（key 空 = 本地；否则 = 远端 roleId）
    void showEmoticonOnTee(int index, const QString &key = QString());
    // M4：本地发表情（显示 + 联机广播）
    void sendLocalEmoticon(int index);
    // M4：右键本地 Tee 打开表情圆盘（全屏/非全屏通用）
    void openEmoticonWheel();
    // M4：圆盘点击提交（表情 / 眼睛 / 取消），widget 坐标
    void submitEmoticonWheel(const QPointF &widgetPos);
    // 启动即进入全屏透明画布（单机/联机统一），避免小窗口↔全屏来回切换
    void enterFullscreenCanvas();
    // 把系统任务栏提升到 topmost 最顶层（周期调用，保证任务栏永远可见）
    void raiseTaskbarTopmost();
    // 临时诊断：输出窗口与任务栏的最终状态（定位任务栏遮挡，定位后移除）
    void logWindowState();
    // M4：右键远端 Tee 管理菜单（隐藏/重置/踢出）
    void showPeerContextMenu(const QString &roleId, const QPoint &g);
    // M4：收到远端 Tee 表情
    void onEmoticonReceivedMp(const QString &roleId, int index);
    // M4：全屏画布内渲染所有活跃表情（本地 + 每个远端 Tee）
    void paintEmoticonsFullscreen(QPainter &p);

private:
    Ui::Floatee *ui;
};

#endif // FLOATEE_H
