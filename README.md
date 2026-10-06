# filo-games

Games and demos written in [Filo](https://github.com/crgimenes/clang_filo),
each its own program. A game is a directory of `.filo` files — an entry
per file, `init`, `draw`, and `key`, `input` and `tick` when it has them —
compiled to a bundle, `NAME.fbb`, that any Filo VM with the app's base can
load, and a binary, `bin/NAME`, that carries it. The terminal, the keys and
the loop are [filo-term](https://github.com/crgimenes/filo-term)'s app.

| game | what it is |
| --- | --- |
| `donut` | a spinning torus, Andy Sloan's donut.c, with no trigonometry at run time and the depth of each cell kept in its tag |
| `snake` | Snake; the screen is the game's memory: the head asks the cell it enters what is in it |
| `down` | a falling sky to steer through, catching what is worth catching |
| `fire` | the fire of the PlayStation DOOM; the heat of each cell lives in the cell |

Each one keeps to the budgets a board gives a program (20,000 steps a key,
200,000 a frame), so the same bundle runs on the desktop and on a board.

## Build

```
make            # bin/donut ... and donut.fbb ...
make bin/snake  # one game
make test       # every game played for a while, under ASan/UBSan
make qa         # the above, clang-format, clang-tidy, cppcheck
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
