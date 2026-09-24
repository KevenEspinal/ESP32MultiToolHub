#define USE_NIMBLE
#include <TFT_eSPI.h>
#include <ESP32Encoder.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TJpg_Decoder.h>
#include <vector>
#include "volumeHeader.h"
#include "navigation.h"
#include "menu.h"
#include "apps.h"
#include "timeApp.h"
#include "linksApp.h"
#include "playlist.h"
#include "diagnosticsApp.h"
#include "mediaEditor.h"
#include "secrets.h"
#include "weatherApp.h"
#include "bleManager.h"
#include "theme.h"
#include "mbedtls/base64.h"

  // Replace these with your actual Wi-Fi network and password
  const char* ssid       = SECRET_SSID;
  const char* password   = SECRET_PASS;

  // NTP Server and Eastern Time Zone Settings
  const char* ntpServer          = "pool.ntp.org";
  const long  gmtOffset_sec      = -18000; // -5 hours (EST) in seconds
  const int   daylightOffset_sec = 3600;   // +1 hour for EDT

  // Declare all objects for later use, like pin numbers for encoder (regular button pins are setup internally), TFT screen, and apps
  TFT_eSPI tft = TFT_eSPI(); 

  #define rotaryEncoderButton 32
  #define rotaryEncoderPin1 25
  #define rotaryEncoderPin2 33
  ESP32Encoder encoder;

  volumeHeader vol;
  navigation navigator;

  menu menuObject;
  mediaEditor mediaScreen;
  app appsInfo;
  ClockApp clockApp("Clock");
  linksApp links("Links");
  playlistApp playlist("Playlists");
  diagnosticsApp diagnostics("Diagnostics");
  weatherApp weather("Weather");
  
  bool needDiagnostics = false;

  const int MAX_ART_SIZE = 8192;
  char artBuffer[MAX_ART_SIZE];
  int artIndex = 0;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  return mediaScreen.decodeArt(x, y, w, h, bitmap);
}

