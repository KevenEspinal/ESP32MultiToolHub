#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "background.h" 
#include "customFonts.h"
#include "smoothFonts.h" 

extern TFT_eSPI tft; 

class menu {
  private:
  TFT_eSprite* canvas; 
  
  int canvasW = 200;
  int canvasH = 240; 
  int canvasX = 7;
  int canvasY = 0; 
  
  int currentSelection = 0;
  std::vector<String> appNames;

  float scrollOffset = 0.0;    
  int targetOffset = 0;        
  int itemSpacing = 36;        

  int center = canvasH / 2;
  public:
  menu(){}

  void setup(std::vector<String> appsNames) { 
    appNames = appsNames;
    currentSelection = 0; 
    
    canvas = new TFT_eSprite(&tft);
    canvas->createSprite(canvasW, canvasH);
    canvas->setTextDatum(MC_DATUM);

    targetOffset = currentSelection * itemSpacing;
    scrollOffset = targetOffset;
    
    renderMenu();
  }

  void scroll(int button) {
    if(button == 2) { 
      if(currentSelection > 0) { 
        currentSelection--;
        targetOffset = currentSelection * itemSpacing; 
      }
    }
    else if(button == 3) { 
      if(currentSelection < appNames.size() - 1) { 
        currentSelection++;
        targetOffset = currentSelection * itemSpacing; 
      }
    }
  }
  
  void animate() {
    if (abs(targetOffset - scrollOffset) > 0.5) {
      scrollOffset += (targetOffset - scrollOffset) * 0.2; 
      renderMenu();
    } else if (scrollOffset != targetOffset) {
      scrollOffset = targetOffset;
      renderMenu();
    }
  }
  
  void renderMenu() {
    canvas->setSwapBytes(true);
    canvas->pushImage(-canvasX, -canvasY, 320, 240, mainBackground);
    
    for(int i = 0; i < appNames.size(); i++) {
      
      float yPos = (i * itemSpacing) - scrollOffset + center;
      
      if(yPos > -30 && yPos < canvasH + 30) {
        
        if(i == currentSelection) {
          canvas->loadFont(BebasNeue_Regular25);
          canvas->setTextColor(TFT_WHITE); 
          canvas->drawString(appNames[i], canvasW / 2, yPos);
        } else {
          canvas->loadFont(BebasNeue_Regular21);
          canvas->setTextColor(TFT_NAVY); 
          canvas->drawString(appNames[i], canvasW / 2, yPos);
        }
      }
    }
    
    canvas->pushSprite(canvasX, canvasY);
  }

  int appSelection(){
    return currentSelection;
  }
  void clearMenu(){
    canvas->deleteSprite();
  }
};