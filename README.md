## Level Designer for Amaze
I am in the process of resurrecting a game called "Amaze" that I wrote for an open-source hand-held games console in the early 2000s. It's a fairly simple game which involves piloting a rocket ship around mazes. The ship has 'asteroids'-style motion, i.e. you thrust forward and to stop you need to turn around and thrust in the opposite direction.

To design the levels originally, I used Inkscape, and wrote a small utility to parse the SVG file to create levels that the game could use.

I tried that recently and Inkscape just has too many bugs and quirks to make it usable for me. I don't recall having so many issues in the past... maybe bit-rot has set in there?

I experimented with other SVG programs, both local and web-based, and none fitted my requirements. So I thought, how hard can it be to make my own? : )

This is the result. Three days' worth of initial effort and I had an graphical editor I could use. It's a bit quick-and-dirty (it sorely requires refactoring), but it's functional.

### Usage
Existing level files can be loaded by specifying the file name on the command line. There is an example.lvl file included.

The editor uses the concept of "modes" for editing. Currently there are five, switchable by the "M" key - "LINE" (for line generation) and "EDIT" for selecting existing lines and deleting them (press 'X' or delete or backspace). The other modes, "START","EXIT", and "FUEL" allow placement of those items specifically.

When in "LINE" mode, click to place a line and keep clicking to keep making lines. If you don't want to connect a line to the last one, just press escape (or right click) then click somewhere else to start a new line. Line snapping is controlled
by the "S" key - "AUTO" will snap to grid vertices or existing lines, "GRID" is vertices only, "LINE" is line only, and "NONE" is no snapping.

Press Cmd-S to 'save' (it currently just outputs to stdout, which is fine for either copy/pasting or piping from the terminal).

Press "Q" to quit (or just close the window). Currently "Q" has a confirmation dialog, but closing the window does not.

* Confirmation dialog on window close

### Requirements

Assuming you have a working clang++, just sfml 3.x. For mac, `brew install sfml`

### Platforms Supported
Currently as this is for my own use I've only bothered testing on **MacOS**. I will probably test on Linux and Windows eventually.
