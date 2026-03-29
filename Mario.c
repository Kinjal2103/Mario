/*
 * ============================================================
 *  MARIO-STYLE GAME FOR DE1-SoC (ARM Cortex-A9)
 *  VGA Pixel Buffer 320x240, 16-bit colour
 *
 *  PERFORMANCE STRATEGY (no full-screen clear per frame):
 *    - Draw static background ONCE at level start into BOTH buffers
 *    - Each frame: erase old sprite bbox with bg colour, draw new
 *    - No coin bobbing animation
 *    - Only redraw background when camera scrolls
 *    - 32-bit word writes for all rects
 *
 *  Controls:
 *    KEY0 = Left   KEY1 = Right   KEY2 = Jump   KEY3 = Restart
 * ============================================================
 */

#include <stdbool.h>

/* ---- Memory-mapped I/O ---- */
#define PIX_BUF_BASE  0xFF203020u
#define KEY_BASE      0xFF200050u
#define SW_BASE       0xFF200040u

/* ---- Screen ---- */
#define SW   320
#define SH   240
#define STRIDE 512   /* VGA buffer row stride in u16 words */

/* ---- Colours RGB565 ---- */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            i32;

#define RGB(r,g,b) (u16)(((r)>>3)<<11|((g)>>2)<<5|(b)>>3)
#define SKY    RGB(100,180,255)
#define DIRT   RGB(139, 90, 43)
#define GRASS  RGB( 34,139, 34)
#define PLAT_C RGB(160,100, 40)
#define RED    RGB(220,  0,  0)
#define SKIN   RGB(255,200,150)
#define BROWN  RGB(120, 60, 10)
#define GOLD   RGB(255,210,  0)
#define WHITE  RGB(255,255,255)
#define BLACK  RGB(  0,  0,  0)
#define DKRED  RGB(160,  0,  0)
#define DKGRN  RGB(  0,160,  0)

/* ---- World constants ---- */
#define GROUND_Y  205
#define GRAV        1
#define JUMP_V    -11
#define PSPEED      2
#define MAX_VY     10
#define N_PLAT      8
#define N_ENEMY     5
#define N_COIN     10
#define PW 14
#define PH 18
#define EW 14
#define EH 12
#define CR  5
#define PLATH 8

/* ============================================================
 *  VGA GLOBALS
 * ============================================================ */
static volatile u16 *vga;
static u32 buf_back;

/* ============================================================
 *  DRAWING PRIMITIVES
 * ============================================================ */
static void rect(i32 x, i32 y, i32 w, i32 h, u16 col) {
    if (x < 0)    { w += x; x = 0; }
    if (y < 0)    { h += y; y = 0; }
    if (x+w > SW) w = SW - x;
    if (y+h > SH) h = SH - y;
    if (w<=0||h<=0) return;
    u32 word = ((u32)col<<16)|col;
    for (i32 r = y; r < y+h; r++) {
        volatile u16 *p = vga + r*STRIDE + x;
        i32 n = w;
        if ((u32)p & 2) { *p++ = col; n--; }
        volatile u32 *q = (volatile u32*)p;
        for (i32 i = n>>1; i > 0; i--) *q++ = word;
        if (n&1) *(volatile u16*)q = col;
    }
}

static void px(i32 x, i32 y, u16 col) {
    if ((u32)x<(u32)SW && (u32)y<(u32)SH) vga[y*STRIDE+x]=col;
}

static void circle(i32 cx, i32 cy, i32 r, u16 col) {
    i32 r2=r*r;
    for (i32 dy=-r; dy<=r; dy++) {
        i32 dy2=dy*dy, dx=0;
        while((dx+1)*(dx+1)+dy2<=r2) dx++;
        rect(cx-dx, cy+dy, dx*2+1, 1, col);
    }
}

static void cls(u16 col) {
    u32 w=((u32)col<<16)|col;
    volatile u32 *p=(volatile u32*)vga;
    for (i32 i=0; i<STRIDE*SH/2; i++) p[i]=w;
}

