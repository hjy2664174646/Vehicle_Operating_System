#include "musicpage.h"

#include "musicplayer.h"
#include "musicprogressslider.h"
#include "vinyldisc.h"

#include <algorithm>
#include <functional>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSlider>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

constexpr const char *kAccentGreen = "#31C27C";
constexpr const char *kAccentGreenBright = "#3DDC84";

void paintMusicBackground(QPainter &p, const QRect &rect)
{
#if defined(__arm__)
    QRadialGradient glow(rect.center(), qreal(qMax(rect.width(), rect.height())) * 0.55);
    glow.setColorAt(0.0, QColor(QStringLiteral("#2a2018")));
    glow.setColorAt(0.35, QColor(QStringLiteral("#14100c")));
    glow.setColorAt(1.0, QColor(QStringLiteral("#080808")));
    p.fillRect(rect, glow);

    QLinearGradient vignette(rect.topLeft(), rect.bottomRight());
    vignette.setColorAt(0.0, QColor(QStringLiteral("#101010")));
    vignette.setColorAt(0.5, QColor(QStringLiteral("#000000")));
    vignette.setColorAt(1.0, QColor(QStringLiteral("#101010")));
    p.fillRect(rect, vignette);
#else
    p.fillRect(rect, QColor(QStringLiteral("#0a0a0a")));

    QRadialGradient glow(rect.center(), qreal(qMax(rect.width(), rect.height())) * 0.52);
    glow.setColorAt(0.0, QColor(58, 44, 32, 200));
    glow.setColorAt(0.45, QColor(20, 16, 12, 120));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, glow);

    QLinearGradient topFade(rect.topLeft(), QPoint(rect.left(), rect.top() + rect.height() / 3));
    topFade.setColorAt(0.0, QColor(0, 0, 0, 80));
    topFade.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect, topFade);
#endif
}

QString listStyleSheet()
{
#if defined(__arm__)
    return QStringLiteral(
        "QListWidget {"
        "  background-color: #141414;"
        "  border: 1px solid #333333;"
        "  border-radius: 8px;"
        "  color: #EEEEEE;"
        "  font-size: 15px;"
        "  padding: 6px;"
        "}"
        "QListWidget::item {"
        "  padding: 10px 12px;"
        "  border-radius: 6px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #1f3d2e;"
        "  color: #31C27C;"
        "}");
#else
    return QStringLiteral(
        "QListWidget {"
        "  background-color: rgba(20, 20, 20, 0.72);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  border-radius: 10px;"
        "  color: #EEEEEE;"
        "  font-size: 15px;"
        "  padding: 6px;"
        "}"
        "QListWidget::item {"
        "  padding: 10px 12px;"
        "  border-radius: 6px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(49, 194, 124, 0.22);"
        "  color: #31C27C;"
        "}");
#endif
}

QString iconButtonStyle()
{
#if defined(__arm__)
    return QStringLiteral(
        "QPushButton {"
        "  color: #CCCCCC;"
        "  background-color: #222222;"
        "  border: 1px solid #444444;"
        "  border-radius: 18px;"
        "  font-size: 16px;"
        "  min-width: 36px; min-height: 36px; max-width: 36px; max-height: 36px;"
        "}"
        "QPushButton:pressed { background-color: #333333; }");
#else
    return QStringLiteral(
        "QPushButton {"
        "  color: #DDDDDD;"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 18px;"
        "  font-size: 16px;"
        "  min-width: 36px; min-height: 36px; max-width: 36px; max-height: 36px;"
        "}"
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.14); }");
#endif
}

constexpr const char *kIconPlay = ":/picture/play.png";
constexpr const char *kIconStop = ":/picture/stop.png";
constexpr const char *kIconLast = ":/picture/last.png";
constexpr const char *kIconNext = ":/picture/next.png";
constexpr const char *kIconList = ":/picture/list.png";

QString pictureButtonStyle()
{
#if defined(__arm__)
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:pressed { background-color: #333333; border-radius: 20px; }");
#else
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; }"
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.12); border-radius: 20px; }");
#endif
}

