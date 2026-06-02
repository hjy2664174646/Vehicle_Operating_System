#include "mappage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QRegExp>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>
#include <functional>

namespace {

constexpr int kTileSize = 256;
constexpr double kPi = 3.14159265358979323846;
constexpr const char *kAmapKey = "a7838e34b0d36957124c9aa84bdaae3d";

QString toolButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(255,255,255,0.10);"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 6px;"
        "  color: #EAF6FF;"
        "  font-size: 18px;"
        "  min-width: 42px;"
        "  min-height: 36px;"
        "}"
        "QPushButton:pressed { background-color: rgba(95,178,255,0.28); }");
}

void paintMapBackground(QPainter &p, const QRect &rect)
{
    p.fillRect(rect, QColor(QStringLiteral("#071016")));
    QRadialGradient glow(QPointF(rect.width() * 0.62, rect.height() * 0.28),
                         qreal(qMax(rect.width(), rect.height())) * 0.62);
    glow.setColorAt(0.0, QColor(28, 92, 82, 170));
    glow.setColorAt(0.45, QColor(9, 24, 32, 120));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);
}

} // namespace

class MapCanvas : public QWidget
{
public:
    std::function<void(const QPoint &)> dragHandler;
    std::function<void()> resizeHandler;

    explicit MapCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(420, 300);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setTiles(const QList<QString> &keys,
                  const QHash<QString, QPixmap> *cache,
                  int cols,
                  int rows,
                  int offsetX,
                  int offsetY,
                  double centerTileX,
                  double centerTileY,
                  int zoom,
                  bool hasRoute,
                  const QList<QPointF> &routePoints)
    {
        m_keys = keys;
        m_cache = cache;
        m_cols = cols;
        m_rows = rows;
        m_offsetX = offsetX;
        m_offsetY = offsetY;
        m_centerTileX = centerTileX;
        m_centerTileY = centerTileY;
        m_zoom = zoom;
        m_hasRoute = hasRoute;
        m_routePoints = routePoints;
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            m_lastPos = event->pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressed) {
            const QPoint delta = event->pos() - m_lastPos;
            m_lastPos = event->pos();
            if (dragHandler)
                dragHandler(delta);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_pressed) {
            m_pressed = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        if (resizeHandler)
            QTimer::singleShot(0, this, [this]() {
                if (resizeHandler)
                    resizeHandler();
            });
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#13232c")));

        for (int i = 0; i < m_keys.size(); ++i) {
            const int row = i / qMax(1, m_cols);
            const int col = i % qMax(1, m_cols);
            const int originX = (width() - kTileSize * m_cols) / 2;
            const int originY = (height() - kTileSize * m_rows) / 2;
            const QRect target(originX + col * kTileSize + m_offsetX,
                               originY + row * kTileSize + m_offsetY,
                               kTileSize,
                               kTileSize);
            const QString &key = m_keys.at(i);

            if (m_cache && m_cache->contains(key)) {
                painter.drawPixmap(target, m_cache->value(key));
            } else {
                painter.fillRect(target, QColor(QStringLiteral("#18313c")));
                painter.setPen(QColor(QStringLiteral("#8EA4B8")));
                painter.drawText(target, Qt::AlignCenter, QStringLiteral("..."));
            }
        }

        if (m_hasRoute && m_routePoints.size() >= 2) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen routePen(QColor(QStringLiteral("#ffcf33")));
            routePen.setWidth(5);
            routePen.setCapStyle(Qt::RoundCap);
            routePen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(routePen);
            QPainterPath path;
            path.moveTo(geoToPoint(m_routePoints.first().x(), m_routePoints.first().y()));
            for (int i = 1; i < m_routePoints.size(); ++i)
                path.lineTo(geoToPoint(m_routePoints.at(i).x(), m_routePoints.at(i).y()));
            painter.drawPath(path);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#20d070")));
            const QPoint start = geoToPoint(m_routePoints.first().x(), m_routePoints.first().y());
            const QPoint end = geoToPoint(m_routePoints.last().x(), m_routePoints.last().y());
            painter.drawEllipse(start, 8, 8);
            painter.setBrush(QColor(QStringLiteral("#ff4d4d")));
            painter.drawEllipse(end, 8, 8);
        }
    }

