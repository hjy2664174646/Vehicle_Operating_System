#include "calculator.h"

#include <QGridLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QtMath>

namespace {

void paintCalcHudBackground(QPainter &p, const QRect &rect)
{
#if defined(__arm__)
    p.fillRect(rect, QColor(QStringLiteral("#0a0e22")));

    QPen gridPen(QColor(QStringLiteral("#1a3555")));
    gridPen.setWidth(1);
    p.setPen(gridPen);
    const int gridStep = 28;
    for (int x = rect.left(); x <= rect.right(); x += gridStep)
        p.drawLine(x, rect.top(), x, rect.bottom());
    for (int y = rect.top(); y <= rect.bottom(); y += gridStep)
        p.drawLine(rect.left(), y, rect.right(), y);

    QPen accent(QColor(QStringLiteral("#00b8d4")));
    accent.setWidth(2);
    p.setPen(accent);
    p.drawLine(rect.left() + 12, rect.top() + 1, rect.right() - 12, rect.top() + 1);
#else
    QLinearGradient base(rect.topLeft(), rect.bottomRight());
    base.setColorAt(0.0, QColor(QStringLiteral("#0a0e22")));
    base.setColorAt(0.45, QColor(QStringLiteral("#0c1538")));
    base.setColorAt(1.0, QColor(QStringLiteral("#050a18")));
    p.fillRect(rect, base);

    QRadialGradient glow(rect.center(), qreal(qMax(rect.width(), rect.height())) * 0.5);
    glow.setColorAt(0.0, QColor(0, 229, 255, 32));
    glow.setColorAt(0.4, QColor(124, 77, 255, 16));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);

    QPen gridPen(QColor(0, 229, 255, 18));
    gridPen.setWidth(1);
    p.setPen(gridPen);
    const int gridStep = 24;
    for (int x = rect.left(); x <= rect.right(); x += gridStep)
        p.drawLine(x, rect.top(), x, rect.bottom());
    for (int y = rect.top(); y <= rect.bottom(); y += gridStep)
        p.drawLine(rect.left(), y, rect.right(), y);
#endif
}

void paintGlassPanel(QPainter &p, const QRect &panel)
{
#if defined(__arm__)
    p.fillRect(panel, QColor(QStringLiteral("#0b1128")));
    QPen pen(QColor(QStringLiteral("#1e5a7a")));
    pen.setWidth(2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(panel.adjusted(1, 1, -2, -2));
#else
    p.setBrush(QColor(10, 18, 40, 72));
    p.setPen(QPen(QColor(0, 229, 255, 90), 1));
    p.drawRoundedRect(panel, 18, 18);

    QLinearGradient sheen(panel.topLeft(), panel.bottomLeft());
    sheen.setColorAt(0.0, QColor(0, 229, 255, 36));
    sheen.setColorAt(0.18, QColor(0, 229, 255, 0));
    p.fillRect(QRect(panel.left() + 1, panel.top() + 1, panel.width() - 2, panel.height() / 3), sheen);

    QPen accent(QColor(0, 229, 255, 120));
    accent.setWidth(2);
    p.setPen(accent);
    p.drawLine(panel.left() + 20, panel.top() + 1, panel.right() - 20, panel.top() + 1);
#endif
}

QPushButton *makeCalcButton(const QString &text, QWidget *parent)
{
    auto *btn = new QPushButton(text, parent);
    btn->setMinimumSize(72, 50);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
#if defined(__arm__)
    btn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #B8E4FF;"
        "  background-color: #0b1128;"
        "  border: 2px solid #1e5a7a;"
        "  border-radius: 8px;"
        "  font-size: 18px;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #123050;"
        "  border: 2px solid #00b8d4;"
        "}"));
#else
    btn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #B8E4FF;"
        "  background-color: rgba(10, 18, 40, 0.42);"
        "  border: 1px solid rgba(0, 229, 255, 0.38);"
        "  border-radius: 12px;"
        "  font-size: 18px;"
        "}"
        "QPushButton:pressed {"
        "  color: #ffffff;"
        "  background-color: rgba(0, 229, 255, 0.28);"
        "  border: 1px solid rgba(0, 229, 255, 0.75);"
        "}"));
#endif
    return btn;
}

QPushButton *makeOpButton(const QString &text, QWidget *parent)
{
    auto *btn = makeCalcButton(text, parent);
#if defined(__arm__)
    btn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #0a0e22;"
        "  background-color: #00b8d4;"
        "  border: 2px solid #00e5ff;"
        "  border-radius: 8px;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #7ce8ff;"
        "}"));
