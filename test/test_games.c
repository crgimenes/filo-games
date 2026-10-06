/* Every game played for a while the way a terminal plays it — time, the
   arrows, the keys it knows, Esc — within the budgets a board gives a
   program, which are the app's own: a game that fits here fits there.
   usage: test_games NAME.fbb... */
#include <stdio.h>
#include <string.h>

#include "app.h"

enum { FBB_CAP = 256 * 1024, TICKS = 400, TICK_MS = 20 };

const app_spec app_program = {"games", "", false, 0, 0, 0, NULL};

static app A;
static uint8_t fbb[FBB_CAP];
static int failures = 0;

static void fail(const char *game, const char *what) {
    printf("FAIL %s: %s\n", game, what);
    failures++;
}

static void drain(void) {
    uint8_t out[TERM_OUT_CAP];
    while (term_out_read(&A.t, out, sizeof(out)) > 0) {
    }
}

static void key(const char *bytes) {
    app_input(&A, (const uint8_t *)bytes, strlen(bytes));
    app_tick(&A, 200); /* a lone Esc is told apart by time */
}

/* The game named by the file, at a size and then another: time passing,
   every kind of key, a resize, Esc. Any error the game raised is one. */
static void play(const char *path) {
    char name[64];
    const char *base = strrchr(path, '/');
    base = base != NULL ? base + 1 : path;
    size_t n = strcspn(base, ".");
    if (n >= sizeof(name)) {
        fail(path, "name too long");
        return;
    }
    memcpy(name, base, n);
    name[n] = '\0';
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail(name, "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {name, "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 7;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail(name, why);
        return;
    }
    static const char *const keys[] = {"\x1b[A", "\x1b[C", "\x1b[B", "\x1b[D", " ", "r", "a", "d"};
    for (int i = 0; i < TICKS; i++) {
        app_tick(&A, TICK_MS);
        if (i % 25 == 0) {
            key(keys[(i / 25) % (int)(sizeof(keys) / sizeof(keys[0]))]);
        }
        if (i == TICKS / 2) {
            app_resize(&A, 132, 40);
        }
        drain();
        if (A.error[0] != '\0') {
            fail(name, A.error);
            return;
        }
    }
    key("\x1b");
    if (!app_done(&A)) {
        fail(name, "Esc did not leave");
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        play(argv[i]);
    }
    if (failures > 0) {
        printf("%d game tests failed\n", failures);
        return 1;
    }
    printf("all %d games played\n", argc - 1);
    return 0;
}
