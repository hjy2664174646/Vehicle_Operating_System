#ifndef WEATHERPAGE_H
#define WEATHERPAGE_H

#include <QList>
#include <QNetworkReply>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class WeatherChart;

class WeatherPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WeatherPageWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct ForecastDay {
        QString date;
        QString week;
        QString dayWeather;
        QString nightWeather;
        QString dayTemp;
        QString nightTemp;
    };

    void buildUi();
    void requestIpCity();
    void requestWeather(const QString &adcode, const QString &displayName);
    void requestLiveWeather(const QString &adcode);
    void requestForecastWeather(const QString &adcode);
    void updateLiveUi(const QString &city,
                      const QString &weather,
                      const QString &temperature,
                      const QString &humidity,
                      const QString &wind,
                      const QString &reportTime);
    void updateForecastUi(const QList<ForecastDay> &days);
    void updateForecastCards();
    void syncForecastStripGeometry();
    void animateForecastCards(int direction);
    void showForecastDay(int index);
    void rebuildChart();
    void shiftForecastWindow(int delta);
    void toggleUnit();
    void refreshUnitTexts();
    void showCityChooser();
    void hideCityChooser();
    void updateCitySuggestions(const QString &text);
    void selectCity(const QString &name, const QString &adcode);
    void setStatus(const QString &text);
    QString weatherIconPath(const QString &weather) const;
    QString weekName(const QString &week) const;
    QString formatTemperature(const QString &celsius) const;
    QString formatTemperature(int celsius) const;
    int displayTemperature(int celsius) const;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_ipReply = nullptr;
    QNetworkReply *m_liveReply = nullptr;
    QNetworkReply *m_forecastReply = nullptr;

    QWidget *m_card = nullptr;
    QPushButton *m_cityButton = nullptr;
    QPushButton *m_unitButton = nullptr;
    QLabel *m_updateLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_temperatureLabel = nullptr;
    QLabel *m_weatherLabel = nullptr;
    QLabel *m_rangeLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_prevDaysButton = nullptr;
    QPushButton *m_nextDaysButton = nullptr;
    QWidget *m_daysViewport = nullptr;
    QWidget *m_daysStrip = nullptr;
    QList<QWidget *> m_dayCards;
    QList<QLabel *> m_dayLabels;
    QList<QLabel *> m_dayIconLabels;
    QList<QLabel *> m_dayTempLabels;
    WeatherChart *m_chart = nullptr;

    QWidget *m_cityChooser = nullptr;
    QLineEdit *m_cityEdit = nullptr;
    QListWidget *m_cityList = nullptr;

    QString m_currentCity = QString::fromUtf8("金坛区");
    QString m_currentAdcode = QStringLiteral("320413");
    QString m_lastTemperature;
    QString m_lastHumidity;
    QString m_lastWind;
    int m_selectedDay = 0;
    int m_forecastWindowStart = 0;
    QTimer *m_daysAnimTimer = nullptr;
    int m_daysAnimOffset = 0;
    int m_daysAnimDirection = 0;
    bool m_useFahrenheit = false;
    QList<ForecastDay> m_forecastDays;
};

#endif // WEATHERPAGE_H
