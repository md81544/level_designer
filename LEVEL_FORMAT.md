# Amaze level file format (JSON), version 1

Levels for amaze are designed with level_designer. This document defines the
file format both programs use.

**This file exists in both repos (amaze and level_designer), as do the code
which reads and writes the format (`levelformat.h` / `levelformat.cpp`) and
its tests (`tests/levelformat_tests.cpp`, `tests/levelformat_test_utils.h`).
The copies must be kept identical:** any change to the format is made to this
document and the code, in both repos, together. (`diff` between the repos
should show no differences in these files.)

## Overview

A level file is a UTF-8 JSON object. For example:

```json
{
  "formatVersion": 1,
  "title": "Rookie Trainer",
  "ship": { "x": 706, "y": 904, "rotation": 0 },
  "exit": { "x": 1200, "y": 300 },
  "fuel": [
    { "x": 500, "y": 500 },
    { "x": 650, "y": 820 }
  ],
  "walls": [
    { "points": [[650, 600], [1450, 600], [1450, 1350], [650, 1350]], "closed": true },
    { "points": [[750, 700], [1350, 700]] }
  ],
  "breakables": [
    { "lines": [{ "points": [[1000, 975], [1000, 1050]] }] }
  ],
  "decorations": [
    { "points": [[700, 1300], [1400, 1300]] }
  ],
  "movingObjects": [
    {
      "lines": [
        { "points": [[900, 400], [1000, 400], [1000, 450], [900, 450]], "closed": true }
      ],
      "speedX": 0.3,
      "rangeX": 100,
      "spin": 0.5
    }
  ]
}
```

## Conventions

- **Coordinates** are in arena units. The arena is 2000 × 2000 units, with
  (0, 0) at the top left and y increasing downwards. Coordinates are numbers.
  level_designer only writes integers, but readers must accept any number.
- **Rotations** are in degrees, **anticlockwise as seen on screen**. 0 means
  the item's normal orientation, i.e. the ship pointing up, or text upright.
  This matches the ship angle in the old format, so it converts unchanged.
- **Optional fields** may be left out, and then take the default given below.
  Writers should leave out fields whose value is the default, to keep files
  short.
- **Unknown fields** are ignored by readers. This lets newer files, with fields
  added later, load in older programs.
