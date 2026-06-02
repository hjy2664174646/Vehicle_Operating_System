#ifndef VINYLDISC_H
#define VINYLDISC_H

#include <QPixmap>
#include <QWidget>

/**
 * @brief QQ 音乐风格黑胶唱机（自绘 + 播放时旋转）
 */
class VinylDiscWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VinylDiscWidget(QWidget *parent = nullptr);

    void setCover(const QPixmap &cover);
    void setPlaying(bool playing);

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void drawTurntable(QPainter &p, const QRectF &area);
    void drawDisc(QPainter &p, const QPointF &center, qreal radius);
    void drawTonearm(QPainter &p, const QRectF &area);

    QPixmap m_cover;
    QPixmap m_defaultCover;
    bool m_playing = false;
    qreal m_angle = 0.0;
    int m_timerId = 0;
};

#endif // VINYLDISC_H
