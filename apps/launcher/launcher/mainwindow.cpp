#include "mainwindow.h"

#include "ap3216c.h"
#include "bootanimationpage.h"
#include "calculator.h"
#include "camerapage.h"
#include "glowtext.h"
#include "mappage.h"
#include "musicpage.h"
#include "swipeview.h"
#include "videopage.h"
#include "wavegauge.h"
#include "weatherpage.h"

#include <QCoreApplication>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEasingCurve>
#include <QFile>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScreen>
#include <QPropertyAnimation>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>
#include <functional>
#if defined(__arm__)
#include <cstdlib>
#endif

namespace {

// 浠庤矾寰勫姞杞藉浘鐗囷紙QImage::load 浼氭寜鏂囦欢鍐呭璇嗗埆鏍煎紡锛屼笉浠呯湅鎵╁睍鍚嶏級
static QPixmap loadImageFromPath(const QString &path)
{
    QImage image;
    if (image.load(path))
        return QPixmap::fromImage(image);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QPixmap();

    QImage fromData;
    if (fromData.loadFromData(file.readAll()))
        return QPixmap::fromImage(fromData);

    return QPixmap();
}

// 鍔犺浇涓婚〉鑳屾櫙鍥撅細浼樺厛 Qt 璧勬簮锛屽け璐ュ垯灏濊瘯纾佺洏璺緞
static QPixmap loadBackgroundPixmap()
{
    const QString resPath = QStringLiteral(":/picture/background.png");

    QPixmap pix = loadImageFromPath(resPath);
    if (!pix.isNull())
        return pix;

        qWarning() << QString::fromUtf8("资源背景加载失败:") << resPath;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList diskPaths = {
        appDir + QStringLiteral("/picture/background.png"),
        appDir + QStringLiteral("/../../launcher/picture/background.png"),
        appDir + QStringLiteral("/../../../launcher/launcher/picture/background.png"),
    };

    for (const QString &path : diskPaths) {
        const QString absPath = QDir(path).absolutePath();
        if (!QFile::exists(absPath))
            continue;
        pix = loadImageFromPath(absPath);
        if (!pix.isNull()) {
            qInfo() << "宸蹭粠纾佺洏鍔犺浇鑳屾櫙:" << absPath;
            return pix;
        }
        qWarning() << QString::fromUtf8("磁盘背景加载失败:") << absPath;
    }

    qWarning() << QString::fromUtf8("背景图加载失败");
    return QPixmap();
}

// 椤堕儴鏃ユ湡鏃堕棿鏍忥細鍦嗚鑳跺泭瀹瑰櫒锛屽乏鏃ユ湡 / 涓椂闂?/ 鍙虫槦鏈?
static QWidget *createStatusBar(QWidget *parent)
{
    // ---------- 澶栧眰瀹瑰櫒锛氬乏鍙崇暀鐧斤紝鑳跺泭涓嶈创灞忓箷杈圭紭 ----------
    auto *wrapper = new QWidget(parent);
    wrapper->setAutoFillBackground(false);
    wrapper->setStyleSheet(QStringLiteral("background: transparent;"));

    auto *wrapperLayout = new QHBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(20, 10, 20, 0);
    wrapperLayout->setSpacing(0);

    // ---------- 鑳跺泭鏈綋锛氬崐閫忔槑娣辫摑鐏?+ 鍦嗚 ----------
    auto *bar = new QWidget(wrapper);
    bar->setObjectName(QStringLiteral("statusPill"));
    bar->setAttribute(Qt::WA_StyledBackground, true);  // 璁╂牱寮忚〃鍦嗚/鑳屾櫙鐢熸晥
    bar->setFixedHeight(52);
    bar->setStyleSheet(QStringLiteral(
        "#statusPill {"
        "  background-color: rgba(45, 55, 72, 0.72);"
        "  border-radius: 26px;"
        "  border: none;"
        "}"));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(22, 0, 22, 0);
    layout->setSpacing(0);

    // 宸﹀彸鏃ユ湡/鏄熸湡锛氶鑹叉洿娣★紝浣滀负杈呭姪淇℃伅
    const QString sideStyle = QStringLiteral(
        "color: rgba(100, 150, 255, 0.75);"
        "font-size: 14px;"
        "background: transparent;");

    // 涓棿鏃堕棿锛氫寒鐏拌摑鑹诧紝瑙嗚鐒︾偣
    const QString centerStyle = QStringLiteral(
        "color: #B8E4FF;"
        "background: transparent;");

    // 宸︿晶锛氬勾鏈堟棩
    auto *dateLabel = new QLabel(bar);
    dateLabel->setStyleSheet(sideStyle);
    dateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 涓棿锛氭椂:鍒?绉掞紙澶у彿鍔犵矖锛?
    auto *timeLabel = new QLabel(bar);
    QFont timeFont = timeLabel->font();
    timeFont.setPointSize(22);
    timeFont.setBold(true);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet(centerStyle);
    timeLabel->setAlignment(Qt::AlignCenter);

    // 鍙充晶锛氭槦鏈?
    auto *weekLabel = new QLabel(bar);
    weekLabel->setStyleSheet(sideStyle);
    weekLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 姣忕鍒锋柊鏃ユ湡銆佹椂闂淬€佹槦鏈燂紙涓枃鏍煎紡锛?
    auto updateDateTime = [dateLabel, timeLabel, weekLabel]() {
        const QDateTime now = QDateTime::currentDateTime();
        const QLocale locale(QLocale::Chinese, QLocale::China);
        dateLabel->setText(now.date().toString(QString::fromUtf8("yyyy年MM月dd日")));
        timeLabel->setText(now.toString(QStringLiteral("HH:mm:ss")));
        weekLabel->setText(locale.toString(now.date(), QStringLiteral("dddd")));
    };
    updateDateTime();

    auto *timer = new QTimer(bar);
    QObject::connect(timer, &QTimer::timeout, bar, updateDateTime);
    timer->start(1000);

    // ---------- 涓夌瓑鍒嗗竷灞€锛氬乏/涓?鍙冲悇鍗?1 浠斤紝鏃堕棿濮嬬粓姘村钩灞呬腑 ----------
    auto *leftBox = new QWidget(bar);
    leftBox->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *leftLayout = new QHBoxLayout(leftBox);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(dateLabel);

    auto *centerBox = new QWidget(bar);
    centerBox->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *centerLayout = new QHBoxLayout(centerBox);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->addWidget(timeLabel);

    auto *rightBox = new QWidget(bar);
    rightBox->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *rightLayout = new QHBoxLayout(rightBox);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(weekLabel);

    layout->addWidget(leftBox, 1);
    layout->addWidget(centerBox, 1);
    layout->addWidget(rightBox, 1);

    wrapperLayout->addWidget(bar);

    return wrapper;
}

class LightingPage : public QWidget
{
public:
    explicit LightingPage(QWidget *parent = nullptr);

private:
    QFile m_ledFile;
    QLabel *m_statusLabel;
    QLabel *m_lampImageLabel;
    QPushButton *m_toggleButton;
    int m_maxBrightness = 255;
    bool m_ledOn = false;

