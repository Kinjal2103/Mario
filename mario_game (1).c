/* ========================================================================
   mario_game.c  —  Simple Mario-style platformer for CPulator (Nios II)
   Target: CPulator simulator (320x240 pixel buffer, PS/2 keyboard)
   ======================================================================== */

#include <stdint.h>

/* Hardware addresses */
#define PIXEL_BUF_CTRL   ((volatile uint32_t *)0xFF203020)
#define PIXEL_BUFFER_BASE 0x08000000
#define PIXEL_BUFFER_BACK 0x09000000
#define PS2_BASE         ((volatile uint32_t *)0xFF200100)

/* Display */
#define SCREEN_W  320
#define SCREEN_H  240
#define FB_W      512

/* Physics */
#define GRAVITY        1
#define JUMP_VEL      -12      /* reduced jump height */
#define MOVE_SPEED     2
#define MAX_FALL_SPEED 10
#define FIXED_SHIFT    4

/* Tile definitions */
#define TILE_W  16
#define TILE_H  16
#define MAP_COLS 60
#define MAP_ROWS 15

#define TILE_AIR         0
#define TILE_GROUND      1
#define TILE_BRICK       2
#define TILE_QBLOCK      3
#define TILE_QBLOCK_USED 4
#define TILE_PIPE        5
#define TILE_GOAL        6

/* Colours (RGB565) */
#define COL(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

#define C_SKY     COL(92,148,252)
#define C_BLACK   COL(0,0,0)
#define C_WHITE   COL(255,255,255)
#define C_RED     COL(220,50,50)
#define C_BROWN   COL(130,70,30)
#define C_DKBROWN COL(90,45,15)
#define C_GREEN   COL(50,200,50)
#define C_YELLOW  COL(255,220,0)
#define C_ORANGE  COL(255,140,0)
#define C_TAN     COL(240,200,140)
#define C_GRAY    COL(150,150,150)
#define C_LTGRAY  COL(210,210,210)
#define C_PIPE_G  COL(60,180,60)
#define C_PIPE_DK COL(20,120,20)
#define C_COIN    COL(255,200,0)
#define C_CLOUD   COL(245,245,255)
#define C_FLAGPOLE COL(180,180,180)

/* PS/2 scan codes */
#define KEY_LEFT  0x6B
#define KEY_RIGHT 0x74
#define KEY_W     0x1D   /* W = jump */
#define KEY_ENTER 0x5A
#define KEY_P     0x4D   /* pause */

/* Entity types */
typedef enum { ENT_NONE=0, ENT_GOOMBA, ENT_COIN, ENT_MUSHROOM } EntityType;

/* Player / game states */
typedef enum { PS_STANDING, PS_WALKING, PS_JUMPING, PS_FALLING, PS_DEAD, PS_WIN } PlayerState;
typedef enum { GS_TITLE, GS_PLAYING, GS_PAUSED, GS_DEAD, GS_WIN, GS_GAMEOVER } GameState;
typedef enum { PU_SMALL, PU_BIG } PowerUp;

/* Structs */
typedef struct {
    int x, y, vx, vy, w, h;
    int frame, frame_timer, facing, on_ground;
    int invincible;
    PlayerState state;
    PowerUp power;
    int lives, score, coins;
} Player;

typedef struct {
    int active;
    EntityType type;
    int x, y, vx, vy, w, h;
    int facing, frame, frame_timer, on_ground;
    int dead, squished;
    int lifetime;
} Entity;

typedef struct {
    int hit, anim_timer, anim_off;
} TileMeta;

typedef struct {
    int score, x, y, timer;
} FloatText;

#define MAX_ENTITIES 20
#define MAX_FLOAT     6

/* ============================================================================
   TILEMAP (60 cols x 15 rows)
   0=air 1=ground 2=brick 3=?block 4=used_q 5=pipe 6=goal
   ============================================================================ */
