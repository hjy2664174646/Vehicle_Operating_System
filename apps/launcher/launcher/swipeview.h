#ifndef SWIPEVIEW_H
#define SWIPEVIEW_H

#include <QEvent>
#include <QList>
#include <QPointF>
#include <QWidget>

class QPropertyAnimation;

/**
 * @brief 横向滑动容器（类似手机左右滑屏）
 *
 * 支持触摸屏与鼠标拖拽；松手后带缓动动画吸附到最近一页。
 */
class SwipeView : public QWidget
{
    Q_OBJECT

public:
    explicit SwipeView(QWidget *parent = nullptr);

    void addPage(QWidget *page);
    int pageCount() const;
    int currentIndex() const;

public slots:
    void setCurrentIndex(int index, bool animated = true);

signals:
    void currentIndexChanged(int index);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool handlePointer(const QPointF &localPos, QEvent::Type type);
    void installPointerFilters(QWidget *root);
    bool isInSwipePage(QWidget *widget) const;
    void updatePageStacking();
    void layoutPages();
    void setStripOffset(int pixelOffset, bool animated);
    void finishDrag();
    int offsetForIndex(int index) const;

    QWidget *m_strip = nullptr;
    QList<QWidget *> m_pages;
    int m_currentIndex = 0;
    int m_pixelOffset = 0;
    int m_pressOffset = 0;
    bool m_pointerDown = false;
    bool m_swipeGesture = false;
    bool m_gestureLocked = false;
    qreal m_pressLocalX = 0;
    qreal m_pressLocalY = 0;
    QPropertyAnimation *m_anim = nullptr;
};

#endif // SWIPEVIEW_H
