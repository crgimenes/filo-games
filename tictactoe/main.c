/* filo-tictactoe on a POSIX terminal: the program is tictactoe.fbb, embedded. */
#include "tty.h"

extern const uint8_t tictactoe_fbb[];
extern const size_t tictactoe_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-tictactoe\n"
    "\n"
    "Tic-tac-toe for 0, 1 or 2 players; 0 is the machine against itself.\n"
    "\n"
    "Keys: 0 1 2 players  arrows move  space plays  1-9 a square  r new  m players  Esc leaves\n"
    "\n"
    "Example: filo-tictactoe\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, tictactoe_fbb, tictactoe_fbb_len, usage);
}
