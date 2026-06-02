#include "videoplayer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>
#if defined(__arm__)
#include <cstdlib>
#endif
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
#include <QAbstractVideoSurface>
#include <QMediaPlayer>
#include <QUrl>
#endif

#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace {

const char *kVideoSuffixes[] = {
    "*.mp4", "*.avi", "*.mkv", "*.mov", "*.flv", "*.wmv", "*.mpeg", "*.mpg", "*.rmvb",
};

bool containsCjk(const QString &text)
{
    for (const QChar &ch : text) {
        const ushort u = ch.unicode();
        if ((u >= 0x3400 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF))
            return true;
    }
    return false;
}

QStringList defaultVideoDirs()
{
    QStringList dirs;
    const QString appDir = QCoreApplication::applicationDirPath();

#if defined(__arm__)
    dirs << QStringLiteral("/video")
         << QStringLiteral("/mnt/sdcard/video")
         << QStringLiteral("/udisk/video")
         << QStringLiteral("/mnt/video");
#else
    dirs << QStringLiteral("/video")
         << QStringLiteral("/mnt/sdcard/video")
         << QStringLiteral("/udisk/video")
         << QStringLiteral("/mnt/video")
         << appDir + QStringLiteral("/video")
         << appDir + QStringLiteral("/../video")
         << appDir + QStringLiteral("/../../video");
#endif

    return dirs;
}

#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
struct EmbedSearchContext {
    DWORD pid = 0;
    HWND hwnd = nullptr;
};

BOOL CALLBACK findWindowForProcess(HWND hwnd, LPARAM lParam)
{
    auto *context = reinterpret_cast<EmbedSearchContext *>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != context->pid || !IsWindowVisible(hwnd))
        return TRUE;
    if (GetWindow(hwnd, GW_OWNER))
        return TRUE;
    context->hwnd = hwnd;
    return FALSE;
}
#endif

} // namespace

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent)
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_embedTimer(new QTimer(this))
#endif
    , m_process(new QProcess(this))
    , m_positionTimer(new QTimer(this))
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    initializeGStreamer();
    m_gstTimer = new QTimer(this);
    m_gstTimer->setInterval(40);
    connect(m_gstTimer, &QTimer::timeout, this, &VideoPlayer::pollGStreamer);
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    m_mediaPlayer->setVolume(m_volume);
    m_embedTimer->setInterval(120);
    connect(m_embedTimer, &QTimer::timeout, this, &VideoPlayer::tryEmbedExternalWindow);

    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoPlayer::positionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoPlayer::durationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state) {
        if (!m_externalPlaying)
            emit playingChanged(state == QMediaPlayer::PlayingState);
    });
    connect(m_mediaPlayer, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
            this, [this](QMediaPlayer::Error) {
        if (m_mediaPlayer->error() == QMediaPlayer::NoError)
            return;

        QString message = m_mediaPlayer->errorString().trimmed();
        if (message.isEmpty()) {
            switch (m_mediaPlayer->error()) {
            case QMediaPlayer::FormatError:
                message = QString::fromUtf8("当前视频编码不受 Windows Qt 播放后端支持");
                break;
            case QMediaPlayer::ResourceError:
                message = QString::fromUtf8("视频文件无法打开或系统缺少对应解码器");
                break;
            case QMediaPlayer::ServiceMissingError:
                message = QString::fromUtf8("Qt 多媒体播放服务不可用");
                break;
            case QMediaPlayer::NetworkError:
                message = QString::fromUtf8("网络媒体错误");
                break;
            case QMediaPlayer::AccessDeniedError:
                message = QString::fromUtf8("没有权限读取视频文件");
                break;
            default:
                message = QString::fromUtf8("PC 端解码失败");
                break;
            }
        }

#if defined(Q_OS_WIN)
        if (!m_triedExternalFallback && startExternalPlayer(message))
            return;
#endif

        emit errorOccurred(message);
    });
#endif

#if defined(USE_MPLAYER_BACKEND)
    m_process->setProcessChannelMode(QProcess::MergedChannels);
