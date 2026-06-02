#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QWidget>

class VideoPlayer;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QPropertyAnimation;
class QTimer;
class QVBoxLayout;

class VideoPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPageWidget(QWidget *parent = nullptr);
    void stopPlayback();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void refreshPlaylist();
    void toggleVideoDrawer();
    void setVideoDrawerOpen(bool open, bool animated = true);
    void layoutVideoDrawer(bool animated);
    int drawerWidth() const;
    void updateProgressUi(qint64 position, qint64 duration);
    void updateVideoGeometry();
    void showVideoMessage(const QString &message);
    void showLoadingIndicator();
    QPixmap loadingPixmap() const;
    void setFullscreenMode(bool fullscreen);
    void updateFullscreenLayout();
    void setFullscreenControlsVisible(bool visible);
    void toggleFullscreenControls();
    void setChromeVisible(bool visible);
    static QString formatTime(qint64 ms);

    VideoPlayer *m_player = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QVBoxLayout *m_leftLayout = nullptr;
    QWidget *m_leftCol = nullptr;
    QWidget *m_bottomBar = nullptr;
    QWidget *m_videoArea = nullptr;
    QWidget *m_fullscreenLayer = nullptr;
    QWidget *m_clickCatcher = nullptr;
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    QWidget *m_videoWidget = nullptr;
#endif
    QLabel *m_titleLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_timeCurrent = nullptr;
    QLabel *m_timeTotal = nullptr;
    QWidget *m_listPanel = nullptr;
    QListWidget *m_list = nullptr;
    QSlider *m_progress = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_listToggleBtn = nullptr;
    QPushButton *m_fullscreenBtn = nullptr;
    QPropertyAnimation *m_drawerAnim = nullptr;
    QTimer *m_loadingTimer = nullptr;
    QTimer *m_fullscreenClickTimer = nullptr;
    int m_loadingFrame = 0;
    bool m_fullscreenMouseDown = false;
    bool m_seeking = false;
    bool m_drawerOpen = false;
    bool m_fullscreenMode = false;
    bool m_fullscreenControlsVisible = false;
};

#endif // VIDEOPAGE_H
