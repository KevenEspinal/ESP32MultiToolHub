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
    // Implemented your exact inherited constructor
    diagnosticsApp(String name) : app(name) {}

    void loadScreen() {
      currentTab = 0;
      
      // FIX 1: Explicitly wipe the root physical screen before drawing our floating sprites
      tft.setSwapBytes(true);
      tft.pushImage(0, 0, 320, 240, mainBackground); 
      
      render();
    }

    void scroll(int buttonDirection) {
      if (buttonDirection == 2) { // Left
        if (currentTab > 0) currentTab--;
      } 
      else if (buttonDirection == 3) { // Right
        if (currentTab < 4) currentTab++;
      }
      render(); // Instantly update UI on scroll
    }

    void run() {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        renderContent(); // Only update the data box, not the whole screen
      }
    }

    void render() {
      renderHeader();
      renderContent();
    }

    void fillBackground(TFT_eSprite* canvas, int canvasX, int canvasY) {
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

      contentSPR->setColorDepth(8);
      
      contentSPR->createSprite(boxW, boxH);
      fillBackground(contentSPR, boxX, boxY);
      
      // The Delineated Box
      contentSPR->drawRoundRect(0, 0, boxW, boxH, 4, TFT_WHITE);
      
      // FIX 2: Explicitly format the text bounds to ensure rendering
      contentSPR->setTextColor(TFT_WHITE);
      contentSPR->setTextDatum(TL_DATUM); 
      contentSPR->setTextWrap(false); 
      contentSPR->loadFont(BebasNeue_Regular21); 
      
      // Basic coordinates for dual-column text
      int col1X = 15;
      int col2X = 160;
      int startY = 15;
      int rowSpacing = 35; // Increased spacing slightly for readability

      switch (currentTab) {
        case 0: // CPU (ESP32 Processor)
          contentSPR->drawString("Cores: 2 (Tensilica LX6)", col1X, startY);
          contentSPR->drawString("Speed: " + String(ESP.getCpuFreqMHz()) + " MHz", col2X, startY);
          
          contentSPR->drawString("Architecture: 32-bit", col1X, startY + rowSpacing);
          contentSPR->drawString("Base speed: 240 MHz", col2X, startY + rowSpacing);
          
          contentSPR->drawString("Up time: " + String(millis() / 1000) + " sec", col1X, startY + (rowSpacing * 2));
          contentSPR->drawString("Sockets: 1", col2X, startY + (rowSpacing * 2));
          break;

        case 1: // Memory (ESP32 RAM)
          contentSPR->drawString("Total Heap: " + String(ESP.getHeapSize() / 1024.0, 1) + " KB", col1X, startY);
          contentSPR->drawString("Available: " + String(ESP.getFreeHeap() / 1024.0, 1) + " KB", col2X, startY);
          
          contentSPR->drawString("In use: " + String((ESP.getHeapSize() - ESP.getFreeHeap()) / 1024.0, 1) + " KB", col1X, startY + rowSpacing);
          contentSPR->drawString("Type: SRAM", col2X, startY + rowSpacing);
          
          contentSPR->drawString("Min Free: " + String(ESP.getMinFreeHeap() / 1024.0, 1) + " KB", col1X, startY + (rowSpacing * 2));
          break;

        case 2: // Disk (ESP32 SPI Flash)
          contentSPR->drawString("Capacity: " + String(ESP.getFlashChipSize() / (1024.0 * 1024.0), 1) + " MB", col1X, startY);
          contentSPR->drawString("Type: SPI Flash", col2X, startY);
          
          contentSPR->drawString("Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz", col1X, startY + rowSpacing);
          contentSPR->drawString("System disk: Yes", col2X, startY + rowSpacing);
          break;

        case 3: // Wi-Fi (ESP32 Wireless)
          contentSPR->drawString("Adapter: ESP32 Wi-Fi", col1X, startY);
          
          // SAFETY FIX: Prevent core panics by checking if the radio is actually on
          if (WiFi.getMode() == WIFI_OFF || WiFi.status() != WL_CONNECTED) {
            contentSPR->drawString("Status: OFFLINE", col2X, startY);
            contentSPR->drawString("Radio: Disabled (Power Saved)", col1X, startY + rowSpacing);
            contentSPR->drawString("Signal (RSSI): N/A", col2X, startY + rowSpacing);
          } else {
            contentSPR->drawString("SSID: " + WiFi.SSID(), col2X, startY);
            contentSPR->drawString("IPv4: " + WiFi.localIP().toString(), col1X, startY + rowSpacing);
            contentSPR->drawString("Signal (RSSI): " + String(WiFi.RSSI()) + " dBm", col2X, startY + rowSpacing);
          }
          break;

        case 4: // GPU (TFT Display Driver)
          contentSPR->drawString("Display: TFT LCD", col1X, startY);
          contentSPR->drawString("Resolution: 320x240", col2X, startY);
          
          contentSPR->drawString("Driver: TFT_eSPI", col1X, startY + rowSpacing);
          contentSPR->drawString("Color Depth: 16-bit", col2X, startY + rowSpacing);
          
          contentSPR->drawString("Interface: SPI", col1X, startY + (rowSpacing * 2));
          contentSPR->drawString("Hardware reserved: 1", col2X, startY + (rowSpacing * 2));
          break;
      }

      contentSPR->pushSprite(boxX, boxY);
      contentSPR->unloadFont();
      contentSPR->deleteSprite();
      delete contentSPR;
    }

    void clearScreen() {
        // Leave empty if memory is strictly managed by local sprite generation
    }
};