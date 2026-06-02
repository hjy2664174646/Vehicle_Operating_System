#include "musicplayer.h"

#if defined(USE_MPLAYER_BACKEND) || defined(USE_MUSIC_MPLAYER_BACKEND)
#define MUSIC_USE_MPLAYER
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#if !defined(MUSIC_USE_MPLAYER)
#include <QMediaPlayer>
#include <QMediaPlaylist>
#endif
#include <QProcess>
#include <QTimer>
#include <QUrl>

namespace {

const char *kAudioSuffixes[] = {
    "*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.aac",
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

QStringList defaultMusicDirs()
{
    QStringList dirs;
    const QString appDir = QCoreApplication::applicationDirPath();

#if defined(MUSIC_USE_MPLAYER)
    dirs << QStringLiteral("/music")
         << QStringLiteral("/mnt/sdcard/music")
         << QStringLiteral("/udisk/music")
         << QStringLiteral("/mnt/music");
#else
    dirs << appDir + QStringLiteral("/music")
         << appDir + QStringLiteral("/../music")
         << appDir + QStringLiteral("/../../music");
#endif

    dirs << appDir + QStringLiteral("/music");
    return dirs;
}

} // namespace

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
#if !defined(MUSIC_USE_MPLAYER)
    , m_player(new QMediaPlayer(this))
    , m_playlist(new QMediaPlaylist(this))
#endif
{
#if defined(MUSIC_USE_MPLAYER)
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(1000);

    connect(m_positionTimer, &QTimer::timeout, this, [this]() {
        if (!m_processPlaying)
            return;
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->write("pausing_keep_force get_time_pos\n");
            if (m_processDuration <= 0)
                m_process->write("pausing_keep_force get_time_length\n");
            m_process->waitForBytesWritten(20);
        }
    });

    connect(m_process, &QProcess::readyRead, this, [this]() {
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
            }
        }
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (!m_processPlaying)
            return;
        m_processPlaying = false;
        m_positionTimer->stop();
        emit playingChanged(false);
    });
#else
    m_player->setPlaylist(m_playlist);
    m_player->setVolume(80);
    connectPlayerSignals();
#endif
}

MusicPlayer::~MusicPlayer() = default;

void MusicPlayer::connectPlayerSignals()
{
#if defined(MUSIC_USE_MPLAYER)
    return;
#else
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, [this](int index) {
        if (index < 0 || index >= m_paths.size())
            return;
        emit trackChanged(index, currentTitle(), currentPath());
    });

    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MusicPlayer::durationChanged);

    connect(m_player, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });

    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
            [this](QMediaPlayer::Error) {
        emit errorOccurred(m_player->errorString());
    });
#endif
}

QString MusicPlayer::titleFromPath(const QString &path) const
{
    QString title = QFileInfo(path).completeBaseName();
#if defined(MUSIC_USE_MPLAYER)
    const QString utf8Title = QString::fromUtf8(title.toLatin1());
    if (!containsCjk(title) && containsCjk(utf8Title) && !utf8Title.contains(QChar(0xFFFD)))
        title = utf8Title;
#endif
    return title;
}

QStringList MusicPlayer::scanMusicFiles() const
{
    QStringList files;
    const QStringList dirs = defaultMusicDirs();

    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        for (const char *pattern : kAudioSuffixes) {
            const QStringList names = dir.entryList(QStringList(QString::fromUtf8(pattern)),
                                                    QDir::Files, QDir::Name);
            for (const QString &name : names) {
                const QString abs = dir.absoluteFilePath(name);
                if (!files.contains(abs))
                    files.append(abs);
            }
        }
    }

    files.sort(Qt::CaseInsensitive);
    return files;
}

bool MusicPlayer::hasTracks() const
{
    return !m_paths.isEmpty();
}

int MusicPlayer::currentIndex() const
{
#if defined(MUSIC_USE_MPLAYER)
    return m_currentIndex;
#else
    return m_playlist->currentIndex();
#endif
}

QString MusicPlayer::currentTitle() const
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return QString();
    return titleFromPath(m_paths.at(m_currentIndex));
#else
    const int idx = m_playlist->currentIndex();
    if (idx < 0 || idx >= m_paths.size())
        return QString();
    return titleFromPath(m_paths.at(idx));
#endif
}

QString MusicPlayer::currentPath() const
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_currentIndex < 0 || m_currentIndex >= m_paths.size())
        return QString();
    return m_paths.at(m_currentIndex);
#else
    const int idx = m_playlist->currentIndex();
    if (idx < 0 || idx >= m_paths.size())
        return QString();
    return m_paths.at(idx);
#endif
}

qint64 MusicPlayer::position() const
{
#if defined(MUSIC_USE_MPLAYER)
    return m_processPosition;
#else
    return m_player->position();
#endif
}

qint64 MusicPlayer::duration() const
{
#if defined(MUSIC_USE_MPLAYER)
    return m_processDuration;
#else
    return m_player->duration();
#endif
}

bool MusicPlayer::isPlaying() const
{
#if defined(MUSIC_USE_MPLAYER)
    return m_processPlaying;
#else
    return m_player->state() == QMediaPlayer::PlayingState;
#endif
}

