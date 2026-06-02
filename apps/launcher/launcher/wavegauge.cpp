#include "wavegauge.h"

#include <QtMath>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

constexpr int kRingWidth = 5;
constexpr int kPadding = 6;

} // namespace

WaveGauge::WaveGauge(QWidget *parent)
    : QWidget(parent)
    , m_primary(QColor(QStringLiteral("#00e5ff")))
    , m_secondary(QColor(QStringLiteral("#7c4dff")))
{
#if defined(__arm__)
    setMinimumSize(150, 150);
    setMaximumSize(180, 180);
    setAutoFillBackground(true);
    QPalette pal;
    // 与传感器卡片背景一致，避免圆角矩形外露出另一块色
    pal.setColor(QPalette::Window, QColor(QStringLiteral("#0b1128")));
    setPalette(pal);
#else
    setMinimumSize(130, 130);
    setAttribute(Qt::WA_TranslucentBackground, true);
#endif

    connect(&m_timer, &QTimer::timeout, this, &WaveGauge::onAnimate);
    m_timer.start(40);
}

WaveGauge::~WaveGauge()
{
    m_timer.stop();
}

void WaveGauge::setPercent(int percent)
{
    const int clamped = qBound(0, percent, 100);
    if (m_percent == clamped)
        return;
    m_percent = clamped;
    update();
}

void WaveGauge::setAccentColors(const QColor &primary, const QColor &secondary)
{
    m_primary = primary;
    m_secondary = secondary.isValid() ? secondary : primary;
    update();
}

void WaveGauge::onAnimate()
{
    m_phase += 0.10;
    if (m_phase > 2 * M_PI)
        m_phase -= 2 * M_PI;
    update();
}

QPainterPath WaveGauge::buildWavePath(const QRectF &bounds,
                                      double baseY,
                                      double phase,
                                      double amplitude) const
{
    QPainterPath path;
    path.moveTo(bounds.left(), bounds.bottom());

    const double step = 2.0;
    const double freq = 0.038;
    for (double x = bounds.left(); x <= bounds.right() + step; x += step) {
        const double y = baseY + amplitude * qSin((x - bounds.left()) * freq + phase);
        path.lineTo(x, y);
    }

    path.lineTo(bounds.right(), bounds.bottom());
    path.closeSubpath();
    return path;
}

