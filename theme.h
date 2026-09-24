#pragma once
#include <TFT_eSPI.h>
#include <math.h>
#include "smoothFonts.h"

// =====================================================================
//  theme.h  —  shared visual language for the Do-All-Inator UI
// ---------------------------------------------------------------------
//  Every screen in this project pulls its colours, fonts, spacing and
//  small icon glyphs from this one file so the whole device reads as
//  one consistent product instead of nine separately-styled screens.
//  Nothing in here changes app behaviour — it only decides how things
//  are drawn.
// =====================================================================

// ---------------------------------------------------------------------
//  Palette (RGB565). UI_ACCENT is the project's original brand colour
//  (0xFE94) left completely untouched; everything else is a warm
//  neutral chosen to sit next to it without fighting for attention.
// ---------------------------------------------------------------------
#define UI_BG            TFT_BLACK    // canvas background
#define UI_SURFACE       0x1082       // faint raised-panel fill (placeholders, sidebars)
#define UI_BORDER        0xE71B       // warm-white structural outlines (boxes, dividers)

#define UI_ACCENT        0xFE94       // brand accent — selected / active / "live" values
#define UI_ACCENT_DIM    0x6A86       // muted accent — secondary emphasis, dim fills/tracks
#define UI_ACCENT_GLOW   0x51C4       // deep accent surface behind an active tab or pill

#define UI_TEXT          0xEF5B       // primary readable text (warm off-white)
#define UI_TEXT_MUTED    TFT_DARKGREY // secondary text that stays visible alongside others
#define UI_TEXT_GHOST    0x39A5       // de-emphasised text for carousels with one focus item

#define UI_GOOD          0x7E11       // status: connected / online / good
#define UI_BAD           0xD36C       // status: disconnected / offline / needs attention

// ---------------------------------------------------------------------
//  Type. Two families are already baked into smoothFonts.h at zero
//  extra flash cost — BebasNeue carries all UI chrome (labels, menus,
//  body copy), and Limelight is reserved for the one "hero" number per
//  screen (clock digits, temperature, host load %) so it reads as a
//  deliberate signature rather than a generic default.
// ---------------------------------------------------------------------
#define FONT_UI_SM    BebasNeue_Regular21
#define FONT_UI_LG    BebasNeue_Regular25
#define FONT_HERO_SM  Limelight_Regular21
#define FONT_HERO_LG  Limelight_Regular25

// ---------------------------------------------------------------------
//  Geometry
// ---------------------------------------------------------------------
#define UI_RADIUS_SM  4
#define UI_RADIUS_MD  8
#define UI_RADIUS_LG  12

// ---------------------------------------------------------------------
//  uiLerp565 — blend two RGB565 colours (t: 0 = a, 1 = b). Plain
//  integer math so it behaves the same on every TFT_eSPI version.
//  Used to fade carousel items as they move away from focus.
// ---------------------------------------------------------------------
inline uint16_t uiLerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r  = ar + (int)((br - ar) * t);
  int g  = ag + (int)((bg - ag) * t);
  int bl = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// ---------------------------------------------------------------------
//  Small glyph icons drawn from primitives — no image assets, no extra
//  flash. Shared between the main menu and the media player's app
//  switcher so the same shape always means the same app everywhere.
// ---------------------------------------------------------------------
inline void uiIconClock(TFT_eSprite* s, int cx, int cy, int r, uint16_t c) {
  s->drawCircle(cx, cy, r, c);
  s->drawLine(cx, cy, cx, cy - (int)(r * 0.55f), c);
  s->drawLine(cx, cy, cx + (int)(r * 0.42f), cy, c);
}

inline void uiIconLink(TFT_eSprite* s, int cx, int cy, int r, uint16_t c) {
  int off = (int)(r * 0.42f);
  int rr  = (int)(r * 0.62f);
  s->drawCircle(cx - off, cy, rr, c);
  s->drawCircle(cx + off, cy, rr, c);
}

inline void uiIconPlaylist(TFT_eSprite* s, int cx, int cy, int w, int h, uint16_t c) {
  const float bars[4] = {0.42f, 1.0f, 0.6f, 0.8f};
  float bw = w / 8.5f, gap = bw * 1.1f;
  float total = 4 * bw + 3 * gap;
  float x = cx - total / 2;
  for (int i = 0; i < 4; i++) {
    int bh = (int)(h * bars[i]);
    s->fillRoundRect((int)x, cy + h / 2 - bh, (int)bw, bh, (int)(bw / 2), c);
    x += bw + gap;
  }
}

inline void uiIconPulse(TFT_eSprite* s, int cx, int cy, int w, int h, uint16_t c) {
  s->drawLine(cx - w / 2,  cy,         cx - w / 5,  cy,          c);
  s->drawLine(cx - w / 5,  cy,         cx - w / 10, cy - h / 2,  c);
  s->drawLine(cx - w / 10, cy - h / 2, cx,           cy + h / 2, c);
  s->drawLine(cx,          cy + h / 2, cx + w / 10,  cy,         c);
  s->drawLine(cx + w / 10, cy,         cx + w / 2,   cy,         c);
}

inline void uiIconSun(TFT_eSprite* s, int cx, int cy, int r, uint16_t c) {
  s->drawCircle(cx, cy, (int)(r * 0.5f), c);
  for (int ang = 0; ang < 360; ang += 45) {
    float a = ang * 3.14159265f / 180.0f;
    int x1 = cx + (int)(cosf(a) * r * 0.8f),  y1 = cy + (int)(sinf(a) * r * 0.8f);
    int x2 = cx + (int)(cosf(a) * r * 1.15f), y2 = cy + (int)(sinf(a) * r * 1.15f);
    s->drawLine(x1, y1, x2, y2, c);
  }
}

inline void uiIconCloud(TFT_eSprite* s, int cx, int cy, int w, int h, uint16_t c) {
  int r = h / 2;
  int lx = cx - (int)(w * 0.22f), rx = cx + (int)(w * 0.24f);
  s->fillCircle(lx, cy + r / 3, (int)(r * 0.75f), c);
  s->fillCircle(cx, cy - r / 4, r, c);
  s->fillCircle(rx, cy + r / 4, (int)(r * 0.85f), c);
  s->fillRect(lx, cy + r / 3 - r / 2, rx - lx, r, c);
}

inline void uiIconRain(TFT_eSprite* s, int cx, int cy, int w, int h, uint16_t cloudC, uint16_t dropC) {
  uiIconCloud(s, cx, cy - h / 6, w, (int)(h * 0.7f), cloudC);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * (w / 4);
    s->drawLine(x, cy + h / 4, x - 3, cy + h / 2, dropC);
  }
}

// Picks a weather icon purely from the condition text already provided
// by the host payload — no new data, just a rendering choice.
inline void uiIconWeather(TFT_eSprite* s, int cx, int cy, int w, int h, String condition, uint16_t c, uint16_t accent) {
  condition.toLowerCase();
  if (condition.indexOf("rain") != -1 || condition.indexOf("storm") != -1 ||
      condition.indexOf("shower") != -1 || condition.indexOf("drizzle") != -1) {
    uiIconRain(s, cx, cy, w, h, c, accent);
  } else if (condition.indexOf("cloud") != -1 || condition.indexOf("overcast") != -1 ||
             condition.indexOf("fog") != -1 || condition.indexOf("mist") != -1) {
    uiIconCloud(s, cx, cy, w, h, c);
  } else {
    uiIconSun(s, cx, cy, h / 2, accent);
  }
}