#if defined(__arm__)
    std::system("killall -q mplayer 2>/dev/null");
#endif
    m_positionTimer->setInterval(1000);
    connect(m_positionTimer, &QTimer::timeout, this, &VideoPlayer::queryPosition);
    connect(m_process, &QProcess::readyRead, this, &VideoPlayer::parseProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (!m_processPlaying)
            return;
        m_processPlaying = false;
        m_positionTimer->stop();
        emit playingChanged(false);
    });
#endif
}

VideoPlayer::~VideoPlayer()
{
    stop();
}

QString VideoPlayer::titleFromPath(const QString &path) const
{
    QString title = QFileInfo(path).completeBaseName();
    const QString utf8Title = QString::fromUtf8(title.toLatin1());
    if (!containsCjk(title) && containsCjk(utf8Title) && !utf8Title.contains(QChar(0xFFFD)))
        title = utf8Title;
    return title;
}

QStringList VideoPlayer::scanVideoFiles() const
{
    QStringList files;
    QSet<QString> seenDirs;
    QSet<QString> seen;
    for (const QString &dirPath : defaultVideoDirs()) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        QString canonicalDir = dir.canonicalPath();
        if (canonicalDir.isEmpty())
            canonicalDir = dir.absolutePath();
        canonicalDir = QDir::cleanPath(canonicalDir).toLower();
        if (seenDirs.contains(canonicalDir))
            continue;
        seenDirs.insert(canonicalDir);

        for (const char *pattern : kVideoSuffixes) {
            const QStringList names = dir.entryList(QStringList(QString::fromUtf8(pattern)),
                                                    QDir::Files, QDir::Name);
            for (const QString &name : names) {
                const QFileInfo info(dir.absoluteFilePath(name));
                QString abs = info.canonicalFilePath();
                if (abs.isEmpty())
                    abs = info.absoluteFilePath();
                if (!seen.contains(abs)) {
                    seen.insert(abs);
                    files.append(abs);
                }
            }
        }
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}

bool VideoPlayer::hasVideos() const
{
    return !m_paths.isEmpty();
}

int VideoPlayer::currentIndex() const
{
    return m_currentIndex;
}

QString VideoPlayer::currentTitle() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return QString();
    return titleFromPath(m_paths.at(m_currentIndex));
}

QString VideoPlayer::currentPath() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return QString();
    return m_paths.at(m_currentIndex);
}

qint64 VideoPlayer::position() const
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline)
        return m_processPosition;
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    return m_externalPlaying ? m_processPosition : m_mediaPlayer->position();
#else
    return m_processPosition;
#endif
}

qint64 VideoPlayer::duration() const
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline)
        return m_processDuration;
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    return m_externalPlaying ? m_processDuration : m_mediaPlayer->duration();
#else
    return m_processDuration;
#endif
}

bool VideoPlayer::isPlaying() const
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline)
        return m_gstPlaying;
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    return m_externalPlaying || m_mediaPlayer->state() == QMediaPlayer::PlayingState;
#else
    return m_processPlaying;
#endif
}

void VideoPlayer::setExternalAudioEnabled(bool enabled)
{
    m_externalAudioEnabled = enabled;
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (!enabled)
        stopGStreamerAudio();
#endif
}

void VideoPlayer::setVideoGeometry(const QRect &geometry)
{
#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
    m_videoGeometry = geometry;
    if (m_externalPlaying)
        tryEmbedExternalWindow();
#elif defined(USE_MPLAYER_BACKEND)
    if (m_stopping)
        return;
    const bool changed = m_videoGeometry.isValid()
        && geometry.isValid()
        && m_videoGeometry != geometry;
    m_videoGeometry = geometry;
    if (changed && !m_restartingForGeometry
        && m_process && m_process->state() != QProcess::NotRunning && m_currentIndex >= 0)
        restartProcessAt(m_processPosition, m_processPlaying);
#else
    m_videoGeometry = geometry;
#endif
}

