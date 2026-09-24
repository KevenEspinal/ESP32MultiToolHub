#pragma once
#include <TFT_eSPI.h> 
#include <vector>
#include "apps.h"
#include "theme.h"

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

    // Carousel sprite is kept close to its original small footprint —
    // the title and position dots below are drawn straight to `tft`
    // instead of living inside a bigger sprite, so this redesign costs
    // almost no extra RAM (~3KB vs. the ~67KB an earlier version used,
    // which is what caused the crash-on-open bug on real hardware).
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
      tft.drawString("LINKS", center, titleY);
      tft.unloadFont();
    }

    void renderDots() {
      int dotSpacing = 16;
      int dotsTotalW = (int)(names.size() - 1) * dotSpacing;
      int dotsStartX = center - dotsTotalW / 2;
      for (int i = 0; i < (int)names.size(); i++) {
        int dx = dotsStartX + i * dotSpacing;
        // Erase to the largest radius used first — otherwise shrinking a
        // dot from "selected" to "unselected" leaves a stray ring behind.
        tft.fillCircle(dx, dotsY, 3, UI_BG);
        if (i == currentSelection) {
          tft.fillCircle(dx, dotsY, 3, UI_ACCENT);
        } else {
          tft.fillCircle(dx, dotsY, 2, UI_TEXT_GHOST);
        }
      }
    }

  public:
    linksApp(String name) : app(name) { 
      canvas = new TFT_eSprite(&tft);
      targetOffset = currentSelection * itemSpacing;
      scrollOffset = targetOffset;
    }

    ~linksApp() {
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

    int linkSelection(){
      return currentSelection;
    }
    
    void clearScreen(){
      canvas->deleteSprite(); 
      // Reset the flag so it knows to rebuild the sprite next time you open the app
      spriteCreated = false; 
    }
};
