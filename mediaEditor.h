#pragma once
#include <TFT_eSPI.h>
#include "background.h" 
#include "customFonts.h"
#include "smoothFonts.h" 

extern TFT_eSPI tft; 

class mediaEditor{
  private:
  // The Struct Definition
  struct UIBounds {
    int x;
    int y;
    int w;
    int h;
  };

  // --- 320x240 Landscape Layout Initialization ---
  // Format: {X, Y, Width, Height}
  
  UIBounds titleTxt    = {130, 20, 170, 40};  // Top Right
  UIBounds artistTxt   = {130, 70, 170, 30};  // Middle Right
  UIBounds thumbnail   = {20, 20, 90, 90};    // Top Left (Reserved for album art later)

  UIBounds prevBtn     = {80, 130, 40, 30};   // Center-Left
  UIBounds playBtn     = {140, 130, 40, 30};  // Dead Center
  UIBounds skipBtn     = {200, 130, 40, 30};  // Center-Right
  
  UIBounds currentTimeD = {0, 180, 50, 30};   // Far Left
  UIBounds progressBar  = {55, 190, 210, 10}; //  centered between the text
  UIBounds totalTimeD   = {270, 180, 50, 30}; // Far Right

  int r = 4; // Corner radius for the progress bar
  // -----------------------------------------------
  
  // Track Data
  int currentTime = 0;
  int totalTime = 1; // Initialized to 1 to prevent divide-by-zero crashes on boot
  String artist;
  String songTitle;

  // Sprite Pointers
  TFT_eSprite* progressBarSPR;
  TFT_eSprite* artistSPR;
  TFT_eSprite* songTitleSPR;
  TFT_eSprite* playSPR;
  TFT_eSprite* skipSPR; 
  TFT_eSprite* previousSPR;
  TFT_eSprite* thumbnailSPR;
  TFT_eSprite* currentTimeSPR;
  TFT_eSprite* totalTimeSPR;

  String currentTimeTxt = "";
  String totalTimeTxt = "";

  unsigned long previousMillis = 0;
  unsigned long interval = 1000;

  bool isPlaying = true;

  int currentSelection = 0;


  // --- Sidebar Variables ---
  bool sidebarOpen = false;
  int sidebarSelection = 0; 
  unsigned long lastActivityMillis = 0;
  const unsigned long sidebarTimeout = 4000;
  
  UIBounds sidebarD = {5, 5, 80, 230}; 
  TFT_eSprite* sidebarSPR;

  bool isTrackLoaded = false;

  public:
  mediaEditor(){
    progressBarSPR = new TFT_eSprite(&tft);
    artistSPR = new TFT_eSprite(&tft);
    songTitleSPR = new TFT_eSprite(&tft); 
    playSPR = new TFT_eSprite(&tft);
    skipSPR = new TFT_eSprite(&tft);      
    previousSPR = new TFT_eSprite(&tft);
    thumbnailSPR = new TFT_eSprite(&tft);
    currentTimeSPR = new TFT_eSprite(&tft);
    totalTimeSPR = new TFT_eSprite(&tft);
    sidebarSPR = new TFT_eSprite(&tft);
  }

  ~mediaEditor() {
    if (progressBarSPR != nullptr) { delete progressBarSPR; progressBarSPR = nullptr; }
    if (artistSPR != nullptr) { delete artistSPR; artistSPR = nullptr; }
    if (songTitleSPR != nullptr) { delete songTitleSPR; songTitleSPR = nullptr; }
    if (playSPR != nullptr) { delete playSPR; playSPR = nullptr; }
    if (skipSPR != nullptr) { delete skipSPR; skipSPR = nullptr; }
    if (previousSPR != nullptr) { delete previousSPR; previousSPR = nullptr; }
    if (thumbnailSPR != nullptr) { delete thumbnailSPR; thumbnailSPR = nullptr; }
    if (currentTimeSPR != nullptr) { delete currentTimeSPR; currentTimeSPR = nullptr; }
    if (totalTimeSPR != nullptr) { delete totalTimeSPR; totalTimeSPR = nullptr; }
    if (sidebarSPR != nullptr) { delete sidebarSPR; sidebarSPR = nullptr; }
  }

  int selection(int buttonDirection) {
    lastActivityMillis = millis();

    if (sidebarOpen) {
      if (buttonDirection == 1) { 
        currentSelection = 1; 
        closeSidebar(); // Erase menu safely
      } 
      else if (buttonDirection == 2) { 
        if (sidebarSelection > 0) sidebarSelection--;
        renderSidebar();
      } 
      else if (buttonDirection == 3) { 
        if (sidebarSelection < 2) sidebarSelection++; 
        renderSidebar();
      }
    } 
    else { 
      if (buttonDirection == 0 && currentSelection == 1) { 
        sidebarOpen = true; 
        sidebarSelection = 0; 
        renderSidebar();
      } 
      else if (buttonDirection == 0 && currentSelection > 1) { 
        currentSelection--;
        renderButtons();
      } 
      else if (buttonDirection == 1 && currentSelection < 3) { 
        currentSelection++;
        renderButtons();
      }
    }
    return currentSelection;
  }