    int readBrightness();
    void readLedState();
    void setLedOn(bool on);
    void updateSwitchUi();
};

LightingPage::LightingPage(QWidget *parent)
    : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral(
      "LightingPage { background-color: #081018; }"
      "QLabel { color: #EAF6FF; background: transparent; }"));

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(24, 20, 24, 20);
  root->setSpacing(24);

  auto *lampPanel = new QWidget(this);
  lampPanel->setAttribute(Qt::WA_StyledBackground, true);
  lampPanel->setStyleSheet(QStringLiteral(
      "background-color: rgba(12,18,25,0.90);"
      "border: 1px solid rgba(255,255,255,0.10);"
      "border-radius: 8px;"));
  auto *lampLayout = new QVBoxLayout(lampPanel);
  lampLayout->setContentsMargins(18, 18, 18, 18);
  lampLayout->setSpacing(12);

  m_lampImageLabel = new QLabel(lampPanel);
  m_lampImageLabel->setAlignment(Qt::AlignCenter);
  const QPixmap lampPixmap(QStringLiteral(":/picture/lux.png"));
  if (!lampPixmap.isNull())
      m_lampImageLabel->setPixmap(lampPixmap.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));

  m_statusLabel = new QLabel(QString::fromUtf8("读取亮度中..."), lampPanel);
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_statusLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #F2FAFF;"));

  lampLayout->addStretch();
  lampLayout->addWidget(m_lampImageLabel);
  lampLayout->addWidget(m_statusLabel);
  lampLayout->addStretch();

  auto *controlPanel = new QWidget(this);
  controlPanel->setFixedWidth(220);
  controlPanel->setAttribute(Qt::WA_StyledBackground, true);
  controlPanel->setStyleSheet(QStringLiteral(
      "background-color: rgba(12,18,25,0.90);"
      "border: 1px solid rgba(255,255,255,0.10);"
      "border-radius: 8px;"));
  auto *controlLayout = new QVBoxLayout(controlPanel);
  controlLayout->setContentsMargins(16, 16, 16, 16);
  controlLayout->setSpacing(10);

  auto *titleLabel = new QLabel(QString::fromUtf8("灯光控制"), controlPanel);
  titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: bold; color: #F2FAFF;"));
  titleLabel->setAlignment(Qt::AlignCenter);
  controlLayout->addWidget(titleLabel);
  controlLayout->addStretch();

  m_toggleButton = new QPushButton(controlPanel);
  m_toggleButton->setMinimumSize(168, 68);
  m_toggleButton->setFocusPolicy(Qt::NoFocus);
  m_toggleButton->setCheckable(true);
  m_toggleButton->setStyleSheet(QStringLiteral(
      "QPushButton { background-color: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.18);"
      " border-radius: 8px; color: #EAF6FF; font-size: 22px; font-weight: bold; }"
      "QPushButton:pressed { background-color: rgba(95,178,255,0.28); }"
      "QPushButton:checked { background-color: rgba(255,198,72,0.34); border-color: rgba(255,210,96,0.78); color: #FFF4D0; }"
      "QPushButton:disabled { color: rgba(234,246,255,0.35); background-color: rgba(255,255,255,0.05); }"));
  connect(m_toggleButton, &QPushButton::clicked, this, [this]() { setLedOn(!m_ledOn); });
  controlLayout->addWidget(m_toggleButton, 0, Qt::AlignCenter);
  controlLayout->addStretch();

  root->addWidget(lampPanel, 1);
  root->addWidget(controlPanel, 0);

  m_ledFile.setFileName(QStringLiteral("/sys/devices/platform/leds/leds/sys-led/brightness"));

#if defined(__arm__)
  system("echo none > /sys/class/leds/sys-led/trigger");
