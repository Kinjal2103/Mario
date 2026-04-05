/* ========================================================================
   mario_game.c  —  CPulator Nios II / ARM Cortex Mario-style platformer (single file)
   Target: CPulator simulator  (320x240 pixel buffer, PS/2 keyboard, timer)
   ======================================================================== */

/* ── Hardware addresses (CPulator DE1-SoC / DE10-Standard mapping) ─────── */
#include <stdint.h>

#define PIXEL_BUF_CTRL    ((volatile uint32_t *)0xFF203020)
#define PIXEL_BUFFER_BASE  0x08000000
#define PIXEL_BUFFER_BACK  0x09000000
#define TIMER_BASE        ((volatile uint32_t *)0xFF202000)
#define PS2_BASE          ((volatile uint32_t *)0xFF200100)
#define HEX_BASE          ((volatile uint32_t *)0xFF200020)

/* ── Display ────────────────────────────────────────────────────────────── */
#define SCREEN_W  320
#define SCREEN_H  240
/* CPulator pixel buffer stride is 512 pixels wide (each pixel is RGB565) */
#define FB_W      512

/* ── Physics constants ──────────────────────────────────────────────────── */
#define GRAVITY          1          /* pixels per frame² (scaled ×16) */
#define JUMP_VEL        -18         /* initial jump velocity (scaled ×16) */
#define MOVE_SPEED       2
#define MAX_FALL_SPEED   12
#define FIXED_SHIFT      4          /* 4-bit fixed point shift */

/* ── Tile definitions ───────────────────────────────────────────────────── */
#define TILE_W           16
#define TILE_H           16
#define MAP_COLS        100
#define MAP_ROWS         15

#define TILE_AIR         0
#define TILE_GROUND      1
#define TILE_BRICK       2
#define TILE_QBLOCK      3
#define TILE_QBLOCK_USED 4
#define TILE_PIPE        5
#define TILE_CLOUD       6
#define TILE_COIN_BLOCK  7
#define TILE_GOAL        8
#define TILE_PLATFORM    9

/* ── Colours (RGB565) ───────────────────────────────────────────────────── */
#define COL(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

#define C_SKY      COL(92,148,252)
#define C_BLACK    COL(0,0,0)
#define C_WHITE    COL(255,255,255)
#define C_RED      COL(220,50,50)
#define C_DKRED    COL(150,20,20)
#define C_BROWN    COL(130,70,30)
#define C_DKBROWN  COL(90,45,15)
#define C_GREEN    COL(50,200,50)
#define C_DKGREEN  COL(20,130,20)
#define C_YELLOW   COL(255,220,0)
#define C_ORANGE   COL(255,140,0)
#define C_TAN      COL(240,200,140)
#define C_GRAY     COL(150,150,150)
#define C_LTGRAY   COL(210,210,210)
#define C_PIPE_G   COL(60,180,60)
#define C_PIPE_DK  COL(20,120,20)
#define C_COIN     COL(255,200,0)
#define C_CLOUD    COL(245,245,255)
#define C_MUSHROOM COL(220,60,60)
#define C_STAR     COL(255,230,50)
#define C_FLAGPOLE COL(180,180,180)

/* ── Sprite IDs ─────────────────────────────────────────────────────────── */
#define SPR_MARIO_STAND    0
#define SPR_MARIO_WALK1    1
#define SPR_MARIO_WALK2    2
#define SPR_MARIO_JUMP     3
#define SPR_MARIO_DEAD     4
#define SPR_GOOMBA1        5
#define SPR_GOOMBA2        6
#define SPR_GOOMBA_FLAT    7
#define SPR_KOOPA1         8
#define SPR_KOOPA2         9
#define SPR_KOOPA_SHELL    10
#define SPR_COIN_SPIN1     11
#define SPR_COIN_SPIN2     12
#define SPR_MUSHROOM       13
#define SPR_STAR           14
#define SPR_FIREBALL       15
#define SPR_COUNT          16

/* ── Entity types ───────────────────────────────────────────────────────── */
typedef enum {
    ENT_NONE = 0,
    ENT_GOOMBA,
    ENT_KOOPA,
    ENT_KOOPA_SHELL,
    ENT_COIN,
    ENT_MUSHROOM,
    ENT_STAR,
    ENT_FIREBALL,
    ENT_PARTICLE
} EntityType;

/* ── Player states ──────────────────────────────────────────────────────── */
typedef enum {
    PS_STANDING,
    PS_WALKING,
    PS_JUMPING,
    PS_FALLING,
    PS_DEAD,
    PS_WIN
} PlayerState;

/* ── Game states ────────────────────────────────────────────────────────── */
typedef enum {
    GS_TITLE,
    GS_PLAYING,
    GS_PAUSED,
    GS_DEAD,
    GS_WIN,
    GS_GAMEOVER
} GameState;

/* ── Power-up states ────────────────────────────────────────────────────── */
typedef enum {
    PU_SMALL,
    PU_BIG,
    PU_FIRE
} PowerUp;

/* ── Structs ─────────────────────────────────────────────────────────────── */
typedef struct {
    int x, y;          /* position (pixel) */
    int vx, vy;        /* velocity (fixed-point ×16) */
    int w, h;          /* hitbox size */
    int frame;         /* animation frame */
    int frame_timer;
    int facing;        /* 1=right, -1=left */
    int on_ground;
    int invincible;    /* invincibility frames */
    int star_timer;
    PlayerState state;
    PowerUp power;
    int lives;
    int score;
    int coins;
} Player;

typedef struct {
    int active;
    EntityType type;
    int x, y;
    int vx, vy;
    int w, h;
    int facing;
    int frame;
    int frame_timer;
    int on_ground;
    int dead;
    int squished;      /* goomba squish timer */
    int shell_moving;  /* koopa shell moving flag */
    int bounce_count;  /* particle bounces */
    int color;         /* particle color */
    int lifetime;
} Entity;

typedef struct {
    int hit;           /* times hit (for ? blocks) */
    int anim_timer;    /* bump animation */
    int anim_off;      /* y offset for bump */
} TileMeta;

typedef struct {
    int score;
    int x, y;
    int timer;
} FloatText;

/* ── PS/2 scan codes ────────────────────────────────────────────────────── */
#define KEY_LEFT   0x6B
#define KEY_RIGHT  0x74
#define KEY_UP     0x75
#define KEY_Z      0x1A   /* jump / confirm */
#define KEY_X      0x22   /* run / fire */
#define KEY_ENTER  0x5A
#define KEY_ESC    0x76
#define KEY_P      0x4D   /* pause */

/* ── Max entities ───────────────────────────────────────────────────────── */
#define MAX_ENTITIES 40
#define MAX_FLOAT    8
#define MAX_PARTICLES 20

/* ============================================================================
   mario_game.c  —  Full Mario-style platformer for CPulator (Nios II / ARM)
   
   Features:
     • Horizontal-scrolling tilemap (100×15 tiles, 16×16 each)
     • Player: walk, run, jump, fall, die, win; Small / Big / Fire power-up states
     • Enemies: Goomba (walk, squish), Koopa (walk, shell, kick shell)
     • Items: Coin (collect), Mushroom (grow), Star (invincibility), Fireball
     • Blocks: Ground, Brick (breakable when big), ? Block (coin/item), Pipe
     • Camera: smooth horizontal scrolling clamped to map bounds
     • HUD: score, coins, lives, time countdown
     • Game states: Title → Playing → Paused → Dead → Win → Game Over
     • Double-buffered rendering (swap pixel buffers each frame)
     • PS/2 keyboard input (scan codes, break codes, key state array)
     • Fixed-point physics (×16 shift)
     • Floating score text pop-ups and particle effects
   ============================================================================ */


/* ── Forward declarations ─────────────────────────────────────────────────── */
static void draw_pixel(int x, int y, unsigned short col);
static void draw_rect(int x, int y, int w, int h, unsigned short col);
static void draw_rect_border(int x, int y, int w, int h, unsigned short col, unsigned short border);
static void clear_screen(unsigned short col);
static void swap_buffers(void);
static void wait_vsync(void);
static void draw_char(int x, int y, char c, unsigned short col);
static void draw_string(int x, int y, const char *s, unsigned short col);
static void draw_number(int x, int y, int n, unsigned short col);
static void draw_tile(int tx, int ty, int tile_id);
static void draw_sprite(int x, int y, int spr_id, int flip);
static void draw_player(void);
static void draw_enemies(void);
static void draw_items(void);
static void draw_hud(void);
static void draw_title(void);
static void draw_win_screen(void);
static void draw_gameover_screen(void);
static void draw_pause_overlay(void);

static void update_input(void);
static void update_player(void);
static void update_enemies(void);
static void update_items(void);
static void update_camera(void);
static void update_tiles(void);
static void update_particles(void);
static void update_float_texts(void);