static void swap_buf(void) {
    volatile u32 *c=(volatile u32*)PIX_BUF_BASE;
    *c=1; while(*(c+3)&1);
    buf_back=*(c+1);
    vga=(volatile u16*)buf_back;
}

/* ============================================================
 *  MINIMAL FONT  (5-wide columns, bit0=top row)
 * ============================================================ */
static const u8 F[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*  0 SPC */
    {0x3E,0x51,0x49,0x45,0x3E}, /*  1  0  */
    {0x00,0x42,0x7F,0x40,0x00}, /*  2  1  */
    {0x42,0x61,0x51,0x49,0x46}, /*  3  2  */
    {0x21,0x41,0x45,0x4B,0x31}, /*  4  3  */
    {0x18,0x14,0x12,0x7F,0x10}, /*  5  4  */
    {0x27,0x45,0x45,0x45,0x39}, /*  6  5  */
    {0x3C,0x4A,0x49,0x49,0x30}, /*  7  6  */
    {0x01,0x71,0x09,0x05,0x03}, /*  8  7  */
    {0x36,0x49,0x49,0x49,0x36}, /*  9  8  */
    {0x06,0x49,0x49,0x29,0x1E}, /* 10  9  */
    {0x00,0x36,0x36,0x00,0x00}, /* 11  :  */
    {0x7F,0x09,0x09,0x09,0x06}, /* 12  P  */
    {0x7F,0x09,0x19,0x29,0x46}, /* 13  R  */
    {0x46,0x49,0x49,0x49,0x31}, /* 14  S  */
    {0x01,0x01,0x7F,0x01,0x01}, /* 15  T  */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 16  U  */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 17  O  */
    {0x7C,0x12,0x11,0x12,0x7C}, /* 18  A  */
    {0x7F,0x49,0x49,0x49,0x36}, /* 19  B  */
    {0x3E,0x41,0x41,0x41,0x22}, /* 20  C  */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 21  D  */
    {0x7F,0x49,0x49,0x49,0x41}, /* 22  E  */
    {0x7F,0x09,0x09,0x09,0x01}, /* 23  F  */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 24  G  */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 25  H  */
    {0x00,0x41,0x7F,0x41,0x00}, /* 26  I  */
    {0x20,0x40,0x41,0x3F,0x01}, /* 27  J  */
    {0x7F,0x08,0x14,0x22,0x41}, /* 28  K  */
    {0x7F,0x40,0x40,0x40,0x40}, /* 29  L  */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 30  M  */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 31  N  */
    {0x7F,0x09,0x09,0x06,0x00}, /* 32  Q? use for W approx */
    {0x41,0x22,0x14,0x08,0x07}, /* 33  V  */
    {0x63,0x14,0x08,0x14,0x63}, /* 34  X  */
    {0x07,0x08,0x70,0x08,0x07}, /* 35  Y  */
    {0x61,0x51,0x49,0x45,0x43}, /* 36  Z  */
    {0x7F,0x41,0x41,0x41,0x7F}, /* 37  W? box */
};

static int cidx(char c) {
    if (c==' ') return 0;
    if (c>='0'&&c<='9') return 1+(c-'0');
    if (c==':') return 11;
    switch(c){
    case 'P':return 12;case 'R':return 13;case 'S':return 14;
    case 'T':return 15;case 'U':return 16;case 'O':return 17;
    case 'A':return 18;case 'B':return 19;case 'C':return 20;
    case 'D':return 21;case 'E':return 22;case 'F':return 23;
    case 'G':return 24;case 'H':return 25;case 'I':return 26;
    case 'J':return 27;case 'K':return 28;case 'L':return 29;
    case 'M':return 30;case 'N':return 31;case 'V':return 33;
    case 'X':return 34;case 'Y':return 35;case 'Z':return 36;
    case 'W':return 37;
    }
    return 0;
}

