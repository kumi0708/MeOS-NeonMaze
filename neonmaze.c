/*
 * Neon Maze - an original maze-chase game for MeOS.
 *
 * Arrows / WASD choose a direction, Space starts or restarts, Q quits.
 * Eat every spark, use the large power cores to turn the hunters vulnerable.
 */
#include "meos.h"
#include "meos_util.h"

#define SCREEN_W 1280
#define SCREEN_H 720
#define MAP_W 21
#define MAP_H 17
#define TILE 32
#define BOARD_X 48
#define BOARD_Y 88
#define HUD_X 760
#define PLAYER_STEP 4
#define POWER_TIME 180

enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };
enum { STATE_TITLE, STATE_PLAY, STATE_WIN, STATE_OVER };

static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

static const char INITIAL_MAP[MAP_H][MAP_W + 1] = {
    "#####################",
    "#o........#........o#",
    "#.###.###.#.###.###.#",
    "#...................#",
    "#.###.#.#####.#.###.#",
    "#.....#...#...#.....#",
    "#####.###.#.###.#####",
    "#.........P.........#",
    "#.###.#.#####.#.###.#",
    "#...#.#...G...#.#...#",
    "###.#.###...###.#.###",
    "#...#.....#.....#...#",
    "#.#####.#.#.#.#####.#",
    "#.......#...#.......#",
    "#.###.###.#.###.###.#",
    "#o.................o#",
    "#####################"
};

static char map_data[MAP_H][MAP_W + 1];

static int player_x, player_y, player_dir, wanted_dir;
static int hunter_x[4], hunter_y[4], hunter_dir[4];
static int hunter_home_x[4] = { 9, 10, 11, 12 };
static int hunter_home_y[4] = { 9, 9, 9, 9 };
static unsigned score;
static int lives, sparks_left, power_left, game_state, pause_left, frame_no;

static const unsigned HUNTER_COLORS[4] = {
    MEOS_RGB(255, 70, 100),
    MEOS_RGB(70, 225, 255),
    MEOS_RGB(255, 130, 230),
    MEOS_RGB(255, 155, 55)
};

static void text_at(int x, int y, const char *s, int scale, unsigned color)
{
    meos_display_text(x, y, s, meos_strlen(s), color, 0, scale);
}

static int absolute(int n) { return n < 0 ? -n : n; }

static int is_open(int x, int y)
{
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H && map_data[y][x] != '#';
}

static int can_move(int x, int y, int dir)
{
    return is_open(x + DX[dir], y + DY[dir]);
}

static void reset_positions(void)
{
    int i;
    player_x = 10;
    player_y = 7;
    player_dir = DIR_LEFT;
    wanted_dir = DIR_LEFT;
    for (i = 0; i < 4; i++) {
        hunter_x[i] = hunter_home_x[i];
        hunter_y[i] = hunter_home_y[i];
        hunter_dir[i] = (i & 1) ? DIR_RIGHT : DIR_LEFT;
    }
}

static void reset_game(void)
{
    int x, y;
    sparks_left = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x <= MAP_W; x++) map_data[y][x] = INITIAL_MAP[y][x];
        for (x = 0; x < MAP_W; x++) {
            if (map_data[y][x] == '.' || map_data[y][x] == 'o') sparks_left++;
            if (map_data[y][x] == 'P' || map_data[y][x] == 'G') map_data[y][x] = ' ';
        }
    }
    score = 0;
    lives = 3;
    power_left = 0;
    pause_left = 0;
    frame_no = 0;
    reset_positions();
}

static void start_play(void)
{
    if (game_state == STATE_WIN || game_state == STATE_OVER) reset_game();
    game_state = STATE_PLAY;
    pause_left = 30;
}

static void eat_spark(void)
{
    char *cell = &map_data[player_y][player_x];
    if (*cell == '.') {
        *cell = ' ';
        score += 10;
        sparks_left--;
    } else if (*cell == 'o') {
        *cell = ' ';
        score += 50;
        sparks_left--;
        power_left = POWER_TIME;
    }
    if (sparks_left <= 0) game_state = STATE_WIN;
}

