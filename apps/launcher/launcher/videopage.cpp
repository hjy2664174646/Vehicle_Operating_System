#include "videopage.h"

#include "musicprogressslider.h"
#include "videoplayer.h"

#include <QCoreApplication>
#include <QAbstractAnimation>
#include <QDebug>
#include <QFileInfo>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>
#if defined(USE_QT_MULTIMEDIA_BACKEND)
#include <QAbstractVideoSurface>
#include <QImage>
#include <QVideoFrame>
#include <QVideoSurfaceFormat>
#endif
#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace {

constexpr const char *kIconPlay = ":/picture/play.png";
constexpr const char *kIconStop = ":/picture/stop.png";
constexpr const char *kIconLast = ":/picture/last.png";
constexpr const char *kIconNext = ":/picture/next.png";
constexpr const char *kIconList = ":/picture/list.png";
constexpr const char *kIconMax = ":/picture/max.png";
constexpr const char *kIconExitMax = ":/picture/exit_max.png";

void paintVideoBackground(QPainter &p, const QRect &rect)
{
#if defined(__arm__)
    p.fillRect(rect, QColor(QStringLiteral("#07090d")));
    QRadialGradient glow(QPointF(rect.width() * 0.36, rect.height() * 0.34),
                         qreal(qMax(rect.width(), rect.height())) * 0.62);
    glow.setColorAt(0.0, QColor(QStringLiteral("#18202a")));
    glow.setColorAt(0.55, QColor(QStringLiteral("#0b1016")));
    glow.setColorAt(1.0, QColor(QStringLiteral("#050608")));
    p.fillRect(rect, glow);
#else
    p.fillRect(rect, QColor(QStringLiteral("#07090d")));
    QRadialGradient glow(QPointF(rect.width() * 0.35, rect.height() * 0.32),
                         qreal(qMax(rect.width(), rect.height())) * 0.58);
    glow.setColorAt(0.0, QColor(38, 52, 68, 210));
    glow.setColorAt(0.48, QColor(12, 18, 25, 145));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);
#endif
}

#if defined(USE_QT_MULTIMEDIA_BACKEND)
QString panelStyle()
{
#if defined(__arm__)
    return QStringLiteral("background-color: #10141a; border: 1px solid #29313b; border-radius: 8px;");
#else
    return QStringLiteral("background-color: rgba(16,20,26,0.86); border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;");
#endif
}
#endif

QString listStyle()
{
    return QStringLiteral(
        "QListWidget { background: transparent; border: none; color: #DDE4EA; font-size: 14px; padding: 4px; }"
        "QListWidget::item { padding: 10px 8px; border-radius: 6px; }"
        "QListWidget::item:selected { background-color: rgba(90, 166, 255, 0.24); color: #8EC5FF; }");
}

QString drawerPanelStyle()
{
#if defined(__arm__)
    return QStringLiteral("background-color: #10141a; border-left: 1px solid #29313b;");
#else
    return QStringLiteral("background-color: rgba(16,20,26,0.92); border-left: 1px solid rgba(255,255,255,0.12);");
#endif
}

QString bottomBarStyle()
{
#if defined(__arm__)
    return QStringLiteral("background-color: #0d1118; border-top: 1px solid #242b35;");
#else
    return QStringLiteral("background-color: rgba(13,17,24,0.92); border-top: 1px solid rgba(255,255,255,0.08);");
#endif
}

QString pictureButtonStyle()
{
#if defined(__arm__)
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:pressed { background-color: #26303a; border-radius: 20px; }");
#else
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:pressed { background-color: rgba(255,255,255,0.13); border-radius: 20px; }");
#endif
}

QPushButton *makePictureButton(const QString &iconPath, int iconSize, QWidget *parent)
{
    auto *btn = new QPushButton(parent);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(iconSize, iconSize));
    const int pad = 8;
    btn->setFixedSize(iconSize + pad, iconSize + pad);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(pictureButtonStyle());
    return btn;
}

#if defined(USE_QT_MULTIMEDIA_BACKEND)
class VideoFrameWidget;

