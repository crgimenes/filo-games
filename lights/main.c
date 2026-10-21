/* filo-lights on a POSIX terminal: the program is lights.fbb, embedded. */
#include "tty.h"

extern const uint8_t lights_fbb[];
extern const size_t lights_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-lights\n"
    "\n"
    "Lights Out: a press turns a light and its neighbours; make the board dark.\n"
    "\n"
    "Keys: arrows or w a s d move  space presses  r new  Esc leaves\n"
    "\n"
    "Example: filo-lights\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, lights_fbb, lights_fbb_len, usage);
}