#if defined(USE_QT_MULTIMEDIA_BACKEND)
void VideoPlayer::setVideoOutput(QAbstractVideoSurface *output)
{
    m_videoOutput = output;
    m_mediaPlayer->setVideoOutput(m_videoOutput);
}

void VideoPlayer::setEmbedWidget(QWidget *widget)
{
    m_embedWidget = widget;
}
#endif

void VideoPlayer::setPlaylist(const QStringList &paths)
{
    m_paths = paths;
    m_currentIndex = m_paths.isEmpty() ? -1 : 0;
    emit playlistChanged(m_paths.size());
    if (!m_paths.isEmpty())
        emit videoChanged(0, titleFromPath(m_paths.first()), m_paths.first());
}

void VideoPlayer::playIndex(int index)
{
    if (index < 0 || index >= m_paths.size())
        return;

#if defined(USE_QT_MULTIMEDIA_BACKEND)
    stop();
    m_currentIndex = index;
    m_triedExternalFallback = false;
    m_processPosition = 0;
    m_processDuration = 0;
    emit videoChanged(index, currentTitle(), currentPath());
    emit positionChanged(0);
    emit durationChanged(0);
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (startGStreamer(m_paths.at(index)))
        return;
#endif
    m_mediaPlayer->setMedia(QUrl::fromLocalFile(m_paths.at(index)));
    m_mediaPlayer->play();
    return;
#else
    stop();
    m_currentIndex = index;
    m_processPosition = 0;
    m_processDuration = 0;
    m_processBuffer.clear();
    emit videoChanged(index, currentTitle(), currentPath());
    emit positionChanged(0);
    emit durationChanged(0);

    QStringList args;
    args << QStringLiteral("-slave")
         << QStringLiteral("-nofs")
         << QStringLiteral("-noaspect")
         << QStringLiteral("-framedrop")
         << QStringLiteral("-lavdopts") << QStringLiteral("fast:skiploopfilter=all")
         << QStringLiteral("-autosync") << QStringLiteral("30")
         << QStringLiteral("-ao") << QStringLiteral("alsa")
         << QStringLiteral("-softvol")
         << QStringLiteral("-softvol-max") << QStringLiteral("100")
         << QStringLiteral("-volume") << QString::number(m_volume);

    if (m_videoGeometry.isValid() && m_videoGeometry.width() > 80 && m_videoGeometry.height() > 60) {
        const int w = m_videoGeometry.width();
        const int h = m_videoGeometry.height();
        const int screenW = 800;
        const int screenH = 480;
        const int x = qBound(0, m_videoGeometry.x(), screenW - 1);
        const int y = qBound(0, m_videoGeometry.y(), screenH - 1);
        const int boundedW = qMin(w, screenW - x);
        const int boundedH = qMin(h, screenH - y);

        args << QStringLiteral("-vo") << QStringLiteral("fbdev")
             << QStringLiteral("-bpp") << QStringLiteral("32")
             << QStringLiteral("-zoom")
             << QStringLiteral("-x") << QString::number(boundedW)
             << QStringLiteral("-y") << QString::number(boundedH)
             << QStringLiteral("-geometry")
             << QStringLiteral("%1:%2").arg(x).arg(y)
             << QStringLiteral("-vf")
             << QStringLiteral("scale=%1:%2,dsize=%1:%2,expand=%1:%2")
                    .arg(boundedW)
                    .arg(boundedH);

        args << QStringLiteral("-screenw")
             << QString::number(screenW)
             << QStringLiteral("-screenh")
             << QString::number(screenH);
    } else {
        args << QStringLiteral("-vo") << QStringLiteral("fbdev")
             << QStringLiteral("-bpp") << QStringLiteral("32");
    }

    args << m_paths.at(index);
#if defined(USE_MPLAYER_BACKEND)
    qDebug() << "mplayer args:" << args;
#endif
    m_process->start(QStringLiteral("/bin/mplayer"), args);
    if (!m_process->waitForStarted(1200)) {
        m_processPlaying = false;
        emit playingChanged(false);
        emit errorOccurred(QStringLiteral("mplayer start failed"));
        return;
    }

    m_processPlaying = true;
    sendCommand(QStringLiteral("pausing_keep_force set_property volume %1\n").arg(m_volume).toUtf8());
    sendCommand(QStringLiteral("pausing_keep_force volume %1 1\n").arg(m_volume).toUtf8());
#if !defined(__arm__)
    m_positionTimer->start();
    queryPosition();
#endif
    emit playingChanged(true);
#endif
}

