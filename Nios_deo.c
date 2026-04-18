#include <stdint.h>

// #define PIXEL_BUF_CTRL ((volatile uint32_t *) 0xFF203020)
// #define PIXEL_BUFFER_1 0x08000000
// #define PIXEL_BUFFER_2 0x00000000

#define FRAMEBUFFER ((volatile uint16_t *) 0x08000000)

#define PS2_BASE ((volatile uint32_t *) 0xFF200100)
#define SW_BASE ((volatile uint32_t *) 0xFF200040)
#define KEY_BASE ((volatile uint32_t *) 0xFF200050)
uint32_t sw_state, prev_sw_state;
uint32_t btn_state, prev_btn_state;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define STRIDE 320

#define GRAVITY 1
#define JUMP_VELOCITY -15
#define SPEED 2
#define FALL_SPEED 10
#define FIXED_SHIFT 4

#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define MAP_COLS 60
#define MAP_ROWS 15

#define TILE_AIR 0
#define TILE_GROUND 1
#define TILE_BRICK 2
#define TILE_QBLOCK 3
#define TILE_QBLOCK_USED 4
#define TILE_PIPE 5
#define TILE_GOAL 6

#define COL(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)
#define SKY COL(92,148,252)
#define BLACK COL(0,0,0)
#define WHITE COL(255,255,255)
#define RED COL(220,50,50)
#define BROWN COL(130,70,30)
#define DKBROWN COL(90,45,15)
#define GREEN   COL(50,200,50)
#define YELLOW  COL(255,220,0)
#define ORANGE  COL(255,140,0)
#define TAN     COL(240,200,140)
#define GRAY    COL(150,150,150)
#define LTGRAY  COL(210,210,210)
#define PIPE_G  COL(60,180,60)
#define PIPE_DK COL(20,120,20)
#define COIN    COL(255,200,0)
#define CLOUD   COL(245,245,255)
#define FLAGPOLE COL(180,180,180)

#define LEFT_KEY 0X6B
#define RIGHT_KEY 0x74
#define KEY_W 0x1D
#define KEY_ENTER 0x5A

typedef enum{ ENT_NONE=0,ENT_ENEMY , ENT_COIN} EntityType;
typedef enum{PS_STANDING,PS_WALKING,PS_JUMPING,PS_FALLING,PS_DEAD,PS_WIN} PlayerState;
typedef enum{ GS_TITLE , GS_PLAYING, GS_DEAD, GS_WIN,GS_GAMEOVER} GameState;

typedef struct{
    int x,y,vx,vy,w,h;
    int frame,frame_timer, facing, on_ground;
    PlayerState state;
    int lives,score,coins;
}Player;

typedef struct{
    int active;
    EntityType type;
    int x,y,vx,vy,w,h;
    int facing,frame,frame_timer,on_ground;
    int dead,squished;
    int lifetime;
} Entity;

typedef struct{
    int hit, anim_timer,anim_off;
} TileMeta;

typedef struct {
    int score, x, y, timer;
} FloatText;

#define MAX_ENTITIES 20
#define MAX_FLOAT     6

