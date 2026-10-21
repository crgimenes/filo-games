/* filo-twenty48 on a POSIX terminal: the program is twenty48.fbb, embedded. */
#include "tty.h"

extern const uint8_t twenty48_fbb[];
extern const size_t twenty48_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: filo-twenty48\n"
                            "\n"
                            "2048: slide the tiles, equal ones join; make a 2048.\n"
                            "\n"
                            "Keys: arrows or w a s d slide  r new  Esc leaves\n"
                            "\n"
                            "Example: filo-twenty48\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, twenty48_fbb, twenty48_fbb_len, usage);
}
