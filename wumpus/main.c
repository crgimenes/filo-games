/* filo-wumpus on a POSIX terminal: the program is wumpus.fbb, embedded. */
#include "tty.h"

extern const uint8_t wumpus_fbb[];
extern const size_t wumpus_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-wumpus\n"
    "\n"
    "Hunt the Wumpus (1973): find it in a cave of twenty rooms and shoot it.\n"
    "\n"
    "Type: m ROOM moves  s ROOM... shoots  new starts over  Esc leaves\n"
    "\n"
    "Example: filo-wumpus\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, wumpus_fbb, wumpus_fbb_len, usage);
}