static void dchar(i32 x, i32 y, char c, u16 col) {
    const u8 *g = F[cidx(c)];
    for (i32 ci=0; ci<5; ci++) {
        u8 b=g[ci];
        for (i32 ri=0; ri<7; ri++)
            if (b&(1<<ri)) px(x+ci, y+ri, col);
    }
}
static void dstr(i32 x, i32 y, const char *s, u16 col) {
    while(*s){ dchar(x,y,*s++,col); x+=6; }
}
static void dint(i32 x, i32 y, int v, u16 col) {
    char buf[8]; int i=7; buf[7]=0;
    if(v==0){dchar(x,y,'0',col);return;}
    while(v>0){buf[--i]='0'+(v%10);v/=10;}
    dstr(x,y,buf+i,col);
}

/* ============================================================
 *  GAME STRUCTS
 * ============================================================ */
typedef struct { i32 x,y,w,vx; } Plat;
typedef struct { i32 x,y,vx,pl,pr; bool alive; } Enemy;
typedef struct { i32 x,y; bool got; } Coin;
typedef struct {
    i32 x,y,vx,vy;
    bool on_ground,alive,right;
    int inv;
} Player;
typedef enum { PLAY,DEAD,WIN } State;

/* ============================================================
 *  GLOBALS
 * ============================================================ */
static Player  pl;
static Plat    plats[N_PLAT];
static Enemy   ens[N_ENEMY];
static Coin    coins[N_COIN];
static int     score,coins_left,cam_x,fc;
static State   state;

/* Previous screen-space positions for erase */
static i32 pl_px,pl_py;
static i32 en_px[N_ENEMY],en_py[N_ENEMY];
static i32 plat_px[N_PLAT];

/* ============================================================
 *  BACKGROUND COLOUR AT WORLD COORDS  (for erase)
 * ============================================================ */
static u16 bgcol(i32 wx, i32 wy) {
    if (wy >= GROUND_Y+4) return DIRT;
    if (wy >= GROUND_Y)   return GRASS;
    for (int i=0; i<N_PLAT; i++) {
        if (wx>=plats[i].x && wx<plats[i].x+plats[i].w &&
            wy>=plats[i].y && wy<plats[i].y+PLATH)
            return (wy<plats[i].y+3)?GRASS:PLAT_C;
    }
    return SKY;
}

/* Erase sprite area by repainting with background */
static void erase(i32 sx, i32 sy, i32 w, i32 h) {
    if (sx<0){w+=sx;sx=0;} if(sy<0){h+=sy;sy=0;}
    if (sx+w>SW) w=SW-sx;  if(sy+h>SH) h=SH-sy;
    if (w<=0||h<=0) return;
    for (i32 row=sy; row<sy+h; row++)
        for (i32 col=sx; col<sx+w; col++)
            vga[row*STRIDE+col]=bgcol(col+cam_x, row);
}

/* ============================================================
 *  STATIC BACKGROUND (called once per level + on camera scroll)
 * ============================================================ */
static void draw_bg(void) {
    cls(SKY);
    rect(0, GROUND_Y,   SW, 4,           GRASS);
    rect(0, GROUND_Y+4, SW, SH-GROUND_Y, DIRT);
    for (int i=0; i<N_PLAT; i++) {
        i32 sx=plats[i].x-cam_x;
        if (sx+plats[i].w<0||sx>SW) continue;
        rect(sx, plats[i].y,   plats[i].w, 3,      GRASS);
        rect(sx, plats[i].y+3, plats[i].w, PLATH-3, PLAT_C);
    }
    for (int i=0; i<N_COIN; i++) {
        if (coins[i].got) continue;
        i32 sx=coins[i].x-cam_x;
        if (sx>-CR&&sx<SW+CR) circle(sx,coins[i].y,CR,GOLD);
    }
}

/* ============================================================
 *  SPRITE DRAWERS
 * ============================================================ */
