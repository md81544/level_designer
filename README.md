## Level Designer for Amaze
I am in the process of resurrecting a game called "Amaze" that I wrote for an open-source hand-held games console in the early 2000s. It's a fairly simple game which involves piloting a rocket ship around mazes. The ship has 'asteroids'-style motion, i.e. you thrust forward and to stop you need to turn around and thrust in the opposite direction.

To design the levels originally, I used Inkscape, and wrote a small utility to parse the SVG file to create levels that the game could use.

I tried that recently and Inkscape just has too many bugs and quirks (on Mac) to make it usable for me.

I experimented with other SVG programs, both local and web-based, and none fitted my requirements. So I thought, how hard can it be to make my own? : )

This is the result. Three days' worth of initial effort and I had an graphical editor I could use. It's a bit quick-and-dirty (it sorely requires refactoring), but it's functional.

### Usage
Open an existing level with **File > Open**, or pass its file name on the command line (if the file doesn't exist, it's created when you save). There is an example.lvl file included.

The editor works in "modes", chosen from the **Insert** menu (Line, Breakable Line, Moving Object, Polygon, Fuel, Start Position, Exit), plus **Edit > Select** for selecting existing items: click an item, or drag a box around several. The selection can be dragged (or nudged with the arrow keys) to move it, rotated by dragging the round handle above it (hold Shift for 15° steps), deleted, or (for lines) turned into a moving object via the **Object** menu. The status bar shows the current mode and a reminder of how to use it.

When drawing lines, click to place each point and keep clicking to keep making lines. To stop, right click, then click somewhere else to start a new line. Escape returns to Select mode from any other mode. Snapping (to the grid, existing lines, both or neither) is set in **View > Snap To**.

The mouse wheel or trackpad scrolls (Shift + wheel scrolls sideways), Ctrl/Cmd + wheel or a trackpad pinch zooms, and Shift + mouse move also scrolls the view. Keyboard shortcuts are shown in the menus, and **Help > Mouse and Keyboard** summarises the rest.

Levels are saved in the JSON format described in [LEVEL_FORMAT.md](LEVEL_FORMAT.md). Levels in the old line-based format can still be opened, and are saved in the new format.

### Requirements

A C++23 compiler, CMake and Qt 6.5 or later. For mac, `brew install qt`. Then `make` (debug) or `make release` (which also bundles Qt into the app). If Catch2 is installed (`brew install catch2`), `make test` runs the unit tests; some of these check amaze's levels, if amaze is checked out alongside this repo.

### Platforms Supported
Currently as this is for my own use I've only bothered testing on **MacOS**. I will probably test on Linux and Windows eventually.