class VideoFrameSurface : public QAbstractVideoSurface
{
public:
    explicit VideoFrameSurface(VideoFrameWidget *widget);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override;
    bool present(const QVideoFrame &frame) override;

private:
    VideoFrameWidget *m_widget = nullptr;
};

class VideoFrameWidget : public QWidget
{
public:
    explicit VideoFrameWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_surface(new VideoFrameSurface(this))
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
    }

    QAbstractVideoSurface *surface() const { return m_surface; }

    void setFrameImage(const QImage &image)
    {
        m_image = image;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.fillRect(rect(), QColor(QStringLiteral("#020305")));
        if (m_image.isNull())
            return;

        const QSize scaledSize = m_image.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target((width() - scaledSize.width()) / 2,
                           (height() - scaledSize.height()) / 2,
                           scaledSize.width(),
                           scaledSize.height());
        p.drawImage(target, m_image);
    }

private:
    VideoFrameSurface *m_surface = nullptr;
    QImage m_image;
};

VideoFrameSurface::VideoFrameSurface(VideoFrameWidget *widget)
    : QAbstractVideoSurface(widget)
    , m_widget(widget)
{
}

QList<QVideoFrame::PixelFormat> VideoFrameSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    if (handleType != QAbstractVideoBuffer::NoHandle)
        return {};

    return {
        QVideoFrame::Format_RGB32,
        QVideoFrame::Format_ARGB32,
        QVideoFrame::Format_ARGB32_Premultiplied,
        QVideoFrame::Format_RGB24,
        QVideoFrame::Format_BGR24,
        QVideoFrame::Format_RGB565,
        QVideoFrame::Format_RGB555
    };
}

bool VideoFrameSurface::present(const QVideoFrame &frame)
{
    qDebug() << "VideoFrameSurface::present" << frame.width() << frame.height() << frame.pixelFormat();
    if (!m_widget)
        return false;

    QVideoFrame copy(frame);
    if (!copy.map(QAbstractVideoBuffer::ReadOnly))
        return false;

    QImage image = QVideoFrame::imageFormatFromPixelFormat(copy.pixelFormat()) == QImage::Format_Invalid
        ? QImage()
        : QImage(copy.bits(),
                 copy.width(),
                 copy.height(),
                 copy.bytesPerLine(),
                 QVideoFrame::imageFormatFromPixelFormat(copy.pixelFormat())).copy();
    copy.unmap();

    if (image.isNull())
        return false;

    m_widget->setFrameImage(image);
    return true;
}
#endif

QString sliderStyle(const QString &accent)
{
    return QStringLiteral(
        "QSlider { background: transparent; }"
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: rgba(255,255,255,0.18); }"
        "QSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: %1; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px;"
        "  background: #F2F7FF; border: 1px solid %1; }").arg(accent);
}

} // namespace

VideoPageWidget::VideoPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_player(new VideoPlayer(this))
{
    setAutoFillBackground(false);
    buildUi();
    refreshPlaylist();

#if defined(USE_QT_MULTIMEDIA_BACKEND) && defined(Q_OS_WIN)
    m_fullscreenClickTimer = new QTimer(this);
    m_fullscreenClickTimer->setInterval(60);
    connect(m_fullscreenClickTimer, &QTimer::timeout, this, [this]() {
        if (!m_fullscreenMode)
            return;
        const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (pressed && !m_fullscreenMouseDown) {
            m_fullscreenMouseDown = true;
        } else if (!pressed && m_fullscreenMouseDown) {
            m_fullscreenMouseDown = false;
            const QPoint localPos = mapFromGlobal(QCursor::pos());
            if (rect().contains(localPos))
                toggleFullscreenControls();
        }
    });
#endif

    connect(m_player, &VideoPlayer::videoChanged, this, [this](int index, const QString &title, const QString &path) {
        m_titleLabel->setText(title.isEmpty() ? QString::fromUtf8("未选择视频") : title);
        m_pathLabel->setText(path);
        if (index >= 0 && index < m_list->count()) {
            const bool blocked = m_list->blockSignals(true);
            m_list->setCurrentRow(index);
            m_list->blockSignals(blocked);
        }
        showVideoMessage(QString());
    });

    connect(m_player, &VideoPlayer::positionChanged, this, [this](qint64 pos) {
        if (!m_seeking)
            updateProgressUi(pos, m_player->duration());
    });

    connect(m_player, &VideoPlayer::durationChanged, this, [this](qint64 dur) {
        updateProgressUi(m_player->position(), dur);
    });

    connect(m_player, &VideoPlayer::playingChanged, this, [this](bool playing) {
        m_playBtn->setIcon(QIcon(QString::fromUtf8(playing ? kIconStop : kIconPlay)));
        if (playing) {
            showVideoMessage(QString());
#if defined(USE_MPLAYER_BACKEND)
            setChromeVisible(true);
#endif
        } else {
#if defined(USE_MPLAYER_BACKEND)
            setChromeVisible(true);
#endif
        }
    });

    connect(m_player, &VideoPlayer::errorOccurred, this, [this](const QString &message) {
        if (message.contains(QStringLiteral("已嵌入外部播放器"))) {
            showLoadingIndicator();
            return;
        }
        showVideoMessage(QString::fromUtf8("播放错误：%1").arg(message));
    });
}

