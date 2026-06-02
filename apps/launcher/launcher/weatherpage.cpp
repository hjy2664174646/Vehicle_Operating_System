#include "weatherpage.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QPixmap>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

namespace {

constexpr const char *kAmapKey = "a7838e34b0d36957124c9aa84bdaae3d";
constexpr const char *kFallbackAdcode = "320413";

QString cardStyle()
{
    return QStringLiteral(
        "QWidget { background-color: rgba(246,248,250,0.94); border-radius: 10px; }"
        "QLabel { background: transparent; color: #0b1520; }"
        "QPushButton { background: transparent; border: none; color: #0b1520; font-weight: bold; text-align: left; }");
}

QString darkButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color: #525862; color: white; border: none; border-radius: 8px;"
        " font-size: 18px; font-weight: bold; min-height: 38px; }"
        "QPushButton:pressed { background-color: #3f4650; }");
}

QString cityEditStyle()
{
    return QStringLiteral(
        "QLineEdit { background-color: rgba(255,255,255,0.96); border: 1px solid rgba(20,34,48,0.18);"
        " border-radius: 6px; color: #0b1520; font-size: 15px; min-height: 34px; padding-left: 10px; }");
}

struct CityEntry {
    const char *name;
    const char *pinyin;
    const char *abbr;
    const char *adcode;
};

const CityEntry kCities[] = {
    {"金坛区", "jintan", "jt", "320413"},
    {"常州市", "changzhou", "cz", "320400"},
    {"南京市", "nanjing", "nj", "320100"},
    {"上海市", "shanghai", "sh", "310000"},
    {"北京市", "beijing", "bj", "110000"},
    {"杭州市", "hangzhou", "hz", "330100"},
    {"苏州市", "suzhou", "suz", "320500"},
    {"无锡市", "wuxi", "wx", "320200"},
    {"广州市", "guangzhou", "gz", "440100"},
    {"深圳市", "shenzhen", "sz", "440300"}
};

} // namespace

class ForecastCard : public QWidget
{
public:
    std::function<void()> clickHandler;

    explicit ForecastCard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && clickHandler) {
            clickHandler();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
};

class WeatherChart : public QWidget
{
public:
    struct PointData {
        QString time;
        int temperature = 0;
        int humidity = 0;
    };

    explicit WeatherChart(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(126);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_animTimer.setInterval(16);
        connect(&m_animTimer, &QTimer::timeout, this, [this]() {
            const double delta = m_targetScroll - m_scroll;
            if (qAbs(delta) < 1.0) {
                m_scroll = m_targetScroll;
                m_animTimer.stop();
                update();
                return;
            }
            m_scroll += delta * 0.22;
            update();
        });
    }

    void setPoints(const QList<PointData> &points)
    {
        m_points = points;
        m_scroll = 0;
        m_targetScroll = 0;
        m_animTimer.stop();
        update();
    }

    void setUseFahrenheit(bool enabled)
    {
        m_useFahrenheit = enabled;
        update();
    }

    void scrollToPointIndex(int index)
    {
        if (m_points.isEmpty())
            return;
        const int left = 28;
        const int right = 20;
        const int usableW = qMax(1, width() - left - right);
        const double step = usableW / 5.0;
        const double maxScroll = qMax(0.0, step * double(m_points.size() - 1) - usableW);
        const double target = step * qBound(0, index, m_points.size() - 1) - usableW * 0.18;
        m_targetScroll = qBound(0.0, target, maxScroll);
        if (!m_animTimer.isActive())
            m_animTimer.start();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(255, 255, 255, 0));

        if (m_points.size() < 2)
            return;

        int minTemp = displayTemperature(m_points.first().temperature);
        int maxTemp = minTemp;
        for (const PointData &point : m_points) {
            const int temp = displayTemperature(point.temperature);
            minTemp = qMin(minTemp, temp);
            maxTemp = qMax(maxTemp, temp);
        }
        if (minTemp == maxTemp) {
            --minTemp;
            ++maxTemp;
        }

        const int left = 28;
        const int right = 20;
        const int top = 24;
        const int bottom = 34;
        const int usableW = qMax(1, width() - left - right);
        const int usableH = qMax(1, height() - top - bottom);
        const double step = usableW / 5.0;
        const double maxScroll = qMax(0.0, step * double(m_points.size() - 1) - usableW);
        m_scroll = qBound(0.0, m_scroll, maxScroll);