QString bottomBarStyle()
{
#if defined(__arm__)
    return QStringLiteral("background-color: #121212; border-top: 1px solid #2a2a2a;");
#else
    return QStringLiteral(
        "background-color: rgba(18, 18, 18, 0.88);"
        "border-top: 1px solid rgba(255, 255, 255, 0.06);");
#endif
}

QVector<QPair<qint64, QString>> parseLrcFile(const QString &lrcPath)
{
    QVector<QPair<qint64, QString>> lines;
    QFile file(lrcPath);
    if (!file.open(QIODevice::ReadOnly))
        return lines;

    QByteArray data = file.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);

    QString content = QString::fromUtf8(data);
    if (content.contains(QChar(0xFFFD)))
        content = QString::fromLocal8Bit(data);

    static const QRegularExpression tag(
        QStringLiteral(R"(\[(\d{1,2}):(\d{2})(?:\.(\d{1,3}))?\])"));

    const QStringList rows = content.split(QChar('\n'), QString::SkipEmptyParts);
    for (QString raw : rows) {
        raw = raw.trimmed();
        if (raw.isEmpty())
            continue;

        QRegularExpressionMatchIterator it = tag.globalMatch(raw);
        if (!it.hasNext())
            continue;

        QString text = raw;
        it = tag.globalMatch(raw);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            text.remove(m.captured(0));
        }
        text = text.trimmed();
        if (text.isEmpty())
            continue;

        it = tag.globalMatch(raw);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const int min = m.captured(1).toInt();
            const int sec = m.captured(2).toInt();
            const QString msStr = m.captured(3);
            int msPart = 0;
            if (!msStr.isEmpty()) {
                if (msStr.length() <= 2)
                    msPart = msStr.toInt() * 10;
                else
                    msPart = msStr.left(3).toInt();
            }
            const qint64 ms = qint64(min) * 60000 + qint64(sec) * 1000 + msPart;
            lines.append(qMakePair(ms, text));
        }
    }

    std::sort(lines.begin(), lines.end(),
              [](const QPair<qint64, QString> &a, const QPair<qint64, QString> &b) {
        return a.first < b.first;
    });
    return lines;
}

QVector<QPair<qint64, QString>> defaultLyrics(const QString &title)
{
    return {
        {0, title},
        {5000, QStringLiteral("暂无 LRC 歌词文件")},
        {10000, QStringLiteral("可在同目录放置同名 .lrc 文件")},
        {15000, QStringLiteral("享受音乐吧~")},
    };
}

QPushButton *makePictureButton(const QString &iconPath, int iconSize, QWidget *parent)
{
    auto *btn = new QPushButton(parent);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(iconSize, iconSize));
    const int pad = 8;
    btn->setFixedSize(iconSize + pad, iconSize + pad);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(pictureButtonStyle());
    return btn;
}

class PlaylistDrawerPanel : public QWidget
{
public:
    explicit PlaylistDrawerPanel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

#if defined(__arm__)
        p.fillRect(rect(), QColor(QStringLiteral("#141414")));
        QPen edge(QColor(QStringLiteral("#333333")));
#else
        p.fillRect(rect(), QColor(18, 18, 18, 215));
        QPen edge(QColor(255, 255, 255, 40));
#endif
        edge.setWidth(1);
        p.setPen(edge);
        p.drawLine(rect().left(), rect().top(), rect().left(), rect().bottom());
    }
};

} // namespace

