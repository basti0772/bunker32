#pragma once
#include "Game.h"

namespace Bunker {
constexpr int WIDTH=160, HEIGHT=120, VIEW_HEIGHT=88;
struct Renderer {
  uint16_t pixels[WIDTH*HEIGHT];
  float depth[WIDTH];
  void draw(const Game &g,int fps,uint16_t input,bool calibrated);
  void rect(int x,int y,int w,int h,uint16_t color);
  void text(int x,int y,const char* s,uint16_t color,int scale=1);
  void center(int y,const char* s,uint16_t color,int scale=1);
  void calibration(int step,const char* note);
  static uint16_t rgb(int r,int g,int b);
private:
  void scene(const Game &g);
  void billboard(const Game &g,float x,float y,int type,bool hurt);
};
}