private:
    QPoint geoToPoint(double lat, double lon) const
    {
        const double n = qPow(2.0, m_zoom);
        const double tileX = (lon + 180.0) / 360.0 * n;
        const double latRad = qBound(-85.0511, lat, 85.0511) * kPi / 180.0;
        const double tileY = (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / kPi) / 2.0 * n;
        const double centerPxX = m_centerTileX * kTileSize;
        const double centerPxY = m_centerTileY * kTileSize;
        return QPoint(int(width() / 2.0 + tileX * kTileSize - centerPxX),
                      int(height() / 2.0 + tileY * kTileSize - centerPxY));
    }

    QList<QString> m_keys;
    const QHash<QString, QPixmap> *m_cache = nullptr;
    QPoint m_lastPos;
    int m_cols = 3;
    int m_rows = 3;
    int m_offsetX = 0;
    int m_offsetY = 0;
    double m_centerTileX = 0.0;
    double m_centerTileY = 0.0;
    int m_zoom = 12;
    bool m_hasRoute = false;
    QList<QPointF> m_routePoints;
    bool m_pressed = false;
};

class MapSearchEdit : public QLineEdit
{
public:
    std::function<void()> clickHandler;

    explicit MapSearchEdit(QWidget *parent = nullptr)
        : QLineEdit(parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (clickHandler)
            clickHandler();
        QLineEdit::mousePressEvent(event);
    }
};

class MapKeyboard : public QWidget
{
public:
    std::function<void(const QString &)> textHandler;
    std::function<void()> backspaceHandler;
    std::function<void()> enterHandler;
    std::function<void()> hideHandler;

    explicit MapKeyboard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(QStringLiteral(
            "QWidget { background-color: #081018; border: 1px solid rgba(255,255,255,0.18); border-radius: 8px; }"
            "QPushButton { background-color: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.16);"
            " color: #EAF6FF; border-radius: 5px; min-height: 28px; font-size: 16px; font-weight: bold; }"
            "QPushButton:pressed { background-color: rgba(95,178,255,0.28); }"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(10, 8, 10, 8);
        root->setSpacing(5);

        const QStringList rows = {
            QStringLiteral("1234567890"),
            QStringLiteral("qwertyuiop"),
            QStringLiteral("asdfghjkl"),
            QStringLiteral("zxcvbnm.,")
        };

        for (const QString &rowText : rows) {
            auto *row = new QHBoxLayout;
            row->setSpacing(4);
            for (const QChar &ch : rowText) {
                auto *btn = new QPushButton(QString(ch), this);
                btn->setFocusPolicy(Qt::NoFocus);
                connect(btn, &QPushButton::clicked, this, [this, ch]() {
                    if (textHandler)
                        textHandler(QString(ch));
                });
                row->addWidget(btn);
            }
            root->addLayout(row);
        }

        auto *cmdRow = new QHBoxLayout;
        cmdRow->setSpacing(4);
        const auto addCmd = [this, cmdRow](const QString &text, const std::function<void()> &handler, int stretch) {
            auto *btn = new QPushButton(text, this);
            btn->setFocusPolicy(Qt::NoFocus);
            connect(btn, &QPushButton::clicked, this, [handler]() {
                if (handler)
                    handler();
            });
            cmdRow->addWidget(btn, stretch);
        };

        addCmd(QString::fromUtf8("空格"), [this]() { if (textHandler) textHandler(QStringLiteral(" ")); }, 2);
        addCmd(QString::fromUtf8("退格"), [this]() { if (backspaceHandler) backspaceHandler(); }, 1);
        addCmd(QString::fromUtf8("搜索"), [this]() { if (enterHandler) enterHandler(); }, 1);
        addCmd(QString::fromUtf8("收起"), [this]() { if (hideHandler) hideHandler(); }, 1);
        root->addLayout(cmdRow);
    }
};

MapPageWidget::MapPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setAutoFillBackground(false);
    buildUi();
    setCenter(m_latitude, m_longitude, m_zoom);
}

void MapPageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    paintMapBackground(painter, rect());
}

void MapPageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutKeyboard();
    refreshTiles();
}