  void updateTrackData(String newTitle, String newArtist, int newProgress, int newDuration, String newIsPlaying) {
    songTitle = newTitle;
    artist = newArtist;
    currentTime = newProgress;
    totalTime = newDuration;
    isTrackLoaded = true; // Flags that a song is active

    if(newIsPlaying == "PAUSED" || newIsPlaying == "Paused") {
      isPlaying = false;
    } else {
      isPlaying = true;
    }
  }

  void render(){
    renderProgressBar();
    renderArtist();
    renderSongTitle();
    renderButtons();
    renderThumbnail();
    if (sidebarOpen) renderSidebar();
  }

  void fillBackground(TFT_eSprite* canvas, int canvasX, int canvasY){
    canvas->setSwapBytes(true);
    canvas->pushImage(-canvasX, -canvasY, 320, 240, mainBackground);
  }

 void renderProgressBar() {
    if (totalTime <= 0) return; 

    // Draw the time text
    char timeBuf[10];
    sprintf(timeBuf, "%d:%02d", currentTime / 60, currentTime % 60);
    String currentStr = String(timeBuf);

    sprintf(timeBuf, "%d:%02d", totalTime / 60, totalTime % 60);
    String totalStr = String(timeBuf);

    // Only draw the Current Time text if the sidebar is closed (since the sidebar covers it)
    if (!sidebarOpen) {
      currentTimeSPR->createSprite(currentTimeD.w, currentTimeD.h);
      fillBackground(currentTimeSPR, currentTimeD.x, currentTimeD.y);
      currentTimeSPR->setTextDatum(MC_DATUM);
      currentTimeSPR->loadFont(BebasNeue_Regular21);
      currentTimeSPR->setTextColor(TFT_DARKGREY);
      currentTimeSPR->drawString(currentStr, currentTimeD.w/2, currentTimeD.h/2);
      currentTimeSPR->pushSprite(currentTimeD.x, currentTimeD.y);
      currentTimeSPR->deleteSprite();
    }

    // Always draw the Total Time text
    totalTimeSPR->createSprite(totalTimeD.w, totalTimeD.h);
    fillBackground(totalTimeSPR, totalTimeD.x, totalTimeD.y);
    totalTimeSPR->setTextDatum(MC_DATUM);
    totalTimeSPR->loadFont(BebasNeue_Regular21);
    totalTimeSPR->setTextColor(TFT_DARKGREY);
    totalTimeSPR->drawString(totalStr, totalTimeD.w/2, totalTimeD.h/2);
    totalTimeSPR->pushSprite(totalTimeD.x, totalTimeD.y);
    totalTimeSPR->deleteSprite();

    // Draw the progress bar with masking
    int percentageW = (currentTime * progressBar.w) / totalTime; 
    if (percentageW > 0 && percentageW < (r * 2)) percentageW = r * 2;

    int pX = progressBar.x;
    int pW = progressBar.w;
    int pOffset = 0;

    // The Masking Math: Shrink the sprite and calculate how much to shift the drawing left
    if (sidebarOpen) {
      pOffset = (sidebarD.x + sidebarD.w + 5) - progressBar.x; 
      if (pOffset > 0) {
        pX += pOffset;
        pW -= pOffset;
      } else {
        pOffset = 0;
      }
    }

    progressBarSPR->createSprite(pW, progressBar.h);
    fillBackground(progressBarSPR, pX, progressBar.y);

    // Draw the shapes shifted by -pOffset. The TFT will perfectly cut off the hidden portion!
    progressBarSPR->fillRoundRect(-pOffset, 0, progressBar.w, progressBar.h, r, TFT_DARKGREY);
    if (percentageW > 0) {
      progressBarSPR->fillRoundRect(-pOffset, 0, percentageW, progressBar.h, r, TFT_WHITE);
    }
    
    progressBarSPR->pushSprite(pX, progressBar.y);
    progressBarSPR->deleteSprite(); 
  }

  void renderArtist(){
    artistSPR->createSprite(artistTxt.w, artistTxt.h); 
    artistSPR->setTextDatum(MC_DATUM);
    artistSPR->setTextWrap(false);

    fillBackground(artistSPR, artistTxt.x, artistTxt.y);
    
    artistSPR->loadFont(BebasNeue_Regular21);
    artistSPR->setTextColor(TFT_NAVY); 
    artistSPR->drawString(artist, artistTxt.w/2, artistTxt.h/2); // Draw to center of sprite
    
    artistSPR->pushSprite(artistTxt.x, artistTxt.y); 
    artistSPR->deleteSprite();
  }

  void renderSongTitle(){
    songTitleSPR->createSprite(titleTxt.w, titleTxt.h);
    songTitleSPR->setTextDatum(MC_DATUM);
    songTitleSPR->setTextWrap(false);

    fillBackground(songTitleSPR, titleTxt.x, titleTxt.y);

    songTitleSPR->loadFont(BebasNeue_Regular21);
    songTitleSPR->setTextColor(TFT_NAVY); 
    songTitleSPR->drawString(songTitle, titleTxt.w/2, titleTxt.h/2);
    
    songTitleSPR->pushSprite(titleTxt.x, titleTxt.y); 
    songTitleSPR->deleteSprite();
  }

