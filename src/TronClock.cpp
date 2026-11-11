#include "MatrixClock.h"
#include "TFTs.h"
#include <math.h>

namespace {
const char *const routes[] = {"ECABDFE","FDBDF","ECDBABDCEF","FEFDCDBABDF","FDCACDBDF","FEFDCABACDF","ECABACDFECDFE","FDBABDF","ECABDCEFDBACDFE","FEFDBACDF"};
const uint8_t pointX[] = {34,101,34,101,34,101};
const uint8_t pointY[] = {36,36,104,104,172,172};
const uint8_t screens[] = {HOURS_TENS,HOURS_ONES,MINUTES_TENS,MINUTES_ONES,SECONDS_TENS,SECONDS_ONES};

constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

__attribute__((noinline)) void drawLightRibbon(TFT_eSprite &sprite, int xEnd, int bike) {
    int y = 208 + bike * 12;
    uint16_t halo = tfts->dimColor(bike ? c565(65, 18, 110) : c565(4, 75, 110));
    sprite.drawFastHLine(0, y, TFT_WIDTH, halo);
    if (xEnd <= 0) return;
    int w = xEnd < TFT_WIDTH ? xEnd : TFT_WIDTH;
    sprite.fillRect(0, y - 2, w, 5, halo);
    sprite.fillRect(0, y - 1, w, 3, tfts->dimColor(bike ? c565(170, 65, 255) : c565(10, 225, 255)));
    sprite.drawFastHLine(0, y, w, tfts->dimColor(bike ? c565(235, 180, 255) : c565(195, 250, 255)));
}

// Follow a continuous digit path. Reversing it lets the two cycles draw
// opposite ends of the same number and meet along its illuminated outline.
__attribute__((noinline)) void trace(TFT_eSprite &sprite, int digit, int progress, bool reverse, int bike,
           uint8_t brightness, int &headX, int &headY, int &direction, int &startX) {
    const char *path = routes[digit];
    int count = strlen(path), length = 0;
    for (int n=1; n<count; ++n) {
        int a=path[n-1]-'A', b=path[n]-'A';
        length += abs(int(pointX[b])-pointX[a]) + abs(int(pointY[b])-pointY[a]);
    }
    int remaining = length * progress / 1000;
    int first = path[reverse ? count-1 : 0]-'A';
    startX = headX = pointX[first]; headY = pointY[first]; direction = 0;
    for (int n=1; n<count && remaining>0; ++n) {
        int a=path[reverse ? count-n : n-1]-'A', b=path[reverse ? count-n-1 : n]-'A';
        int x=pointX[a], y=pointY[a], dx=int(pointX[b])-x, dy=int(pointY[b])-y;
        int distance=abs(dx)+abs(dy), amount=remaining<distance ? remaining : distance;
        headX=x+dx*amount/distance; headY=y+dy*amount/distance;
        direction=dx>0 ? 0 : dy>0 ? 1 : dx<0 ? 2 : 3;
        int red=bike<0 ? 25+y/2 : bike==0 ? y/3 : 140+y/3;
        int green=bike<0 ? 225-y/2 : bike==0 ? 235-y/3 : 95-y/4;
        // Concentric capsules give the tube a bright core and a fading halo.
        // Round caps also join consecutive horizontal/vertical strokes smoothly.
        for (int radius=7;radius>=1;--radius) {
            int strength=brightness*(radius>3 ? (8-radius)*32 : 255)/255;
            uint16_t color=tfts->dimColor(c565((radius==1?(red+255)/2:red)*strength/255,(radius==1?(green+255)/2:green)*strength/255,strength));
            if (dx==0) sprite.fillRect(x-radius,y<headY?y:headY,2*radius+1,abs(headY-y)+1,color);
            else sprite.fillRect(x<headX?x:headX,y-radius,abs(headX-x)+1,2*radius+1,color);
            sprite.fillCircle(x,y,radius,color);
            sprite.fillCircle(headX,headY,radius,color);
        }
        remaining-=amount;
    }
}
}