void VideoPageWidget::stopPlayback()
{
    m_player->stop();
#if defined(USE_MPLAYER_BACKEND)
    setChromeVisible(true);
#endif
}

void VideoPageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
#if defined(USE_MPLAYER_BACKEND)
    if (m_player && m_player->isPlaying() && m_videoArea && m_videoArea->isVisible()) {
        QRegion paintRegion(rect());
        const QRect videoHole(m_videoArea->mapTo(this, QPoint(0, 0)), m_videoArea->size());
        paintRegion = paintRegion.subtracted(QRegion(videoHole.adjusted(-2, -2, 2, 2)));
        p.setClipRegion(paintRegion);
    }
#endif
    paintVideoBackground(p, rect());
}

void VideoPageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fullscreenMode) {
        updateFullscreenLayout();
        updateVideoGeometry();
        return;
    }

    if (m_listToggleBtn) {
        if (!m_drawerOpen)
            m_listToggleBtn->raise();
    }
    layoutVideoDrawer(false);
    updateVideoGeometry();
}

void VideoPageWidget::hideEvent(QHideEvent *event)
{
    m_player->stop();
    if (m_fullscreenMode)
        setFullscreenMode(false);
    QWidget::hideEvent(event);
}

bool VideoPageWidget::eventFilter(QObject *watched, QEvent *event)
{
    const bool fullscreenClickTarget = watched == m_clickCatcher
        || watched == m_fullscreenLayer
        || watched == m_videoArea
#if defined(USE_QT_MULTIMEDIA_BACKEND)
        || watched == m_videoWidget
#endif
        ;

    if (fullscreenClickTarget && m_fullscreenMode && event->type() == QEvent::MouseButtonPress) {
        toggleFullscreenControls();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void VideoPageWidget::buildUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(16, 14, 16, 0);
    m_rootLayout->setSpacing(10);

    auto *mainRow = new QHBoxLayout;
    mainRow->setSpacing(12);

    m_leftCol = new QWidget(this);
    m_leftCol->setAutoFillBackground(false);
    m_leftLayout = new QVBoxLayout(m_leftCol);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(8);

    m_videoArea = new QWidget(m_leftCol);
    m_videoArea->setMinimumSize(420, 250);
#if defined(USE_MPLAYER_BACKEND)
    m_videoArea->setAutoFillBackground(false);
    m_videoArea->setAttribute(Qt::WA_NoSystemBackground, true);
    m_videoArea->setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_videoArea->setStyleSheet(QStringLiteral("background: transparent;"));
#else
    m_videoArea->setStyleSheet(panelStyle() + QStringLiteral("background-color: #020305;"));
    m_videoArea->setAttribute(Qt::WA_StyledBackground, true);
#endif
    m_videoArea->installEventFilter(this);

    auto *videoLayout = new QVBoxLayout(m_videoArea);
#if defined(USE_QT_MULTIMEDIA_BACKEND)
    videoLayout->setContentsMargins(0, 0, 0, 0);
    auto *videoFrameWidget = new VideoFrameWidget(m_videoArea);
    m_videoWidget = videoFrameWidget;
    m_videoWidget->setStyleSheet(QStringLiteral("background: #020305; border-radius: 8px;"));
    videoLayout->addWidget(m_videoWidget);
#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    connect(m_player, &VideoPlayer::frameReady, videoFrameWidget, [videoFrameWidget](const QImage &image) {
        videoFrameWidget->setFrameImage(image);
    });
