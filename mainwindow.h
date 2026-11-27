#pragma once

// The application's main window: menus, status bar and the editing canvas,
// plus file handling (new / open / save) and the dialogs.

#include "level.h"

#include <QMainWindow>

#include <map>

class QAction;
class QLabel;
class QMenu;

namespace mgo {

class LevelWidget;

class MainWindow final : public QMainWindow {
public:
    MainWindow(int defaultWidth, int defaultHeight, const QString& fontFamily);

    // Loads a level file, replacing the current level. If the file doesn't
    // exist, it will be created on save. Returns false (having told the user)
    // on failure.
    bool loadFile(const QString& fileName);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createMenus();
    // Updates everything which reflects the level's state, e.g. title, status
    // bar, menu items' enabled / checked states
    void updateUi();

    // If there are unsaved changes, asks the user whether to save them.
    // Returns false if the user cancelled.
    bool maybeSave();
    void newLevel();
    void open();
    void openRecentFile(const QString& fileName);
    // Recently opened / saved files, most recent first, are kept in the settings
    void addRecentFile(const QString& fileName);
    void updateRecentFilesMenu();
    bool save();
    bool saveAs();
    bool saveTo(const QString& fileName);
    void revert();

    void setMode(Mode mode);
    void choosePolygon();
    void editTitle();
    void editIntroduction();
    // Opens the special object properties dialog, with the given field focused
    enum class SpecialField {
        Motion,
        YSpeed,
        Spin,
        Gravity
    };
    void editSpecialObject(SpecialField focusField);
    void showHelp();

    Level m_level;
    LevelWidget* m_canvas;
    QLabel* m_modeLabel;
    QLabel* m_snapLabel;
    QLabel* m_positionLabel;
    QMenu* m_recentFilesMenu { nullptr };

    std::map<Mode, QAction*> m_modeActions;
    std::map<SnapMode, QAction*> m_snapActions;
    QAction* m_revertAction { nullptr };
    QAction* m_undoAction { nullptr };
    QAction* m_redoAction { nullptr };
    QAction* m_deleteAction { nullptr };
    QAction* m_makeSpecialAction { nullptr };
    QAction* m_makeDecorationAction { nullptr };
    QAction* m_makeWallsAction { nullptr };
    QAction* m_specialPropertiesAction { nullptr };
};

} // namespace mgo