static void move_player(void)
{
    if (can_move(player_x, player_y, wanted_dir)) player_dir = wanted_dir;
    if (can_move(player_x, player_y, player_dir)) {
        player_x += DX[player_dir];
        player_y += DY[player_dir];
        eat_spark();
    }
}

static int target_distance(int id, int dir)
{
    int tx = player_x;
    int ty = player_y;
    int nx = hunter_x[id] + DX[dir];
    int ny = hunter_y[id] + DY[dir];

    if (id == 1) {
        tx += DX[player_dir] * 3;
        ty += DY[player_dir] * 3;
    } else if (id == 2) {
        tx = MAP_W - 1 - player_x;
        ty = player_y;
    } else if (id == 3 && absolute(player_x - hunter_x[id]) + absolute(player_y - hunter_y[id]) < 6) {
        tx = 1;
        ty = MAP_H - 2;
    }
    return absolute(tx - nx) + absolute(ty - ny);
}

static void move_hunter(int id)
{
    int options[4];
    int count = 0;
    int dir, best = -1, best_value = 10000;
    int reverse = (hunter_dir[id] + 2) & 3;

    for (dir = 0; dir < 4; dir++) {
        if (dir != reverse && can_move(hunter_x[id], hunter_y[id], dir)) options[count++] = dir;
    }
    if (count == 0 && can_move(hunter_x[id], hunter_y[id], reverse)) options[count++] = reverse;
    if (count == 0) return;

    if (power_left > 0) {
        best = options[meos_rand() % (unsigned)count];
    } else {
        int i;
        for (i = 0; i < count; i++) {
            int value = target_distance(id, options[i]);
            if (value < best_value || (value == best_value && (meos_rand() & 1u))) {
                best_value = value;
                best = options[i];
            }
        }
    }
    hunter_dir[id] = best;
    hunter_x[id] += DX[best];
    hunter_y[id] += DY[best];
}

static void lose_life(void)
{
    lives--;
    power_left = 0;
    if (lives <= 0) {
        game_state = STATE_OVER;
    } else {
        reset_positions();
        pause_left = 35;
    }
}

static int resolve_collision(int id, int old_px, int old_py, int old_hx, int old_hy)
{
    int same = player_x == hunter_x[id] && player_y == hunter_y[id];
    int crossed = player_x == old_hx && player_y == old_hy &&
                  hunter_x[id] == old_px && hunter_y[id] == old_py;
    if (!same && !crossed) return 0;
    if (power_left > 0) {
        score += 200;
        hunter_x[id] = hunter_home_x[id];
        hunter_y[id] = hunter_home_y[id];
        hunter_dir[id] = DIR_UP;
        return 0;
    }
    lose_life();
    return 1;
}

static void update_game(void)
{
    int i;
    int old_px = player_x, old_py = player_y;
    int old_hx[4], old_hy[4];

    if (game_state != STATE_PLAY) return;
    if (pause_left > 0) {
        pause_left--;
        return;
    }
    if (power_left > 0) power_left--;
    for (i = 0; i < 4; i++) {
        old_hx[i] = hunter_x[i];
        old_hy[i] = hunter_y[i];
    }

    if (frame_no % PLAYER_STEP == 0) move_player();
    for (i = 0; i < 4; i++) {
        int period = power_left > 0 ? 9 : 6 + (i & 1);
        if ((frame_no + i) % period == 0) move_hunter(i);
        if (resolve_collision(i, old_px, old_py, old_hx[i], old_hy[i])) return;
    }
}

static void draw_wall(int sx, int sy)
{
    unsigned edge = MEOS_RGB(30, 110, 255);
    unsigned core = MEOS_RGB(12, 35, 100);
    meos_display_fill(sx + 1, sy + 1, TILE - 2, TILE - 2, edge);
    meos_display_fill(sx + 4, sy + 4, TILE - 8, TILE - 8, core);
}

