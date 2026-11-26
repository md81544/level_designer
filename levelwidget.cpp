#include "levelwidget.h"
#include "level.h"

#include <QAction>
#include <QCursor>
#include <QKeyEvent>
#include <QMenu>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPolygonF>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

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
// The selection's box and rotation handle (pixels): the box is drawn this far
// outside the selection, and the handle this far above the box
constexpr double SELECTION_MARGIN = 4.0;
constexpr double HANDLE_DISTANCE = 24.0;
constexpr double HANDLE_RADIUS = 6.0;
// How far (pixels) the mouse must move with the button down to start a drag
constexpr int DRAG_THRESHOLD = 3;
// With shift held, rotation is in steps of this many degrees
constexpr double ROTATION_STEP = 15.0;
const QColor SELECTION_BOX_COLOUR { 255, 255, 255, 160 };
const QColor HANDLE_COLOUR { 80, 160, 255 };
// Half the size of the square handles on the selected lines' vertices (pixels)
constexpr double VERTEX_HANDLE_HALF_SIZE = 3.0;

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

void LevelWidget::setContextMenuActions(QList<QAction*> actions)
{
    m_contextMenuActions = std::move(actions);
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

QPointF LevelWidget::toViewport(QPointF workspacePos) const
{
    return (workspacePos - m_centre) / m_zoom + viewportCentre();
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

    // Selection box, rotation handle etc are drawn in viewport coordinates,
    // so they're the same size whatever the zoom
    painter.resetTransform();
    painter.setRenderHint(QPainter::Antialiasing);
    QPen dashed(SELECTION_BOX_COLOUR, 1.0, Qt::DashLine);
    if (m_level.mode() == Mode::EDIT && m_drag != Drag::Rotate && m_drag != Drag::Move) {
        painter.setPen(QPen(Qt::white, 1.0));
        painter.setBrush(HANDLE_COLOUR);
        const QPointF half(VERTEX_HANDLE_HALF_SIZE, VERTEX_HANDLE_HALF_SIZE);
        for (const QPointF& vertex : m_level.selectedLineVertices()) {
            const QPointF p = toViewport(vertex);
            painter.drawRect(QRectF(p - half, p + half));
        }
        // Path points are diamonds, to tell them apart from vertices
        for (const QPointF& point : m_level.selectedPathPoints()) {
            const QPointF p = toViewport(point);
            const double r = VERTEX_HANDLE_HALF_SIZE + 1;
            painter.drawPolygon(QPolygonF(
                { p + QPointF(0, -r), p + QPointF(r, 0), p + QPointF(0, r), p + QPointF(-r, 0) }));
        }
    }
    if (m_contextLine.has_value()) {
        painter.setPen(QPen(HANDLE_COLOUR, 3.0));
        painter.drawLine(toViewport(m_contextLine->p1()), toViewport(m_contextLine->p2()));
    }
    if (m_contextVertex.has_value()) {
        painter.setPen(QPen(HANDLE_COLOUR, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(toViewport(*m_contextVertex), 6.0, 6.0);
    }
    if (m_drag == Drag::Box && m_dragMoved) {
        painter.setPen(dashed);
        painter.setBrush(QColor(255, 255, 255, 30));
        painter.drawRect(QRectF(m_dragStart, m_dragCurrent).normalized());
    }
    if (m_drag == Drag::Rotate && m_dragMoved) {
        // From the centre of rotation to the mouse
        const QPointF centre = toViewport(m_rotationCentre);
        painter.setPen(dashed);
        painter.drawLine(centre, m_dragCurrent);
        painter.setPen(QPen(HANDLE_COLOUR, 1.5));
        painter.drawLine(centre - QPointF(5, 0), centre + QPointF(5, 0));
        painter.drawLine(centre - QPointF(0, 5), centre + QPointF(0, 5));
    } else if (const auto box = selectionRect()) {
        painter.setPen(dashed);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(*box);
        const QPointF handle = *rotationHandle();
        painter.drawLine(QPointF(box->center().x(), box->top()), handle);
        painter.setPen(QPen(Qt::white, 1.0));
        painter.setBrush(HANDLE_COLOUR);
        painter.drawEllipse(handle, HANDLE_RADIUS, HANDLE_RADIUS);
    }
}

std::optional<QRectF> LevelWidget::selectionRect() const
{
    if (m_level.mode() != Mode::EDIT) {
        return std::nullopt;
    }
    const auto bounds = m_level.selectionBounds();
    if (!bounds.has_value()) {
        return std::nullopt;
    }
    return QRectF(toViewport(bounds->topLeft()), toViewport(bounds->bottomRight()))
        .adjusted(-SELECTION_MARGIN, -SELECTION_MARGIN, SELECTION_MARGIN, SELECTION_MARGIN);
}

std::optional<QPointF> LevelWidget::rotationHandle() const
{
    const auto box = selectionRect();
    if (!box.has_value()) {
        return std::nullopt;
    }
    return QPointF(box->center().x(), box->top() - HANDLE_DISTANCE);
}

bool LevelWidget::isOnRotationHandle(QPointF viewportPos) const
{
    const auto handle = rotationHandle();
    return handle.has_value() && QLineF(*handle, viewportPos).length() <= HANDLE_RADIUS + 3;
}

void LevelWidget::updateCursor(QPointF viewportPos)
{
    // Inserting uses a crosshair, to show where things will be placed
    Qt::CursorShape shape = m_level.mode() == Mode::EDIT ? Qt::ArrowCursor : Qt::CrossCursor;
    if (m_drag == Drag::Move) {
        shape = Qt::ClosedHandCursor;
    } else if (
        m_drag == Drag::Vertex || m_drag == Drag::PathPoint
        || (m_drag == Drag::None && m_level.mode() == Mode::EDIT
            && (m_level.vertexAt(toMap(viewportPos), m_zoom, true).has_value()
                || m_level.pathPointAt(toMap(viewportPos), m_zoom).has_value()))) {
        shape = Qt::SizeAllCursor;
    } else if (m_drag == Drag::Rotate || (m_drag == Drag::None && isOnRotationHandle(viewportPos))) {
        shape = Qt::PointingHandCursor;
    } else if (
        m_drag == Drag::None && m_level.mode() == Mode::EDIT
        && m_level.isSelectedItemAt(toMap(viewportPos), m_zoom)) {
        shape = Qt::OpenHandCursor;
    }
    viewport()->setCursor(shape);
}

void LevelWidget::refreshCursor()
{
    updateCursor(viewport()->mapFromGlobal(QCursor::pos()));
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
    if (m_drag != Drag::None && event->buttons().testFlag(Qt::LeftButton)) {
        m_dragCurrent = pos;
        m_lastMousePos = pos;
        if (!m_dragMoved && (pos - m_dragStart).manhattanLength() < DRAG_THRESHOLD) {
            return; // not a drag yet: ignore small movements during a click
        }
        m_dragMoved = true;
        if (m_drag == Drag::Move) {
            const QPointF delta = (pos - m_dragStart) * m_zoom;
            m_level.moveSelection(
                static_cast<int>(std::lround(delta.x())), static_cast<int>(std::lround(delta.y())));
        } else if (m_drag == Drag::Rotate) {
            const QPointF centre = toViewport(m_rotationCentre);
            auto angle = [&centre](QPointF p) {
                return std::atan2(p.y() - centre.y(), p.x() - centre.x()) * 180.0 / std::numbers::pi;
            };
            double degrees = angle(pos) - angle(m_dragStart);
            if (event->modifiers().testFlag(Qt::ShiftModifier)) {
                degrees = std::round(degrees / ROTATION_STEP) * ROTATION_STEP;
            }
            m_level.rotateSelection(degrees, m_rotationCentre);
        } else if (m_drag == Drag::Vertex) {
            m_level.moveVertex(toMap(pos));
        } else if (m_drag == Drag::PathPoint) {
            m_level.movePathPoint(toMap(pos));
        }
        changed();
        return;
    }
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
    updateCursor(pos);
    changed();
}

void LevelWidget::mousePressEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    // Note that on macOS, ControlModifier is the Command key, and Ctrl-click
    // arrives as a right click
    const bool extendSelection = event->modifiers().testFlag(Qt::ControlModifier);
    const bool singleLine = event->modifiers().testFlag(Qt::AltModifier);
    if (event->button() == Qt::RightButton) {
        if (m_drag != Drag::None) {
            return; // ignore it during a drag
        }
        if (m_level.isDrawing()) {
            m_level.finishCurrentLine();
        } else {
            showContextMenu(pos);
            return;
        }
    } else if (event->button() == Qt::LeftButton) {
        const QPointF mapPos = toMap(pos);
        m_drag = Drag::None;
        m_dragStart = m_dragCurrent = pos;
        m_dragMoved = false;
        m_pressedOnSelected = false;
        m_dragSingleLine = singleLine;
        if (m_level.mode() == Mode::EDIT) {
            if (isOnRotationHandle(pos)) {
                m_drag = Drag::Rotate;
                m_rotationCentre = m_level.selectionBounds()->center();
            } else if (const auto point = m_level.pathPointAt(mapPos, m_zoom);
                       point.has_value() && !extendSelection) {
                m_drag = Drag::PathPoint;
                m_level.beginPathPointDrag(*point);
            } else if (const auto vertex = m_level.vertexAt(mapPos, m_zoom, true);
                       vertex.has_value() && !extendSelection) {
                m_drag = Drag::Vertex;
                m_level.beginVertexDrag(*vertex);
            } else if (!extendSelection && m_level.isSelectedItemAt(mapPos, m_zoom)) {
                // Drag the selection, or if there's no drag, select just this item
                m_drag = Drag::Move;
                m_pressedOnSelected = true;
            } else {
                m_level.leftClicked(mapPos, m_zoom, extendSelection, singleLine);
                if (m_level.isSelectedItemAt(mapPos, m_zoom)) {
                    m_drag = Drag::Move;
                } else if (!m_level.isItemAt(mapPos, m_zoom)) {
                    m_drag = Drag::Box;
                }
            }
        } else {
            m_level.leftClicked(mapPos, m_zoom, extendSelection, singleLine);
            // Placing the ship or exit selects it, so it can be dragged straight away
            if (m_level.mode() == Mode::EDIT && m_level.isSelectedItemAt(mapPos, m_zoom)) {
                m_drag = Drag::Move;
            }
        }
        if (m_drag == Drag::Move || m_drag == Drag::Rotate) {
            m_level.beginTransform();
        }
        updateCursor(pos);
    }
    changed();
}

void LevelWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_drag == Drag::None) {
        return;
    }
    const QPointF pos = event->position();
    switch (m_drag) {
        case Drag::Move:
        case Drag::Rotate:
        case Drag::Vertex:
        case Drag::PathPoint:
            m_level.endTransform();
            if (m_drag == Drag::Move && !m_dragMoved && m_pressedOnSelected) {
                m_level.selectOnlyItemAt(toMap(pos), m_zoom, m_dragSingleLine);
            }
            break;
        case Drag::Box:
            if (m_dragMoved) {
                m_level.selectInRect(
                    QRectF(toWorkspace(m_dragStart), toWorkspace(pos)).normalized(),
                    event->modifiers().testFlag(Qt::ControlModifier));
            }
            break;
        case Drag::None:
            break;
    }
    m_drag = Drag::None;
    updateCursor(pos);
    changed();
}

