#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QImage>
#include <QRect>
#include <QStringList>

class QProcess;
class QTimer;
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
struct _GstElement;
typedef struct _GstElement GstElement;
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
class QAbstractVideoSurface;
class QMediaPlayer;
class QWidget;
#endif

class VideoPlayer : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer() override;

    QStringList scanVideoFiles() const;
    bool hasVideos() const;
    int currentIndex() const;
    QString currentTitle() const;
    QString currentPath() const;
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;

    void setVideoGeometry(const QRect &geometry);
    void setExternalAudioEnabled(bool enabled);
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    void setVideoOutput(QAbstractVideoSurface *output);
    void setEmbedWidget(QWidget *widget);
#endif

public slots:
    void setPlaylist(const QStringList &paths);
    void playIndex(int index);
    void playPause();
    void stop();
    void next();
    void previous();
    void seek(qint64 ms);
    void setVolume(int percent);

signals:
    void playlistChanged(int count);
    void videoChanged(int index, const QString &title, const QString &path);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playingChanged(bool playing);
    void errorOccurred(const QString &message);
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    void frameReady(const QImage &image);
#endif

private:
    QString titleFromPath(const QString &path) const;
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    static void initializeGStreamer();
    bool startGStreamer(const QString &path);
    void stopGStreamer();
    void startGStreamerAudio(const QString &path);
    void stopGStreamerAudio();
    void pollGStreamer();
    void pullGstFrame();
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
    QString findExternalPlayer() const;
    QStringList externalPlayerArguments(const QString &playerPath, const QString &videoPath) const;
    bool startExternalPlayer(const QString &reason);
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    void tryEmbedExternalWindow();
#endif
    void sendCommand(const QByteArray &command);
    void restartProcessAt(qint64 positionMs, bool playing);
    void queryPosition();
    void parseProcessOutput();

#if defined(USE_QT_MULTIMEDIA_BACKEND)
    QMediaPlayer *m_mediaPlayer = nullptr;
    QAbstractVideoSurface *m_videoOutput = nullptr;
    QWidget *m_embedWidget = nullptr;
    QTimer *m_embedTimer = nullptr;
    quintptr m_externalWindow = 0;
#endif
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    GstElement *m_gstPipeline = nullptr;
    GstElement *m_gstAppSink = nullptr;
    QTimer *m_gstTimer = nullptr;
#endif
    QProcess *m_process = nullptr;
    QTimer *m_positionTimer = nullptr;
    QStringList m_paths;
    QByteArray m_processBuffer;
    QRect m_videoGeometry;
    int m_currentIndex = -1;
    bool m_externalPlaying = false;
    bool m_triedExternalFallback = false;
    bool m_processPlaying = false;
    bool m_gstPlaying = false;
    bool m_restartingForGeometry = false;
    bool m_stopping = false;
    qint64 m_processPosition = 0;
    qint64 m_processDuration = 0;
    int m_volume = 80;
    bool m_externalAudioEnabled = true;
};

#endif // VIDEOPLAYER_H
