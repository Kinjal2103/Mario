#include <stdint.h>

#define PIX_CTRL  ((volatile uint32_t *)0xFF203020)
#define PIX_BUF_A  0x08000000
#define PIX_BUF_B  0x09000000
#define PS2_ADDR  ((volatile uint32_t *)0xFF200100)

#define SW  320
#define SH  240
#define FBW 512

#define GRAV      1
#define JVEL     -12
#define SPD       2
#define MAX_FALL  10
#define FX        4

#define TW  16
#define TH  16
#define MCOLS 60
#define MROWS 15

#define T_AIR  0
#define T_GROUND  1
#define T_BRICK  2
#define T_COINBRICK   3
#define T_COINBRICK_USED  4
#define T_PIPE 5
#define T_GOAL 6

#define RGB(r,g,b) (((r)>>3)<<11|(((g)>>2)<<5)|((b)>>3))
#define SKY   RGB(92,148,252)
#define BLACK RGB(0,0,0)
#define WHITE RGB(255,255,255)
#define RED   RGB(220,50,50)
#define BROWN RGB(130,70,30)
#define DKBRN RGB(90,45,15)
#define GREEN RGB(50,200,50)
#define YELLW RGB(255,220,0)
#define ORNGE RGB(255,140,0)
#define TAN   RGB(240,200,140)
#define GRAY  RGB(150,150,150)
#define LTGRY RGB(210,210,210)
#define PIPEG RGB(60,180,60)
#define PIPED RGB(20,120,20)
#define COIN  RGB(255,200,0)
#define CLOUD RGB(245,245,255)
#define POLE  RGB(180,180,180)

#define K_LEFT  0x6B
#define K_RIGHT 0x74
#define K_JUMP  0x1D
#define K_ENT   0x5A
#define K_P     0x4D

typedef enum { E_NONE=0, E_GOOMBA, E_COIN } EntType;
typedef enum { PL_STAND, PL_WALK, PL_JUMP, PL_FALL, PL_DEAD, PL_WIN } PlayerState;
typedef enum { GS_TITLE, GS_PLAY, GS_DIED, GS_WIN, GS_OVER } GameState;

typedef struct {
    int x,y,vx,vy,w,h;
    int fr,ftimer,dir,ground,inv;
    PlayerState st;
    int lives,score,coins;
} Player;

typedef struct {
    int on; EntType type;
    int x,y,vx,vy,w,h;
    int dir,fr,ftimer,ground;
    int dead,sq,life;
} Ent;

typedef struct { int hit,at,ao; } TMeta;
typedef struct { int pts,x,y,t; } FText;

#define MAXE 20
#define MAXF  6

