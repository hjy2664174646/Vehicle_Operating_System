#include "bootanimationpage.h"

#include "videoplayer.h"

#include <QConicalGradient>
#include <QDebug>
#include <QFileInfo>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QtMath>

BootAnimationPage::BootAnimationPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAutoFillBackground(false);

    const QStringList labels = {
        QString::fromUtf8("音乐"),
        QString::fromUtf8("地图"),
        QString::fromUtf8("天气"),
        QString::fromUtf8("相机"),
        QString::fromUtf8("灯光")
    };
    const QStringList paths = {
        QStringLiteral(":/picture/music.png"),
        QStringLiteral(":/picture/map.png"),
        QStringLiteral(":/picture/weather.png"),
        QStringLiteral(":/picture/camera.png"),
        QStringLiteral(":/picture/light.png")
    };
    for (int i = 0; i < labels.size(); ++i)
        m_icons.push_back({labels.at(i), QPixmap(paths.at(i))});

    m_frameTimer.setInterval(16);
    connect(&m_frameTimer, &QTimer::timeout, this, QOverload<>::of(&BootAnimationPage::update));

    m_finishTimer.setSingleShot(true);
    connect(&m_finishTimer, &QTimer::timeout, this, &BootAnimationPage::finishBoot);

    m_firstFrameTimer.setSingleShot(true);
    connect(&m_firstFrameTimer, &QTimer::timeout, this, [this]() {
        if (m_videoBoot && !m_videoFrameReady && m_videoError.isEmpty()) {
            showVideoError(QString::fromUtf8("load.mp4 未解码出画面：请先确认这个文件能在“视频播放”软件里正常播放"));
        }
    });
}

void BootAnimationPage::start()
{
    if (m_started)
        return;
    if (width() < 640 || height() < 360) {
        QTimer::singleShot(30, this, &BootAnimationPage::start);
        return;
    }

    m_started = true;
    m_elapsed.restart();
    m_finishing = false;

    if (startVideoBoot())
        return;

    startFallbackBoot();
}

void BootAnimationPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_videoPlayer)
        m_videoPlayer->setVideoGeometry(videoGeometryForPlayer());
    update();
}

qreal BootAnimationPage::progress() const
{
    if (!m_elapsed.isValid() || m_durationMs <= 0)
        return 0.0;
    return qBound(0.0, qreal(m_elapsed.elapsed()) / qreal(m_durationMs), 1.0);
}

qreal BootAnimationPage::easeOutCubic(qreal value) const
{
    value = qBound(0.0, value, 1.0);
    const qreal inv = 1.0 - value;
    return 1.0 - inv * inv * inv;
}

qreal BootAnimationPage::easeInOut(qreal value) const
{
    value = qBound(0.0, value, 1.0);
    return value < 0.5 ? 4.0 * value * value * value
                       : 1.0 - qPow(-2.0 * value + 2.0, 3.0) / 2.0;
}

qreal BootAnimationPage::segment(qreal value, qreal start, qreal end) const
{
    if (qFuzzyCompare(start, end))
        return value >= end ? 1.0 : 0.0;
    return qBound(0.0, (value - start) / (end - start), 1.0);
}

void BootAnimationPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_videoBoot) {
        if (m_videoFrameReady && !m_videoFrame.isNull()) {
            p.fillRect(rect(), Qt::black);
            const QSize targetSize = m_videoFrame.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
            const QRect target(QPoint((width() - targetSize.width()) / 2,
                                      (height() - targetSize.height()) / 2),
                               targetSize);
            p.drawImage(target, m_videoFrame);
        } else {
            p.fillRect(rect(), QColor(QStringLiteral("#050914")));
            if (!m_videoError.isEmpty()) {
                QFont errFont = font();
                errFont.setPixelSize(18);
                errFont.setBold(true);
                p.setFont(errFont);
                p.setPen(QColor(QStringLiteral("#ffcf72")));
                p.drawText(rect().adjusted(24, 0, -24, 0),
                           Qt::AlignCenter | Qt::TextWordWrap,
                           m_videoError);
            }
        }
        if (m_videoFrameReady) {
            const qreal videoProgress = qBound(0.0, qreal(m_elapsed.elapsed()) / qreal(m_videoBootDurationMs), 1.0);
            drawProgressBar(p, videoProgress);
        }
        return;
    }

    const qreal t = progress();
    drawBackground(p);

    const QPointF center(width() / 2.0, height() * 0.43);
    drawCenterMark(p, center, t);

    QFont titleFont = font();
    titleFont.setPixelSize(34);
    titleFont.setBold(true);
    p.setFont(titleFont);
    const qreal titleAlpha = easeOutCubic(segment(t, 0.08, 0.32));
    p.setPen(QColor(232, 248, 255, int(255 * titleAlpha)));
    p.drawText(QRectF(0, center.y() + 78, width(), 44),
               Qt::AlignCenter,
               QStringLiteral("Vehicle Operating System"));

    QFont subFont = font();
    subFont.setPixelSize(18);
    subFont.setBold(false);
    p.setFont(subFont);
    p.setPen(QColor(118, 210, 255, int(230 * titleAlpha)));
    p.drawText(QRectF(0, center.y() + 120, width(), 30),
               Qt::AlignCenter,
               QString::fromUtf8("车载系统正在启动"));

    drawBootIcons(p, QRectF(0, height() * 0.72, width(), height() * 0.2), t);

    const qreal readyAlpha = easeOutCubic(segment(t, 0.72, 0.90));
    QFont readyFont = font();
    readyFont.setPixelSize(16);
    readyFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(readyFont);
    p.setPen(QColor(255, 214, 112, int(240 * readyAlpha)));
    p.drawText(QRectF(0, height() - 52, width(), 24),
               Qt::AlignCenter,
               QStringLiteral("SYSTEM READY"));

    drawProgressBar(p, t);
}