#else
    m_player->setVideoOutput(videoFrameWidget->surface());
#endif
    m_player->setEmbedWidget(m_videoArea);
    m_hintLabel = new QLabel(QString::fromUtf8("视频预览"), m_videoArea);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral(
        "color: #9FB8D4; font-size: 16px; background-color: rgba(0,0,0,72);"
        "border: none; border-radius: 18px; padding: 8px;"));
    m_hintLabel->hide();
#else
    videoLayout->setContentsMargins(18, 18, 18, 18);
    videoLayout->addStretch();
    m_hintLabel = new QLabel(QString::fromUtf8("视频预览"), m_videoArea);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #5E6A76; font-size: 18px; background: transparent;"));
    videoLayout->addWidget(m_hintLabel);
    videoLayout->addStretch();
#endif

    m_titleLabel = new QLabel(QString::fromUtf8("未选择视频"), m_leftCol);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral("color: #F3F7FA; font-size: 18px; font-weight: bold; background: transparent;"));
    m_titleLabel->hide();

    m_pathLabel = new QLabel(m_leftCol);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet(QStringLiteral("color: #7E8B98; font-size: 11px; background: transparent;"));
    m_pathLabel->hide();

    m_leftLayout->addWidget(m_videoArea, 1);
    m_leftLayout->addWidget(m_titleLabel);
    m_leftLayout->addWidget(m_pathLabel);

    m_fullscreenLayer = new QWidget(this);
    m_fullscreenLayer->setAttribute(Qt::WA_StyledBackground, true);
    m_fullscreenLayer->setStyleSheet(QStringLiteral("background-color: #020305;"));
    m_fullscreenLayer->installEventFilter(this);
    m_fullscreenLayer->hide();

    m_clickCatcher = new QWidget(m_fullscreenLayer);
    m_clickCatcher->setAttribute(Qt::WA_TranslucentBackground, true);
    m_clickCatcher->setStyleSheet(QStringLiteral("background: transparent;"));
    m_clickCatcher->installEventFilter(this);
    m_clickCatcher->hide();

    m_listPanel = new QWidget(this);
    m_listPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_listPanel->setStyleSheet(drawerPanelStyle());
    m_listPanel->hide();
    auto *listLayout = new QVBoxLayout(m_listPanel);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(8);

    auto *listTitle = new QLabel(QString::fromUtf8("视频列表"), m_listPanel);
    listTitle->setStyleSheet(QStringLiteral("color: #EAF2FA; font-size: 17px; font-weight: bold; background: transparent;"));

    m_list = new QListWidget(m_listPanel);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setStyleSheet(listStyle());

    listLayout->addWidget(listTitle);
    listLayout->addWidget(m_list, 1);

    mainRow->addWidget(m_leftCol, 1);
    m_rootLayout->addLayout(mainRow, 1);

    m_bottomBar = new QWidget(this);
    m_bottomBar->setFixedHeight(116);
    m_bottomBar->setAttribute(Qt::WA_StyledBackground, true);
    m_bottomBar->setStyleSheet(bottomBarStyle());
    auto *bottomLayout = new QVBoxLayout(m_bottomBar);
    bottomLayout->setContentsMargins(0, 7, 0, 8);
    bottomLayout->setSpacing(4);

    m_progress = new GradientProgressSlider(m_bottomBar);
    m_progress->setRange(0, 1000);
    m_progress->setFixedHeight(20);
    bottomLayout->addWidget(m_progress);

    auto *timeRow = new QHBoxLayout;
    m_timeCurrent = new QLabel(QStringLiteral("00:00"), m_bottomBar);
    m_timeTotal = new QLabel(QStringLiteral("00:00"), m_bottomBar);
    const QString timeStyle = QStringLiteral("color: #8D98A4; font-size: 11px; background: transparent;");
    m_timeCurrent->setStyleSheet(timeStyle);
    m_timeTotal->setStyleSheet(timeStyle);
    timeRow->addWidget(m_timeCurrent);
    timeRow->addStretch();
    timeRow->addWidget(m_timeTotal);
    bottomLayout->addLayout(timeRow);

    auto *ctrlRow = new QHBoxLayout;
    ctrlRow->setContentsMargins(0, 0, 0, 0);
    ctrlRow->setSpacing(12);

    auto *prevBtn = makePictureButton(QString::fromUtf8(kIconLast), 30, m_bottomBar);
    m_playBtn = makePictureButton(QString::fromUtf8(kIconPlay), 44, m_bottomBar);
    auto *nextBtn = makePictureButton(QString::fromUtf8(kIconNext), 30, m_bottomBar);
    m_fullscreenBtn = makePictureButton(QString::fromUtf8(kIconMax), 24, m_bottomBar);

    auto *volumeLabel = new QLabel(QString::fromUtf8("音量"), m_bottomBar);
    volumeLabel->setStyleSheet(QStringLiteral("color: #C7D0D9; font-size: 14px; background: transparent;"));
    m_volumeSlider = new QSlider(Qt::Horizontal, m_bottomBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedSize(96, 20);
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setStyleSheet(sliderStyle(QStringLiteral("#5AA6FF")));

    m_listToggleBtn = makePictureButton(QString::fromUtf8(kIconList), 24, m_bottomBar);
    m_listToggleBtn->setFixedSize(36, 36);
    connect(m_listToggleBtn, &QPushButton::clicked, this, &VideoPageWidget::toggleVideoDrawer);

    ctrlRow->addStretch();
    ctrlRow->addWidget(prevBtn, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(m_playBtn, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(nextBtn, 0, Qt::AlignVCenter);
    ctrlRow->addSpacing(24);
    ctrlRow->addWidget(volumeLabel, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(m_volumeSlider, 0, Qt::AlignVCenter);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_listToggleBtn, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(m_fullscreenBtn, 0, Qt::AlignVCenter);
    bottomLayout->addLayout(ctrlRow);

    m_rootLayout->addWidget(m_bottomBar);

    connect(prevBtn, &QPushButton::clicked, m_player, &VideoPlayer::previous);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, [this]() { setFullscreenMode(!m_fullscreenMode); });
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (!m_player->hasVideos())
            return;
        updateVideoGeometry();
        const int row = m_list->currentRow();
        if (!m_player->isPlaying() && row >= 0 && row != m_player->currentIndex())
            m_player->playIndex(row);
        else
            m_player->playPause();
    });
    connect(nextBtn, &QPushButton::clicked, m_player, &VideoPlayer::next);
    connect(m_volumeSlider, &QSlider::valueChanged, m_player, &VideoPlayer::setVolume);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        setVideoDrawerOpen(false, false);
        updateVideoGeometry();
        m_player->playIndex(item->data(Qt::UserRole).toInt());
    });

    connect(m_progress, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });

    connect(m_progress, &QSlider::sliderMoved, this, [this](int value) {
        const qint64 dur = m_player->duration();
        if (dur > 0)
            m_timeCurrent->setText(formatTime(qint64(value) * dur / 1000));
    });

    connect(m_progress, &QSlider::sliderReleased, this, [this]() {
        const qint64 dur = m_player->duration();
        if (dur > 0)
            m_player->seek(qint64(m_progress->value()) * dur / 1000);
        m_seeking = false;
    });

    connect(m_progress, &QSlider::actionTriggered, this, [this](int action) {
        if (action == QSlider::SliderPageStepAdd || action == QSlider::SliderPageStepSub) {
            const qint64 dur = m_player->duration();
            if (dur > 0)
                m_player->seek(qint64(m_progress->value()) * dur / 1000);
        }
    });
}

