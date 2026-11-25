#include "mainwindow.h"
#include "levelwidget.h"
#include "log.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>

#include <array>
#include <exception>

namespace mgo {

namespace {

constexpr const char* WINDOW_GEOMETRY = "window/geometry";
constexpr const char* LAST_DIRECTORY = "files/lastDirectory";

#ifdef Q_OS_MACOS
constexpr const char* CMD = "Cmd";
constexpr const char* ALT = "Option";
#else
constexpr const char* CMD = "Ctrl";
constexpr const char* ALT = "Alt";
#endif

QString modeName(Mode mode)
{
    switch (mode) {
        case Mode::LINE:
            return "Line";
        case Mode::BREAKABLE:
            return "Breakable Line";
        case Mode::EDIT:
            return "Select";
        case Mode::START:
            return "Start Position";
        case Mode::EXIT:
            return "Exit";
        case Mode::FUEL:
            return "Fuel";
        case Mode::MOVING:
            return "Moving Object";
        case Mode::POLYGON_CENTRE:
        case Mode::POLYGON_RADIUS:
            return "Polygon";
    }
    return {};
}

// A short reminder of how to use the current mode
QString modeHint(Mode mode)
{
    switch (mode) {
        case Mode::LINE:
        case Mode::BREAKABLE:
            return "Click to start a line, then click to add each point. Right-click or Esc to "
                   "finish.";
        case Mode::MOVING:
            return "Click to start a line, then click to add each point. Right-click or Esc "
                   "finishes the object.";
        case Mode::EDIT:
            return QString("Click to select (%1-click adds to the selection, %2-click selects a "
                           "single line). Arrow keys nudge.")
                .arg(CMD, ALT);
        case Mode::START:
            return "Click to place the ship; click the ship again to rotate it.";
        case Mode::EXIT:
            return "Click to place the exit.";
        case Mode::FUEL:
            return "Click to place fuel; click existing fuel to remove it.";
        case Mode::POLYGON_CENTRE:
            return "Click to place the polygon's centre.";
        case Mode::POLYGON_RADIUS:
            return "Move to size and rotate the polygon, then click to place it.";
    }
    return {};
}

QString snapName(SnapMode snapMode)
{
    switch (snapMode) {
        case SnapMode::AUTO:
            return "Auto";
        case SnapMode::GRID:
            return "Grid";
        case SnapMode::LINE:
            return "Line";
        case SnapMode::NONE:
            return "None";
    }
    return {};
}

} // namespace

MainWindow::MainWindow(int defaultWidth, int defaultHeight, const QString& fontFamily)
{
    m_canvas = new LevelWidget(m_level, this);
    if (!fontFamily.isEmpty()) {
        m_canvas->setFont(QFont(fontFamily));
    }
    setCentralWidget(m_canvas);
    m_canvas->setChangedCallback([this] { updateUi(); });
    m_canvas->setFocus();

    m_modeLabel = new QLabel(this);
    m_snapLabel = new QLabel(this);
    m_positionLabel = new QLabel(this);
    m_positionLabel->setMinimumWidth(m_positionLabel->fontMetrics().horizontalAdvance("0000, 0000"));
    statusBar()->addPermanentWidget(m_modeLabel);
    statusBar()->addPermanentWidget(m_snapLabel);
    statusBar()->addPermanentWidget(m_positionLabel);

    createMenus();

    QSettings settings;
    if (!restoreGeometry(settings.value(WINDOW_GEOMETRY).toByteArray())) {
        resize(defaultWidth, defaultHeight);
    }
    updateUi();
}

void MainWindow::createMenus()
{
    // Adds an action to the menu, calling fn when triggered
    auto add = [this](
                   QMenu* menu,
                   const QString& text,
                   QList<QKeySequence> shortcuts,
                   std::function<void()> fn) {
        QAction* action = menu->addAction(text);
        action->setShortcuts(shortcuts);
        // By default, on macOS Qt guesses from their text which actions belong
        // in the application menu, e.g. it would make Insert > Exit the Quit
        // item. Roles are set explicitly where needed instead.
        action->setMenuRole(QAction::NoRole);
        connect(action, &QAction::triggered, this, [this, fn = std::move(fn)] {
            fn();
            m_canvas->update();
            updateUi();
        });
        return action;
    };

    // File
    QMenu* fileMenu = menuBar()->addMenu("&File");
    add(fileMenu, "&New", { QKeySequence::New }, [this] { newLevel(); });
    add(fileMenu, "&Open...", { QKeySequence::Open }, [this] { open(); });
    fileMenu->addSeparator();
    add(fileMenu, "&Save", { QKeySequence::Save }, [this] { save(); });
    add(fileMenu, "Save &As...", { QKeySequence::SaveAs }, [this] { saveAs(); });
    m_revertAction = add(fileMenu, "&Revert to Saved", {}, [this] { revert(); });
    fileMenu->addSeparator();
    QAction* quitAction = add(fileMenu, "&Quit", { QKeySequence::Quit }, [this] { close(); });
    quitAction->setMenuRole(QAction::QuitRole);

    // Edit
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    add(editMenu, "&Undo", { QKeySequence::Undo }, [this] { m_level.undo(); });
    add(editMenu,
        "&Redo",
        { QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::Key_Y) },
        [this] { m_level.redo(); });
    editMenu->addSeparator();
    m_deleteAction = add(
        editMenu,
        "&Delete",
        { QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) },
        [this] { m_level.deleteSelection(); });
    editMenu->addSeparator();
    add(editMenu, "Level &Title...", { QKeySequence(Qt::Key_T) }, [this] { editTitle(); });

    // Insert (and select): these are the editing modes
    auto* modeGroup = new QActionGroup(this);
    auto addMode = [&](QMenu* menu, const QString& text, QKeySequence shortcut, Mode mode) {
        QAction* action = add(menu, text, { shortcut }, [this, mode] { setMode(mode); });
        action->setCheckable(true);
        modeGroup->addAction(action);
        m_modeActions[mode] = action;
    };
    editMenu->addSeparator();
    addMode(editMenu, "&Select", QKeySequence(Qt::SHIFT | Qt::Key_E), Mode::EDIT);

    QMenu* insertMenu = menuBar()->addMenu("&Insert");
    addMode(insertMenu, "&Line", QKeySequence(Qt::SHIFT | Qt::Key_L), Mode::LINE);
    addMode(insertMenu, "&Breakable Line", QKeySequence(Qt::SHIFT | Qt::Key_B), Mode::BREAKABLE);
    addMode(insertMenu, "&Moving Object", QKeySequence(Qt::SHIFT | Qt::Key_V), Mode::MOVING);
    // Polygon asks for the number of sides first, so isn't a plain mode change
    QAction* polygonAction = add(
        insertMenu,
        "&Polygon...",
        { QKeySequence(Qt::SHIFT | Qt::Key_P) },
        [this] { choosePolygon(); });
    polygonAction->setCheckable(true);
    modeGroup->addAction(polygonAction);
    m_modeActions[Mode::POLYGON_CENTRE] = polygonAction;
    insertMenu->addSeparator();
    addMode(insertMenu, "&Fuel", QKeySequence(Qt::SHIFT | Qt::Key_F), Mode::FUEL);
    addMode(insertMenu, "&Start Position", QKeySequence(Qt::SHIFT | Qt::Key_S), Mode::START);
    addMode(insertMenu, "E&xit", QKeySequence(Qt::SHIFT | Qt::Key_X), Mode::EXIT);

    // Object: operations on the selection
    QMenu* objectMenu = menuBar()->addMenu("&Object");
    m_makeMovingAction = add(
        objectMenu,
        "&Make Moving Object from Selection",
        { QKeySequence(Qt::Key_V) },
        [this] { m_level.convertSelectionToMovingObject(); });
    m_movingPropertiesAction = add(
        objectMenu,
        "Moving Object &Properties...",
        { QKeySequence(Qt::Key_X) },
        [this] { editMovingObject(0); });
    // Old shortcuts for editing individual properties, which open the same
    // dialog at the relevant field
    for (auto [key, field] : std::array<std::pair<Qt::Key, int>, 3> {
             { { Qt::Key_Y, 2 }, { Qt::Key_R, 4 }, { Qt::Key_G, 5 } } }) {
        auto* action = new QAction(this);
        action->setShortcut(QKeySequence(key));
        connect(action, &QAction::triggered, this, [this, field] {
            editMovingObject(field);
            updateUi();
        });
        addAction(action);
    }

    // View
    QMenu* viewMenu = menuBar()->addMenu("&View");
    add(viewMenu,
        "Zoom &In",
        { QKeySequence::ZoomIn, QKeySequence(Qt::Key_Equal) },
        [this] { m_canvas->zoomIn(); });
    add(viewMenu,
        "Zoom &Out",
        { QKeySequence::ZoomOut, QKeySequence(Qt::Key_Minus) },
        [this] { m_canvas->zoomOut(); });
    viewMenu->addSeparator();
    QMenu* snapMenu = viewMenu->addMenu("&Snap To");
    auto* snapGroup = new QActionGroup(this);
    for (SnapMode snapMode : { SnapMode::AUTO, SnapMode::GRID, SnapMode::LINE, SnapMode::NONE }) {
        QString text = snapMode == SnapMode::AUTO ? "Grid and Lines" : snapName(snapMode);
        QAction* action
            = add(snapMenu, text, {}, [this, snapMode] { m_level.setSnapMode(snapMode); });
        action->setCheckable(true);
        snapGroup->addAction(action);
        m_snapActions[snapMode] = action;
    }
    snapMenu->addSeparator();
    add(snapMenu, "&Next Snap Mode", { QKeySequence(Qt::Key_S) }, [this] {
        m_level.cycleSnapMode();
    });

    // Help
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    add(helpMenu, "&Mouse and Keyboard", {}, [this] { showHelp(); });
}

