# 🕒 EleksTube IPS - Custom Animated Clock Faces

A feature-packed custom firmware for **EleksTube IPS** 6-tube pseudo-Nixie clocks (ESP32 + 6x ST7789 IPS displays).

This project enhances the firmware with **14 custom, high-detail vector-rendered clock faces** inspired by iconic movies, gaming, and retro technology, complete with ambient RGB backlight sync, dynamic live weather HUD, and minute-turn animations.

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

11. **DOOM (1994) / UAC Mars Base**
    - Toxic green nuclear slime pools with animated bubbling acid drops and rusty corrugated industrial steel bulkheads.
    - Centered **Doomguy Status Face** (screen 2) with animated darting eyes, tooth-grit grin, bloody bruising, and evil grin during Berserk mode.
    - Classic DOOM red/orange/yellow chisel HUD typography with heavy drop shadows.
    - Authentic DOOM status bar on screen 5: `HEALTH 100%`, `ARMOR 100%`, and `AMMO 200` shell counter.
    - Berserk Pack flashing red siren alarm at seconds `56–59`, followed by a massive **BFG 9000** green plasma shockwave tearing across all 6 screens at `:00–:02` with ambient green tube backlights.

12. **Portal 2 / Aperture Science**
    - Sterile white Aperture laboratory modular wall tiles with dark seams and warning stripes.
    - Gigantic swirling oval portals: glowing **Blue Portal** (screens 0–1) and **Orange Portal** (screens 3–4) with vortex particle accretion disks.
    - Centered **Aperture Sentry Turret** (screen 2) with slender white tripod legs, dual machine gun pods, and a sweeping red tracking laser sight.
    - Chamber Indicator Sign 04 on screen 5 featuring the Companion Cube pink heart, cake warning icon (*"THE CAKE IS A LIE"*), and high-energy hazard indicators.
    - GLaDOS neurotoxin red klaxon strobe at seconds `56–59`, launching a high-velocity **High Energy Pellet** bouncing through the portals across the clock face at `:00–:02`.

13. **Monty Python / Silly Walks & Holy Grail**
    - Ornate Terry Gilliam Victorian woodcut engraving frame with royal purple velvet damask and gilded curlicues.
    - Centered **The Black Knight** (screen 2) in dark chainmail with red boar crest, dripping limbs, flying cartoon blood spurts, and *"TIS BUT A SCRATCH!"*.
    - Animated **Ministry of Silly Walks** (John Cleese) in bowler hat, black coat, and briefcase, performing acrobatic leg kicks across the bottom of the screens.
    - Screen 5 features the **Killer Rabbit of Caerbannog** with sharp fangs and glowing red eyes, guarded by the **Holy Hand Grenade of Antioch** (*"1.. 2.. 5! THREE, SIR!"*).
    - Terry Gilliam's legendary **Giant Cupid Foot** slams down with an enormous comic *"SPLAT!"* squash-and-stretch animation at `:00–:02` accompanied by royal fanfare backlights.

14. **Ghostbusters (Caça-Fantasmas) / Ecto-1 & Proton Pack**
    - Manhattan midnight skyline with 55 Central Park West ("Spook Central" temple roof), glowing skyscraper windows, and green ectoplasmic slime dripping from the top bezels.
    - Centered **"No-Ghost" Logo (Mooglie the Ghost)** on screen 2: cheerful white ghost waving within the iconic red prohibition circle (`🚫`), *"WHO YA GONNA CALL?"* marquee, and date display.
    - Screen 5 features Dr. Egon Spengler's **P.K.E. Meter** with raising animated sensor wings and blinking amber/red LED arrays, plus an animated floating **Slimer (Geleia)** with glowing green aura, open eating mouth, and live weather readout.
    - High-energy **Proton Wand Stream Digits** with fiery outer halos, intense yellow plasma, and electric cyan/white lightning cores.
    - Minute-turn animation at `:56–:02`: Class 5 Vapor emergency alarm with alternating red/blue Ecto-1 siren backlights at `:56–:59`, followed by **"DON'T CROSS THE STREAMS!"** at `:00–:02` where chaotic dual proton streams tear across all screens and the **Ghost Trap** opens with a brilliant cyan containment light beam!

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