void VideoPageWidget::refreshPlaylist()
{
    const QStringList files = m_player->scanVideoFiles();
    m_player->setPlaylist(files);

    m_list->clear();
    for (int i = 0; i < files.size(); ++i) {
        auto *item = new QListWidgetItem(QFileInfo(files.at(i)).completeBaseName(), m_list);
        item->setData(Qt::UserRole, i);
    }

    if (files.isEmpty()) {
        m_hintLabel->setText(QString::fromUtf8("未找到视频"));
        m_pathLabel->setText(QString::fromUtf8("请将 mp4/avi/mkv 等视频放入 /video 或程序目录的 video 文件夹"));
    } else {
        m_hintLabel->setText(QString::fromUtf8("视频预览"));
        m_list->setCurrentRow(0);
    }
}

int VideoPageWidget::drawerWidth() const
{
    return qMin(240, qMax(190, width() / 3));
}

void VideoPageWidget::toggleVideoDrawer()
{
    setVideoDrawerOpen(!m_drawerOpen, true);
}

void VideoPageWidget::setVideoDrawerOpen(bool open, bool animated)
{
    m_drawerOpen = open;
    layoutVideoDrawer(animated);

    if (m_listPanel) {
        if (open) {
            m_listPanel->raise();
            if (m_bottomBar)
                m_bottomBar->raise();
        } else if (m_listToggleBtn) {
            m_listToggleBtn->raise();
        }
    }
}