static void player_jump(void);
static void player_die(void);
static void player_grow(void);
static void player_shrink(void);
static void player_fire_powerup(void);
static void spawn_entity(EntityType t, int x, int y);
static void spawn_particle(int x, int y, int vx, int vy, int col, int life);
static void spawn_fireball(int x, int y, int dir);
static void add_score(int pts, int x, int y);
static void add_coin(void);
static void hit_block(int tx, int ty);
static void break_block(int tx, int ty);
static int  tile_solid(int tile_id);
static int  tile_passthrough(int tile_id);

static int  player_tile_col(int px, int py, int pw, int ph, int vx, int vy);
static void resolve_player_col(void);
static void resolve_entity_col(Entity *e);
static int  check_entity_player_col(Entity *e);

static void ps2_read(void);
static int  key_held(int scancode);
static int  key_pressed(int scancode);

static void init_game(void);
static void init_level(void);
static void game_tick(void);
static void reset_map(void);
static void clear_entities(void);

/* ============================================================================
   TILEMAP  (100 columns × 15 rows, row 0 = top of screen)
   Legend: 0=air 1=ground 2=brick 3=?block 4=used_q 5=pipe 6=cloud 7=coin_block 8=goal 9=platform
   ============================================================================ */
static const unsigned char MAP[MAP_ROWS][MAP_COLS] = {
/*       0         1         2         3         4         5         6         7         8         9    */
/* 0 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 1 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 2 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 3 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 4 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 5 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,2,0,3,0,0,0,0,0,0,2,2,2,0,0,0,0,0,0,0,3,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 6 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 7 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,9,9,0,0,0,0,0,0,0,0,9,9,9,9,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 8 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 9 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*10 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0},
/*11 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
/*12 */ {1,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
/*13 */ {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
/*14 */ {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

/* mutable copy of the map (for breaking blocks, using ? blocks) */
static unsigned char g_map[MAP_ROWS][MAP_COLS];
static TileMeta g_tile_meta[MAP_ROWS][MAP_COLS];

/* ============================================================================
   GLOBAL STATE
   ============================================================================ */
static volatile unsigned short *g_draw_buf;   /* current draw buffer pointer */
static volatile unsigned short *g_show_buf;   /* currently displayed buffer  */
static int g_buf_flip;

static Player   g_player;
static Entity   g_ents[MAX_ENTITIES];
static FloatText g_floats[MAX_FLOAT];
static int g_camera_x;   /* left-edge pixel of camera in world */
static GameState g_state;
static int g_timer;      /* level countdown (frames) */
static int g_frame;      /* global frame counter */

/* ── Input state ────────────────────────────────────────────────────────── */
static unsigned char g_keys[256];         /* 1 = held */
static unsigned char g_keys_prev[256];    /* state last frame */
static int g_break_code;                  /* expecting break byte */

/* ── Enemy spawn table: {world_x, type} ────────────────────────────────── */
static const int ENEMY_SPAWNS[][3] = {
    /* x,   y (pixels),  type */
    {10*16, 12*16-16, ENT_GOOMBA},
    {14*16, 12*16-16, ENT_GOOMBA},
    {22*16, 12*16-16, ENT_KOOPA},
    {28*16, 12*16-16, ENT_GOOMBA},
    {35*16, 12*16-16, ENT_GOOMBA},
    {37*16, 12*16-16, ENT_GOOMBA},
    {43*16, 12*16-16, ENT_KOOPA},
    {50*16, 12*16-16, ENT_GOOMBA},
    {55*16, 12*16-16, ENT_KOOPA},
    {60*16, 12*16-16, ENT_GOOMBA},
    {65*16, 12*16-16, ENT_GOOMBA},
    {70*16, 12*16-16, ENT_KOOPA},
    {75*16, 12*16-16, ENT_GOOMBA},
    {80*16, 12*16-16, ENT_GOOMBA},
    {85*16, 12*16-16, ENT_KOOPA},
    {90*16, 12*16-16, ENT_GOOMBA},
    {-1, -1, ENT_NONE}
};

/* ============================================================================
   PIXEL BUFFER HELPERS
   ============================================================================ */
static void draw_pixel(int x, int y, unsigned short col) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    *(g_draw_buf + y * FB_W + x) = col;
}

static void draw_rect(int x, int y, int w, int h, unsigned short col) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    int i, j;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++)
            *(g_draw_buf + j * FB_W + i) = col;
}

static void draw_rect_border(int x, int y, int w, int h,
                              unsigned short col, unsigned short border) {
    draw_rect(x, y, w, h, col);
    /* top */
    draw_rect(x, y, w, 2, border);
    /* bottom */
    draw_rect(x, y+h-2, w, 2, border);
    /* left */
    draw_rect(x, y, 2, h, border);
    /* right */
    draw_rect(x+w-2, y, 2, h, border);
}

static void clear_screen(unsigned short col) {
    int i;
    for (i = 0; i < FB_W * SCREEN_H; i++)
        *(g_draw_buf + i) = col;
}

static void wait_vsync(void) {
    /* Write 1 to buffer S bit to request swap, poll S bit to confirm */
    *(PIXEL_BUF_CTRL) = 1;
    while (*(PIXEL_BUF_CTRL + 3) & 1);
}

static void swap_buffers(void) {
    volatile unsigned short *tmp;
    wait_vsync();
    /* The front buffer pointer is read from register 0 after vsync */
    tmp = g_draw_buf;
    g_draw_buf = g_show_buf;
    g_show_buf = tmp;
    /* Update back buffer address to the new draw buffer */
    *(PIXEL_BUF_CTRL + 1) = (uint32_t)(uintptr_t)g_draw_buf;
    g_buf_flip ^= 1;
}

/* ============================================================================
   SIMPLE BITMAP FONT  (5×7, ASCII 32..90)
   Each character: 5 bytes, each byte = 7 pixels column LSB first
   ============================================================================ */
static const unsigned char FONT5X7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
};

static void draw_char(int x, int y, char c, unsigned short col) {
    int ci = (int)c - 32;
    int col_i, row;
    unsigned char bits;
    if (ci < 0 || ci >= 59) return;
    for (col_i = 0; col_i < 5; col_i++) {
        bits = FONT5X7[ci][col_i];
        for (row = 0; row < 7; row++) {
            if (bits & (1 << row))
                draw_pixel(x + col_i, y + row, col);
        }
    }
}

static void draw_string(int x, int y, const char *s, unsigned short col) {
    while (*s) {
        char c = *s >= 'a' && *s <= 'z' ? (*s - 32) : *s;
        draw_char(x, y, c, col);
        x += 6;
        s++;
    }
}

static void draw_number(int x, int y, int n, unsigned short col) {
    char buf[12];
    int i = 10, neg = 0;
    buf[11] = '\0';
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) buf[i--] = '0';
    while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
    if (neg) buf[i--] = '-';
    draw_string(x, y, buf + i + 1, col);
}

/* ============================================================================
   TILE DRAWING  (16×16 tiles drawn with coloured rectangles)
   ============================================================================ */
static void draw_tile(int tx, int ty, int tile_id) {
    /* tx,ty = screen pixels (top-left of tile) */
    int boff = g_tile_meta[ty / TILE_H][tx / TILE_H + g_camera_x / TILE_W].anim_off;
    int sy = ty - boff;
    switch (tile_id) {
    case TILE_GROUND:
        draw_rect(tx, sy, TILE_W, TILE_H, COL(139,90,43));
        draw_rect(tx, sy, TILE_W, 4, COL(80,200,80));
        draw_rect(tx+1, sy+1, TILE_W-2, 2, COL(130,240,130));
        break;
    case TILE_BRICK:
        draw_rect(tx, sy, TILE_W, TILE_H, C_BROWN);
        /* mortar lines */
        draw_rect(tx, sy+7, TILE_W, 2, C_DKBROWN);
        draw_rect(tx+7, sy, 2, 7, C_DKBROWN);
        draw_rect(tx+3, sy+9, 2, 7, C_DKBROWN);
        draw_rect(tx+11, sy+9, 2, 7, C_DKBROWN);
        break;
    case TILE_QBLOCK:
    case TILE_COIN_BLOCK:
        draw_rect(tx, sy, TILE_W, TILE_H, C_YELLOW);
        draw_rect(tx+1, sy+1, TILE_W-2, 2, COL(255,255,180));
        draw_rect(tx+1, sy+13, TILE_W-2, 2, COL(200,160,0));
        draw_rect(tx+4, sy+3, 8, 10, C_ORANGE);
        draw_char(tx+5, sy+4, '?', C_WHITE);
        break;
    case TILE_QBLOCK_USED:
        draw_rect(tx, sy, TILE_W, TILE_H, C_GRAY);
        draw_rect(tx+1, sy+1, TILE_W-2, 2, C_LTGRAY);
        draw_rect(tx+1, sy+13, TILE_W-2, 2, COL(100,100,100));
        break;
    case TILE_PIPE: {
        /* pipe body (green) */
        int px = tx;
        int is_top = (ty/TILE_H > 0 && g_map[ty/TILE_H-1][tx/TILE_W+g_camera_x/TILE_W] != TILE_PIPE);
        draw_rect(px+1, sy, TILE_W-2, TILE_H, C_PIPE_G);
        draw_rect(px+3, sy, TILE_W-6, TILE_H, COL(100,220,100));
        if (is_top) {
            draw_rect(px, sy, TILE_W, 5, C_PIPE_DK);
            draw_rect(px+1, sy, TILE_W-2, 4, C_PIPE_G);
            draw_rect(px+3, sy, TILE_W-6, 4, COL(100,220,100));
        }
        draw_rect(px+1, sy, 2, TILE_H, C_PIPE_DK);
        break;
    }
    case TILE_CLOUD:
        draw_rect(tx+2, sy+6, TILE_W-4, 8, C_CLOUD);
        draw_rect(tx+4, sy+3, 8, 6, C_CLOUD);
        draw_rect(tx+6, sy+1, 4, 4, C_CLOUD);
        break;
    case TILE_PLATFORM:
        draw_rect(tx, sy, TILE_W, 6, COL(200,120,50));
        draw_rect(tx+1, sy+1, TILE_W-2, 2, COL(240,180,100));
        break;
    case TILE_GOAL:
        /* flagpole */
        draw_rect(tx+7, sy, 2, TILE_H, C_FLAGPOLE);
        draw_rect(tx+3, sy, 10, 3, C_GREEN);
        draw_rect(tx+3, sy, 3, 6, C_GREEN);
        break;
    default:
        break;
    }
}