MusicPageWidget::MusicPageWidget(QWidget *parent)
    : QWidget(parent)
    , m_player(new MusicPlayer(this))
{
    setAutoFillBackground(false);
    setBackgroundRole(QPalette::NoRole);
    // Removed WA_OpaquePaintEvent to allow transparency

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_playerRoot = new QWidget(this);
    m_playerRoot->setAutoFillBackground(false);
    m_playerRoot->setBackgroundRole(QPalette::NoRole);
    QPalette playerRootPalette = m_playerRoot->palette();
    playerRootPalette.setColor(QPalette::Window, Qt::transparent);
    playerRootPalette.setColor(QPalette::Base, Qt::transparent);
    m_playerRoot->setPalette(playerRootPalette);
    root->addWidget(m_playerRoot);

    buildPlayerPage();
    buildPlaylistDrawer();
    refreshPlaylist();

    connect(m_player, &MusicPlayer::trackChanged, this, [this](int, const QString &title, const QString &) {
        m_titleLabel->setText(title);
        m_artistLabel->setText(QStringLiteral("本地音乐"));
        loadLyricsForCurrentTrack();
    });

    connect(m_player, &MusicPlayer::positionChanged, this, [this](qint64 pos) {
        if (!m_seeking)
            updateProgressUi(pos, m_player->duration());
        updateLyricHighlight(pos);
    });

    connect(m_player, &MusicPlayer::durationChanged, this, [this](qint64 dur) {
        updateProgressUi(m_player->position(), dur);
    });

    connect(m_player, &MusicPlayer::playingChanged, this, [this](bool playing) {
        m_vinyl->setPlaying(playing);
        m_playBtn->setIcon(QIcon(QString::fromUtf8(playing ? kIconStop : kIconPlay)));
    });

    connect(m_player, &MusicPlayer::errorOccurred, this, [this](const QString &msg) {
        m_listHint->setText(QStringLiteral("播放错误：%1").arg(msg));
    });
}

void MusicPageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    paintMusicBackground(p, rect());
}

void MusicPageWidget::buildPlaylistDrawer()
{
    m_drawerBackdrop = new DrawerBackdrop(this);
    m_drawerBackdrop->clickHandler = [this]() { setPlaylistDrawerOpen(false, true); };

    m_playlistDrawer = new PlaylistDrawerPanel(this);
    m_playlistDrawer->hide();

    auto *layout = new QVBoxLayout(m_playlistDrawer);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("歌曲列表"), m_playlistDrawer);
    title->setStyleSheet(QStringLiteral(
        "color: #EEEEEE; font-size: 18px; font-weight: bold; background: transparent;"));

    m_drawerCountLabel = new QLabel(m_playlistDrawer);
    m_drawerCountLabel->setStyleSheet(QStringLiteral(
        "color: #31C27C; font-size: 12px; background: transparent;"));

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), m_playlistDrawer);
    refreshBtn->setFocusPolicy(Qt::NoFocus);
    refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: #31C27C; background: transparent; border: 1px solid #31C27C;"
        "  border-radius: 12px; padding: 3px 12px; font-size: 12px; }"
        "QPushButton:pressed { background-color: #1f3d2e; }"));

    header->addWidget(title);
    header->addWidget(m_drawerCountLabel);
    header->addStretch();
    refreshBtn->hide();

    m_listHint = new QLabel(m_playlistDrawer);
    m_listHint->setStyleSheet(QStringLiteral("color: #888888; font-size: 12px; background: transparent;"));
    m_listHint->setWordWrap(true);

    m_list = new QListWidget(m_playlistDrawer);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setStyleSheet(listStyleSheet());
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    layout->addLayout(header);
    layout->addWidget(m_listHint);
    layout->addWidget(m_list, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &MusicPageWidget::refreshPlaylist);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        m_player->playIndex(item->data(Qt::UserRole).toInt());
        setPlaylistDrawerOpen(false, true);
    });

    m_playlistToggleBtn = new QPushButton(QStringLiteral("☰ 列表"), this);
    m_playlistToggleBtn->setFocusPolicy(Qt::NoFocus);
    m_playlistToggleBtn->setCursor(Qt::PointingHandCursor);
    m_playlistToggleBtn->setStyleSheet(iconButtonStyle() +
        QStringLiteral(" min-width: 72px; max-width: 96px; min-height: 36px; padding: 0 8px;"));
    m_playlistToggleBtn->setText(QString());
    m_playlistToggleBtn->setIcon(QIcon(QString::fromUtf8(kIconList)));
    m_playlistToggleBtn->setIconSize(QSize(24, 24));
    m_playlistToggleBtn->setStyleSheet(pictureButtonStyle());
    connect(m_playlistToggleBtn, &QPushButton::clicked, this, &MusicPageWidget::togglePlaylistDrawer);
}

int MusicPageWidget::drawerWidth() const
{
    return qMin(260, qMax(210, width() / 3));
}

void MusicPageWidget::togglePlaylistDrawer()
{
    setPlaylistDrawerOpen(!m_drawerOpen, true);
}

