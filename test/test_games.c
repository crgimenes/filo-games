/* Every game played for a while the way a terminal plays it — time, the
   arrows, the keys it knows, Esc — within the budgets a board gives a
   program, which are the app's own: a game that fits here fits there.
   usage: test_games NAME.fbb... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "paint.h"

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

/* What the games hand the Hall of Fame (report-score): how many results,
   the last one and its track. */
static int sc_count;
static int32_t sc_value;
static char sc_track[PAINT_TRACK_MAX];

static void took_score(app *a, const char *track, int32_t value) {
    (void)a;
    sc_count++;
    sc_value = value;
    (void)snprintf(sc_track, sizeof(sc_track), "%s", track);
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
   each, one apart, the grid centred (25,3 on 80 by 24, room kept for the keys). */
enum { LO_N = 5, LO_V = LO_N * LO_N, LO_X = 25, LO_Y = 3 };

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
    sc_count = 0;
    int clicks = 0;
    for (int i = 0; i < LO_N * LO_N; i++) {
        if (solution[i] != 0) {
            lo_click(i);
            clicks++;
        }
    }
    drain();
    if (sc_count != 1 || sc_value != 1 + clicks || sc_track[0] != '\0') {
        fail("lights", "a dark board does not report its moves, once");
    }
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
    sc_count = 0;
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
    /* the score goes to the Hall of Fame once a game: when no move is left,
       or as the game is left with points on it */
    int score = t_score();
    key("\x1b");
    app_tick(&A, 100);
    drain();
    if (score > 0 && (sc_count != 1 || sc_value != score)) {
        fail("twenty48", "a game does not report its score once");
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
    sc_count = 0;
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
    int e_last = 0;
    int n_last = 0;
    m_answer(M_Y + tries - 1, &e_last, &n_last);
    if (e_last == M_PEGS ? (sc_count != 1 || sc_value != tries) : sc_count != 0) {
        fail("mastermind", "a broken code does not report its tries, or a lost one does");
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

/* Minesweeper read off the screen: 9 by 9 cells three columns wide, from
   (26,7) on 80 by 24; the middle column of a cell says what it is: a
   shade dot closed, F a flag, a digit or a space open, * a mine (shown
   when it is lost). The status line is two rows under the field. */
enum { MS_N = 9, MS_V = MS_N * MS_N, MS_X = 26, MS_Y = 7 };

static uint32_t ms_cell(int i) {
    return A.target.cells[MS_Y + (i / MS_N)][MS_X + ((i % MS_N) * 3) + 1].cp;
}

static void ms_click(int i) {
    char sgr[32];
    (void)snprintf(sgr, sizeof(sgr), "\x1b[<0;%d;%dM", MS_X + ((i % MS_N) * 3) + 2,
                   MS_Y + (i / MS_N) + 1);
    app_input(&A, (const uint8_t *)sgr, strlen(sgr));
    drain();
}

static bool ms_status(const char *text) {
    char line[TERM_COLS_MAX + 1];
    w_row(MS_Y + MS_N + 1, line, sizeof(line));
    return strstr(line, text) != NULL;
}

static bool ms_start(const char *path, uint64_t seed) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("mines", "cannot read the bundle");
        return false;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    static const app_spec spec = {"mines", "test", false, 0, 0, 0, NULL}; /* A keeps it */
    char why[256];
    A.seed = seed;
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("mines", why);
        return false;
    }
    drain();
    return true;
}

static int ms_neighbours(const int mine[MS_V], int i) {
    int n = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int r = (i / MS_N) + dr;
            int c = (i % MS_N) + dc;
            if ((dr != 0 || dc != 0) && r >= 0 && r < MS_N && c >= 0 && c < MS_N) {
                n += mine[(r * MS_N) + c];
            }
        }
    }
    return n;
}

/* Every open cell against the mines the loss shows: its number is its
   neighbours' mines, and an empty one has every neighbour open. */
static const char *ms_check_field(const int mine[MS_V]) {
    for (int i = 0; i < MS_V; i++) {
        uint32_t cp = ms_cell(i);
        if (cp == 0xb7 || cp == 'F' || cp == '*') {
            continue;
        }
        int want = ms_neighbours(mine, i);
        int got = cp == ' ' ? 0 : (int)(cp - '0');
        if (got != want) {
            return "an open cell's number is not the mines around it";
        }
    }
    return NULL;
}