/* ============================================================================
   SPRITE DRAWING  (8×8 to 16×16 "sprites" built from coloured rectangles)
   We draw pixel-art style sprites using small rectangles, pixel by pixel
   for the important details. flip=1 mirrors horizontally.
   ============================================================================ */

/* Helper: draw a sprite pixel at relative (rx, ry) with optional H-flip in 16px wide sprite */
#define SP(rx, ry, col) draw_pixel(x + (flip ? (15-(rx)) : (rx)), y+(ry), col)
/* Draw a horizontal run of same colour pixels */
static void sp_run(int x, int y, int rx, int ry, int len, unsigned short col, int flip) {
    int i;
    for (i = 0; i < len; i++)
        SP(rx+i, ry, col);
}
#define SPR(rx, ry, len, col) sp_run(x, y, rx, ry, len, col, flip)

static void draw_mario_stand(int x, int y, int flip, PowerUp pu) {
    unsigned short hat  = pu == PU_FIRE ? C_WHITE : C_RED;
    unsigned short shirt = pu == PU_FIRE ? C_RED   : C_RED;
    unsigned short pants = pu == PU_FIRE ? C_BLACK : COL(0,0,200);
    /* hat */
    SPR(4, 0, 8, hat);
    SPR(3, 1, 10, hat);
    /* hair / face */
    SPR(3, 2, 10, C_BROWN);
    SPR(2, 3, 4, C_TAN); SPR(7, 3, 4, C_TAN); SP(6,3, C_BROWN);
    SPR(2, 4, 12, C_TAN);
    /* eyes */
    SP(4,4, C_BLACK); SP(10,4, C_BLACK);
    /* nose */
    SPR(5, 5, 4, C_TAN);
    /* moustache */
    SPR(3, 6, 4, C_BROWN); SPR(9, 6, 4, C_BROWN);
    /* shirt (overalls) */
    SPR(2, 7, 12, shirt);
    SPR(1, 8, 14, shirt);
    SPR(1, 9, 14, pants);
    SPR(2,10, 12, pants);
    /* buttons */
    SP(4,9, C_WHITE); SP(11,9, C_WHITE);
    /* legs */
    SPR(2,11, 5, pants); SPR(9,11, 5, pants);
    SPR(2,12, 5, pants); SPR(9,12, 5, pants);
    /* shoes */
    SPR(1,13, 6, C_BLACK); SPR(9,13, 6, C_BLACK);
    SPR(1,14, 7, C_BLACK); SPR(8,14, 7, C_BLACK);
    SPR(1,15, 7, C_DKBROWN); SPR(8,15, 7, C_DKBROWN);
}

static void draw_mario_jump(int x, int y, int flip, PowerUp pu) {
    draw_mario_stand(x, y-2, flip, pu);
    /* raise arms */
    SP(0, 8, C_TAN); SP(0, 9, C_TAN);
    SP(15, 8, C_TAN); SP(15, 9, C_TAN);
    /* tuck legs */
    SPR(3,12, 10, pu == PU_FIRE ? C_RED : C_RED);
}

static void draw_mario_dead(int x, int y, int flip) {
    /* spinning dead mario (simplified) */
    draw_mario_stand(x, y, flip, PU_SMALL);
    /* X eyes */
    SP(4,4, C_WHITE); SP(3,3, C_BLACK); SP(5,5, C_BLACK);
    SP(10,4, C_WHITE); SP(9,3, C_BLACK); SP(11,5, C_BLACK);
}

static void draw_goomba(int x, int y, int frame) {
    int flip = 0;
    unsigned short body = COL(180,100,40);
    unsigned short dark = COL(100,50,10);
    unsigned short feet = COL(80,40,10);
    /* head / body */
    SPR(2, 0, 12, body);
    SPR(1, 1, 14, body);
    SPR(0, 2, 16, body);
    SPR(0, 3, 16, body);
    SPR(0, 4, 16, body);
    SPR(0, 5, 16, body);
    /* eyes */
    SPR(2, 2, 3, C_WHITE); SPR(11, 2, 3, C_WHITE);
    SP(3,2, C_BLACK); SP(12,2, C_BLACK);
    SP(2,3, C_BLACK); SP(11,3, C_BLACK);
    /* eyebrows (angry) */
    SPR(2, 1, 4, dark); SPR(10, 1, 4, dark);
    /* feet animation */
    if (frame == 0) {
        SPR(1,12, 5, feet); SPR(9,12, 7, dark);
        SPR(0,13, 6, feet); SPR(9,13, 7, dark);
        SPR(0,14, 6, feet); SPR(10,14, 6, dark);
        SPR(0,15, 6, dark); SPR(10,15, 6, dark);
    } else {
        SPR(0,12, 7, dark); SPR(10,12, 5, feet);
        SPR(0,13, 7, dark); SPR(9,13, 7, feet);
        SPR(1,14, 6, dark); SPR(9,14, 6, feet);
        SPR(1,15, 6, dark); SPR(9,15, 6, dark);
    }
    /* body bottom */
    SPR(0, 6, 16, body); SPR(0, 7, 16, body);
    SPR(1, 8, 14, body); SPR(2, 9, 12, body);
    SPR(3,10, 10, body); SPR(4,11,  8, body);
}

static void draw_goomba_flat(int x, int y) {
    int flip = 0;
    unsigned short body = COL(180,100,40);
    SPR(1,12, 14, body);
    SPR(0,13, 16, body);
    SPR(0,14, 16, body);
    SPR(0,15, 16, body);
    SP(3,13, C_BLACK); SP(12,13, C_BLACK);
}

static void draw_koopa(int x, int y, int frame, int flip) {
    unsigned short shell = COL(80,180,80);
    unsigned short skin  = COL(240,200,80);
    unsigned short dark  = COL(30,120,30);
    /* head */
    SPR(4, 0, 8, skin); 
    SP(5,0, C_WHITE); SP(6,0, C_BLACK);
    SPR(3, 1,10, skin);
    /* shell */
    SPR(1, 4,14, shell);
    SPR(0, 5,16, shell);
    SPR(0, 6,16, shell);
    SPR(0, 7,16, dark);
    SPR(1, 8,14, shell);
    SPR(1, 9,14, shell);
    SPR(2,10,12, shell);
    /* shell lines */
    SPR(4, 6, 8, dark);
    SPR(4, 8, 8, dark);
    /* legs */
    if (frame == 0) {
        SPR(2,11, 5, skin); SPR(9,11, 5, skin);
        SPR(1,12, 5, skin); SPR(9,12, 5, skin);
        SPR(1,13, 5, C_GREEN); SPR(9,13, 5, C_GREEN);
    } else {
        SPR(3,11, 5, skin); SPR(8,11, 5, skin);
        SPR(2,12, 5, skin); SPR(10,12, 5, skin);
        SPR(2,13, 5, C_GREEN); SPR(10,13, 5, C_GREEN);
    }
    SPR(2, 2, 3, skin);
    SPR(3, 3, 4, skin);
}