static const unsigned char MAP[MAP_ROWS][MAP_COLS] = {
/* 0 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 1 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 2 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 3 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 4 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 5 */ {0,0,0,0,0,0,0,0,0,0,0,0,3,0,2,0,3,0,0,0,0,0,2,2,2,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 6 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 7 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 8 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* 9 */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*10 */ {0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0},
/*11 */ {0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
/*12 */ {1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
/*13 */ {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
/*14 */ {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static unsigned char g_map[MAP_ROWS][MAP_COLS];
static TileMeta g_tile_meta[MAP_ROWS][MAP_COLS];

/* Global state */
static volatile unsigned short *g_draw_buf;
static volatile unsigned short *g_show_buf;

static Player    g_player;
static Entity    g_ents[MAX_ENTITIES];
static FloatText g_floats[MAX_FLOAT];
static int g_camera_x;
static GameState g_state;
static int g_timer;
static int g_frame;

/* Input */
static unsigned char g_keys[256];
static unsigned char g_keys_prev[256];
static int g_break_code;

/* Enemy spawn table */
static const int ENEMY_SPAWNS[][3] = {
    {10*16, 12*16-16, ENT_GOOMBA},
    {14*16, 12*16-16, ENT_GOOMBA},
    {20*16, 12*16-16, ENT_GOOMBA},
    {25*16, 12*16-16, ENT_GOOMBA},
    {32*16, 12*16-16, ENT_GOOMBA},
    {38*16, 12*16-16, ENT_GOOMBA},
    {45*16, 12*16-16, ENT_GOOMBA},
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
    int x1 = x+w > SCREEN_W ? SCREEN_W : x+w;
    int y1 = y+h > SCREEN_H ? SCREEN_H : y+h;
    int i, j;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++)
            *(g_draw_buf + j * FB_W + i) = col;
}

static void clear_screen(unsigned short col) {
    int i;
    for (i = 0; i < FB_W * SCREEN_H; i++)
        *(g_draw_buf + i) = col;
}

static void wait_vsync(void) {
    *(PIXEL_BUF_CTRL) = 1;
    while (*(PIXEL_BUF_CTRL + 3) & 1);
}

static void swap_buffers(void) {
    volatile unsigned short *tmp;
    wait_vsync();
    tmp        = g_draw_buf;
    g_draw_buf = g_show_buf;
    g_show_buf = tmp;
    *(PIXEL_BUF_CTRL + 1) = (uint32_t)(uintptr_t)g_draw_buf;
}

/* ============================================================================
   SIMPLE BITMAP FONT (5x7, ASCII 32..90)
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
    int ci2, row;
    unsigned char bits;
    if (ci < 0 || ci >= 59) return;
    for (ci2 = 0; ci2 < 5; ci2++) {
        bits = FONT5X7[ci][ci2];
        for (row = 0; row < 7; row++)
            if (bits & (1 << row))
                draw_pixel(x + ci2, y + row, col);
    }
}

static void draw_string(int x, int y, const char *s, unsigned short col) {
    while (*s) {
        char c = (*s >= 'a' && *s <= 'z') ? (*s - 32) : *s;
        draw_char(x, y, c, col);
        x += 6; s++;
    }
}

static void draw_number(int x, int y, int n, unsigned short col) {
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (n == 0) { buf[i--] = '0'; }
    while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
    draw_string(x, y, buf + i + 1, col);
}

/* ============================================================================
   TILE DRAWING
   ============================================================================ */
static void draw_tile(int tx, int ty, int tile_id) {
    int boff = g_tile_meta[ty/TILE_H][tx/TILE_W + g_camera_x/TILE_W].anim_off;
    int sy = ty - boff;
    switch (tile_id) {
    case TILE_GROUND:
        draw_rect(tx, sy, TILE_W, TILE_H, COL(139,90,43));
        draw_rect(tx, sy, TILE_W, 4, COL(80,200,80));
        break;
    case TILE_BRICK:
        draw_rect(tx, sy, TILE_W, TILE_H, C_BROWN);
        draw_rect(tx, sy+7, TILE_W, 2, C_DKBROWN);
        draw_rect(tx+7, sy, 2, 7, C_DKBROWN);
        draw_rect(tx+3, sy+9, 2, 7, C_DKBROWN);
        break;
    case TILE_QBLOCK:
        draw_rect(tx, sy, TILE_W, TILE_H, C_YELLOW);
        draw_rect(tx+4, sy+3, 8, 10, C_ORANGE);
        draw_char(tx+5, sy+4, '?', C_WHITE);
        break;
    case TILE_QBLOCK_USED:
        draw_rect(tx, sy, TILE_W, TILE_H, C_GRAY);
        draw_rect(tx+1, sy+1, TILE_W-2, 2, C_LTGRAY);
        break;
    case TILE_PIPE:
        draw_rect(tx+1, sy, TILE_W-2, TILE_H, C_PIPE_G);
        draw_rect(tx+1, sy, 2, TILE_H, C_PIPE_DK);
        /* draw pipe top cap if this is the top row of the pipe */
        if (ty/TILE_H > 0 && g_map[ty/TILE_H - 1][tx/TILE_W + g_camera_x/TILE_W] != TILE_PIPE) {
            draw_rect(tx, sy, TILE_W, 5, C_PIPE_DK);
            draw_rect(tx+1, sy, TILE_W-2, 4, C_PIPE_G);
        }
        break;
    case TILE_GOAL:
        draw_rect(tx+7, sy, 2, TILE_H, C_FLAGPOLE);
        draw_rect(tx+3, sy, 10, 3, C_GREEN);
        draw_rect(tx+3, sy, 3, 6, C_GREEN);
        break;
    default:
        break;
    }
}

/* ============================================================================
   SPRITE DRAWING (Mario and Goomba using coloured rectangles)
   ============================================================================ */
#define SP(rx,ry,col) draw_pixel(x + (flip ? (15-(rx)) : (rx)), y+(ry), col)
static void sp_run(int x, int y, int rx, int ry, int len,
                   unsigned short col, int flip) {
    int i;
    for (i = 0; i < len; i++) SP(rx+i, ry, col);
}
#define SPR(rx,ry,len,col) sp_run(x, y, rx, ry, len, col, flip)

static void draw_mario(int x, int y, int flip) {
    /* hat */
    SPR(4, 0, 8, C_RED);
    SPR(3, 1,10, C_RED);
    /* face */
    SPR(3, 2,10, C_BROWN);
    SPR(2, 3, 4, C_TAN); SPR(7,3,4,C_TAN);
    SPR(2, 4,12, C_TAN);
    SP(4,4, C_BLACK); SP(10,4, C_BLACK);
    SPR(3, 6, 4, C_BROWN); SPR(9,6,4,C_BROWN);
    /* body */
    SPR(2, 7,12, C_RED);
    SPR(1, 8,14, C_RED);
    SPR(1, 9,14, COL(0,0,200));
    SPR(2,10,12, COL(0,0,200));
    /* legs */
    SPR(2,11, 5, COL(0,0,200)); SPR(9,11,5,COL(0,0,200));
    SPR(2,12, 5, COL(0,0,200)); SPR(9,12,5,COL(0,0,200));
    /* shoes */
    SPR(1,13, 6, C_BLACK); SPR(9,13,6,C_BLACK);
    SPR(1,14, 7, C_BLACK); SPR(8,14,7,C_BLACK);
}

static void draw_goomba(int x, int y, int frame) {
    int flip = 0;
    unsigned short body = COL(180,100,40);
    unsigned short dark = COL(100,50,10);
    unsigned short feet = COL(80,40,10);
    SPR(2,0,12,body); SPR(1,1,14,body);
    SPR(0,2,16,body); SPR(0,3,16,body);
    SPR(0,4,16,body); SPR(0,5,16,body);
    SPR(2,2,3,C_WHITE); SPR(11,2,3,C_WHITE);
    SP(3,2,C_BLACK); SP(12,2,C_BLACK);
    SP(2,3,C_BLACK); SP(11,3,C_BLACK);
    SPR(2,1,4,dark); SPR(10,1,4,dark);
    SPR(0,6,16,body); SPR(0,7,16,body);
    SPR(1,8,14,body); SPR(2,9,12,body);
    SPR(3,10,10,body); SPR(4,11,8,body);
    if (frame == 0) {
        SPR(1,12,5,feet); SPR(9,12,7,dark);
        SPR(0,13,6,feet); SPR(9,13,7,dark);
    } else {
        SPR(0,12,7,dark); SPR(10,12,5,feet);
        SPR(0,13,7,dark); SPR(9,13,7,feet);
    }
}

static void draw_goomba_flat(int x, int y) {
    int flip = 0;
    unsigned short body = COL(180,100,40);
    SPR(0,12,16,body); SPR(0,13,16,body);
    SPR(0,14,16,body); SPR(0,15,16,body);
}

/* ============================================================================
   PS/2 INPUT
   ============================================================================ */
static void ps2_read(void) {
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int data, rvalid, i;
    unsigned char scan;
    for (i = 0; i < 256; i++) g_keys_prev[i] = g_keys[i];
    for (i = 0; i < 16; i++) {
        data   = *ps2;
        rvalid = (data >> 15) & 1;
        if (!rvalid) break;
        scan = data & 0xFF;
        if (scan == 0xF0) {
            g_break_code = 1;
        } else if (g_break_code) {
            g_keys[scan] = 0;
            g_break_code = 0;
        } else {
            g_keys[scan] = 1;
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
           t == TILE_QBLOCK_USED || t == TILE_PIPE;
}

static int world_tile(int px, int py) {
    int tx = px / TILE_W, ty = py / TILE_H;
    if (tx < 0 || tx >= MAP_COLS || ty < 0 || ty >= MAP_ROWS) return TILE_GROUND;
    return g_map[ty][tx];
}

/* ============================================================================
   COLLISION RESOLUTION
   ============================================================================ */
static void resolve_player_col(void) {
    Player *p = &g_player;
    int vx = p->vx >> FIXED_SHIFT;
    int vy = p->vy >> FIXED_SHIFT;
    int i, tx, ty, tile, foot, head, left, right;

    /* X axis */
    p->x += vx;
    foot = p->y + p->h - 2;
    head = p->y + 4;
    if (vx > 0) {
        right = p->x + p->w - 1;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(right, i);
            if (tile_solid(tile)) {
                tx = (right / TILE_W) * TILE_W;
                p->x = tx - p->w; p->vx = 0; break;
            }
        }
    } else if (vx < 0) {
        left = p->x;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(left, i);
            if (tile_solid(tile)) {
                tx = (left / TILE_W + 1) * TILE_W;
                p->x = tx; p->vx = 0; break;
            }
        }
    }

    /* Y axis */
    p->on_ground = 0;
    p->y += vy;
    left  = p->x + 2;
    right = p->x + p->w - 3;

    if (vy > 0) {
        foot = p->y + p->h - 1;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, foot);
            if (tile_solid(tile)) {
                ty = (foot / TILE_H) * TILE_H;
                p->y = ty - p->h; p->vy = 0; p->on_ground = 1; break;
            }
        }
    } else if (vy < 0) {
        head = p->y;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, head);
            if (tile_solid(tile)) {
                ty = (head / TILE_H + 1) * TILE_H;
                p->y = ty; p->vy = 0;
                /* hit block from below */
                int btx = i / TILE_W, bty = head / TILE_H;
                int bt = g_map[bty][btx];
                if (bt == TILE_QBLOCK) {
                    g_map[bty][btx] = TILE_QBLOCK_USED;
                    g_tile_meta[bty][btx].anim_timer = 8;
                    g_tile_meta[bty][btx].anim_off   = 4;
                    /* spawn coin entity */
                    int j;
                    for (j = 0; j < MAX_ENTITIES; j++) {
                        if (!g_ents[j].active) {
                            g_ents[j].active   = 1;
                            g_ents[j].type     = ENT_COIN;
                            g_ents[j].x        = btx * TILE_W;
                            g_ents[j].y        = bty * TILE_H - TILE_H;
                            g_ents[j].vx       = 0;
                            g_ents[j].vy       = -6 * (1 << FIXED_SHIFT);
                            g_ents[j].w        = 8;
                            g_ents[j].h        = 8;
                            g_ents[j].lifetime = 40;
                            break;
                        }
                    }
                    g_player.score += 200;
                    g_player.coins++;
                } else if (bt == TILE_BRICK && p->power != PU_SMALL) {
                    g_map[bty][btx] = TILE_AIR;
                    g_player.score += 50;
                } else {
                    g_tile_meta[bty][btx].anim_timer = 6;
                    g_tile_meta[bty][btx].anim_off   = 3;
                }
                break;
            }
        }
    }

    /* Fall off bottom = die */
    if (p->y > MAP_ROWS * TILE_H) {
        if (p->state != PS_DEAD) {
            p->state = PS_DEAD;
            p->vy    = JUMP_VEL * (1 << FIXED_SHIFT) / 2;
            p->vx    = 0;
            p->lives--;
        }
    }

    if (p->x < 0) { p->x = 0; p->vx = 0; }
}

static void resolve_entity_col(Entity *e) {
    int vx = e->vx >> FIXED_SHIFT;
    int vy = e->vy >> FIXED_SHIFT;
    int left, right, foot, head, i, tile, tx, ty;

    e->on_ground = 0;
    e->x += vx;
    foot = e->y + e->h - 2;
    head = e->y + 4;
    if (vx != 0) {
        int edge = vx > 0 ? e->x + e->w - 1 : e->x;
        for (i = head; i <= foot; i += 8) {
            tile = world_tile(edge, i);
            if (tile_solid(tile)) {
                if (vx > 0) { tx = (edge / TILE_W)*TILE_W; e->x = tx - e->w; }
                else        { tx = (edge / TILE_W+1)*TILE_W; e->x = tx; }
                e->vx = -e->vx;
                e->facing = -e->facing;
                break;
            }
        }
    }

    e->y += vy;
    left  = e->x + 1;
    right = e->x + e->w - 2;
    if (vy >= 0) {
        foot = e->y + e->h - 1;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, foot);
            if (tile_solid(tile)) {
                ty = (foot / TILE_H) * TILE_H;
                e->y = ty - e->h; e->vy = 0; e->on_ground = 1; break;
            }
        }
    } else {
        head = e->y;
        for (i = left; i <= right; i += 8) {
            tile = world_tile(i, head);
            if (tile_solid(tile)) {
                ty = (head / TILE_H + 1) * TILE_H;
                e->y = ty; e->vy = 0; break;
            }
        }
    }

    if (e->y > MAP_ROWS * TILE_H + 64) e->active = 0;
}

