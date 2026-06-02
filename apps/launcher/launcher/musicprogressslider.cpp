#include "musicprogressslider.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QStyle>
#include <QStyleOptionSlider>

GradientProgressSlider::GradientProgressSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
{
    setFocusPolicy(Qt::NoFocus);
    setTracking(true);
    setCursor(Qt::PointingHandCursor);
    // 隐藏默认样式，仅保留滑块几何用于命中与拖动；渐变由 paintEvent 绘制
    setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: transparent; border: none; }"
        "QSlider::sub-page:horizontal { background: transparent; }"
        "QSlider::add-page:horizontal { background: transparent; }"
        "QSlider::handle:horizontal {"
        "  width: 16px; height: 16px; margin: -6px 0;"
        "  background: transparent; border: none;"
        "}"));
}

void GradientProgressSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                 QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                 QStyle::SC_SliderHandle, this);

    if (groove.isEmpty())
        return;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#333333")));
    p.drawRoundedRect(groove, 2, 2);

    QRect fill = groove;
    fill.setRight(qMax(groove.left(), handle.center().x()));
    if (fill.width() > 1) {
        QLinearGradient grad(fill.left(), fill.center().y(),
                             fill.right(), fill.center().y());
        grad.setColorAt(0.0, QColor(QStringLiteral("#31C27C")));
        grad.setColorAt(0.45, QColor(QStringLiteral("#5AE396")));
        grad.setColorAt(0.75, QColor(QStringLiteral("#42E8C8")));
        grad.setColorAt(1.0, QColor(QStringLiteral("#00D4FF")));
        p.setBrush(grad);
        p.drawRoundedRect(fill, 2, 2);
    }

    const QPoint center = handle.center();
    const int radius = qMax(7, handle.width() / 2);

    QRadialGradient handleGrad(center, qreal(radius));
    handleGrad.setColorAt(0.0, QColor(QStringLiteral("#A8FFD0")));
    handleGrad.setColorAt(0.55, QColor(QStringLiteral("#31C27C")));
    handleGrad.setColorAt(1.0, QColor(QStringLiteral("#1FA86A")));
    p.setBrush(handleGrad);
    p.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), 2));
    p.drawEllipse(center, radius, radius);
}
