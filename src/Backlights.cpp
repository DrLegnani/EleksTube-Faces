#include "Backlights.h"
#include "IPSClock.h"
#include <math.h>

boolean Backlights::backlightState = false;
byte Backlights::backlightHue = 128;
byte Backlights::backlightSaturation = 255;
byte Backlights::backlightBrightness = 255;
std::atomic<uint8_t> Backlights::photoIndex[NUM_DIGITS];
extern StringConfigItem *slidesSet;
#if (NUM_LEDS > 6)
byte Backlights::underlightHue = 128;
byte Backlights::underlightSaturation = 128;
byte Backlights::underlightBrightness = 128;
#endif

void Backlights::begin()  {
  pixels.Begin(); // This initializes the NeoPixel library.
  pixels.Show();
}

void Backlights::loop() {
  //   enum patterns { dark, constant, rainbow, pulse, breath, num_patterns };
  uint8_t current_pattern = getLEDPattern();

  if (backlightState) {
#if (NUM_LEDS > 6)
    fill(backlightHue, backlightSaturation, backlightBrightness, 0, 6);
    fill(underlightHue, underlightSaturation, underlightBrightness, 6, NUM_LEDS);
#else
    fill(backlightHue, backlightSaturation, backlightBrightness, 0, NUM_LEDS);
#endif
    show();
  } else if (off || current_pattern == dark) {
    clear();
    show();
  }
  else if (IPSClock::getTimeOrDate().value == IPSClock::MARIO) {
    extern uint8_t marioActiveScreen;
    extern uint8_t marioHitFlash;
    extern uint8_t marioTimeOfDay; // 0=Day, 1=Dusk, 2=Night
    uint8_t val = (uint32_t)getLEDValue().value * brightness * 80 / (255 * 100);
    if (marioHitFlash) {
      fill(38, 255, val, 0, NUM_LEDS); // Brilliant Coin flash
    } else if (marioActiveScreen < 6) {
      uint8_t bgHue = marioTimeOfDay == 2 ? 165 : marioTimeOfDay == 1 ? 22 : 142;
      for (uint8_t d = 0; d < NUM_LEDS; ++d) {
        if (d == marioActiveScreen || (d >= 6 && (d - 6) / 5 == marioActiveScreen)) {
          setPixelColor(d, 4, 250, val); // Mario red tracker
        } else {
          setPixelColor(d, bgHue, 180, val * 30 / 100);
        }
      }
    } else {
      // Idle ambient matching time of day
      if (marioTimeOfDay == 2) {
        // Night: deep indigo / purple
        fill(168, 220, val * 50 / 100, 0, NUM_LEDS);
      } else if (marioTimeOfDay == 1) {
        // Sunset: warm amber underlights, orange tubes
        fill(18, 240, val * 65 / 100, 0, 6);
        fill(215, 190, val * 50 / 100, 6, NUM_LEDS);
      } else {
        // Day: Yoshi green underlights, sky blue tubes
        fill(142, 160, val * 65 / 100, 0, 6);
        fill(85, 230, val * 55 / 100, 6, NUM_LEDS);
      }
    }
    show();
  }
  else if (IPSClock::getTimeOrDate().value == IPSClock::VADER ||
           IPSClock::getTimeOrDate().value == IPSClock::DEATH_STAR ||
           IPSClock::getTimeOrDate().value == IPSClock::HYPERSPACE ||
           IPSClock::getTimeOrDate().value == IPSClock::VICE_CITY ||
           IPSClock::getTimeOrDate().value == IPSClock::DELOREAN ||
           IPSClock::getTimeOrDate().value == IPSClock::NIXIE_STRANGER ||
           IPSClock::getTimeOrDate().value == IPSClock::CYBERPUNK ||
           IPSClock::getTimeOrDate().value == IPSClock::DOOM ||
           IPSClock::getTimeOrDate().value == IPSClock::PORTAL ||
           IPSClock::getTimeOrDate().value == IPSClock::MONTY_PYTHON ||
           IPSClock::getTimeOrDate().value == IPSClock::GHOSTBUSTERS) {
    extern uint8_t vaderSaberState, deathStarState, hyperspaceJumpState, viceCityState;
    extern uint8_t deloreanState, nixieStrangerState, cyberpunkState;
    extern uint8_t doomState, portalState, montyPythonState, ghostbustersState;
    int mode = IPSClock::getTimeOrDate().value;
    uint8_t val = (uint32_t)getLEDValue().value * brightness * 95 / (255 * 100);
    unsigned long nowMs = millis();
    auto triWave = [nowMs](uint8_t div, uint8_t base, uint8_t span) -> uint8_t {
      uint8_t tri = (nowMs / div) & 0xFF;
      if (tri > 127) tri = 255 - tri;
      return base + ((tri * span) >> 7);
    };

    if (mode == IPSClock::DOOM) {
      if (doomState == 2) {
        // BFG 9000 Green Plasma Ball wave across tubes
        uint8_t front = (nowMs / 55) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool hit = (tube == front);
          setPixelColor(d, 92, hit ? 20 : 250, hit ? val : (val >> 2));
        }
      } else if (doomState == 1) {
        // Berserk Crimson Strobe
        bool red = ((nowMs / 80) & 1) != 0;
        fill(0, 255, red ? val : (val >> 2), 0, NUM_LEDS);
      } else {
        // E1M1 Toxic Green (85) and Rust Bronze (24)
        uint8_t pulse = triWave(14, 110, 140);
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t hue = (tube == 2 || tube == 3) ? 0 : 85;
          setPixelColor(d, hue, 235, (uint16_t)val * pulse / 255);
        }
      }
    } else if (mode == IPSClock::PORTAL) {
      if (portalState == 1) {
        // GLaDOS Red Warning Strobe
        bool strobe = ((nowMs / 95) & 1) != 0;
        fill(0, 250, strobe ? val : (val >> 2), 0, NUM_LEDS);
      } else if (portalState == 2) {
        // High Energy Pellet wave bouncing Blue ➔ Orange
        uint8_t pos = (nowMs / 60) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool hit = (tube == pos);
          setPixelColor(d, (tube < 3) ? 148 : 24, hit ? 30 : 240, hit ? val : (val >> 1));
        }
      } else {
        // Hours: Blue Portal (148), Center: Turret Red (0), Minutes: Orange Portal (24)
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t hue = (tube < 2) ? 148 : ((tube == 2) ? 0 : 24);
          setPixelColor(d, hue, 240, (val * 4) / 5);
        }
      }
    } else if (mode == IPSClock::MONTY_PYTHON) {
      if (montyPythonState == 2) {
        // Giant Foot Splat! Flesh pink (240) impact then white squish
        bool squish = ((nowMs / 100) & 1) != 0;
        fill(squish ? 0 : 240, squish ? 0 : 180, val, 0, NUM_LEDS);
      } else if (montyPythonState == 1) {
        // Black Knight blood spurts!
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          bool spurt = (((nowMs / 70 + d * 5) % 9) == 0);
          setPixelColor(d, 0, 250, spurt ? val : (val >> 3));
        }
      } else {
        // Victorian Circus: Royal Red (0), Gold (35), Royal Blue (160)
        uint8_t step = (nowMs / 600) % 3;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t colorIdx = (tube + step) % 3;
          uint8_t hue = (colorIdx == 0) ? 0 : ((colorIdx == 1) ? 35 : 160);
          setPixelColor(d, hue, 235, (val * 3) >> 2);
        }
      }
    } else if (mode == IPSClock::GHOSTBUSTERS) {
      if (ghostbustersState == 2) {
        // Don't cross the streams! Chaotic proton wand plasma (Orange/Yellow/White) + Trap Cyan cone
        bool flash = ((nowMs / 50) & 1) != 0;
        uint8_t sparkTube = (nowMs / 80) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          if (tube == 2 || tube == 3) {
            // Trap center: bright electric cyan/white
            setPixelColor(d, 135, flash ? 40 : 255, val);
          } else if (tube == sparkTube) {
            // Proton wand arc spark
            setPixelColor(d, 28, 255, val);
          } else {
            // Hot plasma stream (Red/Orange)
            setPixelColor(d, 8, 240, (val * 2) / 3);
          }
        }
      } else if (ghostbustersState == 1) {
        // PKE Meter Critical & Ecto-1 siren strobe (Alternating Red & Blue)
        bool sirenPhase = ((nowMs / 120) & 1) != 0;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool isLeft = (tube < 3);
          bool active = (isLeft == sirenPhase);
          uint8_t hue = isLeft ? 0 : 160; // Red vs Blue
          setPixelColor(d, hue, 255, active ? val : (val >> 3));
        }
      } else {
        // Normal: Ecto-1 emergency pulse (Red ends, Proton Orange, Ecto-Slime Green center)
        uint8_t pulse = triWave(16, 90, 160);
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          if (tube == 0) {
            setPixelColor(d, 0, 240, (val * pulse) >> 8); // Red Ecto siren
          } else if (tube == 5) {
            setPixelColor(d, 160, 240, (val * (255 - pulse)) >> 8); // Blue Ecto siren
          } else if (tube == 2 || tube == 3) {
            setPixelColor(d, 90, 255, (val * 3) >> 2); // Ectoplasm Slimer green
          } else {
            setPixelColor(d, 22, 230, (val * 2) / 3); // Proton pack orange
          }
        }
      }
    } else if (mode == IPSClock::DELOREAN) {
      if (deloreanState == 1) {
        // 88 MPH 1.21 GW Temporal Lightning flashes (Electric Blue & White)
        bool flash = ((nowMs / 65) & 1) != 0;
        fill(145, flash ? 40 : 230, flash ? val : (val >> 1), 0, NUM_LEDS);
      } else if (deloreanState == 2) {
        // Flaming Tire Tracks sweeping across tubes (Fire Orange/Red)
        uint8_t front = (nowMs / 55) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool hot = (tube <= front);
          setPixelColor(d, hot ? 14 : 145, 240, hot ? val : (val >> 1));
        }
      } else {
        // Time Circuits: Hours Red (0), Flux Center Amber (28), Minutes Green (85)
        uint8_t pulse = triWave(10, 150, 100);
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t hue = (tube < 2) ? 4 : (tube == 2 ? 28 : (tube < 5 ? 85 : 145));
          setPixelColor(d, hue, 240, (uint16_t)val * pulse / 255);
        }
      }
    } else if (mode == IPSClock::NIXIE_STRANGER) {
      if (nixieStrangerState >= 1) {
        // Upside Down Mind Flayer Crimson Storm & Joyce Christmas-light flicker
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          bool strike = (((nowMs / 80 + d * 3) % 7) == 0) || (nixieStrangerState == 2);
          setPixelColor(d, strike ? 0 : 165, strike ? 250 : 220, strike ? val : (val >> 2));
        }
      } else {
        // Warm Vintage IN-18 Nixie Filament Glow (Amber-Orange hue 16)
        uint8_t warm = triWave(16, 165, 80);
        fill(16, 245, (uint16_t)val * warm / 255, 0, NUM_LEDS);
      }
    } else if (mode == IPSClock::CYBERPUNK) {
      if (cyberpunkState == 1) {
        // Sandevistan Emerald-Cyan Overdrive
        uint8_t wave = (nowMs / 50) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          setPixelColor(d, (tube == wave) ? 115 : 132, 240, val);
        }
      } else if (cyberpunkState == 2) {
        // Relic Malfunction Glitch (Crimson Red 252 & Cyan 132 strobe)
        bool err = ((nowMs / 75) & 1) != 0;
        fill(err ? 252 : 132, 250, val, 0, NUM_LEDS);
      } else {
        // Night City Cyber Yellow (40) + Kiroshi Cyan (132) + Samurai Red (252)
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t hue = (tube == 2) ? 252 : ((tube == 5) ? 132 : 40);
          setPixelColor(d, hue, 240, (val * 3) >> 2);
        }
      }
    } else if (mode == IPSClock::VICE_CITY) {
      if (viceCityState == 1) {
        // 5-Star Police Siren Strobe (Red / Blue alternating across left & right tubes)
        bool swap = ((nowMs / 110) & 1) != 0;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool redSide = ((tube < 3) ^ swap);
          setPixelColor(d, redSide ? 0 : 165, 250, val);
        }
      } else if (viceCityState == 2) {
        // Nitro Overdrive wave (Hot Pink & Electric Cyan racing across tubes)
        uint8_t front = (nowMs / 60) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool hit = (tube == front);
          setPixelColor(d, (tube & 1) ? 132 : 232, hit ? 60 : 240, hit ? val : (val >> 1));
        }
      } else {
        // Vice City / GTA VI Sunset Neon (Hot Pink 232, Sunset Gold 20, Miami Cyan 132)
        uint8_t pulse = triWave(12, 130, 120);
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t hue = (tube == 2 || tube == 3) ? 20 : ((tube & 1) ? 132 : 232);
          setPixelColor(d, hue, 235, (uint16_t)val * pulse / 255);
        }
      }
    } else if (mode == IPSClock::VADER) {
      if (vaderSaberState == 2) {
        uint8_t front = (nowMs / 70) % 6;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool hit = (tube == front || (front > 0 && tube == front - 1));
          setPixelColor(d, 0, hit ? 70 : 245, hit ? val : (val >> 1));
        }
      } else {
        uint8_t lvl = (vaderSaberState == 1) ? triWave(6, 80, 170) : triWave(14, 45, 195);
        fill(0, 245, (uint16_t)val * lvl / 255, 0, NUM_LEDS);
      }
    } else if (mode == IPSClock::DEATH_STAR) {
      if (deathStarState == 2) {
        uint8_t beamTube = 2 + ((nowMs / 55) % 4);
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool onBeam = (tube >= 2 && tube == beamTube);
          setPixelColor(d, 92, onBeam ? 30 : 210, onBeam ? val : (val >> 1));
        }
      } else if (deathStarState == 1) {
        fill(92, 235, (uint16_t)val * triWave(5, 110, 140) / 255, 0, NUM_LEDS);
      } else {
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool core = (tube == 2 || tube == 3);
          setPixelColor(d, core ? 0 : 152, core ? 240 : 140, core ? (val >> 1) : (val >> 2));
        }
      }
    } else {
      if (hyperspaceJumpState == 2) {
        uint8_t phase = (nowMs / 45) % 3;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          uint8_t dist = (tube < 3) ? (2 - tube) : (tube - 3);
          bool burst = (dist == phase);
          setPixelColor(d, 142, burst ? 20 : 120, burst ? val : (val >> 1));
        }
      } else if (hyperspaceJumpState == 1) {
        uint8_t cVal = (uint16_t)val * (130 + ((nowMs / 8) % 120)) / 255;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool center = (tube == 2 || tube == 3);
          setPixelColor(d, center ? 140 : 155, center ? 90 : 210, center ? val : (cVal >> 1));
        }
      } else {
        uint8_t cVal = (uint16_t)val * triWave(12, 140, 90) / 255;
        for (uint8_t d = 0; d < NUM_LEDS; ++d) {
          uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
          bool center = (tube == 2 || tube == 3);
          setPixelColor(d, center ? 142 : 156, center ? 150 : 235, center ? cVal : (val >> 2));
        }
      }
    }
    show();
  }
  else if (IPSClock::getTimeOrDate().value == IPSClock::TIME && IPSClock::getClockFace().value == "matrix_anim") {
    extern uint8_t matrixMinutePulse;
    uint8_t val = (uint32_t)getLEDValue().value * brightness * 80 / (255 * 100);
    uint8_t hue = 85;

    if (matrixMinutePulse) {
      uint8_t wave = (millis() / 90) % 8;
      for (uint8_t d = 0; d < NUM_LEDS; ++d) {
        uint8_t tube = (d < 6) ? d : ((d - 6) / 5);
        bool lit = (tube == wave || tube == wave - 1);
        setPixelColor(d, hue, lit ? 40 : 240, lit ? val : (val >> 2));
      }
    } else {
      fill(hue, 230, (val * 7) >> 3, 0, 6);
      unsigned long nowMs = millis();
      for (uint8_t tube = 0; tube < 6; ++tube) {
        int dropPhase = (nowMs / 120 + tube * 7) % 7;
        for (uint8_t led = 0; led < 5; ++led) {
          uint8_t idx = 6 + tube * 5 + led;
          if (idx >= NUM_LEDS) break;
          bool head = (led == dropPhase);
          setPixelColor(idx, hue, head ? 50 : 240, head ? val : (val >> 2));
        }
      }
    }
    show();
  }
  else if (IPSClock::getTimeOrDate().value == IPSClock::TIME ||
           IPSClock::getTimeOrDate().value == IPSClock::HYPERSPACE ||
           IPSClock::getTimeOrDate().value == IPSClock::VADER ||
           IPSClock::getTimeOrDate().value == IPSClock::DEATH_STAR ||
           IPSClock::getTimeOrDate().value == IPSClock::TATOOINE) {
    static uint8_t hues[NUM_LEDS] = {};
    static bool first = true;
    static unsigned long lastStep = 0;
    int mode = IPSClock::getTimeOrDate().value;
    bool step = millis() - lastStep >= 70;
    if (step) lastStep = millis();
    uint8_t value = (uint32_t)getLEDValue().value * brightness * 70 / (255 * 100);
    for (uint8_t digit = 0; digit < NUM_LEDS; ++digit) {
      uint8_t base = mode == IPSClock::HYPERSPACE ? 148 : mode == IPSClock::VADER ? 0 : mode == IPSClock::DEATH_STAR ? 92 : mode == IPSClock::MARIO ? 12 : mode == IPSClock::TATOOINE ? (digit < 6 ? 18 : (digit % 3 == 0 ? 215 : 15)) : 85;
      uint8_t target = base + ((millis() / 2500 + digit * 2) % 9) - 4;
      if (first) hues[digit] = target;
      int8_t difference = target - hues[digit];
      if (step && difference) hues[digit] += difference > 0 ? 1 : -1;
      setPixelColor(digit, hues[digit], 200, value);
    }
    first = false;
    show();
  }
  else if (current_pattern == constant) {
    uint16_t val = getLEDValue();

    val = val * brightness / 255;
    
    fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);
    show();
  }
  else if (current_pattern == rainbow) {
    rainbowPattern();
  }
  else if (current_pattern == pulse) {
    pulsePattern();
  }
  else if (current_pattern == breath) {
    breathPattern();
  }
    else if (current_pattern == aurora) {
    auroraPattern();
  }
}

