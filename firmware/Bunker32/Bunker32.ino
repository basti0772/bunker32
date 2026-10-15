#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include "Game.h"
#include "Renderer.h"
#include "Version.h"

using namespace Bunker;
TFT_eSPI tft;
Game game;
Renderer renderer;
static uint16_t expanded[320*8];
static uint16_t mirrorPixels[WIDTH*HEIGHT];
SemaphoreHandle_t mirrorFree,mirrorReady,serialMutex;
uint8_t mirrorMeta[8];
bool streaming=false,snapshot=false,calibrated=false,calibrating=false,legacyMapping=false;
uint16_t remoteInput=0,touchInput=0;
uint32_t lastRemote=0,lastHeartbeat=0,lastMirror=0;
float mapping[6]={},candidate[6]={};
uint16_t calibrationX[3],calibrationY[3];
int calibrationStep=0,fps=0;
const char* calibrationNote="";
Mode beforeCalibration=TITLE;
bool touchHeld=true;
uint32_t touchDownAt=0,releaseAt=0;
uint32_t sampleX=0,sampleY=0,samples=0;

void serialLine(const char* line) {
  xSemaphoreTake(serialMutex,portMAX_DELAY);Serial.println(line);xSemaphoreGive(serialMutex);
}
void mirrorTask(void*) {
  for(;;) {
    xSemaphoreTake(mirrorReady,portMAX_DELAY);
    const uint32_t bytes=sizeof(mirrorPixels);
    uint8_t header[16]={'B','3','2','F',uint8_t(bytes),uint8_t(bytes>>8),uint8_t(bytes>>16),uint8_t(bytes>>24)};
    memcpy(header+8,mirrorMeta,8);
    xSemaphoreTake(serialMutex,portMAX_DELAY);
    Serial.write(header,sizeof(header));Serial.write((uint8_t*)mirrorPixels,bytes);Serial.flush();
    xSemaphoreGive(serialMutex);xSemaphoreGive(mirrorFree);
  }
}
void report() {
  char line[320];snprintf(line,sizeof(line),"STATE mode=%s hp=%d ammo=%d kills=%d key=%d auto=%d x=%.3f y=%.3f angle=%.3f fps=%d heap=%u touch=%d calibrating=%d pressure=%u",
    modeName(game.mode),game.health,game.ammo,game.kills,game.key,game.autoFire,game.x,game.y,game.angle,fps,ESP.getFreeHeap(),calibrated,calibrating,tft.getTouchRawZ());
  serialLine(line);
}
void beginCalibration() {
  beforeCalibration=game.mode;if(game.mode==PLAYING)game.mode=PAUSED;
  calibrating=true;calibrationStep=0;calibrationNote="";touchHeld=true;samples=0;releaseAt=0;remoteInput=touchInput=0;
  serialLine("CALIBRATION tap four crosses; CANCEL returns to game");
}
void calibrationPoint(uint16_t x,uint16_t y) {
  if(calibrationStep<3){calibrationX[calibrationStep]=x;calibrationY[calibrationStep]=y;}
  if(calibrationStep==2) {
    float ux=int(calibrationX[1])-int(calibrationX[0]),uy=int(calibrationY[1])-int(calibrationY[0]);
    float vx=int(calibrationX[2])-int(calibrationX[0]),vy=int(calibrationY[2])-int(calibrationY[0]),det=ux*vy-uy*vx;
    if(fabsf(det)<100000){calibrationStep=0;calibrationNote="PUNKTE ZU NAH. NOCHMAL.";return;}
    candidate[0]=280*vy/det;candidate[1]=-280*vx/det;candidate[2]=20-candidate[0]*calibrationX[0]-candidate[1]*calibrationY[0];
    candidate[3]=-200*uy/det;candidate[4]=200*ux/det;candidate[5]=20-candidate[3]*calibrationX[0]-candidate[4]*calibrationY[0];
  }
  if(calibrationStep==3) {
    float px=candidate[0]*x+candidate[1]*y+candidate[2],py=candidate[3]*x+candidate[4]*y+candidate[5];
    if(fabsf(px-300)>14||fabsf(py-220)>14){calibrationStep=0;calibrationNote="UNGENAU. BITTE NOCHMAL.";return;}
    memcpy(mapping,candidate,sizeof(mapping));calibrated=true;legacyMapping=false;calibrating=false;game.mode=beforeCalibration;
    Preferences prefs;bool saved=false;if(prefs.begin("bunker32",false)){saved=prefs.putBytes("rot1-v1",mapping,sizeof(mapping))==sizeof(mapping);prefs.end();}
    serialLine(saved?"CALIBRATED saved=1":"CALIBRATED saved=0");return;
  }
  ++calibrationStep;
}
void touchPoll() {
  uint32_t now=millis();touchInput=0;
  if(tft.getTouchRawZ()<300) {
    if(!releaseAt)releaseAt=now;
    if(now-releaseAt>65){touchHeld=false;touchDownAt=0;samples=sampleX=sampleY=0;}
    return;
  }
  releaseAt=0;uint16_t rx,ry;tft.getTouchRaw(&rx,&ry);
  if(calibrating) {
    if(touchHeld)return;
    if(!touchDownAt)touchDownAt=now;
    sampleX+=rx;sampleY+=ry;++samples;
    if(now-touchDownAt>180&&samples>=5){touchHeld=true;calibrationPoint(sampleX/samples,sampleY/samples);samples=sampleX=sampleY=0;}
    return;
  }
  if(!calibrated){if(!touchHeld)beginCalibration();return;}
  int x=lroundf(mapping[0]*rx+mapping[1]*ry+mapping[2]),y=lroundf(mapping[3]*rx+mapping[4]*ry+mapping[5]);
  if(legacyMapping){x=319-x;y=239-y;}
  if(x<0||x>=320||y<0||y>=240)return;
  bool pressed=!touchHeld;touchHeld=true;
  if(game.mode==TITLE||game.mode==WON||game.mode==DEAD){if(pressed)game.reset();return;}
  if(game.mode==PAUSED){
    if(pressed){if(y>=80&&y<140){if(x<160)game.autoFire=!game.autoFire;else beginCalibration();}else game.togglePause();}return;
  }
  if(y<24&&pressed){if(x<65)game.togglePause();else if(x>240)game.mapOpen=!game.mapOpen;return;}
  if(y>=198){const uint16_t buttons[]={BACK,LEFT,FORWARD,RIGHT,FIRE};touchInput=buttons[x/64];}
  // Drag within the viewport: top/bottom moves, left/right turns, diagonals combine.
  else if(y>=28&&y<172){if(x<110)touchInput|=LEFT;else if(x>210)touchInput|=RIGHT;if(y<83)touchInput|=FORWARD;else if(y>127)touchInput|=BACK;}
}
void command(char* line) {
  if(strncmp(line,"K ",2)==0) {
    char* end=nullptr;long value=strtol(line+2,&end,10);
    if(end!=line+2&&*end=='\0'&&value>=0&&value<=127){remoteInput=uint16_t(value);lastRemote=millis();lastHeartbeat=millis();}return;
  }
  if(strcmp(line,"STREAM 1")==0){streaming=true;lastHeartbeat=millis();return;}
  if(strcmp(line,"STREAM 0")==0){streaming=false;return;}
  if(strcmp(line,"SNAP")==0){snapshot=true;return;}
  if(strcmp(line,"STATE")==0){report();return;}
  if(strcmp(line,"SELFTEST")==0){char result[64];snprintf(result,sizeof(result),"SELFTEST failures=%d",selfTest());serialLine(result);return;}
  if(strcmp(line,"CAL")==0){if(!calibrating)beginCalibration();return;}
  if(strcmp(line,"CANCEL")==0){if(calibrating){calibrating=false;game.mode=beforeCalibration;}return;}
  if(calibrating){serialLine("BUSY calibration; CANCEL to return");return;}
  if(strcmp(line,"START")==0){if(game.mode==TITLE||game.mode==WON||game.mode==DEAD)game.reset();else if(game.mode==PAUSED)game.mode=PLAYING;}
  else if(strcmp(line,"NEW")==0){game.reset();}
  else if(strcmp(line,"MENU")==0){game.reset();game.mode=TITLE;remoteInput=0;}
  else if(strcmp(line,"PAUSE")==0){game.togglePause();remoteInput=0;}
  else if(strcmp(line,"AUTO")==0){game.autoFire=!game.autoFire;}
  else if(strcmp(line,"MAP")==0){game.mapOpen=!game.mapOpen;}
  else if(strcmp(line,"HELP")==0)serialLine("BUNKER32 v" BUNKER32_VERSION " | K 0..127, START, NEW, PAUSE, AUTO, MAP, CAL, CANCEL, STATE, SELFTEST, SNAP, STREAM 0/1");
}
void serialPoll() {
  static char line[96];static size_t length=0;static bool overflow=false;
  int budget=256;
  while(Serial.available()&&budget-->0){char c=Serial.read();if(c=='\r')continue;if(c=='\n'){if(!overflow){line[length]=0;command(line);}length=0;overflow=false;}else if(length<sizeof(line)-1){line[length++]=c;}else overflow=true;}
  if(millis()-lastRemote>250)remoteInput=0;
  if(streaming&&millis()-lastHeartbeat>2000)streaming=false;
}
void displayFrame() {
  tft.startWrite();
  for(int row=0;row<HEIGHT;row+=4){
    for(int y=0;y<4;++y)for(int x=0;x<WIDTH;++x){uint16_t c=renderer.pixels[(row+y)*WIDTH+x];int at=y*640+x*2;expanded[at]=expanded[at+1]=expanded[at+320]=expanded[at+321]=c;}
    tft.pushImage(0,row*2,320,8,expanded);
  }
  tft.endWrite();
}
void setup() {
  Serial.setRxBufferSize(1024);Serial.begin(921600);
  mirrorFree=xSemaphoreCreateBinary();mirrorReady=xSemaphoreCreateBinary();serialMutex=xSemaphoreCreateMutex();
  xSemaphoreGive(mirrorFree);xTaskCreatePinnedToCore(mirrorTask,"usb-mirror",3072,nullptr,1,nullptr,0);
  tft.init();tft.setRotation(1);tft.setSwapBytes(true);
  Preferences prefs;
  if(prefs.begin("bunker32",true)){if(prefs.getBytesLength("rot1-v1")==sizeof(mapping)){prefs.getBytes("rot1-v1",mapping,sizeof(mapping));calibrated=true;}prefs.end();}
  if(!calibrated&&prefs.begin("chess-touch",true)){if(prefs.getBytesLength("rot3-v1")==sizeof(mapping)){prefs.getBytes("rot3-v1",mapping,sizeof(mapping));calibrated=true;legacyMapping=true;}prefs.end();}
  for(float v:mapping)if(!isfinite(v))calibrated=false;
  serialLine("BUNKER32 READY v" BUNKER32_VERSION " baud=921600 display=ILI9341 rotation=1");report();
}
void loop() {
  static uint32_t previous=millis(),fpsSince=millis();static int count=0;
  uint32_t now=millis();float dt=(now-previous)/1000.f;previous=now;
  serialPoll();touchPoll();
  uint16_t input=remoteInput|touchInput;
  if(!calibrating)game.update(dt,input);
  if(calibrating)renderer.calibration(calibrationStep,calibrationNote);else renderer.draw(game,fps,input,calibrated);
  displayFrame();
  if((snapshot||(streaming&&now-lastMirror>=550))&&xSemaphoreTake(mirrorFree,0)==pdTRUE){
    memcpy(mirrorPixels,renderer.pixels,sizeof(mirrorPixels));
    mirrorMeta[0]=game.health;mirrorMeta[1]=min(game.ammo,255);mirrorMeta[2]=game.kills;mirrorMeta[3]=game.mode;
    mirrorMeta[4]=fps;mirrorMeta[5]=game.key;mirrorMeta[6]=game.autoFire;mirrorMeta[7]=calibrated;
    snapshot=false;lastMirror=now;xSemaphoreGive(mirrorReady);
  }
  ++count;if(now-fpsSince>=1000){fps=count;count=0;fpsSince=now;}
  delay(1);
}
