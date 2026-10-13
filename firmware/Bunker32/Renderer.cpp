#include "Renderer.h"
#include <algorithm>
#include <string.h>
#include <stdio.h>

namespace Bunker {
uint16_t Renderer::rgb(int r,int g,int b) {return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
static const uint16_t INK=0x0842,WHITE=0xef5c,GOLD=0xfd65,CYAN=0x4df9;
void Renderer::rect(int x,int y,int w,int h,uint16_t color) {
  int x0=std::max(x,0),x1=std::min(x+w,WIDTH),y0=std::max(y,0),y1=std::min(y+h,HEIGHT);
  for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)pixels[yy*WIDTH+xx]=color;
}
void Renderer::text(int x,int y,const char* s,uint16_t color,int scale) {
  const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./+!><=";
  static const uint32_t glyph[]={
    0x25755,0x65656,0x34443,0x65556,0x74647,0x74644,0x34553,0x55755,0x72227,
    0x11152,0x55655,0x44447,0x57755,0x57555,0x25552,0x65644,0x25573,0x65655,
    0x34216,0x72222,0x55557,0x55552,0x55775,0x55255,0x55222,0x71247,
    0x75557,0x26227,0x71247,0x71217,0x55711,0x74617,0x74657,0x71111,0x75757,0x75717,
    0x02020,0x00700,0x00002,0x11244,0x02720,0x22202,0x42124,0x12421,0x07070};
  for(;*s;++s,x+=4*scale) {
    char c=*s;if(c>='a'&&c<='z')c-=32;const char* found=strchr(alphabet,c);if(!found)continue;
    uint32_t bits=glyph[found-alphabet];
    for(int row=0;row<5;++row)for(int col=0;col<3;++col)if(bits&(1u<<(4*(4-row)+2-col)))rect(x+col*scale,y+row*scale,scale,scale,color);
  }
}
void Renderer::center(int y,const char* s,uint16_t color,int scale) {text((WIDTH-int(strlen(s))*4*scale+scale)/2,y,s,color,scale);}
void Renderer::scene(const Game &g) {
  for(int yy=0;yy<VIEW_HEIGHT;++yy) {
    bool floor=yy>=VIEW_HEIGHT/2;int shade=floor?24+(yy-VIEW_HEIGHT/2)/2:16+yy/5;
    rect(0,yy,WIDTH,1,rgb(shade,floor?shade-1:shade+3,floor?shade-3:shade+9));
  }
  float dx=cosf(g.angle),dy=sinf(g.angle),planeX=-dy*0.66f,planeY=dx*0.66f;
  for(int xx=0;xx<WIDTH;++xx) {
    float cam=2.f*(xx+0.5f)/WIDTH-1,rx=dx+planeX*cam,ry=dy+planeY*cam;
    Ray ray=g.cast(rx,ry);depth[xx]=ray.distance;
    int wallH=int(VIEW_HEIGHT/ray.distance),top=(VIEW_HEIGHT-wallH)/2;
    int start=std::max(0,top),end=std::min(VIEW_HEIGHT,top+wallH);
    float light=(ray.side?0.68f:0.95f)/(1+ray.distance*0.055f);
    int tx=int(ray.u*32)&31;
    for(int yy=start;yy<end;++yy) {
      int ty=((yy-top)*32/std::max(1,wallH))&31;
      int noise=((tx*13+ty*7)^((tx+ty)*3))&7;
      int r=105+noise,gc=106+noise,b=112+noise;
      bool mortar=(ty%8==0)||((tx+((ty/8)%2)*16)%32==0);
      if(mortar){r=44;gc=45;b=49;}
      if(ray.material==2){r=104+noise;gc=47+noise;b=32+noise;if(mortar){r=40;gc=24;b=20;}}
      if(ray.material==3){r=45+noise;gc=67+noise;b=76+noise;if(tx%8==0||ty==0){r=22;gc=29;b=35;}}
      if(ray.material==4){r=18;gc=110+(tx%4)*9;b=55;if(ty<5||ty>26||tx<3||tx>28){r=23;gc=36;b=31;}if(ty>11&&ty<20&&tx>8&&tx<24){r=175;gc=240;b=153;}}
      pixels[yy*WIDTH+xx]=rgb(int(r*light),int(gc*light),int(b*light));
    }
  }
  struct Object {float distance,x,y;int type;bool hurt;};Object objects[ENEMIES+ITEMS];int n=0;
  for(const Enemy &e:g.enemies)if(e.hp>0)objects[n++]={(e.x-g.x)*(e.x-g.x)+(e.y-g.y)*(e.y-g.y),e.x,e.y,0,e.hurt>0};
  for(const Item &i:g.items)if(!i.taken)objects[n++]={(i.x-g.x)*(i.x-g.x)+(i.y-g.y)*(i.y-g.y),i.x,i.y,1+int(i.type),false};
  std::sort(objects,objects+n,[](const Object &a,const Object &b){return a.distance>b.distance;});
  for(int i=0;i<n;++i)billboard(g,objects[i].x,objects[i].y,objects[i].type,objects[i].hurt);
  // Pixel-art pistol, with recoil and muzzle flash.
  int bob=int(sinf(g.walk)*1.5f),gunY=69+bob+(g.shotTime>0?3:0);
  if(g.shotTime>0){rect(76,gunY-13,8,14,GOLD);rect(70,gunY-9,20,5,rgb(255,173,41));rect(78,gunY-17,4,16,WHITE);}
  rect(72,gunY+5,18,19,rgb(142,99,69));rect(76,gunY,12,22,rgb(30,33,36));
  rect(77,gunY-7,10,18,rgb(94,103,111));rect(79,gunY-8,6,7,rgb(174,183,189));
  rect(79,gunY-6,6,3,INK);rect(80,gunY-9,3,2,GOLD);rect(80,gunY+9,5,11,rgb(46,45,42));
  uint16_t cross=g.target()>=0?GOLD:WHITE;
  rect(75,43,3,1,cross);rect(82,43,3,1,cross);rect(80,38,1,3,cross);rect(80,46,1,3,cross);
  if(g.damageTime>0){rect(0,0,160,3,0xc900);rect(0,85,160,3,0xc900);rect(0,0,3,88,0xc900);rect(157,0,3,88,0xc900);}
}
void Renderer::billboard(const Game &g,float wx,float wy,int type,bool hurt) {
  float dx=cosf(g.angle),dy=sinf(g.angle),ex=wx-g.x,ey=wy-g.y;
  float z=ex*dx+ey*dy;if(z<0.15f)return;
  float side=-ex*dy+ey*dx;int h=std::min(400,int(VIEW_HEIGHT/z)),w=h*2/3;
  int cx=int(WIDTH/2+side/z*WIDTH/(2*0.66f)),left=cx-w/2,top=(VIEW_HEIGHT-h)/2;
  static const char* const guard[]={
    "     HHHHHH     ","    HHHHHHHH    ","    HHHHHHHH    ","     SSSSSS     ",
    "     SXSSXS     ","     SSSSSS     ","      SSSS      ","   UUUUUUUUUU   ",
    "  UUUUUUUUUUUU  "," SSUUUUUUUUUUSS "," SSUUUUUUUUUUSS "," SSUUUGGGGGGUSS ",
    "  UUUUGGGGGGUU  ","   UUUUUUUUUU   ","   BBBBYBBBBB   ","    UUUUUUUU    ",
    "    UUUUUUUU    ","    UUU  UUU    ","    UUU  UUU    ","    UUU  UUU    ",
    "    BBB  BBB    ","    BBB  BBB    ","   BBBB  BBBB   ","   BBBB  BBBB   "};
  for(int xx=std::max(0,left);xx<std::min(WIDTH,left+w);++xx) {
    if(z>=depth[xx])continue;int u=(xx-left)*16/std::max(1,w);
    for(int yy=std::max(0,top);yy<std::min(VIEW_HEIGHT,top+h);++yy) {
      int v=(yy-top)*24/std::max(1,h);uint16_t c=0;
      if(type==0) {
        char p=guard[v][u];if(p==' ')continue;
        switch(p){case 'H':c=rgb(69,79,84);break;case 'S':c=rgb(204,162,114);break;case 'X':c=INK;break;case 'U':c=rgb(75,100,56);break;case 'G':c=rgb(45,48,49);break;case 'Y':c=GOLD;break;default:c=rgb(25,25,25);}
        if(hurt)c=rgb(245,110,82);
      } else {
        if(v<15||v>22||u<3||u>12)continue;
        if(type==1){c=WHITE;if((u>=7&&u<=8)||(v>=18&&v<=19))c=0xe900;}
        if(type==2){c=rgb(109,79,35);if(u==3||u==12||v==15||v==22)c=GOLD;else if(u%3==0&&v>16&&v<21)c=rgb(198,161,63);}
        if(type==3){if(!((u>=6&&u<=9&&v<=18)||(u==7&&v>=18)||(v==21&&u>=7)))continue;c=GOLD;}
      }
      pixels[yy*WIDTH+xx]=c;
    }
  }
}
void Renderer::draw(const Game &g,int fps,uint16_t input,bool calibrated) {
  scene(g);
  rect(0,88,160,32,INK);rect(0,88,160,1,GOLD);
  char info[48];snprintf(info,sizeof(info),"HP %03d  MUN %02d  %d/9  %s",g.health,g.ammo,g.kills,g.key?"KEY":"---");
  text(3,91,info,g.health<30?0xf986:WHITE);
  const char* labels[]={"ZUR","LINKS","VOR","RECHTS","FEUER"};uint16_t bits[]={BACK,LEFT,FORWARD,RIGHT,FIRE};
  for(int i=0;i<5;++i){rect(i*32+1,100,30,19,input&bits[i]?rgb(112,86,29):rgb(31,43,49));text(i*32+(32-int(strlen(labels[i]))*4)/2,107,labels[i],i==4?GOLD:WHITE);}
  rect(0,0,22,10,INK);text(2,2,"MENU",WHITE);rect(131,0,29,10,INK);text(133,2,"KARTE",CYAN);
  if(g.noticeTime>0&&g.mode==PLAYING){rect(0,78,160,10,INK);center(80,g.notice,GOLD);}
  if(g.mapOpen){
    rect(3,13,52,59,INK);text(5,15,"KARTE",GOLD);
    for(int yy=0;yy<16;++yy)for(int xx=0;xx<16;++xx)rect(5+xx*3,23+yy*3,3,3,g.cell(xx,yy)==4?0x3e65:g.cell(xx,yy)?0x528a:0x18c3);
    for(const Item &i:g.items)if(!i.taken&&i.type==KEY)rect(5+int(i.x*3),23+int(i.y*3),2,2,GOLD);
    rect(5+int(g.x*3)-1,23+int(g.y*3)-1,3,3,CYAN);
    rect(5+int((g.x+cosf(g.angle)*0.6f)*3),23+int((g.y+sinf(g.angle)*0.6f)*3),1,1,WHITE);
  }
  if(g.mode!=PLAYING){
    rect(7,13,146,71,INK);rect(7,13,146,1,GOLD);
    if(g.mode==TITLE){center(20,"BUNKER 32",GOLD,3);center(40,"EIN RETRO-EGO-SHOOTER",WHITE);center(49,"SCHLUESSEL FINDEN. AUSGANG ERREICHEN.",CYAN);center(59,"TOUCH: BEWEGEN + AUTO-FEUER",WHITE);center(72,"ANTIPPEN / ENTER: START",GOLD);}
    if(g.mode==PAUSED){center(21,"PAUSE",GOLD,3);center(43,g.autoFire?"AUTO-FEUER: AN":"AUTO-FEUER: AUS",WHITE);center(55,"LINKS TIPPEN: AUTO-FEUER",CYAN);center(65,"RECHTS TIPPEN: TOUCH KALIBRIEREN",CYAN);center(76,"UNTEN / P: WEITER",GOLD);}
    if(g.mode==WON||g.mode==DEAD){center(22,g.mode==WON?"ENTKOMMEN!":"GAME OVER",g.mode==WON?CYAN:GOLD,2);snprintf(info,sizeof(info),"ZEIT %d:%02d  WACHEN %d/9",int(g.elapsed)/60,int(g.elapsed)%60,g.kills);center(43,info,WHITE);snprintf(info,sizeof(info),"TREFFER %d / %d",g.hits,g.shots);center(55,info,WHITE);center(71,"ANTIPPEN / ENTER: NEUSTART",GOLD);}
  }
  if(!calibrated&&g.mode==TITLE){rect(0,112,160,8,INK);center(114,"TOUCH: ZUERST KALIBRIEREN",GOLD);}
  (void)fps;
}
void Renderer::calibration(int step,const char* note) {
  rect(0,0,160,120,INK);center(40,"TOUCH KALIBRIEREN",GOLD);char s[32];snprintf(s,sizeof(s),"KREUZ %d VON 4 HALTEN",step+1);center(51,s,WHITE);center(62,"DANN FINGER ABHEBEN",WHITE);center(78,note,CYAN);
  const int xs[]={10,150,10,150},ys[]={10,10,110,110};int x=xs[step],y=ys[step];rect(x-6,y,13,1,GOLD);rect(x,y-6,1,13,GOLD);
}
}