static void draw_koopa_shell(int x, int y) {
    int flip = 0;
    unsigned short shell = COL(80,180,80);
    unsigned short dark  = COL(30,120,30);
    SPR(2, 4,12, shell);
    SPR(1, 5,14, shell);
    SPR(0, 6,16, shell);
    SPR(0, 7,16, dark);
    SPR(1, 8,14, shell);
    SPR(2, 9,12, shell);
    SPR(3,10,10, shell);
    SPR(4, 6, 8, dark);
    SPR(4, 8, 8, dark);
}

static void draw_coin_sprite(int x, int y, int frame) {
    int flip = 0;
    unsigned short c = frame < 2 ? C_COIN : COL(200,160,0);
    int w = frame == 1 || frame == 3 ? 4 : 8;
    int ox = (8 - w) / 2;
    SPR(4+ox, 0, w, c); SPR(3+ox, 1, w+2, c);
    SPR(2+ox, 2, w+4, c); SPR(2+ox, 3, w+4, c);
    SPR(2+ox, 4, w+4, c); SPR(2+ox, 5, w+4, c);
    SPR(3+ox, 6, w+2, c); SPR(4+ox, 7, w, c);
    SP(6,3, COL(255,255,180)); SP(6,4, COL(255,255,180));
}

static void draw_mushroom_sprite(int x, int y) {
    int flip = 0;
    SPR(3, 0,10, C_MUSHROOM);
    SPR(1, 1,14, C_MUSHROOM);
    SPR(0, 2,16, C_MUSHROOM);
    SPR(0, 3,16, C_MUSHROOM);
    SPR(1, 4,14, C_MUSHROOM);
    SPR(2, 5,12, C_MUSHROOM);
    /* spots */
    SPR(3, 1, 3, C_WHITE); SPR(10, 1, 3, C_WHITE);
    SPR(6, 3, 3, C_WHITE);
    /* stem */
    SPR(3, 6,10, C_TAN); SPR(3, 7,10, C_TAN);
    SPR(3, 8,10, C_TAN); SPR(2, 9,12, C_TAN);
    SPR(2,10,12, C_TAN); SPR(2,11,12, C_TAN);
    SPR(2,12,12, C_TAN);
}

static void draw_star_sprite(int x, int y) {
    int flip = 0;
    unsigned short s = C_STAR;
    SP(7, 0, s); SP(8, 0, s);
    SPR(6, 1, 4, s); SPR(5, 2, 6, s);
    SPR(3, 3,10, s); SPR(2, 4,12, s);
    SPR(0, 5,16, s); SPR(0, 6,16, s);
    SPR(0, 7,16, s); SPR(0, 8,14, s);
    SPR(1, 9,14, s); SPR(2,10, 4, s); SPR(10,10, 4, s);
    SP(2,11,s); SP(3,11,s); SP(12,11,s); SP(13,11,s);
    SP(2,12,s); SP(13,12,s);
    /* eyes */
    SP(5,6, C_BLACK); SP(5,7, C_BLACK); SP(10,6, C_BLACK); SP(10,7, C_BLACK);
    SP(6,5, C_BLACK); SP(11,5, C_BLACK);
}

static void draw_fireball_sprite(int x, int y) {
    int flip = 0;
    SPR(5, 0, 6, C_WHITE);
    SPR(3, 1,10, C_WHITE);
    SPR(2, 2,12, C_YELLOW);
    SPR(2, 3,12, C_ORANGE);
    SPR(3, 4,10, C_RED);
    SPR(4, 5, 8, C_RED);
    SPR(5, 6, 6, C_DKRED);
    SPR(6, 7, 4, C_DKRED);
}

static void draw_sprite(int x, int y, int spr_id, int flip) {
    /* adjust x for flip so sprite stays in same world position */
    switch (spr_id) {
    case SPR_MARIO_STAND: draw_mario_stand(x, y, flip, g_player.power); break;
    case SPR_MARIO_WALK1: draw_mario_stand(x, y, flip, g_player.power); break;
    case SPR_MARIO_WALK2:
        /* slight lean */
        draw_mario_stand(x, y, flip, g_player.power);
        draw_rect(x+1, y+12, 4, 3, C_BLACK); /* feet swap */
        break;
    case SPR_MARIO_JUMP:  draw_mario_jump(x, y, flip, g_player.power); break;
    case SPR_MARIO_DEAD:  draw_mario_dead(x, y, flip); break;
    case SPR_GOOMBA1:     draw_goomba(x, y, 0); break;
    case SPR_GOOMBA2:     draw_goomba(x, y, 1); break;
    case SPR_GOOMBA_FLAT: draw_goomba_flat(x, y); break;
    case SPR_KOOPA1:      draw_koopa(x, y, 0, flip); break;
    case SPR_KOOPA2:      draw_koopa(x, y, 1, flip); break;
    case SPR_KOOPA_SHELL: draw_koopa_shell(x, y); break;
    case SPR_COIN_SPIN1:  draw_coin_sprite(x, y, 0); break;
    case SPR_COIN_SPIN2:  draw_coin_sprite(x, y, 2); break;
    case SPR_MUSHROOM:    draw_mushroom_sprite(x, y); break;
    case SPR_STAR:        draw_star_sprite(x, y); break;
    case SPR_FIREBALL:    draw_fireball_sprite(x, y); break;
    default: break;
    }
}

/* ============================================================================
   PS/2 INPUT
   ============================================================================ */
static void ps2_read(void) {
    volatile int *ps2 = PS2_BASE;
    int data, rvalid;
    unsigned char scan;

    /* Copy current keys to prev */
    int i;
    for (i = 0; i < 256; i++) g_keys_prev[i] = g_keys[i];

    /* Drain PS/2 FIFO (multiple bytes may arrive per frame) */
    for (i = 0; i < 16; i++) {
        data   = *ps2;
        rvalid = (data >> 15) & 1;
        if (!rvalid) break;
        scan = data & 0xFF;

        if (scan == 0xF0) {          /* break code prefix */
            g_break_code = 1;
        } else if (g_break_code) {
            if (scan < 256) g_keys[scan] = 0;
            g_break_code = 0;
        } else {
            if (scan < 256) g_keys[scan] = 1;
        }
    }
}

static int key_held(int sc)    { return g_keys[sc]; }
static int key_pressed(int sc) { return g_keys[sc] && !g_keys_prev[sc]; }

/* ============================================================================
   TILE / MAP HELPERS
   ============================================================================ */
static int tile_solid(int t) {
    return t == TILE_GROUND || t == TILE_BRICK || t == TILE_QBLOCK ||
           t == TILE_QBLOCK_USED || t == TILE_PIPE || t == TILE_COIN_BLOCK ||
           t == TILE_PLATFORM;
}
static int tile_passthrough(int t) { return t == TILE_PLATFORM; }

/* Get tile at world pixel (px,py). Returns 0 for out-of-bounds. */
static int world_tile(int px, int py) {
    int tx = px / TILE_W, ty = py / TILE_H;
    if (tx < 0 || tx >= MAP_COLS || ty < 0 || ty >= MAP_ROWS) return TILE_GROUND;
    return g_map[ty][tx];
}

/* ============================================================================
   PLAYER COLLISION RESOLUTION
   AABB swept collision against solid tiles.
   We test each axis independently (x then y) to allow sliding along walls.
   ============================================================================ */
static void resolve_player_col(void) {
    Player *p = &g_player;
    int vx = p->vx >> FIXED_SHIFT;
    int vy = p->vy >> FIXED_SHIFT;
    int i, tx, ty, tile;
    int foot, head, left, right;

    /* ── X axis ── */
    p->x += vx;
    /* Check left and right edges at top and bottom of player */
    foot = p->y + p->h - 2;
    head = p->y + 4;
    if (vx > 0) {
        right = p->x + p->w - 1;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(right, i);
            if (tile_solid(tile) && !tile_passthrough(tile)) {
                tx = (right / TILE_W) * TILE_W;
                p->x = tx - p->w;
                p->vx = 0;
                break;
            }
        }
    } else if (vx < 0) {
        left = p->x;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(left, i);
            if (tile_solid(tile) && !tile_passthrough(tile)) {
                tx = (left / TILE_W + 1) * TILE_W;
                p->x = tx;
                p->vx = 0;
                break;
            }
        }
    }

    /* ── Y axis ── */
    p->on_ground = 0;
    p->y += vy;
    left  = p->x + 2;
    right = p->x + p->w - 3;

    if (vy > 0) {
        /* Falling — check bottom edge */
        foot = p->y + p->h - 1;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, foot);
            int is_plat = tile_passthrough(tile);
            /* Land on platform only when falling from above */
            if (tile_solid(tile) && (!is_plat || (p->y + p->h - vy) <= (foot/TILE_H)*TILE_H)) {
                ty = (foot / TILE_H) * TILE_H;
                p->y = ty - p->h;
                p->vy = 0;
                p->on_ground = 1;
                break;
            }
        }
    } else if (vy < 0) {
        /* Rising — check top edge (head bump) */
        head = p->y;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, head);
            if (tile_solid(tile) && !tile_passthrough(tile)) {
                ty  = (head / TILE_H + 1) * TILE_H;
                p->y = ty;
                p->vy = 0;
                /* Hit block from below */
                int btx = i / TILE_W, bty = head / TILE_H;
                hit_block(btx, bty);
                break;
            }
        }
    }

    /* Kill player if fallen off bottom */
    if (p->y > MAP_ROWS * TILE_H) player_die();

    /* Clamp left edge */
    if (p->x < 0) { p->x = 0; p->vx = 0; }
}

