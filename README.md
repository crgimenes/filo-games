# filo-games

Games and demos written in [Filo](https://github.com/crgimenes/clang_filo),
each its own program. A game is a directory of `.filo` files — an entry
per file, `init`, `draw`, and `key`, `input` and `tick` when it has them —
compiled to a bundle, `NAME.fbb`, that any Filo VM with the app's base can
load, and a binary, `bin/filo-NAME`, that carries it. The terminal, the keys and
the loop are [filo-term](https://github.com/crgimenes/filo-term)'s app.

| command | what it is |
| --- | --- |
| `filo-donut` | a spinning torus, Andy Sloan's donut.c, with no trigonometry at run time and the depth of each cell kept in its tag |
| `filo-snake` | Snake; the screen is the game's memory: the head asks the cell it enters what is in it |
| `filo-down` | a falling sky to steer through, catching what is worth catching |
| `filo-fire` | the fire of the PlayStation DOOM; the heat of each cell lives in the cell |

Each one keeps to the budgets a board gives a program (20,000 steps a key,
200,000 a frame), so the same bundle runs on the desktop and on a board.

## Install

```
brew install crgimenes/tap/filo-games
```

installs the four games: filo-donut, filo-snake, filo-down and filo-fire.
Or take a binary from the
[releases](https://github.com/crgimenes/filo-games/releases): macOS
(universal, arm64 and x86_64) and Linux (static, amd64 and arm64), nothing
else to install.

## Build

```
make                 # bin/filo-donut ... and donut.fbb ...
make bin/filo-snake  # one game
make test            # every game played for a while, under ASan/UBSan
make qa              # the above, clang-format, clang-tidy, cppcheck
make dist            # the release binaries in dist/ (needs zig for Linux)
```

The bundles are compiled by the filo CLI, C's built from `FILO ?=
../clang_filo` unless another is named: Go's writes the same bytes
(`make FILO_CLI=filo`). `FILO_TERM ?= ../filo-term`. A release links
statically on Linux (`make LDFLAGS=-static` on musl); on macOS a game needs
nothing past libSystem.

## A new game

A directory with its `.filo` files and a `main.c` like the others, and its
name in `GAMES` in the Makefile.

## License

MIT
