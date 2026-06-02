#ifndef MAPPAGE_H
#define MAPPAGE_H

#include <QHash>
#include <QList>
#include <QNetworkReply>
#include <QPixmap>
#include <QSet>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QPushButton;
class MapCanvas;
class MapKeyboard;

class MapPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapPageWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildUi();
    void refreshTiles();
    void requestTile(int x, int y, int z);
    void updateInfo();
    void pan(int dx, int dy);
    void dragMap(const QPoint &delta);
    void setZoom(int zoom);
    void setCenter(double latitude, double longitude, int zoom);
    void fitRouteToView(const QList<QPointF> &points);
    void updateSearchSuggestions(const QString &text);
    void requestInputTips(const QString &text);
    void performSearch();
    void applySuggestion(const QString &name, const QPointF &point);
    void planRoute();
    void requestDrivingRoute();
    void searchTextAppend(const QString &text);
    void searchBackspace();
    void showKeyboard();
    void hideKeyboard();
    void layoutKeyboard();
    QString tileKey(int x, int y, int z) const;
    static int lonToTileX(double longitude, int zoom);
    static int latToTileY(double latitude, int zoom);
    static double lonToTileXDouble(double longitude, int zoom);
    static double latToTileYDouble(double latitude, int zoom);
    static double tileXToLon(double x, int zoom);
    static double tileYToLat(double y, int zoom);
    static bool parseAmapLocation(const QString &location, QPointF *point);

    QNetworkAccessManager *m_network = nullptr;
    MapCanvas *m_canvas = nullptr;
    MapKeyboard *m_keyboard = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_startEdit = nullptr;
    QLineEdit *m_endEdit = nullptr;
    QLineEdit *m_activeEdit = nullptr;
    QListWidget *m_suggestionList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_coordLabel = nullptr;
    QHash<QString, QPixmap> m_cache;
    QSet<QString> m_pending;
    QList<QString> m_visibleKeys;
    QNetworkReply *m_tipReply = nullptr;
    QNetworkReply *m_routeReply = nullptr;

    int m_zoom = 12;
    int m_tileCols = 3;
    int m_tileRows = 3;
    double m_centerTileX = 0.0;
    double m_centerTileY = 0.0;
    double m_latitude = 31.674600;
    double m_longitude = 119.576472;
    bool m_hasRoute = false;
    QList<QPointF> m_routePoints;
};

#endif // MAPPAGE_H