/* ============================================================================
   BLOCK HIT / BREAK
   ============================================================================ */
static void hit_block(int tx, int ty) {
    int t = g_map[ty][tx];
    if (t == TILE_QBLOCK || t == TILE_COIN_BLOCK) {
        if (g_tile_meta[ty][tx].hit == 0) {
            /* Spawn coin or item */
            if (t == TILE_COIN_BLOCK || g_player.power == PU_SMALL) {
                add_score(200, tx*TILE_W - g_camera_x, ty*TILE_H);
                add_coin();
                spawn_entity(ENT_COIN, tx*TILE_W, ty*TILE_H - TILE_H);
            } else {
                spawn_entity(ENT_MUSHROOM, tx*TILE_W, ty*TILE_H - TILE_H);
            }
            g_map[ty][tx] = TILE_QBLOCK_USED;
            g_tile_meta[ty][tx].hit = 1;
        }
        /* Bump animation */
        g_tile_meta[ty][tx].anim_timer = 8;
        g_tile_meta[ty][tx].anim_off   = 4;
    } else if (t == TILE_BRICK) {
        if (g_player.power != PU_SMALL) {
            break_block(tx, ty);
        } else {
            /* Bounce */
            g_tile_meta[ty][tx].anim_timer = 6;
            g_tile_meta[ty][tx].anim_off   = 3;
        }
    }
}

static void break_block(int tx, int ty) {
    int wx = tx * TILE_W, wy = ty * TILE_H;
    g_map[ty][tx] = TILE_AIR;
    add_score(50, wx - g_camera_x, wy);
    /* Spawn 4 debris particles */
    spawn_particle(wx+4,  wy+4,  -2*16, -5*16, C_BROWN, 25);
    spawn_particle(wx+12, wy+4,   2*16, -5*16, C_BROWN, 25);
    spawn_particle(wx+4,  wy+12, -2*16, -3*16, C_DKBROWN, 25);
    spawn_particle(wx+12, wy+12,  2*16, -3*16, C_DKBROWN, 25);
}

/* ============================================================================
   ENTITY COLLISION HELPERS
   ============================================================================ */
static void resolve_entity_col(Entity *e) {
    int vx = e->vx >> FIXED_SHIFT;
    int vy = e->vy >> FIXED_SHIFT;
    int left, right, foot, head, i, tile, tx, ty;

    e->on_ground = 0;

    /* X */
    e->x += vx;
    foot = e->y + e->h - 2;
    head = e->y + 4;
    if (vx != 0) {
        int edge = vx > 0 ? e->x + e->w - 1 : e->x;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(edge, i);
            if (tile_solid(tile) && !tile_passthrough(tile)) {
                if (vx > 0) {
                    tx = (edge / TILE_W) * TILE_W;
                    e->x = tx - e->w;
                } else {
                    tx = (edge / TILE_W + 1) * TILE_W;
                    e->x = tx;
                }
                e->vx = -e->vx; /* reverse direction */
                e->facing = -e->facing;
                break;
            }
        }
    }

    /* Y */
    e->y += vy;
    left  = e->x + 1;
    right = e->x + e->w - 2;
    if (vy >= 0) {
        foot = e->y + e->h - 1;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, foot);
            if (tile_solid(tile)) {
                ty = (foot / TILE_H) * TILE_H;
                e->y = ty - e->h;
                e->vy = 0;
                e->on_ground = 1;
                break;
            }
        }
    } else {
        head = e->y;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, head);
            if (tile_solid(tile) && !tile_passthrough(tile)) {
                ty = (head / TILE_H + 1) * TILE_H;
                e->y = ty;
                e->vy = 0;
                break;
            }
        }
    }

    /* Kill entity if fallen off */
    if (e->y > MAP_ROWS * TILE_H + 64) e->active = 0;
}

/* AABB test between player and entity */
static int check_entity_player_col(Entity *e) {
    Player *p = &g_player;
    return p->x < e->x + e->w && p->x + p->w > e->x &&
           p->y < e->y + e->h && p->y + p->h > e->y;
}

/* ============================================================================
   SCORE / COINS / PARTICLES / FLOAT TEXT
   ============================================================================ */
static void add_score(int pts, int sx, int sy) {
    g_player.score += pts;
    /* Add floating text */
    int i;
    for (i = 0; i < MAX_FLOAT; i++) {
        if (g_floats[i].timer <= 0) {
            g_floats[i].score = pts;
            g_floats[i].x = sx;
            g_floats[i].y = sy;
            g_floats[i].timer = 40;
            break;
        }
    }
}

static void add_coin(void) {
    g_player.coins++;
    if (g_player.coins >= 100) {
        g_player.coins -= 100;
        g_player.lives++;
    }
}

static void spawn_particle(int x, int y, int vx, int vy, int col, int life) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        if (!g_ents[i].active) {
            g_ents[i].active      = 1;
            g_ents[i].type        = ENT_PARTICLE;
            g_ents[i].x           = x;
            g_ents[i].y           = y;
            g_ents[i].vx          = vx;
            g_ents[i].vy          = vy;
            g_ents[i].w           = 4;
            g_ents[i].h           = 4;
            g_ents[i].color       = col;
            g_ents[i].lifetime    = life;
            g_ents[i].bounce_count = 0;
            break;
        }
    }
}

static void spawn_fireball(int x, int y, int dir) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        if (!g_ents[i].active) {
            g_ents[i].active   = 1;
            g_ents[i].type     = ENT_FIREBALL;
            g_ents[i].x        = x;
            g_ents[i].y        = y;
            g_ents[i].vx       = dir * 4 * (1 << FIXED_SHIFT);
            g_ents[i].vy       = 2 * (1 << FIXED_SHIFT);
            g_ents[i].w        = 8;
            g_ents[i].h        = 8;
            g_ents[i].facing   = dir;
            g_ents[i].lifetime = 80;
            break;
        }
    }
}

static void spawn_entity(EntityType t, int x, int y) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        if (!g_ents[i].active) {
            g_ents[i].active       = 1;
            g_ents[i].type         = t;
            g_ents[i].x            = x;
            g_ents[i].y            = y;
            g_ents[i].dead         = 0;
            g_ents[i].squished     = 0;
            g_ents[i].shell_moving = 0;
            g_ents[i].frame        = 0;
            g_ents[i].frame_timer  = 0;
            g_ents[i].bounce_count = 0;
            g_ents[i].lifetime     = 0;
            switch (t) {
            case ENT_GOOMBA:
                g_ents[i].vx = -1 * (1 << FIXED_SHIFT);
                g_ents[i].vy = 0;
                g_ents[i].w  = 16; g_ents[i].h = 16;
                g_ents[i].facing = -1;
                break;
            case ENT_KOOPA:
                g_ents[i].vx = -1 * (1 << FIXED_SHIFT);
                g_ents[i].vy = 0;
                g_ents[i].w  = 16; g_ents[i].h = 16;
                g_ents[i].facing = -1;
                break;
            case ENT_COIN:
                g_ents[i].vx = 0;
                g_ents[i].vy = -6 * (1 << FIXED_SHIFT);
                g_ents[i].w  = 8; g_ents[i].h = 8;
                g_ents[i].lifetime = 40;
                break;
            case ENT_MUSHROOM:
                g_ents[i].vx = 1 * (1 << FIXED_SHIFT);
                g_ents[i].vy = 0;
                g_ents[i].w  = 16; g_ents[i].h = 16;
                break;
            case ENT_STAR:
                g_ents[i].vx = 2 * (1 << FIXED_SHIFT);
                g_ents[i].vy = -4 * (1 << FIXED_SHIFT);
                g_ents[i].w  = 16; g_ents[i].h = 16;
                break;
            default: break;
            }
            break;
        }
    }
}


/* ============================================================================
   PLAYER UPDATE
   ============================================================================ */
static void player_die(void) {
    if (g_player.state == PS_DEAD) return;
    g_player.state = PS_DEAD;
    g_player.vy    = JUMP_VEL * (1 << FIXED_SHIFT) / 2;  /* death hop */
    g_player.vx    = 0;
    g_player.lives--;
}