#endif

  QFile maxFile(QStringLiteral("/sys/class/leds/sys-led/max_brightness"));
  if (maxFile.open(QIODevice::ReadOnly)) {
      bool ok = false;
      const int value = QString::fromLatin1(maxFile.readAll()).trimmed().toInt(&ok);
      if (ok && value > 0)
          m_maxBrightness = value;
  }

  if (!m_ledFile.exists()) {
#if defined(__arm__)
    m_statusLabel->setText(QString::fromUtf8("未获取到灯光设备"));
    m_toggleButton->setEnabled(false);
#else
    m_statusLabel->setText(QString::fromUtf8("PC 仿真：灯光关闭"));
#endif
  }

  readLedState();
}

int LightingPage::readBrightness()
{
  if (!m_ledFile.exists())
    return 0;

  if (!m_ledFile.open(QIODevice::ReadWrite)) {
    qDebug() << m_ledFile.errorString();
    return 0;
  }

  QTextStream in(&m_ledFile);
  const QString buf = in.readLine().trimmed();
  m_ledFile.close();

  bool ok = false;
  const int value = buf.toInt(&ok);
  return ok ? qBound(0, value, m_maxBrightness) : 0;
}

void LightingPage::readLedState()
{
  const int value = readBrightness();
  m_ledOn = value > 0;
  updateSwitchUi();
}

void LightingPage::setLedOn(bool on)
{
  m_ledOn = on;

#if !defined(__arm__)
  if (!m_ledFile.exists()) {
      updateSwitchUi();
      return;
  }
#endif

  if (!m_ledFile.exists())
      return;

  if (!m_ledFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      qDebug() << m_ledFile.errorString();
      updateSwitchUi();
      return;
  }
  m_ledFile.write(QByteArray::number(on ? m_maxBrightness : 0));
  m_ledFile.close();

  readLedState();
}

void LightingPage::updateSwitchUi()
{
#if defined(__arm__)
  m_statusLabel->setText(m_ledOn ? QString::fromUtf8("当前：已开启")
                                 : QString::fromUtf8("当前：已关闭"));
#else
  m_statusLabel->setText(m_ledOn ? QString::fromUtf8("PC 仿真：灯光开启")
                                 : QString::fromUtf8("PC 仿真：灯光关闭"));
#endif
  m_toggleButton->setChecked(m_ledOn);
  m_toggleButton->setText(m_ledOn ? QString::fromUtf8("关闭灯光")
                                  : QString::fromUtf8("打开灯光"));
}

namespace {

// 鏉跨浼犳劅鍣ㄩ〉缁熶竴鑹叉澘锛堥〉闈€佸崱鐗囥€佷华琛ㄥ妗嗕竴鑷达紝閬垮厤鑹插樊锛?
constexpr const char *kSensorPageBg0 = "#0a0e22";
constexpr const char *kSensorPageBg1 = "#0c1538";
constexpr const char *kSensorPanelBg = "#0b1128";
constexpr const char *kSensorPanelBorder = "#1e5a7a";
constexpr const char *kSensorGridLine = "#1a3555";

} // namespace

// 浼犳劅鍣ㄩ〉 HUD 鑳屾櫙锛堟澘绔敤涓嶉€忔槑鑹诧紝閬垮厤 linuxfb 閫忔槑/娓愬彉寮傚父锛?
static void paintSensorHudBackground(QPainter &p, const QRect &rect)
{
#if defined(__arm__)
    QLinearGradient base(rect.topLeft(), rect.bottomRight());
    base.setColorAt(0.0, QColor(kSensorPageBg0));
    base.setColorAt(0.5, QColor(kSensorPageBg1));
    base.setColorAt(1.0, QColor(kSensorPageBg0));
    p.fillRect(rect, base);

    QPen gridPen{QColor{kSensorGridLine}};
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
    base.setColorAt(0.42, QColor(QStringLiteral("#0c1538")));
    base.setColorAt(1.0, QColor(QStringLiteral("#050a18")));
    p.fillRect(rect, base);

    QRadialGradient glow(rect.center(), qreal(qMax(rect.width(), rect.height())) * 0.55);
    glow.setColorAt(0.0, QColor(0, 229, 255, 28));
    glow.setColorAt(0.35, QColor(124, 77, 255, 14));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);

    p.save();
    QPen gridPen(QColor(0, 229, 255, 16));
    gridPen.setWidth(1);
    p.setPen(gridPen);
    const int gridStep = 24;
    for (int x = rect.left(); x <= rect.right(); x += gridStep)
        p.drawLine(x, rect.top(), x, rect.bottom());
    for (int y = rect.top(); y <= rect.bottom(); y += gridStep)
        p.drawLine(rect.left(), y, rect.right(), y);
    p.restore();

    QLinearGradient topBand(0, 0, 0, 48);
    topBand.setColorAt(0.0, QColor(0, 229, 255, 55));
    topBand.setColorAt(1.0, QColor(0, 229, 255, 0));
    p.fillRect(QRect(rect.left(), rect.top(), rect.width(), 48), topBand);

    QPen accent(QColor(0, 229, 255, 90));
    accent.setWidth(2);
    p.setPen(accent);
    p.drawLine(rect.left() + 16, rect.top() + 1, rect.right() - 16, rect.top() + 1);

    QPen bottomLine(QColor(124, 77, 255, 45));
    bottomLine.setWidth(1);
    p.setPen(bottomLine);
    p.drawLine(rect.left() + 24, rect.bottom() - 2, rect.right() - 24, rect.bottom() - 2);
#endif
}

