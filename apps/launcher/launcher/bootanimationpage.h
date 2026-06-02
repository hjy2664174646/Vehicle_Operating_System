#ifndef BOOTANIMATIONPAGE_H
#define BOOTANIMATIONPAGE_H

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class VideoPlayer;

class BootAnimationPage : public QWidget
{
    Q_OBJECT

public:
    explicit BootAnimationPage(QWidget *parent = nullptr);
    void start();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct BootIcon {
        QString label;
        QPixmap pixmap;
    };

    qreal progress() const;
    qreal easeOutCubic(qreal value) const;
    qreal easeInOut(qreal value) const;
    qreal segment(qreal value, qreal start, qreal end) const;
    void drawBackground(QPainter &p);
    void drawCenterMark(QPainter &p, const QPointF &center, qreal progressValue);
    void drawBootIcons(QPainter &p, const QRectF &area, qreal progressValue);
    void drawProgressBar(QPainter &p, qreal value);
    bool startVideoBoot();
    void finishBoot();
    void startFallbackBoot();
    void showVideoError(const QString &message);
    QRect videoGeometryForPlayer() const;

    QTimer m_frameTimer;
    QTimer m_finishTimer;
    QTimer m_firstFrameTimer;
    QElapsedTimer m_elapsed;
    QVector<BootIcon> m_icons;
    VideoPlayer *m_videoPlayer = nullptr;
    QImage m_videoFrame;
    QString m_videoError;
    int m_durationMs = 3300;
    int m_videoBootDurationMs = 3600;
    bool m_videoBoot = false;
    bool m_videoFrameReady = false;
    bool m_finishing = false;
    bool m_started = false;
};

#endif // BOOTANIMATIONPAGE_H