#else
    btn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: #E8FFFF;"
        "  background-color: rgba(0, 229, 255, 0.22);"
        "  border: 1px solid rgba(0, 229, 255, 0.62);"
        "  border-radius: 12px;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(0, 229, 255, 0.42);"
        "  border: 1px solid rgba(184, 228, 255, 0.9);"
        "}"));
#endif
    return btn;
}

QString displayStyleSheet()
{
#if defined(__arm__)
    return QStringLiteral(
        "color: #00e5ff;"
        "background-color: #0b1128;"
        "border: 2px solid #1e5a7a;"
        "border-radius: 8px;"
        "padding: 8px 16px;"
        "font-size: 28px;"
        "font-weight: bold;");
#else
    return QStringLiteral(
        "color: #00e5ff;"
        "background-color: rgba(5, 12, 28, 0.48);"
        "border: 1px solid rgba(0, 229, 255, 0.45);"
        "border-radius: 14px;"
        "padding: 8px 18px;"
        "font-size: 30px;"
        "font-weight: bold;");
#endif
}

} // namespace

CalculatorWidget::CalculatorWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
#if defined(__arm__)
    setAttribute(Qt::WA_OpaquePaintEvent, true);
#else
    setAttribute(Qt::WA_StyledBackground, true);
#endif

    auto *root = new QVBoxLayout(this);
#if defined(__arm__)
    root->setContentsMargins(20, 16, 20, 16);
#else
    root->setContentsMargins(28, 24, 28, 24);
#endif
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("计算器"), this);
#if defined(__arm__)
    title->setStyleSheet(QStringLiteral(
        "color: #B8E4FF;"
        "font-size: 15px;"
        "background: transparent;"));
#else
    title->setStyleSheet(QStringLiteral(
        "color: rgba(184, 228, 255, 0.88);"
        "font-size: 15px;"
        "background: transparent;"));
#endif

    m_display = new QLabel(QStringLiteral("0"), this);
    m_display->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_display->setMinimumHeight(68);
    m_display->setStyleSheet(displayStyleSheet());

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const struct {
        const char *text;
        int row;
        int col;
        int rowSpan;
        int colSpan;
        int type; // 0 digit, 1 op, 2 eq, 3 clear, 4 back, 5 dot
    } keys[] = {
        { "C",  0, 0, 1, 1, 3 },
        { "⌫", 0, 1, 1, 1, 4 },
        { "÷",  0, 2, 1, 1, 1 },
        { "×",  0, 3, 1, 1, 1 },
        { "7",  1, 0, 1, 1, 0 },
        { "8",  1, 1, 1, 1, 0 },
        { "9",  1, 2, 1, 1, 0 },
        { "-",  1, 3, 1, 1, 1 },
        { "4",  2, 0, 1, 1, 0 },
        { "5",  2, 1, 1, 1, 0 },
        { "6",  2, 2, 1, 1, 0 },
        { "+",  2, 3, 1, 1, 1 },
        { "1",  3, 0, 1, 1, 0 },
        { "2",  3, 1, 1, 1, 0 },
        { "3",  3, 2, 1, 1, 0 },
        { "=",  3, 3, 2, 1, 2 },
        { "0",  4, 0, 1, 2, 0 },
        { ".",  4, 2, 1, 1, 5 },
    };

    for (const auto &key : keys) {
        const QString label = QString::fromUtf8(key.text);
        QPushButton *btn = (key.type == 1 || key.type == 2)
            ? makeOpButton(label, this)
            : makeCalcButton(label, this);

        switch (key.type) {
        case 0:
            connect(btn, &QPushButton::clicked, this, [this, label]() {
                appendDigit(label);
            });
            break;
        case 1:
            connect(btn, &QPushButton::clicked, this, [this, label]() {
                applyOperator(label);
            });
            break;
        case 2:
            connect(btn, &QPushButton::clicked, this, &CalculatorWidget::onEqualClicked);
            break;
        case 3:
            connect(btn, &QPushButton::clicked, this, &CalculatorWidget::onClearClicked);
            break;
        case 4:
            connect(btn, &QPushButton::clicked, this, &CalculatorWidget::onBackspaceClicked);
            break;
        case 5:
            connect(btn, &QPushButton::clicked, this, &CalculatorWidget::onDecimalClicked);
            break;
        }

        grid->addWidget(btn, key.row, key.col, key.rowSpan, key.colSpan);
    }

    root->addWidget(title);
    root->addWidget(m_display);
    root->addLayout(grid, 1);
}

void CalculatorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
#if !defined(__arm__)
    p.setRenderHint(QPainter::Antialiasing);
#endif

    paintCalcHudBackground(p, rect());
    paintGlassPanel(p, rect().adjusted(14, 10, -14, -10));
}

void CalculatorWidget::appendDigit(const QString &digit)
{
    if (m_error) {
        m_input.clear();
        m_error = false;
        m_freshInput = true;
    }

    if (m_freshInput) {
        m_input = digit;
        m_freshInput = false;
    } else if (m_input == QStringLiteral("0") && digit != QStringLiteral(".")) {
        m_input = digit;
    } else {
        m_input += digit;
    }
    updateDisplay();
}

void CalculatorWidget::applyPendingOperation()
{
    const double value = m_input.toDouble();
    if (m_pendingOp.isNull()) {
        m_accumulator = value;
        return;
    }

    switch (m_pendingOp.unicode()) {
    case '+':
        m_accumulator += value;
        break;
    case '-':
        m_accumulator -= value;
        break;
    case 0x00D7: // ×
        m_accumulator *= value;
        break;
    case 0x00F7: // ÷
        if (qFuzzyIsNull(value)) {
            setError(QStringLiteral("除数不能为0"));
            return;
        }
        m_accumulator /= value;
        break;
    default:
        m_accumulator = value;
        break;
    }

    m_input = QString::number(m_accumulator, 'g', 12);
    if (m_input.contains(QLatin1Char('.'))) {
        while (m_input.endsWith(QLatin1Char('0')))
            m_input.chop(1);
        if (m_input.endsWith(QLatin1Char('.')))
            m_input.chop(1);
    }
    updateDisplay();
}

void CalculatorWidget::updateDisplay()
{
    m_display->setText(m_input.isEmpty() ? QStringLiteral("0") : m_input);
}

void CalculatorWidget::setError(const QString &message)
{
    m_error = true;
    m_freshInput = true;
    m_pendingOp = QChar();
    m_display->setText(message);
#if defined(__arm__)
    m_display->setStyleSheet(QStringLiteral(
        "color: #ff6b8a;"
        "background-color: #280810;"
        "border: 2px solid #aa3355;"
        "border-radius: 8px;"
        "padding: 8px 16px;"
        "font-size: 20px;"
        "font-weight: bold;"));
#else
    m_display->setStyleSheet(QStringLiteral(
        "color: #ff6b8a;"
        "background-color: rgba(40, 8, 16, 0.55);"
        "border: 1px solid rgba(255, 80, 120, 0.55);"
        "border-radius: 14px;"
        "padding: 8px 18px;"
        "font-size: 22px;"
        "font-weight: bold;"));
#endif
}

void CalculatorWidget::applyOperator(const QString &opText)
{
    if (m_error || opText.isEmpty())
        return;

    if (!m_freshInput || !m_pendingOp.isNull())
        applyPendingOperation();
    if (m_error)
        return;

    m_pendingOp = opText.at(0);
    m_freshInput = true;
}

void CalculatorWidget::onEqualClicked()
{
    if (m_error)
        return;

    applyPendingOperation();
    m_pendingOp = QChar();
    m_freshInput = true;
}

void CalculatorWidget::onClearClicked()
{
    m_input.clear();
    m_accumulator = 0.0;
    m_pendingOp = QChar();
    m_freshInput = true;
    m_error = false;
    m_display->setStyleSheet(displayStyleSheet());
    updateDisplay();
}

void CalculatorWidget::onBackspaceClicked()
{
    if (m_error) {
        onClearClicked();
        return;
    }
    if (m_freshInput)
        return;
    if (!m_input.isEmpty())
        m_input.chop(1);
    if (m_input.isEmpty())
        m_input = QStringLiteral("0");
    updateDisplay();
}

void CalculatorWidget::onDecimalClicked()
{
    if (m_error) {
        m_input = QStringLiteral("0.");
        m_error = false;
        m_freshInput = false;
        m_display->setStyleSheet(displayStyleSheet());
        updateDisplay();
        return;
    }

    if (m_freshInput) {
        m_input = QStringLiteral("0.");
        m_freshInput = false;
    } else if (!m_input.contains(QLatin1Char('.'))) {
        if (m_input.isEmpty())
            m_input = QStringLiteral("0.");
        else
            m_input += QLatin1Char('.');
    }
    updateDisplay();
}