void VideoPageWidget::layoutVideoDrawer(bool animated)
{
    if (!m_listPanel)
        return;

    const int panelW = drawerWidth();
    const int bottomReserved = m_fullscreenMode ? (m_fullscreenControlsVisible && m_bottomBar ? m_bottomBar->height() : 0)
                                                : (m_bottomBar ? m_bottomBar->height() : 0);
    const int panelH = qMax(120, height() - bottomReserved - 24);
    const int y = 8;
    const QRect openRect(width() - panelW, y, panelW, panelH);
    const QRect closedRect(width(), y, panelW, panelH);
    const QRect target = m_drawerOpen ? openRect : closedRect;

    if (!animated) {
        if (m_drawerAnim) {
            m_drawerAnim->stop();
            m_drawerAnim->deleteLater();
            m_drawerAnim = nullptr;
        }
        m_listPanel->setGeometry(target);
        m_listPanel->setVisible(m_drawerOpen);
        return;
    }

    if (m_drawerAnim) {
        m_drawerAnim->stop();
        m_drawerAnim->deleteLater();
        m_drawerAnim = nullptr;
    }

    m_listPanel->show();
    m_listPanel->raise();
    if (m_bottomBar)
        m_bottomBar->raise();
    m_drawerAnim = new QPropertyAnimation(m_listPanel, "geometry", this);
    m_drawerAnim->setDuration(180);
    m_drawerAnim->setStartValue(m_listPanel->geometry());
    m_drawerAnim->setEndValue(target);
    connect(m_drawerAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_drawerOpen)
            m_listPanel->hide();
        if (m_listToggleBtn && !m_drawerOpen)
            m_listToggleBtn->raise();
        m_drawerAnim = nullptr;
    });
    m_drawerAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void VideoPageWidget::setFullscreenMode(bool fullscreen)
{
    if (m_fullscreenMode == fullscreen)
        return;

    m_fullscreenMode = fullscreen;
    m_fullscreenControlsVisible = !fullscreen;
    setVideoDrawerOpen(false, false);

    if (fullscreen) {
        if (m_rootLayout)
            m_rootLayout->removeWidget(m_bottomBar);
        if (m_leftLayout)
            m_leftLayout->removeWidget(m_videoArea);

        if (m_fullscreenLayer) {
            m_fullscreenLayer->setGeometry(rect());
            m_fullscreenLayer->show();
            m_fullscreenLayer->raise();
        }

        m_videoArea->setParent(m_fullscreenLayer);
        m_videoArea->show();
        m_videoArea->raise();

        if (m_fullscreenBtn) {
            m_fullscreenBtn->setIcon(QIcon(QString::fromUtf8(kIconExitMax)));
            m_fullscreenBtn->setIconSize(QSize(24, 24));
        }
        if (m_bottomBar) {
            m_bottomBar->setParent(m_fullscreenLayer);
            m_bottomBar->hide();
        }
        if (m_leftCol)
            m_leftCol->hide();
#if defined(USE_QT_MULTIMEDIA_BACKEND)
        m_fullscreenMouseDown = false;
        if (m_fullscreenClickTimer)
            m_fullscreenClickTimer->start();
#endif
    } else {
#if defined(USE_QT_MULTIMEDIA_BACKEND)
        if (m_fullscreenClickTimer)
            m_fullscreenClickTimer->stop();
        m_fullscreenMouseDown = false;
#endif
        m_videoArea->setParent(m_leftCol);
        if (m_leftLayout)
            m_leftLayout->insertWidget(0, m_videoArea, 1);
        if (m_bottomBar) {
            m_bottomBar->setParent(this);
            if (m_rootLayout)
                m_rootLayout->addWidget(m_bottomBar);
            m_bottomBar->show();
        }
        if (m_fullscreenBtn) {
            m_fullscreenBtn->setIcon(QIcon(QString::fromUtf8(kIconMax)));
            m_fullscreenBtn->setIconSize(QSize(24, 24));
        }
        if (m_clickCatcher)
            m_clickCatcher->hide();
        if (m_fullscreenLayer)
            m_fullscreenLayer->hide();
        if (m_leftCol)
            m_leftCol->show();
        m_videoArea->show();
    }

    updateFullscreenLayout();
    updateVideoGeometry();
}