void VideoPlayer::playPause()
{
    if (m_paths.isEmpty())
        return;

#if defined(USE_QT_MULTIMEDIA_BACKEND)
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline) {
        const GstState targetState = m_gstPlaying ? GST_STATE_PAUSED : GST_STATE_PLAYING;
        if (gst_element_set_state(m_gstPipeline, targetState) != GST_STATE_CHANGE_FAILURE) {
            if (m_process && m_process->state() != QProcess::NotRunning)
                sendCommand("pause\n");
            m_gstPlaying = !m_gstPlaying;
            emit playingChanged(m_gstPlaying);
        }
        return;
    }
#endif
    if (m_externalPlaying) {
        stop();
        return;
    }

    if (m_currentIndex < 0) {
        playIndex(0);
        return;
    }

    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
    } else if (m_mediaPlayer->mediaStatus() == QMediaPlayer::NoMedia
               || m_mediaPlayer->mediaStatus() == QMediaPlayer::InvalidMedia) {
        playIndex(m_currentIndex);
    } else {
        m_mediaPlayer->play();
    }
    return;
#else
    if (m_currentIndex < 0) {
        playIndex(0);
        return;
    }

    if (!m_process || m_process->state() == QProcess::NotRunning) {
        playIndex(m_currentIndex);
        return;
    }

    sendCommand("pause\n");
    m_processPlaying = !m_processPlaying;
    if (m_processPlaying) {
#if !defined(__arm__)
        m_positionTimer->start();
        queryPosition();
#endif
    } else {
        m_positionTimer->stop();
    }
    emit playingChanged(m_processPlaying);
#endif
}

void VideoPlayer::stop()
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    stopGStreamer();
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    if (m_mediaPlayer)
        m_mediaPlayer->stop();
    if (m_embedTimer)
        m_embedTimer->stop();
    m_externalWindow = 0;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
            m_process->kill();
            m_process->waitForFinished(500);
        }
    }
    m_externalPlaying = false;
    emit playingChanged(false);
#else
    m_stopping = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        sendCommand("quit\n");
        m_process->waitForFinished(120);
        if (m_process->state() != QProcess::NotRunning)
            m_process->terminate();
        m_process->waitForFinished(120);
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
        m_process->waitForFinished(300);
    }
#if defined(__arm__)
    std::system("killall -q mplayer 2>/dev/null");
#endif
    m_processPlaying = false;
    m_positionTimer->stop();
    emit playingChanged(false);
    m_stopping = false;
#endif
}

void VideoPlayer::next()
{
    if (m_paths.isEmpty())
        return;
    playIndex((m_currentIndex + 1 + m_paths.size()) % m_paths.size());
}

void VideoPlayer::previous()
{
    if (m_paths.isEmpty())
        return;
    playIndex((m_currentIndex - 1 + m_paths.size()) % m_paths.size());
}

void VideoPlayer::seek(qint64 ms)
{
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline) {
        const qint64 boundedMs = m_processDuration > 0 ? qBound(qint64(0), ms, m_processDuration) : qMax(qint64(0), ms);
        gst_element_seek_simple(m_gstPipeline, GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                boundedMs * GST_MSECOND);
        m_processPosition = boundedMs;
        emit positionChanged(m_processPosition);
        if (m_process && m_process->state() != QProcess::NotRunning) {
            sendCommand(QStringLiteral("pausing_keep_force seek %1 2\n")
                            .arg(double(boundedMs) / 1000.0, 0, 'f', 3)
                            .toUtf8());
        }
        return;
    }
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    if (!m_externalPlaying)
        m_mediaPlayer->setPosition(qMax(qint64(0), ms));