/* The same seed, the same first step: the same field, now cleared round
   flags, every safe cell opened by hand, and the sweep reported. */
static void ms_clear(const char *path, const int mine[MS_V]) {
    if (!ms_start(path, 21)) {
        return;
    }
    key("x"); /* 31, beside the first step, flagged first: the opening goes round it */
    ms_click(31);
    key("x");
    ms_click(40);
    if (ms_cell(31) != 'F' || ms_cell(40) != ' ') {
        fail("mines", "the opening went over a flag");
        return;
    }
    key("x");
    ms_click(31); /* and unflagged: still shut, the opening did not take it */
    key("x");
    if (ms_cell(31) != 0xb7) {
        fail("mines", "the opening went under a flag");
        return;
    }
    int first = 0; /* a mine, flagged by a click after x; a click on it then opens nothing */
    while (mine[first] == 0) {
        first++;
    }
    key("x");
    ms_click(first);
    key("x");
    drain();
    ms_click(first);
    if (ms_cell(first) != 'F' || !ms_status("flags 1") || ms_status("Boom")) {
        fail("mines", "a flag is not kept, or a click opened it");
        return;
    }
    ms_click(40);  /* open already: no step of the sweep */
    int opens = 1; /* the first step's */
    for (int i = 0; i < MS_V; i++) {
        if (mine[i] == 0 && ms_cell(i) == 0xb7) {
            ms_click(i);
            opens++;
        }
    }
    if (!ms_status("Cleared")) {
        fail("mines", "every safe cell open is not a win");
    }
    if (sc_count != 1 || sc_value != opens) {
        fail("mines", "a cleared field does not report the cells opened by hand");
    }
}

static void test_mines(const char *path) {
    if (!ms_start(path, 21)) {
        return;
    }
    sc_count = 0;
    ms_click(40); /* the first step is always safe, and opens its ground */
    if (ms_status("Boom") || ms_cell(40) != ' ') {
        fail("mines", "the first cell opened was a mine, or had one beside it");
        return;
    }
    for (int i = 0; i < MS_V && !ms_status("Boom") && !ms_status("Cleared"); i++) {
        if (ms_cell(i) == 0xb7) {
            ms_click(i);
        }
    }
    if (!ms_status("Boom")) {
        fail("mines", "opening every cell in turn did not hit a mine");
        return;
    }
    int mine[MS_V];
    int mines = 0;
    for (int i = 0; i < MS_V; i++) {
        mine[i] = ms_cell(i) == '*' ? 1 : 0;
        mines += mine[i];
    }
    const char *bad = mines == 10 ? ms_check_field(mine) : "a lost field does not show ten mines";
    if (bad != NULL) {
        fail("mines", bad);
        return;
    }
    if (sc_count != 0) {
        fail("mines", "a field not cleared reports a score");
    }
    ms_clear(path, mine);
}
/* Sokoban read off the screen: two columns a cell, the level's left wall
   the leftmost block, its top row two below the title; walls, goals (two
   dots), boxes ([ ], green on a goal) and the player (@@). Each level is
   solved by a breadth-first search over the player and the boxes, and the
   solution played: every level the game ships has one. */
enum { SK_MAX = 16, SK_BOXES = 7, SK_SLOTS = 1048576 };

typedef struct {
    int h;
    bool wall[SK_MAX * SK_MAX];
    bool goal[SK_MAX * SK_MAX];
    int boxes[SK_BOXES];
    int nboxes;
    int player;
} sk_level;

static uint64_t sk_keys[SK_SLOTS];
static uint32_t sk_parent[SK_SLOTS];
static char sk_move[SK_SLOTS];
static uint64_t sk_queue[SK_SLOTS];

/* where the level starts: the title's row, and the leftmost wall block */
static bool sk_origin(int *title, int *x0) {
    *title = -1;
    *x0 = 1000;
    for (uint16_t y = 0; y < A.target.rows; y++) {
        if (lo_row_has(y, "SOKOBAN")) {
            *title = y;
        }
        for (uint16_t x = 0; x < A.target.cols; x++) {
            if (A.target.cells[y][x].cp == 0x2588 && x < *x0) {
                *x0 = x;
            }
        }
    }
    return *title >= 0;
}

