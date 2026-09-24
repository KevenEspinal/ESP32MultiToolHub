#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "background.h"
#include "theme.h"

extern TFT_eSPI tft;

class menu {
  private:
  TFT_eSprite* canvas;

  int canvasW = 200;
  int canvasH = 240;
  int canvasX = 7;
  int canvasY = 0;

  int currentSelection = 0;
  std::vector<String> appNames;

  float scrollOffset = 0.0;
  int targetOffset = 0;
  int itemSpacing = 36;

  int center = canvasH / 2;

  // Layout for the icon + label row. Kept as named constants (rather
  // than literals scattered through renderMenu) so the row is easy to
  // re-balance later.
  int iconX = 20;
  int textX = 40;

  // Maps a menu entry to one of the shared glyph icons from theme.h.
  // Falls back to a plain dot for any app name not in the known list,
  // so a future app added to the roster still renders something sane.
  void drawAppIcon(const String &appName, int x, int y, int r, uint16_t color) {
    if (appName == "Clock") {
      uiIconClock(canvas, x, y, r, color);
    } else if (appName == "Links") {
      uiIconLink(canvas, x, y, r, color);
    } else if (appName == "Playlists") {
      uiIconPlaylist(canvas, x, y, (int)(r * 1.7f), (int)(r * 1.3f), color);
    } else if (appName == "Diagnostics") {
      uiIconPulse(canvas, x, y, (int)(r * 1.9f), (int)(r * 1.2f), color);
    } else if (appName == "Weather") {
      uiIconSun(canvas, x, y, r, color);
    } else {
      canvas->fillCircle(x, y, (int)(r * 0.35f), color);
    }
  }

  public:
  menu(){
    canvas = new TFT_eSprite(&tft);
  }

  ~menu() {
    if (canvas != nullptr) {
      canvas->deleteSprite();
      delete canvas;
      canvas = nullptr;
    }
  }

  void setup(std::vector<String> appsNames) {
    appNames = appsNames;
    currentSelection = 0;

    canvas->createSprite(canvasW, canvasH);
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextWrap(false);

    targetOffset = currentSelection * itemSpacing;
    scrollOffset = targetOffset;

    renderMenu();
  }

  void scroll(int button) {
    if(button == 2) {
      if(currentSelection > 0) {
        currentSelection--;
        targetOffset = currentSelection * itemSpacing;
      }
    }
    else if(button == 3) {
      // Signed compare on purpose: appNames.size() is unsigned, so on an
      // empty list "size() - 1" wraps to a huge number and the selection
      // index runs away instead of staying put.
      if(currentSelection + 1 < (int)appNames.size()) {
        currentSelection++;
        targetOffset = currentSelection * itemSpacing;
      }
    }
  }

  void animate() {
    if (abs(targetOffset - scrollOffset) > 0.5) {
      scrollOffset += (targetOffset - scrollOffset) * 0.2;
      renderMenu();
    } else if (scrollOffset != targetOffset) {
      scrollOffset = targetOffset;
      renderMenu();
    }
  }

  void renderMenu() {
    canvas->setSwapBytes(true);
    canvas->fillScreen(UI_BG);

    for(int i = 0; i < appNames.size(); i++) {

      float yPos = (i * itemSpacing) - scrollOffset + center;

      if(yPos > -30 && yPos < canvasH + 30) {
        int yi = (int)yPos;

        if(i == currentSelection) {
          // Selected row: soft pill behind the text + a left edge
          // indicator, so the current app reads clearly even mid-scroll.
          canvas->fillRoundRect(2, yi - 16, canvasW - 14, 32, UI_RADIUS_LG, UI_ACCENT_GLOW);
          canvas->fillRect(0, yi - 14, 4, 28, UI_ACCENT);

          drawAppIcon(appNames[i], iconX, yi, 11, UI_ACCENT);

          canvas->loadFont(FONT_UI_LG);
          canvas->setTextColor(UI_ACCENT);
          canvas->drawString(appNames[i], textX, yi);
        } else {
          // Unselected rows fade out the further they sit from focus,
          // like items settling away from the center of a picker wheel.
          float t = fabsf((float)(i - currentSelection)) / 3.0f;
          uint16_t fade = uiLerp565(UI_TEXT_MUTED, UI_TEXT_GHOST, t);

          drawAppIcon(appNames[i], iconX, yi, 8, fade);

          canvas->loadFont(FONT_UI_SM);
          canvas->setTextColor(fade);
          canvas->drawString(appNames[i], textX, yi);
        }
      }
    }

    canvas->pushSprite(canvasX, canvasY);
  }

  int appSelection(){
    return currentSelection;
  }
  void clearMenu(){
    canvas->deleteSprite();
  }
};