void MusicPageWidget::setPlaylistDrawerOpen(bool open, bool animated)
{
    if (m_drawerOpen == open && m_playlistDrawer->isVisible() == open)
        return;

    m_drawerOpen = open;
    if (open)
        m_drawerBackdrop->setVisible(true);
    m_playlistDrawer->setVisible(true);

    if (m_playlistToggleBtn) {
#if defined(__arm__)
        const QString activeStyle = QStringLiteral(
            "QPushButton { color: #31C27C; background-color: #1f3d2e;"
            "  border: 1px solid #31C27C; border-radius: 18px; font-size: 15px;"
            "  min-width: 40px; min-height: 36px; padding: 0 10px; }"
            "QPushButton:pressed { background-color: #2a5240; }");
#else
        const QString activeStyle = QStringLiteral(
            "QPushButton { color: #31C27C; background-color: rgba(49, 194, 124, 0.18);"
            "  border: 1px solid #31C27C; border-radius: 18px; font-size: 15px;"
            "  min-width: 40px; min-height: 36px; padding: 0 10px; }"
            "QPushButton:pressed { background-color: rgba(49, 194, 124, 0.28); }");
#endif
        m_playlistToggleBtn->setStyleSheet(open ? activeStyle : iconButtonStyle() +
            QStringLiteral(" min-width: 72px; max-width: 96px; min-height: 36px; padding: 0 8px;"));
        m_playlistToggleBtn->setStyleSheet(pictureButtonStyle());
    }

    layoutPlaylistDrawer(animated);

    if (open) {
        m_drawerBackdrop->raise();
        m_playlistDrawer->raise();
    } else if (m_playlistToggleBtn) {
        m_playlistToggleBtn->raise();
    }
}

void MusicPageWidget::layoutPlaylistDrawer(bool animated)
{
    if (!m_playlistDrawer)
        return;

    const int w = width();
    const int h = height();
    const int panelW = drawerWidth();

    m_drawerBackdrop->setGeometry(0, 0, w, h);

    const QRect closedRect(w, 0, panelW, h);
    const QRect openRect(w - panelW, 0, panelW, h);
    const QRect target = m_drawerOpen ? openRect : closedRect;

    if (m_drawerAnim) {
        m_drawerAnim->stop();
        m_drawerAnim->deleteLater();
        m_drawerAnim = nullptr;
    }

    if (!animated) {
        m_playlistDrawer->setGeometry(target);
        if (!m_drawerOpen) {
            m_playlistDrawer->hide();
            m_drawerBackdrop->hide();
        }
        return;
    }

    if (!m_playlistDrawer->isVisible())
        m_playlistDrawer->setGeometry(closedRect);

    m_drawerAnim = new QPropertyAnimation(m_playlistDrawer, "geometry", this);
    m_drawerAnim->setStartValue(m_playlistDrawer->geometry());
    m_drawerAnim->setEndValue(target);
    m_drawerAnim->setDuration(280);
    m_drawerAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_drawerAnim, &QPropertyAnimation::finished, this, [this]() {
        m_drawerAnim = nullptr;
        if (!m_drawerOpen) {
            m_playlistDrawer->hide();
            m_drawerBackdrop->hide();
        }
    });
    m_drawerAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MusicPageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_drawerBackdrop)
        m_drawerBackdrop->setGeometry(0, 0, width(), height());

    if (m_playlistToggleBtn) {
        const int btnW = 32;
        m_playlistToggleBtn->setGeometry(width() - btnW - 10, 8, btnW, 36);
        if (!m_drawerOpen)
            m_playlistToggleBtn->raise();
    }

    layoutPlaylistDrawer(false);
}