static void sk_cell(sk_level *lv, int i, const cv_cell *cell) {
    if (cell->cp == 0x2588) {
        lv->wall[i] = true;
    }
    if (cell->cp == 0xb7 || (cell->cp == '[' && cell->fg == 46)) {
        lv->goal[i] = true;
    }
    if (cell->cp == '[' && lv->nboxes < SK_BOXES) {
        lv->boxes[lv->nboxes++] = i;
    }
    if (cell->cp == '@') {
        lv->player = i;
    }
}

static bool sk_read(sk_level *lv) {
    memset(lv, 0, sizeof(*lv));
    int title = 0;
    int x0 = 0;
    if (!sk_origin(&title, &x0)) {
        return false;
    }
    int y0 = title + 2;
    for (int r = 0; r < SK_MAX && !lo_row_has(y0 + r, "level "); r++) {
        for (int c = 0; c < SK_MAX && x0 + (2 * c) < A.target.cols; c++) {
            sk_cell(lv, (r * SK_MAX) + c, &A.target.cells[y0 + r][x0 + (2 * c)]);
        }
        lv->h = r + 1;
    }
    return lv->nboxes > 0;
}

/* a state as one number: the player, then the boxes in order, 8 bits each */
static uint64_t sk_key(int player, const int *boxes, int n) {
    int b[SK_BOXES];
    memcpy(b, boxes, sizeof(int) * (size_t)n);
    for (int i = 1; i < n; i++) {
        for (int j = i; j > 0 && b[j - 1] > b[j]; j--) {
            int t = b[j];
            b[j] = b[j - 1];
            b[j - 1] = t;
        }
    }
    uint64_t k = (uint64_t)player;
    for (int i = 0; i < n; i++) {
        k = (k << 8U) | (uint64_t)b[i];
    }
    return k + 1; /* 0 is an empty slot */
}

static uint32_t sk_slot(uint64_t state) {
    uint32_t h = (uint32_t)((state * 0x9E3779B97F4A7C15ULL) >> 44U);
    while (sk_keys[h] != 0 && sk_keys[h] != state) {
        h = (h + 1U) % (uint32_t)SK_SLOTS;
    }
    return h;
}

static void sk_unpack(uint64_t state, int n, int *player, int *boxes) {
    state -= 1;
    for (int i = n - 1; i >= 0; i--) {
        boxes[i] = (int)(state & 0xFFU);
        state >>= 8U;
    }
    *player = (int)state;
}

static bool sk_done(const sk_level *lv, const int *boxes) {
    for (int i = 0; i < lv->nboxes; i++) {
        if (!lv->goal[boxes[i]]) {
            return false;
        }
    }
    return true;
}

/* One step d (0 up, 1 down, 2 left, 3 right) from a state: the boxes after
   it in nb and the player's cell, or -1 when a wall or a second box stops
   it. */
static int sk_step(const sk_level *lv, int player, const int *boxes, int d, int *nb) {
    static const int delta[4] = {-SK_MAX, SK_MAX, -1, 1};
    int next = player + delta[d];
    if (lv->wall[next]) {
        return -1;
    }
    memcpy(nb, boxes, sizeof(int) * (size_t)lv->nboxes);
    for (int i = 0; i < lv->nboxes; i++) {
        if (nb[i] != next) {
            continue;
        }
        int beyond = next + delta[d];
        if (lv->wall[beyond]) {
            return -1;
        }
        for (int j = 0; j < lv->nboxes; j++) {
            if (nb[j] == beyond) {
                return -1;
            }
        }
        nb[i] = beyond;
    }
    return next;
}

/* the moves that led to a state, from the start, as u d l r */
static void sk_path(uint64_t state, char *out, size_t cap) {
    char rev[512];
    size_t n = 0;
    uint32_t at = sk_slot(state);
    while (sk_parent[at] != UINT32_MAX && n + 1 < sizeof(rev)) {
        rev[n++] = sk_move[at];
        at = sk_parent[at];
    }
    for (size_t i = 0; i < n && i + 1 < cap; i++) {
        out[i] = rev[n - 1 - i];
        out[i + 1] = '\0';
    }
}