/* ============================================================================
   SCORE / FLOATING TEXT
   ============================================================================ */
static void add_score(int pts, int sx, int sy) {
    int i;
    g_player.score += pts;
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

/* ============================================================================
   PLAYER UPDATE
   ============================================================================ */
static int g_jump_held;

static void update_player(void) {
    Player *p = &g_player;

    if (p->state == PS_WIN) {
        p->x += 2;
        if (p->x > (MAP_COLS - 2) * TILE_W) g_state = GS_WIN;
        return;
    }

    if (p->state == PS_DEAD) {
        p->vy += GRAVITY * (1 << FIXED_SHIFT);
        p->y  += p->vy >> FIXED_SHIFT;
        if (p->y > SCREEN_H + 32) {
            g_state = (p->lives <= 0) ? GS_GAMEOVER : GS_DEAD;
        }
        return;
    }

    if (p->invincible > 0) p->invincible--;

    /* Horizontal movement */
    int accel   = 1 << (FIXED_SHIFT - 1);
    int friction = 1 << (FIXED_SHIFT - 2);

    if (key_held(KEY_LEFT)) {
        p->vx -= accel;
        if (p->vx < -MOVE_SPEED * (1 << FIXED_SHIFT))
            p->vx = -MOVE_SPEED * (1 << FIXED_SHIFT);
        p->facing = -1;
    } else if (key_held(KEY_RIGHT)) {
        p->vx += accel;
        if (p->vx > MOVE_SPEED * (1 << FIXED_SHIFT))
            p->vx = MOVE_SPEED * (1 << FIXED_SHIFT);
        p->facing = 1;
    } else {
        if (p->vx > 0) p->vx -= friction;
        if (p->vx < 0) p->vx += friction;
        if (p->vx > -friction && p->vx < friction) p->vx = 0;
    }

    /* Jump with W key */
    if (key_pressed(KEY_W) && p->on_ground) {
        p->vy = JUMP_VEL * (1 << FIXED_SHIFT);
        g_jump_held = 1;
    }
    if (!key_held(KEY_W)) g_jump_held = 0;

    /* Gravity */
    p->vy += GRAVITY * (1 << FIXED_SHIFT);
    if (p->vy > MAX_FALL_SPEED * (1 << FIXED_SHIFT))
        p->vy = MAX_FALL_SPEED * (1 << FIXED_SHIFT);

    resolve_player_col();

    /* State machine */
    if (!p->on_ground)
        p->state = (p->vy < 0) ? PS_JUMPING : PS_FALLING;
    else if (p->vx != 0)
        p->state = PS_WALKING;
    else
        p->state = PS_STANDING;

    /* Animation */
    p->frame_timer++;
    if (p->state == PS_WALKING && p->frame_timer >= 8) {
        p->frame = (p->frame + 1) % 2;
        p->frame_timer = 0;
    }

    if (p->x < g_camera_x) p->x = g_camera_x;

    /* Check goal */
    int ptx = (p->x + p->w/2) / TILE_W;
    int pty = (p->y + p->h/2) / TILE_H;
    if (ptx >= 0 && ptx < MAP_COLS && pty >= 0 && pty < MAP_ROWS)
        if (g_map[pty][ptx] == TILE_GOAL) {
            add_score(5000, p->x - g_camera_x, p->y);
            p->state = PS_WIN;
        }
}

/* ============================================================================
   ENEMY UPDATE
   ============================================================================ */
static void update_enemies(void) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents[i];
        if (!e->active) continue;

        if (e->type == ENT_COIN) {
            e->vy += GRAVITY * (1 << FIXED_SHIFT);
            e->y  += e->vy >> FIXED_SHIFT;
            e->lifetime--;
            if (e->lifetime <= 0) e->active = 0;
            continue;
        }

        if (e->type == ENT_GOOMBA) {
            if (e->dead) {
                e->squished--;
                if (e->squished <= 0) e->active = 0;
                continue;
            }

            /* Only update when near camera */
            if (e->x < g_camera_x - 32 || e->x > g_camera_x + SCREEN_W + 32) continue;

            e->vy += GRAVITY * (1 << FIXED_SHIFT);
            if (e->vy > MAX_FALL_SPEED * (1 << FIXED_SHIFT))
                e->vy = MAX_FALL_SPEED * (1 << FIXED_SHIFT);

            resolve_entity_col(e);

            e->frame_timer++;
            if (e->frame_timer >= 12) { e->frame ^= 1; e->frame_timer = 0; }

            /* Player collision */
            Player *p = &g_player;
            int hit = (p->x < e->x + e->w && p->x + p->w > e->x &&
                       p->y < e->y + e->h && p->y + p->h > e->y);
            if (hit) {
                if (p->invincible > 0) {
                    e->active = 0;
                    add_score(200, e->x - g_camera_x, e->y);
                } else {
                    int stomp = (p->vy > 0) && (p->y + p->h - 8 <= e->y + 4);
                    if (stomp) {
                        e->dead = 1; e->squished = 20;
                        add_score(100, e->x - g_camera_x, e->y);
                        p->vy = JUMP_VEL * (1 << FIXED_SHIFT) / 2;
                    } else {
                        /* Hurt player */
                        if (p->power != PU_SMALL) {
                            p->power = PU_SMALL;
                            p->h = 16;
                            p->invincible = 120;
                        } else {
                            if (p->state != PS_DEAD) {
                                p->state = PS_DEAD;
                                p->vy = JUMP_VEL * (1 << FIXED_SHIFT) / 2;
                                p->vx = 0;
                                p->lives--;
                            }
                        }
                    }
                }
            }
        }
    }
}