#else
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;

    const qint64 boundedMs = m_processDuration > 0 ? qBound(qint64(0), ms, m_processDuration) : qMax(qint64(0), ms);
    m_processPosition = boundedMs;
    emit positionChanged(m_processPosition);
    sendCommand(QStringLiteral("pausing_keep_force seek %1 2\n")
                    .arg(double(boundedMs) / 1000.0, 0, 'f', 3)
                    .toUtf8());
#endif
}

void VideoPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    if (m_gstPipeline && m_process && m_process->state() != QProcess::NotRunning) {
        sendCommand(QStringLiteral("pausing_keep_force set_property volume %1\n").arg(m_volume).toUtf8());
        sendCommand(QStringLiteral("pausing_keep_force volume %1 1\n").arg(m_volume).toUtf8());
    }
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    m_mediaPlayer->setVolume(m_volume);
#else
    if (m_process && m_process->state() != QProcess::NotRunning) {
        sendCommand(QStringLiteral("pausing_keep_force set_property volume %1\n").arg(m_volume).toUtf8());
        sendCommand(QStringLiteral("pausing_keep_force volume %1 1\n").arg(m_volume).toUtf8());
    }
#endif
}

#if defined(USE_GSTREAMER_VIDEO_BACKEND)
void VideoPlayer::initializeGStreamer()
{
    static bool initialized = false;
    if (initialized)
        return;
    int argc = 0;
    char **argv = nullptr;
    gst_init(&argc, &argv);
    initialized = true;
}

void VideoPlayer::pullGstFrame()
{
    if (!m_gstAppSink)
        return;

    GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(m_gstAppSink), 0);
    if (!sample)
        return;

    GstCaps *caps = gst_sample_get_caps(sample);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstVideoInfo info;
    if (!caps || !buffer || !gst_video_info_from_caps(&info, caps)) {
        gst_sample_unref(sample);
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return;
    }

    const int width = GST_VIDEO_INFO_WIDTH(&info);
    const int height = GST_VIDEO_INFO_HEIGHT(&info);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    QImage image(map.data, width, height, stride, QImage::Format_RGB888);
    emit frameReady(image.copy());

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
}