/* the moves (u d l r) from the start to every box on a goal, or "" */
static void sk_solve(const sk_level *lv, char *out, size_t cap) {
    static const char names[4] = {'u', 'd', 'l', 'r'};
    memset(sk_keys, 0, sizeof(sk_keys));
    out[0] = '\0';
    uint64_t start = sk_key(lv->player, lv->boxes, lv->nboxes);
    uint32_t head = 0;
    uint32_t tail = 0;
    uint32_t s0 = sk_slot(start);
    sk_keys[s0] = start;
    sk_parent[s0] = UINT32_MAX;
    sk_queue[tail++] = start;
    while (head < tail) {
        uint64_t state = sk_queue[head++];
        int player = 0;
        int boxes[SK_BOXES];
        sk_unpack(state, lv->nboxes, &player, boxes);
        if (sk_done(lv, boxes)) {
            sk_path(state, out, cap);
            return;
        }
        for (int d = 0; d < 4 && tail < SK_SLOTS; d++) {
            int nb[SK_BOXES];
            int next = sk_step(lv, player, boxes, d, nb);
            uint64_t k = next < 0 ? 0 : sk_key(next, nb, lv->nboxes);
            uint32_t slot = next < 0 ? 0 : sk_slot(k);
            if (next < 0 || sk_keys[slot] == k) {
                continue;
            }
            sk_keys[slot] = k;
            sk_parent[slot] = sk_slot(state);
            sk_move[slot] = names[d];
            sk_queue[tail++] = k;
        }
    }
}

static void sk_play(const char *moves) {
    for (const char *m = moves; *m != '\0'; m++) {
        switch (*m) {
        case 'u':
            key("\x1b[A");
            break;
        case 'd':
            key("\x1b[B");
            break;
        case 'l':
            key("\x1b[D");
            break;
        default:
            key("\x1b[C");
            break;
        }
    }
    drain();
}

static bool sk_status(const char *text) {
    for (uint16_t y = 0; y < A.target.rows; y++) {
        if (lo_row_has(y, text)) {
            return true;
        }
    }
    return false;
}

static void test_sokoban(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("sokoban", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"sokoban", "test", false, 0, 0, 0, NULL};
    char why[256];
    if (!app_start(&A, &spec, fbb, len, 80, 24, why, sizeof(why))) {
        fail("sokoban", why);
        return;
    }
    drain();
    /* level 1: a wall stops the player, a step counts, u takes it back */
    key("\x1b[D");
    drain();
    if (!sk_status("moves 0")) {
        fail("sokoban", "the player walked into a wall");
    }
    key("\x1b[C");
    drain();
    bool counted = sk_status("moves 1 ");
    key("u");
    drain();
    if (!counted || !sk_status("moves 0")) {
        fail("sokoban", "a step is not counted, or u does not take it back");
    }
    for (int level = 1; level <= 6; level++) {
        sk_level lv;
        char moves[512];
        if (!sk_read(&lv)) {
            fail("sokoban", "a level cannot be read off the screen");
            return;
        }
        sk_solve(&lv, moves, sizeof(moves));
        if (moves[0] == '\0') {
            fail("sokoban", "a level has no solution");
            return;
        }
        sc_count = 0;
        sk_play(moves);
        if (!sk_status("Solved in")) {
            fail("sokoban", "the solution's moves did not solve the level");
            return;
        }
        char track[4];
        (void)snprintf(track, sizeof(track), "%d", level);
        if (sc_count != 1 || sc_value != (int32_t)strlen(moves) || strcmp(sc_track, track) != 0) {
            fail("sokoban", "a solved level does not report its moves on its track");
        }
        key("n");
        drain();
    }
    if (!sk_status("level 1/6")) {
        fail("sokoban", "n after the last level is not the first");
    }
}

/* Tic-tac-toe read off the screen: on 80 by 24 a square's mark sits at
   (7 + 4r, 33 + 6c), the status two rows under the grid. One player: the
   machine is played every game a person could play, from the first move
   and from the second, by replaying the moves into a fresh start (the same
   seed: the machine's picks between equal moves come out the same); it
   must never lose. No players: every game a draw, then the war, then the
   last words. */
enum { TT_Y = 7, TT_X = 33, TT_PATHS_MAX = 4096 };

static const uint8_t *tt_fbb;
static size_t tt_len;
static int tt_games;

static bool tt_start(bool machine_first) {
    static const app_spec spec = {"tictactoe", "test", false, 0, 0, 0, NULL}; /* A keeps it */
    char why[256];
    A.seed = 13;
    if (!app_start(&A, &spec, tt_fbb, tt_len, 80, 24, why, sizeof(why))) {
        fail("tictactoe", why);
        return false;
    }
    key("1");
    if (machine_first) {
        key("r");
    }
    app_tick(&A, 400); /* the machine's move, when it is its turn */
    drain();
    return true;
}

