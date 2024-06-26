## Level Designer for Amaze
I am in the process of resurrecting a game I wrote for an open-source hand-held games console in the early 2000s. It's a fairly simple game which involves piloting a rocket ship around mazes. The ship has 'asteroids'-style motion, i.e. you thrust forward and to stop you need to turn around and thrust in the opposite direction.

Originally, to design the levels, I used Inkscape, and parsed the SVG output file to create levels that the game could use.

I tried that recently and Inkscape just has too many bugs and quirks to make it usable. I don't recall having so many issues in the past... maybe bit-rot has set in there?

I experimented with other SVG programs, both local and web-based, and none fitted my requirements. So I thought, how hard can it be to make my own? : )

This is the result. Three days' worth of effort and I have an editor I can use.

The repository for the actual game 'Amaze' is set to private currently but I will make it public once I've knocked it into what I'd deem a suitable first draft.

### Usage
Existing level files can be loaded by specifying the file name on the command line. There is an example.lvl file included.

Currently there are two "modes", switchable by the "I" key - "Insert" (for line generation) and "Edit" for selecting existing lines and deleting them (press 'X' or delete or backspace).

Press "S" to 'save' (it currently just outputs to stdout, which is fine for either copy/pasting or piping from the terminal).

Press "Q" to quit (or just close the window).

### TODO
* Ability to place start point
* Ability to place exit point
* Ability to place fuel markers

### Requirements

Assuming you have a working clang++, just sfml. For mac, `brew install sfml`

### Platforms Supported
Currently as this is for my own use I've only bothered testing on **MacOS**. I will probably test on Linux and Windows eventually.