// 鍒楀鍣細鏉跨涓嶇敤 QSS锛岀洿鎺ョ敾杈规锛坙inuxfb 涓?styleSheet 甯稿け鏁堬級
class SensorColumnFrame : public QWidget
{
public:
    explicit SensorColumnFrame(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.fillRect(rect(), QColor(kSensorPanelBg));
        QPen pen{QColor{kSensorPanelBorder}};
        pen.setWidth(2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
};

// 浼犳劅鍣ㄩ〉锛氫笁鍒楀苟鎺掞紙绾㈠ | 鎺ヨ繎璺濈 | 鐜鍏夛級
class SensorPage : public QWidget
{
public:
    explicit SensorPage(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_sensor(new Ap3216c(this))
    {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);

        auto *root = new QVBoxLayout(this);
#if defined(__arm__)
        root->setContentsMargins(4, 6, 4, 4);
#else
        root->setContentsMargins(16, 24, 16, 12);
#endif
        root->setSpacing(8);

        auto *row = new QHBoxLayout;
#if defined(__arm__)
        row->setSpacing(6);
        row->setContentsMargins(4, 0, 4, 0);
#else
        row->setSpacing(12);
#endif

        const auto addCol = [row](QWidget *col) {
#if defined(__arm__)
            col->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            row->addWidget(col, 1);
#else
            row->addWidget(col);
#endif
        };

        addCol(createColumn(QString::fromUtf8("红外强度"),
                            &m_gaugeIr, &m_valueIr,
                            QColor(QStringLiteral("#00e5ff")),
                            QColor(QStringLiteral("#0066ff"))));
        addCol(createColumn(QString::fromUtf8("接近距离"),
                            &m_gaugePs, &m_valuePs,
                            QColor(QStringLiteral("#ff2bd6")),
                            QColor(QStringLiteral("#7c4dff"))));
        addCol(createColumn(QString::fromUtf8("光照强度"),
                            &m_gaugeAls, &m_valueAls,
                            QColor(QStringLiteral("#00ffc8")),
                            QColor(QStringLiteral("#0088ff"))));

#if defined(__arm__)
        root->addLayout(row, 1);
#else
        root->addStretch();
        root->addLayout(row, 1);
        root->addStretch();
#endif

        connect(m_sensor, &Ap3216c::ap3216cDataChanged, this, [this]() {
            updateDisplay();
        });

#if defined(__arm__)
        m_sensor->setCapture(true);
#else
        m_gaugeIr->setPercent(22);
        m_gaugePs->setPercent(58);
        m_gaugeAls->setPercent(35);
        m_valueIr->setTextData(QStringLiteral("--"));
        m_valuePs->setTextData(QStringLiteral("--"));
        m_valueAls->setTextData(QStringLiteral("--"));
#endif
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
#if defined(__arm__)
        paintSensorHudBackground(painter, rect());
#else
        painter.setRenderHint(QPainter::Antialiasing);
        paintSensorHudBackground(painter, rect());
#endif
        QWidget::paintEvent(event);
    }

private:
    static QWidget *createColumn(const QString &title,
                                 WaveGauge **gaugeOut,
                                 GlowText **valueOut,
                                 const QColor &primary,
                                 const QColor &secondary)
    {
#if defined(__arm__)
        auto *col = new SensorColumnFrame;
#else
        auto *col = new QWidget;
        col->setAttribute(Qt::WA_StyledBackground, true);
        col->setStyleSheet(QStringLiteral(
            "background-color: rgba(8, 14, 36, 0.62);"
            "border: 1px solid rgba(0, 229, 255, 0.28);"
            "border-radius: 14px;"));
#endif

        auto *layout = new QVBoxLayout(col);
#if defined(__arm__)
        layout->setContentsMargins(4, 6, 4, 8);
        layout->setSpacing(4);
#else
        layout->setContentsMargins(12, 14, 12, 14);
        layout->setSpacing(8);
#endif
        layout->setAlignment(Qt::AlignHCenter);

        auto *gauge = new WaveGauge(col);
#if defined(__arm__)
        gauge->setFixedSize(160, 160);
        gauge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
#else
        gauge->setMinimumSize(140, 140);
#endif
        gauge->setAccentColors(primary, secondary);

        auto *titleLabel = new QLabel(title, col);
#if defined(__arm__)
        {
            QPalette titlePal = titleLabel->palette();
            titlePal.setColor(QPalette::WindowText, QColor(QStringLiteral("#7ec8ff")));
            titleLabel->setPalette(titlePal);
        }
        QFont titleFont = titleLabel->font();
        titleFont.setPixelSize(32);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setWordWrap(true);
        titleLabel->setAlignment(Qt::AlignCenter);
#else
        titleLabel->setStyleSheet(QStringLiteral(
            "color: #7ec8ff;"
            "font-size: 14px;"
            "font-weight: bold;"
            "background: transparent;"));
#endif

        auto *value = new GlowText(col);
#if defined(__arm__)
        value->setFontSize(36);
#else
        value->setFontSize(16);
#endif
        value->setTextColor(primary);

#if defined(__arm__)
        auto *labelBox = new QVBoxLayout;
        labelBox->setContentsMargins(4, 0, 4, 0);
        labelBox->setSpacing(6);
        labelBox->addWidget(titleLabel, 0, Qt::AlignHCenter);
        labelBox->addWidget(value, 0, Qt::AlignHCenter);

        layout->addStretch(1);
        layout->addWidget(gauge, 0, Qt::AlignHCenter);
        layout->addSpacing(8);
        layout->addLayout(labelBox, 0);
        layout->addStretch(2);
#else
        auto *textRow = new QWidget(col);
        textRow->setStyleSheet(QStringLiteral("background: transparent;"));
        auto *textLayout = new QHBoxLayout(textRow);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(4);
        titleLabel->setParent(textRow);
        value->setParent(textRow);
        textLayout->addWidget(titleLabel);
        textLayout->addWidget(value, 0, Qt::AlignVCenter);
        layout->addWidget(gauge, 0, Qt::AlignHCenter);
        layout->addWidget(textRow, 0, Qt::AlignHCenter);
#endif

        *gaugeOut = gauge;
        *valueOut = value;
        return col;
    }

    void updateDisplay()
    {
        const QString ir = m_sensor->irData();
        const QString ps = m_sensor->psData();
        const QString als = m_sensor->alsData();

        m_gaugeIr->setPercent(int(ir.toUInt()) * 100 / 1023);
        m_gaugePs->setPercent(int(ps.toUInt()) * 100 / 1023);
        m_gaugeAls->setPercent(int(als.toUInt()) * 100 / 65535);

        m_valueIr->setTextData(ir);
        m_valuePs->setTextData(ps);
        m_valueAls->setTextData(als);
    }

    Ap3216c *m_sensor;
    WaveGauge *m_gaugeIr = nullptr;
    WaveGauge *m_gaugePs = nullptr;
    WaveGauge *m_gaugeAls = nullptr;
    GlowText *m_valueIr = nullptr;
    GlowText *m_valuePs = nullptr;
    GlowText *m_valueAls = nullptr;
};


// 涓婚〉 App 鍏ュ彛锛氳嚜缁樺浘鏍?鏂囧瓧锛岃嚜宸卞鐞?hover/鐐瑰嚮锛岄伩鍏嶈婊戝姩灞傛嫤鎴?
class AppTile : public QWidget
{
public:
    using ActivateHandler = std::function<void(QWidget *)>;

    AppTile(const QString &text, const QString &iconPath, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_text(text)
        , m_icon(iconPath)
    {
        setFixedSize(140, 110);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void setActivateHandler(ActivateHandler handler)
    {
        m_handler = std::move(handler);
    }

protected:
    void enterEvent(QEvent *event) override
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            event->accept();
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_handler)
            m_handler(this);
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (m_hovered) {
            painter.setBrush(QColor(255, 255, 255, 40));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 12, 12);
        }

        const QPixmap icon = loadImageFromPath(m_icon);
        const int iconSize = 64;
        const int iconY = 10;
        if (!icon.isNull()) {
            const QPixmap scaled = icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
            const int iconX = (width() - scaled.width()) / 2;
            painter.drawPixmap(iconX, iconY, scaled);
        }

        painter.setPen(QColor(QStringLiteral("#ffffff")));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        const int textTop = iconY + iconSize + 6;
        painter.drawText(QRect(0, textTop, width(), height() - textTop),
                         Qt::AlignHCenter | Qt::AlignTop, m_text);
    }

private:
    QString m_text;
    QString m_icon;
    ActivateHandler m_handler;
    bool m_hovered = false;
};


// 甯﹁儗鏅浘鐨勪富椤靛鍣細搴曞眰 QLabel 鏄剧ず鑳屾櫙锛屼笂灞傜敤鏍囧噯甯冨眬鏀炬帶浠?
class HomePageWidget : public QWidget
{
public:
    explicit HomePageWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_bg(new QLabel(this))
        , m_layout(new QVBoxLayout(this))
    {
        m_bg->setScaledContents(true);
        m_bg->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_bg->lower();

        setAutoFillBackground(false);

        const QPixmap bgPix = loadBackgroundPixmap();
        if (!bgPix.isNull())
            m_bg->setPixmap(bgPix);

        m_layout->setContentsMargins(0, 0, 0, 16);
        m_layout->setSpacing(0);
    }