void MusicPageWidget::buildPlayerPage()
{
    auto *page = m_playerRoot;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *body = new QWidget(page);
    body->setAutoFillBackground(false);
    body->setBackgroundRole(QPalette::NoRole);
    QPalette bodyPalette = body->palette();
    bodyPalette.setColor(QPalette::Window, Qt::transparent);
    bodyPalette.setColor(QPalette::Base, Qt::transparent);
    body->setPalette(bodyPalette);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(16, 12, 16, 0);
    bodyLayout->setSpacing(12);

    m_vinyl = new VinylDiscWidget(body);
    bodyLayout->addWidget(m_vinyl, 4);

    auto *infoCol = new QWidget(body);
    infoCol->setAutoFillBackground(false);
    infoCol->setBackgroundRole(QPalette::NoRole);
    QPalette infoColPalette = infoCol->palette();
    infoColPalette.setColor(QPalette::Window, Qt::transparent);
    infoColPalette.setColor(QPalette::Base, Qt::transparent);
    infoCol->setPalette(infoColPalette);
    auto *infoLayout = new QVBoxLayout(infoCol);
    infoLayout->setContentsMargins(0, 8, 96, 0);
    infoLayout->setSpacing(6);

    m_titleLabel = new QLabel(QStringLiteral("未播放"), infoCol);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: #FFFFFF; font-size: 18px; font-weight: bold; background: transparent;"));

    m_artistLabel = new QLabel(QStringLiteral("本地音乐"), infoCol);
    m_artistLabel->setStyleSheet(QStringLiteral(
        "color: #999999; font-size: 13px; background: transparent;"));

    auto *tagRow = new QHBoxLayout;
    auto *tag = new QLabel(QStringLiteral("本地"), infoCol);
    tag->setStyleSheet(QStringLiteral(
        "color: #AAAAAA; font-size: 11px; background: transparent;"
        "border: 1px solid #555555; border-radius: 4px; padding: 2px 8px;"));
    tagRow->addWidget(tag);
    tagRow->addStretch();

    auto *scroll = new QScrollArea(infoCol);
    m_lyricScroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAutoFillBackground(false);
    scroll->setBackgroundRole(QPalette::NoRole);
    QPalette scrollPalette = scroll->palette();
    scrollPalette.setColor(QPalette::Window, Qt::transparent);
    scrollPalette.setColor(QPalette::Base, Qt::transparent);
    scroll->setPalette(scrollPalette);
    // Removed stylesheet to rely on palette settings

    m_lyricPanel = new QWidget;
    m_lyricPanel->setAutoFillBackground(false);
    m_lyricPanel->setBackgroundRole(QPalette::NoRole);
    QPalette panelPalette = m_lyricPanel->palette();
    panelPalette.setColor(QPalette::Window, Qt::transparent);
    panelPalette.setColor(QPalette::Base, Qt::transparent);
    m_lyricPanel->setPalette(panelPalette);
    m_lyricLayout = new QVBoxLayout(m_lyricPanel);
    m_lyricLayout->setContentsMargins(0, 0, 0, 0);
    m_lyricLayout->setSpacing(12);

    m_lyricTopPad = new QWidget(m_lyricPanel);
    m_lyricTopPad->setAutoFillBackground(false);
    m_lyricTopPad->setBackgroundRole(QPalette::NoRole);
    QPalette topPadPalette = m_lyricTopPad->palette();
    topPadPalette.setColor(QPalette::Window, Qt::transparent);
    topPadPalette.setColor(QPalette::Base, Qt::transparent);
    m_lyricTopPad->setPalette(topPadPalette);
    m_lyricTopPad->setFixedHeight(80);
    m_lyricBottomPad = new QWidget(m_lyricPanel);
    m_lyricBottomPad->setAutoFillBackground(false);
    m_lyricBottomPad->setBackgroundRole(QPalette::NoRole);
    QPalette bottomPadPalette = m_lyricBottomPad->palette();
    bottomPadPalette.setColor(QPalette::Window, Qt::transparent);
    bottomPadPalette.setColor(QPalette::Base, Qt::transparent);
    m_lyricBottomPad->setPalette(bottomPadPalette);
    m_lyricBottomPad->setFixedHeight(80);
    m_lyricLayout->addWidget(m_lyricTopPad);
    m_lyricLayout->addWidget(m_lyricBottomPad);

    scroll->setWidget(m_lyricPanel);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->viewport()->setBackgroundRole(QPalette::NoRole);
    QPalette viewportPalette = scroll->viewport()->palette();
    viewportPalette.setColor(QPalette::Window, Qt::transparent);
    viewportPalette.setColor(QPalette::Base, Qt::transparent);
    scroll->viewport()->setPalette(viewportPalette);
    scroll->viewport()->installEventFilter(this);

    // Also set the scroll area's widget palette
    if (scroll->widget()) {
        scroll->widget()->setAutoFillBackground(false);
        scroll->widget()->setBackgroundRole(QPalette::NoRole);
        QPalette widgetPalette = scroll->widget()->palette();
        widgetPalette.setColor(QPalette::Window, Qt::transparent);
        widgetPalette.setColor(QPalette::Base, Qt::transparent);
        scroll->widget()->setPalette(widgetPalette);
    }

    // Re-apply the panel palette after setting it as the scroll widget
    m_lyricPanel->setAutoFillBackground(false);
    m_lyricPanel->setBackgroundRole(QPalette::NoRole);
    panelPalette = m_lyricPanel->palette();
    panelPalette.setColor(QPalette::Window, Qt::transparent);
    panelPalette.setColor(QPalette::Base, Qt::transparent);
    m_lyricPanel->setPalette(panelPalette);

    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_artistLabel);
    infoLayout->addLayout(tagRow);
    infoLayout->addWidget(scroll, 1);

    bodyLayout->addWidget(infoCol, 5);
    layout->addWidget(body, 1);

    auto *bottom = new QWidget(page);
    bottom->setAutoFillBackground(true);
    bottom->setAttribute(Qt::WA_StyledBackground, true);
    bottom->setFixedHeight(128);
    bottom->setStyleSheet(bottomBarStyle());

    auto *bottomLayout = new QVBoxLayout(bottom);
    bottomLayout->setContentsMargins(16, 6, 16, 8);
    bottomLayout->setSpacing(4);

    auto *progressWrap = new QWidget(bottom);
    progressWrap->setFixedHeight(30);
    progressWrap->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *progressWrapLayout = new QVBoxLayout(progressWrap);
    progressWrapLayout->setContentsMargins(0, 5, 0, 5);
    progressWrapLayout->setSpacing(0);

    m_progress = new GradientProgressSlider(progressWrap);
    m_progress->setRange(0, 1000);
    m_progress->setFixedHeight(20);
    progressWrapLayout->addWidget(m_progress);

    auto *timeRow = new QHBoxLayout;
    m_timeCurrent = new QLabel(QStringLiteral("00:00"), bottom);
    m_timeTotal = new QLabel(QStringLiteral("00:00"), bottom);
    const QString timeStyle = QStringLiteral("color: #888888; font-size: 11px; background: transparent;");
    m_timeCurrent->setStyleSheet(timeStyle);
    m_timeTotal->setStyleSheet(timeStyle);
    timeRow->addWidget(m_timeCurrent);
    timeRow->addStretch();
    timeRow->addWidget(m_timeTotal);

    auto *ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(12);
    ctrlRow->setContentsMargins(0, 0, 0, 0);

    auto *prevBtn = makePictureButton(QString::fromUtf8(kIconLast), 30, bottom);
    m_playBtn = makePictureButton(QString::fromUtf8(kIconPlay), 44, bottom);
    auto *nextBtn = makePictureButton(QString::fromUtf8(kIconNext), 30, bottom);

    auto *volumeLabel = new QLabel(QStringLiteral("音量"), bottom);
    volumeLabel->setStyleSheet(QStringLiteral("color: #BBBBBB; font-size: 13px; background: transparent;"));
    m_volumeSlider = new QSlider(Qt::Horizontal, bottom);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedSize(96, 20);
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setStyleSheet(QStringLiteral(
        "QSlider { background: transparent; }"
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: rgba(255,255,255,0.18); }"
        "QSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: #31C27C; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px;"
        "  background: #e8fff3; border: 1px solid #31C27C; }"));

    ctrlRow->addStretch();
    ctrlRow->addWidget(prevBtn, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(m_playBtn, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(nextBtn, 0, Qt::AlignVCenter);
    ctrlRow->addSpacing(22);
    ctrlRow->addWidget(volumeLabel, 0, Qt::AlignVCenter);
    ctrlRow->addWidget(m_volumeSlider, 0, Qt::AlignVCenter);
    ctrlRow->addStretch();

    bottomLayout->addWidget(progressWrap);
    bottomLayout->addLayout(timeRow);
    bottomLayout->addLayout(ctrlRow);
    layout->addWidget(bottom);

    connect(prevBtn, &QPushButton::clicked, m_player, &MusicPlayer::previous);
    connect(m_playBtn, &QPushButton::clicked, m_player, &MusicPlayer::playPause);
    connect(nextBtn, &QPushButton::clicked, m_player, &MusicPlayer::next);
    connect(m_volumeSlider, &QSlider::valueChanged, m_player, &MusicPlayer::setVolume);

    connect(m_progress, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });

    connect(m_progress, &QSlider::sliderMoved, this, [this](int value) {
        const qint64 dur = m_player->duration();
        if (dur <= 0)
            return;
        const qint64 pos = qint64(value) * dur / 1000;
        m_timeCurrent->setText(formatTime(pos));
        updateLyricHighlight(pos);
    });

    connect(m_progress, &QSlider::sliderReleased, this, [this]() {
        const qint64 dur = m_player->duration();
        if (dur > 0)
            m_player->seek(qint64(m_progress->value()) * dur / 1000);
        m_seeking = false;
    });

    connect(m_progress, &QSlider::actionTriggered, this, [this](int action) {
        const qint64 dur = m_player->duration();
        if (dur <= 0)
            return;

        if (action == QSlider::SliderPageStepAdd || action == QSlider::SliderPageStepSub) {
            // 点击进度条空白处跳转
            m_seeking = true;
            const qint64 pos = qint64(m_progress->value()) * dur / 1000;
            m_player->seek(pos);
            m_timeCurrent->setText(formatTime(pos));
            updateLyricHighlight(pos);
            m_seeking = false;
        } else if (action != QSlider::SliderNoAction) {
            m_seeking = true;
        }
    });
}

