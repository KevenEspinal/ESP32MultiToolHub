#pragma once
#include <TFT_eSPI.h>
#include <ESP32Encoder.h>
#include "theme.h"

extern TFT_eSPI tft;
extern ESP32Encoder encoder;

// =====================================================================
//  volumeHeader.h — the floating volume bar
// ---------------------------------------------------------------------
//  A rounded "capsule" track pinned to the left edge of the screen, with
//  an accent-coloured fill showing the host device's volume. It slides
//  in whenever the volume changes, then slides back out and hands its
//  column back to whatever app is underneath.
//
//  Call order from the sketch:
//      setup()  ->  vol.setupSprite();
//      loop()   ->  if (vol.update(encoder.getCount())) { repaint app }
//
//  NOTE: update() takes the RAW encoder count. The old calculateVolume()
//  expected a pre-divided count (encoder.getCount() / 2); the divide now
//  happens inside, because doing it outside truncated toward zero and
//  made the notch either side of 0 twice as wide as every other notch.
// =====================================================================

// Magenta stands in for "leave this pixel alone" when the bar is pushed,
// so the rounded corners let the app underneath show through instead of
// punching black notches into it. Nothing else in the UI uses this colour.
#define VOL_TRANSPARENT_KEY 0xF81F

class volumeHeader {
private:
  // -------------------------------------------------------------------
  //  Layout. The sprite is exactly the size of the track — that is the
  //  whole trick behind this file. Every pixel the fill can ever occupy
  //  belongs to the sprite, so every pixel gets repainted every frame.
  // -------------------------------------------------------------------
  int volBarW = 14;
  int volBarH = 200;
  int volBarX = 14;
  int volBarY = 20;           // sane default; setupSprite() recentres it
  int trackR  = volBarW / 2;  // corner radius of the curved rectangle

  // -------------------------------------------------------------------
  //  Tunables
  // -------------------------------------------------------------------
  // uint32_t on purpose: millis() is 32 bits on the ESP32, and every
  // comparison below is written as an unsigned subtraction so it keeps
  // working across the ~49 day wraparound.
  static const uint32_t HIDE_AFTER_MS = 1200;  // idle time before it slides away
  static const uint32_t FRAME_MS      = 20;    // animation tick, ~50fps
  static const uint32_t ENDSTOP_MS    = 200;   // length of the 0% / 100% flash
  static const int VOLUME_PER_DETENT = 3;           // volume units per encoder notch
  static const int MAX_HOST_STEPS    = 12;          // cap on messages sent in one frame

  // -------------------------------------------------------------------
  //  State
  // -------------------------------------------------------------------
  int currentVolume = 0;   // 0..100, the value the bar represents
  int currentSize   = 0;   // px of fill currently drawn
  int targetSize    = 0;   // px of fill being animated toward

  bool isVisible = false;  // the bar currently owns its column
  bool isHiding  = false;  // animating down to nothing

  long lastDetent    = 0;
  bool encoderSeeded = false;

  uint32_t lastInteraction = 0;
  uint32_t lastFrameTime   = 0;
  uint32_t endStopStart    = 0;
  bool     endStopActive   = false;

  TFT_eSprite volumeSPR;

  // ESP32Encoder counts two per detent in half-quad mode. A plain "/ 2"
  // truncates toward zero, which makes the notch either side of zero
  // twice as wide as all the others; flooring keeps every detent equal.
  static long toDetents(long count) {
    return (count >= 0) ? (count / 2) : ((count - 1) / 2);
  }

  int volumeToPixels(int volume) const {
    if (volume <= 0)   return 0;
    if (volume >= 100) return volBarH;
    return (volume * volBarH) / 100;
  }

  bool ensureSprite() {
    if (volumeSPR.created()) return true;
    volumeSPR.setColorDepth(16);
    return volumeSPR.createSprite(volBarW, volBarH) != nullptr;
  }

  void wake() {
    isVisible       = true;
    isHiding        = false;
    lastInteraction = (uint32_t)millis();
  }

  uint16_t fillColour() {
    // Brief bright flash on hitting 0% or 100% — the "you have run out of
    // dial" feedback the old overshoot animation was reaching for, but
    // one that cannot draw past the end of the sprite.
    if (endStopActive) {
      if ((uint32_t)millis() - endStopStart < ENDSTOP_MS) return UI_TEXT;
      endStopActive = false;
    }
    return UI_ACCENT;
  }

  void drawBar() {
    if (!ensureSprite()) return;

    volumeSPR.fillSprite(VOL_TRANSPARENT_KEY);

    // The empty track is redrawn on EVERY frame, and this is what lets the
    // bar shrink at all: the pixels the fill used to occupy get painted
    // over by the track. Filling only the fill region and pushing with a
    // transparent key (what this used to do) leaves the taller, older bar
    // sitting on the screen, so turning the volume down did nothing.
    volumeSPR.fillRoundRect(0, 0, volBarW, volBarH, trackR, UI_SURFACE);
    volumeSPR.drawRoundRect(0, 0, volBarW, volBarH, trackR, UI_ACCENT_DIM);

    int h = currentSize;
    if (h > volBarH) h = volBarH;
    if (h > 0) {
      int fillR = trackR;
      if (h < trackR * 2) fillR = h / 2;  // keep the cap sane on a nearly empty bar
      volumeSPR.fillRoundRect(0, volBarH - h, volBarW, h, fillR, fillColour());
    }

    volumeSPR.pushSprite(volBarX, volBarY, VOL_TRANSPARENT_KEY);
  }

