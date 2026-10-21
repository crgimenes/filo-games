/* Every game played for a while the way a terminal plays it — time, the
   arrows, the keys it knows, Esc — within the budgets a board gives a
   program, which are the app's own: a game that fits here fits there.
   usage: test_games NAME.fbb... */
#include <stdio.h>
#include <stdlib.h>
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

/* Lights Out read off the screen: 5 by 5 lights, five columns by two rows
   each, one apart, the grid centred (25,5 on 80 by 24). */
enum { LO_N = 5, LO_V = LO_N * LO_N, LO_X = 25, LO_Y = 5 };

static int lo_on(int i) {
    const cv_cell *c = &A.target.cells[LO_Y + ((i / LO_N) * 3)][LO_X + ((i % LO_N) * 6)];
    return c->cp == 0x2588 ? 1 : 0; /* a lit light is full blocks, a dark one shade */
}

static void lo_click(int i) {
    char sgr[32];
    (void)snprintf(sgr, sizeof(sgr), "\x1b[<0;%d;%dM", LO_X + ((i % LO_N) * 6) + 1,
                   LO_Y + ((i / LO_N) * 3) + 1);
    app_input(&A, (const uint8_t *)sgr, strlen(sgr));
}

static bool lo_row_has(int row, const char *text) {
    char line[TERM_COLS_MAX + 1];
    size_t n = 0;
    for (uint16_t c = 0; c < A.target.cols; c++) {
        uint32_t cp = A.target.cells[row][c].cp;
        line[n++] = cp > 0 && cp < 0x80 ? (char)cp : ' ';
    }
    line[n] = '\0';
    return strstr(line, text) != NULL;
}

/* The presses that make this board dark, by Gaussian elimination over
   GF(2): row i says which presses turn light i, and whether it is on. */
/* Press at (r, c), when that is on the board, turns light i. */
static void lo_link(uint8_t m[LO_V][LO_V + 1], int i, int r, int c) {
    if (r >= 0 && r < LO_N && c >= 0 && c < LO_N) {
        m[i][(r * LO_N) + c] = 1;
    }
}

static void lo_matrix(uint8_t m[LO_V][LO_V + 1]) {
    memset(m, 0, sizeof(uint8_t) * LO_V * (LO_V + 1));
    for (int i = 0; i < LO_V; i++) {
        int r = i / LO_N;
        int c = i % LO_N;
        lo_link(m, i, r, c);
        lo_link(m, i, r - 1, c);
        lo_link(m, i, r + 1, c);
        lo_link(m, i, r, c - 1);
        lo_link(m, i, r, c + 1);
        m[i][LO_V] = (uint8_t)lo_on(i);
    }
}

/* Reduced row echelon form in place; pivot_of[col] the row of a column's
   pivot, -1 for a free one. The rank comes back. */
static int lo_eliminate(uint8_t m[LO_V][LO_V + 1], int pivot_of[LO_V]) {
    int row = 0;
    for (int col = 0; col < LO_V; col++) {
        pivot_of[col] = -1;
        int p = row;
        while (p < LO_V && m[p][col] == 0) {
            p++;
        }
        if (p == LO_V) {
            continue;
        }
        uint8_t tmp[LO_V + 1];
        memcpy(tmp, m[p], sizeof(tmp));
        memcpy(m[p], m[row], sizeof(tmp));
        memcpy(m[row], tmp, sizeof(tmp));
        for (int k = 0; k < LO_V; k++) {
            if (k == row || m[k][col] == 0) {
                continue;
            }
            for (int j = col; j <= LO_V; j++) {
                m[k][j] ^= m[row][j];
            }
        }
        pivot_of[col] = row;
        row++;
    }
    return row;
}

/* Every puzzle the game makes has a solution: false when this one has not. */
static bool lo_solve(int out[LO_V]) {
    static uint8_t m[LO_V][LO_V + 1];
    int pivot_of[LO_V];
    lo_matrix(m);
    int rank = lo_eliminate(m, pivot_of);
    for (int k = rank; k < LO_V; k++) {
        if (m[k][LO_V] != 0) {
            return false;
        }
    }
    for (int col = 0; col < LO_V; col++) {
        out[col] = pivot_of[col] >= 0 ? m[pivot_of[col]][LO_V] : 0;
    }
    return true;
}

