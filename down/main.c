/* filo-down on a POSIX terminal: the program is down.fbb, embedded. */
#include "tty.h"

extern const uint8_t down_fbb[];
extern const size_t down_fbb_len;

const app_spec app_program = {"filo-down", GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: filo-down\n"
                            "\n"
                            "Fly down a falling sky in Filo, catching what is worth catching.\n"
                            "\n"
                            "Keys: left/right, h l or a d steer  r flies again  Esc leaves\n"
                            "\n"
                            "Example: filo-down\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, down_fbb, down_fbb_len, usage);
}
