#pragma once
#include <TFT_eSPI.h> 
#include <vector>
#include "apps.h"

extern TFT_eSPI tft;

class linksApp : public app {
  private:
    std::vector<String> links = {
      "https://www.youtube.com/",
      "https://gemini.google.com/app",
      "https://github.com/",
      "https://www.desmos.com/calculator",
      "spotify:"
    };
    std::vector<String> names = {
      "YouTube",
      "Gemini",
      "GitHub",
      "Desmos",
      "Spotify"
    }; 

    TFT_eSprite* canvas; 
  
    int iconW;
    int iconH = 140; 
    int iconX;
    int iconY;
    int iconTextSeparation = 10;

    int canvasW = 320;
    int canvasH = 35; 
    int canvasX = 0;
    int canvasY = 160; 
  
    int currentSelection = 0;
    float scrollOffset = 0.0;    
    int targetOffset = 0;        
    int itemSpacing = 120;        
    int center = canvasW / 2;

    bool spriteCreated = false; 

  public:
    linksApp(String name) : app(name) { 
      canvas = new TFT_eSprite(&tft);
      targetOffset = currentSelection * itemSpacing;
      scrollOffset = targetOffset;
    }

    void scroll(int button) {
      if(button == 0) { 
        if(currentSelection > 0) { 
          currentSelection--;
          targetOffset = currentSelection * itemSpacing; 
        }
      }
      else if(button == 1) { 
        if(currentSelection < names.size() - 1) { 
          currentSelection++;
          targetOffset = currentSelection * itemSpacing; 
        }
      }
    }

    void animate() {
      if (abs(targetOffset - scrollOffset) > 0.5) {
        scrollOffset += (targetOffset - scrollOffset) * 0.2; 
        renderSelectionScreen();
      } else if (scrollOffset != targetOffset) {
        scrollOffset = targetOffset;
        renderSelectionScreen();
      }
    }

    void renderSelectionScreen() {
      // NEW: Only allocate the RAM and set the datum on the very first render!
      if (!spriteCreated) {
        canvas->createSprite(canvasW, canvasH);
        canvas->setTextDatum(MC_DATUM);
        canvas->setTextWrap(false);
        spriteCreated = true;
      }

      canvas->setSwapBytes(true);
      canvas->pushImage(-canvasX, -canvasY, 320, 240, mainBackground);
      
      for(int i = 0; i < names.size(); i++) {
        float xPos = (i * itemSpacing) - scrollOffset + center;
        
        if(xPos > -30 && xPos < canvasW + 30) {
          if(i == currentSelection) {
            canvas->loadFont(BebasNeue_Regular25);
            canvas->setTextColor(TFT_WHITE); 
            canvas->drawString(names[i], xPos, canvasH / 2);
          } else {
            canvas->loadFont(BebasNeue_Regular21);
            canvas->setTextColor(TFT_NAVY); 
            canvas->drawString(names[i], xPos, canvasH / 2);
          }
        }
      }
      canvas->pushSprite(canvasX, canvasY);
    }

    void run() {
      if (!spriteCreated) {
        renderSelectionScreen();
      }
      animate();
    }

    int linkSelection(){
      return currentSelection;
    }
    
    void clearScreen(){
      canvas->deleteSprite(); 
      // Reset the flag so it knows to rebuild the sprite next time you open the app
      spriteCreated = false; 
    }
};