/* fire on a POSIX terminal: the program is fire.fbb, embedded. */
#include "tty.h"

extern const uint8_t fire_fbb[];
extern const size_t fire_fbb_len;

const app_spec app_program = {"fire", GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: fire\n"
    "\n"
    "The fire of the PlayStation DOOM, in Filo: the heat lives in the cells.\n"
    "\n"
    "Keys: space puts it out or lights it  Esc leaves\n"
    "\n"
    "Example: fire\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, fire_fbb, fire_fbb_len, usage);
}
