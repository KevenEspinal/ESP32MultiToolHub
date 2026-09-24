#pragma once
#include <TFT_eSPI.h>
#include "apps.h"
#include "theme.h"
#include <time.h> // Swapped to standard time.h for the ESP32 RTC struct

extern TFT_eSPI tft;

class ClockApp: public app {
  private:
    TFT_eSprite* canvas;
    int canvasW = 280;
    int canvasH = 100;
    int canvasX = 160 - (canvasW / 2);
    int canvasY = 120 - (canvasH / 2);
    
    int lastSecond = -1;
    bool spriteCreated = false; // NEW: Memory protection flag

  public:
    ClockApp(String name) : app(name) {
      canvas = new TFT_eSprite(&tft);
    }

    ~ClockApp() {
      if (canvas != nullptr) {
        canvas->deleteSprite();
        delete canvas;
        canvas = nullptr;
      }
    }

    void loadSprites() {
      // 1. MEMORY OPTIMIZATION: Only allocate this RAM the very first time!
      if (!spriteCreated) {
        canvas->createSprite(canvasW, canvasH);
        spriteCreated = true;
      }

      canvas->setSwapBytes(true);
      canvas->fillScreen(UI_BG);

      canvas->setTextDatum(MC_DATUM);

      // 2. HARDWARE TIME FETCH: Talk directly to the ESP32's RTC
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
          char timeBuffer[30];
          strftime(timeBuffer, sizeof(timeBuffer), "%I:%M:%S %p", &timeinfo);

          char dateBuffer[40];
          strftime(dateBuffer, sizeof(dateBuffer), "%A, %B %d, %Y", &timeinfo);

          canvas->setTextColor(UI_ACCENT);
          canvas->loadFont(FONT_UI_LG);
          canvas->drawString(timeBuffer, canvasW / 2, (canvasH / 2) - 18);

          // A short accent rule between time and date - a small "watch
          // face" touch that also separates the two info tiers visually.
          canvas->drawFastHLine((canvasW / 2) - 20, (canvasH / 2) + 2, 40, UI_ACCENT_DIM);

          canvas->setTextColor(UI_TEXT_MUTED);
          canvas->loadFont(FONT_UI_SM);
          canvas->drawString(dateBuffer, canvasW / 2, (canvasH / 2) + 20);
      }

      canvas->pushSprite(canvasX, canvasY);
    }

    void run() {
      struct tm timeinfo;
      // 3. Check the physical RTC to see if the second advanced
      if (getLocalTime(&timeinfo)) {
        if (timeinfo.tm_sec != lastSecond) {
          lastSecond = timeinfo.tm_sec; // Update memory
          loadSprites();                // Redraw screen
        }
      }
    }

    void clearScreen() {
      canvas->deleteSprite();
      spriteCreated = false; 
    }
};