bool VideoPlayer::startGStreamer(const QString &path)
{
    stopGStreamer();

    {
        const int targetW = m_videoGeometry.isValid()
            ? qBound(160, m_videoGeometry.width(), 800)
            : 400;
        const int targetH = m_videoGeometry.isValid()
            ? qBound(120, m_videoGeometry.height(), 480)
            : 240;
        const QByteArray uri = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toEncoded();
        const QString pipelineDescription = QStringLiteral(
            "uridecodebin uri=\"%1\" caps=video/x-raw "
            "! queue max-size-buffers=3 leaky=downstream "
            "! videoconvert "
            "! videoscale "
            "! video/x-raw,format=RGB,width=%2,height=%3 "
            "! appsink name=qt-video-appsink sync=true max-buffers=2 drop=true emit-signals=false")
            .arg(QString::fromLatin1(uri))
            .arg(targetW)
            .arg(targetH);

        GError *parseError = nullptr;
        m_gstPipeline = gst_parse_launch(pipelineDescription.toUtf8().constData(), &parseError);
        if (parseError) {
            const QString message = QString::fromUtf8(parseError->message);
            g_error_free(parseError);
            stopGStreamer();
            emit errorOccurred(message);
            return false;
        }

        m_gstAppSink = gst_bin_get_by_name(GST_BIN(m_gstPipeline), "qt-video-appsink");
        if (!m_gstPipeline || !m_gstAppSink) {
            stopGStreamer();
            emit errorOccurred(QString::fromUtf8("GStreamer video pipeline create failed"));
            return false;
        }

        const GstStateChangeReturn ret = gst_element_set_state(m_gstPipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            stopGStreamer();
            emit errorOccurred(QString::fromUtf8("GStreamer video start failed"));
            return false;
        }

        m_processPlaying = true;
        m_gstPlaying = true;
        startGStreamerAudio(path);
        if (m_gstTimer)
            m_gstTimer->start();
        emit playingChanged(true);
        return true;
    }

    m_gstPipeline = gst_element_factory_make("playbin", "qt-video-playbin");
    m_gstAppSink = gst_element_factory_make("appsink", "qt-video-appsink");
    GstElement *audioSink = gst_element_factory_make("fakesink", "qt-video-audio-fakesink");

    if (!m_gstPipeline || !m_gstAppSink) {
        stopGStreamer();
        emit errorOccurred(QString::fromUtf8("GStreamer 播放组件创建失败"));
        return false;
    }

    GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGB");
    g_object_set(G_OBJECT(m_gstAppSink),
                 "emit-signals", FALSE,
                 "sync", TRUE,
                 "max-buffers", 2,
                 "drop", TRUE,
                 "caps", caps,
                 nullptr);
    gst_caps_unref(caps);

    const QByteArray uri = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toEncoded();
    g_object_set(G_OBJECT(m_gstPipeline),
                 "uri", uri.constData(),
                 "video-sink", m_gstAppSink,
                 "volume", double(m_volume) / 100.0,
                 nullptr);
    if (audioSink)
        g_object_set(G_OBJECT(m_gstPipeline), "audio-sink", audioSink, nullptr);

    const GstStateChangeReturn ret = gst_element_set_state(m_gstPipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        stopGStreamer();
        emit errorOccurred(QString::fromUtf8("GStreamer 播放启动失败"));
        return false;
    }

    m_processPlaying = true;
    m_gstPlaying = true;
    startGStreamerAudio(path);
    if (m_gstTimer)
        m_gstTimer->start();
    emit playingChanged(true);
    return true;
}

void VideoPlayer::stopGStreamer()
{
    if (m_gstTimer)
        m_gstTimer->stop();
    stopGStreamerAudio();
    if (m_gstPipeline) {
        gst_element_set_state(m_gstPipeline, GST_STATE_NULL);
        if (m_gstAppSink) {
            gst_object_unref(GST_OBJECT(m_gstAppSink));
            m_gstAppSink = nullptr;
        }
        gst_object_unref(GST_OBJECT(m_gstPipeline));
        m_gstPipeline = nullptr;
    }
    m_processPlaying = false;
    m_gstPlaying = false;
}

void VideoPlayer::startGStreamerAudio(const QString &path)
{
    if (!m_externalAudioEnabled)
        return;

    if (!m_process)
        return;

    stopGStreamerAudio();

    QStringList args;
    args << QStringLiteral("-slave")
         << QStringLiteral("-quiet")
         << QStringLiteral("-novideo")
         << QStringLiteral("-ao") << QStringLiteral("alsa")
         << QStringLiteral("-softvol")
         << QStringLiteral("-softvol-max") << QStringLiteral("100")
         << QStringLiteral("-volume") << QString::number(m_volume)
         << QFileInfo(path).absoluteFilePath();

    m_process->start(QStringLiteral("/bin/mplayer"), args);
    if (!m_process->waitForStarted(1200))
        qDebug() << "mplayer audio failed to start";
}

void VideoPlayer::stopGStreamerAudio()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished(250)) {
        m_process->kill();
        m_process->waitForFinished(250);
    }
}