void fetchAlbumArt(String artist, String title) {
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;

  String query = artist + "+" + title;
  query.replace(" ", "+");
  
  String url = "https://itunes.apple.com/search?term=" + query + "&entity=song&limit=1";
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    int urlStart = payload.indexOf("\"artworkUrl100\":\"");
    if (urlStart != -1) {
      urlStart += 17; 
      int urlEnd = payload.indexOf("\"", urlStart);
      String imageUrl = payload.substring(urlStart, urlEnd);
      
      imageUrl.replace("100x100bb.jpg", "120x120bb.jpg");
      
      // Close the search request before reusing the client for the image.
      // Calling begin() again while the previous connection is still open
      // leaves it dangling and the second GET can fail outright.
      http.end();

      http.begin(client, imageUrl);
      int imgHttpCode = http.GET();
      if (imgHttpCode == 200) {
        int len = http.getSize();
        if (len > 0) {
          uint8_t* imgBuffer = (uint8_t*)malloc(len);
          if (imgBuffer != nullptr) {
            WiFiClient *imgClient = http.getStreamPtr();
            int bytesRead = 0;
            unsigned long lastByteTime = millis();

            // The old loop had no timeout, so a server that went quiet
            // while still "connected" hung the entire device.
            while (bytesRead < len && millis() - lastByteTime < 5000) {
              size_t avail = imgClient->available();
              if (avail) {
                // Never read past the end of the allocation: available()
                // can report more than the space left, and readBytes()
                // would walk straight off the end of the heap block.
                if (avail > (size_t)(len - bytesRead)) avail = (size_t)(len - bytesRead);
                int c = imgClient->readBytes(&imgBuffer[bytesRead], avail);
                bytesRead += c;
                lastByteTime = millis();
              } else if (!imgClient->connected()) {
                break;                          // hung up rather than stalled
              } else {
                delay(1);
              }
            }

            if (bytesRead == len) {
              mediaScreen.setAlbumArt(imgBuffer, len);  // takes ownership
            } else {
              // A half-downloaded JPEG is worse than none, and the old code
              // both fed it to the decoder and leaked it on the failure path.
              free(imgBuffer);
            }
          }
        }
      }
    }
  }
  http.end();
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setup() {
  Serial.setRxBufferSize(1024);
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  setupBLE();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);

  // --- LOADING SCREEN ---
  tft.fillScreen(UI_BG);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(FONT_HERO_LG);
  tft.setTextColor(UI_ACCENT);
  tft.drawString("DO-ALL-INATOR", 160, 96);
  tft.unloadFont();
  tft.loadFont(FONT_UI_SM);
  tft.setTextColor(UI_TEXT_MUTED);
  tft.drawString("Connecting to Wi-Fi...", 160, 140);
  tft.unloadFont();

  // --- WI-FI & TIME SYNC FIX ---
  WiFi.begin(ssid, password);
  
  // Timeout counter
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 20 attempts * 500ms = 10 seconds
    delay(500);
    attempts++;
  }

  // Only try to get the time if the Wi-Fi actually connected
  if (WiFi.status() == WL_CONNECTED) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      struct tm timeinfo;
      int timeAttempts = 0;
      // Also add a timeout to the time fetch, just in case
      while (!getLocalTime(&timeinfo) && timeAttempts < 10) { 
        delay(500);
        timeAttempts++;
      }
  } else {
      // Optional: Print a message to the Serial monitor if it failed
      Serial.println("Wi-Fi Connection Failed. Clock will be incorrect.");
  }

  // Brief on-screen confirmation of the result before handing off to the
  // menu, so a failed connection isn't silently invisible to the user.
  tft.loadFont(FONT_UI_SM);
  tft.setTextDatum(MC_DATUM);
  tft.fillRect(0, 128, 320, 24, UI_BG);
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? UI_GOOD : UI_BAD);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "Wi-Fi Connected" : "Wi-Fi Unavailable", 160, 140);
  tft.unloadFont();
  delay(600);
  
  // Safely turn off Wi-Fi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  // -----------------------------

  // Re-draw the background to erase the loading message
  tft.fillScreen(UI_BG); 

  menuObject.setup(appsInfo.getNamesList());
  vol.setupSprite();
  navigator.setupButtons();
  
  encoder.attachHalfQuad(rotaryEncoderPin1, rotaryEncoderPin2);
  encoder.setCount(0);
  pinMode(rotaryEncoderButton, INPUT_PULLUP);
}

int currentMode = 0;  // 1 = clock app   2 = links app   3 = playlist app   4 = media editor mode   5 = diagnostics app   6 = weather app