void MainWindow::updateUi()
{
    const std::string& fileName = m_level.fileName();
    QString title = fileName.empty() ? "Untitled" : QFileInfo(QString::fromStdString(fileName)).fileName();
    title += "[*]";
    if (!m_level.description().empty()) {
        title += " - " + QString::fromStdString(m_level.description());
    }
    setWindowTitle(title);
    setWindowFilePath(QString::fromStdString(fileName));
    setWindowModified(m_level.isDirty());

    const Mode mode = m_level.mode() == Mode::POLYGON_RADIUS ? Mode::POLYGON_CENTRE : m_level.mode();
    m_modeActions.at(mode)->setChecked(true);
    m_snapActions.at(m_level.snapMode())->setChecked(true);

    statusBar()->showMessage(modeHint(m_level.mode()));
    m_modeLabel->setText("Mode: " + modeName(m_level.mode()));
    m_snapLabel->setText("Snap: " + snapName(m_level.snapMode()));
    if (auto pos = m_canvas->cursorPosition()) {
        m_positionLabel->setText(QString("%1, %2").arg(qRound(pos->x())).arg(qRound(pos->y())));
    } else {
        m_positionLabel->clear();
    }

    m_revertAction->setEnabled(
        m_level.isDirty() && QFileInfo::exists(QString::fromStdString(fileName)));
    m_deleteAction->setEnabled(m_level.hasSelection());
    m_makeMovingAction->setEnabled(m_level.hasSelectedLines());
    m_movingPropertiesAction->setEnabled(m_level.selectedMovingObject().has_value());
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

bool MainWindow::loadFile(const QString& fileName)
{
    const QString path = QFileInfo(fileName).absoluteFilePath();
    Level level;
    try {
        level.load(path.toStdString());
    } catch (const std::exception& e) {
        Log::error("Failed to load {}: {}", path.toStdString(), e.what());
        QMessageBox::warning(
            this, "Open Level", QString("Could not open %1:\n%2").arg(path, e.what()));
        return false;
    }
    Log::info("Loaded {}", path.toStdString());
    // Keep the user's editing preferences
    level.setSnapMode(m_level.snapMode());
    level.setPolygonSides(m_level.polygonSides());
    level.changeMode(m_level.mode() == Mode::POLYGON_RADIUS ? Mode::POLYGON_CENTRE : m_level.mode());
    m_level = std::move(level);
    QSettings().setValue(LAST_DIRECTORY, QFileInfo(path).absolutePath());
    m_canvas->update();
    updateUi();
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_level.isDirty()) {
        return true;
    }
    const auto answer = QMessageBox::warning(
        this,
        "Unsaved Changes",
        "The level has unsaved changes. Do you want to save them?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    switch (answer) {
        case QMessageBox::Save:
            return save();
        case QMessageBox::Discard:
            return true;
        default:
            return false;
    }
}

