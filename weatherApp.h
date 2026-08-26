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

    String times[8];
    int temperatures[8];
    String conditions[8];

    bool hasRequested = false;

    String getDateString() {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char dayBuf[16];
        char monthBuf[16];
        strftime(dayBuf, sizeof(dayBuf), "%A", &timeinfo);
        strftime(monthBuf, sizeof(monthBuf), "%B", &timeinfo);
        return String(dayBuf) + ", " + String(monthBuf) + " " + String(timeinfo.tm_mday);
      }
      return "Friday, August 7";
    }

  public:
    bool dataArrived = false;

    weatherApp(String name) : app(name) {
    }

    void reqData(){
      if (hasRequested) return;
      Serial.println("REQ_WEATHER_DATA");
      hasRequested = true;
    }

    void updateData(String t[], int temps[], String conds[], int arraySize){
      for(int i = 0; i < arraySize; i++){
        times[i] = t[i];
        temperatures[i] = temps[i];
        conditions[i] = conds[i];
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
      TFT_eSprite* loadingScreen = new TFT_eSprite(&tft);
      loadingScreen->createSprite(160, 40);
      loadingScreen->setSwapBytes(true);
      loadingScreen->pushImage(-80, -100, 320, 240, mainBackground);
      loadingScreen->setTextColor(TFT_WHITE);
      loadingScreen->setTextDatum(MC_DATUM);
      loadingScreen->loadFont(BebasNeue_Regular21);
      loadingScreen->drawString("Collecting Data ...", 80, 20);
      loadingScreen->pushSprite(80, 100);
      loadingScreen->unloadFont();
      loadingScreen->deleteSprite();
      delete loadingScreen;
    }

    void scroll(int button) {
      if(button == 2) { 
        if(currentSelection > 0) { 
          currentSelection--;
          targetOffset = currentSelection * itemSpacing; 
          updateUI();
        }
      }
      else if(button == 3) { 
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
      TFT_eSprite* topSPR = new TFT_eSprite(&tft);
      topSPR->createSprite(topArea.w, topArea.h);
      topSPR->setSwapBytes(true);
      topSPR->pushImage(0, 0, 320, 240, mainBackground);
      
      topSPR->setTextColor(TFT_WHITE);
      topSPR->setTextDatum(MC_DATUM);
      topSPR->loadFont(BebasNeue_Regular21);
      
      topSPR->drawString(getDateString(), 160, 20);
      topSPR->drawString("Cranston, RI", 160, 45);

      topSPR->drawString(conditions[currentSelection], 160, 90);
      topSPR->drawString(String(temperatures[currentSelection]) + "  ", 160, 125); 

      topSPR->pushSprite(topArea.x, topArea.y);
      topSPR->unloadFont();
      topSPR->deleteSprite();
      delete topSPR;
    }

    void renderScrollArea() {
      TFT_eSprite* scrollSPR = new TFT_eSprite(&tft);
      scrollSPR->createSprite(scrollArea.w, scrollArea.h);
      scrollSPR->setSwapBytes(true);
      scrollSPR->pushImage(0, -scrollArea.y, 320, 240, mainBackground);

      scrollSPR->setTextColor(TFT_WHITE);
      scrollSPR->setTextDatum(MC_DATUM);
      scrollSPR->loadFont(BebasNeue_Regular21);

      for(int i = 0; i < 8; i++) {
        int xPos = (i * itemSpacing) - targetOffset + 160; 
        
        if (xPos > -30 && xPos < 350) {
          if (i == currentSelection) {
            scrollSPR->setTextColor(TFT_NAVY);
          } else {
            scrollSPR->setTextColor(TFT_DARKGREY);
          }
          scrollSPR->drawString(times[i], xPos, 20);
          scrollSPR->drawString(String(temperatures[i]), xPos, 50);
        }
      }

      scrollSPR->pushSprite(scrollArea.x, scrollArea.y);
      scrollSPR->unloadFont();
      scrollSPR->deleteSprite();
      delete scrollSPR;
    }
};