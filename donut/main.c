/* filo-donut on a POSIX terminal: the program is donut.fbb, embedded. */
#include "tty.h"

extern const uint8_t donut_fbb[];
extern const size_t donut_fbb_len;

const app_spec app_program = {"filo-donut", GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-donut\n"
    "\n"
    "A spinning torus in the terminal: Andy Sloan's donut.c, written in Filo.\n"
    "\n"
    "Keys: space holds it  Esc leaves\n"
    "\n"
    "Example: filo-donut\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, donut_fbb, donut_fbb_len, usage);
}
