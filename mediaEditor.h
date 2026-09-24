#pragma once
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "background.h" 
#include "theme.h" 

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
  
  UIBounds titleTxt    = {140, 20, 170, 40};  // Top Right
  UIBounds artistTxt   = {140, 70, 170, 30};  // Middle Right
  UIBounds thumbnail   = {10, 20, 120, 120};    // Top Left

  UIBounds prevBtn     = {80, 150, 40, 30};   // Center-Left
  UIBounds playBtn     = {140, 150, 40, 30};  // Dead Center
  UIBounds skipBtn     = {200, 150, 40, 30};  // Center-Right
  
  UIBounds currentTimeD = {0, 190, 50, 30};   // Far Left
  UIBounds progressBar  = {55, 200, 210, 10}; //  centered between the text
  UIBounds totalTimeD   = {270, 190, 50, 30}; // Far Right

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

  // --- Title / artist marquee ---
  // Text that's too wide for its sprite holds on the start, scrolls once
  // to reveal the rest, holds on the end, then loops back — never a
  // constant crawl. Text that already fits just draws statically and
  // never enters this state machine at all.
  struct MarqueeState {
    int phase = 0;             // 0 = hold start, 1 = scrolling, 2 = hold end
    unsigned long phaseStart = 0;
    int offset = 0;            // current negative x pixel offset
    bool spriteReady = false;  // sprite is created once and reused, not per-frame

    void reset() {
      phase = 0;
      phaseStart = millis();
      offset = 0;
    }
  };
  MarqueeState titleMarquee;
  MarqueeState artistMarquee;
  unsigned long previousMarqueeMillis = 0;
  const unsigned long marqueeInterval = 60; // ~16fps, smooth enough for a slow crawl

  // Shared renderer for both the title and artist sprites. Handles the
  // hold/scroll/hold timing itself; callers just say what text and font
  // to use. The sprite is created once (kept alive, unlike most of this
  // class's other transient sprites) since this redraws far more often
  // than a button or a time label ever did.
  void renderMarqueeText(TFT_eSprite* spr, UIBounds bounds, const String &text, uint16_t color, MarqueeState &state, const uint8_t* font) {
    if (!state.spriteReady) {
      spr->createSprite(bounds.w, bounds.h);
      state.spriteReady = true;
    }
    fillBackground(spr, bounds.x, bounds.y);
    spr->setTextWrap(false);
    spr->loadFont(font);
    spr->setTextColor(color);

    int textW = spr->textWidth(text);

    if (textW <= bounds.w - 4) {
      // Fits comfortably — simple static, centered, exactly as before.
      spr->setTextDatum(MC_DATUM);
      spr->drawString(text, bounds.w / 2, bounds.h / 2);
    } else {
      const unsigned long HOLD_START_MS = 3000;
      const unsigned long SCROLL_MS     = 3500; // within the requested 3-4s
      const unsigned long HOLD_END_MS   = 1200;

      int maxOffset = textW - bounds.w + 8; // small margin so the last letter fully clears
      unsigned long elapsed = millis() - state.phaseStart;

      if (state.phase == 0) {                 // holding at the start
        state.offset = 0;
        if (elapsed >= HOLD_START_MS) { state.phase = 1; state.phaseStart = millis(); }
      } else if (state.phase == 1) {           // scrolling to the end
        float t = (float)elapsed / (float)SCROLL_MS;
        if (t >= 1.0f) { t = 1.0f; state.phase = 2; state.phaseStart = millis(); }
        state.offset = (int)(-maxOffset * t);
      } else {                                 // holding at the end
        state.offset = -maxOffset;
        if (elapsed >= HOLD_END_MS) { state.phase = 0; state.phaseStart = millis(); state.offset = 0; }
      }

      spr->setTextDatum(ML_DATUM);
      spr->drawString(text, state.offset, bounds.h / 2);
    }

    spr->pushSprite(bounds.x, bounds.y);
  }

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

  uint8_t* albumBuffer = nullptr;
  size_t albumSize = 0;
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
    titleMarquee.reset();
    artistMarquee.reset();
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
    if (albumBuffer != nullptr) { free(albumBuffer); albumBuffer = nullptr; }
  }

  int selection(int buttonDirection) {
    lastActivityMillis = millis();

    if (sidebarOpen) {
      if (buttonDirection == 1) { 
        currentSelection = 1; 
        closeSidebar(); // Erase menu 
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
    // A genuinely new title/artist restarts that marquee from the
    // beginning instead of picking up mid-scroll from the last song.
    if (newTitle != songTitle) titleMarquee.reset();
    if (newArtist != artist) artistMarquee.reset();

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
    canvas->fillSprite(UI_BG);
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
      currentTimeSPR->loadFont(FONT_UI_SM);
      currentTimeSPR->setTextColor(UI_TEXT_MUTED);
      currentTimeSPR->drawString(currentStr, currentTimeD.w/2, currentTimeD.h/2);
      currentTimeSPR->pushSprite(currentTimeD.x, currentTimeD.y);
      currentTimeSPR->deleteSprite();
    }

    // Always draw the Total Time text
    totalTimeSPR->createSprite(totalTimeD.w, totalTimeD.h);
    fillBackground(totalTimeSPR, totalTimeD.x, totalTimeD.y);
    totalTimeSPR->setTextDatum(MC_DATUM);
    totalTimeSPR->loadFont(FONT_UI_SM);
    totalTimeSPR->setTextColor(UI_TEXT_MUTED);
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
    progressBarSPR->fillRoundRect(-pOffset, 0, progressBar.w, progressBar.h, r, UI_TEXT_MUTED);
    if (percentageW > 0) {
      progressBarSPR->fillRoundRect(-pOffset, 0, percentageW, progressBar.h, r, UI_ACCENT);
    }
    
    progressBarSPR->pushSprite(pX, progressBar.y);
    progressBarSPR->deleteSprite(); 
  }

  void renderArtist(){
    // Dimmed while the sidebar is open, to keep focus on the app switcher
    uint16_t color = sidebarOpen ? UI_TEXT_GHOST : UI_TEXT_MUTED;
    renderMarqueeText(artistSPR, artistTxt, artist, color, artistMarquee, FONT_UI_SM);
  }

  void renderSongTitle(){
    // The title is the most important thing on this screen, so it gets
    // full-strength text rather than the muted navy the rest once used.
    uint16_t color = sidebarOpen ? UI_TEXT_GHOST : UI_TEXT;
    renderMarqueeText(songTitleSPR, titleTxt, songTitle, color, titleMarquee, FONT_UI_SM);
  }

  void renderButtons(){

    playSPR->createSprite(playBtn.w, playBtn.h);
    skipSPR->createSprite(skipBtn.w, skipBtn.h);
    previousSPR->createSprite(prevBtn.w, prevBtn.h);

    fillBackground(playSPR, playBtn.x, playBtn.y);
    fillBackground(skipSPR, skipBtn.x, skipBtn.y);
    fillBackground(previousSPR, prevBtn.x, prevBtn.y);

    uint16_t playColor = (currentSelection == 2) ? UI_ACCENT : UI_TEXT_MUTED;
    uint16_t skipColor = (currentSelection == 3) ? UI_ACCENT : UI_TEXT_MUTED;
    uint16_t prevColor = (currentSelection == 1) ? UI_ACCENT : UI_TEXT_MUTED;

    // Real vector glyphs instead of ASCII stand-ins ("|| / |>", "-->", "<--").

    // Previous: bar + left-pointing triangle
    previousSPR->fillRect(11, 8, 3, 14, prevColor);
    previousSPR->fillTriangle(27, 8, 27, 22, 14, 15, prevColor);

    // Play / pause — drawn straight from the real isPlaying state, so the
    // icon always matches what tapping it will actually do next.
    if (isPlaying) {
      playSPR->fillRect(14, 7, 6, 16, playColor);
      playSPR->fillRect(22, 7, 6, 16, playColor);
    } else {
      playSPR->fillTriangle(14, 7, 14, 23, 28, 15, playColor);
    }

    // Skip: right-pointing triangle + bar
    skipSPR->fillTriangle(13, 8, 13, 22, 26, 15, skipColor);
    skipSPR->fillRect(26, 8, 3, 14, skipColor);

    // Push sprites using struct coordinates
    playSPR->pushSprite(playBtn.x, playBtn.y);
    skipSPR->pushSprite(skipBtn.x, skipBtn.y);
    previousSPR->pushSprite(prevBtn.x, prevBtn.y);
    
    playSPR->deleteSprite();
    skipSPR->deleteSprite();
    previousSPR->deleteSprite();
  }

  bool decodeArt(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    thumbnailSPR->pushImage(x, y, w, h, bitmap);
    return 1;
  }

  void setAlbumArt(uint8_t* imageData, size_t imageSize){
    if (albumBuffer != nullptr) {
      free(albumBuffer);
    }
    albumBuffer = imageData;
    albumSize = imageSize;
  }

  void renderThumbnail(){
    thumbnailSPR->createSprite(thumbnail.w, thumbnail.h);

    if (albumBuffer != nullptr) {
      thumbnailSPR->setColorDepth(16);
      thumbnailSPR->setSwapBytes(true);
      // Draw the JPEG at (0,0) inside the sprite
      TJpgDec.drawJpg(0, 0, albumBuffer, albumSize);
    } else {
      // No art fetched yet — a quiet placeholder instead of a blank hole
      fillBackground(thumbnailSPR, thumbnail.x, thumbnail.y);
      thumbnailSPR->fillRoundRect(0, 0, thumbnail.w, thumbnail.h, UI_RADIUS_MD, UI_SURFACE);
      thumbnailSPR->drawRoundRect(0, 0, thumbnail.w, thumbnail.h, UI_RADIUS_MD, UI_TEXT_GHOST);
      uiIconPlaylist(thumbnailSPR, thumbnail.w / 2, thumbnail.h / 2, thumbnail.w / 2, thumbnail.h / 3, UI_TEXT_GHOST);
    }

    // Push the completed sprite to the physical screen
    thumbnailSPR->pushSprite(thumbnail.x, thumbnail.y);
    thumbnailSPR->deleteSprite();
  }

  void renderSidebar() {
    if (!sidebarOpen) return;

    sidebarSPR->createSprite(sidebarD.w, sidebarD.h);
    
    // Draw the background FIRST, then the panel on top of it
    fillBackground(sidebarSPR, sidebarD.x, sidebarD.y);
    sidebarSPR->fillRoundRect(0, 0, sidebarD.w, sidebarD.h, UI_RADIUS_LG, UI_SURFACE);
    sidebarSPR->drawRoundRect(0, 0, sidebarD.w, sidebarD.h, UI_RADIUS_LG, UI_BORDER);

    // These 3 slots map to the same 3 quick-launch apps selection() already
    // routes to below (Clock / Links / Playlist) — same behaviour as
    // before, just icons instead of bare "1 / 2 / 3" placeholders.
    for (int i = 0; i < 3; i++) {
      int slotY = 20 + (i * 70);
      int cy = slotY + 30;
      int cx = sidebarD.w / 2;
      bool active = (i == sidebarSelection);
      uint16_t fg = active ? UI_ACCENT : UI_TEXT_MUTED;

      if (active) {
        sidebarSPR->fillRoundRect(10, slotY, 60, 60, UI_RADIUS_MD, UI_ACCENT_GLOW);
      } else {
        sidebarSPR->drawRoundRect(10, slotY, 60, 60, UI_RADIUS_MD, UI_TEXT_GHOST);
      }

      if (i == 0) uiIconClock(sidebarSPR, cx, cy, 15, fg);
      else if (i == 1) uiIconLink(sidebarSPR, cx, cy, 15, fg);
      else uiIconPlaylist(sidebarSPR, cx, cy, 26, 20, fg);
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

    // Advance the title/artist marquees on their own faster tick so a
    // scroll pass actually looks smooth rather than jumping once a second.
    if (currentMillis - previousMarqueeMillis >= marqueeInterval) {
      previousMarqueeMillis = currentMillis;
      renderSongTitle();
      renderArtist();
    }
  }

  bool isSidebarOpen() { return sidebarOpen; }
  int getSidebarSelection() { return sidebarSelection; }
  int getSelection() { return currentSelection; }
  bool hasActiveMedia() { return isTrackLoaded; }
  void clearActiveMedia() { isTrackLoaded = false; }

  // Delete all pointers to prevent memory leak
  void clearScreen() {
    sidebarOpen = false;
    // Title/artist sprites are kept alive across frames (unlike this
    // class's other, transient sprites) so the marquee can redraw ~16x/s
    // without reallocating — free them here since every other sprite in
    // this class already frees on the way out.
    if (titleMarquee.spriteReady) { songTitleSPR->deleteSprite(); titleMarquee.spriteReady = false; }
    if (artistMarquee.spriteReady) { artistSPR->deleteSprite(); artistMarquee.spriteReady = false; }
  }

};