    QVBoxLayout *contentLayout() const { return m_layout; }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        m_bg->setGeometry(rect());
        m_bg->lower();
    }

private:
    QLabel *m_bg;
    QVBoxLayout *m_layout;
};


// 搴曢儴鍦嗙偣椤电爜鎸囩ず锛堜富鐣岄潰婊戝姩鐢級
class PageDots : public QWidget
{
public:
    explicit PageDots(int count, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_count(qMax(1, count))
    {
        setFixedHeight(28);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    void setActiveIndex(int index)
    {
        index = qBound(0, index, m_count - 1);
        if (m_active == index)
            return;
        m_active = index;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int dot = 10;
        const int gap = 14;
        const int totalW = m_count * dot + (m_count - 1) * gap;
        int x = (width() - totalW) / 2 + dot / 2;
        const int cy = height() / 2;

        for (int i = 0; i < m_count; ++i) {
            if (i == m_active) {
                p.setBrush(QColor(QStringLiteral("#00e5ff")));
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(x, cy), 7, 7);
            } else {
                p.setBrush(QColor(255, 255, 255, 70));
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(x, cy), dot / 2, dot / 2);
            }
            x += dot + gap;
        }
    }

private:
    int m_count;
    int m_active = 0;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_stack(new QStackedWidget(this))
{
    // IMX6ULL 甯哥敤 RGB 灞忎负 800x480锛涘嬁鐢?QScreen::geometry()锛坙inuxfb 甯告姤鍙?
    resize(800, 480);

    setWindowTitle(QString::fromUtf8("车载操作系统"));
    setCentralWidget(m_stack);

#if defined(__arm__)
    const QColor windowBg(QStringLiteral("#0a0e22"));
    QPalette windowPal = palette();
    windowPal.setColor(QPalette::Window, windowBg);
    setPalette(windowPal);
    m_stack->setAutoFillBackground(true);
    m_stack->setPalette(windowPal);
#else
    m_stack->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
#endif

    // 鍏ㄥ眬鎹曡幏鎸夐敭锛氬瓙椤甸潰鏈夌劍鐐规椂涔熻兘鍝嶅簲 KEY0 杩斿洖涓婚〉
    qApp->installEventFilter(this);
    setFocusPolicy(Qt::StrongFocus);
    setupBackKeyHandling();

    // ---------- 娉ㄥ唽鎵€鏈夐〉闈紙椤哄簭蹇呴』涓?PageIndex 鏋氫妇涓€鑷达級----------
    m_stack->addWidget(createHomePage());                                    // 绱㈠紩 0
    m_stack->addWidget(createMusicPage());                                   // 绱㈠紩 1
    m_stack->addWidget(createVideoPage());                                    // index 2
    m_stack->addWidget(createLightingPage()); // 绱㈠紩 3
    m_stack->addWidget(createSensorPage());                                  // 绱㈠紩 4
    m_stack->addWidget(createMapPage());                                     // index 5
    m_stack->addWidget(createWeatherPage());                                  // index 6
    m_stack->addWidget(createCameraPage());                                  // index 7
    m_stack->addWidget(createCalculatorPage());                            // 绱㈠紩 8

    m_stack->setCurrentIndex(PageHome);  // 鍚姩鏃舵樉绀轰富椤?
    startBootAnimation();
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::createHomePage()
{
    auto *page = new HomePageWidget;
    auto *layout = page->contentLayout();
    layout->setContentsMargins(0, 0, 0, 10);

    layout->addWidget(createStatusBar(page));

    auto *appsPanel = new QWidget(page);
    appsPanel->setAutoFillBackground(false);
    auto *appLayout = new QVBoxLayout(appsPanel);
    appLayout->setContentsMargins(24, 8, 24, 0);
    appLayout->setSpacing(12);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(16);

    struct AppEntry {
        const char *text;
        const char *iconPath;
        int pageIndex;
    };

    const AppEntry apps[] = {
        { "音乐播放", ":/picture/music.png",   PageMusic },
        { "视频播放", ":/picture/vedio.png",   PageVideo },
        { "灯光控制", ":/picture/light.png",   PageLighting },
        { "传感器中心", ":/picture/sensor.png", PageSensor },
        { "百度地图", ":/picture/map.png",     PageMap },
        { "天气预报", ":/picture/weather.png", PageWeather },
        { "相机", ":/picture/camera.png",  PageCamera },
    };

    const int columnCount = 3;

    for (int i = 0; i < int(sizeof(apps) / sizeof(apps[0])); ++i) {
        const AppEntry &app = apps[i];

        auto *tile = new AppTile(QString::fromUtf8(app.text),
                                 QString::fromUtf8(app.iconPath),
                                 appsPanel);
        tile->setActivateHandler([this, app](QWidget *source) {
            showPageWithOpenAnimation(app.pageIndex, source);
        });
        m_pageTiles.insert(app.pageIndex, tile);

        grid->addWidget(tile, i / columnCount, i % columnCount, Qt::AlignCenter);
    }

    appLayout->addStretch();
    appLayout->addLayout(grid);
    appLayout->addStretch();

    auto *swipe = new SwipeView(page);
    swipe->setObjectName(QStringLiteral("homeSwipe"));
    swipe->addPage(appsPanel);

    auto *calcPanel = new QWidget;
    calcPanel->setAutoFillBackground(false);
    auto *calcAppLayout = new QVBoxLayout(calcPanel);
    calcAppLayout->setContentsMargins(24, 8, 24, 0);
    calcAppLayout->setSpacing(0);

    auto *calcTile = new AppTile(QString::fromUtf8("计算器"),
                                 QStringLiteral(":/picture/computer.png"),
                                 calcPanel);
    calcTile->setActivateHandler([this](QWidget *source) {
        showPageWithOpenAnimation(PageCalculator, source);
    });
    m_pageTiles.insert(PageCalculator, calcTile);
    calcAppLayout->addWidget(calcTile, 0, Qt::AlignLeft | Qt::AlignTop);
    calcAppLayout->addStretch();
    swipe->addPage(calcPanel);

    auto *dots = new PageDots(swipe->pageCount(), page);
    QObject::connect(swipe, &SwipeView::currentIndexChanged, dots, &PageDots::setActiveIndex);

    layout->addWidget(swipe, 1);
    layout->addWidget(dots, 0, Qt::AlignHCenter);

    return page;
}

QWidget *MainWindow::createPlaceholderPage(const QString &title)
{
    QWidget *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    // 涓棿鎻愮ず鏂囧瓧锛堝悗缁浛鎹负鐪熷疄鍔熻兘鐣岄潰锛?
    auto *label = new QLabel(title + QString::fromUtf8("（开发中）"));
    QFont font = label->font();
    font.setPointSize(16);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();

    return page;
}

QWidget *MainWindow::createLightingPage()
{
    return new LightingPage;
}

QWidget *MainWindow::createSensorPage()
{
    return new SensorPage;
}

QWidget *MainWindow::createCalculatorPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new CalculatorWidget(page));
    return page;
}

