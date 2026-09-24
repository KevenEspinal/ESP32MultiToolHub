#pragma once
#include <TFT_eSPI.h>
#include <time.h>
#include "apps.h"
#include "background.h"
#include "theme.h"

extern TFT_eSPI tft;

class weatherApp : public app {
  private:
    struct UIBounds {
      int x;
      int y;
      int w;
      int h;
    };

    UIBounds topArea = {0, 0, 320, 150};
    UIBounds scrollArea = {0, 160, 320, 80};

    int currentSelection = 0;
    int targetOffset = 0;
    int itemSpacing = 70;

    // Zero-initialised on purpose. renderMainDisplay() is reachable before
    // any payload has arrived (see isDataLoaded() below), and drawString()
    // on an unterminated char array walks straight off the end of the object.
    char times[8][16]      = {{0}};
    int  temperatures[8]   = {0};
    char conditions[8][32] = {{0}};
    int  entryCount        = 0;   // how many of the 8 slots actually hold data

    bool hasRequested = false;
    TFT_eSprite sprite;

    // These sprites are large (the top area alone is 320x150 = 96KB) and are
    // rebuilt on every redraw, so an allocation can fail once BLE and the
    // album-art buffer have taken their share. createSprite() returning null
    // used to pass unnoticed and the screen just stayed blank.
    bool beginSprite(int w, int h) {
      if (sprite.created()) sprite.deleteSprite();
      sprite.setColorDepth(16);
      if (sprite.createSprite(w, h) == nullptr) {
        Serial.println("WEATHER: sprite allocation failed");
        return false;
      }
      sprite.setSwapBytes(true);
      return true;
    }

