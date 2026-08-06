#pragma once
#include <TFT_eSPI.h> 
#include <vector>
#include "apps.h"

extern TFT_eSPI tft;

class playlistApp : public app {
  private:
    std::vector<String> playlists = {
      "2PJmFBjYZfcbabEZO57z0y?si=85d1424d16cd49cd",
      "513OYkesf7ud9u8fYMR21H?si=123267eee845405d"
    };
    std::vector<String> names = {
      "Non Rap Playlist",
      "Rap Playlist"
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

    // NEW: Memory protection flag
    bool spriteCreated = false; 

  public:
    playlistApp(String name) : app(name) { 
      canvas = new TFT_eSprite(&tft);
      targetOffset = currentSelection * itemSpacing;
      scrollOffset = targetOffset;
      
      // REMOVED ALL DRAWING COMMANDS FROM HERE!
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

    int playlistSelection(){
      return currentSelection;
    }
    
    void clearScreen(){
      canvas->deleteSprite(); 
      // Reset the flag so it knows to rebuild the sprite next time you open the app
      spriteCreated = false; 
    }
};