void MusicPageWidget::refreshPlaylist()
{
    const QStringList files = m_player->scanMusicFiles();
    m_player->setPlaylist(files);

    m_list->clear();
    for (int i = 0; i < files.size(); ++i) {
        const QFileInfo info(files.at(i));
        auto *item = new QListWidgetItem(info.completeBaseName(), m_list);
        item->setData(Qt::UserRole, i);
    }

    if (m_drawerCountLabel)
        m_drawerCountLabel->setText(QStringLiteral("(%1)").arg(files.size()));

    if (files.isEmpty()) {
        const QString hint = QStringLiteral(
            "未找到音乐文件。请将 mp3/wav 放入以下目录之一后点「刷新」：\n%1/music");
        m_listHint->setText(hint.arg(QCoreApplication::applicationDirPath()));
    } else {
        m_listHint->setText(QStringLiteral("共 %1 首，点击歌曲开始播放").arg(files.size()));
    }
}

QString MusicPageWidget::formatTime(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const int totalSec = int(ms / 1000);
    const int min = totalSec / 60;
    const int sec = totalSec % 60;
    return QStringLiteral("%1:%2")
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'));
}

void MusicPageWidget::updateProgressUi(qint64 position, qint64 duration)
{
    m_timeCurrent->setText(formatTime(position));
    m_timeTotal->setText(formatTime(duration));

    if (duration > 0 && !m_seeking)
        m_progress->setValue(int(position * 1000 / duration));
    else if (duration <= 0)
        m_progress->setValue(0);
}