    const char* getDateString() {
      static char dateBuf[64];
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char dayBuf[16];
        char monthBuf[16];
        strftime(dayBuf, sizeof(dayBuf), "%A", &timeinfo);
        strftime(monthBuf, sizeof(monthBuf), "%B", &timeinfo);
        snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d", dayBuf, monthBuf, timeinfo.tm_mday);
        return dateBuf;
      }
      return "Friday, August 7";
    }

  public:
    bool dataArrived = false;

    weatherApp(String name) : app(name), sprite(&tft) {
    }

    void reqData(){
      if (hasRequested) return;
      Serial.println("REQ_WEATHER_DATA");
      hasRequested = true;
    }

    void updateData(String t[], int temps[], String conds[], int arraySize){
      // The sketch passes a fixed 8 even when the payload was short, so the
      // count has to be clamped here rather than trusted.
      if (arraySize < 0) arraySize = 0;
      if (arraySize > 8) arraySize = 8;

      for(int i = 0; i < arraySize; i++){
        strncpy(times[i], t[i].c_str(), sizeof(times[i]) - 1);
        times[i][sizeof(times[i]) - 1] = '\0';

        temperatures[i] = temps[i];

        strncpy(conditions[i], conds[i].c_str(), sizeof(conditions[i]) - 1);
        conditions[i][sizeof(conditions[i]) - 1] = '\0';
      }

      // Blank every slot the host didn't fill, so a short payload can't
      // leave the previous forecast (or garbage) sitting in the carousel.
      for(int i = arraySize; i < 8; i++){
        times[i][0] = '\0';
        conditions[i][0] = '\0';
        temperatures[i] = 0;
      }

      entryCount = arraySize;
      if (currentSelection >= entryCount) currentSelection = (entryCount > 0) ? entryCount - 1 : 0;
      targetOffset = currentSelection * itemSpacing;

      dataArrived = (entryCount > 0);
      if (dataArrived) updateUI();
    }

    void loadScreen() {
      reqData();

      currentSelection = 0;
      targetOffset = 0;
      tft.setSwapBytes(true);
      tft.fillScreen(UI_BG);

      if(!dataArrived) {
        renderLoadingScreen();
      } else {
        renderMainDisplay();
        renderScrollArea();
      }
    }

    void renderLoadingScreen(){
      if (!beginSprite(160, 40)) return;
      sprite.fillScreen(UI_BG);
      sprite.setTextColor(UI_TEXT_MUTED);
      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(FONT_UI_SM);
      sprite.drawString("Collecting Data...", 80, 20);
      sprite.pushSprite(80, 100);
      sprite.unloadFont();
      sprite.deleteSprite();
    }

    void scroll(int button) {
      if (!dataArrived) return;   // nothing to scroll through yet

      if(button == 0) {
        if(currentSelection > 0) {
          currentSelection--;
          targetOffset = currentSelection * itemSpacing;
          updateUI();
        }
      }
      // Was hard-coded to 7, which let you scroll onto hours the host never
      // sent whenever the payload was short.
      else if(button == 1) {
        if(currentSelection < entryCount - 1) {
          currentSelection++;
          targetOffset = currentSelection * itemSpacing;
          updateUI();
        }
      }
    }

    void run() {
    }

    void clearScreen() {
      dataArrived = false;
      hasRequested = false;
      entryCount = 0;
      currentSelection = 0;
      targetOffset = 0;
      if (sprite.created()) sprite.deleteSprite();
    }

    void updateUI() {
      // Falls back to the placeholder rather than rendering an empty array.
      if (!dataArrived) { renderLoadingScreen(); return; }
      renderMainDisplay();
      renderScrollArea();
    }

    void renderMainDisplay() {
      if (!dataArrived) return;
      if (!beginSprite(topArea.w, topArea.h)) return;
      sprite.fillScreen(UI_BG);
      
      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(FONT_UI_SM);
      sprite.setTextColor(UI_TEXT_MUTED);
      sprite.drawString(getDateString(), 160, 16);
      sprite.setTextColor(UI_TEXT_GHOST);
      sprite.drawString("Cranston, RI", 160, 36);

      // Icon chosen straight from the condition text the host already
      // sent over — no new data, just picking how to draw what we have.
      uiIconWeather(&sprite, 106, 90, 68, 46, String(conditions[currentSelection]), UI_TEXT, UI_ACCENT);

      // Hero temperature reading, with a small drawn ring standing in for
      // a degree mark (the compact font set may not include a "°" glyph).
      sprite.loadFont(FONT_HERO_LG);
      sprite.setTextDatum(ML_DATUM);
      sprite.setTextColor(UI_ACCENT);
      String tempStr = String(temperatures[currentSelection]);
      int tempX = 168;
      int tempW = sprite.textWidth(tempStr);
      sprite.drawString(tempStr, tempX, 92);
      sprite.drawCircle(tempX + tempW + 9, 92 - 15, 4, UI_ACCENT_DIM);

      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(FONT_UI_SM);
      sprite.setTextColor(UI_TEXT);
      sprite.drawString(conditions[currentSelection], 160, 130);

      sprite.pushSprite(topArea.x, topArea.y);
      sprite.unloadFont();
      sprite.deleteSprite();
    }

    void renderScrollArea() {
      if (!dataArrived) return;
      if (!beginSprite(scrollArea.w, scrollArea.h)) return;
      sprite.fillSprite(UI_BG);

      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(FONT_UI_SM);

      for(int i = 0; i < entryCount; i++) {
        int xPos = (i * itemSpacing) - targetOffset + 160; 
        
        if (xPos > -30 && xPos < 350) {
          // NOTE: the original had this pair swapped (the selected hour was
          // drawn in the dim colour and every other hour in the brighter
          // one) — flipped here to match how "selected" reads everywhere
          // else in the project.
          if (i == currentSelection) {
            sprite.setTextColor(UI_ACCENT);
          } else {
            sprite.setTextColor(UI_TEXT_MUTED);
          }
          sprite.drawString(times[i], xPos, 20);
          
          char tempBuf[16];
          snprintf(tempBuf, sizeof(tempBuf), "%d", temperatures[i]);
          sprite.drawString(tempBuf, xPos, 50);
        }
      }

      sprite.pushSprite(scrollArea.x, scrollArea.y);
      sprite.unloadFont();
      sprite.deleteSprite();
    }


    bool isDataLoaded(){
      return dataArrived;
    }
};
