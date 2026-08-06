#pragma once
#include <TFT_eSPI.h>
#include <ESP32Encoder.h>
#include "background.h" 
#include "customFonts.h" 

extern TFT_eSPI tft;
extern ESP32Encoder encoder;

class pinScreen {
  private:
  // FIX: Removed the '=' sign
  struct UIBounds {
    int x;
    int y;
    int w;
    int h;
  };

  // Give these some default layout coordinates to start
  UIBounds numberDisplay = {110, 50, 100, 60}; // Dead center, upper half
  
  // 4 evenly spaced circles across the bottom
  UIBounds circles[4] = {
    {65, 150, 30, 30},
    {115, 150, 30, 30},
    {165, 150, 30, 30},
    {215, 150, 30, 30}
  };

  TFT_eSprite* circlesSPR[4];
  TFT_eSprite* numberSPR;

  int pinNumbers[4] = {1, 2, 3, 4}; // The master password
  int currentDigit = 0; // FIX: C++ arrays start at 0

  public:
  pinScreen() {
    encoder.setCount(0);
    
    // FIX: Must initialize all sprite pointers in memory
    for(int i = 0; i < 4; i++){
      circlesSPR[i] = new TFT_eSprite(&tft);
    }
    numberSPR = new TFT_eSprite(&tft);
  }

  // --- NEW: Add this helper from your previous classes ---
  void fillBackground(TFT_eSprite* canvas, int canvasX, int canvasY){
    canvas->setSwapBytes(true);
    canvas->pushImage(-canvasX, -canvasY, 320, 240, mainBackground);
  }

  // Use this in your main loop() to keep the screen updating
  void run() {
    int currentCount = encoder.getCount() / 2; 
    
    // Keep the encoder cleanly looping between 0 and 9
    if (currentCount > 9) { encoder.setCount(0); currentCount = 0; }
    if (currentCount < 0) { encoder.setCount(18); currentCount = 9; } // 18 / 2 = 9

    renderPrompt(currentCount);
  }

  // Call this when the encoder is clicked (clickState == 1)
  bool input(bool confirm) { 
    if(confirm) {
      int currentCount = encoder.getCount() / 2;
      return verify(currentCount); // Return the true/false unlock state
    }
    return false;
  }

  bool verify(int userInput) {
    // If they got the digit right
    if(userInput == pinNumbers[currentDigit]) {
      currentDigit++;
      encoder.setCount(0); // Reset dial to 0 for the next number
      
      // If they successfully entered all 4 digits
      if(currentDigit == 4) {
        return true; // UNLOCKED!
      }
    } 
    // If they get it wrong at any point, brutally reset them back to the start
    else {
      currentDigit = 0; 
      encoder.setCount(0);
    }
    return false;
  }

  void renderPrompt(int currentNumber) {
    // 1. Draw the live number so you aren't flying blind
    numberSPR->createSprite(numberDisplay.w, numberDisplay.h);
    fillBackground(numberSPR, numberDisplay.x, numberDisplay.y);
    numberSPR->setTextDatum(MC_DATUM);
    numberSPR->loadFont(BebasNeue_Regular21);
    numberSPR->setTextColor(TFT_WHITE);
    numberSPR->drawString(String(currentNumber), numberDisplay.w/2, numberDisplay.h/2);
    numberSPR->pushSprite(numberDisplay.x, numberDisplay.y);
    numberSPR->deleteSprite();

    // 2. Draw the 4 progress circles
    for(int i = 0; i < 4; i++) {
      circlesSPR[i]->createSprite(circles[i].w, circles[i].h);
      fillBackground(circlesSPR[i], circles[i].x, circles[i].y);
      
      // FIX: Calculate Center and Radius for circles
      int radius = circles[i].w / 2;
      int centerX = circles[i].w / 2;
      int centerY = circles[i].h / 2;

      // Draw a solid circle if completed, hollow if pending
      if(i < currentDigit) {
        circlesSPR[i]->fillCircle(centerX, centerY, radius, TFT_WHITE);
      } else {
        circlesSPR[i]->drawCircle(centerX, centerY, radius, TFT_WHITE);
      }
      
      circlesSPR[i]->pushSprite(circles[i].x, circles[i].y);
      circlesSPR[i]->deleteSprite();
    }
  }

  // Safely clear memory when exiting
  void clearScreen() {
    delete numberSPR;
    for(int i = 0; i < 4; i++) {
      delete circlesSPR[i];
    }
  }
};