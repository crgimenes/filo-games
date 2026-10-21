/* filo-mastermind on a POSIX terminal: the program is mastermind.fbb, embedded. */
#include "tty.h"

extern const uint8_t mastermind_fbb[];
extern const size_t mastermind_fbb_len;

const app_spec app_program = {GAME_NAME, GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: filo-mastermind\n"
                            "\n"
                            "Mastermind: crack a code of four colours in ten guesses.\n"
                            "\n"
                            "Keys: 1-6 colours  Backspace erases  Enter tries  r new  Esc leaves\n"
                            "\n"
                            "Example: filo-mastermind\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, mastermind_fbb, mastermind_fbb_len, usage);
}