void BootAnimationPage::drawBackground(QPainter &p)
{
    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.0, QColor(QStringLiteral("#050914")));
    bg.setColorAt(0.48, QColor(QStringLiteral("#081b24")));
    bg.setColorAt(1.0, QColor(QStringLiteral("#0b101b")));
    p.fillRect(rect(), bg);

    QRadialGradient glow(QPointF(width() * 0.5, height() * 0.42), width() * 0.46);
    glow.setColorAt(0.0, QColor(42, 205, 255, 58));
    glow.setColorAt(0.42, QColor(24, 124, 177, 22));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);

    p.setPen(QPen(QColor(70, 190, 255, 22), 1));
    const int step = 44;
    for (int x = -height(); x < width() + height(); x += step)
        p.drawLine(QPointF(x, height()), QPointF(x + height(), 0));
}

void BootAnimationPage::drawCenterMark(QPainter &p, const QPointF &center, qreal progressValue)
{
    const qreal intro = easeOutCubic(segment(progressValue, 0.0, 0.34));
    const qreal pulse = 0.5 + 0.5 * qSin(qreal(m_elapsed.elapsed()) / 230.0);
    const qreal radius = 58.0 + 6.0 * intro;

    p.save();
    p.translate(center);
    p.setOpacity(intro);

    QPen outerPen(QColor(75, 218, 255, 150), 2.0);
    p.setPen(outerPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(0, 0), radius, radius);

    QPen innerPen(QColor(255, 212, 92, 120 + int(70 * pulse)), 4.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(innerPen);
    const int sweep = int(280.0 * 16.0);
    const int start = int((-90.0 + qreal(m_elapsed.elapsed()) / 8.0) * 16.0);
    p.drawArc(QRectF(-radius - 8, -radius - 8, (radius + 8) * 2, (radius + 8) * 2), start, sweep);

    QConicalGradient cone(QPointF(0, 0), qreal(m_elapsed.elapsed()) / 14.0);
    cone.setColorAt(0.0, QColor(83, 222, 255, 0));
    cone.setColorAt(0.72, QColor(83, 222, 255, 18));
    cone.setColorAt(1.0, QColor(83, 222, 255, 96));
    p.setPen(Qt::NoPen);
    p.setBrush(cone);
    p.drawEllipse(QPointF(0, 0), radius - 12, radius - 12);

    QFont markFont = font();
    markFont.setPixelSize(34);
    markFont.setBold(true);
    p.setFont(markFont);
    p.setPen(QColor(238, 249, 255));
    p.drawText(QRectF(-64, -24, 128, 42), Qt::AlignCenter, QStringLiteral("VOS"));

    QFont smallFont = font();
    smallFont.setPixelSize(10);
    smallFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(smallFont);
    p.setPen(QColor(151, 224, 255, 190));
    p.drawText(QRectF(-64, 18, 128, 24), Qt::AlignCenter, QStringLiteral("IMX6ULL"));
    p.restore();
}

void BootAnimationPage::drawBootIcons(QPainter &p, const QRectF &area, qreal progressValue)
{
    if (m_icons.isEmpty())
        return;

    const int count = m_icons.size();
    const qreal spacing = 92.0;
    const qreal startX = area.center().x() - spacing * (count - 1) / 2.0;
    const qreal y = area.top() + 18.0;

    QFont iconFont = font();
    iconFont.setPixelSize(14);
    p.setFont(iconFont);

    for (int i = 0; i < count; ++i) {
        const qreal appear = easeOutCubic(segment(progressValue, 0.32 + i * 0.055, 0.52 + i * 0.055));
        const QPointF center(startX + i * spacing, y + 26.0 - (1.0 - appear) * 12.0);
        const QRectF iconRect(center.x() - 21, center.y() - 21, 42, 42);

        p.save();
        p.setOpacity(appear);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 22));
        p.drawRoundedRect(QRectF(center.x() - 31, center.y() - 31, 62, 62), 8, 8);
        p.setPen(QPen(QColor(80, 212, 255, 90), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(center.x() - 31, center.y() - 31, 62, 62), 8, 8);

        if (!m_icons.at(i).pixmap.isNull())
            p.drawPixmap(iconRect.toRect(), m_icons.at(i).pixmap);

        p.setPen(QColor(222, 245, 255, 210));
        p.drawText(QRectF(center.x() - 40, center.y() + 35, 80, 22),
                   Qt::AlignCenter,
                   m_icons.at(i).label);
        p.restore();
    }
}