static int tt_free(int i) {
    uint32_t cp = A.target.cells[TT_Y + (4 * (i / 3))][TT_X + (6 * (i % 3))].cp;
    return (cp != 'X' && cp != 'O') ? 1 : 0;
}

/* every way the person can go on from these moves; false on a failure */
static bool tt_walk(bool machine_first, const int *moves, int n) {
    if (!tt_start(machine_first)) {
        return false;
    }
    for (int k = 0; k < n; k++) {
        const char d[2] = {(char)('1' + moves[k]), '\0'};
        key(d);
        app_tick(&A, 400);
        drain();
    }
    if (A.error[0] != '\0') {
        fail("tictactoe", A.error);
        return false;
    }
    if (sk_status("you win")) {
        fail("tictactoe", "a person beat the machine");
        return false;
    }
    if (sk_status("space plays again")) {
        tt_games++;
        return true;
    }
    int next[9];
    for (int k = 0; k < n; k++) {
        next[k] = moves[k];
    }
    int open = 0;
    for (int i = 0; i < 9; i++) {
        if (tt_free(i) != 0) {
            open++;
            next[n] = i;
            if (!tt_walk(machine_first, next, n + 1)) {
                return false;
            }
        }
    }
    if (open == 0) {
        fail("tictactoe", "a full board that is not over");
        return false;
    }
    return true;
}

static void tt_wopr(void) {
    static const app_spec spec = {"tictactoe", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 21;
    if (!app_start(&A, &spec, tt_fbb, tt_len, 80, 24, why, sizeof(why))) {
        fail("tictactoe", why);
        return;
    }
    key("0");
    int war = 0;
    int games = 0;
    for (int t = 0; t < 120000 && !sk_status("NOT TO PLAY."); t += 40) {
        app_tick(&A, 40);
        drain();
        if (A.error[0] != '\0') {
            fail("tictactoe", A.error);
            return;
        }
        if (sk_status("WINNER: X") || sk_status("WINNER: O")) {
            fail("tictactoe", "the machine beat itself");
            return;
        }
        if (sk_status("GAMES PLAYED 35 ")) {
            games = 1;
        }
        if (sk_status("GLOBAL THERMONUCLEAR WAR")) {
            war = 1;
        }
    }
    if (games == 0 || war == 0 || !sk_status("GREETINGS, PROFESSOR FALKEN.") ||
        !sk_status("THE ONLY WINNING MOVE IS NOT TO PLAY.")) {
        fail("tictactoe", "no players: the draws, the war or the last words did not come");
    }
}

static void test_tictactoe(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("tictactoe", "cannot read the bundle");
        return;
    }
    tt_len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    tt_fbb = fbb;
    tt_games = 0;
    if (!tt_walk(false, NULL, 0)) {
        return;
    }
    int first = tt_games;
    if (!tt_walk(true, NULL, 0)) {
        return;
    }
    /* the machine's answers prune the tree: 87 games from the first move
       and 56 from the second at this seed */
    if (first < 50 || tt_games - first < 20) {
        fail("tictactoe", "too few games were played to tell anything");
    }
    tt_wopr();
}

/* A phone's screen, 35 by 20: the keys under a game break onto rows
   rather than run off the edge (their last word, esc, is on screen), and
   the wumpus's story breaks at the width (the tunnels' numbers show). */
static void test_narrow(const char *path, const char *name) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail(name, "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {name, "test", false, 0, 0, 0, NULL};
    char why[256];
    if (!app_start(&A, &spec, fbb, len, 35, 20, why, sizeof(why))) {
        fail(name, why);
        return;
    }
    drain();
    if (strcmp(name, "wumpus") == 0) {
        for (uint16_t y = 0; y + 1 < A.target.rows; y++) {
            if (lo_row_has(y, "Tunnels lead to") &&
                !(A.target.cells[y + 1][0].cp >= '0' && A.target.cells[y + 1][0].cp <= '9')) {
                fail(name, "the tunnels' numbers run off a narrow screen");
            }
        }
        return;
    }
    if (!sk_status("esc")) {
        fail(name, "the keys run off a narrow screen");
    }
}

/* Life read off the screen at 80 by 24: the world is the whole width and 22
   text rows of two cells each, and the line under it says the generation,
   or that the world is being sown. Each generation shown must be the one
   before it stepped by the rule, on a torus, which this test works out on
   its own. */
enum { LF_W = 80, LF_H = 44, LF_ROWS = LF_H / 2 };

