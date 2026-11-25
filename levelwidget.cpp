#include "levelwidget.h"
#include "level.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace mgo {

namespace {

// Zoom limits: the most zoomed in view shows MIN_VIEW_WIDTH workspace units
// across, the most zoomed out shows MAX_ZOOM workspace units per pixel
constexpr double MIN_VIEW_WIDTH = 100.0;
constexpr double MAX_ZOOM = 2.0;
// Zoom factor per step for the menu / keyboard
constexpr double ZOOM_STEP = 1.25;

} // namespace

LevelWidget::LevelWidget(Level& level, QWidget* parent)
    : QWidget(parent)
    , m_level(level)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // so we get mouse moves without a button pressed
    setMinimumSize(200, 200);
}

void LevelWidget::setChangedCallback(std::function<void()> callback)
{
    m_changedCallback = std::move(callback);
}

void LevelWidget::changed()
{
    update();
    if (m_changedCallback) {
        m_changedCallback();
    }
}

QPointF LevelWidget::toWorkspace(QPointF widgetPos) const
{
    return m_centre + (widgetPos - QPointF(width() / 2.0, height() / 2.0)) * m_zoom;
}

QPointF LevelWidget::toMap(QPointF widgetPos) const
{
    const QPointF p = toWorkspace(widgetPos);
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

void LevelWidget::clampView()
{
    const double mapSize = Level::MAP_SIZE;
    const double halfWidth = width() * m_zoom / 2.0;
    const double halfHeight = height() * m_zoom / 2.0;
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

void LevelWidget::zoomBy(double factor, QPointF anchor)
{
    const QPointF anchorOnWorkspace = toWorkspace(anchor);
    const double minZoom = MIN_VIEW_WIDTH / std::max(width(), 1);
    m_zoom = std::clamp(m_zoom * factor, minZoom, MAX_ZOOM);
    m_centre = anchorOnWorkspace - (anchor - QPointF(width() / 2.0, height() / 2.0)) * m_zoom;
    clampView();
    update();
}

void LevelWidget::zoomIn()
{
    zoomBy(1.0 / ZOOM_STEP, rect().center());
}

void LevelWidget::zoomOut()
{
    zoomBy(ZOOM_STEP, rect().center());
}

void LevelWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(1.0 / m_zoom, 1.0 / m_zoom);
    painter.translate(-m_centre);
    m_level.draw(painter);
}

void LevelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    clampView();
}

void LevelWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        // Pan
        if (m_lastMousePos.has_value()) {
            m_centre += (*m_lastMousePos - pos) * m_zoom;
            clampView();
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
    // angleDelta is in eighths of a degree; a typical mouse wheel notch is 15
    // degrees, i.e. 120. Trackpads send many smaller deltas.
    const double notches = event->angleDelta().y() / 120.0;
    if (notches != 0.0) {
        zoomBy(std::pow(1.05, -notches), event->position());
        changed();
    }
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
            QWidget::keyPressEvent(event);
            return;
    }
    changed();
}

void LevelWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_lastMousePos = std::nullopt;
    changed();
}

} // namespace mgo
