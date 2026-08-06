#pragma once
#include <TFT_eSPI.h>
#include <ESP32Encoder.h> 
#include "background.h"   

extern TFT_eSPI tft;
extern ESP32Encoder encoder;

class volumeHeader{
  private:

  //Volume Bar setup
  int volBarW = 14;                                              //width
  int volBarH = 210;                                             //height
  int volBarX = 5;                                              //x position
  int volBarY;
  int separation = 0;                                            //pixel difference between outline and inner figure
  int oldSize = 0;
  int size = 0;
  int r = volBarW / 2;
  bool temp = true;

  unsigned long hideTime = 0; 
  long oldVolume = 0;
  TFT_eSprite volumeSPR;      

  public:

  volumeHeader() : volumeSPR(&tft) {}

  void setupSprite(){
    volBarY = (tft.height() - volBarH) / 2; 
    volumeSPR.createSprite(volBarW, volBarH);   
  }

  void calculateVolume(int encoderReading){
    
    if (encoderReading > 34) {
      temp = true;
    }

    long currentVolume = encoderReading * 3;
    
    if(currentVolume > 100) { currentVolume = 100; encoder.setCount(68); }
    if(currentVolume < 0)   { currentVolume = 0;   encoder.setCount(0); }
    
    // --- NEW: THE PC BROADCASTER ---
    // Only send the command if the volume physically changed from the last frame
    if (currentVolume > oldVolume) {
      Serial.println("VOL_UP");
    } else if (currentVolume < oldVolume) {
      Serial.println("VOL_DOWN");
    }
    // -------------------------------

    if(currentVolume != oldVolume || oldSize != size || (currentVolume == 100 && temp)) {
      if (!volumeSPR.created()) {
        volumeSPR.createSprite(volBarW, volBarH);
      }
      volumeBar(currentVolume);
      hideTime = millis() + 800; 
    }
    
    // The oldVolume is updated here, resetting the Broadcaster check for the next loop
    oldVolume = currentVolume; 

    // The non-blocking disappearing act
    if (hideTime > 0 && millis() > hideTime) {
      if (!volumeSPR.created()) {
        volumeSPR.createSprite(volBarW, volBarH);
      }
      volumeSPR.setSwapBytes(true);
      volumeSPR.pushImage(-volBarX, -volBarY, 320, 240, mainBackground);
      
      volumeSPR.pushSprite(volBarX, volBarY);
      
      if (volumeSPR.created()) {
        volumeSPR.deleteSprite();
      }
      hideTime = 0; 
    }
  }

  void volumeBar(int volume){
    if(volume == 100 && temp){
      volume = 112; 
    } else if (volume < 100) {
      temp = true;
    }
    
    int targetSize = ( (volBarH - 24 - (2*separation)) * volume ) / 100;  
    size = targetSize;
    
    if(oldSize < targetSize) {
      oldSize++;
    } else if (oldSize > targetSize) {
      oldSize--;
    }
    volumeSPR.setSwapBytes(true);
    volumeSPR.pushImage(-volBarX, -volBarY, 320, 240, mainBackground);
    int startY = volBarH - separation - oldSize;
    volumeSPR.fillRoundRect(separation, startY, volBarW - (2 * separation), oldSize, r, TFT_WHITE);
    volumeSPR.pushSprite(volBarX, volBarY);
    delay(7); 
    
    if(oldSize == targetSize && volume == 112){
      temp = false;
      size = ( (volBarH - 24 - (2*separation)) * 100 ) / 100;
    }
  }
};