- **Appearance** (colours, and which text is drawn for the exit and fuel) is
  decided by the game from each item's type, not stored in the file. See
  [Appearance](#appearance-for-reference). Only line thickness can be
  overridden, because a few existing levels use non-standard thicknesses.

## Top level

| Field | Type | Default | Meaning |
|---|---|---|---|
| `formatVersion` | integer | (required) | Always 1 for this version; see [Versions](#versions) |
| `title` | string | `""` | The level's title, shown before it starts |
| `ship` | [Ship](#ship) | at (0, 0), rotation 0 | The ship's starting position |
| `exit` | [Exit](#exit) | none | The exit. A level has at most one. |
| `fuel` | array of [Fuel](#fuel) | `[]` | Fuel to collect |
| `walls` | array of [Polyline](#polyline) | `[]` | Obstructions |
| `breakables` | array of [Breakable](#breakable) | `[]` | Walls which the ship's exhaust breaks |
| `decorations` | array of [Polyline](#polyline) | `[]` | Lines which are only drawn: the ship passes over them. (Older programs, which don't know about them, ignore them.) |
| `movingObjects` | array of [Moving object](#moving-object) | `[]` | Obstructions which move |

## Ship

| Field | Type | Default | Meaning |
|---|---|---|---|
| `x`, `y` | number | (required) | The ship's centre |
| `rotation` | number | `0` | Degrees anticlockwise; 0 is pointing up |

## Exit

| Field | Type | Default | Meaning |
|---|---|---|---|
| `x`, `y` | number | (required) | The centre of the exit's text |
| `rotation` | number | `0` | Degrees anticlockwise about its centre; 0 is upright |
| `thickness` | number | `6` | Line thickness of the text |

## Fuel

| Field | Type | Default | Meaning |
|---|---|---|---|
| `x`, `y` | number | (required) | The centre of the fuel |
| `thickness` | number | `12` | Line thickness of the fuel's symbol |

## Polyline

A sequence of connected straight lines: each point is joined to the next.

| Field | Type | Default | Meaning |
|---|---|---|---|
| `points` | array of `[x, y]` | (required) | At least two points |
| `closed` | boolean | `false` | If true, the last point is also joined to the first |
| `thickness` | number | depends on use (see [Appearance](#appearance-for-reference)) | Line thickness |

## Breakable

A wall which breaks when the ship's exhaust hits it. It breaks as a whole, so
it can be made of several separate pieces which break together (e.g.
`level8.lvl` has a breakable made of two separate parallel lines).

| Field | Type | Default | Meaning |
|---|---|---|---|
| `lines` | array of [Polyline](#polyline) | (required) | At least one polyline |

## Moving object

An obstruction which moves and/or spins. Its lines are given at their
starting position, in arena coordinates. It moves in one of two ways:

- Bouncing within a box: `speedX`, `rangeX`, `speedY` and `rangeY`. Each axis
  moves back and forth independently, so unless the two take the same time to
  cover their ranges, it zigzags around the box.
- Following a `path`: back and forth along a line, or a route of several
  lines, or round and round a loop.

An object can't have both a `path` and any of `speedX`, `rangeX`, `speedY` or
`rangeY`.

| Field | Type | Default | Meaning |
|---|---|---|---|
| `lines` | array of [Polyline](#polyline) | (required) | The object's shape, at least one polyline |
| `speedX` | number | `0` | Horizontal movement per game tick, in arena units (amaze runs at 120 ticks per second) |
| `rangeX` | number | `0` | How far it moves back and forth either side of its starting position, horizontally |
| `speedY` | number | `0` | As `speedX`, vertically |
| `rangeY` | number | `0` | As `rangeX`, vertically |
| `path` | [Path](#path) | none | The path it follows, instead of bouncing within a box |
| `spin` | number | `0` | Rotation about its centre per game tick, in degrees |
| `gravity` | number | `0` | Strength of the object's pull on the ship, which acts within `gravity` × 25 units of it. 10 to 100 is a good range. |
| `name` | string | none | An identifier, e.g. for debugging |

## Path

A moving object starts at the position it's drawn in, and moves (without
turning) through each of the `offsets` in turn. At the last one, it comes back
the same way, and so on; or if `loop` is true, it goes straight from the last
one back to the start, and round again. Each offset is how far the object has
moved from its drawn position at that point of the path, in arena units, e.g.
`[[200, -150]]` moves it back and forth between where it's drawn and 200 units
right and 150 up.

| Field | Type | Default | Meaning |
|---|---|---|---|
| `offsets` | array of [x, y] | (required) | At least one offset |
| `speed` | number | `0` | Distance moved along the path per game tick |
| `loop` | boolean | `false` | Whether to go round and round, rather than back and forth |

## Appearance (for reference)

This is decided by the game, so isn't part of the format. It's listed here
because level_designer draws items the same way.

| Item | Colour (RGB) | Default thickness |
|---|---|---|
| Walls | 255, 0, 0 | 2 |
| Breakables | 255, 150, 50 | 6 |
| Decorations | 0, 90, 0 (slightly lighter than amaze's background grid, which is 0, 32, 0); level_designer, whose grid is brighter (0, 100, 0), draws them in 0, 170, 0 | 2 |
| Moving objects | 255, 172, 163 | 6 |
| Exit (text "EXIT") | 52, 213, 235 | 6 |
| Fuel (text "\*") | 255, 255, 0 | 12 |

## Versions

`formatVersion` is only increased for changes which older programs can't read
correctly, e.g. a field changing meaning. Adding a new optional field doesn't
change it, because older programs will simply ignore the field. A reader given
a file with a higher `formatVersion` than it supports should refuse to load
it, with a message saying so.

## Writing files

To keep files readable, and their diffs small, writers should:

- write fields in the order given in this document
- indent with two spaces, putting each point (and each short object, such as
  a fuel item) on one line

Note that Qt's `QJsonDocument::Indented` output puts every number on its own
line, which makes files very long, so writers will need their own formatting
(or to post-process Qt's compact output).

## The old format, and migration

The old format, as described in amaze's `levelformat.h`, is a line-based text
format with `~`-separated fields, starting with a `!` header line.

During migration:

1. Both programs read both formats, telling them apart by the first
   non-whitespace character: `{` for JSON, `!` for the old format.
2. level_designer saves JSON.
3. The levels in amaze's `data` directory are converted, e.g. by opening and
   saving them in level_designer.
4. Once nothing uses the old format, both programs stop reading it.

Converting from the old format:

- The header's time limit and fuel fields are unused, and are dropped.
- Lines in an `OBSTRUCTION` object become `walls`. Lines which join end to end
  are combined into polylines, and a polyline which ends where it starts
  becomes `"closed": true`.
- Each `BREAKABLE` object becomes one entry in `breakables`, so it still breaks
  as a whole. Its lines are combined into polylines as for walls.
- Each `MOVING` object becomes a moving object: `xDelta`, `xMaxDifference`,
  `yDelta`, `yMaxDifference`, `rotationDelta` and `gravity` become `speedX`,
  `rangeX`, `speedY`, `rangeY`, `spin` and `gravity`.
- A `MOVING` object's lines are combined into polylines as for walls.
- `EXIT` and `FUEL` objects take their position from their `P` line, and their
  thickness from their `T` line (if it isn't the default).
- Line colours are dropped: all existing levels use the standard colours.
- amaze also recognises `PRISONER` and `KEY` objects (and treats unknown types
  as neutral), but no levels use them. They aren't part of version 1, and can
  be added later as new optional fields.

## File names

Level files keep the `.lvl` extension, whichever format they're in: their
contents say which format a file is in.
