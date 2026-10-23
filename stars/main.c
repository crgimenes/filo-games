/* filo-stars on a POSIX terminal: the program is stars.fbb, embedded. */
#include "tty.h"

extern const uint8_t stars_fbb[];
extern const size_t stars_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: filo-stars\n"
                            "\n"
                            "A field of stars flying at the screen: far is a dot, near is a star.\n"
                            "\n"
                            "Keys: space holds it  Esc leaves\n"
                            "\n"
                            "Example: filo-stars\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, stars_fbb, stars_fbb_len, usage);
}
