#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QStringList>

class QMediaPlayer;
class QMediaPlaylist;
class QProcess;
class QTimer;

/**
 * @brief 本地音乐播放后端（QMediaPlayer + 播放列表）
 */
class MusicPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer() override;

    QStringList scanMusicFiles() const;
    bool hasTracks() const;
    int currentIndex() const;
    QString currentTitle() const;
    QString currentPath() const;
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;

public slots:
    void setPlaylist(const QStringList &paths);
    void playIndex(int index);
    void playPause();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 ms);
    void setVolume(int percent);

signals:
    void playlistChanged(int count);
    void trackChanged(int index, const QString &title, const QString &path);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playingChanged(bool playing);
    void errorOccurred(const QString &message);

private:
    QString titleFromPath(const QString &path) const;
    void connectPlayerSignals();

    QMediaPlayer *m_player = nullptr;
    QMediaPlaylist *m_playlist = nullptr;
    QProcess *m_process = nullptr;
    QTimer *m_positionTimer = nullptr;
    QStringList m_paths;
    QByteArray m_processBuffer;
    int m_currentIndex = -1;
    bool m_processPlaying = false;
    qint64 m_processPosition = 0;
    qint64 m_processDuration = 0;
    int m_volume = 80;
};

#endif // MUSICPLAYER_H