static void draw_board(void)
{
    int x, y;
    unsigned floor = MEOS_RGB(2, 5, 18);
    unsigned spark = MEOS_RGB(255, 220, 135);
    meos_display_fill(BOARD_X - 8, BOARD_Y - 8, MAP_W * TILE + 16, MAP_H * TILE + 16, MEOS_RGB(5, 20, 55));
    meos_display_fill(BOARD_X, BOARD_Y, MAP_W * TILE, MAP_H * TILE, floor);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int sx = BOARD_X + x * TILE;
            int sy = BOARD_Y + y * TILE;
            char cell = map_data[y][x];
            if (cell == '#') draw_wall(sx, sy);
            else if (cell == '.') meos_display_fill(sx + 14, sy + 14, 4, 4, spark);
            else if (cell == 'o') {
                unsigned pulse = ((frame_no / 5) & 1) ? MEOS_RGB(255, 255, 255) : MEOS_RGB(255, 150, 255);
                meos_display_fill(sx + 10, sy + 10, 12, 12, pulse);
            }
        }
    }
}

static void draw_player_at(int gx, int gy, int dir, int small)
{
    int sx = BOARD_X + gx * TILE;
    int sy = BOARD_Y + gy * TILE;
    unsigned yellow = MEOS_RGB(255, 225, 35);
    unsigned floor = MEOS_RGB(2, 5, 18);
    int mouth = ((frame_no / 3) & 1) ? 7 : 3;

    if (small) {
        meos_display_fill(sx + 9, sy + 9, 14, 14, yellow);
        return;
    }
    meos_display_fill(sx + 5, sy + 3, 22, 26, yellow);
    meos_display_fill(sx + 3, sy + 7, 26, 18, yellow);
    if (dir == DIR_RIGHT) meos_display_tri(sx + 29, sy + 16, sx + 18, sy + 16 - mouth, sx + 18, sy + 16 + mouth, floor);
    else if (dir == DIR_LEFT) meos_display_tri(sx + 3, sy + 16, sx + 14, sy + 16 - mouth, sx + 14, sy + 16 + mouth, floor);
    else if (dir == DIR_UP) meos_display_tri(sx + 16, sy + 3, sx + 16 - mouth, sy + 14, sx + 16 + mouth, sy + 14, floor);
    else meos_display_tri(sx + 16, sy + 29, sx + 16 - mouth, sy + 18, sx + 16 + mouth, sy + 18, floor);
}

static void draw_hunter(int id)
{
    int sx = BOARD_X + hunter_x[id] * TILE;
    int sy = BOARD_Y + hunter_y[id] * TILE;
    unsigned body = HUNTER_COLORS[id];
    unsigned white = MEOS_RGB(245, 250, 255);
    unsigned pupil = MEOS_RGB(15, 35, 80);
    if (power_left > 0) {
        if (power_left < 45 && ((frame_no / 4) & 1)) body = MEOS_RGB(235, 235, 255);
        else body = MEOS_RGB(45, 70, 230);
    }
    meos_display_fill(sx + 5, sy + 8, 22, 19, body);
    meos_display_fill(sx + 8, sy + 5, 16, 5, body);
    meos_display_fill(sx + 5, sy + 26, 6, 3, body);
    meos_display_fill(sx + 14, sy + 26, 5, 3, body);
    meos_display_fill(sx + 22, sy + 26, 5, 3, body);
    meos_display_fill(sx + 8, sy + 11, 6, 7, white);
    meos_display_fill(sx + 18, sy + 11, 6, 7, white);
    meos_display_fill(sx + 10 + DX[hunter_dir[id]], sy + 14 + DY[hunter_dir[id]], 3, 3, pupil);
    meos_display_fill(sx + 20 + DX[hunter_dir[id]], sy + 14 + DY[hunter_dir[id]], 3, 3, pupil);
}