static void player_grow(void) {
    if (g_player.power == PU_SMALL) {
        g_player.power = PU_BIG;
        g_player.h = 24; /* taller hitbox */
        g_player.y -= 8;
    }
}

static void player_shrink(void) {
    if (g_player.power > PU_SMALL) {
        g_player.power = PU_SMALL;
        g_player.h = 16;
        g_player.invincible = 120; /* invincibility window */
    } else {
        player_die();
    }
}

static void player_fire_powerup(void) {
    g_player.power = PU_FIRE;
    g_player.h = 24;
}

static int g_jump_held;
static int g_fire_cooldown;

static void update_player(void) {
    Player *p = &g_player;

    if (p->state == PS_WIN) {
        /* Walk toward goal post and freeze */
        p->x += 2;
        if (p->x > 99 * TILE_W) {
            g_state = GS_WIN;
        }
        return;
    }

    if (p->state == PS_DEAD) {
        /* Death animation: arc upward then fall */
        p->vy += GRAVITY * (1 << FIXED_SHIFT);
        p->y  += p->vy >> FIXED_SHIFT;
        if (p->y > SCREEN_H + 32) {
            if (p->lives <= 0)
                g_state = GS_GAMEOVER;
            else {
                g_state = GS_DEAD;
            }
        }
        return;
    }

    /* Invincibility countdown */
    if (p->invincible > 0) p->invincible--;
    if (p->star_timer > 0) p->star_timer--;

    /* ── Horizontal movement ── */
    int running = key_held(KEY_X);
    int speed   = running ? MOVE_SPEED + 1 : MOVE_SPEED;
    int accel   = 1 << (FIXED_SHIFT - 1);   /* half-pixel acceleration */
    int friction = 1 << (FIXED_SHIFT - 2);  /* deceleration */

    if (key_held(KEY_LEFT)) {
        p->vx -= accel;
        if (p->vx < -speed * (1 << FIXED_SHIFT))
            p->vx = -speed * (1 << FIXED_SHIFT);
        p->facing = -1;
    } else if (key_held(KEY_RIGHT)) {
        p->vx += accel;
        if (p->vx >  speed * (1 << FIXED_SHIFT))
            p->vx =  speed * (1 << FIXED_SHIFT);
        p->facing = 1;
    } else {
        /* Friction */
        if (p->vx > 0) p->vx -= friction;
        if (p->vx < 0) p->vx += friction;
        if (p->vx > -friction && p->vx < friction) p->vx = 0;
    }

    /* ── Jump ── */
    if (key_pressed(KEY_Z) && p->on_ground) {
        p->vy = JUMP_VEL * (1 << FIXED_SHIFT);
        g_jump_held = 1;
    }
    /* Variable jump height: hold Z for higher jump */
    if (g_jump_held && key_held(KEY_Z) && p->vy < 0) {
        p->vy -= (1 << (FIXED_SHIFT - 2)); /* slight boost while held */
        if (p->vy < JUMP_VEL * (1 << FIXED_SHIFT) * 3 / 2)
            g_jump_held = 0;
    }
    if (!key_held(KEY_Z)) g_jump_held = 0;

    /* ── Fireball ── */
    if (g_fire_cooldown > 0) g_fire_cooldown--;
    if (p->power == PU_FIRE && key_pressed(KEY_X) && g_fire_cooldown == 0) {
        spawn_fireball(p->x + (p->facing > 0 ? p->w : -8),
                       p->y + 4, p->facing);
        g_fire_cooldown = 20;
    }

    /* ── Gravity ── */
    p->vy += GRAVITY * (1 << FIXED_SHIFT);
    if (p->vy > MAX_FALL_SPEED * (1 << FIXED_SHIFT))
        p->vy = MAX_FALL_SPEED * (1 << FIXED_SHIFT);

    resolve_player_col();

    /* ── State machine ── */
    if (!p->on_ground) {
        p->state = p->vy < 0 ? PS_JUMPING : PS_FALLING;
    } else if (p->vx != 0) {
        p->state = PS_WALKING;
    } else {
        p->state = PS_STANDING;
    }

    /* ── Animation frame ── */
    p->frame_timer++;
    if (p->state == PS_WALKING && p->frame_timer >= 6) {
        p->frame = (p->frame + 1) % 3;
        p->frame_timer = 0;
    }

    /* Clamp to camera left (can't walk off left screen edge) */
    if (p->x < g_camera_x) p->x = g_camera_x;

    /* Check goal */
    int ptx = (p->x + p->w/2) / TILE_W;
    int pty = (p->y + p->h/2) / TILE_H;
    if (ptx >= 0 && ptx < MAP_COLS && pty >= 0 && pty < MAP_ROWS) {
        if (g_map[pty][ptx] == TILE_GOAL) {
            add_score(5000, p->x - g_camera_x, p->y);
            p->state = PS_WIN;
        }
    }
}

/* ============================================================================
   ENEMY UPDATE
   ============================================================================ */
static void update_enemies(void) {
    int i, j;
    for (i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents[i];
        if (!e->active) continue;

        if (e->type == ENT_PARTICLE) {
            /* Physics for debris */
            e->vy += GRAVITY * (1 << FIXED_SHIFT);
            e->x  += e->vx >> FIXED_SHIFT;
            e->y  += e->vy >> FIXED_SHIFT;
            /* simple ground bounce for particles */
            if (world_tile(e->x, e->y + e->h) != TILE_AIR) {
                e->y = (e->y / TILE_H) * TILE_H - e->h;
                e->vy = -(e->vy * 6) / 10;
                e->vx = (e->vx * 8) / 10;
                e->bounce_count++;
                if (e->bounce_count > 3) e->lifetime = 1;
            }
            e->lifetime--;
            if (e->lifetime <= 0) e->active = 0;
            continue;
        }

        if (e->type == ENT_COIN) {
            e->vy += GRAVITY * (1 << FIXED_SHIFT);
            e->y  += e->vy >> FIXED_SHIFT;
            e->frame_timer++;
            if (e->frame_timer > 8) { e->frame = (e->frame+1)%2; e->frame_timer=0; }
            e->lifetime--;
            if (e->lifetime <= 0) e->active = 0;
            continue;
        }

        if (e->type == ENT_MUSHROOM || e->type == ENT_STAR) {
            e->vy += GRAVITY * (1 << FIXED_SHIFT);
            resolve_entity_col(e);
            if (e->type == ENT_STAR && e->on_ground)
                e->vy = -6 * (1 << FIXED_SHIFT);   /* star bounces */
            /* Player pickup */
            if (!e->dead && check_entity_player_col(e)) {
                if (e->type == ENT_MUSHROOM) {
                    player_grow();
                    add_score(1000, e->x - g_camera_x, e->y);
                } else {
                    g_player.star_timer = 600;
                    add_score(1000, e->x - g_camera_x, e->y);
                }
                e->active = 0;
            }
            continue;
        }

        if (e->type == ENT_FIREBALL) {
            e->vy += (GRAVITY * 3 / 2) * (1 << FIXED_SHIFT);
            if (e->vy > 5*(1<<FIXED_SHIFT)) e->vy = 5*(1<<FIXED_SHIFT);
            resolve_entity_col(e);
            if (e->on_ground) e->vy = -4*(1<<FIXED_SHIFT); /* bounce */
            /* Hit enemies */
            for (j = 0; j < MAX_ENTITIES; j++) {
                Entity *en = &g_ents[j];
                if (!en->active || en->dead) continue;
                if (en->type != ENT_GOOMBA && en->type != ENT_KOOPA) continue;
                if (e->x < en->x+en->w && e->x+e->w > en->x &&
                    e->y < en->y+en->h && e->y+e->h > en->y) {
                    en->dead = 1;
                    en->squished = 1;
                    en->active = 0;
                    add_score(200, en->x - g_camera_x, en->y);
                    spawn_particle(en->x+8, en->y+8, 0, -3*(1<<FIXED_SHIFT), C_YELLOW, 20);
                    e->active = 0;
                    break;
                }
            }
            e->lifetime--;
            if (e->lifetime <= 0) e->active = 0;
            continue;
        }

        /* ── Goomba / Koopa AI ── */
        if (e->type != ENT_GOOMBA && e->type != ENT_KOOPA &&
            e->type != ENT_KOOPA_SHELL) continue;

        if (e->dead) {
            e->squished--;
            if (e->squished <= 0) e->active = 0;
            continue;
        }

        /* Only update if near camera */
        if (e->x < g_camera_x - 32 || e->x > g_camera_x + SCREEN_W + 32) continue;

        e->vy += GRAVITY * (1 << FIXED_SHIFT);
        if (e->vy > MAX_FALL_SPEED * (1 << FIXED_SHIFT))
            e->vy = MAX_FALL_SPEED * (1 << FIXED_SHIFT);

        if (e->type == ENT_KOOPA_SHELL) {
            if (!e->shell_moving) {
                /* Static shell — wait for player kick */
            }
            /* Moving shell kills enemies it hits */
            if (e->shell_moving) {
                for (j = 0; j < MAX_ENTITIES; j++) {
                    Entity *en = &g_ents[j];
                    if (j == i || !en->active || en->dead) continue;
                    if (en->type == ENT_GOOMBA || en->type == ENT_KOOPA) {
                        if (e->x < en->x+en->w && e->x+e->w > en->x &&
                            e->y < en->y+en->h && e->y+e->h > en->y) {
                            en->active = 0;
                            add_score(200, en->x - g_camera_x, en->y);
                        }
                    }
                }
            }
        }

        resolve_entity_col(e);

        /* Animation */
        e->frame_timer++;
        if (e->frame_timer >= 12) { e->frame ^= 1; e->frame_timer = 0; }

        /* Player collision */
        if (check_entity_player_col(e)) {
            Player *p = &g_player;
            if (p->invincible > 0 || p->star_timer > 0) {
                /* Invincible: kill enemy */
                e->active = 0;
                add_score(200, e->x - g_camera_x, e->y);
                continue;
            }
            int stomp = (p->vy > 0) &&
                        (p->y + p->h - 8 <= e->y + 4);
            if (stomp) {
                /* Stomp! */
                if (e->type == ENT_GOOMBA) {
                    e->dead = 1; e->squished = 20;
                    add_score(100, e->x - g_camera_x, e->y);
                } else if (e->type == ENT_KOOPA) {
                    /* Turn into shell */
                    e->type = ENT_KOOPA_SHELL;
                    e->vx = 0; e->shell_moving = 0;
                    add_score(100, e->x - g_camera_x, e->y);
                } else if (e->type == ENT_KOOPA_SHELL) {
                    if (!e->shell_moving) {
                        /* Kick shell */
                        e->vx = p->facing * 5 * (1<<FIXED_SHIFT);
                        e->shell_moving = 1;
                    } else {
                        e->vx = 0; e->shell_moving = 0;
                    }
                }
                /* Bounce player */
                p->vy = JUMP_VEL * (1<<FIXED_SHIFT) / 2;
            } else {
                /* Hurt player */
                if (e->type == ENT_KOOPA_SHELL && !e->shell_moving) {
                    /* Kick shell sideways */
                    int dir = (p->x < e->x) ? 1 : -1;
                    e->vx = dir * 5 * (1<<FIXED_SHIFT);
                    e->shell_moving = 1;
                } else {
                    player_shrink();
                }
            }
        }
    }
}

