#ifndef MUSICPAGE_H
#define MUSICPAGE_H

#include <QVector>
#include <QWidget>
#include <QMouseEvent>
#include <functional>

class MusicPlayer;
class VinylDiscWidget;
class QScrollArea;
class QListWidget;
class QLabel;
class QVBoxLayout;
class QSlider;
class QPushButton;
class QPropertyAnimation;

class DrawerBackdrop : public QWidget
{
public:
    std::function<void()> clickHandler;

    explicit DrawerBackdrop(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        hide();
    }

protected:
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
};

/**
 * @brief 音乐模块：QQ 风格播放页 + 右侧半透明歌曲抽屉
 */
class MusicPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPageWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildPlayerPage();
    void buildPlaylistDrawer();
    void refreshPlaylist();
    void togglePlaylistDrawer();
    void setPlaylistDrawerOpen(bool open, bool animated = true);
    void layoutPlaylistDrawer(bool animated);
    int drawerWidth() const;
    void updateProgressUi(qint64 position, qint64 duration);
    void loadLyricsForCurrentTrack();
    void updateLyricHighlight(qint64 positionMs);
    void updateLyricPadding();
    void scrollLyricToIndex(int index, bool animated = true);
    static QString formatTime(qint64 ms);

    QPropertyAnimation *m_lyricScrollAnim = nullptr;
    QPropertyAnimation *m_drawerAnim = nullptr;

    MusicPlayer *m_player = nullptr;
    QWidget *m_playerRoot = nullptr;
    DrawerBackdrop *m_drawerBackdrop = nullptr;
    QWidget *m_playlistDrawer = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_listHint = nullptr;
    QPushButton *m_playlistToggleBtn = nullptr;
    VinylDiscWidget *m_vinyl = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_artistLabel = nullptr;
    QScrollArea *m_lyricScroll = nullptr;
    QWidget *m_lyricPanel = nullptr;
    QVBoxLayout *m_lyricLayout = nullptr;
    QWidget *m_lyricTopPad = nullptr;
    QWidget *m_lyricBottomPad = nullptr;
    QVector<QLabel *> m_lyricLabels;
    QLabel *m_timeCurrent = nullptr;
    QLabel *m_timeTotal = nullptr;
    QSlider *m_progress = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QPushButton *m_playBtn = nullptr;
    QLabel *m_drawerCountLabel = nullptr;

    QVector<QPair<qint64, QString>> m_lyrics;
    int m_activeLyric = -1;
    bool m_seeking = false;
    bool m_drawerOpen = false;
};

#endif // MUSICPAGE_H