static void test_lights(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("lights", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"lights", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 11;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("lights", why);
        return;
    }
    drain();
    int before[LO_N * LO_N];
    int lit = 0;
    for (int i = 0; i < LO_N * LO_N; i++) {
        before[i] = lo_on(i);
        lit += before[i];
    }
    if (lit == 0) {
        fail("lights", "a new game is already dark");
    }
    key(" "); /* the cursor starts in the middle: 12 and its four turn */
    drain();
    for (int i = 0; i < LO_N * LO_N; i++) {
        int turned = (i == 12 || i == 7 || i == 17 || i == 11 || i == 13) ? 1 : 0;
        if (lo_on(i) != (turned != 0 ? 1 - before[i] : before[i])) {
            fail("lights", "a press turned the wrong lights");
            break;
        }
    }
    if (!lo_row_has(LO_Y + 15, "moves 1")) {
        fail("lights", "a press is not counted");
    }
    int solution[LO_N * LO_N];
    if (!lo_solve(solution)) {
        fail("lights", "the puzzle has no solution");
        return;
    }
    for (int i = 0; i < LO_N * LO_N; i++) {
        if (solution[i] != 0) {
            lo_click(i);
        }
    }
    drain();
    for (int i = 0; i < LO_N * LO_N; i++) {
        if (lo_on(i) != 0) {
            fail("lights", "the solution's clicks left a light on");
            break;
        }
    }
    if (!lo_row_has(LO_Y + 15, "All dark in")) {
        fail("lights", "a dark board is not a win");
    }
    key("r");
    drain();
    if (lo_row_has(LO_Y + 15, "All dark") || !lo_row_has(LO_Y + 15, "moves 0")) {
        fail("lights", "r does not deal a new game");
    }
}

/* 2048 read off the screen: 4 by 4 tiles, six columns by three rows,
   one column apart, the number on the middle row; the grid centred (26,6
   on 80 by 24), the score two rows under it. */
enum { T_N = 4, T_V = T_N * T_N, T_X = 26, T_Y = 6 };

static int t_read(int i) {
    int row = T_Y + ((i / T_N) * 3) + 1;
    int col = T_X + ((i % T_N) * 7);
    int v = 0;
    for (int k = 0; k < 6; k++) {
        uint32_t cp = A.target.cells[row][col + k].cp;
        if (cp >= '0' && cp <= '9') {
            v = (v * 10) + (int)(cp - '0');
        }
    }
    return v;
}

static int t_score(void) {
    int row = T_Y + (T_N * 3) + 1;
    for (uint16_t c = 0; c + 6 < A.target.cols; c++) {
        if (A.target.cells[row][c].cp == 's' && A.target.cells[row][c + 5].cp == ' ') {
            int v = 0;
            for (uint16_t k = (uint16_t)(c + 6); k < A.target.cols; k++) {
                uint32_t cp = A.target.cells[row][k].cp;
                if (cp < '0' || cp > '9') {
                    break;
                }
                v = (v * 10) + (int)(cp - '0');
            }
            return v;
        }
    }
    return -1;
}

/* The cell j of line k as a move in dir pushes it: 0 left, 1 right, 2 up,
   3 down; j = 0 is where the tiles go. */
static int t_cell(int dir, int k, int j) {
    switch (dir) {
    case 0:
        return (k * T_N) + j;
    case 1:
        return (k * T_N) + (T_N - 1 - j);
    case 2:
        return (j * T_N) + k;
    default:
        return ((T_N - 1 - j) * T_N) + k;
    }
}

/* The reference move: every line slid to its start, equal neighbours
   joined once; what the joins score comes back. */
