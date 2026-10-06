/* snake on a POSIX terminal: the program is snake.fbb, embedded. */
#include "tty.h"

extern const uint8_t snake_fbb[];
extern const size_t snake_fbb_len;

const app_spec app_program = {"snake", GAME_VERSION, false, 0, 0, 0, NULL};

static app A;

static const char usage[] = "usage: snake\n"
                            "\n"
                            "Snake, the game, in Filo: the screen itself is the game's memory.\n"
                            "\n"
                            "Keys: arrows or w a s d move  r plays again  Esc leaves\n"
                            "\n"
                            "Example: snake\n";

int main(int argc, char **argv) {
    return tty_main(argc, argv, &A, &app_program, snake_fbb, snake_fbb_len, usage);
}