void VideoPageWidget::updateFullscreenLayout()
{
    if (!m_fullscreenMode || !m_videoArea)
        return;

    if (!m_fullscreenLayer)
        return;

    m_fullscreenLayer->setGeometry(rect());
    m_fullscreenLayer->raise();

    const QRect layerRect = m_fullscreenLayer->rect();
    QRect videoRect = layerRect;
#if defined(USE_MPLAYER_BACKEND)
    if (m_bottomBar && m_fullscreenControlsVisible)
        videoRect.setBottom(qMax(videoRect.top(), layerRect.bottom() - m_bottomBar->height()));
#endif
    m_videoArea->setGeometry(videoRect);
    m_videoArea->show();
    m_videoArea->raise();

    if (m_clickCatcher) {
        m_clickCatcher->setGeometry(layerRect);
        m_clickCatcher->show();
        m_clickCatcher->raise();
    }

    if (m_bottomBar && m_fullscreenControlsVisible) {
        const int h = m_bottomBar->height();
        m_bottomBar->setGeometry(0, layerRect.height() - h, layerRect.width(), h);
        m_bottomBar->show();
        m_bottomBar->raise();
    }
}

void VideoPageWidget::setFullscreenControlsVisible(bool visible)
{
    if (!m_fullscreenMode || !m_bottomBar)
        return;

    m_fullscreenControlsVisible = visible;
    m_bottomBar->setVisible(visible);
    if (visible)
        m_bottomBar->show();
    updateFullscreenLayout();
    updateVideoGeometry();
    if (m_clickCatcher)
        m_clickCatcher->raise();
    if (visible) {
        m_bottomBar->raise();
        m_bottomBar->update();
        m_bottomBar->repaint();
    }
}

void VideoPageWidget::toggleFullscreenControls()
{
    setFullscreenControlsVisible(!m_fullscreenControlsVisible);
}

void VideoPageWidget::setChromeVisible(bool visible)
{
    if (m_bottomBar)
        m_bottomBar->setVisible(visible);
    if (m_listPanel && !visible)
        m_listPanel->hide();
    if (visible) {
        if (m_bottomBar)
            m_bottomBar->raise();
        update();
    }
}

void VideoPageWidget::updateProgressUi(qint64 position, qint64 duration)
{
    m_timeCurrent->setText(formatTime(position));
    m_timeTotal->setText(formatTime(duration));

    if (duration > 0 && !m_seeking)
        m_progress->setValue(int(position * 1000 / duration));
    else if (duration <= 0)
        m_progress->setValue(0);
}