static const unsigned char MAP[MROWS][MCOLS] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,3,0,2,0,3,0,0,0,0,0,2,2,2,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0},
    {0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,5,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static unsigned char map[MROWS][MCOLS];
static TMeta meta[MROWS][MCOLS];
static volatile unsigned short *dbuf, *sbuf;
static Player pl;
static Ent ents[MAXE];
static FText ftexts[MAXF];
static int camx;
static GameState gst;
static int gtimer, gframe;
static unsigned char keys[256], keys_p[256];
static int bcode;

static const int SPAWNS[][3] = {
    {10*16,12*16-16,E_GOOMBA},{14*16,12*16-16,E_GOOMBA},
    {20*16,12*16-16,E_GOOMBA},{25*16,12*16-16,E_GOOMBA},
    {32*16,12*16-16,E_GOOMBA},{38*16,12*16-16,E_GOOMBA},
    {45*16,12*16-16,E_GOOMBA},{-1,-1,E_NONE}
};

static void draw_pixel(int x,int y,unsigned short c){
    if(x<0||x>=SW||y<0||y>=SH)return;
    *(dbuf+y*FBW+x)=c;
}
static void draw_rectangle(int x,int y,int w,int h,unsigned short c){
    int i,j,x1=x+w>SW?SW:x+w,y1=y+h>SH?SH:y+h;
    int x0=x<0?0:x,y0=y<0?0:y;
    for(j=y0;j<y1;j++)for(i=x0;i<x1;i++)*(dbuf+j*FBW+i)=c;
}
static void cls(unsigned short c){int i;for(i=0;i<FBW*SH;i++)*(dbuf+i)=c;}
static void vsync(void){*(PIX_CTRL)=1;while(*(PIX_CTRL+3)&1);}
static void flip(void){
    volatile unsigned short *t;
    vsync();t=dbuf;dbuf=sbuf;sbuf=t;
    *(PIX_CTRL+1)=(uint32_t)(uintptr_t)dbuf;
}

static const unsigned char F5X7[][5]={
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static void draw_char(int x,int y,char c,unsigned short col){
    int ci=(int)c-32,k,r;unsigned char b;
    if(ci<0||ci>=59)return;
    for(k=0;k<5;k++){b=F5X7[ci][k];for(r=0;r<7;r++)if(b&(1<<r))draw_pixel(x+k,y+r,col);}
}
static void draw_string(int x,int y,const char*s,unsigned short col){
    while(*s){char c=(*s>='a'&&*s<='z')?*s-32:*s;draw_char(x,y,c,col);x+=6;s++;}
}
static void draw_number(int x,int y,int n,unsigned short col){
    char b[12];int i=10;b[11]=0;
    if(n==0)b[i--]='0';
    while(n>0){b[i--]='0'+(n%10);n/=10;}
    draw_string(x,y,b+i+1,col);
}

static void draw_tile(int tx,int ty,int t){
    int off=meta[ty/TH][tx/TW+camx/TW].ao,sy=ty-off;
    if(t==T_GROUND){draw_rectangle(tx,sy,TW,TH,RGB(139,90,43));draw_rectangle(tx,sy,TW,4,RGB(80,200,80));}
    else if(t==T_BRICK){draw_rectangle(tx,sy,TW,TH,BROWN);draw_rectangle(tx,sy+7,TW,2,DKBRN);draw_rectangle(tx+7,sy,2,7,DKBRN);draw_rectangle(tx+3,sy+9,2,7,DKBRN);}
    else if(t==T_COINBRICK){draw_rectangle(tx,sy,TW,TH,YELLW);draw_rectangle(tx+4,sy+3,8,10,ORNGE);draw_char(tx+5,sy+4,'?',WHITE);}
    else if(t==T_COINBRICK_USED){draw_rectangle(tx,sy,TW,TH,GRAY);draw_rectangle(tx+1,sy+1,TW-2,2,LTGRY);}
    else if(t==T_PIPE){
        draw_rectangle(tx+1,sy,TW-2,TH,PIPEG);draw_rectangle(tx+1,sy,2,TH,PIPED);
        if(ty/TH>0&&map[ty/TH-1][tx/TW+camx/TW]!=T_PIPE){draw_rectangle(tx,sy,TW,5,PIPED);draw_rectangle(tx+1,sy,TW-2,4,PIPEG);}
    }
    else if(t==T_GOAL){draw_rectangle(tx+7,sy,2,TH,POLE);draw_rectangle(tx+3,sy,10,3,GREEN);draw_rectangle(tx+3,sy,3,6,GREEN);}
}

#define SP(rx,ry,c) draw_pixel(x+(fl?(15-(rx)):(rx)),y+(ry),c)
static void row(int x,int y,int rx,int ry,int len,unsigned short c,int fl){int i;for(i=0;i<len;i++)SP(rx+i,ry,c);}
#define ROW(rx,ry,l,c) row(x,y,rx,ry,l,c,fl)

static void draw_mario(int x,int y,int fl){
    ROW(4,0,8,RED);ROW(3,1,10,RED);ROW(3,2,10,BROWN);
    ROW(2,3,4,TAN);ROW(7,3,4,TAN);ROW(2,4,12,TAN);SP(4,4,BLACK);SP(10,4,BLACK);
    ROW(3,6,4,BROWN);ROW(9,6,4,BROWN);
    ROW(2,7,12,RED);ROW(1,8,14,RED);
    ROW(1,9,14,RGB(0,0,200));ROW(2,10,12,RGB(0,0,200));
    ROW(2,11,5,RGB(0,0,200));ROW(9,11,5,RGB(0,0,200));
    ROW(2,12,5,RGB(0,0,200));ROW(9,12,5,RGB(0,0,200));
    ROW(1,13,6,BLACK);ROW(9,13,6,BLACK);ROW(1,14,7,BLACK);ROW(8,14,7,BLACK);
}

static void draw_enemy(int x,int y,int fr){
    int fl=0;
    unsigned short bo=RGB(180,100,40),dk=RGB(100,50,10),ft=RGB(80,40,10);
    ROW(2,0,12,bo);ROW(1,1,14,bo);ROW(0,2,16,bo);ROW(0,3,16,bo);
    ROW(0,4,16,bo);ROW(0,5,16,bo);
    ROW(2,2,3,WHITE);ROW(11,2,3,WHITE);SP(3,2,BLACK);SP(12,2,BLACK);SP(2,3,BLACK);SP(11,3,BLACK);
    ROW(2,1,4,dk);ROW(10,1,4,dk);
    ROW(0,6,16,bo);ROW(0,7,16,bo);ROW(1,8,14,bo);ROW(2,9,12,bo);ROW(3,10,10,bo);ROW(4,11,8,bo);
    if(fr==0){ROW(1,12,5,ft);ROW(9,12,7,dk);ROW(0,13,6,ft);ROW(9,13,7,dk);}
    else     {ROW(0,12,7,dk);ROW(10,12,5,ft);ROW(0,13,7,dk);ROW(9,13,7,ft);}
}
static void draw_enemy_flat(int x,int y){
    int fl=0;unsigned short bo=RGB(180,100,40);
    ROW(0,12,16,bo);ROW(0,13,16,bo);ROW(0,14,16,bo);ROW(0,15,16,bo);
}

static void read_ps2(void){
    volatile int*ps2=(volatile int*)PS2_ADDR;
    int d,rv,i;unsigned char sc;
    for(i=0;i<256;i++)keys_p[i]=keys[i];
    for(i=0;i<16;i++){
        d=*ps2;rv=(d>>15)&1;if(!rv)break;
        sc=d&0xFF;
        if(sc==0xF0)bcode=1;
        else if(bcode){keys[sc]=0;bcode=0;}
        else keys[sc]=1;
    }
}
static int held(int k){return keys[k];}
static int pressed(int k){return keys[k]&&!keys_p[k];}

static int solid(int t){return t==T_GROUND||t==T_BRICK||t==T_COINBRICK||t==T_COINBRICK_USED||t==T_PIPE;}
static int wtile(int draw_pixel2,int py){
    int tx=draw_pixel2/TW,ty=py/TH;
    if(tx<0||tx>=MCOLS||ty<0||ty>=MROWS)return T_GROUND;
    return map[ty][tx];
}

static void player_collision(void){
    Player*p=&pl;
    int vx=p->vx>>FX,vy=p->vy>>FX,i,tx,ty,t,ft,hd,lf,rt;
    p->x+=vx;
    ft=p->y+p->h-2; hd=p->y+4;
    if(vx>0){
        rt=p->x+p->w-1;
        for(i=hd;i<=ft;i+=8){t=wtile(rt,i);if(solid(t)){tx=(rt/TW)*TW;p->x=tx-p->w;p->vx=0;break;}}
    } else if(vx<0){
        lf=p->x;
        for(i=hd;i<=ft;i+=8){t=wtile(lf,i);if(solid(t)){tx=(lf/TW+1)*TW;p->x=tx;p->vx=0;break;}}
    }
    p->ground=0; p->y+=vy;
    lf=p->x+2; rt=p->x+p->w-3;
    if(vy>0){
        ft=p->y+p->h-1;
        for(i=lf;i<=rt;i+=8){t=wtile(i,ft);if(solid(t)){ty=(ft/TH)*TH;p->y=ty-p->h;p->vy=0;p->ground=1;break;}}
    } else if(vy<0){
        hd=p->y;
        for(i=lf;i<=rt;i+=8){
            t=wtile(i,hd);
            if(solid(t)){
                ty=(hd/TH+1)*TH;p->y=ty;p->vy=0;
                int btx=i/TW,bty=hd/TH,bt=map[bty][btx];
                if(bt==T_COINBRICK){
                    map[bty][btx]=T_COINBRICK_USED;
                    meta[bty][btx].at=8;meta[bty][btx].ao=4;
                    int j;
                    for(j=0;j<MAXE;j++){
                        if(!ents[j].on){
                            ents[j].on=1;ents[j].type=E_COIN;
                            ents[j].x=btx*TW;ents[j].y=bty*TH-TH;
                            ents[j].vx=0;ents[j].vy=-6*(1<<FX);
                            ents[j].w=8;ents[j].h=8;ents[j].life=40;
                            break;
                        }
                    }
                    p->score+=200;p->coins++;
                } else if(bt==T_BRICK){
                    map[bty][btx]=T_AIR;p->score+=50;
                } else {
                    meta[bty][btx].at=6;meta[bty][btx].ao=3;
                }
                break;
            }
        }
    }
    if(p->y>MROWS*TH){if(p->st!=PL_DEAD){p->st=PL_DEAD;p->vy=JVEL*(1<<FX)/2;p->vx=0;p->lives--;}}
    if(p->x<0){p->x=0;p->vx=0;}
}

static void entity_collision(Ent*e){
    int vx=e->vx>>FX,vy=e->vy>>FX,lf,rt,ft,hd,i,t,tx,ty;
    e->ground=0; e->x+=vx;
    ft=e->y+e->h-2; hd=e->y+4;
    if(vx!=0){
        int ed=vx>0?e->x+e->w-1:e->x;
        for(i=hd;i<=ft;i+=8){
            t=wtile(ed,i);
            if(solid(t)){
                if(vx>0){tx=(ed/TW)*TW;e->x=tx-e->w;}
                else    {tx=(ed/TW+1)*TW;e->x=tx;}
                e->vx=-e->vx;e->dir=-e->dir;break;
            }
        }
    }
    e->y+=vy;
    lf=e->x+1;rt=e->x+e->w-2;
    if(vy>=0){
        ft=e->y+e->h-1;
        for(i=lf;i<=rt;i+=8){t=wtile(i,ft);if(solid(t)){ty=(ft/TH)*TH;e->y=ty-e->h;e->vy=0;e->ground=1;break;}}
    } else {
        hd=e->y;
        for(i=lf;i<=rt;i+=8){t=wtile(i,hd);if(solid(t)){ty=(hd/TH+1)*TH;e->y=ty;e->vy=0;break;}}
    }
    if(e->y>MROWS*TH+64)e->on=0;
}

static void add_score(int pts,int sx,int sy){
    int i;pl.score+=pts;
    for(i=0;i<MAXF;i++){if(ftexts[i].t<=0){ftexts[i].pts=pts;ftexts[i].x=sx;ftexts[i].y=sy;ftexts[i].t=40;break;}}
}

static void upd_player(void){
    Player*p=&pl;
    if(p->st==PL_WIN){p->x+=2;if(p->x>(MCOLS-2)*TW)gst=GS_WIN;return;}
    if(p->st==PL_DEAD){p->vy+=GRAV*(1<<FX);p->y+=p->vy>>FX;if(p->y>SH+32)gst=(p->lives<=0)?GS_OVER:GS_DIED;return;}
    if(p->inv>0)p->inv--;

    int ac=1<<(FX-1),fr2=1<<(FX-2);
    if(held(K_LEFT)){p->vx-=ac;if(p->vx<-SPD*(1<<FX))p->vx=-SPD*(1<<FX);p->dir=-1;}
    else if(held(K_RIGHT)){p->vx+=ac;if(p->vx>SPD*(1<<FX))p->vx=SPD*(1<<FX);p->dir=1;}
    else{if(p->vx>0)p->vx-=fr2;if(p->vx<0)p->vx+=fr2;if(p->vx>-fr2&&p->vx<fr2)p->vx=0;}

    if(pressed(K_JUMP)&&p->ground)p->vy=JVEL*(1<<FX);

    p->vy+=GRAV*(1<<FX);
    if(p->vy>MAX_FALL*(1<<FX))p->vy=MAX_FALL*(1<<FX);
    player_collision();

    if(!p->ground)p->st=(p->vy<0)?PL_JUMP:PL_FALL;
    else if(p->vx!=0)p->st=PL_WALK;
    else p->st=PL_STAND;

    p->ftimer++;
    if(p->st==PL_WALK&&p->ftimer>=8){p->fr=(p->fr+1)%2;p->ftimer=0;}
    if(p->x<camx)p->x=camx;

    int ptx=(p->x+p->w/2)/TW,pty=(p->y+p->h/2)/TH;
    if(ptx>=0&&ptx<MCOLS&&pty>=0&&pty<MROWS&&map[pty][ptx]==T_GOAL){
        add_score(5000,p->x-camx,p->y);p->st=PL_WIN;
    }
}

static void upd_enemies(void){
    int i;
    for(i=0;i<MAXE;i++){
        Ent*e=&ents[i];if(!e->on)continue;
        if(e->type==E_COIN){e->vy+=GRAV*(1<<FX);e->y+=e->vy>>FX;e->life--;if(e->life<=0)e->on=0;continue;}
        if(e->type==E_GOOMBA){
            if(e->dead){e->sq--;if(e->sq<=0)e->on=0;continue;}
            if(e->x<camx-32||e->x>camx+SW+32)continue;
            e->vy+=GRAV*(1<<FX);
            if(e->vy>MAX_FALL*(1<<FX))e->vy=MAX_FALL*(1<<FX);
            entity_collision(e);
            e->ftimer++;if(e->ftimer>=12){e->fr^=1;e->ftimer=0;}
            Player*p=&pl;
            int hit=(p->x<e->x+e->w&&p->x+p->w>e->x&&p->y<e->y+e->h&&p->y+p->h>e->y);
            if(hit){
                if(p->inv>0){e->on=0;add_score(200,e->x-camx,e->y);}
                else{
                    int stomp=(p->vy>0)&&(p->y+p->h-8<=e->y+4);
                    if(stomp){e->dead=1;e->sq=20;add_score(100,e->x-camx,e->y);p->vy=JVEL*(1<<FX)/2;}
                    else if(p->st!=PL_DEAD){p->st=PL_DEAD;p->vy=JVEL*(1<<FX)/2;p->vx=0;p->lives--;}
                }
            }
        }
    }
}

static void upd_cam(void){
    int tgt=pl.x-SW*2/5;
    camx+=(tgt-camx)/4;
    if(camx<0)camx=0;
    if(camx>MCOLS*TW-SW)camx=MCOLS*TW-SW;
}

static void upd_tiles(void){
    int ty,tx;
    for(ty=0;ty<MROWS;ty++)for(tx=0;tx<MCOLS;tx++){
        TMeta*m=&meta[ty][tx];
        if(m->at>0){m->at--;m->ao=(m->at==0)?0:((m->at>4)?4:-4);}
    }
}

static void upd_floats(void){
    int i;for(i=0;i<MAXF;i++)if(ftexts[i].t>0){ftexts[i].y--;ftexts[i].t--;}
}

static void draw_bg(void){
    draw_rectangle(0,0,SW,SH,SKY);
    int cx=camx/2;
    int c1=((200-cx)%SW+SW)%SW,c2=((380-cx)%SW+SW)%SW;
    draw_rectangle(c1,20,32,12,CLOUD);draw_rectangle(c1+4,14,24,12,CLOUD);
    draw_rectangle(c2,40,28,10,CLOUD);draw_rectangle(c2+4,34,20,10,CLOUD);
}
static void draw_map(void){
    int stx=camx/TW,etx=stx+SW/TW+2,ty,tx,t;
    if(stx<0)stx=0; if(etx>MCOLS)etx=MCOLS;
    for(ty=0;ty<MROWS;ty++)for(tx=stx;tx<etx;tx++){
        t=map[ty][tx];if(t==T_AIR)continue;
        draw_tile(tx*TW-camx,ty*TH,t);
    }
}
static void draw_player(void){
    Player*p=&pl;
    int sx=p->x-camx,sy=p->y,fl=(p->dir<0)?1:0;
    if(p->inv>0&&(gframe/4)%2)return;
    draw_mario(sx,sy,fl);
}
static void draw_ents(void){
    int i;
    for(i=0;i<MAXE;i++){
        Ent*e=&ents[i];if(!e->on)continue;
        int sx=e->x-camx,sy=e->y;
        if(sx<-32||sx>SW+32)continue;
        if(e->type==E_COIN){draw_rectangle(sx+4,sy,8,8,COIN);continue;}
        if(e->type==E_GOOMBA){if(e->dead)draw_enemy_flat(sx,sy);else draw_enemy(sx,sy,e->fr);}
    }
}
static void draw_hud(void){
    draw_rectangle(0,0,SW,12,RGB(20,20,60));
    draw_string(2,2,"MARIO",RGB(200,200,200));draw_number(38,2,pl.score,WHITE);
    draw_rectangle(120,3,6,6,COIN);draw_string(128,2,"X",WHITE);draw_number(136,2,pl.coins,WHITE);
    draw_string(170,2,"WORLD 1-1",WHITE);draw_string(262,2,"TIME",WHITE);
    draw_number(292,2,gtimer/30,(gtimer<150)?RED:WHITE);
    draw_string(2,SH-10,"LIVES:",WHITE);draw_number(40,SH-10,pl.lives,YELLW);
    int i;for(i=0;i<MAXF;i++)if(ftexts[i].t>0)draw_number(ftexts[i].x,ftexts[i].y,ftexts[i].pts,YELLW);
}

static void draw_title(void){
    cls(RGB(20,20,80));
    draw_string(80,50,"SUPER MARIO",RGB(255,50,50));draw_string(95,64,"CPU EDITION",YELLW);
    draw_rectangle(40,80,SW-80,2,WHITE);
    draw_string(55,95,"PRESS ENTER TO PLAY",WHITE);
    draw_string(55,110,"W = JUMP",RGB(200,200,200));
    draw_string(55,122,"LEFT / RIGHT = MOVE",RGB(200,200,200));
    draw_string(55,134,"P = PAUSE",RGB(200,200,200));
    draw_string(55,150,"STOMP GOOMBAS TO SCORE",RGB(180,180,255));
    draw_string(55,162,"HIT ? BLOCKS FOR COINS",RGB(180,180,255));
    draw_string(55,174,"REACH THE FLAGPOLE!",RGB(100,255,100));
}
static void draw_win(void){
    cls(RGB(20,80,20));
    draw_string(80,80,"YOU WIN!",YELLW);draw_string(60,100,"FINAL SCORE:",WHITE);draw_number(60,112,pl.score,YELLW);
    draw_string(50,140,"PRESS ENTER TO PLAY AGAIN",RGB(180,255,180));
}
static void draw_over(void){
    cls(RGB(60,10,10));
    draw_string(105,80,"GAME OVER",RED);draw_string(60,110,"FINAL SCORE:",WHITE);draw_number(60,122,pl.score,YELLW);
    draw_string(50,150,"PRESS ENTER TO TRY AGAIN",RGB(255,180,180));
}
static void draw_pause(void){
    int y;for(y=0;y<SH;y+=2)draw_rectangle(0,y,SW,1,BLACK);
    draw_string(115,110,"PAUSED",WHITE);draw_string(80,126,"P = RESUME",RGB(200,200,200));
}
static void draw_died(void){
    draw_string(95,110,"YOU DIED!",RED);draw_string(60,126,"PRESS ENTER TO CONTINUE",WHITE);
    draw_number(95,142,pl.lives,YELLW);draw_string(109,142,"LIVES LEFT",WHITE);
}

static void reset_map2(void){
    int i,j;
    for(i=0;i<MROWS;i++)for(j=0;j<MCOLS;j++){
        map[i][j]=MAP[i][j];
        meta[i][j].hit=meta[i][j].at=meta[i][j].ao=0;
    }
}
static void clear_ents(void){
    int i;for(i=0;i<MAXE;i++)ents[i].on=0;for(i=0;i<MAXF;i++)ftexts[i].t=0;
}
static void init_level(void){
    int i,j;reset_map2();clear_ents();
    for(i=0;SPAWNS[i][2]!=E_NONE;i++){
        Ent*e=0;
        for(j=0;j<MAXE;j++){if(!ents[j].on){e=&ents[j];break;}}
        if(!e)break;
        e->on=1;e->type=E_GOOMBA;e->x=SPAWNS[i][0];e->y=SPAWNS[i][1];
        e->vx=-1*(1<<FX);e->vy=0;e->w=16;e->h=16;e->dir=-1;
        e->dead=e->sq=e->fr=e->ftimer=0;
    }
    pl.x=2*TW;pl.y=11*TH-16;pl.vx=pl.vy=0;pl.w=14;pl.h=16;
    pl.dir=1;pl.ground=0;pl.fr=pl.ftimer=0;
    pl.st=PL_STAND;pl.inv=0;pl.coins=0;
    camx=0;gtimer=30*200;gst=GS_PLAY;
}
static void init_game(void){
    int i;for(i=0;i<256;i++)keys[i]=keys_p[i]=0;
    bcode=0;camx=0;gframe=0;gst=GS_TITLE;
}

static void tick(void){
    read_ps2();gframe++;
    switch(gst){
    case GS_TITLE:
        draw_title();
        if(pressed(K_ENT)){pl.lives=3;pl.score=0;init_level();}
        break;
    case GS_PLAY:
        if(gtimer>0)gtimer--;
        else if(pl.st!=PL_DEAD){
            pl.st=PL_DEAD;
            pl.vy=JVEL*(1<<FX)/2;
            pl.vx=0;pl.lives--;}
        if(pressed(K_P)){gst=GS_DIED+3;break;}  /* reuse index trick — just use a local pause flag */
        upd_player();
        upd_enemies();
        upd_cam();
        upd_tiles();
        upd_floats();
        draw_bg();draw_map();draw_ents();draw_player();draw_hud();
        break;
    case GS_DIED:
        draw_bg();draw_map();draw_hud();draw_died();
        if(pressed(K_ENT)){if(pl.lives>0)init_level();else gst=GS_OVER;}
        break;
    case GS_WIN:
        draw_win();
        if(pressed(K_ENT)){pl.lives=3;pl.score=0;init_level();}
        break;
    case GS_OVER:
        draw_over();
        if(pressed(K_ENT)){pl.lives=3;pl.score=0;init_level();}
        break;
    }
}

int main(void){
    *(PIX_CTRL+1)=PIX_BUF_B;
    vsync();
    sbuf=(volatile unsigned short*)(uintptr_t)(*PIX_CTRL);
    *(PIX_CTRL+1)=PIX_BUF_A;
    dbuf=(volatile unsigned short*)PIX_BUF_A;
    cls(SKY);flip();cls(SKY);
    init_game();
    while(1){tick();flip();}
    return 0;
}