static int t_move(const int in[T_V], int out[T_V], int dir) {
    int gained = 0;
    for (int k = 0; k < T_N; k++) {
        int vals[T_N];
        int n = 0;
        for (int j = 0; j < T_N; j++) {
            int v = in[t_cell(dir, k, j)];
            if (v != 0) {
                vals[n++] = v;
            }
        }
        int line[T_N] = {0};
        int m = 0;
        for (int i = 0; i < n; i++) {
            if (i + 1 < n && vals[i] == vals[i + 1]) {
                line[m++] = vals[i] * 2;
                gained += vals[i] * 2;
                i++;
            } else {
                line[m++] = vals[i];
            }
        }
        for (int j = 0; j < T_N; j++) {
            out[t_cell(dir, k, j)] = line[j];
        }
    }
    return gained;
}

static const char *t_check_move(const int before[T_V], const int after[T_V], int dir) {
    int want[T_V];
    (void)t_move(before, want, dir);
    int fresh = 0;
    int changed = 0;
    for (int i = 0; i < T_V; i++) {
        if (want[i] != before[i]) {
            changed = 1;
        }
        if (after[i] == want[i]) {
            continue;
        }
        if (want[i] != 0 || (after[i] != 2 && after[i] != 4)) {
            return "a tile is not where the slide puts it";
        }
        fresh++;
    }
    if (fresh != changed) {
        return "a move that changes the board brings one new tile, and only then";
    }
    return NULL;
}

static void test_twenty48(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("twenty48", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"twenty48", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 5;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("twenty48", why);
        return;
    }
    drain();
    int before[T_V];
    int tiles = 0;
    for (int i = 0; i < T_V; i++) {
        before[i] = t_read(i);
        tiles += before[i] != 0 ? 1 : 0;
    }
    if (tiles != 2 || t_score() != 0) {
        fail("twenty48", "a new game is not two tiles and no score");
    }
    static const char *const arrows[] = {"\x1b[D", "\x1b[C", "\x1b[A", "\x1b[B"};
    static const int order[] = {0, 3, 1, 3, 2, 0, 3};
    for (int turn = 0; turn < 300; turn++) {
        int dir = order[turn % 7];
        int want[T_V];
        int gained = t_move(before, want, dir);
        int score = t_score();
        key(arrows[dir]);
        drain();
        int after[T_V];
        for (int i = 0; i < T_V; i++) {
            after[i] = t_read(i);
        }
        const char *why_not = t_check_move(before, after, dir);
        if (why_not != NULL) {
            fail("twenty48", why_not);
            return;
        }
        if (t_score() != score + gained) {
            fail("twenty48", "the score is not what the joins made");
            return;
        }
        memcpy(before, after, sizeof(before));
    }
}

/* Mastermind read off the screen: ten rows from (27,5) on 80 by 24, a
   peg's digit at 3 + 4i + 1 into its row, the answer's dots from column
   20; the code is told under the rows when the tries run out. */
enum { M_PEGS = 4, M_TRIES = 10, M_X = 27, M_Y = 5 };

static void m_answer(int row, int *exact, int *near) {
    *exact = 0;
    *near = 0;
    for (int k = 0; k < M_PEGS; k++) {
        uint32_t cp = A.target.cells[row][M_X + 20 + k].cp;
        *exact += cp == 0x25cf ? 1 : 0;
        *near += cp == 0x25cb ? 1 : 0;
    }
}

/* The reference answer: right places, then right colours elsewhere. */
static void m_score(const int code[M_PEGS], const int guess[M_PEGS], int *exact, int *near) {
    int in_code[7] = {0};
    int in_guess[7] = {0};
    *exact = 0;
    for (int i = 0; i < M_PEGS; i++) {
        *exact += code[i] == guess[i] ? 1 : 0;
        in_code[code[i]]++;
        in_guess[guess[i]]++;
    }
    int common = 0;
    for (int c = 1; c <= 6; c++) {
        common += in_code[c] < in_guess[c] ? in_code[c] : in_guess[c];
    }
    *near = common - *exact;
}