void VideoPageWidget::showVideoMessage(const QString &message)
{
    if (!m_hintLabel)
        return;

    if (m_loadingTimer)
        m_loadingTimer->stop();
    m_hintLabel->setPixmap(QPixmap());
    m_hintLabel->setMinimumSize(0, 0);
    m_hintLabel->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    if (message.isEmpty()) {
        m_hintLabel->hide();
        return;
    }

    m_hintLabel->setText(message);
    m_hintLabel->adjustSize();
    const int maxW = qMax(120, m_videoArea->width() - 48);
    if (m_hintLabel->width() > maxW) {
        m_hintLabel->setFixedWidth(maxW);
        m_hintLabel->setWordWrap(true);
        m_hintLabel->adjustSize();
    }
    m_hintLabel->move((m_videoArea->width() - m_hintLabel->width()) / 2,
                      (m_videoArea->height() - m_hintLabel->height()) / 2);
    m_hintLabel->raise();
    m_hintLabel->show();
}

QPixmap VideoPageWidget::loadingPixmap() const
{
    const int size = 42;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center(size / 2.0, size / 2.0);
    const qreal radius = 14.0;
    for (int i = 0; i < 12; ++i) {
        const int alpha = 55 + ((i + m_loadingFrame) % 12) * 16;
        p.setPen(QPen(QColor(142, 197, 255, qMin(alpha, 255)), 3, Qt::SolidLine, Qt::RoundCap));
        const qreal angle = (i * 30.0 - 90.0) * 3.14159265358979323846 / 180.0;
        const QPointF outer(center.x() + qCos(angle) * radius, center.y() + qSin(angle) * radius);
        const QPointF inner(center.x() + qCos(angle) * (radius - 6), center.y() + qSin(angle) * (radius - 6));
        p.drawLine(inner, outer);
    }
    return pixmap;
}

void VideoPageWidget::showLoadingIndicator()
{
    if (!m_hintLabel)
        return;

    if (!m_loadingTimer) {
        m_loadingTimer = new QTimer(this);
        m_loadingTimer->setInterval(80);
        connect(m_loadingTimer, &QTimer::timeout, this, [this]() {
            ++m_loadingFrame;
            if (!m_hintLabel)
                return;
            m_hintLabel->setPixmap(loadingPixmap());
            m_hintLabel->adjustSize();
            m_hintLabel->move((m_videoArea->width() - m_hintLabel->width()) / 2,
                              (m_videoArea->height() - m_hintLabel->height()) / 2);
            m_hintLabel->raise();
        });
    }

    m_hintLabel->setText(QString());
    m_hintLabel->setMinimumSize(0, 0);
    m_hintLabel->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    m_hintLabel->setWordWrap(false);
    m_hintLabel->setPixmap(loadingPixmap());
    m_hintLabel->adjustSize();
    m_hintLabel->move((m_videoArea->width() - m_hintLabel->width()) / 2,
                      (m_videoArea->height() - m_hintLabel->height()) / 2);
    m_hintLabel->raise();
    m_hintLabel->show();
    m_loadingTimer->start();
    QTimer::singleShot(1800, this, [this]() {
        if (m_loadingTimer)
            m_loadingTimer->stop();
        if (m_hintLabel && !m_player->isPlaying())
            m_hintLabel->hide();
    });
}

void VideoPageWidget::updateVideoGeometry()
{
    if (!m_videoArea)
        return;
#if defined(__arm__)
    const QPoint topLeft = m_videoArea->mapTo(window(), QPoint(0, 0));
    QRect globalRect(topLeft, m_videoArea->size());
#else
    QRect globalRect(m_videoArea->mapToGlobal(QPoint(0, 0)), m_videoArea->size());
#endif
    globalRect.adjust(2, 2, -2, -2);
#if defined(USE_MPLAYER_BACKEND)
    qDebug() << "video geometry:" << globalRect << "window:" << window()->geometry();
#endif
    m_player->setVideoGeometry(globalRect);
    if (m_hintLabel && m_hintLabel->isVisible())
        showVideoMessage(m_hintLabel->text());
}

QString VideoPageWidget::formatTime(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const int totalSec = int(ms / 1000);
    const int hour = totalSec / 3600;
    const int min = (totalSec / 60) % 60;
    const int sec = totalSec % 60;
    if (hour > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hour, 2, 10, QLatin1Char('0'))
            .arg(min, 2, 10, QLatin1Char('0'))
            .arg(sec, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'));
}
