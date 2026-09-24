#pragma once
#include <TFT_eSPI.h> 
#include <vector>
#include "apps.h"
#include "theme.h"

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

    // Carousel sprite kept close to its original small footprint — see
    // the matching note in linksApp.h for why.
    int canvasW = 320;
    int canvasH = 40; 
    int canvasX = 0;
    int canvasY = 100; 

    int titleY = 68;
    int dotsY = 158;
  
    int currentSelection = 0;
    float scrollOffset = 0.0;    
    int targetOffset = 0;        
    int itemSpacing = 120;        
    int center = canvasW / 2;

    bool spriteCreated = false; 

    void renderTitle() {
      tft.setTextDatum(MC_DATUM);
      tft.loadFont(FONT_UI_SM);
      tft.setTextColor(UI_TEXT_GHOST);
      tft.drawString("PLAYLISTS", center, titleY);
      tft.unloadFont();
    }

    void renderDots() {
      int dotSpacing = 16;
      int dotsTotalW = (int)(names.size() - 1) * dotSpacing;
      int dotsStartX = center - dotsTotalW / 2;
      for (int i = 0; i < (int)names.size(); i++) {
        int dx = dotsStartX + i * dotSpacing;
        tft.fillCircle(dx, dotsY, 3, UI_BG);
        if (i == currentSelection) {
          tft.fillCircle(dx, dotsY, 3, UI_ACCENT);
        } else {
          tft.fillCircle(dx, dotsY, 2, UI_TEXT_GHOST);
        }
      }
    }

  public:
    playlistApp(String name) : app(name) { 
      canvas = new TFT_eSprite(&tft);
      targetOffset = currentSelection * itemSpacing;
      scrollOffset = targetOffset;
    }

    ~playlistApp() {
      if (canvas != nullptr) {
        canvas->deleteSprite();
        delete canvas;
        canvas = nullptr;
      }
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
      // Only allocate the RAM and set the datum on the very first render!
      if (!spriteCreated) {
        canvas->createSprite(canvasW, canvasH);
        canvas->setTextDatum(MC_DATUM);
        canvas->setTextWrap(false);
        spriteCreated = true;
      }

      canvas->setSwapBytes(true);
      canvas->fillSprite(UI_BG);
      
      for(int i = 0; i < names.size(); i++) {
        float xPos = (i * itemSpacing) - scrollOffset + center;
        
        if(xPos > -30 && xPos < canvasW + 30) {
          if(i == currentSelection) {
            canvas->loadFont(FONT_UI_LG);
            canvas->setTextColor(UI_ACCENT); 
            canvas->drawString(names[i], xPos, canvasH / 2);
          } else {
            float t = fabsf((float)(i - currentSelection)) / 3.0f;
            canvas->loadFont(FONT_UI_SM);
            canvas->setTextColor(uiLerp565(UI_TEXT_MUTED, UI_TEXT_GHOST, t));
            canvas->drawString(names[i], xPos, canvasH / 2);
          }
        }
      }
      canvas->pushSprite(canvasX, canvasY);

      renderTitle();
      renderDots();
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