        QList<QPointF> tempPoints;
        QList<QPointF> humidityPoints;
        for (int i = 0; i < m_points.size(); ++i) {
            const double x = left + step * i - m_scroll;
            const int displayTemp = displayTemperature(m_points.at(i).temperature);
            const double ratio = double(displayTemp - minTemp) / double(maxTemp - minTemp);
            const double y = top + usableH * (1.0 - ratio);
            tempPoints << QPointF(x, y);

            const double humidityRatio = qBound(0.0, double(m_points.at(i).humidity) / 100.0, 1.0);
            humidityPoints << QPointF(x, top + usableH * (1.0 - humidityRatio));
        }

        QPainterPath fillPath;
        fillPath = smoothPath(tempPoints);
        fillPath.lineTo(tempPoints.last().x(), height() - bottom);
        fillPath.lineTo(tempPoints.first().x(), height() - bottom);
        fillPath.closeSubpath();

        painter.save();
        painter.setClipRect(QRect(left, 0, usableW, height()));
        QLinearGradient fillGrad(0, top, 0, height() - bottom);
        fillGrad.setColorAt(0.0, QColor(255, 112, 96, 96));
        fillGrad.setColorAt(1.0, QColor(255, 197, 120, 16));
        painter.fillPath(fillPath, fillGrad);

        QPainterPath linePath = smoothPath(tempPoints);
        painter.setPen(QPen(QColor(QStringLiteral("#f39a8f")), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(linePath);

        QPainterPath humidityPath = smoothPath(humidityPoints);
        QPen humidityPen(QColor(QStringLiteral("#2f9cff")), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(humidityPen);
        painter.drawPath(humidityPath);

        painter.setPen(QColor(QStringLiteral("#32475c")));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);
        for (int i = 0; i < tempPoints.size(); ++i) {
            if (tempPoints.at(i).x() < left - 30 || tempPoints.at(i).x() > width() - right + 30)
                continue;
            painter.drawText(QRectF(tempPoints.at(i).x() - 18, tempPoints.at(i).y() - 20, 36, 16),
                             Qt::AlignCenter,
                             QStringLiteral("%1°").arg(displayTemperature(m_points.at(i).temperature)));
            painter.setPen(QColor(QStringLiteral("#2f7ec7")));
            painter.drawText(QRectF(humidityPoints.at(i).x() - 20, humidityPoints.at(i).y() + 4, 42, 16),
                             Qt::AlignCenter,
                             QStringLiteral("%1%").arg(m_points.at(i).humidity));
            painter.setPen(QColor(QStringLiteral("#32475c")));
        }
        painter.restore();

        painter.setPen(QPen(QColor(QStringLiteral("#9aa7b5")), 1));
        painter.drawLine(left, height() - bottom, width() - right, height() - bottom);

        painter.setPen(QColor(QStringLiteral("#32475c")));
        for (int i = 0; i < m_points.size(); ++i) {
            const double x = left + step * i - m_scroll;
            if (x < left - 24 || x > width() - right + 24)
                continue;
            painter.drawText(QRectF(x - 24, height() - bottom + 6, 48, 16), Qt::AlignCenter, m_points.at(i).time);
        }

        painter.setPen(QColor(QStringLiteral("#687887")));
        painter.drawText(QRectF(width() - 104, 8, 96, 18), Qt::AlignRight | Qt::AlignVCenter, QString::fromUtf8("温度 / 湿度"));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_animTimer.stop();
            m_lastX = event->pos().x();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            const int dx = event->pos().x() - m_lastX;
            m_lastX = event->pos().x();
            m_scroll = qMax(0.0, m_scroll - double(dx));
            m_targetScroll = m_scroll;
            update();
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    static QPainterPath smoothPath(const QList<QPointF> &points)
    {
        QPainterPath path;
        if (points.isEmpty())
            return path;
        path.moveTo(points.first());
        for (int i = 1; i < points.size(); ++i) {
            const QPointF p0 = points.at(i - 1);
            const QPointF p1 = points.at(i);
            const double dx = (p1.x() - p0.x()) * 0.45;
            path.cubicTo(QPointF(p0.x() + dx, p0.y()),
                         QPointF(p1.x() - dx, p1.y()),
                         p1);
        }
        return path;
    }

    int displayTemperature(int celsius) const
    {
        return m_useFahrenheit ? qRound(celsius * 9.0 / 5.0 + 32.0) : celsius;
    }

    QList<PointData> m_points;
    QTimer m_animTimer;
    double m_scroll = 0.0;
    double m_targetScroll = 0.0;
    bool m_useFahrenheit = false;
    bool m_dragging = false;
    int m_lastX = 0;
};

WeatherPageWidget::WeatherPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setAutoFillBackground(false);
    buildUi();
    requestIpCity();
}

void WeatherPageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#09131c")));
    QRadialGradient glow(QPointF(width() * 0.28, height() * 0.18), qMax(width(), height()) * 0.85);
    glow.setColorAt(0.0, QColor(42, 110, 140, 160));
    glow.setColorAt(0.55, QColor(18, 32, 44, 120));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), glow);
}

void WeatherPageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncForecastStripGeometry();
}

void WeatherPageWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        syncForecastStripGeometry();
        updateForecastCards();
    });
}

void WeatherPageWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(0);

    m_card = new QWidget(this);
    m_card->setAttribute(Qt::WA_StyledBackground, true);
    m_card->setStyleSheet(cardStyle());
    root->addWidget(m_card);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(12);

    auto *topRow = new QHBoxLayout;
    m_cityButton = new QPushButton(QString::fromUtf8("金坛区  --:--"), m_card);
    m_cityButton->setFocusPolicy(Qt::NoFocus);
    m_unitButton = new QPushButton(QString::fromUtf8("°C"), m_card);
    m_unitButton->setFixedWidth(46);
    m_unitButton->setFocusPolicy(Qt::NoFocus);
    m_unitButton->setStyleSheet(darkButtonStyle());
    topRow->addWidget(m_cityButton, 1);
    topRow->addWidget(m_unitButton, 0);
    cardLayout->addLayout(topRow);

    m_updateLabel = new QLabel(QString::fromUtf8("正在获取天气..."), m_card);
    m_updateLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #617080;"));
    cardLayout->addWidget(m_updateLabel);

    auto *liveRow = new QHBoxLayout;
    liveRow->setSpacing(18);
    liveRow->addSpacing(28);
    m_iconLabel = new QLabel(m_card);
    m_iconLabel->setFixedSize(72, 72);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));

    m_temperatureLabel = new QLabel(QStringLiteral("--°C"), m_card);
    m_temperatureLabel->setStyleSheet(QStringLiteral("font-size: 46px; font-weight: bold; color: #000;"));

    auto *weatherCol = new QVBoxLayout;
    weatherCol->setSpacing(4);
    m_weatherLabel = new QLabel(QStringLiteral("--"), m_card);
    m_weatherLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold; color: #000;"));
    m_rangeLabel = new QLabel(QStringLiteral("--"), m_card);
    m_rangeLabel->setStyleSheet(QStringLiteral("font-size: 15px; color: #000;"));
    m_detailLabel = new QLabel(QStringLiteral("--"), m_card);
    m_detailLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #536171;"));
    weatherCol->addWidget(m_weatherLabel);
    weatherCol->addWidget(m_rangeLabel);
    weatherCol->addWidget(m_detailLabel);
    weatherCol->addStretch();

    liveRow->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
    liveRow->addWidget(m_temperatureLabel, 0, Qt::AlignVCenter);
    liveRow->addLayout(weatherCol, 1);
    cardLayout->addLayout(liveRow);

    auto *daysRow = new QHBoxLayout;
    daysRow->setSpacing(8);
    m_prevDaysButton = new QPushButton(QStringLiteral("<"), m_card);
    m_nextDaysButton = new QPushButton(QStringLiteral(">"), m_card);
    for (auto *btn : {m_prevDaysButton, m_nextDaysButton}) {
        btn->setFixedWidth(24);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: rgba(210,224,255,0.70); color: #2b4f90; border: none;"
            " border-radius: 6px; font-size: 18px; font-weight: bold; }"
            "QPushButton:pressed { background-color: rgba(166,195,255,0.95); }"
            "QPushButton:disabled { background-color: rgba(230,235,240,0.55); color: #a4adb8; }"));
    }
    daysRow->addWidget(m_prevDaysButton);
    m_daysViewport = new QWidget(m_card);
    m_daysViewport->setMinimumHeight(86);
    m_daysViewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_daysViewport->setAttribute(Qt::WA_StyledBackground, true);
    m_daysViewport->setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_daysViewport->setStyleSheet(QStringLiteral("background: transparent;"));

    m_daysStrip = new QWidget(m_daysViewport);
    m_daysStrip->setAttribute(Qt::WA_StyledBackground, true);
    m_daysStrip->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *stripLayout = new QHBoxLayout(m_daysStrip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setSpacing(8);

    for (int i = 0; i < 5; ++i) {
        auto *dayBox = new ForecastCard(m_daysStrip);
        dayBox->setStyleSheet(i == 0
                                  ? QStringLiteral("background-color: white; border-radius: 8px;")
                                  : QStringLiteral("background-color: transparent; border-radius: 8px;"));
        dayBox->clickHandler = [this, i]() { showForecastDay(m_forecastWindowStart + i); };
        auto *dayLayout = new QVBoxLayout(dayBox);
        dayLayout->setContentsMargins(8, 8, 8, 8);
        dayLayout->setSpacing(4);
        auto *nameLabel = new QLabel(i == 0 ? QString::fromUtf8("今天") : QStringLiteral("--"), dayBox);
        auto *iconLabel = new QLabel(QStringLiteral("--"), dayBox);
        auto *tempLabel = new QLabel(QStringLiteral("--"), dayBox);
        nameLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setAlignment(Qt::AlignCenter);
        tempLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #0b1520;"));
        iconLabel->setFixedSize(38, 30);
        iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
        tempLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #0b1520;"));
        dayLayout->addWidget(nameLabel);
        dayLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
        dayLayout->addWidget(tempLabel);
        stripLayout->addWidget(dayBox, 1);
        m_dayCards << dayBox;
        m_dayLabels << nameLabel;
        m_dayIconLabels << iconLabel;
        m_dayTempLabels << tempLabel;
    }
    daysRow->addWidget(m_daysViewport, 1);
    daysRow->addWidget(m_nextDaysButton);
    cardLayout->addLayout(daysRow);

    m_chart = new WeatherChart(m_card);
    cardLayout->addWidget(m_chart);

    m_statusLabel = new QLabel(m_card);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #7b8794;"));
    m_statusLabel->setAlignment(Qt::AlignRight);
    cardLayout->addWidget(m_statusLabel);

    m_cityChooser = new QWidget(this);
    m_cityChooser->setAttribute(Qt::WA_StyledBackground, true);
    m_cityChooser->setStyleSheet(QStringLiteral(
        "QWidget { background-color: rgba(12,20,30,0.96); border: 1px solid rgba(255,255,255,0.16); border-radius: 8px; }"
        "QListWidget { background: transparent; color: #EAF6FF; border: none; font-size: 15px; }"
        "QListWidget::item { padding: 8px; }"
        "QListWidget::item:selected { background-color: rgba(95,178,255,0.28); }"));
    auto *chooserLayout = new QVBoxLayout(m_cityChooser);
    chooserLayout->setContentsMargins(10, 10, 10, 10);
    chooserLayout->setSpacing(8);
    m_cityEdit = new QLineEdit(m_cityChooser);
    m_cityEdit->setPlaceholderText(QString::fromUtf8("城市：拼音/简称"));
    m_cityEdit->setStyleSheet(cityEditStyle());
    m_cityList = new QListWidget(m_cityChooser);
    m_cityList->setFocusPolicy(Qt::NoFocus);
    chooserLayout->addWidget(m_cityEdit);
    chooserLayout->addWidget(m_cityList, 1);
    m_cityChooser->hide();

    connect(m_cityButton, &QPushButton::clicked, this, &WeatherPageWidget::showCityChooser);
    connect(m_unitButton, &QPushButton::clicked, this, &WeatherPageWidget::toggleUnit);
    connect(m_prevDaysButton, &QPushButton::clicked, this, [this]() { shiftForecastWindow(-1); });
    connect(m_nextDaysButton, &QPushButton::clicked, this, [this]() { shiftForecastWindow(1); });
    connect(m_cityEdit, &QLineEdit::textChanged, this, &WeatherPageWidget::updateCitySuggestions);
    connect(m_cityList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        selectCity(item->data(Qt::UserRole + 1).toString(), item->data(Qt::UserRole).toString());
    });

    updateCitySuggestions(QString());
}

