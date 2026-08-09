#ifndef FLOATEE_H
#define FLOATEE_H

#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
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

#include "ui/windowsidehide.h"
#include "ui/teeyes.h"
#include "ui/emoticonwindow.h"
#include "core/jsonopt.h"
#include "core/teedrawer.h"

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
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    QSystemTrayIcon TrayIcon;
    QMenu *TrayMenu = nullptr;
    QMenu *SkinMenu = nullptr;
    QMenu *EyeMenu = nullptr;
    QMenu *SizeMenu = nullptr;
    QActionGroup *SkinGroup = nullptr;
    QActionGroup *EyeGroup = nullptr;
    QActionGroup *SizeGroup = nullptr;
    QAction *AlwaysOnTopAction = nullptr;
    QAction *WindowSideHideAction = nullptr;
    QAction *TeEyesAction = nullptr;
    QString CurrentSkin;
    QPoint MousePoint;
    bool MousePress;

    // Pet zoom (1.0 = base 96×96 window). Drives TeeDrawer::setRenderScale;
    // persisted to setup.json "Size".
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
    void showEmoticonOnTee(int index);

private:
    Ui::Floatee *ui;
};

#endif // FLOATEE_H