void Backlights::pulsePattern() {
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float valAdjust = 1 + abs(sin(2 * M_PI * millis() / pulse_length_millis)) * 254;
  uint16_t val = valAdjust * getLEDValue().value / 256;
  val = val * brightness / 255;

  fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);

  show();
}

void Backlights::breathPattern() {
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;
  val = val * getLEDValue().value / 256;
  val = val * brightness / 255;

  fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);

  show();
}

void Backlights::rainbowPattern() {
  const uint16_t hue_per_digit = (256/NUM_DIGITS)/2;

  uint16_t hue = millis()/((21-getBreathPerMin().value) * 10) % 256;  
  
  uint16_t val = getLEDValue();

  val = val * brightness / 255;

  for (uint8_t digit=0; digit < NUM_LEDS; digit++) {
    // Shift the hue for this LED.
    uint16_t digitHue = (hue + digit*hue_per_digit) % 256;
 		setPixelColor(digit, digitHue, getLEDSaturation(), val);
  }
  show();
}

void Backlights::auroraPattern()
{
  const uint16_t hue_per_led = getHuePerLed().value; //(256/NUM_LEDS)/2;

  uint16_t hue = millis()/((21-getBreathPerMin().value) * 10) % 256;  
  
  uint16_t val = getLEDValue();

  val = val * brightness / 255;

  for (uint8_t digit=0; digit < NUM_LEDS; digit++) {
    // Shift the hue for this LED.
    uint16_t digitHue = (hue + digit*hue_per_led) % 256;
 		setPixelColor(digit, digitHue, getLEDSaturation(), val);
  }
  show();
}

void Backlights::fill(byte hue, byte sat, byte val, byte start, byte end) {
	RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);

  for (uint8_t digit=start; digit < end; digit++) {
		pixels.SetPixelColor(digit, colorGamma.Correct(color));
  }
}

void Backlights::clear() {
  fill(0, 0, 0, 0, NUM_LEDS);
}

void Backlights::show() {
  pixels.Show();
}

void Backlights::setPixelColor(uint8_t digit, uint8_t hue, uint8_t sat, uint8_t val) {
    RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);
    pixels.SetPixelColor(digit, colorGamma.Correct(color));
}

const String Backlights::patterns_str[Backlights::num_patterns] = 
  { "Dark", "Constant", "Rainbow", "Pulse", "Breath", "Aurora" };