/* ============================================================================
   CAMERA UPDATE
   ============================================================================ */
static void update_camera(void) {
    int target = g_player.x - SCREEN_W * 2 / 5;
    g_camera_x += (target - g_camera_x) / 4;
    if (g_camera_x < 0) g_camera_x = 0;
    if (g_camera_x > MAP_COLS * TILE_W - SCREEN_W)
        g_camera_x = MAP_COLS * TILE_W - SCREEN_W;
}

/* ============================================================================
   TILE ANIMATION UPDATE
   ============================================================================ */
static void update_tiles(void) {
    int ty, tx;
    for (ty = 0; ty < MAP_ROWS; ty++) {
        for (tx = 0; tx < MAP_COLS; tx++) {
            TileMeta *m = &g_tile_meta[ty][tx];
            if (m->anim_timer > 0) {
                m->anim_timer--;
                m->anim_off = (m->anim_timer == 0) ? 0 : ((m->anim_timer > 4) ? 4 : -4);
            }
        }
    }
}

static void update_float_texts(void) {
    int i;
    for (i = 0; i < MAX_FLOAT; i++)
        if (g_floats[i].timer > 0) { g_floats[i].y--; g_floats[i].timer--; }
}

/* ============================================================================
   RENDERING
   ============================================================================ */
