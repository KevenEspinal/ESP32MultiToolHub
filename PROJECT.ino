#include <TFT_eSPI.h>
#include <ESP32Encoder.h> 
#include <WiFi.h>
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



  // Replace these with your actual Wi-Fi network and password
  const char* ssid       = SECRET_SSID;
  const char* password   = SECRET_PASS;

  // NTP Server and Eastern Time Zone Settings
  const char* ntpServer          = "pool.ntp.org";
  const long  gmtOffset_sec      = -18000; // -5 hours (EST) in seconds
  const int   daylightOffset_sec = 3600;   // +1 hour for EDT

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



void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  // --- LOADING SCREEN ---
  tft.pushImage(0, 0, 320, 240, mainBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Connecting to Wi-Fi...", 160, 120, 4); // Draw a simple message

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
  
  // Safely turn off Wi-Fi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  // -----------------------------

  // Re-draw the clean background to erase the loading message
  tft.pushImage(0, 0, 320, 240, mainBackground); 

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

  String incomingMsg = Serial.readStringUntil('\n');
  incomingMsg.trim(); 

  int firstDelimiter = incomingMsg.indexOf('|');
  String command = incomingMsg.substring(0, firstDelimiter);

  if (command == "MEDIA") {
    int secondDelim = incomingMsg.indexOf('|', firstDelimiter + 1);
    int thirdDelim  = incomingMsg.indexOf('|', secondDelim + 1);
    int fourthDelim = incomingMsg.indexOf('|', thirdDelim + 1);
    int fifthDelim  = incomingMsg.indexOf('|', fourthDelim + 1);

    String songName   = incomingMsg.substring(firstDelimiter + 1, secondDelim);
    String artistName = incomingMsg.substring(secondDelim + 1, thirdDelim);
    int progressSec   = incomingMsg.substring(thirdDelim + 1, fourthDelim).toInt();
    int durationSec   = incomingMsg.substring(fourthDelim + 1, fifthDelim).toInt();
    String state      = incomingMsg.substring(fifthDelim + 1);

    mediaScreen.updateTrackData(songName, artistName, progressSec, durationSec, state);

    if (currentMode == 0 || currentMode == 1 || currentMode == 3) {
      if (currentMode == 0) menuObject.clearMenu();
      if (currentMode == 1) clockApp.clearScreen();
      if (currentMode == 3) playlist.clearScreen();
      
      tft.pushImage(0, 0, 320, 240, mainBackground);
      currentMode = 4;
      mediaScreen.render(); 
    } 
    // NEW: If we are already on the media screen, explicitly tell it to redraw
    else if (currentMode == 4) {
      mediaScreen.render();
    }
  } 
  else if (command == "IDLE") {
    mediaScreen.clearActiveMedia(); // Erase the active flag
    
    if (currentMode == 4) {
      mediaScreen.clearScreen();
      tft.pushImage(0, 0, 320, 240, mainBackground);
      menuObject.setup(appsInfo.getNamesList());
      currentMode = 0;
    }
  }
}

void loop() {
  vol.calculateVolume(encoder.getCount() / 2);
  
  int buttonPressed = navigator.navigate(); // Rotary scroll (-1, 0, or 1)
  int clickState = navigator.checkClick(rotaryEncoderButton); // NEW: 0, 1, or 2 

  // --- UNIVERSAL EXIT (DOUBLE CLICK) ---
  if (clickState == 2) {
    if (currentMode == 1) clockApp.clearScreen();
    else if (currentMode == 2) links.clearScreen();
    else if (currentMode == 3) playlist.clearScreen();
    else if (currentMode == 4) mediaScreen.clearScreen();
    else if (currentMode == 5) mediaScreen.clearScreen();

    tft.pushImage(0, 0, 320, 240, mainBackground);
    
    // THE NEW ROUTING LOGIC
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
        
        // 1. Free the Media Editor's RAM and wipe the screen
        mediaScreen.clearScreen();
        tft.pushImage(0, 0, 320, 240, mainBackground);
        
        // 2. Load the newly selected internal app
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
          diagnostics.loadScreen();
        }
      } 
      
      // MEDIA CONTROL LOGIC (Sidebar Closed)
      else {
        int currentHover = mediaScreen.getSelection(); 
        if (currentHover == 1) Serial.println("MEDIA_PREV");
        else if (currentHover == 2) Serial.println("MEDIA_PLAY_PAUSE");
        else if (currentHover == 3) Serial.println("MEDIA_NEXT");
      }
    }

    // Keep the timers ticking
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
        diagnostics.loadScreen();
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
      else if (selectedLink == 1) Serial.println("LAUNCH_GEMINI");
      else if (selectedLink == 2) Serial.println("LAUNCH_GITHUB");
      else if (selectedLink == 3) Serial.println("LAUNCH_DESMOS");
      else if (selectedLink == 4) Serial.println("LAUNCH_SPOTIFY");
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
}