void WeatherPageWidget::requestIpCity()
{
    if (m_ipReply) {
        m_ipReply->abort();
        m_ipReply->deleteLater();
    }

    const QUrl url(QStringLiteral("http://restapi.amap.com/v3/ip?key=%1").arg(QString::fromLatin1(kAmapKey)));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_ipReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_ipReply == reply)
            m_ipReply = nullptr;
        const QByteArray data = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();

        QString cityName = QString::fromUtf8("金坛区");
        QString adcode = QString::fromLatin1(kFallbackAdcode);
        if (ok) {
            const QJsonObject obj = QJsonDocument::fromJson(data).object();
            const QString apiAdcode = obj.value(QStringLiteral("adcode")).toString();
            const QString apiCity = obj.value(QStringLiteral("city")).toString();
            if (!apiAdcode.isEmpty()) {
                adcode = apiAdcode;
                cityName = apiCity.isEmpty() ? cityName : apiCity;
            }
        }
        requestWeather(adcode, cityName);
    });
}

void WeatherPageWidget::requestWeather(const QString &adcode, const QString &displayName)
{
    m_currentAdcode = adcode.isEmpty() ? QString::fromLatin1(kFallbackAdcode) : adcode;
    m_currentCity = displayName.isEmpty() ? QString::fromUtf8("金坛区") : displayName;
    setStatus(QString::fromUtf8("正在刷新..."));
    requestLiveWeather(m_currentAdcode);
    requestForecastWeather(m_currentAdcode);
}