static void draw_background(void) {
    draw_rect(0, 0, SCREEN_W, SCREEN_H, C_SKY);
    /* Simple parallax clouds */
    int cx = g_camera_x / 2;
    int c1x = ((200 - cx) % SCREEN_W + SCREEN_W) % SCREEN_W;
    int c2x = ((380 - cx) % SCREEN_W + SCREEN_W) % SCREEN_W;
    draw_rect(c1x, 20, 32, 12, C_CLOUD);
    draw_rect(c1x+4, 14, 24, 12, C_CLOUD);
    draw_rect(c2x, 40, 28, 10, C_CLOUD);
    draw_rect(c2x+4, 34, 20, 10, C_CLOUD);
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
            draw_tile(tx * TILE_W - g_camera_x, ty * TILE_H, tile);
        }
    }
}

static void draw_player_sprite(void) {
    Player *p = &g_player;
    int sx = p->x - g_camera_x;
    int sy = p->y;
    int flip = (p->facing < 0) ? 1 : 0;

    if (p->invincible > 0 && (g_frame / 4) % 2) return;

    if (p->state == PS_DEAD) {
        draw_mario(sx, sy, flip);
        /* X eyes to indicate dead */
        draw_pixel(sx+4, sy+3, C_BLACK); draw_pixel(sx+5, sy+5, C_BLACK);
        draw_pixel(sx+10,sy+3, C_BLACK); draw_pixel(sx+11,sy+5, C_BLACK);
        return;
    }
    draw_mario(sx, sy, flip);
}

