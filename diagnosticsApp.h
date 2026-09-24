#pragma once
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "background.h"
#include "theme.h"
#include "apps.h" // Ensure the parent class is available

extern TFT_eSPI tft;

class diagnosticsApp : public app {
  private:
    int currentTab = 0; // 0=CPU, 1=Memory, 2=Disk, 3=Wi-Fi, 4=GPU
    unsigned long previousMillis = 0;
    const unsigned long interval = 1000; // Update stats every 1 second

    String tabs[5] = {"CPU", "MEM", "DISK", "WIFI", "GPU"};
    
    // UI Bounds
    int headerY = 5;
    int headerH = 30;
    int boxX = 5;
    int boxY = 40;
    int boxW = 310;
    int boxH = 195;
    int contentH = 140; // kept at the original footprint on purpose — see notes below

    // NEW: Persistent pointers to replace local generation
    TFT_eSprite* headerSPR;
    TFT_eSprite* contentSPR;

    // ---- shared row / hero-stat layouts, reused by every tab ----
    void renderRow(int index, const String &label, const String &value, uint16_t valueColor) {
      int y = 14 + index * 32;
      contentSPR->loadFont(FONT_UI_SM);
      contentSPR->setTextDatum(ML_DATUM);
      contentSPR->setTextColor(UI_TEXT_MUTED);
      contentSPR->drawString(label, 16, y);

      contentSPR->loadFont(FONT_UI_LG);
      contentSPR->setTextDatum(MR_DATUM);
      contentSPR->setTextColor(valueColor);
      contentSPR->drawString(value, 290, y);
    }

    void renderHeroStat(int value, const String &label) {
      if (millis() - lastHostUpdate >= 3000) {
        contentSPR->loadFont(FONT_UI_SM);
        contentSPR->setTextDatum(MC_DATUM);
        contentSPR->setTextColor(UI_TEXT_MUTED);
        contentSPR->drawString("Waiting for Host...", 153, contentH / 2);
        return;
      }

      int v = constrain(value, 0, 100);

      contentSPR->loadFont(FONT_HERO_LG);
      contentSPR->setTextDatum(MC_DATUM);
      contentSPR->setTextColor(UI_ACCENT);
      contentSPR->drawString(String(v) + "%", 153, 38);

      contentSPR->loadFont(FONT_UI_SM);
      contentSPR->setTextColor(UI_TEXT_MUTED);
      contentSPR->drawString(label, 153, 70);

      int barX = 40, barY = 92, barW = 226, barH = 14;
      contentSPR->fillRoundRect(barX, barY, barW, barH, barH / 2, UI_TEXT_MUTED);
      int fillW = (barW * v) / 100;
      if (fillW > 0) {
        contentSPR->fillRoundRect(barX, barY, fillW, barH, barH / 2, UI_ACCENT);
      }
    }

  public:
    int hostCPU = 0;
    int hostRAM = 0;
    int hostGPU = 0;
    unsigned long lastHostUpdate = 0;

    diagnosticsApp(String name) : app(name) {
      headerSPR = new TFT_eSprite(&tft);
      contentSPR = new TFT_eSprite(&tft);
    }

    ~diagnosticsApp() {
      if (headerSPR != nullptr) {
        headerSPR->deleteSprite();
        delete headerSPR;
        headerSPR = nullptr;
      }
      if (contentSPR != nullptr) {
        contentSPR->deleteSprite();
        delete contentSPR;
        contentSPR = nullptr;
      }
    }

    void updateHostStats(int cpu, int ram, int gpu) {
      hostCPU = cpu;
      hostRAM = ram;
      hostGPU = gpu;
      lastHostUpdate = millis();
    }

    void loadScreen() {
      currentTab = 0;
      
      // Explicitly wipe the root physical screen before drawing our floating sprites
      tft.setSwapBytes(true);
      tft.fillScreen(UI_BG); 

      // Create sprites in memory ONCE per app launch
      headerSPR->createSprite(320, headerH);
      contentSPR->createSprite(boxW - 4, contentH);
      
      render();
    }

    void scroll(int buttonDirection) {
      bool tabChanged = false;

      if (buttonDirection == 0) { // LEFT
        if (currentTab > 0) {
          currentTab--;
          tabChanged = true;
        }
      } 
      else if (buttonDirection == 1) { // RIGHT
        if (currentTab < 4) {
          currentTab++;
          tabChanged = true;
        }
      }

      if (tabChanged) {
        renderHeader();
        renderContent();
      }
    }

