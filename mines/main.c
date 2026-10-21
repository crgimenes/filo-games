/* filo-mines on a POSIX terminal: the program is mines.fbb, embedded. */
#include "tty.h"

extern const uint8_t mines_fbb[];
extern const size_t mines_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-mines\n"
    "\n"
    "Minesweeper: open every cell without a mine; the numbers count the mines around.\n"
    "\n"
    "Keys: arrows move  space opens  f flags  x makes a click flag  r new  Esc leaves\n"
    "\n"
    "Example: filo-mines\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, mines_fbb, mines_fbb_len, usage);
}