void LevelWidget::showContextMenu(QPointF pos)
{
    const QPointF mapPos = toMap(pos);
    // As with a click, right-clicking something not selected selects it.
    // Right-clicking empty space keeps the selection, so it can be acted on
    // there (e.g. moving a path's end to where was clicked).
    const auto pathPoint = m_level.mode() == Mode::EDIT ? m_level.pathPointAt(mapPos, m_zoom)
                                                        : std::nullopt;
    if (m_level.mode() == Mode::EDIT && m_level.isItemAt(mapPos, m_zoom)
        && !m_level.isSelectedItemAt(mapPos, m_zoom)
        && !m_level.vertexAt(mapPos, m_zoom, true).has_value() && !pathPoint.has_value()) {
        m_level.leftClicked(mapPos, m_zoom, false, false);
    }
    // Brings the main window's actions up to date with the selection
    changed();

    QMenu menu(this);
    // A vertex takes precedence over the lines meeting there
    m_contextVertex = m_level.vertexAt(mapPos, m_zoom, false);
    if (m_contextVertex.has_value()) {
        QAction* join
            = menu.addAction("Join Lines Here", [&] { m_level.joinLinesAt(mapPos, m_zoom); });
        join->setEnabled(m_level.canJoinLinesAt(mapPos, m_zoom));
    } else if ((m_contextLine = m_level.lineAt(mapPos, m_zoom))) {
        menu.addAction("Delete Line", [&] { m_level.deleteLineAt(mapPos, m_zoom); });
        QAction* split
            = menu.addAction("Split Line Here", [&] { m_level.splitLineAt(mapPos, m_zoom); });
        split->setEnabled(m_level.canSplitLineAt(mapPos, m_zoom));
        menu.addSeparator();
        menu.addAction("Select Line", [&] { m_level.selectLineAt(mapPos, m_zoom, false); });
        menu.addAction(
            "Select Connected Lines", [&] { m_level.selectLineAt(mapPos, m_zoom, true); });
    }
    // The selected moving object's path
    if (const auto moving = m_level.selectedMovingObject()) {
        menu.addSeparator();
        if (pathPoint.has_value()) {
            menu.addAction("Remove Path Point", [&] { m_level.removePathPoint(*pathPoint); });
        } else {
            menu.addAction(
                moving->pathOffsets.empty() ? "Move Here and Back" : "Move Path End Here",
                [&] { m_level.setPathEndAt(mapPos); });
            QAction* add
                = menu.addAction("Add Path Point Here", [&] { m_level.addPathPointAt(mapPos); });
            add->setEnabled(!moving->pathOffsets.empty());
        }
    }
    // Placing items where clicked
    menu.addSeparator();
    menu.addAction("Place Start Position Here", [&] { m_level.placeShip(mapPos); });
    menu.addAction("Place Exit Here", [&] { m_level.placeExit(mapPos); });
    menu.addAction("Add Fuel Here", [&] { m_level.addFuel(mapPos); });
    if (!m_contextMenuActions.isEmpty()) {
        menu.addSeparator();
        menu.addActions(m_contextMenuActions);
    }
    viewport()->update();
    menu.exec(viewport()->mapToGlobal(pos.toPoint()));
    m_contextLine = std::nullopt;
    m_contextVertex = std::nullopt;
    updateCursor(pos);
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
            if (m_level.mode() == Mode::EDIT) {
                m_level.clearSelection();
            } else {
                // Finishes anything being drawn, and goes back to selecting
                m_level.changeMode(Mode::EDIT);
            }
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
