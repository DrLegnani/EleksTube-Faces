#include "DigitalRainAnimation.h"

unsigned long DigitalRainAnimation::getMsDelay() {
  return 1000UL / getMatrixSpeed();
}

void DigitalRainAnimation::prepareAnim() {}
void DigitalRainAnimation::lineUpdate(int, int) {}
void DigitalRainAnimation::mutateCharAt(int, int) {}
void DigitalRainAnimation::lineAnimation2(int, int, int) {}
void DigitalRainAnimation::lineAnimation(int) {}
char DigitalRainAnimation::getASCIIChar() { return '0'; }
char DigitalRainAnimation::getAbcASCIIChar() { return '0'; }
int DigitalRainAnimation::getRandomNum(int min, int max) { return min; }
std::string DigitalRainAnimation::getKey(int key_length) { return ""; }
void DigitalRainAnimation::resetKey() {}
void DigitalRainAnimation::setBigText(bool isOn) {}
DigitalRainAnimation::DigitalRainAnimation() {}
void DigitalRainAnimation::init(TFT_eSPI *gfx, bool biggerText, bool alphabetOnly) {}
boolean DigitalRainAnimation::loop() { return false; }
void DigitalRainAnimation::animate(uint8_t brightness) {}
void DigitalRainAnimation::pause() {}
void DigitalRainAnimation::resume() {}
