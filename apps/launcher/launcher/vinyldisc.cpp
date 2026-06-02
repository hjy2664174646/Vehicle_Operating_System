#include "vinyldisc.h"

#include <QConicalGradient>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QTimerEvent>
#include <QtMath>

namespace {

constexpr int kSpinTimerMs = 40;

} // namespace

VinylDiscWidget::VinylDiscWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_defaultCover.load(QStringLiteral(":/picture/music.png"));
#if defined(__arm__)
    setAttribute(Qt::WA_OpaquePaintEvent, false);
#else
    setAttribute(Qt::WA_TranslucentBackground, true);
#endif
}

void VinylDiscWidget::setCover(const QPixmap &cover)
{
    m_cover = cover.isNull() ? m_defaultCover : cover;
    update();
}

void VinylDiscWidget::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {
        if (m_timerId == 0)
            m_timerId = startTimer(kSpinTimerMs);
    } else if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    update();
}

void VinylDiscWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId)
        return;

    m_angle += 0.35;
    if (m_angle >= 360.0)
        m_angle -= 360.0;
    update();
}

void VinylDiscWidget::drawTurntable(QPainter &p, const QRectF &area)
{
    p.save();

    const QPointF center = area.center();
    const qreal radius = qMin(area.width(), area.height()) * 0.44;

    QRadialGradient paper(center, radius * 1.28);
    paper.setColorAt(0.0, QColor(246, 240, 226, 70));
    paper.setColorAt(0.55, QColor(221, 211, 188, 32));
    paper.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(paper);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, radius * 1.18, radius * 1.18);

    QPen washPen(QColor(220, 205, 170, 44), 1.2);
    p.setPen(washPen);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 9; ++i) {
        const qreal r = radius * (0.58 + i * 0.055);
        QRectF ring(center.x() - r, center.y() - r * 0.98, r * 2, r * 1.96);
        p.drawArc(ring.adjusted(i % 2, -i % 3, 0, i % 2), 20 * 16, (260 + i * 7) * 16);
    }

    p.restore();
}

void VinylDiscWidget::drawDisc(QPainter &p, const QPointF &center, qreal radius)
{
    p.save();
    p.translate(center);
    p.rotate(m_angle);

    QRadialGradient ink(0, 0, radius);
    ink.setColorAt(0.0, QColor(38, 45, 38, 218));
    ink.setColorAt(0.34, QColor(30, 36, 32, 196));
    ink.setColorAt(0.66, QColor(18, 24, 23, 174));
    ink.setColorAt(1.0, QColor(6, 10, 10, 80));
    p.setBrush(ink);
    p.setPen(QPen(QColor(235, 226, 204, 58), 1.2));
    p.drawEllipse(QPointF(0, 0), radius, radius);

    QConicalGradient cloud(0, 0, -m_angle);
    cloud.setColorAt(0.00, QColor(255, 255, 255, 22));
    cloud.setColorAt(0.18, QColor(42, 63, 58, 110));
    cloud.setColorAt(0.36, QColor(255, 255, 255, 12));
    cloud.setColorAt(0.55, QColor(14, 20, 20, 126));
    cloud.setColorAt(0.78, QColor(126, 154, 132, 64));
    cloud.setColorAt(1.00, QColor(255, 255, 255, 22));
    p.setBrush(cloud);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), radius * 0.96, radius * 0.96);

    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 18; ++i) {
        const qreal start = (i * 22 + qSin(i * 1.7) * 18) * 16;
        const qreal span = (58 + qCos(i * 2.1) * 18) * 16;
        const qreal r = radius * (0.42 + i * 0.028);
        QPen inkLine(QColor(230, 232, 218, 20 + (i % 4) * 8), 1.0 + (i % 3) * 0.25);
        p.setPen(inkLine);
        p.drawArc(QRectF(-r, -r, r * 2, r * 2), int(start), int(span));
    }

    QPen edge(QColor(222, 212, 184, 95), 2.0);
    p.setPen(edge);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(0, 0), radius * 0.985, radius * 0.985);

    QRadialGradient jade(0, 0, radius * 0.28);
    jade.setColorAt(0.0, QColor(225, 244, 218, 238));
    jade.setColorAt(0.48, QColor(101, 154, 126, 218));
    jade.setColorAt(1.0, QColor(32, 70, 62, 236));
    p.setBrush(jade);
    p.setPen(QPen(QColor(236, 224, 190, 170), 1.4));
    p.drawEllipse(QPointF(0, 0), radius * 0.24, radius * 0.24);

    p.setBrush(QColor(145, 32, 26, 230));
    p.setPen(QPen(QColor(238, 196, 160, 150), 1));
    const QRectF seal(radius * 0.05, radius * 0.05, radius * 0.18, radius * 0.18);
    p.drawRoundedRect(seal, 3, 3);
    p.setPen(QPen(QColor(255, 226, 198, 210), 1.5));
    p.drawLine(seal.left() + seal.width() * 0.28, seal.top() + seal.height() * 0.25,
               seal.left() + seal.width() * 0.28, seal.bottom() - seal.height() * 0.22);
    p.drawLine(seal.left() + seal.width() * 0.55, seal.top() + seal.height() * 0.25,
               seal.left() + seal.width() * 0.55, seal.bottom() - seal.height() * 0.22);
    p.drawLine(seal.left() + seal.width() * 0.25, seal.center().y(),
               seal.right() - seal.width() * 0.2, seal.center().y());

    p.restore();
}

void VinylDiscWidget::drawTonearm(QPainter &p, const QRectF &area)
{
    p.save();

    const QPointF start(area.left() + area.width() * 0.12, area.bottom() - area.height() * 0.18);
    QPainterPath branch;
    branch.moveTo(start);
    branch.cubicTo(area.left() + area.width() * 0.24, area.bottom() - area.height() * 0.28,
                   area.left() + area.width() * 0.34, area.bottom() - area.height() * 0.18,
                   area.left() + area.width() * 0.45, area.bottom() - area.height() * 0.30);
    p.setPen(QPen(QColor(122, 82, 55, 122), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawPath(branch);

    p.setPen(Qt::NoPen);
    for (int i = 0; i < 5; ++i) {
        const QPointF blossom(start.x() + area.width() * (0.10 + i * 0.055),
                              start.y() - area.height() * (0.08 + (i % 2) * 0.05));
        p.setBrush(QColor(206, 112, 104, 72));
        p.drawEllipse(blossom, 4.5, 3.6);
    }

    p.restore();
}

void VinylDiscWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = rect().adjusted(4, 4, -4, -4);
    drawTurntable(p, area);

    const QPointF center(area.center().x(), area.center().y() - area.height() * 0.01);
    const qreal radius = qMin(area.width(), area.height()) * 0.38;
    drawDisc(p, center, radius);
    drawTonearm(p, area);
}