/* ============================================================================
   CAMERA UPDATE
   ============================================================================ */
static void update_camera(void) {
    /* Target: keep player at 40% from left edge */
    int target = g_player.x - SCREEN_W * 2 / 5;
    /* Smooth follow */
    g_camera_x += (target - g_camera_x) / 4;
    /* Clamp */
    if (g_camera_x < 0) g_camera_x = 0;
    if (g_camera_x > MAP_COLS * TILE_W - SCREEN_W)
        g_camera_x = MAP_COLS * TILE_W - SCREEN_W;
}

/* ============================================================================
   TILE ANIMATION UPDATE (block bump, ? block used flash)
   ============================================================================ */
static void update_tiles(void) {
    int ty, tx;
    for (ty = 0; ty < MAP_ROWS; ty++) {
        for (tx = 0; tx < MAP_COLS; tx++) {
            TileMeta *m = &g_tile_meta[ty][tx];
            if (m->anim_timer > 0) {
                m->anim_timer--;
                if (m->anim_timer == 0) m->anim_off = 0;
                else m->anim_off = (m->anim_timer > 4) ? 4 : -4;
            }
        }
    }
}

static void update_particles(void) {
    /* Handled in update_enemies for now */
}

static void update_float_texts(void) {
    int i;
    for (i = 0; i < MAX_FLOAT; i++) {
        if (g_floats[i].timer > 0) {
            g_floats[i].y--;
            g_floats[i].timer--;
        }
    }
}

/* ============================================================================
   RENDERING
   ============================================================================ */
static void draw_background(void) {
    /* Sky gradient: two bands */
    draw_rect(0, 0,       SCREEN_W, 140, C_SKY);
    draw_rect(0, 140, SCREEN_W,  100, COL(70,120,230));
    /* Small cloud decorations (parallax: move at half speed) */
    int cx = g_camera_x / 2;
    int c1x = (200 - cx) % SCREEN_W;
    int c2x = (380 - cx) % SCREEN_W;
    int c3x = (520 - cx) % SCREEN_W;
    /* Simple cloud shapes */
    draw_rect(c1x, 20, 32, 12, C_CLOUD);
    draw_rect(c1x+4, 14, 24, 12, C_CLOUD);
    draw_rect(c2x, 40, 28, 10, C_CLOUD);
    draw_rect(c2x+4, 34, 20, 10, C_CLOUD);
    draw_rect(c3x, 18, 36, 14, C_CLOUD);
    draw_rect(c3x+6, 12, 24, 14, C_CLOUD);
}

static void draw_tilemap(void) {
    int start_tx = g_camera_x / TILE_W;
    int end_tx   = start_tx + SCREEN_W / TILE_W + 2;
    int ty, tx, tile;
    if (start_tx < 0) start_tx = 0;
    if (end_tx > MAP_COLS) end_tx = MAP_COLS;

    for (ty = 0; ty < MAP_ROWS; ty++) {
        for (tx = start_tx; tx < end_tx; tx++) {
            tile = g_map[ty][tx];
            if (tile == TILE_AIR) continue;
            int sx = tx * TILE_W - g_camera_x;
            int sy = ty * TILE_H;
            draw_tile(sx, sy, tile);
        }
    }
}

static void draw_player(void) {
    Player *p = &g_player;
    int sx = p->x - g_camera_x;
    int sy = p->y;

    /* Invincible flicker */
    if (p->invincible > 0 && (g_frame / 4) % 2) return;

    /* Star flicker (rainbow) */
    if (p->star_timer > 0 && (g_frame / 3) % 2) {
        draw_rect(sx, sy, p->w, p->h, C_STAR);
        return;
    }

    int spr;
    if (p->state == PS_DEAD) {
        draw_sprite(sx, sy, SPR_MARIO_DEAD, p->facing < 0 ? 1 : 0);
        return;
    }
    if (p->state == PS_JUMPING || p->state == PS_FALLING)
        spr = SPR_MARIO_JUMP;
    else if (p->state == PS_WALKING)
        spr = (p->frame == 0) ? SPR_MARIO_STAND : (p->frame == 1 ? SPR_MARIO_WALK1 : SPR_MARIO_WALK2);
    else
        spr = SPR_MARIO_STAND;

    draw_sprite(sx, sy, spr, p->facing < 0 ? 1 : 0);
}

static void draw_enemies(void) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents[i];
        if (!e->active) continue;
        int sx = e->x - g_camera_x;
        int sy = e->y;
        if (sx < -32 || sx > SCREEN_W + 32) continue;

        if (e->type == ENT_PARTICLE) {
            draw_rect(sx, sy, 4, 4, (unsigned short)e->color);
            continue;
        }
        if (e->type == ENT_COIN) {
            draw_sprite(sx, sy, (e->frame == 0) ? SPR_COIN_SPIN1 : SPR_COIN_SPIN2, 0);
            continue;
        }
        if (e->type == ENT_MUSHROOM) {
            draw_sprite(sx, sy, SPR_MUSHROOM, 0);
            continue;
        }
        if (e->type == ENT_STAR) {
            draw_sprite(sx, sy, SPR_STAR, 0);
            continue;
        }
        if (e->type == ENT_FIREBALL) {
            draw_sprite(sx, sy, SPR_FIREBALL, 0);
            continue;
        }
        if (e->type == ENT_GOOMBA) {
            if (e->dead)
                draw_sprite(sx, sy+8, SPR_GOOMBA_FLAT, 0);
            else
                draw_sprite(sx, sy, e->frame ? SPR_GOOMBA2 : SPR_GOOMBA1, 0);
            continue;
        }
        if (e->type == ENT_KOOPA) {
            draw_sprite(sx, sy, e->frame ? SPR_KOOPA2 : SPR_KOOPA1, e->facing < 0 ? 1 : 0);
            continue;
        }
        if (e->type == ENT_KOOPA_SHELL) {
            draw_sprite(sx, sy, SPR_KOOPA_SHELL, 0);
            continue;
        }
    }
}