const unsigned char MAP[MAP_ROWS][MAP_COLS] = {
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

unsigned char map[MAP_ROWS][MAP_COLS];
TileMeta tile_meta[MAP_ROWS][MAP_COLS];

volatile unsigned short* draw_buff;
unsigned short* show_buff;

Player player;
Entity ents[MAX_ENTITIES];
FloatText floats[MAX_FLOAT];
int camera;
GameState gstate;
int timer;
int frame;

unsigned char keys[256];
unsigned char prev_keys[256];
int break_code;

const int ENEMY_SPAWNS[][3]={
    {10*16,12*16-16 , ENT_ENEMY},
    {14*16,12*16-16 , ENT_ENEMY},
    {20*16,12*16-16 , ENT_ENEMY},
    {25*16,12*16-16 , ENT_ENEMY},
    {32*16,12*16-16 , ENT_ENEMY},
    {38*16,12*16-16 , ENT_ENEMY},
    {45*16,12*16-16 , ENT_ENEMY},
    {-1,-1,ENT_NONE}
};

void draw_pixel(int x,int y,unsigned short col){
    if(x<0 || x>=SCREEN_WIDTH || y<0 || y>=SCREEN_HEIGHT) return;
    *(draw_buff + y*STRIDE +x)=col;
}

void draw_rect(int x,int y,int w,int h,unsigned short col){
    int x0=x<0 ? 0:x;
    int y0=y<0 ? 0:y;
    int x1=x+w>SCREEN_WIDTH ? SCREEN_WIDTH: x+w;
    int y1=y+h>SCREEN_HEIGHT ? SCREEN_HEIGHT: y+h;
    int j,i;
    for(j=y0;j<y1;j++){
        for(i=x0;i<x1;i++){
            *(draw_buff + j*STRIDE + i)=col;
        }
    }
}

void clear_screen(unsigned short col){
    int i;
    for(i=0;i<STRIDE*SCREEN_HEIGHT;i++){
        *(draw_buff + i)=col;
    }
}

// void wait_vsync(void) {
//     *(PIXEL_BUF_CTRL) = 1;
//     while (*(PIXEL_BUF_CTRL + 3) & 1);
// }

// void swap_buffers(void){
//     unsigned short *tmp;
//     wait_vsync();
//     tmp=draw_buff;
//     draw_buff=show_buff;
//     show_buff=tmp;
//     *(PIXEL_BUF_CTRL +1)= (uint32_t)(uintptr_t)draw_buff;
// }

const unsigned char FONT[][5] = {
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

void draw_char(int x,int y,char c, unsigned short col){
    int ci=(int)c-32 ;
    unsigned char bits;
    if(ci<0 || ci>=59) return;
    int ci2,row;
    for(ci2=0 ; ci2<5 ; ci2++){
        bits=FONT[ci][ci2];
        for(row=0;row<7 ;row++){
            if(bits & (1<<row)) draw_pixel(x+ci2,y+row,col);
        }
    }
}

void draw_string(int x,int y, const char * s, unsigned short col){
    while(*s){
        char c=(*s >='a' && *s<= 'z') ? (*s -32) : *s;
        draw_char(x,y,c,col);
        x+=6; s++;
    }
}

void draw_number(int x, int y, int n, unsigned short col) {
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (n == 0) { buf[i--] = '0'; }
    while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
    draw_string(x, y, buf + i + 1, col);
}

void draw_tile(int x,int y,int tile_id){
    int buff=tile_meta[y/TILE_HEIGHT][x/TILE_WIDTH + camera/TILE_WIDTH].anim_off;
    int sy=y-buff;
    switch (tile_id){
        case TILE_GROUND:
            draw_rect(x,sy,TILE_WIDTH,TILE_HEIGHT, COL(139,90,43));
            draw_rect(x,sy,TILE_WIDTH,4, COL(80,200,80));
            break;
        case TILE_BRICK:
            draw_rect(x,sy,TILE_WIDTH,TILE_HEIGHT,BROWN);
            draw_rect(x,sy+7,TILE_WIDTH,2,DKBROWN);
            draw_rect(x+7,sy,2,7,DKBROWN);
            draw_rect(x+3,sy+9,2,7,DKBROWN);
            break;
        case TILE_QBLOCK:
            draw_rect(x, sy, TILE_WIDTH, TILE_HEIGHT, YELLOW);
            draw_rect(x+4, sy+3, 8, 10, ORANGE);
            draw_char(x+5, sy+4, '?', WHITE);
            break;
        case TILE_QBLOCK_USED:
            draw_rect(x, sy, TILE_WIDTH, TILE_HEIGHT,GRAY);
            draw_rect(x+1, sy+1, TILE_WIDTH-2, 2, LTGRAY);
            break;
        case TILE_PIPE:
            draw_rect(x+1, sy, TILE_WIDTH-2, TILE_HEIGHT, PIPE_G);
            draw_rect(x+1, sy, 2, TILE_HEIGHT, PIPE_DK);
            if (y/TILE_HEIGHT > 0 && map[y/TILE_HEIGHT - 1][x/TILE_WIDTH + camera/TILE_WIDTH] != TILE_PIPE) {
                draw_rect(x, sy, TILE_WIDTH, 5, PIPE_DK);
                draw_rect(x+1, sy, TILE_WIDTH-2, 4, PIPE_G);
            }
            break;
        case TILE_GOAL:
            draw_rect(x+7, sy, 2, TILE_HEIGHT, FLAGPOLE);
            draw_rect(x+3, sy, 10, 3, GREEN);
            draw_rect(x+3, sy, 3, 6, GREEN);
            break;
        default:
            break;
    }
}

static inline void sp_run(int x,int y,int rx,int ry,int len,unsigned short col,int flip){
    int i;
    for(i=0;i<len;i++){
        int px=x+(flip?(15-(rx+i)):(rx+i));
        draw_pixel(px,y+ry,col);
    }
}
#define SPR(rx,ry,len,col) sp_run(x,y,rx,ry,len,col,flip)
void draw_mario(int x,int y, int flip){
    SPR(4,0,8,RED);   SPR(3,1,10,RED);
    SPR(3,2,10,BROWN);
    SPR(2,3,4,TAN);   SPR(7,3,4,TAN);
    SPR(2,4,12,TAN);
    draw_pixel(x + (flip?11:4), y+4, BLACK);
    draw_pixel(x + (flip?5:10), y+4, BLACK);
    SPR(3,6,4,BROWN); SPR(9,6,4,BROWN);
    SPR(2,7,12,RED);  SPR(1,8,14,RED);
    SPR(1,9,14,COL(0,0,200));
    SPR(2,10,12,COL(0,0,200));
    SPR(2,11,5,COL(0,0,200)); SPR(9,11,5,COL(0,0,200));
    SPR(2,12,5,COL(0,0,200)); SPR(9,12,5,COL(0,0,200));
    SPR(1,13,6,BLACK); SPR(9,13,6,BLACK);
    SPR(1,14,7,BLACK); SPR(8,14,7,BLACK);
}

void draw_enemy(int x, int y, int frame) {
    int flip = 0;
    unsigned short body = COL(180,100,40),dark = COL(100,50,10),feet = COL(80,40,10);

    SPR(2,0,12,body); SPR(1,1,14,body);
    int i;
    for(i=2;i<=5;i++) SPR(0,i,16,body);

    SPR(2,2,3,WHITE); SPR(11,2,3,WHITE);
    draw_pixel(x+3,y+2,BLACK); draw_pixel(x+12,y+2,BLACK);
    draw_pixel(x+2,y+3,BLACK); draw_pixel(x+11,y+3,BLACK);

    SPR(2,1,4,dark); SPR(10,1,4,dark);

    for(i=6;i<=7;i++) SPR(0,i,16,body);
    SPR(1,8,14,body); SPR(2,9,12,body);
    SPR(3,10,10,body); SPR(4,11,8,body);

    if(frame==0){
        SPR(1,12,5,feet); SPR(9,12,7,dark);
        SPR(0,13,6,feet); SPR(9,13,7,dark);
    } else {
        SPR(0,12,7,dark); SPR(10,12,5,feet);
        SPR(0,13,7,dark); SPR(9,13,7,feet);
    }
}

void draw_enemy_flat(int x, int y) {
    int flip = 0;
    unsigned short body = COL(180,100,40);
    int i;
    for(i=12;i<=15;i++) SPR(0,i,16,body);
}

void read_inputs(void) {
    prev_sw_state = sw_state;
    sw_state = *SW_BASE;

    prev_btn_state = btn_state;
    btn_state = *KEY_BASE;
}

int sw_held(int bit) {
    return (sw_state >> bit) & 1;
}

int btn_held(int bit) { 
    return (btn_state >> bit) & 1; 
}

int btn_pressed(int bit) { 
    return ((btn_state >> bit) & 1) && !((prev_btn_state >> bit) & 1); 
}

int tile_solid(int t){
    return t==TILE_GROUND|| t==TILE_BRICK || t==TILE_QBLOCK || t==TILE_QBLOCK_USED || t==TILE_PIPE;
}

int world_tile(int x,int y){
    int tx=x/TILE_WIDTH,ty=y/TILE_HEIGHT;
    if(tx<0 || tx>=MAP_COLS || ty<0 || ty>= MAP_ROWS) return TILE_GROUND;
    return map[ty][tx];
}

void player_col(void){
    Player *p=&player;
    int vx=p->vx >> FIXED_SHIFT, vy=p->vy >> FIXED_SHIFT;
    p->x+=vx;
    int head=p->y+4, foot=p->y+p->h+2;
    if(vx){
        int dir= vx>0 , edge=dir ? p->x + p->w -1 : p->x;
        int i;
        for(i=head;i<=foot;i+=8){
            if(tile_solid(world_tile(edge,i))){
                int tx=(edge / TILE_WIDTH +( dir ? 0:1)) * TILE_WIDTH;
                p->x= dir ? tx - p->w : tx ;
                p->vx=0 ;break;
            }
        }
    }
    p->on_ground=0;
    p->y+=vy;
    int left=p->x+2 , right =p->x + p->w -3 ;
    if(vy){
        int dir=vy>0, edge=dir?p->y+p->h-1:p->y;
        int i;
        for(i=left;i<=right;i+=8){
            if(tile_solid(world_tile(i,edge))){
                int ty=( edge /TILE_HEIGHT +(dir ? 0: 1)) * TILE_HEIGHT;
                p->y=dir ? ty-p->h :ty ; 
                p->vy=0;
                if(dir) p->on_ground=1;
                else{
                    int btx=i/TILE_WIDTH, bty=edge/TILE_HEIGHT, bt=map[bty][btx];
                    if(bt==TILE_QBLOCK){
                        map[bty][btx]=TILE_QBLOCK_USED;
                        // FIXED: Replaced non-standard 'typeof' with standard 'TileMeta'
                        tile_meta[bty][btx]=(TileMeta){0,8,4};
                        int j;
                        for(j=0;j<MAX_ENTITIES;j++) if(!ents[j].active){
                            ents[j]=(Entity){1,ENT_COIN,btx*TILE_WIDTH,bty*TILE_HEIGHT-TILE_HEIGHT,0,-6*(1<<FIXED_SHIFT),8,8,0,0,0,0,0,0,40};
                            break;
                        }
                        p->score+=200; p->coins++;
                    } else if(bt==TILE_BRICK){
                        map[bty][btx]=TILE_AIR; p->score+=50;
                    } else {
                        // FIXED: Replaced non-standard 'typeof' with standard 'TileMeta'
                        tile_meta[bty][btx]=(TileMeta){0,6,3};
                    }
                }
                break;
            }
        }
    }

    if(p->y>MAP_ROWS*TILE_HEIGHT && p->state!=PS_DEAD){
        p->state=PS_DEAD;
        p->vy=JUMP_VELOCITY*(1<<FIXED_SHIFT)/2;
        p->vx=0; p->lives--;
    }

    if(p->x<0){ p->x=0; p->vx=0; }
}

void entity_col(Entity *e){
    int vx=e->vx>>FIXED_SHIFT, vy=e->vy>>FIXED_SHIFT;

    e->on_ground=0;
    e->x+=vx;

    int head=e->y+4, foot=e->y+e->h-2;

    if(vx){
        int dir=vx>0, edge=dir?e->x+e->w-1:e->x;
        int i;
        for(i=head;i<=foot;i+=8){
            if(tile_solid(world_tile(edge,i))){
                int tx=(edge/TILE_WIDTH+(dir?0:1))*TILE_WIDTH;
                e->x=dir?tx-e->w:tx;
                e->vx=-e->vx;
                e->facing=-e->facing;
                break;
            }
        }
    }

    e->y+=vy;
    int left=e->x+1, right=e->x+e->w-2;

    if(vy){
        int dir=vy>0, edge=dir?e->y+e->h-1:e->y;
        int i;
        for(i=left;i<=right;i+=8){
            if(tile_solid(world_tile(i,edge))){
                int ty=(edge/TILE_HEIGHT+(dir?0:1))*TILE_HEIGHT;
                e->y=dir?ty-e->h:ty;
                e->vy=0;
                if(dir) e->on_ground=1;
                break;
            }
        }
    }

    if(e->y>MAP_ROWS*TILE_HEIGHT+64) e->active=0;
}

void add_score(int points,int x,int y){
    player.score+=points;
    int i;
    for(i=0;i<MAX_FLOAT;i++){
        if(floats[i].timer<=0){
            floats[i].score=points;
            floats[i].x=x; floats[i].y=y;
            floats[i].timer=40;
            break;
        }
    }
}

int jump_held;

void update_player(){
    Player *p=&player;
    if(p->state==PS_WIN){
        if((p->x+=2)>(MAP_COLS-2)*TILE_WIDTH) gstate=GS_WIN;
        return;
    }

    if(p->state==PS_DEAD){
        p->vy+=GRAVITY*(1<<FIXED_SHIFT);
        if((p->y+=p->vy>>FIXED_SHIFT)>SCREEN_HEIGHT+32)
            gstate=(p->lives<=0)?GS_GAMEOVER:GS_DEAD;
        return;
    }

    int acc=1<<(FIXED_SHIFT-1), fr=1<<(FIXED_SHIFT-2), max=SPEED*(1<<FIXED_SHIFT);

    if(sw_held(1))  p->vx = (p->vx-acc<-max?-max:p->vx-acc), p->facing=-1;
    else if(sw_held(0)) p->vx = (p->vx+acc>max?max:p->vx+acc), p->facing=1;
    else{
        if(p->vx>0) p->vx-=fr;
        if(p->vx<0) p->vx+=fr;
        if(p->vx>-fr && p->vx<fr) p->vx=0;
    }

    if(btn_pressed(0)&&p->on_ground) p->vy=JUMP_VELOCITY*(1<<FIXED_SHIFT), jump_held=1;
    if(!btn_held(0)) jump_held=0;

    if((p->vy+=GRAVITY*(1<<FIXED_SHIFT))>FALL_SPEED*(1<<FIXED_SHIFT))
        p->vy=FALL_SPEED*(1<<FIXED_SHIFT);

    player_col();

    p->state = !p->on_ground ? (p->vy<0?PS_JUMPING:PS_FALLING)
             : p->vx ? PS_WALKING : PS_STANDING;

    if(++p->frame_timer>=8 && p->state==PS_WALKING)
        p->frame=(p->frame+1)%2, p->frame_timer=0;

    if(p->x<camera) p->x=camera;

    int tx=(p->x+p->w/2)/TILE_WIDTH, ty=(p->y+p->h/2)/TILE_HEIGHT;
    if(tx>=0&&tx<MAP_COLS&&ty>=0&&ty<MAP_ROWS && map[ty][tx]==TILE_GOAL)
        add_score(5000,p->x-camera,p->y), p->state=PS_WIN;
}

void update_enemies(){
    int i;
    for(i=0;i<MAX_ENTITIES;i++){
        Entity *e=&ents[i];
        if(!e->active) continue;

        if(e->type==ENT_COIN){
            e->vy+=GRAVITY*(1<<FIXED_SHIFT);
            e->y+=e->vy>>FIXED_SHIFT;
            if(--e->lifetime<=0) e->active=0;
            continue;
        }

        if(e->type==ENT_ENEMY){
            if(e->dead){ 
                if(--e->squished<=0) e->active=0; 
                continue; 
            }
            if(e->x<camera-32||e->x>camera+SCREEN_WIDTH+32) continue;

            if((e->vy+=GRAVITY*(1<<FIXED_SHIFT))>FALL_SPEED*(1<<FIXED_SHIFT))
                e->vy=FALL_SPEED*(1<<FIXED_SHIFT);

            entity_col(e);

            if(++e->frame_timer>=12) e->frame^=1, e->frame_timer=0;

            Player *p=&player;
            if(p->x<e->x+e->w && p->x+p->w>e->x && p->y<e->y+e->h && p->y+p->h>e->y){
                int stomp = p->vy>0 && p->y+p->h-8<=e->y+4;
                if(stomp){
                    e->dead=1; e->squished=20;
                    add_score(100,e->x-camera,e->y);
                    p->vy=JUMP_VELOCITY*(1<<FIXED_SHIFT)/2;
                }
                else if(p->state!=PS_DEAD){
                    p->state=PS_DEAD;
                    p->vy=JUMP_VELOCITY*(1<<FIXED_SHIFT)/2;
                    p->vx=0; p->lives--;
                }
            }
        }
    }
}

void update_camera(void){
    int target=player.x - SCREEN_WIDTH * 2/5;
    camera+= (target - camera) /4;
    if(camera < 0) camera=0;
    if(camera> MAP_COLS * TILE_WIDTH- SCREEN_WIDTH) camera=MAP_COLS * TILE_WIDTH- SCREEN_WIDTH;
}

void update_tiles(){
    int y,x;
    for(y=0;y<MAP_ROWS;y++)
        for(x=0;x<MAP_COLS;x++){
            TileMeta *m=&tile_meta[y][x];
            if(m->anim_timer){
                m->anim_off=(--m->anim_timer)?(m->anim_timer>4?4:-4):0;
            }
        }
}

void update_float_texts(){
    int i;
    for(i=0;i<MAX_FLOAT;i++)
        if(floats[i].timer) floats[i].y--, floats[i].timer--;
}

void draw_background(){
    draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,SKY);
    int cx=camera/2;
    int c1=((200-cx)%SCREEN_WIDTH+SCREEN_WIDTH)%SCREEN_WIDTH;
    int c2=((380-cx)%SCREEN_WIDTH+SCREEN_WIDTH)%SCREEN_WIDTH;
    draw_rect(c1,20,32,12,CLOUD);   draw_rect(c1+4,14,24,12,CLOUD);
    draw_rect(c2,40,28,10,CLOUD);   draw_rect(c2+4,34,20,10,CLOUD);
}

void draw_tilemap(){
    int st=camera/TILE_WIDTH, en=st+SCREEN_WIDTH/TILE_WIDTH+2;
    if(st<0) st=0; if(en>MAP_COLS) en=MAP_COLS;

    int y,x;
    for(y=0;y<MAP_ROWS;y++)
        for(x=st;x<en;x++)
            if(map[y][x]!=TILE_AIR)
                draw_tile(x*TILE_WIDTH-camera,y*TILE_HEIGHT,map[y][x]);
}

void draw_player_sprite(){
    Player *p=&player;
    int sx=p->x-camera, sy=p->y, flip=p->facing<0;

    draw_mario(sx,sy,flip);

    if(p->state==PS_DEAD){
        draw_pixel(sx+4,sy+3,BLACK);  draw_pixel(sx+5,sy+5,BLACK);
        draw_pixel(sx+10,sy+3,BLACK); draw_pixel(sx+11,sy+5,BLACK);
    }
}

void draw_entities(){
    int i;
    for(i=0;i<MAX_ENTITIES;i++){
        Entity *e=&ents[i];
        if(!e->active) continue;

        int sx=e->x-camera, sy=e->y;
        if(sx<-32||sx>SCREEN_WIDTH+32) continue;

        if(e->type==ENT_COIN) draw_rect(sx+4,sy,8,8,COIN);
        else if(e->type==ENT_ENEMY) e->dead ? draw_enemy_flat(sx,sy): draw_enemy(sx,sy,e->frame);
    }
}

void draw_hud(){
    draw_rect(0,0,SCREEN_WIDTH,12,COL(20,20,60));
    draw_string(2,2,"MARIO",COL(200,200,200));
    draw_number(38,2,player.score,WHITE);

    draw_rect(120,3,6,6,COIN);
    draw_string(128,2,"X",WHITE);
    draw_number(136,2,player.coins,WHITE);

    draw_string(170,2,"WORLD 1-1",WHITE);
    draw_string(262,2,"TIME",WHITE);
    draw_number(292,2,timer/30,(timer<150)?RED:WHITE);

    draw_string(2,SCREEN_HEIGHT-10,"LIVES:",WHITE);
    draw_number(40,SCREEN_HEIGHT-10,player.lives,YELLOW);
    int i;
    for(i=0;i<MAX_FLOAT;i++)
        if(floats[i].timer) draw_number(floats[i].x,floats[i].y,floats[i].score,YELLOW);
}

void draw_title(){
    clear_screen(COL(20,20,80));
    draw_string(80,50,"SUPER MARIO",COL(255,50,50));
    draw_rect(40,80,SCREEN_WIDTH-80,2,WHITE);
    draw_string(55,95,"PRESS KEY[1] TO PLAY",WHITE); // Adjusted text for DE1-SoC
    draw_string(55,110,"KEY[0] = JUMP",COL(200,200,200));
    draw_string(55,122,"SW[0] / SW[1] = MOVE",COL(200,200,200));
    draw_string(55,150,"STOMP GOOMBAS TO SCORE",COL(180,180,255));
    draw_string(55,162,"HIT ? BLOCKS FOR COINS",COL(180,180,255));
    draw_string(55,174,"REACH THE FLAGPOLE!",COL(100,255,100));
}

void draw_win_screen(){
    clear_screen(COL(20,80,20));
    draw_string(80,80,"YOU WIN!",YELLOW);
    draw_string(60,100,"FINAL SCORE:",WHITE);
    draw_number(60,112,player.score,YELLOW);
    draw_string(50,140,"PRESS KEY[1] TO PLAY AGAIN",COL(180,255,180));
}

void draw_gameover_screen(){
    clear_screen(COL(60,10,10));
    draw_string(105,80,"GAME OVER",RED);
    draw_string(60,110,"FINAL SCORE:",WHITE);
    draw_number(60,122,player.score,YELLOW);
    draw_string(50,150,"PRESS KEY[1] TO TRY AGAIN",COL(255,180,180));
}

void draw_dead_screen(){
    draw_string(95,110,"YOU DIED!",RED);
    draw_string(60,126,"PRESS KEY[1] TO CONTINUE",WHITE);
    draw_number(95,142,player.lives,YELLOW);
    draw_string(109,142,"LIVES LEFT",WHITE);
}

void reset_map(){
    int i,j;
    for(i=0;i<MAP_ROWS;i++)
        for(j=0;j<MAP_COLS;j++){
            map[i][j]=MAP[i][j];
            tile_meta[i][j]=(TileMeta){0,0,0};
        }
}

void clear_entities(){
    int i;
    for(i=0;i<MAX_ENTITIES;i++) ents[i].active=0;
    for(i=0;i<MAX_FLOAT;i++) floats[i].timer=0;
}

void init_level(){
    reset_map(); clear_entities();
    int i,j;
    for(i=0;ENEMY_SPAWNS[i][2]!=ENT_NONE;i++){
        Entity *e=0;
        for(j=0;j<MAX_ENTITIES;j++)
            if(!ents[j].active){ e=&ents[j]; break; }
        if(!e) break;

        *e=(Entity){1,ENT_ENEMY, ENEMY_SPAWNS[i][0], ENEMY_SPAWNS[i][1], -1*(1<<FIXED_SHIFT),0,16,16,-1,0,0,0,0};
    }

    player=(Player){2*TILE_WIDTH,11*TILE_HEIGHT-16,0,0,14,16,1,0,0,0,PS_STANDING,0,0};

    camera=0; timer=30*100;
    jump_held=0; gstate=GS_PLAYING;
}

void init_game(){
    int i;
    for(i=0;i<256;i++) keys[i]=prev_keys[i]=0;
    break_code=0; camera=0; frame=0;
    gstate=GS_TITLE;
}

void game_tick(){
    read_inputs(); frame++;
    switch(gstate){
        case GS_TITLE:
            draw_title();
            if(btn_pressed(1))
                player.lives=3, player.score=0, init_level();
            break;

        case GS_PLAYING:
            if(timer>0) timer--;
            else if(player.state!=PS_DEAD){
                player.state=PS_DEAD;
                player.vy=JUMP_VELOCITY*(1<<FIXED_SHIFT)/2;
                player.vx=0; player.lives--;
            }

            update_player(); update_enemies(); update_camera();
            update_tiles(); update_float_texts();

            draw_background(); draw_tilemap();
            draw_entities(); draw_player_sprite(); draw_hud();
            break;

        case GS_DEAD:
            draw_background(); draw_tilemap(); draw_hud();
            draw_dead_screen();
            if(btn_pressed(1))
                player.lives>0 ? init_level() : (gstate=GS_GAMEOVER);
            break;

        case GS_WIN:
            draw_win_screen();
            if(btn_pressed(1))
                player.lives=3, player.score=0, init_level();
            break;

        case GS_GAMEOVER:
            draw_gameover_screen();
            if(btn_pressed(1))
                player.lives=3,   player.score=0, init_level();
            break;
    }
}

int main(){
    draw_buff = FRAMEBUFFER;

    clear_screen(SKY);
    init_game();

    while (1) {
        game_tick();
    }
    // *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_1;
    // wait_vsync();

    // *(PIXEL_BUF_CTRL + 1) = PIXEL_BUFFER_2;

    // // Step 4: Safely assign our standard C pointers (No volatile needed!)
    // show_buff = (unsigned short*)PIXEL_BUFFER_1;
    // draw_buff = (unsigned short*)PIXEL_BUFFER_2;

    // clear_screen(SKY); swap_buffers();
    // clear_screen(SKY);

    // init_game();

    // while(1){ 
    //     game_tick(); 
    //     swap_buffers(); 
    // }
}