static void lf_read(uint8_t g[LF_H][LF_W]) {
    for (int r = 0, y = 0; r < LF_ROWS; r++, y += 2) {
        for (int c = 0; c < LF_W; c++) {
            const cv_cell *cell = &A.target.cells[r][c];
            g[y][c] = cell->cp == 0x2580 ? 1 : 0; /* ▀ */
            g[y + 1][c] =
                (cell->cp == 0x2580 && cell->bg != CV_COLOR_DEFAULT) || cell->cp == 0x2584 /* ▄ */
                    ? 1
                    : 0;
        }
    }
}

static void lf_step(uint8_t g[LF_H][LF_W], uint8_t out[LF_H][LF_W]) {
    for (int y = 0; y < LF_H; y++) {
        for (int x = 0; x < LF_W; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dy != 0 || dx != 0) {
                        n += g[(y + dy + LF_H) % LF_H][(x + dx + LF_W) % LF_W];
                    }
                }
            }
            out[y][x] = (n == 3 || (n == 2 && g[y][x] != 0)) ? 1 : 0;
        }
    }
}

static int lf_generation(void) {
    char line[TERM_COLS_MAX + 1];
    size_t n = 0;
    for (uint16_t c = 0; c < A.target.cols; c++) {
        uint32_t cp = A.target.cells[LF_ROWS][c].cp;
        line[n++] = cp > 0 && cp < 0x80 ? (char)cp : ' ';
    }
    line[n] = '\0';
    if (strstr(line, "sowing") != NULL) {
        return -1;
    }
    const char *at = strstr(line, "generation ");
    if (at == NULL) {
        return -2;
    }
    char *end = NULL;
    long gen = strtol(at + strlen("generation "), &end, 10);
    if (end == at + strlen("generation ") || gen < 0 || gen > INT32_MAX) {
        return -2;
    }
    return (int)gen;
}

static void test_life(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fail("life", "cannot read the bundle");
        return;
    }
    size_t len = fread(fbb, 1, sizeof(fbb), f);
    (void)fclose(f);
    app_spec spec = {"life", "test", false, 0, 0, 0, NULL};
    char why[256];
    A.seed = 7;
    if (!app_start(&A, &spec, fbb, len, LF_W, LF_ROWS + 2, why, sizeof(why))) {
        fail("life", why);
        return;
    }
    static uint8_t was[LF_H][LF_W];
    static uint8_t now[LF_H][LF_W];
    static uint8_t want[LF_H][LF_W];
    int last = -1;
    int checked = 0;
    for (int i = 0; i < 2000 && checked < 100; i++) {
        app_tick(&A, 20);
        drain();
        int gen = lf_generation();
        if (A.error[0] != '\0' || gen == -2) {
            fail("life", A.error[0] != '\0' ? A.error : "no generation under the world");
            return;
        }
        if (gen == last) {
            continue;
        }
        lf_read(now);
        if (last >= 0 && gen == last + 1) {
            lf_step(was, want);
            if (memcmp(now, want, sizeof(now)) != 0) {
                char what[64];
                (void)snprintf(what, sizeof(what), "generation %d does not follow the rule", gen);
                fail("life", what);
                return;
            }
            checked++;
        }
        memcpy(was, now, sizeof(was));
        last = gen;
    }
    if (checked < 100) {
        fail("life", "fewer than 100 generations seen");
    }
}

int main(int argc, char **argv) {
    A.score = took_score;
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
        if (strstr(argv[i], "mines") != NULL) {
            test_mines(argv[i]);
        }
        if (strstr(argv[i], "sokoban") != NULL) {
            test_sokoban(argv[i]);
        }
        if (strstr(argv[i], "tictactoe") != NULL) {
            test_tictactoe(argv[i]);
        }
        if (strstr(argv[i], "life") != NULL) {
            test_life(argv[i]);
        }
        static const char *const narrow[] = {
            "lights", "twenty48", "mastermind", "mines", "sokoban", "wumpus", "tictactoe",
        };
        for (size_t k = 0; k < sizeof(narrow) / sizeof(narrow[0]); k++) {
            if (strstr(argv[i], narrow[k]) != NULL) {
                test_narrow(argv[i], narrow[k]);
            }
        }
    }
    if (failures > 0) {
        printf("%d game tests failed\n", failures);
        return 1;
    }
    printf("all %d games played\n", argc - 1);
    return 0;
}