void WeatherPageWidget::requestLiveWeather(const QString &adcode)
{
    if (m_liveReply) {
        m_liveReply->abort();
        m_liveReply->deleteLater();
    }
    const QUrl url(QStringLiteral("http://restapi.amap.com/v3/weather/weatherInfo?key=%1&city=%2&extensions=base")
                       .arg(QString::fromLatin1(kAmapKey), adcode));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_liveReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_liveReply == reply)
            m_liveReply = nullptr;
        const QByteArray data = reply->readAll();
        const QString error = reply->errorString();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        if (!ok) {
            setStatus(QString::fromUtf8("天气获取失败：%1").arg(error));
            return;
        }

        const QJsonArray lives = QJsonDocument::fromJson(data).object().value(QStringLiteral("lives")).toArray();
        if (lives.isEmpty()) {
            setStatus(QString::fromUtf8("未获取到实时天气"));
            return;
        }
        const QJsonObject live = lives.first().toObject();
        const QString city = live.value(QStringLiteral("city")).toString(m_currentCity);
        const QString weather = live.value(QStringLiteral("weather")).toString();
        const QString temp = live.value(QStringLiteral("temperature")).toString();
        const QString humidity = live.value(QStringLiteral("humidity")).toString();
        const QString wind = live.value(QStringLiteral("winddirection")).toString()
                             + QString::fromUtf8("风 ")
                             + live.value(QStringLiteral("windpower")).toString()
                             + QString::fromUtf8("级");
        const QString reportTime = live.value(QStringLiteral("reporttime")).toString();
        updateLiveUi(city, weather, temp, humidity, wind, reportTime);
    });
}

void WeatherPageWidget::requestForecastWeather(const QString &adcode)
{
    if (m_forecastReply) {
        m_forecastReply->abort();
        m_forecastReply->deleteLater();
    }
    const QUrl url(QStringLiteral("http://restapi.amap.com/v3/weather/weatherInfo?key=%1&city=%2&extensions=all")
                       .arg(QString::fromLatin1(kAmapKey), adcode));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_forecastReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_forecastReply == reply)
            m_forecastReply = nullptr;
        const QByteArray data = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        if (!ok)
            return;

        const QJsonArray forecasts = QJsonDocument::fromJson(data).object().value(QStringLiteral("forecasts")).toArray();
        if (forecasts.isEmpty())
            return;
        const QJsonArray casts = forecasts.first().toObject().value(QStringLiteral("casts")).toArray();
        QList<ForecastDay> days;
        for (const auto &value : casts) {
            if (days.size() >= 7)
                break;
            const QJsonObject obj = value.toObject();
            ForecastDay day;
            day.date = obj.value(QStringLiteral("date")).toString();
            day.week = obj.value(QStringLiteral("week")).toString();
            day.dayWeather = obj.value(QStringLiteral("dayweather")).toString();
            day.nightWeather = obj.value(QStringLiteral("nightweather")).toString();
            day.dayTemp = obj.value(QStringLiteral("daytemp")).toString();
            day.nightTemp = obj.value(QStringLiteral("nighttemp")).toString();
            days << day;
        }
        updateForecastUi(days);
    });
}