static void draw_player(i32 sx, i32 sy, bool right, bool blink) {
    if (blink) return;
    rect(sx+2, sy,    10, 3, RED);
    rect(sx+1, sy+3,  12, 3, RED);
    rect(sx+2, sy+6,  10, 5, SKIN);
    px(right?sx+9:sx+4, sy+7, BLACK);
    rect(sx+3, sy+9,  8, 2, BLACK);
    rect(sx+1, sy+11, 12,4, RED);
    rect(sx+1, sy+15,  5,3, BROWN);
    rect(sx+8, sy+15,  5,3, BROWN);
}

static void draw_enemy(i32 sx, i32 sy) {
    circle(sx+EW/2, sy+5, 6, BROWN);
    rect(sx,   sy+8, 5,4, BROWN);
    rect(sx+9, sy+8, 5,4, BROWN);
    px(sx+4,sy+3,WHITE); px(sx+9,sy+3,WHITE);
    px(sx+4,sy+3,BLACK); px(sx+9,sy+3,BLACK);
    rect(sx+3,sy+1,3,1,BLACK);
    rect(sx+8,sy+1,3,1,BLACK);
}

/* ============================================================
 *  HUD — only redrawn when score/coins change
 * ============================================================ */
static int hud_score=-1, hud_coins=-1;
static void draw_hud(void) {
    if (score==hud_score && coins_left==hud_coins) return;
    rect(0,0,SW,12,SKY);
    dstr(2,2,"SC:",WHITE); dint(22,2,score,WHITE);
    dstr(90,2,"CO:",WHITE); dint(112,2,coins_left,GOLD);
    hud_score=score; hud_coins=coins_left;
}

/* ============================================================
 *  LEVEL INIT
 * ============================================================ */
static void init_level(void) {
    score=0; cam_x=0; fc=0; state=PLAY;
    hud_score=-1; hud_coins=-1;

    pl=(Player){40,GROUND_Y-PH,0,0,true,true,true,0};
    pl_px=pl.x; pl_py=pl.y;

    plats[0]=(Plat){ 90,175,60, 0};
    plats[1]=(Plat){190,155,50, 1};
    plats[2]=(Plat){275,135,55, 0};
    plats[3]=(Plat){365,160,50,-1};
    plats[4]=(Plat){455,145,60, 0};
    plats[5]=(Plat){545,170,50, 1};
    plats[6]=(Plat){635,135,65, 0};
    plats[7]=(Plat){725,155,55, 0};
    for(int i=0;i<N_PLAT;i++) plat_px[i]=plats[i].x;

    ens[0]=(Enemy){100,GROUND_Y-EH, 1, 80,170,true};
    ens[1]=(Enemy){280,135-EH,     -1,265,325,true};
    ens[2]=(Enemy){370,160-EH,      1,360,415,true};
    ens[3]=(Enemy){465,GROUND_Y-EH,-1,415,515,true};
    ens[4]=(Enemy){550,170-EH,      1,540,595,true};
    for(int i=0;i<N_ENEMY;i++){en_px[i]=ens[i].x;en_py[i]=ens[i].y;}

    int cx[]={100,210,290,380,470,560,640,750,130,320};
    int cy[]={158,138,118,148,128,158,118,138,198,138};
    coins_left=N_COIN;
    for(int i=0;i<N_COIN;i++) coins[i]=(Coin){cx[i],cy[i],false};
}

/* ============================================================
 *  COLLISION
 * ============================================================ */
static bool ovlp(i32 ax,i32 ay,i32 aw,i32 ah,
                 i32 bx,i32 by,i32 bw,i32 bh){
    return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;
}

/* ============================================================
 *  INPUT
 * ============================================================ */
static u32 prev_k=0;
static u32 rkeys(void){return ~(*(volatile u32*)KEY_BASE)&0xF;}
static u32 kedge(void){u32 k=rkeys();u32 e=k&~prev_k;prev_k=k;return e;}

/* ============================================================
 *  UPDATE
 * ============================================================ */