void VideoPlayer::pollGStreamer()
{
    if (!m_gstPipeline)
        return;

    GstBus *bus = gst_element_get_bus(m_gstPipeline);
    while (bus) {
        GstMessage *msg = gst_bus_pop_filtered(bus, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (!msg)
            break;

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError *err = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            const QString message = err ? QString::fromUtf8(err->message) : QString::fromUtf8("GStreamer 播放错误");
            if (err)
                g_error_free(err);
            if (debug)
                g_free(debug);
            gst_message_unref(msg);
            stopGStreamer();
            emit errorOccurred(message);
            return;
        }

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            gst_message_unref(msg);
            stopGStreamer();
            emit playingChanged(false);
            return;
        }

        gst_message_unref(msg);
    }
    if (bus)
        gst_object_unref(bus);

    pullGstFrame();

    gint64 pos = 0;
    gint64 dur = 0;
    if (gst_element_query_position(m_gstPipeline, GST_FORMAT_TIME, &pos)) {
        m_processPosition = pos / GST_MSECOND;
        emit positionChanged(m_processPosition);
    }
    if (gst_element_query_duration(m_gstPipeline, GST_FORMAT_TIME, &dur)) {
        m_processDuration = dur / GST_MSECOND;
        emit durationChanged(m_processDuration);
    }
}
#endif

#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
QString VideoPlayer::findExternalPlayer() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList localCandidates = {
        appDir + QStringLiteral("/ffplay.exe"),
        appDir + QStringLiteral("/mplayer.exe"),
        appDir + QStringLiteral("/mpv.exe"),
        appDir + QStringLiteral("/vlc.exe"),
        appDir + QStringLiteral("/tools/ffplay.exe"),
        appDir + QStringLiteral("/tools/mplayer.exe"),
        appDir + QStringLiteral("/tools/mpv.exe"),
        appDir + QStringLiteral("/tools/vlc.exe"),
    };

    for (const QString &path : localCandidates) {
        if (QFileInfo::exists(path))
            return path;
    }

    const QStringList pathCandidates = {
        QStringLiteral("ffplay.exe"),
        QStringLiteral("mplayer.exe"),
        QStringLiteral("mpv.exe"),
        QStringLiteral("vlc.exe"),
        QStringLiteral("ffplay"),
        QStringLiteral("mplayer"),
        QStringLiteral("mpv"),
        QStringLiteral("vlc"),
    };

    for (const QString &name : pathCandidates) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
            return found;
    }

    const QStringList vlcCandidates = {
        QStringLiteral("C:/Program Files/VideoLAN/VLC/vlc.exe"),
        QStringLiteral("C:/Program Files (x86)/VideoLAN/VLC/vlc.exe"),
    };

    for (const QString &path : vlcCandidates) {
        if (QFileInfo::exists(path))
            return path;
    }

    return QString();
}

QStringList VideoPlayer::externalPlayerArguments(const QString &playerPath, const QString &videoPath) const
{
    const QString name = QFileInfo(playerPath).fileName().toLower();
    if (name.contains(QStringLiteral("ffplay"))) {
        return QStringList()
            << QStringLiteral("-noborder")
            << QStringLiteral("-autoexit")
            << QStringLiteral("-window_title") << currentTitle()
            << videoPath;
    }

    if (name.contains(QStringLiteral("mplayer"))) {
        return QStringList()
            << QStringLiteral("-quiet")
            << QStringLiteral("-ao") << QStringLiteral("dsound")
            << videoPath;
    }

    if (name.contains(QStringLiteral("mpv")))
        return QStringList() << videoPath;

    if (name.contains(QStringLiteral("vlc"))) {
        return QStringList()
            << QStringLiteral("--play-and-exit")
            << QStringLiteral("--no-video-title-show")
            << videoPath;
    }

    return QStringList() << videoPath;
}

bool VideoPlayer::startExternalPlayer(const QString &reason)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return false;

    const QString playerPath = findExternalPlayer();
    if (playerPath.isEmpty()) {
        emit errorOccurred(reason + QString::fromUtf8("；未找到 VLC/ffplay/mplayer/mpv 兜底播放器"));
        return false;
    }

    m_triedExternalFallback = true;
    m_externalWindow = 0;
    m_mediaPlayer->stop();

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }

    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (m_embedTimer)
            m_embedTimer->stop();
        if (!m_externalPlaying)
            return;
        m_externalPlaying = false;
        emit playingChanged(false);
    }, Qt::UniqueConnection);

    m_process->start(playerPath, externalPlayerArguments(playerPath, currentPath()));
    if (!m_process->waitForStarted(1200)) {
        emit errorOccurred(reason + QString::fromUtf8("；外部播放器启动失败：") + playerPath);
        return false;
    }

    m_externalPlaying = true;
    if (m_embedTimer)
        m_embedTimer->start();
    emit playingChanged(true);
    emit errorOccurred(QString::fromUtf8("Qt 解码失败，已嵌入外部播放器：%1").arg(QFileInfo(playerPath).fileName()));
    return true;
}
#endif

