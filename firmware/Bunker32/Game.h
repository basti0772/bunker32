#pragma once
#include <stdint.h>
#include <math.h>

namespace Bunker {
constexpr int MAP_SIZE = 16, ENEMIES = 9, ITEMS = 7;
enum Input : uint16_t { FORWARD=1, BACK=2, LEFT=4, RIGHT=8, STRAFE_L=16, STRAFE_R=32, FIRE=64 };
enum Mode : uint8_t { TITLE, PLAYING, PAUSED, WON, DEAD };
enum ItemType : uint8_t { MEDKIT, AMMO, KEY };
struct Enemy { float x,y,cooldown; int hp; float hurt; };
struct Item { float x,y; ItemType type; bool taken; };
struct Ray { float distance, u; int material; bool side; };
struct Game {
  float x,y,angle,elapsed,shotTime,damageTime,fireCooldown,noticeTime,walk;
  int health,ammo,kills,shots,hits;
  bool key,autoFire,mapOpen;
  Mode mode;
  const char* notice;
  Enemy enemies[ENEMIES];
  Item items[ITEMS];
  Game();
  void reset();
  void update(float dt,uint16_t input);
  void shoot();
  void togglePause();
  void message(const char* text,float seconds=2.0f);
  void move(float dx,float dy);
  int cell(int cx,int cy) const;
  bool freeAt(float px,float py,float radius=0.18f) const;
  bool sight(float ax,float ay,float bx,float by) const;
  Ray cast(float dx,float dy) const;
  int target() const;
};
const char* modeName(Mode m);
int selfTest();
}