void MapPageWidget::buildUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    auto *mapPanel = new QWidget(this);
    mapPanel->setAttribute(Qt::WA_StyledBackground, true);
    mapPanel->setStyleSheet(QStringLiteral(
        "background-color: #071016;"
        "border: 1px solid rgba(255,255,255,0.10);"
        "border-radius: 8px;"));

    auto *mapLayout = new QVBoxLayout(mapPanel);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(0);
    m_canvas = new MapCanvas(mapPanel);
    m_canvas->dragHandler = [this](const QPoint &delta) {
        dragMap(delta);
    };
    m_canvas->resizeHandler = [this]() {
        refreshTiles();
    };
    mapLayout->addWidget(m_canvas);

    auto *ctrlPanel = new QWidget(this);
    ctrlPanel->setFixedWidth(210);
    ctrlPanel->setAttribute(Qt::WA_StyledBackground, true);
    ctrlPanel->setStyleSheet(QStringLiteral(
        "background-color: rgba(12,18,25,0.90);"
        "border: 1px solid rgba(255,255,255,0.10);"
        "border-radius: 8px;"));

    auto *ctrlLayout = new QVBoxLayout(ctrlPanel);
    ctrlLayout->setContentsMargins(14, 14, 14, 14);
    ctrlLayout->setSpacing(10);

    const QString editStyle = QStringLiteral(
        "QLineEdit { background-color: rgba(7,12,18,0.90); border: 1px solid rgba(255,255,255,0.16);"
        " border-radius: 6px; color: #EAF6FF; font-size: 14px; min-height: 34px; padding-left: 10px; }");

    auto *startEdit = new MapSearchEdit(ctrlPanel);
    auto *endEdit = new MapSearchEdit(ctrlPanel);
    m_startEdit = startEdit;
    m_endEdit = endEdit;
    m_searchEdit = startEdit;
    m_activeEdit = startEdit;

    m_startEdit->setPlaceholderText(QString::fromUtf8("起始：拼音/经纬度"));
    m_endEdit->setPlaceholderText(QString::fromUtf8("目标：拼音/经纬度"));
    m_startEdit->setFocusPolicy(Qt::NoFocus);
    m_endEdit->setFocusPolicy(Qt::NoFocus);
    m_startEdit->setStyleSheet(editStyle);
    m_endEdit->setStyleSheet(editStyle);

    m_suggestionList = new QListWidget(ctrlPanel);
    m_suggestionList->setFocusPolicy(Qt::NoFocus);
    m_suggestionList->setMaximumHeight(136);
    m_suggestionList->setStyleSheet(QStringLiteral(
        "QListWidget { background-color: rgba(7,12,18,0.90); border: 1px solid rgba(255,255,255,0.12);"
        " color: #DDEEFF; font-size: 13px; }"
        "QListWidget::item { padding: 6px; }"
        "QListWidget::item:selected { background-color: rgba(95,178,255,0.28); }"));

    m_keyboard = new MapKeyboard(this);
    m_keyboard->hide();

    auto *zoomRow = new QHBoxLayout;
    auto *zoomOutBtn = new QPushButton(QStringLiteral("-"), ctrlPanel);
    auto *zoomInBtn = new QPushButton(QStringLiteral("+"), ctrlPanel);
    zoomOutBtn->setStyleSheet(toolButtonStyle());
    zoomInBtn->setStyleSheet(toolButtonStyle());
    zoomOutBtn->setFocusPolicy(Qt::NoFocus);
    zoomInBtn->setFocusPolicy(Qt::NoFocus);
    zoomRow->addWidget(zoomOutBtn);
    zoomRow->addWidget(zoomInBtn);

    auto *homeBtn = new QPushButton(QString::fromUtf8("金坛"), ctrlPanel);
    auto *routeBtn = new QPushButton(QString::fromUtf8("规划路线"), ctrlPanel);
    for (auto *btn : {homeBtn, routeBtn}) {
        btn->setStyleSheet(toolButtonStyle());
        btn->setFocusPolicy(Qt::NoFocus);
    }

    m_coordLabel = new QLabel(ctrlPanel);
    m_coordLabel->setWordWrap(true);
    m_coordLabel->setStyleSheet(QStringLiteral("color: #B6DFFF; font-size: 13px; background: transparent;"));

    m_statusLabel = new QLabel(ctrlPanel);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #8EA4B8; font-size: 13px; background: transparent;"));
    m_statusLabel->hide();

    ctrlLayout->addWidget(m_startEdit);
    ctrlLayout->addWidget(m_endEdit);
    ctrlLayout->addWidget(m_suggestionList);
    ctrlLayout->addLayout(zoomRow);
    ctrlLayout->addWidget(routeBtn);
    ctrlLayout->addWidget(homeBtn);
    ctrlLayout->addSpacing(8);
    ctrlLayout->addWidget(m_coordLabel);
    ctrlLayout->addStretch();

    root->addWidget(mapPanel, 1);
    root->addWidget(ctrlPanel, 0);

    connect(zoomInBtn, &QPushButton::clicked, this, [this]() { setZoom(m_zoom + 1); });
    connect(zoomOutBtn, &QPushButton::clicked, this, [this]() { setZoom(m_zoom - 1); });
    connect(homeBtn, &QPushButton::clicked, this, [this]() { setCenter(31.674600, 119.576472, 13); });
    connect(routeBtn, &QPushButton::clicked, this, &MapPageWidget::planRoute);
    connect(m_startEdit, &QLineEdit::textChanged, this, &MapPageWidget::updateSearchSuggestions);
    connect(m_endEdit, &QLineEdit::textChanged, this, &MapPageWidget::updateSearchSuggestions);
    connect(m_startEdit, &QLineEdit::returnPressed, this, &MapPageWidget::performSearch);
    connect(m_endEdit, &QLineEdit::returnPressed, this, &MapPageWidget::performSearch);
    startEdit->clickHandler = [this]() {
        m_activeEdit = m_startEdit;
        m_searchEdit = m_startEdit;
        updateSearchSuggestions(m_startEdit->text());
        showKeyboard();
    };
    endEdit->clickHandler = [this]() {
        m_activeEdit = m_endEdit;
        m_searchEdit = m_endEdit;
        updateSearchSuggestions(m_endEdit->text());
        showKeyboard();
    };
    connect(m_suggestionList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        const QString name = item->data(Qt::UserRole + 1).toString();
        const QPointF point = item->data(Qt::UserRole).toPointF();
        applySuggestion(name, point);
    });

    m_keyboard->textHandler = [this](const QString &text) { searchTextAppend(text); };
    m_keyboard->backspaceHandler = [this]() { searchBackspace(); };
    m_keyboard->enterHandler = [this]() { performSearch(); };
    m_keyboard->hideHandler = [this]() { hideKeyboard(); };

    updateSearchSuggestions(QString());
}

