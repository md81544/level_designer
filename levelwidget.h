#pragma once

// The editing canvas: draws the level, and turns mouse and keyboard input
// into calls on the level. Owns the view onto the workspace, i.e. the
// scroll position and zoom.
//
// Shift + mouse move pans the view, the mouse wheel / trackpad zooms.

#include <QPointF>
#include <QWidget>

#include <functional>
#include <optional>

namespace mgo {

class Level;

class LevelWidget final : public QWidget {
public:
    explicit LevelWidget(Level& level, QWidget* parent = nullptr);
    // Called after any input which may have changed the level or its editing state
    void setChangedCallback(std::function<void()> callback);

    void zoomIn();
    void zoomOut();
    // The mouse's position in workspace coordinates, if it's over the canvas
    std::optional<QPointF> cursorPosition() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPointF toWorkspace(QPointF widgetPos) const;
    // As toWorkspace(), but limited to the map's area
    QPointF toMap(QPointF widgetPos) const;
    // Zooms by the factor (< 1 zooms in), keeping the anchor (in widget
    // coordinates) at the same position on the workspace
    void zoomBy(double factor, QPointF anchor);
    void clampView();
    void changed();

    Level& m_level;
    std::function<void()> m_changedCallback;
    QPointF m_centre; // workspace coordinates of the centre of the widget
    double m_zoom { 1.0 }; // workspace units per pixel
    std::optional<QPointF> m_lastMousePos; // widget coordinates
};

} // namespace mgo
