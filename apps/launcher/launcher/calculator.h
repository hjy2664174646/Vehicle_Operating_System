#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QWidget>

class QLabel;

/**
 * @brief 简易四则运算计算器（适配主页第二屏 / 800x480）
 */
class CalculatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalculatorWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onEqualClicked();
    void onClearClicked();
    void onBackspaceClicked();
    void onDecimalClicked();

private:
    void appendDigit(const QString &digit);
    void applyOperator(const QString &opText);
    void applyPendingOperation();
    void updateDisplay();
    void setError(const QString &message);

    QLabel *m_display = nullptr;
    QString m_input;
    double m_accumulator = 0.0;
    QChar m_pendingOp;
    bool m_freshInput = true;
    bool m_error = false;
};

#endif // CALCULATOR_H