static void update(void) {
    if(state!=PLAY) return;
    fc++;
    u32 k=rkeys(), sw=*(volatile u32*)SW_BASE;
    bool left =(k&1)||(sw&1);
    bool right=(k&2)||(sw&2);
    bool jump =(k&4)||(sw&4);

    pl.vx = left?-PSPEED:right?PSPEED:0;
    if(right) pl.right=true;
    if(left)  pl.right=false;
    if(jump&&pl.on_ground){pl.vy=JUMP_V;pl.on_ground=false;}

    pl.vy+=GRAV; if(pl.vy>MAX_VY) pl.vy=MAX_VY;
    pl.x+=pl.vx; pl.y+=pl.vy;
    if(pl.x<0) pl.x=0;

    /* Camera */
    int tc=pl.x-80; if(tc<0)tc=0; if(tc>800-SW)tc=800-SW;
    cam_x=tc;

    /* Ground */
    pl.on_ground=false;
    if(pl.y+PH>=GROUND_Y){pl.y=GROUND_Y-PH;pl.vy=0;pl.on_ground=true;}

    /* Platforms */
    for(int i=0;i<N_PLAT;i++){
        Plat *p=&plats[i];
        if(p->vx){p->x+=p->vx;if(p->x<60||p->x+p->w>790)p->vx=-p->vx;}
        if(pl.vy>=0&&pl.x+PW>p->x&&pl.x<p->x+p->w&&
           pl.y+PH>=p->y&&pl.y+PH<=p->y+PLATH+pl.vy+2){
            pl.y=p->y-PH;pl.vy=0;pl.on_ground=true;pl.x+=p->vx;
        }
    }

    if(pl.y>SH+20){state=DEAD;return;}
    if(pl.inv>0) pl.inv--;

    for(int i=0;i<N_ENEMY;i++){
        Enemy *e=&ens[i]; if(!e->alive) continue;
        e->x+=e->vx;
        if(e->x<=e->pl||e->x+EW>=e->pr) e->vx=-e->vx;
        if(ovlp(pl.x,pl.y,PW,PH,e->x,e->y,EW,EH)){
            if(pl.vy>0&&pl.y+PH<e->y+EH/2+4){
                e->alive=false;pl.vy=-8;score+=100;
            } else if(pl.inv==0){
                state=DEAD;return;
            }
        }
    }

    for(int i=0;i<N_COIN;i++){
        Coin *c=&coins[i]; if(c->got) continue;
        if(ovlp(pl.x,pl.y,PW,PH,c->x-CR,c->y-CR,CR*2,CR*2)){
            c->got=true;score+=50;coins_left--;
        }
    }
    if(coins_left==0) state=WIN;
}

/* ============================================================
 *  RENDER — erase/redraw only what moved
 * ============================================================ */
static int prev_cam=-1;
static bool need_full_bg=true;