static bool m_code(int code[M_PEGS], int tries) {
    const cv_cell *row = A.target.cells[M_Y + M_TRIES + 1];
    for (uint16_t c = 0; c + 13 < A.target.cols; c++) {
        if (row[c].cp == 'w' && row[c + 1].cp == 'a' && row[c + 2].cp == 's' &&
            row[c + 3].cp == ' ') {
            for (int i = 0; i < M_PEGS; i++) {
                code[i] = (int)row[c + 4 + (2 * i)].cp - '0';
            }
            return true;
        }
    }
    /* cracked: the code is the last guess */
    for (int i = 0; i < M_PEGS; i++) {
        code[i] = (int)A.target.cells[M_Y + tries - 1][M_X + 3 + (4 * i) + 1].cp - '0';
    }
    return false;
}

static void test_mastermind(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("mastermind", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"mastermind", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 3;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("mastermind", why);
        return;
    }
    static const char *const guesses[M_TRIES] = {
        "1111", "2222", "3333", "4444", "5555", "6666", "1234", "5612", "3456", "6543",
    };
    key("12");
    key("\x7f"); /* Backspace takes the 2 back */
    key("\r");   /* three pegs are no guess */
    drain();
    if (A.target.cells[M_Y][M_X + 20].cp == 0x25cf || A.target.cells[M_Y][M_X + 3 + 1].cp != '1') {
        fail("mastermind", "a short guess was taken, or typing is lost");
    }
    key("\x7f");
    int tries = 0;
    for (int t = 0; t < M_TRIES; t++) {
        char line[8];
        (void)snprintf(line, sizeof(line), "%s\r", guesses[t]);
        key(line);
        drain();
        tries++;
        int e = 0;
        int n = 0;
        m_answer(M_Y + t, &e, &n);
        if (e == M_PEGS) {
            break;
        }
    }
    int code[M_PEGS];
    (void)m_code(code, tries);
    for (int t = 0; t < tries; t++) {
        int guess[M_PEGS];
        for (int i = 0; i < M_PEGS; i++) {
            guess[i] = guesses[t][i] - '0';
            if (code[i] < 1 || code[i] > 6) {
                fail("mastermind", "the code is not told");
                return;
            }
        }
        int e = 0;
        int n = 0;
        int we = 0;
        int wn = 0;
        m_answer(M_Y + t, &e, &n);
        m_score(code, guess, &we, &wn);
        if (e != we || n != wn) {
            fail("mastermind", "an answer is not the guess scored against the code");
            return;
        }
    }
}

/* Hunt the Wumpus read off the screen: the story's rows from (4,8) on 80
   by 24, the title row with the arrows two above. */
enum { W_X = 4, W_TOP = 6, W_LOG = 14 };

static const int w_cave[21][3] = {
    {0, 0, 0},   {2, 5, 8},   {1, 3, 10},   {2, 4, 12},  {3, 5, 14},   {1, 4, 6},    {5, 7, 15},
    {6, 8, 17},  {1, 7, 9},   {8, 10, 18},  {2, 9, 11},  {10, 12, 19}, {3, 11, 13},  {12, 14, 20},
    {4, 13, 15}, {6, 14, 16}, {15, 17, 20}, {7, 16, 18}, {9, 17, 19},  {11, 18, 20}, {13, 16, 19},
};

static void w_row(int row, char *line, size_t cap) {
    size_t n = 0;
    for (uint16_t c = 0; c < A.target.cols && n + 1 < cap; c++) {
        uint32_t cp = A.target.cells[row][c].cp;
        line[n++] = cp > 0 && cp < 0x80 ? (char)cp : ' ';
    }
    line[n] = '\0';
}

static bool w_story_has(const char *text) {
    char line[TERM_COLS_MAX + 1];
    for (int r = 0; r < W_LOG; r++) {
        w_row(W_TOP + 2 + r, line, sizeof(line));
        if (strstr(line, text) != NULL) {
            return true;
        }
    }
    return false;
}

/* The first n whole numbers in a line, in order; how many there were. */
static int w_numbers(const char *line, long out[], int n) {
    int got = 0;
    const char *p = line;
    while (*p != '\0' && got < n) {
        if (*p >= '0' && *p <= '9') {
            char *end = NULL;
            out[got++] = strtol(p, &end, 10);
            p = end;
        } else {
            p++;
        }
    }
    return got;
}

/* The room the last line of the story puts you in, its tunnels in t;
   -1 when the last line is no room. */