void MapPageWidget::refreshTiles()
{
    const int maxTile = 1 << m_zoom;
    m_visibleKeys.clear();
    const int centerTileX = int(qFloor(m_centerTileX));
    const int centerTileY = int(qFloor(m_centerTileY));
    const int offsetX = int((0.5 - (m_centerTileX - centerTileX)) * kTileSize);
    const int offsetY = int((0.5 - (m_centerTileY - centerTileY)) * kTileSize);
    const int canvasWidth = m_canvas ? qMax(kTileSize, m_canvas->width()) : kTileSize * 3;
    const int canvasHeight = m_canvas ? qMax(kTileSize, m_canvas->height()) : kTileSize * 3;
    m_tileCols = qMax(3, int(qCeil(double(canvasWidth) / kTileSize)) + 2);
    m_tileRows = qMax(3, int(qCeil(double(canvasHeight) / kTileSize)) + 2);
    if (m_tileCols % 2 == 0)
        ++m_tileCols;
    if (m_tileRows % 2 == 0)
        ++m_tileRows;
    const int colRadius = m_tileCols / 2;
    const int rowRadius = m_tileRows / 2;

    for (int row = 0; row < m_tileRows; ++row) {
        for (int col = 0; col < m_tileCols; ++col) {
            int x = centerTileX + col - colRadius;
            const int y = centerTileY + row - rowRadius;
            x = (x % maxTile + maxTile) % maxTile;

            if (y < 0 || y >= maxTile) {
                m_visibleKeys << QString();
                continue;
            }
            const QString key = tileKey(x, y, m_zoom);
            m_visibleKeys << key;
            requestTile(x, y, m_zoom);
        }
    }

    if (m_canvas)
        m_canvas->setTiles(m_visibleKeys,
                           &m_cache,
                           m_tileCols,
                           m_tileRows,
                           offsetX,
                           offsetY,
                           m_centerTileX,
                           m_centerTileY,
                           m_zoom,
                           m_hasRoute,
                           m_routePoints);
    updateInfo();
}