    void run() {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        renderContent(); // Only update the data box, not the whole screen
      }
    }

    void render() {
      // Wipe the screen and redraw the main background first
      tft.setSwapBytes(true);
      tft.fillScreen(UI_BG);
      
      // Draw the static box outline directly to the screen
      tft.drawRoundRect(boxX, boxY, boxW, boxH, UI_RADIUS_SM, UI_BORDER);

      renderHeader();
      renderContent();
    }

    // Repaint everything this app owns WITHOUT resetting the tab and
    // without a full-screen wipe. This is what the sketch should call when
    // the volume overlay hands its column back: loadScreen() was being used
    // for that, and it sent you back to the CPU tab and strobed the display
    // every single time you touched the volume knob inside this app.
    void redraw() {
      tft.drawRoundRect(boxX, boxY, boxW, boxH, UI_RADIUS_SM, UI_BORDER);
      renderHeader();
      renderContent();
    }

    void fillBackground(TFT_eSprite* canvas, int canvasX, int canvasY) {
      canvas->setSwapBytes(true);
      canvas->fillScreen(UI_BG);
    }

    void renderHeader() {
      fillBackground(headerSPR, 0, headerY);
      
      headerSPR->loadFont(FONT_UI_SM);
      headerSPR->setTextDatum(MC_DATUM);
      headerSPR->setTextWrap(false);
      
      int tabWidth = 320 / 5;
      
      for (int i = 0; i < 5; i++) {
        int centerX = (i * tabWidth) + (tabWidth / 2);
        
        // Stationary text, moving highlight pill
        if (i == currentTab) {
          headerSPR->fillRoundRect(i * tabWidth + 2, 0, tabWidth - 4, headerH, UI_RADIUS_SM, UI_ACCENT_GLOW);
          headerSPR->setTextColor(UI_ACCENT);
        } else {
          headerSPR->setTextColor(UI_TEXT_MUTED);
        }
        
        headerSPR->drawString(tabs[i], centerX, headerH / 2);
      }

      headerSPR->pushSprite(0, headerY);
      headerSPR->unloadFont();
    }

    void renderContent() {
      // Offset the background fill so it aligns perfectly inside the static box
      fillBackground(contentSPR, boxX + 2, boxY + 2);
      contentSPR->setTextWrap(false);

      switch (currentTab) {
        case 0:
          renderHeroStat(hostCPU, "HOST CPU LOAD");
          break;

        case 1:
          renderHeroStat(hostRAM, "HOST RAM LOAD");
          break;

        case 2:
          renderRow(0, "CAPACITY", String(ESP.getFlashChipSize() / (1024.0 * 1024.0), 1) + " MB", UI_TEXT);
          renderRow(1, "TYPE", "SPI Flash", UI_TEXT);
          renderRow(2, "SPEED", String(ESP.getFlashChipSpeed() / 1000000) + " MHz", UI_TEXT);
          renderRow(3, "SYSTEM DISK", "Yes", UI_TEXT);
          break;

        case 3: {
          bool online = !(WiFi.getMode() == WIFI_OFF || WiFi.status() != WL_CONNECTED);
          renderRow(0, "ADAPTER", "ESP32 Wi-Fi", UI_TEXT);
          if (!online) {
            renderRow(1, "STATUS", "OFFLINE", UI_BAD);
            renderRow(2, "RADIO", "Disabled", UI_TEXT_MUTED);
          } else {
            renderRow(1, "SSID", WiFi.SSID(), UI_GOOD);
            renderRow(2, "IPV4", WiFi.localIP().toString(), UI_TEXT);
            renderRow(3, "SIGNAL", String(WiFi.RSSI()) + " dBm", UI_TEXT);
          }
          break;
        }

        case 4:
          renderHeroStat(hostGPU, "HOST GPU LOAD");
          break;
      }

      // Push the sprite safely inside the static border
      contentSPR->pushSprite(boxX + 2, boxY + 2);
      contentSPR->unloadFont();
    }

    void clearScreen() {
      headerSPR->deleteSprite();
      contentSPR->deleteSprite();
    }
};