static int w_where(int t[3]) {
    char line[TERM_COLS_MAX + 1];
    for (int r = W_LOG - 1; r >= 0; r--) {
        w_row(W_TOP + 2 + r, line, sizeof(line));
        const char *at = strstr(line, "You are in room ");
        long v[4];
        if (at != NULL && w_numbers(at, v, 4) == 4) {
            t[0] = (int)v[1];
            t[1] = (int)v[2];
            t[2] = (int)v[3];
            return (int)v[0];
        }
        if (strspn(line, " ") != strlen(line)) {
            return -1; /* the story ends in something else */
        }
    }
    return -1;
}

static void w_type(const char *text) {
    key(text);
    key("\r");
    drain();
}

/* NULL when the tunnels told are the room's own */
static const char *w_tunnels_ok(int room, const int t[3]) {
    for (int k = 0; k < 3; k++) {
        int found = 0;
        for (int j = 0; j < 3; j++) {
            found += w_cave[room][j] == t[k] ? 1 : 0;
        }
        if (found == 0) {
            return "the tunnels told are not the room's";
        }
    }
    return NULL;
}

/* A room no tunnel reaches is refused, and the room told again. */
static const char *w_through_rock(int room, const int t[3]) {
    int far = 1;
    while (far == room || far == t[0] || far == t[1] || far == t[2]) {
        far++;
    }
    char cmd[16];
    (void)snprintf(cmd, sizeof(cmd), "m %d", far);
    w_type(cmd);
    int again[3];
    if (!w_story_has("No tunnel to room") || w_where(again) != room) {
        return "a move through rock was taken";
    }
    return NULL;
}

/* A move down a tunnel ends in that room, unless bats or the wumpus say
   otherwise (a pit ends the story, which w_where tells). */
static const char *w_walk(int to) {
    char cmd[16];
    (void)snprintf(cmd, sizeof(cmd), "m %d", to);
    w_type(cmd);
    int t[3];
    int now = w_where(t);
    if (now > 0 && now != to && !w_story_has("super bat snatch") &&
        !w_story_has("Bumped a wumpus")) {
        return "a move did not end in the room walked to";
    }
    return NULL;
}

/* One turn: the story checked, then a move; the moves made are counted. */
static const char *w_turn(int turn, int *moves) {
    int t[3];
    int room = w_where(t);
    if (room < 1 || room > 20) {
        if (!w_story_has("Type new") && !w_story_has("The hunt is over")) {
            return "the story ends neither in a room nor in the end of the hunt";
        }
        w_type("new");
        return NULL;
    }
    const char *bad = w_tunnels_ok(room, t);
    if (bad != NULL) {
        return bad;
    }
    if (turn == 3) {
        return w_through_rock(room, t);
    }
    (*moves)++;
    return w_walk(t[turn % 3]);
}

static void test_wumpus(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("wumpus", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"wumpus", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 9;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("wumpus", why);
        return;
    }
    drain();
    int moves = 0;
    for (int turn = 0; turn < 80; turn++) {
        const char *bad = w_turn(turn, &moves);
        if (bad != NULL) {
            fail("wumpus", bad);
            return;
        }
    }
    if (moves < 20) {
        fail("wumpus", "too few moves were made to tell anything");
    }
    w_type("new"); /* a new game wipes the story: no old line passes for a new one */
    w_type("s 1");
    if (!w_story_has("Missed!") && !w_story_has("got the wumpus") &&
        !w_story_has("arrow got you")) {
        fail("wumpus", "an arrow flew and the story said nothing of it");
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        play(argv[i]);
        if (strstr(argv[i], "lights") != NULL) {
            test_lights(argv[i]);
        }
        if (strstr(argv[i], "twenty48") != NULL) {
            test_twenty48(argv[i]);
        }
        if (strstr(argv[i], "mastermind") != NULL) {
            test_mastermind(argv[i]);
        }
        if (strstr(argv[i], "wumpus") != NULL) {
            test_wumpus(argv[i]);
        }
    }
    if (failures > 0) {
        printf("%d game tests failed\n", failures);
        return 1;
    }
    printf("all %d games played\n", argc - 1);
    return 0;
}