void MusicPageWidget::loadLyricsForCurrentTrack()
{
    for (QLabel *label : m_lyricLabels)
        label->deleteLater();
    m_lyricLabels.clear();

    while (m_lyricLayout->count() > 2) {
        QLayoutItem *item = m_lyricLayout->takeAt(1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QString path = m_player->currentPath();
    const QString lrcPath = path.left(path.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".lrc");
    m_lyrics = parseLrcFile(lrcPath);
    if (m_lyrics.isEmpty())
        m_lyrics = defaultLyrics(m_player->currentTitle());

    m_activeLyric = -1;
    for (const auto &line : m_lyrics) {
        auto *label = new QLabel(line.second, m_lyricPanel);
        label->setAutoFillBackground(false);
        label->setBackgroundRole(QPalette::NoRole);
        QPalette labelPalette = label->palette();
        labelPalette.setColor(QPalette::Window, Qt::transparent);
        labelPalette.setColor(QPalette::Base, Qt::transparent);
        labelPalette.setColor(QPalette::Background, Qt::transparent);
        labelPalette.setColor(QPalette::Text, QColor(0xBB, 0xBB, 0xBB));
        label->setPalette(labelPalette);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        label->setStyleSheet(QStringLiteral(
            "color: #BBBBBB; font-size: 15px; padding: 4px 0;"));
        m_lyricLabels.append(label);
        m_lyricLayout->insertWidget(m_lyricLayout->count() - 1, label);
    }

    updateLyricPadding();
    QTimer::singleShot(0, this, [this]() {
        updateLyricHighlight(m_player->position());
    });
}

void MusicPageWidget::updateLyricPadding()
{
    if (!m_lyricScroll || !m_lyricTopPad || !m_lyricBottomPad)
        return;

    const int pad = qMax(72, m_lyricScroll->viewport()->height() / 2 - 12);
    m_lyricTopPad->setFixedHeight(pad);
    m_lyricBottomPad->setFixedHeight(pad);
}

void MusicPageWidget::scrollLyricToIndex(int index, bool animated)
{
    if (!m_lyricScroll || index < 0 || index >= m_lyricLabels.size())
        return;

    QLabel *label = m_lyricLabels.at(index);
    if (!label)
        return;

    m_lyricPanel->adjustSize();

    const int viewH = m_lyricScroll->viewport()->height();
    const int centerY = label->mapTo(m_lyricPanel, label->rect().center()).y();
    int target = centerY - viewH / 2;

    QScrollBar *bar = m_lyricScroll->verticalScrollBar();
    target = qBound(bar->minimum(), target, bar->maximum());

    const int current = bar->value();
    if (current == target)
        return;

    if (!animated || qAbs(target - current) < 6) {
        if (m_lyricScrollAnim) {
            m_lyricScrollAnim->stop();
            m_lyricScrollAnim->deleteLater();
            m_lyricScrollAnim = nullptr;
        }
        bar->setValue(target);
        return;
    }

    if (m_lyricScrollAnim) {
        m_lyricScrollAnim->stop();
        m_lyricScrollAnim->deleteLater();
        m_lyricScrollAnim = nullptr;
    }

    auto *anim = new QPropertyAnimation(bar, "value", this);
    m_lyricScrollAnim = anim;
    anim->setStartValue(current);
    anim->setEndValue(target);
    anim->setDuration(qBound(220, qAbs(target - current) * 2, 520));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        m_lyricScrollAnim = nullptr;
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MusicPageWidget::updateLyricHighlight(qint64 positionMs)
{
    if (m_lyrics.isEmpty() || m_lyricLabels.isEmpty())
        return;

    int idx = 0;
    for (int i = 0; i < m_lyrics.size(); ++i) {
        if (m_lyrics.at(i).first <= positionMs)
            idx = i;
        else
            break;
    }

    const bool indexChanged = (idx != m_activeLyric);
    m_activeLyric = idx;

    for (int i = 0; i < m_lyricLabels.size(); ++i) {
        QLabel *label = m_lyricLabels.at(i);
        if (!label)
            continue;

        if (i == m_activeLyric) {
            label->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 18px; font-weight: bold; background: transparent; padding: 4px 0;")
                                     .arg(QString::fromUtf8(kAccentGreen)));
        } else {
            label->setStyleSheet(QStringLiteral(
                "color: %1; font-size: 15px; background: transparent; padding: 4px 0;")
                                     .arg(i < m_activeLyric ? QStringLiteral("#666666")
                                                            : QStringLiteral("#BBBBBB")));
        }
    }

    if (indexChanged)
        scrollLyricToIndex(m_activeLyric, !m_seeking);
}

bool MusicPageWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (m_lyricScroll && watched == m_lyricScroll->viewport() && event->type() == QEvent::Resize) {
        updateLyricPadding();
        if (m_activeLyric >= 0)
            scrollLyricToIndex(m_activeLyric, false);
    }
    return QWidget::eventFilter(watched, event);
}

void DrawerBackdrop::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
#if defined(__arm__)
    p.fillRect(rect(), QColor(0, 0, 0, 160));
#else
    p.fillRect(rect(), QColor(0, 0, 0, 120));
#endif
}

void DrawerBackdrop::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (clickHandler)
        clickHandler();
}
