#pragma once
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "background.h"
#include "customFonts.h"
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

  public:
    int hostCPU = 0;
    int hostRAM = 0;
    int hostGPU = 0;
    unsigned long lastHostUpdate = 0;

    // Implemented your exact inherited constructor
    diagnosticsApp(String name) : app(name) {}

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
      tft.pushImage(0, 0, 320, 240, mainBackground); 
      
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
      tft.pushImage(0, 0, 320, 240, mainBackground);
      
      // Draw the static white box directly to the screen ONCE
      tft.drawRoundRect(boxX, boxY, boxW, boxH, 4, TFT_WHITE);

      renderHeader();
      renderContent();
    }

    void fillBackground(TFT_eSprite* canvas, int canvasX, int canvasY) {
      canvas->setSwapBytes(true);
      canvas->pushImage(-canvasX, -canvasY, 320, 240, mainBackground);
    }

    void renderHeader() {
      TFT_eSprite* headerSPR = new TFT_eSprite(&tft);
      headerSPR->createSprite(320, headerH);
      fillBackground(headerSPR, 0, headerY);
      
      headerSPR->loadFont(BebasNeue_Regular21);
      headerSPR->setTextDatum(MC_DATUM);
      headerSPR->setTextWrap(false);
      
      int tabWidth = 320 / 5;
      
      for (int i = 0; i < 5; i++) {
        int centerX = (i * tabWidth) + (tabWidth / 2);
        
        // Stationary text, moving cursor box
        if (i == currentTab) {
          headerSPR->fillRoundRect(i * tabWidth + 2, 0, tabWidth - 4, headerH, 4, TFT_NAVY);
          headerSPR->setTextColor(TFT_WHITE);
        } else {
          headerSPR->setTextColor(TFT_DARKGREY);
        }
        
        headerSPR->drawString(tabs[i], centerX, headerH / 2);
      }

      headerSPR->pushSprite(0, headerY);
      headerSPR->unloadFont();
      headerSPR->deleteSprite();
      delete headerSPR;
    }

    void renderContent() {
      TFT_eSprite* contentSPR = new TFT_eSprite(&tft);
      
      // shrink the sprite to fit inside the white box and 
      // reduce the height to 140px. 306x140 takes ~85 KB of RAM, meaning 
      // the ESP32 can safely render it in true 16-bit color
      contentSPR->createSprite(boxW - 4, 140);
      
      // Offset the background fill so it aligns perfectly inside the static box
      fillBackground(contentSPR, boxX + 2, boxY + 2);
      
      contentSPR->setTextColor(TFT_WHITE);
      contentSPR->setTextDatum(TL_DATUM); 
      contentSPR->setTextWrap(false); 
      contentSPR->loadFont(BebasNeue_Regular21); 
      
      // Adjusted coordinates to account for the 2px sprite shift
      int col1X = 13; 
      int startY = 13;
      int rowSpacing = 35;

      switch (currentTab) {
        case 0:
          if (millis() - lastHostUpdate < 3000) {
            contentSPR->drawString("Host CPU Load: " + String(hostCPU) + "%", col1X, startY);
          } else {
            contentSPR->drawString("Waiting for Host...", col1X, startY);
          }
          break;

        case 1:
          if (millis() - lastHostUpdate < 3000) {
            contentSPR->drawString("Host RAM Load: " + String(hostRAM) + "%", col1X, startY);
          } else {
            contentSPR->drawString("Waiting for Host...", col1X, startY);
          }
          break;

        case 2:
          contentSPR->drawString("Capacity: " + String(ESP.getFlashChipSize() / (1024.0 * 1024.0), 1) + " MB", col1X, startY);
          contentSPR->drawString("Type: SPI Flash", col1X, startY + rowSpacing);
          contentSPR->drawString("Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz", col1X, startY + (rowSpacing * 2));
          contentSPR->drawString("System disk: Yes", col1X, startY + (rowSpacing * 3));
          break;

        case 3:
          contentSPR->drawString("Adapter: ESP32 Wi-Fi", col1X, startY);
          
          if (WiFi.getMode() == WIFI_OFF || WiFi.status() != WL_CONNECTED) {
            contentSPR->drawString("Status: OFFLINE", col1X, startY + rowSpacing);
            contentSPR->drawString("Radio: Disabled", col1X, startY + (rowSpacing * 2));
          } else {
            contentSPR->drawString("SSID: " + WiFi.SSID(), col1X, startY + rowSpacing);
            contentSPR->drawString("IPv4: " + WiFi.localIP().toString(), col1X, startY + (rowSpacing * 2));
            contentSPR->drawString("Signal (RSSI): " + String(WiFi.RSSI()) + " dBm", col1X, startY + (rowSpacing * 3));
          }
          break;

        case 4:
          if (millis() - lastHostUpdate < 3000) {
            contentSPR->drawString("Host GPU Load: " + String(hostGPU) + "%", col1X, startY);
          } else {
            contentSPR->drawString("Waiting for Host...", col1X, startY);
          }
          break;
      }

      // Push the sprite safely inside the static white borders
      contentSPR->pushSprite(boxX + 2, boxY + 2);
      contentSPR->unloadFont();
      contentSPR->deleteSprite();
      delete contentSPR;
    }

    void clearScreen() {
        // Leave empty if memory is strictly managed by local sprite generation
    }
};