void handleSerialInput() {
  if (Serial.available() == 0) return; 

  char incomingMsg[1024];
  size_t len = Serial.readBytesUntil('\n', incomingMsg, sizeof(incomingMsg) - 1);
  incomingMsg[len] = '\0';
  
  while(len > 0 && (incomingMsg[len-1] == '\r' || incomingMsg[len-1] == ' ')){
    incomingMsg[len-1] = '\0';
    len--;
  }

  char* command = strtok(incomingMsg, "|");
  if (!command) return;

  if (strcmp(command, "MEDIA") == 0) {
    artIndex = 0;
    artBuffer[0] = '\0';

    char* songName = strtok(NULL, "|");
    char* artistName = strtok(NULL, "|");
    char* progressSecStr = strtok(NULL, "|");
    char* durationSecStr = strtok(NULL, "|");
    char* state = strtok(NULL, "|");

    if (songName && artistName && progressSecStr && durationSecStr && state) {
      int progressSec = atoi(progressSecStr);
      int durationSec = atoi(durationSecStr);

      mediaScreen.updateTrackData(String(songName), String(artistName), progressSec, durationSec, String(state));

      if (currentMode == 0 || currentMode == 1 || currentMode == 3) {
        if (currentMode == 0) menuObject.clearMenu();
        if (currentMode == 1) clockApp.clearScreen(); 
        if (currentMode == 3) playlist.clearScreen(); 
        
        tft.fillScreen(TFT_BLACK);
        currentMode = 4;
        mediaScreen.render(); 
      } 
      // Explicitly tell it to redraw
      else if (currentMode == 4) {
        mediaScreen.render();
      }
    }
  } else if (strcmp(command, "DIAG") == 0 && needDiagnostics) {
    char* cpuStr = strtok(NULL, "|");
    char* ramStr = strtok(NULL, "|");
    char* gpuStr = strtok(NULL, "|");

    if (cpuStr && ramStr && gpuStr) {
      diagnostics.updateHostStats(atoi(cpuStr + 4), atoi(ramStr + 4), atoi(gpuStr + 4));
    }
  } else if (strcmp(command, "WEATHER") == 0) {
    // There are 27 total delineators incoming from a WEATHER type payload
    // Stores all indeces of delineators from payload
    char* currentConditionStr = strtok(NULL, "|");
    char* currentTempStr = strtok(NULL, "|");

    if (currentConditionStr && currentTempStr) {
      // Independently store data for current time as it messes up the module math in the loop
      String currentCondition = String(currentConditionStr);
      int currentTemp = atoi(currentTempStr);

      // Declare arrays to store data for all times after the current time.
      // temperatures[] is zero-initialised: a short payload used to leave
      // uninitialised stack values in the unfilled slots, and all 8 were
      // handed to the weather app regardless of how many actually parsed.
      String conditions[8];
      int temperatures[8] = {0};
      String times[8];

      int dataStorageIndex = 0;

      for (int i = 0; i < 24; i++) {
        char* extractedData = strtok(NULL, "|");
        if (!extractedData) break;

        if (i % 3 == 0) {
          times[dataStorageIndex] = String(extractedData);
        } else if (i % 3 == 1) {
          temperatures[dataStorageIndex] = atoi(extractedData);
        } else if (i % 3 == 2) {
          conditions[dataStorageIndex] = String(extractedData);
          dataStorageIndex++;
          if (dataStorageIndex >= 8) break;
        }
      }
      // Pass how many hours actually arrived, not a hard-coded 8.
      weather.updateData(times, temperatures, conditions, dataStorageIndex);
    }
  } else if (strcmp(command, "VOLUME") == 0) {
    // Lets a connected PC report its actual system volume so the bar
    // reflects reality instead of only ever tracking the encoder's own
    // raw position. Expected format: "VOLUME|<0-100>", e.g. "VOLUME|75".
    // NOTE: this is a new command this sketch didn't previously handle —
    // the PC-side companion app needs to actually send it for this to
    // do anything; adjust the format here to match if it already sends
    // volume some other way.
    char* volStr = strtok(NULL, "|");
    if (volStr) {
      vol.updateHostVolume(atoi(volStr));
    }
  } else if (strcmp(command, "ART") == 0) {
    char* chunk = incomingMsg + 4;
    int chunkLen = strlen(chunk);
    
    // Prevent buffer overflows by ensuring the chunk fits in the array
    if (artIndex + chunkLen < MAX_ART_SIZE) {
      strcpy(artBuffer + artIndex, chunk);
      artIndex += chunkLen;
    }
  } else if (strcmp(command, "ART_END") == 0) {
    if (artIndex > 0) {
      size_t outputLen = 0;
      unsigned char* decodedData = (unsigned char*)malloc(artIndex);
      
      if (decodedData != nullptr) {
        int err = mbedtls_base64_decode(decodedData, artIndex, &outputLen, (const unsigned char*)artBuffer, artIndex);
        
        if (err == 0 && outputLen > 4 && decodedData[0] == 0xFF && decodedData[1] == 0xD8 && decodedData[outputLen-2] == 0xFF && decodedData[outputLen-1] == 0xD9) {
          mediaScreen.setAlbumArt(decodedData, outputLen);
          
          if (currentMode == 4) {
            mediaScreen.render();
          }
        } else {
          free(decodedData);
        }
      }
      
      artIndex = 0; 
      artBuffer[0] = '\0'; 
    }
  } else if (strcmp(command, "IDLE") == 0) {
    mediaScreen.clearActiveMedia(); 
    mediaScreen.setAlbumArt(nullptr, 0); 
    
    if (currentMode == 4) {
      mediaScreen.clearScreen();
      tft.fillScreen(TFT_BLACK);
      menuObject.setup(appsInfo.getNamesList());
      currentMode = 0;
    }
  }
}