static void draw_entities(void) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents[i];
        if (!e->active) continue;
        int sx = e->x - g_camera_x;
        int sy = e->y;
        if (sx < -32 || sx > SCREEN_W + 32) continue;

        if (e->type == ENT_COIN) {
            draw_rect(sx+4, sy, 8, 8, C_COIN);
            continue;
        }
        if (e->type == ENT_GOOMBA) {
            if (e->dead) draw_goomba_flat(sx, sy);
            else         draw_goomba(sx, sy, e->frame);
        }
    }
}

static void draw_hud(void) {
    draw_rect(0, 0, SCREEN_W, 12, COL(20,20,60));
    draw_string(2,  2, "MARIO", COL(200,200,200));
    draw_number(38, 2, g_player.score, C_WHITE);
    draw_rect(120, 3, 6, 6, C_COIN);
    draw_string(128, 2, "X", C_WHITE);
    draw_number(136, 2, g_player.coins, C_WHITE);
    draw_string(170, 2, "WORLD 1-1", C_WHITE);
    draw_string(262, 2, "TIME", C_WHITE);
    draw_number(292, 2, g_timer / 30, (g_timer < 150) ? C_RED : C_WHITE);
    draw_string(2, SCREEN_H-10, "LIVES:", C_WHITE);
    draw_number(40, SCREEN_H-10, g_player.lives, C_YELLOW);

    int i;
    for (i = 0; i < MAX_FLOAT; i++)
        if (g_floats[i].timer > 0)
            draw_number(g_floats[i].x, g_floats[i].y, g_floats[i].score, C_YELLOW);
}