void MainWindow::newLevel()
{
    if (!maybeSave()) {
        return;
    }
    Level level;
    level.setSnapMode(m_level.snapMode());
    level.setPolygonSides(m_level.polygonSides());
    m_level = std::move(level);
}

void MainWindow::open()
{
    if (!maybeSave()) {
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Level",
        QSettings().value(LAST_DIRECTORY, QDir::homePath()).toString(),
        "Level files (*.lvl);;All files (*)");
    if (!fileName.isEmpty()) {
        loadFile(fileName);
    }
}

bool MainWindow::save()
{
    if (m_level.fileName().empty()) {
        return saveAs();
    }
    return saveTo(QString::fromStdString(m_level.fileName()));
}

bool MainWindow::saveAs()
{
    QString dir = QSettings().value(LAST_DIRECTORY, QDir::homePath()).toString();
    if (!m_level.fileName().empty()) {
        dir = QString::fromStdString(m_level.fileName());
    }
    QString fileName
        = QFileDialog::getSaveFileName(this, "Save Level", dir, "Level files (*.lvl)");
    if (fileName.isEmpty()) {
        return false;
    }
    if (QFileInfo(fileName).suffix().isEmpty()) {
        fileName += ".lvl";
    }
    return saveTo(fileName);
}

bool MainWindow::saveTo(const QString& fileName)
{
    try {
        m_level.save(fileName.toStdString());
    } catch (const std::exception& e) {
        Log::error("Failed to save {}: {}", fileName.toStdString(), e.what());
        QMessageBox::warning(
            this, "Save Level", QString("Could not save %1:\n%2").arg(fileName, e.what()));
        return false;
    }
    Log::info("Saved {}", fileName.toStdString());
    QSettings().setValue(LAST_DIRECTORY, QFileInfo(fileName).absolutePath());
    statusBar()->showMessage("Saved " + fileName, 3000);
    updateUi();
    return true;
}