void MapPageWidget::requestTile(int x, int y, int z)
{
    const QString key = tileKey(x, y, z);
    if (m_cache.contains(key))
        return;

    if (m_pending.contains(key))
        return;

    m_pending.insert(key);
    const int server = ((x + y + z) % 4) + 1;
    const QUrl url(QStringLiteral("http://webrd0%1.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4")
                       .arg(server)
                       .arg(x)
                       .arg(y)
                       .arg(z));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "VehicleOperatingSystem/1.0");

    auto *reply = m_network->get(request);
    QTimer::singleShot(10000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        m_pending.remove(key);
        const QByteArray data = reply->readAll();
        QPixmap pixmap;
        if (reply->error() == QNetworkReply::NoError && pixmap.loadFromData(data)) {
            pixmap = pixmap.scaled(kTileSize, kTileSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            m_cache.insert(key, pixmap);
            if (m_canvas)
                m_canvas->update();
        } else {
            m_statusLabel->setText(QString::fromUtf8("地图瓦片加载失败：%1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void MapPageWidget::updateInfo()
{
    m_longitude = tileXToLon(m_centerTileX, m_zoom);
    m_latitude = tileYToLat(m_centerTileY, m_zoom);
    m_coordLabel->setText(QString::fromUtf8("中心：%1, %2\n缩放：%3")
                              .arg(m_latitude, 0, 'f', 5)
                              .arg(m_longitude, 0, 'f', 5)
                              .arg(m_zoom));
}

void MapPageWidget::pan(int dx, int dy)
{
    const int maxTile = 1 << m_zoom;
    m_centerTileX = std::fmod(m_centerTileX + dx + maxTile, double(maxTile));
    m_centerTileY = qBound(0.0, m_centerTileY + dy, double(maxTile - 1));
    refreshTiles();
}

void MapPageWidget::dragMap(const QPoint &delta)
{
    const int maxTile = 1 << m_zoom;
    m_centerTileX = std::fmod(m_centerTileX - double(delta.x()) / kTileSize + maxTile, double(maxTile));
    m_centerTileY = qBound(0.0, m_centerTileY - double(delta.y()) / kTileSize, double(maxTile - 1));
    refreshTiles();
}

void MapPageWidget::setZoom(int zoom)
{
    zoom = qBound(2, zoom, 18);
    if (zoom == m_zoom)
        return;
    setCenter(m_latitude, m_longitude, zoom);
}

void MapPageWidget::setCenter(double latitude, double longitude, int zoom)
{
    m_zoom = qBound(2, zoom, 18);
    m_latitude = latitude;
    m_longitude = longitude;
    m_centerTileX = lonToTileXDouble(longitude, m_zoom);
    m_centerTileY = latToTileYDouble(latitude, m_zoom);
    refreshTiles();
}

void MapPageWidget::fitRouteToView(const QList<QPointF> &points)
{
    if (points.isEmpty()) {
        return;
    }

    double minLat = points.first().x();
    double maxLat = minLat;
    double minLon = points.first().y();
    double maxLon = minLon;
    for (const QPointF &point : points) {
        minLat = qMin(minLat, point.x());
        maxLat = qMax(maxLat, point.x());
        minLon = qMin(minLon, point.y());
        maxLon = qMax(maxLon, point.y());
    }

    const double centerLat = (minLat + maxLat) / 2.0;
    const double centerLon = (minLon + maxLon) / 2.0;
    const int viewWidth = m_canvas ? qMax(1, m_canvas->width()) : 1;
    const int viewHeight = m_canvas ? qMax(1, m_canvas->height()) : 1;
    const int padding = 56;
    const double usableWidth = qMax(80, viewWidth - padding * 2);
    const double usableHeight = qMax(80, viewHeight - padding * 2);

    int bestZoom = 2;
    for (int zoom = 18; zoom >= 2; --zoom) {
        const double minX = lonToTileXDouble(minLon, zoom) * kTileSize;
        const double maxX = lonToTileXDouble(maxLon, zoom) * kTileSize;
        const double minY = latToTileYDouble(maxLat, zoom) * kTileSize;
        const double maxY = latToTileYDouble(minLat, zoom) * kTileSize;
        const double routeWidth = qMax(1.0, qAbs(maxX - minX));
        const double routeHeight = qMax(1.0, qAbs(maxY - minY));
        if (routeWidth <= usableWidth && routeHeight <= usableHeight) {
            bestZoom = zoom;
            break;
        }
    }

    setCenter(centerLat, centerLon, bestZoom);
}

void MapPageWidget::updateSearchSuggestions(const QString &text)
{
    struct Place {
        const char *name;
        const char *pinyin;
        const char *abbr;
        double lat;
        double lon;
    };

    static const Place places[] = {
        {"河海大学常州新校区", "hohai changzhou", "hh", 31.674600, 119.576472},
        {"金坛区", "jintan", "jt", 31.723220, 119.597897},
        {"常州", "changzhou", "cz", 31.810689, 119.974092},
        {"北京", "beijing", "bj", 39.9042, 116.4074},
        {"上海", "shanghai", "sh", 31.2304, 121.4737},
        {"广州", "guangzhou", "gz", 23.1291, 113.2644},
        {"深圳", "shenzhen", "sz", 22.5431, 114.0579},
        {"杭州", "hangzhou", "hz", 30.2741, 120.1551},
        {"成都", "chengdu", "cd", 30.5728, 104.0668},
        {"西安", "xian", "xa", 34.3416, 108.9398},
        {"武汉", "wuhan", "wh", 30.5928, 114.3055},
        {"南京", "nanjing", "nj", 32.0603, 118.7969},
        {"重庆", "chongqing", "cq", 29.5630, 106.5516},
        {"天津", "tianjin", "tj", 39.3434, 117.3616},
        {"苏州", "suzhou", "suz", 31.2989, 120.5853},
        {"正点原子", "zhengdianyuanzi", "zdyz", 23.1291, 113.2644}
    };

    const QString keyword = text.trimmed().toLower();
    m_suggestionList->clear();

    if (keyword.size() >= 2)
        requestInputTips(text.trimmed());

    for (const auto &place : places) {
        const QString name = QString::fromUtf8(place.name);
        const QString pinyin = QString::fromLatin1(place.pinyin);
        const QString abbr = QString::fromLatin1(place.abbr);
        if (!keyword.isEmpty()
            && !name.toLower().contains(keyword)
            && !pinyin.contains(keyword)
            && !abbr.contains(keyword)) {
            continue;
        }
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(name, pinyin), m_suggestionList);
        item->setData(Qt::UserRole, QPointF(place.lat, place.lon));
        item->setData(Qt::UserRole + 1, name);
        if (m_suggestionList->count() >= 5)
            break;
    }
}

void MapPageWidget::requestInputTips(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    if (m_tipReply) {
        m_tipReply->abort();
        m_tipReply->deleteLater();
        m_tipReply = nullptr;
    }

    const QUrl url(QStringLiteral("http://restapi.amap.com/v3/assistant/inputtips?key=%1&keywords=%2&city=&datatype=all")
                       .arg(QString::fromLatin1(kAmapKey),
                            QString::fromUtf8(QUrl::toPercentEncoding(text))));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_tipReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!reply)
            return;
        if (m_tipReply == reply)
            m_tipReply = nullptr;

        const QByteArray data = reply->readAll();
        const bool canceled = (reply->error() == QNetworkReply::OperationCanceledError);
        const bool failed = (reply->error() != QNetworkReply::NoError);
        reply->deleteLater();

        if (canceled || failed)
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QJsonArray tips = doc.object().value(QStringLiteral("tips")).toArray();
        for (const auto &value : tips) {
            const QJsonObject obj = value.toObject();
            const QString name = obj.value(QStringLiteral("name")).toString();
            const QString district = obj.value(QStringLiteral("district")).toString();
            const QString location = obj.value(QStringLiteral("location")).toString();
            QPointF point;
            if (name.isEmpty() || !parseAmapLocation(location, &point))
                continue;

            const QString label = district.isEmpty() ? name : QStringLiteral("%1  %2").arg(name, district);
            bool exists = false;
            for (int i = 0; i < m_suggestionList->count(); ++i) {
                if (m_suggestionList->item(i)->data(Qt::UserRole + 1).toString() == name) {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;

            auto *item = new QListWidgetItem(label, m_suggestionList);
            item->setData(Qt::UserRole, point);
            item->setData(Qt::UserRole + 1, name);
            if (m_suggestionList->count() >= 8)
                break;
        }
    });
}

void MapPageWidget::applySuggestion(const QString &name, const QPointF &point)
{
    if (!m_activeEdit)
        m_activeEdit = m_startEdit;

    {
        QSignalBlocker blocker(m_activeEdit);
        m_activeEdit->setText(name);
    }
    setCenter(point.x(), point.y(), 13);
    hideKeyboard();
}

void MapPageWidget::performSearch()
{
    if (!m_activeEdit)
        m_activeEdit = m_startEdit;

    const QString text = m_activeEdit->text().trimmed();
    if (text.isEmpty())
        return;

    const QStringList parts = text.split(QRegExp(QStringLiteral("[,，\\s]+")), QString::SkipEmptyParts);
    if (parts.size() >= 2) {
        bool latOk = false;
        bool lonOk = false;
        const double lat = parts.at(0).toDouble(&latOk);
        const double lon = parts.at(1).toDouble(&lonOk);
        if (latOk && lonOk) {
            setCenter(lat, lon, 13);
            hideKeyboard();
            return;
        }
    }

    for (int i = 0; i < m_suggestionList->count(); ++i) {
        auto *item = m_suggestionList->item(i);
        const QString name = item->data(Qt::UserRole + 1).toString();
        if (name == text || item->text().contains(text, Qt::CaseInsensitive)) {
            const QPointF point = item->data(Qt::UserRole).toPointF();
            applySuggestion(name, point);
            return;
        }
    }

    if (m_suggestionList->count() > 0) {
        auto *item = m_suggestionList->item(0);
        const QPointF point = item->data(Qt::UserRole).toPointF();
        const QString name = item->data(Qt::UserRole + 1).toString();
        applySuggestion(name, point);
    } else {
        m_statusLabel->setText(QString());
    }
}

void MapPageWidget::planRoute()
{
    const auto resolve = [this](const QString &text, QPointF *out) {
        const QString value = text.trimmed();
        const QStringList parts = value.split(QRegExp(QStringLiteral("[,，\\s]+")), QString::SkipEmptyParts);
        if (parts.size() >= 2) {
            bool latOk = false;
            bool lonOk = false;
            const double lat = parts.at(0).toDouble(&latOk);
            const double lon = parts.at(1).toDouble(&lonOk);
            if (latOk && lonOk) {
                *out = QPointF(lat, lon);
                return true;
            }
        }

        updateSearchSuggestions(value);
        for (int i = 0; i < m_suggestionList->count(); ++i) {
            auto *item = m_suggestionList->item(i);
            const QString name = item->data(Qt::UserRole + 1).toString();
            if (name == value || item->text().contains(value, Qt::CaseInsensitive)) {
                *out = item->data(Qt::UserRole).toPointF();
                return true;
            }
        }
        return false;
    };

    QPointF start;
    QPointF end;
    if (!resolve(m_startEdit->text(), &start) || !resolve(m_endEdit->text(), &end)) {
        m_statusLabel->setText(QString::fromUtf8("请选择起始和目标"));
        return;
    }

    m_routePoints.clear();
    m_routePoints << start << end;
    requestDrivingRoute();
}

void MapPageWidget::requestDrivingRoute()
{
    if (m_routePoints.size() < 2)
        return;

    if (m_routeReply) {
        m_routeReply->abort();
        m_routeReply->deleteLater();
        m_routeReply = nullptr;
    }

    const QPointF start = m_routePoints.first();
    const QPointF end = m_routePoints.last();
    const QString origin = QStringLiteral("%1,%2").arg(start.y(), 0, 'f', 6).arg(start.x(), 0, 'f', 6);
    const QString destination = QStringLiteral("%1,%2").arg(end.y(), 0, 'f', 6).arg(end.x(), 0, 'f', 6);
    const QUrl url(QStringLiteral("http://restapi.amap.com/v3/direction/driving?key=%1&origin=%2&destination=%3&extensions=base")
                       .arg(QString::fromLatin1(kAmapKey), origin, destination));

    m_statusLabel->setText(QString::fromUtf8("正在规划路线..."));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_routeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!reply)
            return;
        if (m_routeReply == reply)
            m_routeReply = nullptr;

        const QByteArray data = reply->readAll();
        const QString error = reply->errorString();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();

        if (!ok) {
            m_statusLabel->setText(QString::fromUtf8("路线规划失败：%1").arg(error));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QJsonObject route = doc.object().value(QStringLiteral("route")).toObject();
        const QJsonArray paths = route.value(QStringLiteral("paths")).toArray();
        if (paths.isEmpty()) {
            m_statusLabel->setText(QString::fromUtf8("未获取到路线"));
            return;
        }

        QList<QPointF> points;
        const QJsonArray steps = paths.first().toObject().value(QStringLiteral("steps")).toArray();
        for (const auto &stepValue : steps) {
            const QString polyline = stepValue.toObject().value(QStringLiteral("polyline")).toString();
            const QStringList pairs = polyline.split(QLatin1Char(';'), QString::SkipEmptyParts);
            for (const QString &pair : pairs) {
                QPointF point;
                if (parseAmapLocation(pair, &point))
                    points << point;
            }
        }

        if (points.size() < 2) {
            m_statusLabel->setText(QString::fromUtf8("路线数据为空"));
            return;
        }

        m_routePoints = points;
        m_hasRoute = true;

        fitRouteToView(points);

        const QString distance = paths.first().toObject().value(QStringLiteral("distance")).toString();
        m_statusLabel->setText(QString::fromUtf8("路线已规划，距离 %1 米").arg(distance));
    });
}

void MapPageWidget::searchTextAppend(const QString &text)
{
    if (!m_activeEdit)
        m_activeEdit = m_startEdit;
    m_activeEdit->setText(m_activeEdit->text() + text);
}

void MapPageWidget::searchBackspace()
{
    if (!m_activeEdit)
        m_activeEdit = m_startEdit;
    QString text = m_activeEdit->text();
    if (!text.isEmpty()) {
        text.chop(1);
        m_activeEdit->setText(text);
    }
}

void MapPageWidget::showKeyboard()
{
    if (m_keyboard) {
        layoutKeyboard();
        m_keyboard->show();
        m_keyboard->raise();
    }
}

void MapPageWidget::hideKeyboard()
{
    if (m_keyboard)
        m_keyboard->hide();
}

void MapPageWidget::layoutKeyboard()
{
    if (!m_keyboard)
        return;

    const int margin = 10;
    const int keyboardHeight = qMin(210, qMax(172, height() / 2));
    m_keyboard->setGeometry(margin,
                            height() - keyboardHeight - margin,
                            width() - margin * 2,
                            keyboardHeight);
}

QString MapPageWidget::tileKey(int x, int y, int z) const
{
    return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y);
}

