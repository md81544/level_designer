#include "levelwidget.h"
#include "level.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace mgo {

namespace {

// Zoom limits: the most zoomed in view shows MIN_VIEW_WIDTH workspace units
// across; the most zoomed out shows MAX_ZOOM workspace units per pixel, or
// the whole map if that needs more
constexpr double MIN_VIEW_WIDTH = 100.0;
constexpr double MAX_ZOOM = 2.0;
// Zoom factor per step for the menu / keyboard
constexpr double ZOOM_STEP = 1.25;
// Zoom factor per mouse wheel notch
constexpr double WHEEL_ZOOM_STEP = 1.1;
// Pixels scrolled per scroll bar arrow click; a mouse wheel notch scrolls three times this
constexpr int SCROLL_STEP = 20;

// The application's style, but with permanently visible scroll bars. On
// macOS, scroll bars are by default "transient" overlays which only appear
// while scrolling, and are almost invisible over our black background.
class PermanentScrollBarStyle final : public QProxyStyle {
public:
    int styleHint(
        StyleHint hint,
        const QStyleOption* option,
        const QWidget* widget,
        QStyleHintReturn* returnData) const override
    {
        if (hint == SH_ScrollBar_Transient) {
            return 0;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

} // namespace

LevelWidget::LevelWidget(Level& level, QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_level(level)
{
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    // Both the scroll area and its scroll bars check whether scroll bars are
    // transient. Setting a widget's style doesn't affect its children.
    auto* scrollBarStyle = new PermanentScrollBarStyle;
    scrollBarStyle->setParent(this);
    setStyle(scrollBarStyle);
    horizontalScrollBar()->setStyle(scrollBarStyle);
    verticalScrollBar()->setStyle(scrollBarStyle);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setMouseTracking(true); // so we get mouse moves without a button pressed
    setMinimumSize(200, 200);
}

void LevelWidget::setChangedCallback(std::function<void()> callback)
{
    m_changedCallback = std::move(callback);
}

void LevelWidget::changed()
{
    viewport()->update();
    if (m_changedCallback) {
        m_changedCallback();
    }
}

QPointF LevelWidget::viewportCentre() const
{
    return { viewport()->width() / 2.0, viewport()->height() / 2.0 };
}

QPointF LevelWidget::toWorkspace(QPointF viewportPos) const
{
    return m_centre + (viewportPos - viewportCentre()) * m_zoom;
}

QPointF LevelWidget::toMap(QPointF viewportPos) const
{
    const QPointF p = toWorkspace(viewportPos);
    return { std::clamp(p.x(), 0.0, static_cast<double>(Level::MAP_SIZE)),
             std::clamp(p.y(), 0.0, static_cast<double>(Level::MAP_SIZE)) };
}

std::optional<QPointF> LevelWidget::cursorPosition() const
{
    if (!m_lastMousePos.has_value()) {
        return std::nullopt;
    }
    return toWorkspace(*m_lastMousePos);
}

// ---------------------------------------------------------------------------
// The view
// ---------------------------------------------------------------------------

void LevelWidget::viewChanged()
{
    clampView();
    updateScrollBars();
    viewport()->update();
}

void LevelWidget::clampView()
{
    const double mapSize = Level::MAP_SIZE;
    const double halfWidth = viewport()->width() * m_zoom / 2.0;
    const double halfHeight = viewport()->height() * m_zoom / 2.0;
    double x = std::max(halfWidth, std::min(m_centre.x(), mapSize - halfWidth));
    double y = std::max(halfHeight, std::min(m_centre.y(), mapSize - halfHeight));
    // If the view is larger than the map in either dimension, centre it
    if (halfWidth * 2 > mapSize) {
        x = mapSize / 2.0;
    }
    if (halfHeight * 2 > mapSize) {
        y = mapSize / 2.0;
    }
    m_centre = { x, y };
}

void LevelWidget::updateScrollBars()
{
    // The scroll bars work in pixels: their range is the map's size in pixels
    // less the viewport's size, and their value is the position of the
    // viewport's left / top edge on the map.
    //
    // Showing or hiding a scroll bar resizes the viewport, which changes the
    // other scroll bar's range (and may in turn show or hide it), so repeat
    // until the viewport's size settles.
    m_updatingScrollBars = true;
    const double mapPixels = Level::MAP_SIZE / m_zoom;
    for (int attempt = 0; attempt < 3; ++attempt) {
        m_resizedWhileUpdatingScrollBars = false;
        clampView();
        auto update = [&](QScrollBar* bar, int viewportSize, double centre) {
            bar->setRange(0, std::max(0, static_cast<int>(std::lround(mapPixels - viewportSize))));
            bar->setPageStep(viewportSize);
            bar->setSingleStep(SCROLL_STEP);
            bar->setValue(static_cast<int>(std::lround(centre / m_zoom - viewportSize / 2.0)));
        };
        update(horizontalScrollBar(), viewport()->width(), m_centre.x());
        update(verticalScrollBar(), viewport()->height(), m_centre.y());
        if (!m_resizedWhileUpdatingScrollBars) {
            break;
        }
    }
    m_updatingScrollBars = false;
}

void LevelWidget::scrollContentsBy(int, int)
{
    if (m_updatingScrollBars) {
        return;
    }
    // The user has moved a scroll bar (directly, or via the wheel etc)
    if (horizontalScrollBar()->maximum() > 0) {
        m_centre.setX((horizontalScrollBar()->value() + viewport()->width() / 2.0) * m_zoom);
    }
    if (verticalScrollBar()->maximum() > 0) {
        m_centre.setY((verticalScrollBar()->value() + viewport()->height() / 2.0) * m_zoom);
    }
    clampView();
    changed();
}

void LevelWidget::zoomBy(double factor, QPointF anchor)
{
    const QPointF anchorOnWorkspace = toWorkspace(anchor);
    // Note the viewport is measured without scroll bars, as they will
    // disappear when the whole map fits
    const QSize size = maximumViewportSize();
    const double minZoom = MIN_VIEW_WIDTH / std::max(size.width(), 1);
    const double maxZoom = std::max(
        MAX_ZOOM, static_cast<double>(Level::MAP_SIZE) / std::max(std::min(size.width(), size.height()), 1));
    m_zoom = std::clamp(m_zoom * factor, minZoom, maxZoom);
    m_centre = anchorOnWorkspace - (anchor - viewportCentre()) * m_zoom;
    viewChanged();
}

void LevelWidget::zoomIn()
{
    zoomBy(1.0 / ZOOM_STEP, viewportCentre());
}

void LevelWidget::zoomOut()
{
    zoomBy(ZOOM_STEP, viewportCentre());
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

bool LevelWidget::viewportEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::NativeGesture:
            {
                // Trackpad pinch (macOS, and some other platforms)
                const auto* gesture = static_cast<QNativeGestureEvent*>(event);
                if (gesture->gestureType() == Qt::ZoomNativeGesture) {
                    // value() is the change in scale since the last event, e.g. 0.01
                    zoomBy(1.0 / (1.0 + gesture->value()), gesture->position());
                    changed();
                    return true;
                }
                break;
            }
        case QEvent::Leave:
            m_lastMousePos = std::nullopt;
            changed();
            break;
        default:
            break;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void LevelWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), Qt::black);
    painter.translate(viewportCentre());
    painter.scale(1.0 / m_zoom, 1.0 / m_zoom);
    painter.translate(-m_centre);
    m_level.draw(painter);
}

void LevelWidget::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    // Keep the top left of the view in the same place (rather than the centre)
    if (event->oldSize().isValid()) {
        const QSizeF change = QSizeF(event->size() - event->oldSize()) / 2.0;
        m_centre += QPointF(change.width(), change.height()) * m_zoom;
    }
    if (m_updatingScrollBars) {
        // A scroll bar has been shown or hidden; updateScrollBars() will go round again
        m_resizedWhileUpdatingScrollBars = true;
    } else {
        viewChanged();
    }
}

void LevelWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        // Pan
        if (m_lastMousePos.has_value()) {
            m_centre += (*m_lastMousePos - pos) * m_zoom;
            viewChanged();
        }
    } else {
        m_level.mouseMoved(toMap(pos));
    }
    m_lastMousePos = pos;
    changed();
}

