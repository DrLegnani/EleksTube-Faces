# 🕒 EleksTube IPS - Custom Animated Clock Faces

A feature-packed custom firmware for **EleksTube IPS** 6-tube pseudo-Nixie clocks (ESP32 + 6x ST7789 IPS displays).

This project enhances the firmware with **10 custom, high-detail vector-rendered clock faces** inspired by iconic movies, gaming, and retro technology, complete with ambient RGB backlight sync, dynamic live weather HUD, and minute-turn animations.

---

## ✨ Included Clock Faces

1. **Matrix Digital Rain**
   - Cascading green phosphor digital rain with glyphs clipped into the numeric digits.
   - Cyberpunk system date display (`// SYS_OP //`) and weather HUD.

2. **Super Mario Bros / SMW**
   - Retro 16-bit animated Mario world with coin blocks, Goombas, and retro brick typography.

3. **Tatooine Binary Sunset (Star Wars)**
   - Twin glowing suns on the desert horizon with Sandcrawler and moisture vaporators.

4. **Millennium Falcon Hyperspace (Star Wars)**
   - Cockpit view with dynamic hyperspace jump sequences at the minute turn.

5. **Darth Vader - Rogue One Corridor (Star Wars)**
   - The iconic dark hallway scene: Imperial bulkhead lights, volumetric crimson smoke, reflective grating, klaxon sirens, and Vader's half-body silhouette holding an ignited crimson lightsaber.
   - Blaster bolt deflections at seconds `57–59` and cross-tube double plasma slash at `:00`.

6. **Death Star Orbital Laser (Star Wars)**
   - Detailed Death Star surface trench with superlaser convergence and firing animations.

7. **GTA Vice City / GTA VI**
   - Ocean Drive tropical synthwave sunset gradient, swaying palm trees, 80s supercar front profile with neon underglow, classic GTA wanted stars, health/armor meters, and *"FLASH FM"* animated equalizer.

8. **Back to the Future - DeLorean Time Circuits**
   - Brushed stainless-steel DMC-12 chassis with embossed Dymo labels (*Destination / Present Time*).
   - Centered animated **Y-Flux Capacitor** with converging energy pulses and a digital **88.0 MPH** speedometer that triggers 1.21 GW lightning and twin flaming tire tracks at the minute turn.

9. **Vintage IN-18 Nixie Tubes & Stranger Things**
   - Hyper-realistic Russian IN-18 vacuum tube glass envelopes, honeycomb anode meshes, and warm glowing wire cathodes with unlit cathode stack depth.
   - Flips into **The Upside Down** at seconds `55–02` with Creel clock pendulum swings, red Mind Flayer lightning, creeping vines, and floating spores.

10. **Cyberpunk 2077 / Night City**
    - Kiroshi Optics HUD layout in signature Cyberpunk Yellow (`#FCEE09`) and Electric Cyan (`#00F0FF`).
    - Real-time RGB chromatic aberration glitch on the digits, SAMURAI Cyber-Oni mask, Sandevistan emerald trails, and Relic malfunction glitch tears at the turn of every minute.

---

## 🛠️ Hardware & Tech Stack

- **Platform:** ESP32 240MHz (8MB Flash)
- **Displays:** 6x ST7789 IPS LCD screens (135x240 resolution) via SPI
- **LEDs:** 34x Addressable NeoPixels synchronized to each theme's animation state
- **Framework:** Arduino / PlatformIO
- **Networking:** Asynchronous WebServer, NTP Time Sync, Open-Meteo Weather API, mDNS (`ipstube.local`)

---

## 🚀 Building & Flashing

Compiled via [PlatformIO](https://platformio.org/):

```bash
pio run -e ipstube
pio run -e ipstube -t upload
```

Cycles through clock faces with a single click of the physical **Mode** button.