void BootAnimationPage::drawProgressBar(QPainter &p, qreal value)
{
    value = qBound(0.0, value, 1.0);
    const qreal barW = qMin(width() * 0.62, 440.0);
    const qreal barH = 10.0;
    const QRectF track((width() - barW) / 2.0, height() - 24.0, barW, barH);
    const QRectF fill(track.left(), track.top(), track.width() * easeInOut(value), track.height());
    const qreal knobX = qBound(track.left(), fill.right(), track.right());

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF halo = track.adjusted(-14, -13, 14, 12);
    QLinearGradient haloGrad(halo.topLeft(), halo.bottomRight());
    haloGrad.setColorAt(0.0, QColor(238, 253, 255, 60));
    haloGrad.setColorAt(0.45, QColor(94, 219, 255, 48));
    haloGrad.setColorAt(1.0, QColor(255, 255, 255, 24));
    p.setPen(Qt::NoPen);
    p.setBrush(haloGrad);
    p.drawRoundedRect(halo, 16, 16);

    p.setPen(QPen(QColor(219, 249, 255, 112), 1));
    p.drawLine(QPointF(track.left() + 12, track.top() - 7),
               QPointF(track.right() - 12, track.top() - 7));

    QLinearGradient trackGrad(track.topLeft(), track.bottomLeft());
    trackGrad.setColorAt(0.0, QColor(255, 255, 255, 122));
    trackGrad.setColorAt(1.0, QColor(170, 235, 255, 86));
    p.setPen(QPen(QColor(255, 255, 255, 120), 1));
    p.setBrush(trackGrad);
    p.drawRoundedRect(track, barH / 2.0, barH / 2.0);

    QLinearGradient grad(fill.topLeft(), fill.topRight());
    grad.setColorAt(0.0, QColor(QStringLiteral("#ecfbff")));
    grad.setColorAt(0.46, QColor(QStringLiteral("#9eeeff")));
    grad.setColorAt(1.0, QColor(QStringLiteral("#42d6ff")));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(fill, barH / 2.0, barH / 2.0);

    if (fill.width() > 10) {
        QLinearGradient shine(fill.topLeft(), fill.bottomLeft());
        shine.setColorAt(0.0, QColor(255, 255, 255, 180));
        shine.setColorAt(0.5, QColor(255, 255, 255, 42));
        shine.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(shine);
        p.drawRoundedRect(fill.adjusted(3, 2, -3, -barH / 2.0), 3, 3);
    }

    const QPointF bubbles[] = {
        QPointF(track.left() + barW * 0.10, track.top() - 14),
        QPointF(track.left() + barW * 0.24, track.top() - 9),
        QPointF(track.left() + barW * 0.72, track.top() - 13),
        QPointF(track.left() + barW * 0.88, track.top() - 8)
    };
    const qreal radii[] = {2.2, 1.5, 1.8, 2.6};
    for (int i = 0; i < 4; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(238, 253, 255, 118));
        p.drawEllipse(bubbles[i], radii[i], radii[i]);
    }

    p.setBrush(QColor(255, 255, 255, 170));
    p.drawEllipse(QPointF(knobX, track.center().y()), 4.2, 4.2);

    QRadialGradient knobGlow(QPointF(knobX, track.center().y()), 24);
    knobGlow.setColorAt(0.0, QColor(230, 253, 255, 150));
    knobGlow.setColorAt(0.45, QColor(90, 220, 255, 58));
    knobGlow.setColorAt(1.0, QColor(90, 220, 255, 0));
    p.setBrush(knobGlow);
    p.drawEllipse(QPointF(knobX, track.center().y()), 24, 24);

    const QPixmap cat(QStringLiteral(":/picture/cat.png"));
    if (!cat.isNull()) {
        const int catSize = qBound(26, int(width() * 0.052), 38);
        const qreal catX = knobX - catSize / 2.0;
        const qreal catY = track.top() - catSize + 3.0;
        QRectF catRect(catX, catY, catSize, catSize);
        QRadialGradient catGlow(catRect.center(), catSize * 0.75);
        catGlow.setColorAt(0.0, QColor(255, 255, 255, 128));
        catGlow.setColorAt(0.62, QColor(145, 235, 255, 62));
        catGlow.setColorAt(1.0, QColor(145, 235, 255, 0));
        p.setBrush(catGlow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(catRect.adjusted(-5, -5, 5, 5));
        p.drawPixmap(catRect.toRect(), cat.scaled(catSize, catSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    p.setPen(QPen(QColor(218, 250, 255, 138), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(track.adjusted(-1, -1, 1, 1), barH / 2.0 + 1, barH / 2.0 + 1);
    p.restore();
}

bool BootAnimationPage::startVideoBoot()
{
    const QString videoPath = QStringLiteral("/root/videos/load.mp4");
    if (!QFileInfo::exists(videoPath))
        return false;

    const QFileInfo videoInfo(videoPath);
    if (videoInfo.size() <= 0) {
        m_videoBoot = true;
        showVideoError(QString::fromUtf8("/root/videos/load.mp4 文件为空"));
        return true;
    }

    m_videoBoot = true;
    m_videoFrameReady = false;
    m_videoFrame = QImage();
    m_videoError.clear();
    m_videoPlayer = new VideoPlayer(this);
    m_videoPlayer->setVolume(80);
    m_videoPlayer->setVideoGeometry(videoGeometryForPlayer());
    m_videoPlayer->setPlaylist(QStringList() << videoPath);

    m_frameTimer.start();
    m_finishTimer.start(8000);
    m_firstFrameTimer.start(3500);

#if defined(USE_GSTREAMER_VIDEO_BACKEND)
    connect(m_videoPlayer, &VideoPlayer::frameReady, this, [this](const QImage &image) {
        m_videoFrame = image;
        if (!m_videoFrameReady) {
            m_videoFrameReady = true;
            m_firstFrameTimer.stop();
            m_videoError.clear();
            m_elapsed.restart();
            m_finishTimer.start(m_videoBootDurationMs);
        }
        update();
    });
#else
    showVideoError(QString::fromUtf8("当前构建未启用 GStreamer，无法播放 /root/videos/load.mp4"));
#endif

    connect(m_videoPlayer, &VideoPlayer::durationChanged, this, [this](qint64) {});
    connect(m_videoPlayer, &VideoPlayer::playingChanged, this, [this](bool playing) {
        if (!playing && m_videoBoot && m_videoFrameReady && m_elapsed.elapsed() > 700)
            finishBoot();
    });
    connect(m_videoPlayer, &VideoPlayer::errorOccurred, this, [this](const QString &message) {
        showVideoError(QString::fromUtf8("load.mp4 播放失败：%1").arg(message));
    });

    m_videoPlayer->playIndex(0);

    return true;
}

void BootAnimationPage::startFallbackBoot()
{
    m_videoBoot = false;
    m_elapsed.restart();
    m_frameTimer.start();
    m_finishTimer.start(m_durationMs);
    update();
}

void BootAnimationPage::showVideoError(const QString &message)
{
    qWarning() << "boot video error:" << message;
    m_videoError = message;
    m_videoFrameReady = false;
    m_firstFrameTimer.stop();
    m_finishTimer.start(6000);
    update();
}

QRect BootAnimationPage::videoGeometryForPlayer() const
{
#if defined(__arm__)
    const QPoint topLeft = mapTo(window(), QPoint(0, 0));
    QRect geometry(topLeft, size());
#else
    QRect geometry(mapToGlobal(QPoint(0, 0)), size());
#endif
    geometry.adjust(0, 0, -1, -1);
    return geometry;
}

void BootAnimationPage::finishBoot()
{
    if (m_finishing)
        return;
    m_finishing = true;
    m_frameTimer.stop();
    m_finishTimer.stop();
    m_firstFrameTimer.stop();
    if (m_videoPlayer)
        m_videoPlayer->stop();
    emit finished();
}
