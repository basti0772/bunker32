#include "Game.h"
#include <algorithm>

namespace Bunker {
static const char* const world[MAP_SIZE] = {
  "1111111111111111",
  "1000000100000001",
  "1000000100020001",
  "1000000000020001",
  "1000000100000001",
  "1110111110110111",
  "1000000100000001",
  "1000000100000001",
  "1000000000000001",
  "1011011101110101",
  "1000000000010001",
  "1000000100010001",
  "1110110100000001",
  "1000000100030001",
  "1000000000030041",
  "1111111111111111"
};
Game::Game() { reset(); mode=TITLE; }
void Game::reset() {
  x=3.5f; y=2.5f; angle=1.5707963f; elapsed=shotTime=damageTime=fireCooldown=walk=0;
  health=100; ammo=48; kills=shots=hits=0; key=false; autoFire=true; mapOpen=false; mode=PLAYING;
  const float spawns[ENEMIES][2]={{4.5f,4.3f},{3.5f,7.5f},{9.5f,3.5f},{12.5f,7.5f},{5.5f,10.5f},{2.5f,13.5f},{9.5f,12.5f},{13.5f,11.5f},{13.5f,13.5f}};
  for(int i=0;i<ENEMIES;++i) enemies[i]={spawns[i][0],spawns[i][1],2.5f+i*0.11f,2,0};
  items[0]={4.5f,3.5f,AMMO,false}; items[1]={2.5f,7.5f,MEDKIT,false};
  items[2]={13.5f,2.5f,KEY,false}; items[3]={10.5f,6.5f,AMMO,false};
  items[4]={6.5f,11.5f,AMMO,false}; items[5]={3.5f,14.5f,MEDKIT,false};
  items[6]={12.5f,12.5f,AMMO,false}; message("FINDE DEN GOLDENEN SCHLUESSEL",4);
}
const char* modeName(Mode m) {
  switch(m) {case TITLE:return "TITLE";case PLAYING:return "PLAYING";case PAUSED:return "PAUSED";case WON:return "WON";default:return "DEAD";}
}
int Game::cell(int cx,int cy) const {
  if(cx<0||cy<0||cx>=MAP_SIZE||cy>=MAP_SIZE) return 1;
  return world[cy][cx]-'0';
}
bool Game::freeAt(float px,float py,float r) const {
  return cell(int(floorf(px-r)),int(floorf(py-r)))==0 && cell(int(floorf(px+r)),int(floorf(py-r)))==0 &&
         cell(int(floorf(px-r)),int(floorf(py+r)))==0 && cell(int(floorf(px+r)),int(floorf(py+r)))==0;
}
void Game::move(float dx,float dy) { if(freeAt(x+dx,y)) x+=dx; if(freeAt(x,y+dy)) y+=dy; }
bool Game::sight(float ax,float ay,float bx,float by) const {
  float dx=bx-ax,dy=by-ay; int steps=int(sqrtf(dx*dx+dy*dy)*14)+1;
  for(int i=1;i<=steps;++i) { float t=float(i)/steps; if(cell(int(floorf(ax+dx*t)),int(floorf(ay+dy*t)))) return false; }
  return true;
}
Ray Game::cast(float dx,float dy) const {
  int mx=int(x),my=int(y),sx=dx<0?-1:1,sy=dy<0?-1:1;
  float ddx=fabsf(dx)<0.00001f?1e8f:fabsf(1/dx),ddy=fabsf(dy)<0.00001f?1e8f:fabsf(1/dy);
  float tx=(dx<0?x-mx:mx+1-x)*ddx,ty=(dy<0?y-my:my+1-y)*ddy;
  bool side=false; int wall=1;
  for(int i=0;i<64;++i) {
    if(tx<ty) {tx+=ddx;mx+=sx;side=false;} else {ty+=ddy;my+=sy;side=true;}
    wall=cell(mx,my); if(wall) break;
  }
  float d=std::max(0.05f,side?ty-ddy:tx-ddx),u=side?x+d*dx:y+d*dy;
  u-=floorf(u); if((!side&&dx>0)||(side&&dy<0)) u=1-u;
  return {d,u,wall,side};
}
int Game::target() const {
  float best=20; int found=-1; float dx=cosf(angle),dy=sinf(angle);
  for(int i=0;i<ENEMIES;++i) {
    const Enemy &e=enemies[i]; if(e.hp<=0) continue;
    float ex=e.x-x,ey=e.y-y,forward=ex*dx+ey*dy,across=fabsf(ex*dy-ey*dx);
    if(forward>0.1f && forward<best && across<0.26f+forward*0.025f && sight(x,y,e.x,e.y)) {best=forward;found=i;}
  }
  return found;
}
void Game::shoot() {
  if(mode!=PLAYING||fireCooldown>0) return;
  if(ammo<=0) {message("KEINE MUNITION - SUCHE KISTEN");fireCooldown=0.4f;return;}
  --ammo; ++shots; shotTime=0.12f; fireCooldown=0.28f;
  int i=target(); if(i>=0) { ++hits; Enemy &e=enemies[i]; --e.hp;e.hurt=0.22f;e.cooldown=std::max(e.cooldown,0.5f); if(e.hp==0) {++kills;ammo+=3;message("WACHE BESIEGT  +3 MUNITION",0.8f);} }
}
void Game::message(const char* text,float seconds) {notice=text;noticeTime=seconds;}
void Game::togglePause() { if(mode==PLAYING)mode=PAUSED;else if(mode==PAUSED)mode=PLAYING; }
void Game::update(float dt,uint16_t input) {
  if(mode!=PLAYING) return;
  dt=std::max(0.f,std::min(dt,0.08f)); elapsed+=dt;
  shotTime=std::max(0.f,shotTime-dt);damageTime=std::max(0.f,damageTime-dt);
  noticeTime=std::max(0.f,noticeTime-dt);fireCooldown=std::max(0.f,fireCooldown-dt);
  angle+=((input&RIGHT?1.f:0.f)-(input&LEFT?1.f:0.f))*1.9f*dt;
  if(angle>6.2831853f)angle-=6.2831853f;if(angle<0)angle+=6.2831853f;
  float forward=(input&FORWARD?1.f:0.f)-(input&BACK?1.f:0.f),strafe=(input&STRAFE_R?1.f:0.f)-(input&STRAFE_L?1.f:0.f);
  float norm=(forward&&strafe)?0.70710678f:1.f,dx=cosf(angle),dy=sinf(angle);
  move((dx*forward-dy*strafe)*dt*2.15f*norm,(dy*forward+dx*strafe)*dt*2.15f*norm);
  if(forward||strafe)walk+=dt*10;
  if((input&FIRE)||(autoFire&&target()>=0))shoot();
  for(Item &item:items) {
    float ix=x-item.x,iy=y-item.y;
    if(item.taken||ix*ix+iy*iy>0.32f)continue;
    if(item.type==MEDKIT&&health==100)continue;
    item.taken=true;
    if(item.type==KEY){key=true;message("SCHLUESSEL! SUCHE DEN GRUENEN AUSGANG",3);}
    else if(item.type==MEDKIT){health=std::min(100,health+40);message("VERBAND  +40 GESUNDHEIT");}
    else {ammo+=24;message("MUNITIONSKISTE  +24");}
  }
  for(Enemy &e:enemies) {
    e.hurt=std::max(0.f,e.hurt-dt);if(e.hp<=0)continue;
    float ex=x-e.x,ey=y-e.y,d=sqrtf(ex*ex+ey*ey);
    if(d>6.5f||!sight(e.x,e.y,x,y))continue;
    e.cooldown-=dt;
    if(d>1.7f) {
      float nx=e.x+ex/d*dt*0.58f,ny=e.y+ey/d*dt*0.58f;
      if(freeAt(nx,e.y,0.2f))e.x=nx;if(freeAt(e.x,ny,0.2f))e.y=ny;
    }
    if(d<4.5f && e.cooldown<=0 && e.hurt<=0) {
      health-=6;damageTime=0.18f;e.cooldown=1.6f;
      if(health<=0){health=0;mode=DEAD;message("MISSION GESCHEITERT");return;}
    }
  }
  if(x>13.4f&&y>13.4f) {
    if(key){mode=WON;message("MISSION ERFUELLT");}
    else if(noticeTime<=0)message("DER AUSGANG BRAUCHT DEN SCHLUESSEL");
  }
}
// Runs on the device with separate state; leaves the current game untouched.
int selfTest() {
  int failures=0;Game g;g.reset();
  if(!g.freeAt(g.x,g.y)||g.cell(-1,0)!=1)++failures;
  g.x=1.2f;g.y=1.2f;g.move(-0.3f,0);if(g.x!=1.2f)++failures;
  g.x=3.5f;g.y=2.5f;if(g.sight(3.5f,2.5f,8.5f,2.5f))++failures;
  Ray r=g.cast(0,-1);if(fabsf(r.distance-1.5f)>0.001f)++failures;
  // Every floor tile, pickup, enemy and exit approach must be reachable.
  bool seen[16][16]={};int qx[256],qy[256],head=0,tail=1;qx[0]=3;qy[0]=2;seen[2][3]=true;
  const int vx[4]={1,-1,0,0},vy[4]={0,0,1,-1};
  while(head<tail) {int cx=qx[head],cy=qy[head++];for(int i=0;i<4;++i){int nx=cx+vx[i],ny=cy+vy[i];if(g.cell(nx,ny)==0&&!seen[ny][nx]){seen[ny][nx]=true;qx[tail]=nx;qy[tail++]=ny;}}}
  for(int yy=0;yy<16;++yy)for(int xx=0;xx<16;++xx)if(!g.cell(xx,yy)&&!seen[yy][xx])++failures;
  for(const Item &i:g.items)if(!seen[int(i.y)][int(i.x)])++failures;
  for(const Enemy &e:g.enemies)if(!g.freeAt(e.x,e.y))++failures;
  for(Enemy &e:g.enemies)e.hp=0;
  g.enemies[0]={3.5f,4.0f,10,2,0};g.angle=1.5707963f;g.autoFire=false;
  g.shoot();g.fireCooldown=0;g.shoot();if(g.kills!=1||g.hits!=2||g.ammo!=49)++failures;
  g.reset();g.x=13.5f;g.y=14.5f;for(Enemy &e:g.enemies)e.hp=0;g.update(.01f,0);if(g.mode==WON)++failures;
  g.key=true;g.update(.01f,0);if(g.mode!=WON)++failures;
  g.reset();g.togglePause();float old=g.x;g.update(1,FORWARD);if(g.x!=old||g.elapsed!=0)++failures;
  return failures;
}
}