void WeatherPageWidget::updateLiveUi(const QString &city,
                                     const QString &weather,
                                     const QString &temperature,
                                     const QString &humidity,
                                     const QString &wind,
                                     const QString &reportTime)
{
    m_currentCity = city;
    m_lastTemperature = temperature;
    m_lastHumidity = humidity;
    m_lastWind = wind;
    const QString timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    m_cityButton->setText(QStringLiteral("%1  %2").arg(city, timeText));
    m_updateLabel->setText(QString::fromUtf8("几分钟前更新"));
    m_temperatureLabel->setText(formatTemperature(temperature));
    m_weatherLabel->setText(weather.isEmpty() ? QStringLiteral("--") : weather);
    m_iconLabel->setPixmap(QPixmap(weatherIconPath(weather)).scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_detailLabel->setText(QString::fromUtf8("湿度 %1%  %2").arg(humidity.isEmpty() ? QStringLiteral("--") : humidity, wind));

    if (!m_forecastDays.isEmpty()) {
        const ForecastDay &today = m_forecastDays.first();
        m_rangeLabel->setText(QString::fromUtf8("高温 %1  低温 %2").arg(formatTemperature(today.dayTemp), formatTemperature(today.nightTemp)));
    } else {
        m_rangeLabel->setText(reportTime);
    }
    setStatus(QString());
}

void WeatherPageWidget::updateForecastUi(const QList<ForecastDay> &days)
{
    m_forecastDays = days;
    m_selectedDay = qBound(0, m_selectedDay, qMax(0, m_forecastDays.size() - 1));
    m_forecastWindowStart = qBound(0, m_forecastWindowStart, qMax(0, m_forecastDays.size() - m_dayCards.size()));
    updateForecastCards();
    if (!m_lastTemperature.isEmpty() && !days.isEmpty())
        m_rangeLabel->setText(QString::fromUtf8("高温 %1  低温 %2").arg(formatTemperature(days.first().dayTemp), formatTemperature(days.first().nightTemp)));

    rebuildChart();
    showForecastDay(qBound(0, m_selectedDay, qMax(0, days.size() - 1)));
    QTimer::singleShot(0, this, [this]() {
        syncForecastStripGeometry();
        updateForecastCards();
    });
}

void WeatherPageWidget::updateForecastCards()
{
    syncForecastStripGeometry();

    const int visibleCount = m_dayCards.size();
    const int maxStart = qMax(0, m_forecastDays.size() - visibleCount);
    m_forecastWindowStart = qBound(0, m_forecastWindowStart, maxStart);

    for (int i = 0; i < visibleCount; ++i) {
        const int dayIndex = m_forecastWindowStart + i;
        const bool hasDay = (dayIndex >= 0 && dayIndex < m_forecastDays.size());
        m_dayCards.at(i)->setVisible(hasDay);
        if (!hasDay)
            continue;

        const ForecastDay &day = m_forecastDays.at(dayIndex);
        m_dayLabels.at(i)->setText(dayIndex == 0 ? QString::fromUtf8("今天") : weekName(day.week));
        m_dayIconLabels.at(i)->setPixmap(QPixmap(weatherIconPath(day.dayWeather)).scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_dayTempLabels.at(i)->setText(QStringLiteral("%1 %2").arg(formatTemperature(day.dayTemp), formatTemperature(day.nightTemp)));
        m_dayCards.at(i)->setStyleSheet(dayIndex == m_selectedDay
                                            ? QStringLiteral("background-color: white; border-radius: 8px;")
                                            : QStringLiteral("background-color: transparent; border-radius: 8px;"));
    }

    if (m_prevDaysButton)
        m_prevDaysButton->setEnabled(m_forecastWindowStart > 0);
    if (m_nextDaysButton)
        m_nextDaysButton->setEnabled(m_forecastWindowStart < maxStart);
}

void WeatherPageWidget::syncForecastStripGeometry()
{
    if (!m_daysViewport || !m_daysStrip)
        return;
    const int width = qMax(1, m_daysViewport->width());
    const int height = qMax(1, m_daysViewport->height());
    m_daysStrip->setGeometry(m_daysAnimOffset, 0, width, height);
}

void WeatherPageWidget::animateForecastCards(int direction)
{
    if (!m_daysViewport || !m_daysStrip)
        return;

    if (!m_daysAnimTimer) {
        m_daysAnimTimer = new QTimer(this);
        m_daysAnimTimer->setInterval(16);
        connect(m_daysAnimTimer, &QTimer::timeout, this, [this]() {
            m_daysAnimOffset = qRound(m_daysAnimOffset * 0.72);
            if (qAbs(m_daysAnimOffset) < 2) {
                m_daysAnimOffset = 0;
                m_daysAnimTimer->stop();
            }
            syncForecastStripGeometry();
        });
    }

    m_daysAnimDirection = direction;
    m_daysAnimOffset = direction > 0 ? qMax(36, m_daysViewport->width() / 3) : -qMax(36, m_daysViewport->width() / 3);
    syncForecastStripGeometry();
    m_daysAnimTimer->start();
}

void WeatherPageWidget::showForecastDay(int index)
{
    if (index < 0 || index >= m_forecastDays.size())
        return;

    m_selectedDay = index;
    if (m_selectedDay < m_forecastWindowStart)
        m_forecastWindowStart = m_selectedDay;
    if (m_selectedDay >= m_forecastWindowStart + m_dayCards.size())
        m_forecastWindowStart = m_selectedDay - m_dayCards.size() + 1;
    updateForecastCards();

    const ForecastDay &day = m_forecastDays.at(index);
    const QString temp = (index == 0 && !m_lastTemperature.isEmpty()) ? m_lastTemperature : day.dayTemp;
    m_temperatureLabel->setText(formatTemperature(temp));
    m_weatherLabel->setText(day.dayWeather.isEmpty() ? QStringLiteral("--") : day.dayWeather);
    m_rangeLabel->setText(QString::fromUtf8("高温 %1  低温 %2").arg(formatTemperature(day.dayTemp), formatTemperature(day.nightTemp)));
    m_iconLabel->setPixmap(QPixmap(weatherIconPath(day.dayWeather)).scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (index == 0) {
        m_detailLabel->setText(QString::fromUtf8("湿度 %1%  %2").arg(m_lastHumidity.isEmpty() ? QStringLiteral("--") : m_lastHumidity, m_lastWind));
    } else {
        m_detailLabel->setText(QString::fromUtf8("%1白天，夜间%2").arg(day.dayWeather, day.nightWeather));
    }
    m_chart->scrollToPointIndex(index * 8);
}

void WeatherPageWidget::rebuildChart()
{
    if (m_forecastDays.isEmpty())
        return;

    const QStringList times = {
        QStringLiteral("00:00"), QStringLiteral("03:00"), QStringLiteral("06:00"), QStringLiteral("09:00"),
        QStringLiteral("12:00"), QStringLiteral("15:00"), QStringLiteral("18:00"), QStringLiteral("21:00")
    };
    bool humidityOk = false;
    const int baseHumidity = m_lastHumidity.toInt(&humidityOk);

    QList<WeatherChart::PointData> chartPoints;
    for (int dayIndex = 0; dayIndex < m_forecastDays.size(); ++dayIndex) {
        const ForecastDay &day = m_forecastDays.at(dayIndex);
        bool highOk = false;
        bool lowOk = false;
        const int high = day.dayTemp.toInt(&highOk);
        const int low = day.nightTemp.toInt(&lowOk);
        const int dayHigh = highOk ? high : 28;
        const int dayLow = lowOk ? low : dayHigh - 6;
        for (int i = 0; i < times.size(); ++i) {
            WeatherChart::PointData point;
            point.time = (i == 0 && !day.date.isEmpty())
                             ? (dayIndex == 0 ? QString::fromUtf8("今天") : day.date.mid(5))
                             : times.at(i);
            const double phase = double(i) / double(times.size() - 1);
            const double wave = (qSin((phase * 2.0 - 0.55) * 3.14159265358979323846) + 1.0) / 2.0;
            point.temperature = qRound(dayLow + (dayHigh - dayLow) * wave);
            if (dayIndex == 0 && i == 4 && !m_lastTemperature.isEmpty())
                point.temperature = m_lastTemperature.toInt();
            point.humidity = qBound(20, (humidityOk ? baseHumidity : 55) + (i % 4 - 1) * 3 + dayIndex * 2, 95);
            chartPoints << point;
        }
    }
    m_chart->setPoints(chartPoints);
}

void WeatherPageWidget::shiftForecastWindow(int delta)
{
    const int maxStart = qMax(0, m_forecastDays.size() - m_dayCards.size());
    const int oldStart = m_forecastWindowStart;
    m_forecastWindowStart = qBound(0, m_forecastWindowStart + delta, maxStart);
    if (m_forecastWindowStart == oldStart)
        return;
    updateForecastCards();
    animateForecastCards(delta);
}

void WeatherPageWidget::toggleUnit()
{
    m_useFahrenheit = !m_useFahrenheit;
    refreshUnitTexts();
}

void WeatherPageWidget::refreshUnitTexts()
{
    if (m_unitButton)
        m_unitButton->setText(m_useFahrenheit ? QString::fromUtf8("°F") : QString::fromUtf8("°C"));
    if (m_chart)
        m_chart->setUseFahrenheit(m_useFahrenheit);

    updateForecastCards();
    showForecastDay(qBound(0, m_selectedDay, qMax(0, m_forecastDays.size() - 1)));
}

void WeatherPageWidget::showCityChooser()
{
    m_cityChooser->setGeometry(18, 14, qMin(width() - 36, 270), qMin(height() - 28, 310));
    m_cityChooser->raise();
    m_cityChooser->show();
    m_cityEdit->setFocus();
    updateCitySuggestions(m_cityEdit->text());
}

void WeatherPageWidget::hideCityChooser()
{
    m_cityChooser->hide();
}

void WeatherPageWidget::updateCitySuggestions(const QString &text)
{
    const QString keyword = text.trimmed().toLower();
    m_cityList->clear();
    for (const CityEntry &city : kCities) {
        const QString name = QString::fromUtf8(city.name);
        const QString pinyin = QString::fromLatin1(city.pinyin);
        const QString abbr = QString::fromLatin1(city.abbr);
        if (!keyword.isEmpty()
            && !name.toLower().contains(keyword)
            && !pinyin.contains(keyword)
            && !abbr.contains(keyword)) {
            continue;
        }
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(name, pinyin), m_cityList);
        item->setData(Qt::UserRole, QString::fromLatin1(city.adcode));
        item->setData(Qt::UserRole + 1, name);
    }
}

void WeatherPageWidget::selectCity(const QString &name, const QString &adcode)
{
    hideCityChooser();
    requestWeather(adcode, name);
}

void WeatherPageWidget::setStatus(const QString &text)
{
    m_statusLabel->setText(text);
}

QString WeatherPageWidget::weatherIconPath(const QString &weather) const
{
    if (weather.contains(QString::fromUtf8("雷")))
        return QStringLiteral(":/picture/weather_thunder.png");
    if (weather.contains(QString::fromUtf8("暴雨")) || weather.contains(QString::fromUtf8("大雨")))
        return QStringLiteral(":/picture/weather_heavy_rain.png");
    if (weather.contains(QString::fromUtf8("雨")))
        return QStringLiteral(":/picture/weather_rain.png");
    if (weather.contains(QString::fromUtf8("雪")))
        return QStringLiteral(":/picture/weather_snow.png");
    if (weather.contains(QString::fromUtf8("雾"))
        || weather.contains(QString::fromUtf8("霾"))
        || weather.contains(QString::fromUtf8("尘"))
        || weather.contains(QString::fromUtf8("沙")))
        return QStringLiteral(":/picture/weather_fog.png");
    if (weather.contains(QString::fromUtf8("阴")))
        return QStringLiteral(":/picture/weather_overcast.png");
    if (weather.contains(QString::fromUtf8("云")))
        return QStringLiteral(":/picture/weather_cloudy.png");
    return QStringLiteral(":/picture/weather_sunny.png");
}

QString WeatherPageWidget::weekName(const QString &week) const
{
    if (week == QStringLiteral("1")) return QString::fromUtf8("周一");
    if (week == QStringLiteral("2")) return QString::fromUtf8("周二");
    if (week == QStringLiteral("3")) return QString::fromUtf8("周三");
    if (week == QStringLiteral("4")) return QString::fromUtf8("周四");
    if (week == QStringLiteral("5")) return QString::fromUtf8("周五");
    if (week == QStringLiteral("6")) return QString::fromUtf8("周六");
    return QString::fromUtf8("周日");
}

QString WeatherPageWidget::formatTemperature(const QString &celsius) const
{
    bool ok = false;
    const int value = celsius.toInt(&ok);
    if (!ok)
        return QStringLiteral("--°");
    return formatTemperature(value);
}

QString WeatherPageWidget::formatTemperature(int celsius) const
{
    return QStringLiteral("%1%2").arg(displayTemperature(celsius)).arg(m_useFahrenheit ? QString::fromUtf8("°F") : QString::fromUtf8("°C"));
}

int WeatherPageWidget::displayTemperature(int celsius) const
{
    return m_useFahrenheit ? qRound(celsius * 9.0 / 5.0 + 32.0) : celsius;
}