#if defined(USE_QT_MULTIMEDIA_BACKEND)
void VideoPlayer::tryEmbedExternalWindow()
{
#if defined(Q_OS_WIN)
    if (!m_externalPlaying || !m_embedWidget || !m_process || m_process->state() == QProcess::NotRunning)
        return;

    HWND parent = HWND(m_embedWidget->winId());
    HWND playerWindow = HWND(m_externalWindow);
    if (!playerWindow) {
        EmbedSearchContext context;
        context.pid = DWORD(m_process->processId());
        EnumWindows(findWindowForProcess, reinterpret_cast<LPARAM>(&context));
        if (!context.hwnd)
            return;
        playerWindow = context.hwnd;
        m_externalWindow = quintptr(playerWindow);
    }

    SetParent(playerWindow, parent);

    LONG_PTR style = GetWindowLongPtr(playerWindow, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_POPUP);
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLongPtr(playerWindow, GWL_STYLE, style);

    const QSize size = m_embedWidget->size();
    MoveWindow(playerWindow, 0, 0, qMax(1, size.width()), qMax(1, size.height()), TRUE);
    ShowWindow(playerWindow, SW_SHOW);
    UpdateWindow(playerWindow);
    if (m_externalWindow)
        m_embedTimer->stop();
#endif
}
#endif

void VideoPlayer::sendCommand(const QByteArray &command)
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    m_process->write(command);
    m_process->waitForBytesWritten(50);
}

void VideoPlayer::restartProcessAt(qint64 positionMs, bool playing)
{
#if defined(USE_MPLAYER_BACKEND)
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return;

    const int index = m_currentIndex;
    const qint64 boundedPosition = qMax(qint64(0), positionMs);
    m_restartingForGeometry = true;
    playIndex(index);
    m_restartingForGeometry = false;
    if (boundedPosition > 1000)
        seek(boundedPosition);
    if (!playing && m_process && m_process->state() != QProcess::NotRunning) {
        sendCommand("pause\n");
        m_processPlaying = false;
        m_positionTimer->stop();
        emit playingChanged(false);
    }
#else
    Q_UNUSED(positionMs);
    Q_UNUSED(playing);
#endif
}

void VideoPlayer::queryPosition()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    sendCommand("pausing_keep_force get_time_pos\n");
    if (m_processDuration <= 0)
        sendCommand("pausing_keep_force get_time_length\n");
}

void VideoPlayer::parseProcessOutput()
{
    m_processBuffer.append(m_process->readAll());

    int lineEnd = -1;
    while ((lineEnd = m_processBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_processBuffer.left(lineEnd).trimmed();
        m_processBuffer.remove(0, lineEnd + 1);

        if (line.startsWith("ANS_TIME_POSITION=")) {
            bool ok = false;
            const double seconds = line.mid(sizeof("ANS_TIME_POSITION=") - 1).toDouble(&ok);
            if (ok) {
                m_processPosition = qint64(seconds * 1000.0);
                emit positionChanged(m_processPosition);
            }
        } else if (line.startsWith("ANS_LENGTH=")) {
            bool ok = false;
            const double seconds = line.mid(sizeof("ANS_LENGTH=") - 1).toDouble(&ok);
            if (ok) {
                m_processDuration = qint64(seconds * 1000.0);
                emit durationChanged(m_processDuration);
            }
        } else if (!line.isEmpty()) {
            qDebug() << "mplayer:" << QString::fromLocal8Bit(line);
        }
    }
}
