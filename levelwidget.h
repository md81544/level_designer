#pragma once

// The editing canvas: draws the level, and turns mouse and keyboard input
// into calls on the level. Owns the view onto the workspace, i.e. the
// scroll position and zoom, and shows scroll bars when the view doesn't
// contain the whole map.
//
// The mouse wheel / trackpad scrolls (shift + wheel scrolls horizontally),
// Ctrl / Cmd + wheel and trackpad pinch zoom, and shift + mouse move pans.
//
// In EDIT (select) mode, the selection can be dragged to move it, and has a
// handle which can be dragged to rotate it; dragging from empty space selects
// the items inside the box dragged out.

#include <QAbstractScrollArea>
#include <QPointF>
#include <QRectF>

#include <functional>
#include <optional>

namespace mgo {

class Level;

class LevelWidget final : public QAbstractScrollArea {
public:
    explicit LevelWidget(Level& level, QWidget* parent = nullptr);
    // Called after any input which may have changed the level or its editing state
    void setChangedCallback(std::function<void()> callback);

    void zoomIn();
    void zoomOut();
    // The mouse's position in workspace coordinates, if it's over the canvas
    std::optional<QPointF> cursorPosition() const;

protected:
    // Note that the events below are for the viewport (the area inside the
    // scroll bars), so positions are relative to it
    bool viewportEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    QPointF viewportCentre() const;
    QPointF toWorkspace(QPointF viewportPos) const;
    QPointF toViewport(QPointF workspacePos) const;
    // As toWorkspace(), but limited to the map's area
    QPointF toMap(QPointF viewportPos) const;
    // Zooms by the factor (< 1 zooms in), keeping the anchor (in viewport
    // coordinates) at the same position on the workspace
    void zoomBy(double factor, QPointF anchor);
    // Call after changing m_centre or m_zoom: keeps the view on the map, and
    // updates the scroll bars and display
    void viewChanged();
    void clampView();
    void updateScrollBars();
    void changed();
    // The box drawn around the selection, and its rotation handle (in
    // viewport coordinates), if there's a selection in EDIT mode
    std::optional<QRectF> selectionRect() const;
    std::optional<QPointF> rotationHandle() const;
    bool isOnRotationHandle(QPointF viewportPos) const;
    void updateCursor(QPointF viewportPos);

    Level& m_level;
    std::function<void()> m_changedCallback;
    QPointF m_centre; // workspace coordinates of the centre of the viewport
    double m_zoom { 1.0 }; // workspace units per pixel
    std::optional<QPointF> m_lastMousePos; // viewport coordinates
    // Set while we're setting the scroll bars, so we know their changes
    // aren't the user's
    bool m_updatingScrollBars { false };
    bool m_resizedWhileUpdatingScrollBars { false };

    // Dragging with the left button: moving or rotating the selection, or
    // dragging out a box to select what's inside it
    enum class Drag {
        None,
        Move,
        Rotate,
        Box
    };
    Drag m_drag { Drag::None };
    QPointF m_dragStart; // viewport coordinates
    QPointF m_dragCurrent; // viewport coordinates
    bool m_dragMoved { false }; // has it moved far enough to be a drag?
    bool m_pressedOnSelected { false }; // was the press on an already-selected item?
    bool m_dragSingleLine { false };
    QPointF m_rotationCentre; // workspace coordinates
};

} // namespace mgo
