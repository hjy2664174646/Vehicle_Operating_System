#ifndef WAVEGAUGE_H
#define WAVEGAUGE_H

#include <QColor>
#include <QTimer>
#include <QWidget>

/** 赛博风水波液位仪表：正弦波动画 + 霓虹外环进度 */
class WaveGauge : public QWidget
{
    Q_OBJECT

public:
    explicit WaveGauge(QWidget *parent = nullptr);
    ~WaveGauge() override;

    void setPercent(int percent);
    void setAccentColors(const QColor &primary, const QColor &secondary);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAnimate();

private:
    QPainterPath buildWavePath(const QRectF &bounds,
                               double baseY,
                               double phase,
                               double amplitude) const;
    void drawBackground(QPainter &p, const QRectF &bounds);
    void drawWaves(QPainter &p, const QRectF &bounds);
    void drawProgressRing(QPainter &p, const QRectF &bounds);
    void drawCenterText(QPainter &p, const QRectF &bounds);

    QTimer m_timer;
    double m_phase = 0.0;
    int m_percent = 0;
    QColor m_primary;
    QColor m_secondary;
};

#endif // WAVEGAUGE_H
