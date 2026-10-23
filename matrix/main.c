/* filo-matrix on a POSIX terminal: the program is matrix.fbb, embedded. */
#include "tty.h"

extern const uint8_t matrix_fbb[];
extern const size_t matrix_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] =
    "usage: filo-matrix\n"
    "\n"
    "The rain of the Matrix: half-width katakana falling, a column at its own pace.\n"
    "\n"
    "Keys: space holds it  Esc leaves\n"
    "\n"
    "Example: filo-matrix\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, matrix_fbb, matrix_fbb_len, usage);
}