static void draw_title(void) {
    clear_screen(COL(20,20,80));
    draw_string(80, 50,  "SUPER MARIO", COL(255,50,50));
    draw_string(95, 64,  "CPU EDITION", C_YELLOW);
    draw_rect(40, 80, SCREEN_W-80, 2, C_WHITE);
    draw_string(55, 95,  "PRESS ENTER TO PLAY", C_WHITE);
    draw_string(55, 110, "W = JUMP", COL(200,200,200));
    draw_string(55, 122, "LEFT / RIGHT = MOVE", COL(200,200,200));
    draw_string(55, 134, "P = PAUSE", COL(200,200,200));
    draw_string(55, 150, "STOMP GOOMBAS TO SCORE", COL(180,180,255));
    draw_string(55, 162, "HIT ? BLOCKS FOR COINS", COL(180,180,255));
    draw_string(55, 174, "REACH THE FLAGPOLE!", COL(100,255,100));
}

static void draw_win_screen(void) {
    clear_screen(COL(20,80,20));
    draw_string(80,  80, "YOU WIN!", C_YELLOW);
    draw_string(60, 100, "FINAL SCORE:", C_WHITE);
    draw_number(60, 112, g_player.score, C_YELLOW);
    draw_string(50, 140, "PRESS ENTER TO PLAY AGAIN", COL(180,255,180));
}