static void render(void) {
    /* Full background redraw when camera scrolls */
    if(cam_x!=prev_cam||need_full_bg){
        draw_bg();
        /* Re-stamp all visible enemies after bg redraw */
        for(int i=0;i<N_ENEMY;i++){
            if(!ens[i].alive) continue;
            i32 sx=ens[i].x-cam_x;
            if(sx>-EW&&sx<SW) draw_enemy(sx,ens[i].y);
        }
        /* Re-stamp player */
        {
            i32 sx=pl.x-cam_x;
            if(sx>-PW&&sx<SW) draw_player(sx,pl.y,pl.right,false);
        }
        prev_cam=cam_x;
        need_full_bg=false;
        /* Update stored screen positions */
        pl_px=pl.x; pl_py=pl.y;
        for(int i=0;i<N_ENEMY;i++){en_px[i]=ens[i].x;en_py[i]=ens[i].y;}
        for(int i=0;i<N_PLAT;i++) plat_px[i]=plats[i].x;
    } else {
        /* --- Moving platforms --- */
        for(int i=0;i<N_PLAT;i++){
            if(!plats[i].vx) continue;
            if(plats[i].x==plat_px[i]) continue;
            i32 osx=plat_px[i]-cam_x;
            i32 nsx=plats[i].x-cam_x;
            erase(osx,plats[i].y,plats[i].w,PLATH);
            rect(nsx,plats[i].y,  plats[i].w,3,     GRASS);
            rect(nsx,plats[i].y+3,plats[i].w,PLATH-3,PLAT_C);
            plat_px[i]=plats[i].x;
        }

        /* --- Enemies --- */
        for(int i=0;i<N_ENEMY;i++){
            i32 osx=en_px[i]-cam_x;
            if(!ens[i].alive){
                erase(osx,en_py[i],EW,EH);
                en_px[i]=ens[i].x;
                continue;
            }
            i32 nsx=ens[i].x-cam_x;
            if(osx!=nsx||en_py[i]!=ens[i].y)
                erase(osx,en_py[i],EW,EH);
            if(nsx>-EW&&nsx<SW) draw_enemy(nsx,ens[i].y);
            en_px[i]=ens[i].x; en_py[i]=ens[i].y;
        }

        /* --- Collected coins --- */
        for(int i=0;i<N_COIN;i++){
            if(!coins[i].got||coins[i].x==-9999) continue;
            i32 sx=coins[i].x-cam_x;
            erase(sx-CR,coins[i].y-CR,CR*2+1,CR*2+1);
            coins[i].x=-9999;
        }

        /* --- Player --- */
        {
            i32 osx=pl_px-cam_x;
            i32 nsx=pl.x-cam_x;
            erase(osx,pl_py,PW,PH);
            bool blink=(pl.inv>0)&&(fc%8<4);
            if(nsx>-PW&&nsx<SW) draw_player(nsx,pl.y,pl.right,blink);
            pl_px=pl.x; pl_py=pl.y;
        }
    }

    /* HUD */
    draw_hud();

    /* Overlays */
    if(state==DEAD){
        rect(80,95,160,46,BLACK); rect(82,97,156,42,DKRED);
        dstr(92,106,"GAME OVER",WHITE);
        dstr(86,120,"KEY3 RESTART",WHITE);
    }
    if(state==WIN){
        rect(80,95,160,46,BLACK); rect(82,97,156,42,DKGRN);
        dstr(100,106,"YOU WIN",WHITE);
        dstr(86,120,"KEY3 RESTART",WHITE);
    }
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void) {
    volatile u32 *c=(volatile u32*)PIX_BUF_BASE;
    *c=1; while(*(c+3)&1);
    buf_back=*(c+1);
    vga=(volatile u16*)buf_back;

    init_level();

    /* Paint background into both buffers before starting */
    draw_bg(); draw_hud();
    swap_buf();
    draw_bg(); draw_hud();
    /* Stamp initial sprites */
    draw_player(pl.x-cam_x, pl.y, pl.right, false);
    for(int i=0;i<N_ENEMY;i++){
        i32 sx=ens[i].x-cam_x;
        if(sx>-EW&&sx<SW) draw_enemy(sx,ens[i].y);
    }

    while(1){
        u32 edge=kedge();
        if(edge&0x8){
            /* Full restart */
            init_level();
            need_full_bg=true; prev_cam=-1;
            draw_bg(); draw_hud();
            draw_player(pl.x-cam_x,pl.y,pl.right,false);
            for(int i=0;i<N_ENEMY;i++){
                i32 sx=ens[i].x-cam_x;
                if(sx>-EW&&sx<SW) draw_enemy(sx,ens[i].y);
            }
            swap_buf();
            draw_bg(); draw_hud();
            draw_player(pl.x-cam_x,pl.y,pl.right,false);
            for(int i=0;i<N_ENEMY;i++){
                i32 sx=ens[i].x-cam_x;
                if(sx>-EW&&sx<SW) draw_enemy(sx,ens[i].y);
            }
        }

        update();
        render();
        swap_buf();
    }
    return 0;
}