int MapPageWidget::lonToTileX(double longitude, int zoom)
{
    const double n = qPow(2.0, zoom);
    return qBound(0, int(qFloor((longitude + 180.0) / 360.0 * n)), int(n) - 1);
}

int MapPageWidget::latToTileY(double latitude, int zoom)
{
    latitude = qBound(-85.0511, latitude, 85.0511);
    const double latRad = latitude * kPi / 180.0;
    const double n = qPow(2.0, zoom);
    return qBound(0, int(qFloor((1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / kPi) / 2.0 * n)), int(n) - 1);
}

double MapPageWidget::lonToTileXDouble(double longitude, int zoom)
{
    const double n = qPow(2.0, zoom);
    return qBound(0.0, (longitude + 180.0) / 360.0 * n, n - 1.0);
}

double MapPageWidget::latToTileYDouble(double latitude, int zoom)
{
    latitude = qBound(-85.0511, latitude, 85.0511);
    const double latRad = latitude * kPi / 180.0;
    const double n = qPow(2.0, zoom);
    return qBound(0.0, (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / kPi) / 2.0 * n, n - 1.0);
}

double MapPageWidget::tileXToLon(double x, int zoom)
{
    return x / qPow(2.0, zoom) * 360.0 - 180.0;
}

double MapPageWidget::tileYToLat(double y, int zoom)
{
    const double n = kPi - 2.0 * kPi * y / qPow(2.0, zoom);
    return 180.0 / kPi * qAtan(0.5 * (qExp(n) - qExp(-n)));
}

bool MapPageWidget::parseAmapLocation(const QString &location, QPointF *point)
{
    const QStringList parts = location.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (parts.size() != 2 || !point)
        return false;

    bool lonOk = false;
    bool latOk = false;
    const double lon = parts.at(0).toDouble(&lonOk);
    const double lat = parts.at(1).toDouble(&latOk);
    if (!lonOk || !latOk)
        return false;

    *point = QPointF(lat, lon);
    return true;
}