void loop() {
  static String lastTrack = "";

// --- IPHONE MODE ---
  // Resets connetion status, clears media data sprites and resets back to menu screen
  if (phoneDisconnected) {
    phoneDisconnected = false;
    mediaScreen.clearActiveMedia();
    if (currentMode == 4) {
      mediaScreen.clearScreen();
      tft.fillScreen(TFT_BLACK);
      menuObject.setup(appsInfo.getNamesList());
      currentMode = 0;
    }
  }

  // Runs only after custom_gap_cb runs and resets Data Updated status. Updates variables of media screen, if user is outside of media editor screen then the media editor screen opens and pushes the sprites
  if (amsDataUpdated) {
    amsDataUpdated = false;
    mediaScreen.updateTrackData(amsTitle, amsArtist, (int)amsProgress, amsDuration, amsState);
    
    if (currentMode == 0 || currentMode == 1 || currentMode == 3) {
      if (currentMode == 0) menuObject.clearMenu();
      if (currentMode == 1) clockApp.clearScreen(); 
      if (currentMode == 3) playlist.clearScreen(); 
      
      tft.fillScreen(TFT_BLACK);
      currentMode = 4;
      mediaScreen.render(); 
    } 
    else if (currentMode == 4) {
      mediaScreen.render();
    }

    if (amsTitle != lastTrack && amsTitle != "") {
      lastTrack = amsTitle;
      fetchAlbumArt(amsArtist, amsTitle);
    }
  }

  runBLEStateMachine();

  int buttonPressed = navigator.navigate();
  int clickState = navigator.checkClick(rotaryEncoderButton); 

// --- UNIVERSAL EXIT (DOUBLE CLICK) ---
  if (clickState == 2) {
    needDiagnostics = false;
    if (currentMode == 1) clockApp.clearScreen();
    else if (currentMode == 2) links.clearScreen();
    else if (currentMode == 3) playlist.clearScreen();
    else if (currentMode == 4) mediaScreen.clearScreen();
    else if (currentMode == 5) diagnostics.clearScreen();
    else if (currentMode == 6) weather.clearScreen();

    tft.fillScreen(TFT_BLACK);
    
    // ROUTING LOGIC
    if (mediaScreen.hasActiveMedia() && currentMode != 4) {
      currentMode = 4;
      mediaScreen.render();
    } else {
      menuObject.setup(appsInfo.getNamesList()); 
      currentMode = 0; 
    }
    
    return; 
  }

  handleSerialInput();

// --- MEDIA EDITOR MODE ---
  if (currentMode == 4) {
    
    // Process physical button scrolls
    if (buttonPressed != -1) { 
      mediaScreen.selection(buttonPressed);
    }

    // ENCODER CLICK 
    if (clickState == 1) {
      
      // SIDEBAR APP LAUNCH LOGIC
      if (mediaScreen.isSidebarOpen()) {
        int appToLaunch = mediaScreen.getSidebarSelection();
        
        // Free the Media Editor's RAM and wipe the screen
        mediaScreen.clearScreen();
        tft.fillScreen(TFT_BLACK);
        
        // Load the newly selected internal app
        if (appToLaunch == 0) {
          currentMode = 1;
          clockApp.loadScreen();    
          clockApp.loadSprites();   
        }
        else if (appToLaunch == 1) {
          currentMode = 2;
          links.loadScreen();
        }
        else if (appToLaunch == 2) {
          currentMode = 3;
          playlist.loadScreen();
        }
        else if (appToLaunch == 3) { 
          currentMode = 5;
          needDiagnostics = true;
          handleSerialInput();
          diagnostics.loadScreen();
        }else if (appToLaunch == 4) { 
          currentMode = 6;
          weather.loadScreen();
        }
      } 
      
      // MEDIA CONTROL LOGIC (Sidebar Closed)
      else {
        int currentHover = mediaScreen.getSelection(); 
        if (currentHover == 1) {
          Serial.println("MEDIA_PREVIOUS");
          sendMediaCommand(4);
        }
        else if (currentHover == 2) {
          Serial.println("MEDIA_PLAY_PAUSE");
          sendMediaCommand(2);
        }
        else if (currentHover == 3) {
          Serial.println("MEDIA_NEXT");
          sendMediaCommand(3);
        }
      }
    }

    // Keep timer ticking
    mediaScreen.run();
  }
  

// --- MENU MODE ---
  else if (currentMode == 0) {      // Check if we're in menu mode
    if (buttonPressed != -1) {      // Check if a button has been preseed
      menuObject.scroll(buttonPressed);
    }
    menuObject.animate();
    
    if (clickState == 1) { // SINGLE CLICK, meaning the user has selected something
      int selectedApp = menuObject.appSelection(); 
      menuObject.clearMenu(); 

      // Checks what app the user selected
      if (selectedApp == 0) {
        currentMode = 1;
        clockApp.loadScreen();     
        clockApp.loadSprites();    
      } else if (selectedApp == 1) {
        currentMode = 2;
        links.loadScreen();     
      } else if (selectedApp == 2) { 
        currentMode = 3;
        playlist.loadScreen();
      } else if (selectedApp == 3) {
        currentMode = 5;
        needDiagnostics = true;
        diagnostics.loadScreen();
      } else if (selectedApp == 4) {
        currentMode = 6;
        weather.loadScreen();
      }
    }
  }
  
// --- CLOCK APP MODE ---
  else if (currentMode == 1) {
    clockApp.run(); 
  }
  
// --- LINKS APP MODE ---
  else if(currentMode == 2) {
    if (buttonPressed != -1) {
      links.scroll(buttonPressed);
    }
    links.run();

    if (clickState == 1) { // SINGLE CLICK
      int selectedLink = links.linkSelection();
      
      if (selectedLink == 0) Serial.println("LAUNCH_YT");
      else if (selectedLink == 1) 
      {
        Serial.println("LAUNCH_GEMINI");
      }
      else if (selectedLink == 2) 
      {
        Serial.println("LAUNCH_GITHUB");
      }
      else if (selectedLink == 3) 
      {
        Serial.println("LAUNCH_DESMOS");
      }
      else if (selectedLink == 4) 
      {
        Serial.println("LAUNCH_SPOTIFY");
      }
    }
  }

// --- PLAYLIST APP MODE ---
  else if(currentMode == 3){
    if(buttonPressed != -1){
      playlist.scroll(buttonPressed);
    }
    playlist.run();

    if(clickState == 1) { 
      int selectedPlaylist = playlist.playlistSelection();

      if(selectedPlaylist == 0) Serial.println("LAUNCH_PLAYLIST_#1");
      else if(selectedPlaylist == 1) Serial.println("LAUNCH_PLAYLIST_#2");
    }
  }

// --- DIAGNOSTICS APP MODE ---
  else if(currentMode == 5){
    if (buttonPressed != -1) {
      diagnostics.scroll(buttonPressed);
    }
    diagnostics.run();
  }

// --- WEATHER APP MODE ---
  else if(currentMode == 6){
    if (buttonPressed != -1) {
      weather.scroll(buttonPressed);
    }
    weather.run(); // Currently does nothing, in the future it will load the icons corresponding to each hour
  }

// --- VOLUME OVERLAY ---
  // Composited LAST, after the active app has taken its turn to draw. It
  // used to run at the top of loop(), so a menu scroll or a marquee tick
  // painted straight over the bar a fraction of a second later.
  //
  // Takes the raw encoder count (the divide by two now happens inside the
  // class, where it can be floored instead of truncated toward zero), and
  // returns true for exactly one frame: the moment the bar has finished
  // sliding away and wiped its own column. That is the app's cue to repaint.
  if (vol.update(encoder.getCount())) {
    switch (currentMode) {
      case 0: menuObject.renderMenu(); break;
      case 1: clockApp.loadSprites(); break;
      case 2: links.renderSelectionScreen(); break;
      case 3: playlist.renderSelectionScreen(); break;
      case 4: mediaScreen.render(); break;

      // Was diagnostics.loadScreen(), which resets the tab back to CPU and
      // wipes the whole screen — so nudging the volume while reading the
      // Wi-Fi tab threw you back to CPU and strobed the display.
      case 5: diagnostics.redraw(); break;

      case 6:
        if (weather.isDataLoaded()) {
          weather.updateUI();
        } else {
          weather.loadScreen(); // Keep the "Collecting Data..." screen alive
        }
        break;
    }
  }
}
