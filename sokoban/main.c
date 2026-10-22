/* filo-sokoban on a POSIX terminal: the program is sokoban.fbb, embedded. */
#include "tty.h"

extern const uint8_t sokoban_fbb[];
extern const size_t sokoban_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: filo-sokoban\n"
                            "\n"
                            "Sokoban: push every box onto a goal; six levels of the board.s own.\n"
                            "\n"
                            "Keys: arrows push  u undo  r restart  n p level  Esc leaves\n"
                            "\n"
                            "Example: filo-sokoban\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, sokoban_fbb, sokoban_fbb_len, usage);
}