  void renderButtons(){

    playSPR->createSprite(playBtn.w, playBtn.h);
    skipSPR->createSprite(skipBtn.w, skipBtn.h);
    previousSPR->createSprite(prevBtn.w, prevBtn.h);

    playSPR->setTextDatum(MC_DATUM);
    skipSPR->setTextDatum(MC_DATUM);
    previousSPR->setTextDatum(MC_DATUM);

    playSPR->setTextWrap(false);
    skipSPR->setTextWrap(false);
    previousSPR->setTextWrap(false); 

    fillBackground(playSPR, playBtn.x, playBtn.y);
    fillBackground(skipSPR, skipBtn.x, skipBtn.y);
    fillBackground(previousSPR, prevBtn.x, prevBtn.y);

    playSPR->loadFont(BebasNeue_Regular21);
    skipSPR->loadFont(BebasNeue_Regular21);
    previousSPR->loadFont(BebasNeue_Regular21);

    playSPR->setTextColor(TFT_NAVY);
    skipSPR->setTextColor(TFT_NAVY);
    previousSPR->setTextColor(TFT_NAVY);

    if(currentSelection == 2) playSPR->setTextColor(TFT_WHITE);
    else if(currentSelection == 3) skipSPR->setTextColor(TFT_WHITE);
    else if(currentSelection == 1) previousSPR->setTextColor(TFT_WHITE);

    // Draw strings to the center of their respective sprites
    playSPR->drawString("|| / |>", playBtn.w/2, playBtn.h/2);
    skipSPR->drawString("-->", skipBtn.w/2, skipBtn.h/2);
    previousSPR->drawString("<--", prevBtn.w/2, prevBtn.h/2);

    // Push sprites using struct coordinates
    playSPR->pushSprite(playBtn.x, playBtn.y);
    skipSPR->pushSprite(skipBtn.x, skipBtn.y);
    previousSPR->pushSprite(prevBtn.x, prevBtn.y);
    
    playSPR->deleteSprite();
    skipSPR->deleteSprite();
    previousSPR->deleteSprite();
  }

  void renderThumbnail(){}

  void renderSidebar() {
    if (!sidebarOpen) return;

    sidebarSPR->createSprite(sidebarD.w, sidebarD.h);
    
    // Draw the background image FIRST, then draw the curved dark grey container on top
    fillBackground(sidebarSPR, sidebarD.x, sidebarD.y);
    sidebarSPR->fillRoundRect(0, 0, sidebarD.w, sidebarD.h, 10, TFT_DARKGREY); 
    
    sidebarSPR->setTextColor(TFT_WHITE);
    sidebarSPR->setTextDatum(MC_DATUM);
    sidebarSPR->loadFont(BebasNeue_Regular21);
    
    // Draw temporary numbers 
    for (int i = 0; i < 3; i++) {
      if (i == sidebarSelection) {
        sidebarSPR->fillRoundRect(10, 20 + (i * 70), 60, 60, 4, TFT_NAVY); // Highlight
      } else {
        sidebarSPR->fillRoundRect(10, 20 + (i * 70), 60, 60, 4, TFT_BLACK); 
      }
      
      char numStr[2];
      sprintf(numStr, "%d", i + 1);
      sidebarSPR->drawString(String(numStr), sidebarD.w / 2, 50 + (i * 70));
    }
    
    sidebarSPR->pushSprite(sidebarD.x, sidebarD.y);
    sidebarSPR->deleteSprite();
  }

  void closeSidebar() {
    sidebarOpen = false;
    
    // 1. Create a temporary patch exactly the size of the sidebar
    TFT_eSprite* patch = new TFT_eSprite(&tft);
    patch->createSprite(sidebarD.w, sidebarD.h);
    
    // 2. Draw the clean mainBackground onto this patch
    fillBackground(patch, sidebarD.x, sidebarD.y);
    
    // Push the clean background patch to the screen to erase the sidebar
    patch->pushSprite(sidebarD.x, sidebarD.y);
    
    // Safely delete the temporary patch
    patch->deleteSprite();
    delete patch;
    
    // Redraw the normal UI elements on top
    render(); 
  }

  void run() {
    unsigned long currentMillis = millis();

    // Use the safe eraser function when it times out
    if (sidebarOpen && (currentMillis - lastActivityMillis >= sidebarTimeout)) {
      closeSidebar(); 
    }

    if (currentTime >= totalTime) {
      currentTime = totalTime; 
    } 
    else if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis; 
      
      // Only tick the time forward and redraw if the music is actually playing!
      if (isPlaying) {
        currentTime++; 
        renderProgressBar();
      }
    }
  }

  bool isSidebarOpen() { return sidebarOpen; }
  int getSidebarSelection() { return sidebarSelection; }
  int getSelection() { return currentSelection; }
  bool hasActiveMedia() { return isTrackLoaded; }
  void clearActiveMedia() { isTrackLoaded = false; }

  // Safely delete all pointers to prevent total system memory leak
  void clearScreen() {
    sidebarOpen = false;
  }

};