QWidget *MainWindow::createMusicPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new MusicPageWidget(page));
    return page;
}

QWidget *MainWindow::createVideoPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new VideoPageWidget(page));
    return page;
}

QWidget *MainWindow::createMapPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new MapPageWidget(page));
    return page;
}

QWidget *MainWindow::createWeatherPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new WeatherPageWidget(page));
    return page;
}

QWidget *MainWindow::createCameraPage()
{
    auto *page = new QWidget;
    page->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new CameraPageWidget(page));
    return page;
}

void MainWindow::showPage(int index)
{
    // 闃叉闈炴硶绱㈠紩瀵艰嚧宕╂簝
    if (index >= PageHome && index < PageCount) {
        if (m_stack->currentIndex() == PageVideo && index != PageVideo) {
            if (auto *videoPage = m_stack->widget(PageVideo)->findChild<VideoPageWidget *>())
                videoPage->stopPlayback();
        }
#if defined(__arm__)
        if (index == PageVideo && m_stack->currentIndex() != PageVideo)
            std::system("killall -q mplayer 2>/dev/null");
#endif
        m_stack->setCurrentIndex(index);
#if defined(__arm__)
        // 瀛愰〉鎸夐挳浼氭姠鐒︾偣锛屽垏椤靛悗鎶婄劍鐐规媺鍥炰富绐楀彛浠ヤ究 KEY0/鏂瑰悜閿敓鏁?
        setFocus(Qt::OtherFocusReason);
#endif
    }
}