  // One opaque frame that actually wipes the column. A transparent push
  // can never erase anything, so hiding has to be done explicitly rather
  // than by just deleting the sprite and hoping the app repaints in time.
  void eraseBar() {
    if (!ensureSprite()) return;
    volumeSPR.fillSprite(UI_BG);
    volumeSPR.pushSprite(volBarX, volBarY);
  }

public:
  volumeHeader() : volumeSPR(&tft) {}

  // Called once from setup(). Safe to call before the encoder is attached.
  void setupSprite() {
    volBarY = (tft.height() - volBarH) / 2;
    if (volBarY < 0) volBarY = 0;

    // Allocated once, up front, while the heap is still clean, and kept.
    // The old version allocated on demand and never checked the result,
    // so an allocation that failed later (after the album-art malloc had
    // fragmented the heap) just made the bar silently stop drawing.
    if (!ensureSprite()) Serial.println("VOL: sprite allocation failed");
  }

  int  getVolume()  const { return currentVolume; }
  bool barVisible() const { return isVisible; }

  // Optional: hand the ~5.6KB back if some future screen needs it.
  void releaseSprite() {
    if (volumeSPR.created()) volumeSPR.deleteSprite();
  }

  // "VOLUME|<0-100>" reported by the host.
  void updateHostVolume(int volume) {
    int clamped = volume;
    if (clamped < 0)   clamped = 0;
    if (clamped > 100) clamped = 100;

    // A host that re-reports the same level once a second must not pin the
    // bar on screen forever, so only a real change wakes it up.
    bool changed  = (clamped != currentVolume);
    currentVolume = clamped;
    targetSize    = volumeToPixels(currentVolume);

    // Deliberately does NOT write back to the encoder. The old version did
    // (setCount((volume / 3) * 2)), and that integer divide rounded the
    // position off, so the next notch either jumped or did nothing.
    if (changed) wake();
  }

  // Call once per loop() with the raw count: vol.update(encoder.getCount()).
  // Returns true for exactly one frame — the moment the bar finishes
  // hiding and gives its column back, which is the app's cue to repaint.
  bool update(long encoderCount) {
    long detents = toDetents(encoderCount);

    // The first call adopts wherever the encoder is sitting, so a stale
    // count left over from boot doesn't fling the bar open.
    if (!encoderSeeded) {
      lastDetent    = detents;
      encoderSeeded = true;
    }

    if (detents != lastDetent) {
      long delta = detents - lastDetent;
      lastDetent = detents;  // always track the real position: no wind-up

      int before = currentVolume;
      int wanted = currentVolume + (int)(delta * VOLUME_PER_DETENT);
      if (wanted < 0)   wanted = 0;
      if (wanted > 100) wanted = 100;
      currentVolume = wanted;

      // One message per notch. The old code printed a single VOL_UP no
      // matter how far the wheel had moved, so a fast spin walked the bar
      // several steps while the host only moved one and the two desynced.
      long steps = (delta > 0) ? delta : -delta;
      if (steps > MAX_HOST_STEPS) steps = MAX_HOST_STEPS;
      for (long i = 0; i < steps; i++) {
        Serial.println(delta > 0 ? "VOL_UP" : "VOL_DOWN");
      }

      // Clamped means we hit an end stop. Guarded so that holding the
      // encoder past 100% flashes once instead of retriggering forever.
      if (currentVolume == before && !endStopActive) {
        endStopActive = true;
        endStopStart  = (uint32_t)millis();
      }

      targetSize = volumeToPixels(currentVolume);
      wake();
    }

    if (!isVisible) return false;

    // Unsigned subtraction, so this still behaves after millis() wraps at
    // ~49 days. The old "millis() > hideTime" compared absolute stamps and
    // broke across the wrap.
    if (!isHiding && (uint32_t)millis() - lastInteraction >= HIDE_AFTER_MS) {
      isHiding   = true;
      targetSize = 0;   // slide out instead of vanishing mid-animation
    }

    if ((uint32_t)millis() - lastFrameTime < FRAME_MS) return false;
    lastFrameTime = (uint32_t)millis();

    if (currentSize != targetSize) {
      // Distance-proportional, so a jump from 0 to 100% settles in ~250ms
      // however big it is. The old fixed 1px-per-5ms crawl took 900ms to
      // cross the bar — longer than the 1s hide timer, so a big change was
      // cut off before it ever finished drawing.
      int diff = targetSize - currentSize;
      int step = diff / 3;
      if (step == 0) step = (diff > 0) ? 1 : -1;  // always finish the last few px
      currentSize += step;
    }

    if (isHiding && currentSize == 0) {
      isVisible     = false;
      isHiding      = false;
      endStopActive = false;
      eraseBar();
      return true;
    }

    // Redrawn every tick while visible, not only while the size is
    // changing. That keeps the bar on top of an app repainting underneath
    // it, and means a level that happens to equal the current one still
    // appears — the old code only ever drew from inside the "size is
    // changing" branch, so an unchanged level drew nothing at all.
    drawBar();
    return false;
  }
};
