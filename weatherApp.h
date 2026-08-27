#pragma once
#include <TFT_eSPI.h>
#include <time.h>
#include "apps.h"
#include "background.h"
#include "customFonts.h"

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

    char times[8][16];
    int temperatures[8];
    char conditions[8][32];

    bool hasRequested = false;
    TFT_eSprite sprite;

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
      for(int i = 0; i < arraySize; i++){
        strncpy(times[i], t[i].c_str(), sizeof(times[i]) - 1);
        times[i][sizeof(times[i]) - 1] = '\0';
        
        temperatures[i] = temps[i];
        
        strncpy(conditions[i], conds[i].c_str(), sizeof(conditions[i]) - 1);
        conditions[i][sizeof(conditions[i]) - 1] = '\0';
      }
      dataArrived = true;
      updateUI();
    }

    void loadScreen() {
      reqData();

      currentSelection = 0;
      targetOffset = 0;
      tft.setSwapBytes(true);
      tft.pushImage(0, 0, 320, 240, mainBackground);

      if(!dataArrived) {
        renderLoadingScreen();
      } else {
        renderMainDisplay();
        renderScrollArea();
      }
    }

    void renderLoadingScreen(){
      sprite.createSprite(160, 40);
      sprite.setSwapBytes(true);
      sprite.pushImage(-80, -100, 320, 240, mainBackground);
      sprite.setTextColor(TFT_WHITE);
      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(BebasNeue_Regular21);
      sprite.drawString("Collecting Data ...", 80, 20);
      sprite.pushSprite(80, 100);
      sprite.unloadFont();
      sprite.deleteSprite();
    }

    void scroll(int button) {
      if(button == 0) { 
        if(currentSelection > 0) { 
          currentSelection--;
          targetOffset = currentSelection * itemSpacing; 
          updateUI();
        }
      }
      else if(button == 1) { 
        if(currentSelection < 7) { 
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
    }

    void updateUI() {
      renderMainDisplay();
      renderScrollArea();
    }

    void renderMainDisplay() {
      sprite.createSprite(topArea.w, topArea.h);
      sprite.setSwapBytes(true);
      sprite.pushImage(0, 0, 320, 240, mainBackground);
      
      sprite.setTextColor(TFT_WHITE);
      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(BebasNeue_Regular21);
      
      sprite.drawString(getDateString(), 160, 20);
      sprite.drawString("Cranston, RI", 160, 45);

      sprite.drawString(conditions[currentSelection], 160, 90);
      
      char tempBuf[16];
      snprintf(tempBuf, sizeof(tempBuf), "%d  ", temperatures[currentSelection]);
      sprite.drawString(tempBuf, 160, 125); 

      sprite.pushSprite(topArea.x, topArea.y);
      sprite.unloadFont();
      sprite.deleteSprite();
    }

    void renderScrollArea() {
      sprite.createSprite(scrollArea.w, scrollArea.h);
      sprite.setSwapBytes(true);
      sprite.pushImage(0, -scrollArea.y, 320, 240, mainBackground);

      sprite.setTextColor(TFT_WHITE);
      sprite.setTextDatum(MC_DATUM);
      sprite.loadFont(BebasNeue_Regular21);

      for(int i = 0; i < 8; i++) {
        int xPos = (i * itemSpacing) - targetOffset + 160; 
        
        if (xPos > -30 && xPos < 350) {
          if (i == currentSelection) {
            sprite.setTextColor(TFT_NAVY);
          } else {
            sprite.setTextColor(TFT_DARKGREY);
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
};