static void draw_hud(void) {
    /* Dark header bar */
    draw_rect(0, 0, SCREEN_W, 12, COL(20,20,60));
    /* Score */
    draw_string(2, 2, "SCORE", C_WHITE);
    draw_number(2, 2, g_player.score, C_YELLOW); /* will overlap — split to two areas */
    /* Fix: left area = MARIO label + score */
    draw_string(2, 2, "MARIO", COL(200,200,200));
    draw_number(38, 2, g_player.score, C_WHITE);
    /* Coins */
    draw_rect(120, 3, 6, 6, C_COIN);
    draw_string(128, 2, "X", C_WHITE);
    draw_number(136, 2, g_player.coins, C_WHITE);
    /* World */
    draw_string(170, 2, "WORLD 1-1", C_WHITE);
    /* Time */
    draw_string(262, 2, "TIME", C_WHITE);
    draw_number(292, 2, g_timer / 30, g_timer < 150 ? C_RED : C_WHITE);
    /* Lives */
    draw_string(2, SCREEN_H - 10, "LIVES:", C_WHITE);
    draw_number(40, SCREEN_H - 10, g_player.lives, C_YELLOW);

    /* Floating score texts */
    int i;
    for (i = 0; i < MAX_FLOAT; i++) {
        if (g_floats[i].timer > 0) {
            draw_number(g_floats[i].x, g_floats[i].y, g_floats[i].score, C_YELLOW);
        }
    }
}

static void draw_title(void) {
    clear_screen(COL(20,20,80));
    /* Stars in background */
    int i;
    for (i = 0; i < 40; i++) {
        int sx = ((i * 137 + 7) % SCREEN_W);
        int sy = ((i * 89  + 3) % (SCREEN_H-30)) + 10;
        draw_pixel(sx, sy, C_WHITE);
    }
    draw_string(70, 40, "SUPER MARIO", COL(255,50,50));
    draw_string(90, 54, "CPU EDITION", C_YELLOW);
    draw_rect(40, 70, SCREEN_W-80, 2, C_WHITE);
    draw_string(50, 85,  "PRESS ENTER TO PLAY", C_WHITE);
    draw_string(50, 100, "Z = JUMP    X = RUN/FIRE", COL(200,200,200));
    draw_string(50, 112, "LEFT/RIGHT = MOVE", COL(200,200,200));
    draw_string(50, 124, "P = PAUSE", COL(200,200,200));
    draw_string(50, 140, "STOMP ENEMIES TO DEFEAT THEM", COL(180,180,255));
    draw_string(50, 152, "HIT ? BLOCKS FOR POWER-UPS", COL(180,180,255));
    draw_string(50, 164, "REACH THE FLAGPOLE!", COL(100,255,100));
    /* Draw a little mario */
    draw_mario_stand(140, 190, 0, PU_SMALL);
}

static void draw_win_screen(void) {
    clear_screen(COL(20,80,20));
    draw_string(80, 80,  "CONGRATULATIONS!", C_YELLOW);
    draw_string(70, 100, "YOU REACHED THE CASTLE!", C_WHITE);
    draw_string(80, 120, "FINAL SCORE:", C_WHITE);
    draw_number(80, 132, g_player.score, C_YELLOW);
    draw_string(60, 160, "PRESS ENTER TO PLAY AGAIN", COL(180,255,180));
}

static void draw_gameover_screen(void) {
    clear_screen(COL(60,10,10));
    draw_string(110, 80, "GAME OVER", C_RED);
    draw_string(70, 110, "FINAL SCORE:", C_WHITE);
    draw_number(70, 122, g_player.score, C_YELLOW);
    draw_string(60, 150, "PRESS ENTER TO TRY AGAIN", COL(255,180,180));
}

static void draw_pause_overlay(void) {
    /* Semi-transparent overlay effect: draw dark bars */
    int y;
    for (y = 0; y < SCREEN_H; y += 2)
        draw_rect(0, y, SCREEN_W, 1, COL(0,0,0));
    draw_string(110, 110, "PAUSED", C_WHITE);
    draw_string(75, 126, "P = RESUME", COL(200,200,200));
}

static void draw_dead_screen(void) {
    draw_string(90, 110, "YOU DIED!", C_RED);
    draw_string(60, 126, "PRESS ENTER TO CONTINUE", C_WHITE);
    draw_number(90, 140, g_player.lives, C_YELLOW);
    draw_string(104, 140, "LIVES LEFT", C_WHITE);
}

/* ============================================================================
   INITIALIZATION
   ============================================================================ */
static void init_game(void) {
    int i;
    reset_map();
    clear_entities();

    /* Clear keys */
    for (i = 0; i < 256; i++) { g_keys[i] = 0; g_keys_prev[i] = 0; }
    g_break_code = 0;

    g_camera_x = 0;
    g_frame    = 0;
    g_state    = GS_TITLE;
}

static void init_level(void) {
    int i;
    reset_map();
    clear_entities();

    /* Spawn enemies from table */
    for (i = 0; ENEMY_SPAWNS[i][2] != ENT_NONE; i++) {
        spawn_entity((EntityType)ENEMY_SPAWNS[i][2],
                     ENEMY_SPAWNS[i][0], ENEMY_SPAWNS[i][1]);
    }

    /* Player */
    g_player.x           = 2 * TILE_W;
    g_player.y           = 11 * TILE_H - 16;
    g_player.vx          = 0;
    g_player.vy          = 0;
    g_player.w           = 14;
    g_player.h           = 16;
    g_player.facing      = 1;
    g_player.on_ground   = 0;
    g_player.frame       = 0;
    g_player.frame_timer = 0;
    g_player.state       = PS_STANDING;
    g_player.power       = PU_SMALL;
    g_player.invincible  = 0;
    g_player.star_timer  = 0;
    g_player.coins       = 0;

    g_camera_x  = 0;
    g_timer     = 30 * 300;  /* 300 seconds × 30 fps */
    g_jump_held = 0;
    g_fire_cooldown = 0;
    g_state = GS_PLAYING;
}

static void reset_map(void) {
    int i, j;
    for (i = 0; i < MAP_ROWS; i++)
        for (j = 0; j < MAP_COLS; j++) {
            g_map[i][j] = MAP[i][j];
            g_tile_meta[i][j].hit = 0;
            g_tile_meta[i][j].anim_timer = 0;
            g_tile_meta[i][j].anim_off = 0;
        }
}

static void clear_entities(void) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) g_ents[i].active = 0;
    for (i = 0; i < MAX_FLOAT; i++)   g_floats[i].timer = 0;
}

/* ============================================================================
   MAIN GAME TICK
   ============================================================================ */
static void game_tick(void) {
    ps2_read();
    g_frame++;

    switch (g_state) {
    case GS_TITLE:
        draw_title();
        if (key_pressed(KEY_ENTER)) {
            g_player.lives = 3;
            g_player.score = 0;
            init_level();
        }
        break;

    case GS_PLAYING:
        /* Timer */
        if (g_timer > 0) g_timer--;
        else { player_die(); }

        if (key_pressed(KEY_P)) { g_state = GS_PAUSED; break; }

        update_player();
        update_enemies();
        update_camera();
        update_tiles();
        update_float_texts();

        /* Draw */
        draw_background();
        draw_tilemap();
        draw_enemies();
        draw_player();
        draw_hud();
        break;

    case GS_PAUSED:
        /* Redraw game scene frozen */
        draw_background();
        draw_tilemap();
        draw_enemies();
        draw_player();
        draw_hud();
        draw_pause_overlay();
        if (key_pressed(KEY_P)) g_state = GS_PLAYING;
        break;

    case GS_DEAD:
        draw_background();
        draw_tilemap();
        draw_hud();
        draw_dead_screen();
        if (key_pressed(KEY_ENTER)) {
            if (g_player.lives > 0) init_level();
            else g_state = GS_GAMEOVER;
        }
        break;

    case GS_WIN:
        draw_win_screen();
        if (key_pressed(KEY_ENTER)) {
            g_player.lives = 3;
            g_player.score = 0;
            init_level();
        }
        break;

    case GS_GAMEOVER:
        draw_gameover_screen();
        if (key_pressed(KEY_ENTER)) {
            g_player.lives = 3;
            g_player.score = 0;
            init_level();
        }
        break;
    }
}

/* ============================================================================
   ENTRY POINT
   ============================================================================ */
int main(void) {
    /* Setup pixel buffers */
    /* Set an initial back buffer, then sync so it becomes the front */
    *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_BACK;
    wait_vsync();
    g_show_buf = (volatile unsigned short *)(uintptr_t)(*PIXEL_BUF_CTRL);
    /* Point back buffer to the other address for drawing */
    *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_BASE;
    g_draw_buf = (volatile unsigned short *)PIXEL_BUFFER_BASE;
    g_buf_flip = 0;

    /* Clear both buffers */
    clear_screen(C_SKY);
    swap_buffers();
    clear_screen(C_SKY);

    init_game();

    /* Main loop */
    while (1) {
        game_tick();
        swap_buffers();
    }
    return 0;
}