void MusicPlayer::setPlaylist(const QStringList &paths)
{
    m_paths = paths;
#if defined(MUSIC_USE_MPLAYER)
    m_currentIndex = m_paths.isEmpty() ? -1 : 0;
#else
    m_playlist->clear();

    for (const QString &path : m_paths)
        m_playlist->addMedia(QUrl::fromLocalFile(path));
#endif

    emit playlistChanged(m_paths.size());
    if (!m_paths.isEmpty())
        emit trackChanged(0, titleFromPath(m_paths.first()), m_paths.first());
}

void MusicPlayer::playIndex(int index)
{
    if (index < 0 || index >= m_paths.size())
        return;
#if defined(MUSIC_USE_MPLAYER)
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(800);
    }

    m_currentIndex = index;
    m_processPosition = 0;
    m_processDuration = 0;
    m_processBuffer.clear();
    emit trackChanged(index, currentTitle(), currentPath());
    emit positionChanged(0);
    emit durationChanged(0);

    QStringList args;
    args << QStringLiteral("-slave")
         << QStringLiteral("-quiet")
         << QStringLiteral("-ao")
         << QStringLiteral("alsa")
         << QStringLiteral("-softvol")
         << QStringLiteral("-softvol-max")
         << QStringLiteral("100")
         << QStringLiteral("-volume")
         << QString::number(m_volume)
         << m_paths.at(index);

    m_process->start(QStringLiteral("/bin/mplayer"), args);
    if (!m_process->waitForStarted(1200)) {
        m_processPlaying = false;
        emit playingChanged(false);
        emit errorOccurred(QStringLiteral("mplayer 启动失败"));
        return;
    }

    m_processPlaying = true;
    m_process->write(QStringLiteral("pausing_keep_force set_property volume %1\n").arg(m_volume).toUtf8());
    m_process->write(QStringLiteral("pausing_keep_force volume %1 1\n").arg(m_volume).toUtf8());
    m_process->waitForBytesWritten(50);
    m_positionTimer->start();
    m_process->write("pausing_keep_force get_time_length\n");
    m_process->write("pausing_keep_force get_time_pos\n");
    m_process->waitForBytesWritten(50);
    emit playingChanged(true);
#else
    m_playlist->setCurrentIndex(index);
    m_player->play();
#endif
}

void MusicPlayer::playPause()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_paths.isEmpty())
        return;

    if (m_currentIndex < 0) {
        playIndex(0);
        return;
    }

    if (!m_process || m_process->state() == QProcess::NotRunning) {
        playIndex(m_currentIndex);
        return;
    }

    m_process->write("pause\n");
    m_processPlaying = !m_processPlaying;
    if (m_processPlaying)
        m_positionTimer->start();
    else
        m_positionTimer->stop();
    emit playingChanged(m_processPlaying);
#else
    if (m_player->state() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
#endif
}

void MusicPlayer::play()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_currentIndex < 0)
        playIndex(0);
    else if (!m_processPlaying)
        playPause();
#else
    m_player->play();
#endif
}

void MusicPlayer::pause()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_processPlaying)
        playPause();
#else
    m_player->pause();
#endif
}

void MusicPlayer::stop()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
    m_processPlaying = false;
    m_positionTimer->stop();
    emit playingChanged(false);
#else
    m_player->stop();
#endif
}

void MusicPlayer::next()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_paths.isEmpty())
        return;
    const int nextIndex = (m_currentIndex + 1 + m_paths.size()) % m_paths.size();
    playIndex(nextIndex);
#else
    if (m_playlist->mediaCount() <= 0)
        return;
    m_playlist->next();
    m_player->play();
#endif
}

void MusicPlayer::previous()
{
#if defined(MUSIC_USE_MPLAYER)
    if (m_paths.isEmpty())
        return;
    const int prevIndex = (m_currentIndex - 1 + m_paths.size()) % m_paths.size();
    playIndex(prevIndex);
#else
    if (m_playlist->mediaCount() <= 0)
        return;
    m_playlist->previous();
    m_player->play();
#endif
}

void MusicPlayer::seek(qint64 ms)
{
#if defined(MUSIC_USE_MPLAYER)
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;

    const qint64 boundedMs = m_processDuration > 0 ? qBound(qint64(0), ms, m_processDuration) : qMax(qint64(0), ms);
    m_processPosition = boundedMs;
    emit positionChanged(m_processPosition);
    m_process->write(QStringLiteral("pausing_keep_force seek %1 2\n")
                         .arg(double(boundedMs) / 1000.0, 0, 'f', 3)
                         .toUtf8());
    m_process->waitForBytesWritten(50);
#else
    m_player->setPosition(ms);
#endif
}

void MusicPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
#if defined(MUSIC_USE_MPLAYER)
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->write(QStringLiteral("pausing_keep_force set_property volume %1\n").arg(m_volume).toUtf8());
        m_process->write(QStringLiteral("pausing_keep_force volume %1 1\n").arg(m_volume).toUtf8());
        m_process->waitForBytesWritten(50);
    }
#else
    m_player->setVolume(m_volume);
#endif
}