void LevelWidget::mousePressEvent(QMouseEvent* event)
{
    // Note that on macOS, ControlModifier is the Command key, and Ctrl-click
    // arrives as a right click
    if (event->button() == Qt::RightButton) {
        m_level.finishCurrentLine();
    } else if (event->button() == Qt::LeftButton) {
        m_level.leftClicked(
            toMap(event->position()),
            m_zoom,
            event->modifiers().testFlag(Qt::ControlModifier),
            event->modifiers().testFlag(Qt::AltModifier));
    }
    changed();
}

void LevelWidget::wheelEvent(QWheelEvent* event)
{
    // Note that on macOS, ControlModifier is the Command key and MetaModifier
    // is the Control key; we zoom with either. (Some trackpads, e.g. on
    // Windows, also send pinches as Ctrl + wheel.)
    const auto modifiers = event->modifiers();
    const QPoint angle = event->angleDelta(); // 120 per wheel notch
    if (modifiers & (Qt::ControlModifier | Qt::MetaModifier)) {
        const double notches = (angle.y() != 0 ? angle.y() : angle.x()) / 120.0;
        if (notches != 0.0) {
            zoomBy(std::pow(WHEEL_ZOOM_STEP, -notches), event->position());
            changed();
        }
        event->accept();
        return;
    }

    // Scroll. Trackpads supply a pixel delta; otherwise use the wheel's angle.
    QPoint delta = event->pixelDelta();
    if (delta.isNull()) {
        delta = angle * 3 * SCROLL_STEP / 120;
    }
    // Shift + vertical wheel scrolls horizontally (macOS already does this
    // for mice, so we only get a horizontal delta from it)
    if (modifiers.testFlag(Qt::ShiftModifier) && delta.x() == 0) {
        delta = { delta.y(), 0 };
    }
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
}

void LevelWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            m_level.finishCurrentLine();
            break;
        case Qt::Key_Left:
            m_level.nudgeSelection(-1, 0);
            break;
        case Qt::Key_Right:
            m_level.nudgeSelection(1, 0);
            break;
        case Qt::Key_Up:
            m_level.nudgeSelection(0, -1);
            break;
        case Qt::Key_Down:
            m_level.nudgeSelection(0, 1);
            break;
        default:
            QAbstractScrollArea::keyPressEvent(event);
            return;
    }
    changed();
}

} // namespace mgo