void WaveGauge::drawBackground(QPainter &p, const QRectF &bounds)
{
#if defined(__arm__)
    QRadialGradient innerGlow(bounds.center(), bounds.width() / 2.0);
    innerGlow.setColorAt(0.0, QColor(QStringLiteral("#0c1538")));
    innerGlow.setColorAt(0.55, QColor(QStringLiteral("#0b1128")));
    innerGlow.setColorAt(1.0, QColor(QStringLiteral("#0a0e22")));
#else
    QRadialGradient innerGlow(bounds.center(), bounds.width() / 2.0);
    innerGlow.setColorAt(0.0, QColor(QStringLiteral("#0c1230")));
    innerGlow.setColorAt(0.55, QColor(QStringLiteral("#080e24")));
    innerGlow.setColorAt(0.85, QColor(QStringLiteral("#060a18")));
    innerGlow.setColorAt(1.0, QColor(QStringLiteral("#040810")));
#endif
    p.setBrush(innerGlow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(bounds);

    p.save();
    QPainterPath clip;
    clip.addEllipse(bounds);
    p.setClipPath(clip);

#if defined(__arm__)
    QPen gridPen(QColor(QStringLiteral("#1a3555")));
#else
    QPen gridPen(QColor(255, 255, 255, 18));
#endif
    gridPen.setWidth(1);
    p.setPen(gridPen);
    const int gridStep = 12;
    for (int x = int(bounds.left()); x <= int(bounds.right()); x += gridStep)
        p.drawLine(x, int(bounds.top()), x, int(bounds.bottom()));
    for (int y = int(bounds.top()); y <= int(bounds.bottom()); y += gridStep)
        p.drawLine(int(bounds.left()), y, int(bounds.right()), y);
    p.restore();

#if defined(__arm__)
    QPen ringPen(m_primary.darker(140));
#else
    QPen ringPen(QColor(m_primary.red(), m_primary.green(), m_primary.blue(), 70));
#endif
    ringPen.setWidth(1);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(bounds.adjusted(1, 1, -1, -1));
}

void WaveGauge::drawWaves(QPainter &p, const QRectF &bounds)
{
    if (m_percent <= 0)
        return;

    const double fillHeight = bounds.height() * m_percent / 100.0;
    const double baseY = bounds.bottom() - fillHeight;

    QPainterPath clip;
    clip.addEllipse(bounds);
    p.setClipPath(clip);

#if defined(__arm__)
    const QColor backWave(m_secondary.darker(160));
    const QColor frontWave(m_primary.darker(115));
#else
    const QColor backWave(m_secondary.red(), m_secondary.green(), m_secondary.blue(), 90);
    const QColor frontWave(m_primary.red(), m_primary.green(), m_primary.blue(), 160);
#endif

    p.fillPath(buildWavePath(bounds, baseY, m_phase + 1.4, 5.0), backWave);
    p.fillPath(buildWavePath(bounds, baseY, m_phase, 7.0), frontWave);

    // 液面高光线
#if defined(__arm__)
    QPen highlight(m_primary);
#else
    QPen highlight(QColor(m_primary.red(), m_primary.green(), m_primary.blue(), 180));
#endif
    highlight.setWidth(2);
    p.setPen(highlight);
    p.setBrush(Qt::NoBrush);

    QPainterPath surface;
    const double step = 2.0;
    const double freq = 0.038;
    bool first = true;
    for (double x = bounds.left(); x <= bounds.right() + step; x += step) {
        const double y = baseY + 7.0 * qSin((x - bounds.left()) * freq + m_phase);
        if (first) {
            surface.moveTo(x, y);
            first = false;
        } else {
            surface.lineTo(x, y);
        }
    }
    p.drawPath(surface);

    p.setClipping(false);
}

void WaveGauge::drawProgressRing(QPainter &p, const QRectF &bounds)
{
    const QRectF ringRect = bounds.adjusted(-kRingWidth, -kRingWidth,
                                            kRingWidth, kRingWidth);

#if defined(__arm__)
    QPen trackPen(QColor(QStringLiteral("#1a3555")));
#else
    QPen trackPen(QColor(255, 255, 255, 28));
#endif
    trackPen.setWidth(kRingWidth);
    trackPen.setCapStyle(Qt::RoundCap);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(ringRect, 90 * 16, -360 * 16);

    if (m_percent <= 0)
        return;

    const int span = -m_percent * 360 / 100 * 16;

#if !defined(__arm__)
    QPen glowPen(QColor(m_primary.red(), m_primary.green(), m_primary.blue(), 60));
    glowPen.setWidth(kRingWidth + 4);
    glowPen.setCapStyle(Qt::RoundCap);
    p.setPen(glowPen);
    p.drawArc(ringRect, 90 * 16, span);
#endif

    QPen progressPen(m_primary);
    progressPen.setWidth(kRingWidth);
    progressPen.setCapStyle(Qt::RoundCap);
    p.setPen(progressPen);
    p.drawArc(ringRect, 90 * 16, span);
}

void WaveGauge::drawCenterText(QPainter &p, const QRectF &bounds)
{
    const QString text = QString::number(m_percent) + QStringLiteral("%");

    QFont font = p.font();
    font.setPixelSize(qMax(14, int(bounds.width() / 5.5)));
    font.setBold(true);
    p.setFont(font);

#if !defined(__arm__)
    p.setPen(QColor(m_primary.red(), m_primary.green(), m_primary.blue(), 80));
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0)
                continue;
            p.drawText(bounds.translated(dx, dy), Qt::AlignCenter, text);
        }
    }
#endif

    p.setPen(m_primary);
    p.drawText(bounds, Qt::AlignCenter, text);
}

void WaveGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
#if defined(__arm__)
    p.fillRect(rect(), palette().color(QPalette::Window));
    p.setRenderHint(QPainter::Antialiasing, true);
#else
    p.setRenderHint(QPainter::Antialiasing, true);
#endif

    const int side = qMin(width(), height());
    const QRectF bounds((width() - side) / 2.0 + kPadding,
                        (height() - side) / 2.0 + kPadding,
                        side - kPadding * 2,
                        side - kPadding * 2);

    drawBackground(p, bounds);
    drawWaves(p, bounds);
    drawProgressRing(p, bounds);
    drawCenterText(p, bounds);
}