void MainWindow::showPageWithOpenAnimation(int index, QWidget *source)
{
    if (!source || index <= PageHome || index >= PageCount) {
        showPage(index);
        return;
    }

    QWidget *targetPage = m_stack->widget(index);
    QPixmap targetShot(m_stack->size());
    targetShot.fill(Qt::transparent);
    targetPage->resize(m_stack->size());
    targetPage->render(&targetShot);

    if (targetShot.isNull()) {
        showPage(index);
        return;
    }

    const QRect stackRect = m_stack->rect();
    const QPoint sourceTopLeft = source->mapTo(m_stack, QPoint(0, 0));
    QRect startRect(sourceTopLeft + QPoint(source->width() / 2 - 18, source->height() / 2 - 18),
                    QSize(36, 36));
    if (!stackRect.contains(startRect.center()))
        startRect = QRect(stackRect.center() - QPoint(18, 18), QSize(36, 36));

    auto *overlay = new QLabel(m_stack);
    overlay->setScaledContents(true);
    overlay->setPixmap(targetShot);
    overlay->setGeometry(startRect);
    overlay->setStyleSheet(QStringLiteral("background-color: #071016; border-radius: 10px;"));
    overlay->raise();
    overlay->show();

    auto *opacity = new QGraphicsOpacityEffect(overlay);
    opacity->setOpacity(0.25);
    overlay->setGraphicsEffect(opacity);

    auto *group = new QParallelAnimationGroup(overlay);
    auto *geoAnim = new QPropertyAnimation(overlay, "geometry", group);
    geoAnim->setDuration(260);
    geoAnim->setStartValue(startRect);
    geoAnim->setEndValue(stackRect);
    geoAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *opacityAnim = new QPropertyAnimation(opacity, "opacity", group);
    opacityAnim->setDuration(220);
    opacityAnim->setStartValue(0.25);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    connect(group, &QParallelAnimationGroup::finished, this, [this, index, overlay]() {
        overlay->deleteLater();
        showPage(index);
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::showHomeWithCloseAnimation()
{
    const int currentIndex = m_stack->currentIndex();
    if (currentIndex == PageHome) {
        return;
    }

    QWidget *targetTile = m_pageTiles.value(currentIndex, nullptr);
    QPixmap currentShot = m_stack->widget(currentIndex)->grab();
    if (!targetTile || currentShot.isNull()) {
        showHome();
        return;
    }

    const QRect stackRect = m_stack->rect();
    const QPoint targetTopLeft = targetTile->mapTo(m_stack, QPoint(0, 0));
    QRect endRect(targetTopLeft + QPoint(targetTile->width() / 2 - 18, targetTile->height() / 2 - 18),
                  QSize(36, 36));
    if (!stackRect.contains(endRect.center()))
        endRect = QRect(stackRect.center() - QPoint(18, 18), QSize(36, 36));

    auto *overlay = new QLabel(m_stack);
    overlay->setScaledContents(true);
    overlay->setPixmap(currentShot);
    overlay->setGeometry(stackRect);
    overlay->setStyleSheet(QStringLiteral("background-color: #071016; border-radius: 10px;"));
    overlay->raise();
    overlay->show();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    m_stack->setCurrentIndex(PageHome);
    overlay->raise();

    auto *opacity = new QGraphicsOpacityEffect(overlay);
    opacity->setOpacity(1.0);
    overlay->setGraphicsEffect(opacity);

    auto *group = new QParallelAnimationGroup(overlay);
    auto *geoAnim = new QPropertyAnimation(overlay, "geometry", group);
    geoAnim->setDuration(230);
    geoAnim->setStartValue(stackRect);
    geoAnim->setEndValue(endRect);
    geoAnim->setEasingCurve(QEasingCurve::InCubic);

    auto *opacityAnim = new QPropertyAnimation(opacity, "opacity", group);
    opacityAnim->setDuration(210);
    opacityAnim->setStartValue(1.0);
    opacityAnim->setEndValue(0.05);
    opacityAnim->setEasingCurve(QEasingCurve::InCubic);

    connect(group, &QParallelAnimationGroup::finished, this, [this, overlay]() {
        overlay->deleteLater();
#if defined(__arm__)
        setFocus(Qt::OtherFocusReason);
        forceFramebufferRefresh();
#endif
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::startBootAnimation()
{
    if (!m_stack)
        return;

    auto *boot = new BootAnimationPage(m_stack);
    QRect bootRect = m_stack->rect();
    if (bootRect.isEmpty())
        bootRect = QRect(0, 0, width() > 0 ? width() : 800, height() > 0 ? height() : 480);
    boot->setGeometry(bootRect);
    boot->raise();
    boot->show();
    m_bootOverlay = boot;

    QTimer::singleShot(0, this, [this, boot]() {
        if (!boot || !m_stack)
            return;
        boot->setGeometry(m_stack->rect());
        boot->raise();
        boot->start();
    });

    connect(boot, &BootAnimationPage::finished, this, [this, boot]() {
        auto *opacity = new QGraphicsOpacityEffect(boot);
        opacity->setOpacity(1.0);
        boot->setGraphicsEffect(opacity);

        auto *fade = new QPropertyAnimation(opacity, "opacity", boot);
        fade->setDuration(160);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(fade, &QPropertyAnimation::finished, boot, [this, boot]() {
            if (m_bootOverlay == boot)
                m_bootOverlay = nullptr;
            boot->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_bootOverlay && m_stack) {
        m_bootOverlay->setGeometry(m_stack->rect());
        m_bootOverlay->raise();
        if (auto *boot = qobject_cast<BootAnimationPage *>(m_bootOverlay))
            boot->start();
    }
}

namespace {

// 涓插彛/TTY 妯″紡涓?KEY0 甯镐笂鎶ヤ负璇ヨ浆涔変覆锛堝睆骞曞彲瑙?^[[26~锛?
const QLatin1String kKey0TtySequence("\x1B[26~");

bool isBackKeyCode(int key)
{
#if defined(__arm__)
    return key == Qt::Key_VolumeDown
        || key == Qt::Key_Down
        || key == Qt::Key_Home
        || key == Qt::Key_Escape;
#else
    return key == Qt::Key_Down;
#endif
}

bool isBackNativeKey(quint32 nativeCode)
{
#if defined(__arm__)
    // Linux input: KEY_VOLUMEDOWN=114, KEY_PROG1=148锛堥儴鍒嗘澘绾?KEY0 鏄犲皠锛?
    return nativeCode == 114u || nativeCode == 148u;
#else
    Q_UNUSED(nativeCode);
    return false;
#endif
}

bool isBackKeyText(const QString &text)
{
    return text.contains(kKey0TtySequence);
}

} // namespace

void MainWindow::setupBackKeyHandling()
{
    const auto addBackShortcut = [this](Qt::Key key) {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::ApplicationShortcut);
        shortcut->setAutoRepeat(false);
        connect(shortcut, &QShortcut::activated, this, &MainWindow::onBackKeyActivated);
    };

#if defined(__arm__)
    addBackShortcut(Qt::Key_VolumeDown);
    addBackShortcut(Qt::Key_Down);
#else
    addBackShortcut(Qt::Key_Down);
#endif
}

bool MainWindow::handleBackKey(QKeyEvent *event)
{
    if (!event || event->isAutoRepeat())
        return false;

    const QString text = event->text();
    if (!text.isEmpty()) {
        m_key0EscapeBuffer += text;
        if (m_key0EscapeBuffer.size() > 16)
            m_key0EscapeBuffer = m_key0EscapeBuffer.right(16);

        if (m_key0EscapeBuffer.contains(kKey0TtySequence)) {
            m_key0EscapeBuffer.clear();
            if (m_stack->currentIndex() != PageHome)
                onBackKeyActivated();
            return true;
        }

        if (m_key0EscapeBuffer.contains(QLatin1Char('~'))
            && !m_key0EscapeBuffer.contains(kKey0TtySequence)) {
            m_key0EscapeBuffer.clear();
        }

        // 姝ｅ湪杈撳叆杞箟搴忓垪鏃跺悶鎺夊瓧绗︼紝閬垮厤灞忓箷涓婂嚭鐜?^[[26~
        if (m_key0EscapeBuffer.contains(QLatin1Char('\x1B'))
            && !m_key0EscapeBuffer.contains(QLatin1Char('~'))) {
            return true;
        }

        if (isBackKeyText(text)) {
            m_key0EscapeBuffer.clear();
            if (m_stack->currentIndex() != PageHome)
                onBackKeyActivated();
            return true;
        }
    }

    if (isBackKeyCode(event->key()) || isBackNativeKey(event->nativeVirtualKey())) {
        m_key0EscapeBuffer.clear();
        if (m_stack->currentIndex() == PageHome)
            return true;
        onBackKeyActivated();
        return true;
    }

    return false;
}

void MainWindow::onBackKeyActivated()
{
    if (m_stack->currentIndex() != PageHome)
        showHome();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (handleBackKey(keyEvent))
            return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (handleBackKey(event))
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (handleBackKey(event))
        return;
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::showHome()
{
    if (m_stack->currentIndex() == PageVideo) {
        if (auto *videoPage = m_stack->widget(PageVideo)->findChild<VideoPageWidget *>())
            videoPage->stopPlayback();
    }
    showHomeWithCloseAnimation();
#if defined(__arm__)
    setFocus(Qt::OtherFocusReason);
    forceFramebufferRefresh();
#endif
}

void MainWindow::forceFramebufferRefresh()
{
#if defined(__arm__)
    update();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
    repaint();
    if (m_stack) {
        m_stack->repaint();
        if (QWidget *page = m_stack->currentWidget())
            page->repaint();
    }
#endif
}