static void draw_hud(void)
{
    int i;
    unsigned cyan = MEOS_RGB(70, 230, 255);
    unsigned white = MEOS_RGB(235, 245, 255);
    unsigned pink = MEOS_RGB(255, 95, 205);
    meos_display_fill(HUD_X - 24, 0, SCREEN_W - HUD_X + 24, SCREEN_H, MEOS_RGB(2, 5, 18));
    text_at(HUD_X, 74, "NEON MAZE", 3, cyan);
    text_at(HUD_X, 112, "SPARK RUNNER", 2, pink);
    hud(HUD_X, 170, "SCORE  ", score, white);
    hud(HUD_X, 208, "SPARKS ", (unsigned)sparks_left, white);
    text_at(HUD_X, 260, "LIVES", 2, cyan);
    for (i = 0; i < lives; i++) {
        int x = HUD_X + i * 42;
        meos_display_fill(x, 298, 26, 18, MEOS_RGB(255, 225, 35));
        meos_display_tri(x + 26, 307, x + 17, 302, x + 17, 312, MEOS_RGB(2, 5, 18));
    }
    if (power_left > 0) {
        int w = power_left * 360 / POWER_TIME;
        text_at(HUD_X, 350, "POWER", 2, MEOS_RGB(120, 150, 255));
        meos_display_fill(HUD_X, 384, 360, 16, MEOS_RGB(20, 25, 60));
        meos_display_fill(HUD_X, 384, w, 16, MEOS_RGB(60, 90, 255));
    }
    text_at(HUD_X, 470, "ARROWS / WASD", 2, white);
    text_at(HUD_X, 504, "MOVE", 2, white);
    text_at(HUD_X, 552, "SPACE  START", 2, white);
    text_at(HUD_X, 586, "Q      QUIT", 2, white);

    if (game_state == STATE_TITLE) {
        text_at(HUD_X, 642, "PRESS SPACE", 2, MEOS_RGB(255, 225, 35));
    } else if (game_state == STATE_WIN) {
        text_at(HUD_X, 642, "MAZE CLEARED!", 2, MEOS_RGB(90, 255, 150));
    } else if (game_state == STATE_OVER) {
        text_at(HUD_X, 642, "RUN ENDED", 2, MEOS_RGB(255, 80, 100));
    } else if (pause_left > 0) {
        text_at(HUD_X, 642, "GET READY!", 2, MEOS_RGB(255, 225, 35));
    }
}

static void draw_game(void)
{
    int i;
    meos_display_fill(0, 0, SCREEN_W, SCREEN_H, MEOS_RGB(2, 5, 18));
    draw_board();
    draw_player_at(player_x, player_y, player_dir, 0);
    for (i = 0; i < 4; i++) draw_hunter(i);
    draw_hud();
}

static void handle_key(int key)
{
    if (key == 'q' || key == 'Q') {
        meos_display_close();
        game_state = -1;
        return;
    }
    if (key == MEOS_KEY_UP || key == 'w' || key == 'W') wanted_dir = DIR_UP;
    else if (key == MEOS_KEY_RIGHT || key == 'd' || key == 'D') wanted_dir = DIR_RIGHT;
    else if (key == MEOS_KEY_DOWN || key == 's' || key == 'S') wanted_dir = DIR_DOWN;
    else if (key == MEOS_KEY_LEFT || key == 'a' || key == 'A') wanted_dir = DIR_LEFT;
    else if (key == ' ' || key == '\r') {
        if (game_state != STATE_PLAY) start_play();
    }
}

void _start(void)
{
    unsigned last;
    if (meos_display_open() < 0) {
        meos_puts("neonmaze needs display permission\n");
        return;
    }
    reset_game();
    game_state = STATE_TITLE;
    last = meos_uptime_ms();

    for (;;) {
        int key;
        while ((key = meos_getkey()) >= 0) {
            handle_key(key);
            if (game_state < 0) return;
        }
        update_game();
        draw_game();
        frame_no++;
        frame_wait(&last, 50);
    }
}
