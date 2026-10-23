/* filo-life on a POSIX terminal: the program is life.fbb, embedded. */
#include "tty.h"

extern const uint8_t life_fbb[];
extern const size_t life_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-life\n"
    "\n"
    "Conway's Life on half blocks, white-hot when born, cooling to embers.\n"
    "\n"
    "Keys: space holds it  Esc leaves\n"
    "\n"
    "Example: filo-life\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, life_fbb, life_fbb_len, usage);
}