static void draw_gameover_screen(void) {
    clear_screen(COL(60,10,10));
    draw_string(105, 80, "GAME OVER", C_RED);
    draw_string(60, 110, "FINAL SCORE:", C_WHITE);
    draw_number(60, 122, g_player.score, C_YELLOW);
    draw_string(50, 150, "PRESS ENTER TO TRY AGAIN", COL(255,180,180));
}

static void draw_pause_overlay(void) {
    int y;
    for (y = 0; y < SCREEN_H; y += 2)
        draw_rect(0, y, SCREEN_W, 1, C_BLACK);
    draw_string(115, 110, "PAUSED", C_WHITE);
    draw_string(80,  126, "P = RESUME", COL(200,200,200));
}

static void draw_dead_screen(void) {
    draw_string(95, 110, "YOU DIED!", C_RED);
    draw_string(60, 126, "PRESS ENTER TO CONTINUE", C_WHITE);
    draw_number(95, 142, g_player.lives, C_YELLOW);
    draw_string(109,142, "LIVES LEFT", C_WHITE);
}

/* ============================================================================
   INITIALIZATION
   ============================================================================ */
static void reset_map(void) {
    int i, j;
    for (i = 0; i < MAP_ROWS; i++)
        for (j = 0; j < MAP_COLS; j++) {
            g_map[i][j] = MAP[i][j];
            g_tile_meta[i][j].hit = 0;
            g_tile_meta[i][j].anim_timer = 0;
            g_tile_meta[i][j].anim_off   = 0;
        }
}

static void clear_entities(void) {
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) g_ents[i].active = 0;
    for (i = 0; i < MAX_FLOAT;    i++) g_floats[i].timer = 0;
}

static void init_level(void) {
    int i;
    reset_map();
    clear_entities();

    /* Spawn goombas from table */
    for (i = 0; ENEMY_SPAWNS[i][2] != ENT_NONE; i++) {
        Entity *e = 0;
        int j;
        for (j = 0; j < MAX_ENTITIES; j++) {
            if (!g_ents[j].active) { e = &g_ents[j]; break; }
        }
        if (!e) break;
        e->active   = 1;
        e->type     = ENT_GOOMBA;
        e->x        = ENEMY_SPAWNS[i][0];
        e->y        = ENEMY_SPAWNS[i][1];
        e->vx       = -1 * (1 << FIXED_SHIFT);
        e->vy       = 0;
        e->w        = 16; e->h = 16;
        e->facing   = -1;
        e->dead     = 0;
        e->squished = 0;
        e->frame    = 0;
        e->frame_timer = 0;
    }

    /* Player init */
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
    g_player.coins       = 0;

    g_camera_x   = 0;
    g_timer      = 30 * 200;   /* 200 seconds */
    g_jump_held  = 0;
    g_state      = GS_PLAYING;
}

static void init_game(void) {
    int i;
    for (i = 0; i < 256; i++) { g_keys[i] = 0; g_keys_prev[i] = 0; }
    g_break_code = 0;
    g_camera_x   = 0;
    g_frame      = 0;
    g_state      = GS_TITLE;
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
        if (g_timer > 0) g_timer--;
        else {
            if (g_player.state != PS_DEAD) {
                g_player.state = PS_DEAD;
                g_player.vy = JUMP_VEL * (1 << FIXED_SHIFT) / 2;
                g_player.vx = 0;
                g_player.lives--;
            }
        }

        if (key_pressed(KEY_P)) { g_state = GS_PAUSED; break; }

        update_player();
        update_enemies();
        update_camera();
        update_tiles();
        update_float_texts();

        draw_background();
        draw_tilemap();
        draw_entities();
        draw_player_sprite();
        draw_hud();
        break;

    case GS_PAUSED:
        draw_background();
        draw_tilemap();
        draw_entities();
        draw_player_sprite();
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
    *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_BACK;
    wait_vsync();
    g_show_buf = (volatile unsigned short *)(uintptr_t)(*PIXEL_BUF_CTRL);
    *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_BASE;
    g_draw_buf = (volatile unsigned short *)PIXEL_BUFFER_BASE;

    clear_screen(C_SKY);
    swap_buffers();
    clear_screen(C_SKY);

    init_game();

    while (1) {
        game_tick();
        swap_buffers();
    }

    return 0;
}