void MainWindow::revert()
{
    const auto answer = QMessageBox::question(
        this,
        "Revert to Saved",
        "Discard all changes since the level was last saved?",
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Discard) {
        loadFile(QString::fromStdString(m_level.fileName()));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    QSettings().setValue(WINDOW_GEOMETRY, saveGeometry());
    event->accept();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void MainWindow::setMode(Mode mode)
{
    m_level.changeMode(mode);
}

void MainWindow::choosePolygon()
{
    bool ok = false;
    const int sides = QInputDialog::getInt(
        this, "Insert Polygon", "Number of sides:", m_level.polygonSides(), 3, 64, 1, &ok);
    if (ok) {
        m_level.setPolygonSides(sides);
        m_level.changeMode(Mode::POLYGON_CENTRE);
    }
}

void MainWindow::editTitle()
{
    bool ok = false;
    QString title = QInputDialog::getText(
        this,
        "Level Title",
        "Title:",
        QLineEdit::Normal,
        QString::fromStdString(m_level.description()),
        &ok);
    if (ok) {
        // '~' separates fields in the level file
        title.replace('~', '-');
        m_level.setDescription(title.toStdString());
    }
}

void MainWindow::editMovingObject(int focusField)
{
    auto object = m_level.selectedMovingObject();
    if (!object.has_value()) {
        return;
    }
    struct Field {
        const char* label;
        float MovingObject::* member;
        double min;
        double max;
    };
    const std::array<Field, 6> fields { {
        { "X delta:", &MovingObject::xDelta, -1000, 1000 },
        { "X max +/- motion (grid square = 50):", &MovingObject::xMaxDifference, 0, 2000 },
        { "Y delta:", &MovingObject::yDelta, -1000, 1000 },
        { "Y max +/- motion (grid square = 50):", &MovingObject::yMaxDifference, 0, 2000 },
        { "Rotation delta:", &MovingObject::rotationDelta, -360, 360 },
        { "Gravity (10 to 100 is good):", &MovingObject::gravity, -1000, 1000 },
    } };

    QDialog dialog(this);
    dialog.setWindowTitle("Moving Object Properties");
    auto* form = new QFormLayout(&dialog);
    std::array<QDoubleSpinBox*, fields.size()> spinBoxes;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        auto* spinBox = new QDoubleSpinBox(&dialog);
        spinBox->setRange(fields[i].min, fields[i].max);
        spinBox->setDecimals(2);
        spinBox->setValue((*object).*fields[i].member);
        form->addRow(fields[i].label, spinBox);
        spinBoxes[i] = spinBox;
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (focusField >= 0 && focusField < static_cast<int>(spinBoxes.size())) {
        spinBoxes[focusField]->setFocus();
        spinBoxes[focusField]->selectAll();
    }

    if (dialog.exec() == QDialog::Accepted) {
        for (std::size_t i = 0; i < fields.size(); ++i) {
            (*object).*fields[i].member = static_cast<float>(spinBoxes[i]->value());
        }
        m_level.setSelectedMovingObjectMotion(*object);
        m_canvas->update();
    }
}

void MainWindow::showHelp()
{
    QMessageBox::information(
        this,
        "Mouse and Keyboard",
        QString("<p>Choose what to insert from the <b>Insert</b> menu, or <b>Edit &gt; "
                "Select</b> to select existing items. The status bar shows how to use the "
                "current mode.</p>"
                "<ul>"
                "<li><b>Mouse wheel / trackpad</b>: scroll (<b>Shift + wheel</b> scrolls "
                "sideways)</li>"
                "<li><b>%1 + wheel</b> or <b>pinch</b>: zoom</li>"
                "<li><b>Shift + move mouse</b>: scroll the view</li>"
                "<li><b>Right-click / Esc</b>: stop drawing the current line</li>"
                "<li><b>%1-click</b>: add to the selection</li>"
                "<li><b>%2-click</b>: select a single line rather than all those "
                "connected to it</li>"
                "<li><b>Arrow keys</b>: nudge the selection</li>"
                "</ul>"
                "<p>Other keyboard shortcuts are shown in the menus.</p>")
            .arg(CMD, ALT));
}

} // namespace mgo
