#include "swipeview.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTouchEvent>
#include <QEasingCurve>
#include <QtMath>

SwipeView::SwipeView(QWidget *parent)
    : QWidget(parent)
    , m_strip(new QWidget(this))
{
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setMouseTracking(true);
    // 勿对 m_strip 设置 WA_TransparentForMouseEvents，否则 Windows 上鼠标到不了子控件
}

void SwipeView::installPointerFilters(QWidget *root)
{
    if (!root)
        return;

    root->setMouseTracking(true);
    root->installEventFilter(this);
    for (QObject *child : root->children()) {
        if (auto *childWidget = qobject_cast<QWidget *>(child))
            installPointerFilters(childWidget);
    }
}

void SwipeView::addPage(QWidget *page)
{
    if (!page)
        return;

    page->setParent(m_strip);
    installPointerFilters(page);
    m_pages.append(page);
    layoutPages();
}

int SwipeView::pageCount() const
{
    return m_pages.size();
}

int SwipeView::currentIndex() const
{
    return m_currentIndex;
}

void SwipeView::setCurrentIndex(int index, bool animated)
{
    if (m_pages.isEmpty())
        return;

    index = qBound(0, index, m_pages.size() - 1);
    if (index == m_currentIndex && m_pixelOffset == offsetForIndex(index) && !m_pointerDown)
        return;

    m_currentIndex = index;
    setStripOffset(offsetForIndex(index), animated);
    updatePageStacking();
    emit currentIndexChanged(m_currentIndex);
}

int SwipeView::offsetForIndex(int index) const
{
    return -index * width();
}

void SwipeView::updatePageStacking()
{
    for (int i = 0; i < m_pages.size(); ++i) {
        // 非当前页不参与命中，避免叠层时挡住 App 按钮
        m_pages.at(i)->setAttribute(Qt::WA_TransparentForMouseEvents, i != m_currentIndex);
    }

    if (m_currentIndex >= 0 && m_currentIndex < m_pages.size())
        m_pages.at(m_currentIndex)->raise();
}

void SwipeView::layoutPages()
{
    const int w = qMax(1, width());
    const int h = qMax(1, height());

    m_strip->setGeometry(0, 0, w * m_pages.size(), h);
    for (int i = 0; i < m_pages.size(); ++i)
        m_pages.at(i)->setGeometry(i * w, 0, w, h);

    if (!m_pointerDown)
        m_strip->move(m_pixelOffset, 0);

    updatePageStacking();
}

void SwipeView::setStripOffset(int pixelOffset, bool animated)
{
    const int minOffset = -qMax(0, (m_pages.size() - 1) * width());
    const int maxOffset = 0;
    m_pixelOffset = qBound(minOffset, pixelOffset, maxOffset);

    if (m_anim) {
        m_anim->stop();
        m_anim->deleteLater();
        m_anim = nullptr;
    }

#if defined(__arm__)
    Q_UNUSED(animated);
    m_strip->move(m_pixelOffset, 0);
    return;
#else
    if (!animated || width() <= 0) {
        m_strip->move(m_pixelOffset, 0);
        return;
    }
#endif

    m_anim = new QPropertyAnimation(m_strip, "pos", this);
    m_anim->setDuration(280);
    m_anim->setStartValue(m_strip->pos());
    m_anim->setEndValue(QPoint(m_pixelOffset, 0));
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished, m_anim, &QObject::deleteLater);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() { m_anim = nullptr; });
    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SwipeView::finishDrag()
{
    m_pointerDown = false;
    m_swipeGesture = false;
    m_gestureLocked = false;

    if (m_pages.isEmpty() || width() <= 0) {
        setStripOffset(0, true);
        return;
    }

    const int delta = m_pixelOffset - m_pressOffset;
    const int threshold = width() / 4;
    int targetIndex = m_currentIndex;

    if (delta <= -threshold && m_currentIndex < m_pages.size() - 1)
        targetIndex = m_currentIndex + 1;
    else if (delta >= threshold && m_currentIndex > 0)
        targetIndex = m_currentIndex - 1;

    m_currentIndex = targetIndex;
    setStripOffset(offsetForIndex(targetIndex), true);
    updatePageStacking();
    emit currentIndexChanged(m_currentIndex);
}

bool SwipeView::isInSwipePage(QWidget *widget) const
{
    if (!widget)
        return false;

    for (QWidget *page : m_pages) {
        for (QWidget *w = widget; w; w = w->parentWidget()) {
            if (w == page)
                return true;
        }
    }
    return false;
}

bool SwipeView::handlePointer(const QPointF &localPos, QEvent::Type type)
{
    switch (type) {
    case QEvent::TouchBegin:
    case QEvent::MouseButtonPress:
        if (m_anim)
            m_anim->stop();
        m_pointerDown = true;
        m_swipeGesture = false;
        m_gestureLocked = false;
        m_pressLocalX = localPos.x();
        m_pressLocalY = localPos.y();
        m_pressOffset = m_pixelOffset;
        return false;
    case QEvent::TouchUpdate:
    case QEvent::MouseMove:
        if (!m_pointerDown)
            return false;
        if (!m_gestureLocked) {
            const qreal dx = qAbs(localPos.x() - m_pressLocalX);
            const qreal dy = qAbs(localPos.y() - m_pressLocalY);
            if (dx > 12 || dy > 12) {
                m_gestureLocked = true;
                m_swipeGesture = dx >= dy;
            }
        }
        if (!m_swipeGesture)
            return false;
        setStripOffset(m_pressOffset + int(localPos.x() - m_pressLocalX), false);
        return true;
    case QEvent::TouchEnd:
    case QEvent::MouseButtonRelease:
        if (!m_pointerDown)
            return false;
        if (m_swipeGesture) {
            finishDrag();
            return true;
        }
        m_pointerDown = false;
        m_gestureLocked = false;
        return false;
    default:
        return false;
    }
}

bool SwipeView::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget || !isInSwipePage(widget))
        return QWidget::eventFilter(watched, event);

    const QEvent::Type type = event->type();

    if (type == QEvent::TouchBegin || type == QEvent::TouchUpdate || type == QEvent::TouchEnd) {
        const auto *touch = static_cast<const QTouchEvent *>(event);
        if (touch->touchPoints().isEmpty())
            return false;
        const QPointF local = mapFromGlobal(
            touch->touchPoints().first().screenPos().toPoint());
        return handlePointer(local, type);
    }

    if (type == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        handlePointer(mapFromGlobal(mouse->globalPos()), type);
        return false;
    }

    if (type == QEvent::MouseMove) {
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        return handlePointer(mapFromGlobal(mouse->globalPos()), type);
    }

    if (type == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        return handlePointer(mapFromGlobal(mouse->globalPos()), type);
    }

    return false;
}

bool SwipeView::event(QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type == QEvent::TouchBegin || type == QEvent::TouchUpdate || type == QEvent::TouchEnd) {
        const auto *touch = static_cast<const QTouchEvent *>(event);
        if (touch->touchPoints().isEmpty())
            return QWidget::event(event);
        return handlePointer(touch->touchPoints().first().pos(), type);
    }

    return QWidget::event(event);
}

void SwipeView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutPages();
    if (!m_pointerDown)
        setStripOffset(offsetForIndex(m_currentIndex), false);
}
