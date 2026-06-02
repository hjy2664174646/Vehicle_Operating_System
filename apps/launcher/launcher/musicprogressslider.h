#ifndef MUSICPROGRESSSLIDER_H
#define MUSICPROGRESSSLIDER_H

#include <QSlider>

/**
 * @brief 渐变色进度条（自绘，PC/板端一致）
 */
class GradientProgressSlider : public QSlider
{
    Q_OBJECT

public:
    explicit GradientProgressSlider(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // MUSICPROGRESSSLIDER_H