void MatrixClock::renderTron(IPSClock &clock, TimeSync &time, WeatherService &weather) {
    unsigned long ms=millis();
    static int8_t shown[6]={-1,-1,-1,-1,-1,-1}, target[6]={-1,-1,-1,-1,-1,-1};
    static unsigned long lastVisit=0, start=0, lastWeatherMs=0;
    static uint8_t changed=0, lastBri=0, lastHalfSec=255, dirty=0x3F;
    static int8_t prevSlot=-1;

    if (ms-lastFrame < (changed ? 50UL : 150UL)) return;
    lastFrame=ms;

    struct tm now; suseconds_t subsecond;
    time.getLocalTime(&now,&subsecond);
    int hour=now.tm_hour;
    if (IPSClock::getHourFormat()) { hour%=12; if (!hour) hour=12; }
    int current[6]={hour/10,hour%10,-1,now.tm_min/10,now.tm_min%10,-1};
    uint8_t brightness=clock.getBrightness();

    if (shown[0]<0 || ms-lastVisit>2000 || brightness!=lastBri) {
        for (int i=0;i<6;++i) shown[i]=target[i]=current[i];
        lastBri=brightness;
        changed=0;
        prevSlot=-1;
        dirty=0x3F;
    }
    lastVisit=ms;

    if (!changed) {
        for (int i=0;i<6;++i) {
            target[i]=current[i];
            if (shown[i]!=current[i]) changed|=1<<i;
        }
        if (changed) {
            start=ms;
            dirty=0x3F;
        }
    }

    int slot=-1, local=0;
    if (changed) {
        unsigned long elapsed=ms-start;
        for (int i=0;i<6;++i) {
            int duration=(changed&(1<<i)) ? 4000 : 1000;
            if (elapsed<(unsigned)duration) { slot=i;local=elapsed;break; }
            elapsed-=duration;
            if (shown[i]!=target[i]) { shown[i]=target[i]; dirty|=1<<i; }
        }
        if (slot<0) {
            changed=0;
            dirty=0x3F;
        }
    }

    if (prevSlot>=0 && prevSlot!=slot) dirty|=1<<prevSlot;
    prevSlot=slot;

    uint8_t halfSec=(ms/500)&1;
    if (halfSec!=lastHalfSec) {
        lastHalfSec=halfSec;
        dirty|=1<<2;
    }
    if (ms-lastWeatherMs>5000) {
        lastWeatherMs=ms;
        dirty|=1<<5;
    }

    tfts->claim();tfts->setDimming(brightness);tfts->setBox(TFT_WIDTH,TFT_HEIGHT);
    tfts->setImageJustification(TFTs::MIDDLE_CENTER);tfts->enableAllDisplays();
    TFT_eSprite &sprite=tfts->getSprite();
    uint16_t cyan=tfts->dimColor(c565(10,225,255));
    uint16_t violet=tfts->dimColor(c565(170,65,255));

    for (int i=0;i<6;++i) {
        if (!(dirty&(1<<i)) && slot!=i) continue;
        dirty&=~(1<<i);

        for (int y=0;y<TFT_HEIGHT;++y) {
            // Softened 2D grid for higher neon contrast
            uint16_t grid=tfts->dimColor(c565(4+y/9, 26-y/11, 56));
            uint16_t background=tfts->dimColor(c565(1+y/45, 1, 5+y/30));
            sprite.drawFastHLine(0,y,TFT_WIDTH,y%20?background:grid);
            if (y%20) for (int x=0;x<TFT_WIDTH;x+=20) sprite.drawPixel(x,y,grid);
        }

        for (int b=0;b<2;++b) drawLightRibbon(sprite, (!changed || slot>i) ? TFT_WIDTH : 0, b);

        int x=0,y=0,dir=0,sx=0;
        if (current[i]>=0) {
            // Keep the previous digit legible until its replacement is traced.
            trace(sprite,shown[i],1000,false,-1,changed&(1<<i) && slot<=i ? 45 : 255,x,y,dir,sx);
        } else {
            sprite.setTextFont(4);sprite.setTextSize(1);sprite.setTextDatum(MC_DATUM);
            sprite.setTextColor(cyan);
            if (i==2) {
                uint16_t pulse=halfSec?violet:cyan;
                sprite.fillCircle(67,76,4,pulse);sprite.fillCircle(67,120,4,pulse);
                char date[6];date[0]='0'+now.tm_mday/10;date[1]='0'+now.tm_mday%10;date[2]='/';date[3]='0'+(now.tm_mon+1)/10;date[4]='0'+(now.tm_mon+1)%10;date[5]=0;
                sprite.drawString(date,67,187,4);
            } else {
                String icon=weather.getIconName(5);
                int code=icon.toInt();
                int weatherIcon=code==1?(icon.endsWith("n")?1:0):code==2?2:code==9?4:code==10?5:code==11?6:code==13?7:code==50?8:3;
                char text[24]="/ips/tron/w0.bmp";text[11]='0'+weatherIcon;
                tfts->overlayImageAt(text,25,22);
                float temp=weather.getNowTemp(),high=weather.getHigh(5),low=weather.getLow(5);
                if (isnan(temp)) strcpy(text,"--C");else snprintf(text,sizeof(text),"%dC",(int)roundf(temp));
                sprite.setTextSize(2);sprite.drawString(text,67,128,4);
                if (!isnan(high)&&!isnan(low)) {
                    sprite.setTextSize(1);sprite.setTextColor(violet);
                    snprintf(text,sizeof(text),"MAX %dC",(int)roundf(high));sprite.drawString(text,67,174,4);
                    snprintf(text,sizeof(text),"MIN %dC",(int)roundf(low));sprite.drawString(text,67,194,4);
                }
            }
        }
        if (slot==i) for (int bike=0;bike<2;++bike) {
            int laneY=208+bike*12,ribbonX;
            if (changed&(1<<i)) {
                // Prefix and suffix meet to form the new digit with both trails.
                int progress=local<400?0:local>=3400?500:(local-400)*500/3000;
                trace(sprite,target[i],progress,bike!=0,bike,255,x,y,dir,sx);
                if (local<400) {
                    x=-20-bike*12+(x+20+bike*12)*local/400;
                    y=laneY+(y-laneY)*local/400;
                    dir=0;
                } else if (local>=3400) {
                    int exit=local-3400;
                    if (exit<300) { y+=(laneY-y)*exit/300;dir=1; }
                    else { x+=(140-bike*12-x)*(exit-300)/300;y=laneY;dir=0; }
                }
                ribbonX = (local>=400 && local<3400) ? sx : x;
            } else {
                ribbonX = x = -20+local*160/1000-bike*12;
                y = laneY;
                dir = 0;
            }
            drawLightRibbon(sprite, ribbonX, bike);
            char path[24];snprintf(path,sizeof(path),"/ips/tron/%d.bmp",bike*4+dir);
            tfts->overlayImageAt(path,x-(dir%2?10:20),y-(dir%2?20:10));
        }
        tfts->chip_select.setDigit(screens[i]);sprite.pushSprite(0,0);
    }
    sprite.setTextSize(1);tfts->release();
}
