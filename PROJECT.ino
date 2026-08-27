#define USE_NIMBLE
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
#include "weatherApp.h"
#include <NimBLEDevice.h>


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

  // Variables for iPhone pairing, including recognition as well as track data
  static BLEUUID amsServiceUUID("89D3502B-0F36-433A-8EF4-C502AD55F8DC");    // standard string for AMS recognition

  static uint16_t activeConnHandle = 0xFFFF;
  static uint16_t amsStartHandle = 0;
  static uint16_t amsEndHandle = 0;
  static uint16_t entityUpdateHandle = 0;
  static uint16_t remoteCmdHandle = 0;
  static uint16_t cccdHandle = 0;

  static int setupPhase = 0;
  static bool actionComplete = false;
  static bool amsDataUpdated = false;
  static bool phoneDisconnected = false;

  static String amsArtist = "";
  static String amsTitle = "";
  static int amsDuration = 0;
  static float amsProgress = 0;
  static String amsState = "";
  
  bool needDiagnostics = false;
  static unsigned long phase1StartTime = 0;

// This sends the commands that the user wants to execute to the iPhone
int media_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  if (error->status == 0) {
    Serial.println("<<< iPhone Acknowledged Media Command! <<<");
  } else {
    Serial.print("--- ERROR: iPhone rejected command. Code: ");
    Serial.println(error->status);
  }
  return 0;
}

void sendMediaCommand(uint8_t commandID) {
  if (remoteCmdHandle == 0 || activeConnHandle == 0xFFFF){
    Serial.println("--- ERROR: Commands blocked ---");
    return;
  } 

  int rc = ble_gattc_write_flat(activeConnHandle, remoteCmdHandle, &commandID, 1, media_write_cb, nullptr);
  
  if (rc == 0) {
    Serial.println("Media Command Dispatched (Waiting for iPhone receipt...)");
  } else {
    Serial.print("Failed to dispatch command. Local BLE code: ");
    Serial.println(rc);
  }
}

// Executes after all call back functions have 
int write_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  actionComplete = true;
  return 0;
}

// Handles incoming notification, whether its an (un)successful connection, encryption is completed, or incoming pushed data 
int custom_gap_cb(struct ble_gap_event *event, void *arg) {
  if (event->type == BLE_GAP_EVENT_CONNECT) {                       // Checks if Doallinator is trying to connect
    if (event->connect.status == 0) {                               // Checks that that the status is successful
      activeConnHandle = event->connect.conn_handle;                // Stores the unique ID (handle) of this specific connection
      NimBLEDevice::startSecurity(activeConnHandle);                // Requests a secure, encrypted connection
    }
  } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
    activeConnHandle = 0xFFFF; 
    setupPhase = 0;
    phoneDisconnected = true;
    NimBLEDevice::getAdvertising()->start();
  } else if (event->type == BLE_GAP_EVENT_ENC_CHANGE) {             // Triggers when the security/encryption process finishes
    if (event->enc_change.status == 0) {                            // Safety net, checks if the encryption was successful and updates global variables
      setupPhase = 1;
      phase1StartTime = millis();
      actionComplete = true;
    }
  } else if (event->type == BLE_GAP_EVENT_NOTIFY_RX) {              // Triggers when iPhone pushes data 
    if (event->notify_rx.attr_handle == entityUpdateHandle) {       // Checks if the pushed data characterized as media information from bluetooth
      // Extracts data payload and stores the lenght of payload as well as a pointer
      uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
      uint8_t *data = OS_MBUF_DATA(event->notify_rx.om, uint8_t *);
      
      // First 3 bytes are headers. 
      // First byte is the identity (Player Entity, Queue Entity, or Track Entity), specifies what the next byte of data is refferring to
      // Second byte stores the actual data for the specified entity , for example under player entity this byte stores the app name and play state and volume
      // Third byte is ignored in logic 
      if (len >= 3) {
        // Stores first two bytes
        uint8_t entityID = data[0];                                 
        uint8_t attributeID = data[1];
        String value = "";

        for (size_t i = 3; i < len; i++) value += (char)data[i];      // Stores data payload onto string
        
        // Stores the corresponding data that was transmitted only if we're under Track Entity id
        // which contains artist name, song title, current time of song, etc
        if (entityID == 2) {
          if (attributeID == 0) amsArtist = value;
          else if (attributeID == 2) amsTitle = value;
          else if (attributeID == 3) amsDuration = value.toFloat();
        }

        // Stores the state of song (paused vs playing)
        else if (entityID == 0 && attributeID == 1) {

          // Creates substrings using commas as delimeters
          int firstComma = value.indexOf(',');
          int secondComma = value.indexOf(',', firstComma + 1);

          // After passing safety check both the status of the song and how much time the user is into the song are stored in global variables
          if (firstComma != -1 && secondComma != -1) {
            String stateStr = value.substring(0, firstComma);
            String elapsedStr = value.substring(secondComma + 1);
            if (stateStr == "1") amsState = "Playing";
            else amsState = "Paused";
            amsProgress = elapsedStr.toFloat();
          }
        }
        amsDataUpdated = true;
      }
    }
  }
  return 0;
}

int dsc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {

  // Checks if a descriptor was successfully found without any connection errors
  if (error->status == 0 && dsc != nullptr) {

    // Ignores AMS 128 bit UUID and simply checks for the 16 bit
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16 && dsc->uuid.u16.value == 0x2902) {
      // Safety check, ensuring handle hasn't already been found. Then stores the handle into a global variable
      if (cccdHandle == 0) cccdHandle = dsc->handle;                                        
    }
    return 0;
  }

  // Checks if the ESP32 is done scanning all of the characteristics and marks the action as complete
  if (error->status == BLE_HS_EDONE) {
    actionComplete = true;
    return 0;
  }
  return 0;
}

// Stores the "address" of where to send data from ESP32 to iPhone
int chr_disc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {

  // Ensures the ESP32 successfully found a characteristic without any connection errors
  if (error->status == 0 && chr != nullptr) {
    // Convert UUID to readable uppercased string
    char uuid_str[37];
    ble_uuid_to_str(&(chr->uuid.u), uuid_str);
    String foundStr = String(uuid_str);
    foundStr.toUpperCase();
    
    Serial.print("Scanned Characteristic: ");
    Serial.println(foundStr);
    
    // Stores the listening and writing UUID
    if (foundStr.indexOf("2F7CABCE") != -1) {
      entityUpdateHandle = chr->val_handle;
      Serial.println(">>> SAVED Entity Update Handle <<<");
    }
    else if (foundStr.indexOf("9B3C81D8") != -1) {
      remoteCmdHandle = chr->val_handle;
      Serial.println(">>> SAVED Remote Command Handle <<<");
    }
    return 0;
  }

  // Checks if the ESP32 is done scanning all of the characteristics and marks the action as complete
  if (error->status == BLE_HS_EDONE) {
    actionComplete = true;
    return 0;
  }
  return 0;
}

// Stores UUID and pointers to the start and end of the handle
int gattc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
  // Ensures the ESP32 successfully found a service without any connection errors
  if (error->status == 0 && service != nullptr) {
    // Extracts UUID of the discovered service through service->uuid.u, converts it into a string, and creates a NimBLEUUID object for easy comparison
    char uuid_str[37];
    ble_uuid_to_str(&(service->uuid.u), uuid_str);
    NimBLEUUID foundSvc(uuid_str);
    
    // Checks for a match and stores values to global variables
    if (foundSvc.equals(amsServiceUUID)) {
      amsStartHandle = service->start_handle;
      amsEndHandle = service->end_handle;
    }
    return 0;
  }

  // Checks if the ESP32 is done scanning all of the information and marks the action as complete
  if (error->status == BLE_HS_EDONE) {
    actionComplete = true;
    return 0;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  NimBLEDevice::init("Testing-AMS");                                                        // Boots up the underlying NimBLE host stack and assigns an internal name.
  NimBLEDevice::setCustomGapHandler(custom_gap_cb);                                         // Registers custom_gap_cb into NimBLE library
  NimBLEDevice::setSecurityAuth(true, false, true);                                         // Enables bonding (meaning paired devices will remember each other in the future), disbales MITM, enables LE Secure connections 
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);                                // Prohibits input or output capabilties, directly disabling the need to input a PIN for added security
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);             // Sets the keys that the initiator will receive or accept
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);             // Sets the keys that the responsed will receive or accept
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);                                                   // Sets the bluetooth transmission power on ESP32 to max level
  NimBLEDevice::setMTU(512);

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();                                         // Stores pointer to internal advertising object
  pAdv->setName("Testing-AMS");                                                                     // Registers public name  
  pAdv->start();                                                                                    // Starts broadcasting

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

  // Re-draw the background to erase the loading message
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

  char incomingMsg[512];
  size_t len = Serial.readBytesUntil('\n', incomingMsg, sizeof(incomingMsg) - 1);
  incomingMsg[len] = '\0';
  
  while(len > 0 && (incomingMsg[len-1] == '\r' || incomingMsg[len-1] == ' ')){
    incomingMsg[len-1] = '\0';
    len--;
  }

  char* command = strtok(incomingMsg, "|");
  if (!command) return;

  if (strcmp(command, "MEDIA") == 0) {
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
        
        tft.pushImage(0, 0, 320, 240, mainBackground);
        currentMode = 4;
        mediaScreen.render(); 
      } 
      // Explicitly tell it to redraw
      else if (currentMode == 4) {
        mediaScreen.render();
      }
    }
  } 
  else if (strcmp(command, "IDLE") == 0) {
    mediaScreen.clearActiveMedia(); // Erase the active flag
    
    if (currentMode == 4) {
      mediaScreen.clearScreen();
      tft.pushImage(0, 0, 320, 240, mainBackground);
      menuObject.setup(appsInfo.getNamesList());
      currentMode = 0;
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

      // Declare arrays to store data for all times after the current time
      String conditions[8];
      int temperatures[8];
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
      weather.updateData(times, temperatures, conditions, 8);
    }
  }
}

void loop() {
// --- IPHONE MODE ---
  // Resets connetion status, clears media data sprites and resets back to menu screen
  if (phoneDisconnected) {
    phoneDisconnected = false;
    mediaScreen.clearActiveMedia();
    if (currentMode == 4) {
      mediaScreen.clearScreen();
      tft.pushImage(0, 0, 320, 240, mainBackground);
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
      
      tft.pushImage(0, 0, 320, 240, mainBackground);
      currentMode = 4;
      mediaScreen.render(); 
    } 
    else if (currentMode == 4) {
      mediaScreen.render();
    }
  }

  // State machine that executes the required steps to fully subscribe to the iPhone's media notifications
  if (actionComplete) {
    if (setupPhase == 1 && (millis() - phase1StartTime < 2000)) {
      // Non-blocking wait: let the loop continue running while we wait 2 seconds
    } else {
      actionComplete = false;
      
      // Waits for 2000 milliseconds to ensure the newly established BLE connection is stable, advances the phase to 2, and asks the NimBLE stack to discover all available services on the iPhone
      if (setupPhase == 1) {
        setupPhase = 2;
        ble_gattc_disc_all_svcs(activeConnHandle, gattc_cb, nullptr);
      } 
      // Checks if the AMS was successfully found by verifying amsStartHandle is not zero
      // If found, it advances to phase 3 and asks the stack to discover all characteristics strictly within the start and end handles of that specific service
      else if (setupPhase == 2) {
        if (amsStartHandle != 0) {
          setupPhase = 3;
          ble_gattc_disc_all_chrs(activeConnHandle, amsStartHandle, amsEndHandle, chr_disc_cb, nullptr);
        }
      }
      // Checks if the "Entity Update" characteristic was successfully found. If found, it advances to phase 4 and requests the discovery of all descriptors attached to it
      else if (setupPhase == 3) {
        if (entityUpdateHandle != 0) {
          setupPhase = 4;
          ble_gattc_disc_all_dscs(activeConnHandle, entityUpdateHandle, amsEndHandle, dsc_disc_cb, nullptr);
        }
      }
      // Checks if CCCD was found. It advances to phase 5 and writes a byte array to CCCD
      // This is the universal BLE command that tells the iPhone to turn on notifications. Checks for failures (rc != 0), it resets the actionComplete flag to true so the loop will try again
      else if (setupPhase == 4) {
        if (cccdHandle != 0) {
          setupPhase = 5;
          uint8_t cccd_val[] = {0x01, 0x00};
          int rc = ble_gattc_write_flat(activeConnHandle, cccdHandle, cccd_val, sizeof(cccd_val), write_cb, (void*)4);
          if (rc != 0) actionComplete = true;
        }
      }
      // Advances to phase 6 and writes {2, 0, 1, 2, 3} to the Entity Update characteristic
      // Which according to AMS specification, Entity ID 2 represents the "Track"
      // This specific byte array commands the iPhone to notify the ESP32 whenever the track's Artist (0), Album (1), Title (2), or Duration (3) changes
      else if (setupPhase == 5) {
        setupPhase = 6;
        uint8_t trackCmd[] = {2, 0, 1, 2, 3};
        int rc = ble_gattc_write_flat(activeConnHandle, entityUpdateHandle, trackCmd, sizeof(trackCmd), write_cb, (void*)5);
        if (rc != 0) actionComplete = true;
      }
      // Advances to phase 7 and writes {0, 1} to the Entity Update characteristic. Entity ID 0 represents the "Media Player"
      // This specific command tells the iPhone to notify the ESP32 whenever the playback state changes, such as when a song is paused or playing
      else if (setupPhase == 6) {
        setupPhase = 7;
        uint8_t playerCmd[] = {0, 1};
        int rc = ble_gattc_write_flat(activeConnHandle, entityUpdateHandle, playerCmd, sizeof(playerCmd), write_cb, (void*)6);
        if (rc != 0) actionComplete = true;
      }
      // The setup process is fully complete, so it resets the setupPhase variable back to 0. The ESP32 is now passively waiting to receive pushed data
      else if (setupPhase == 7) {
        setupPhase = 0; 
      }
    }
  }

  vol.calculateVolume(encoder.getCount() / 2);
  
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

    tft.pushImage(0, 0, 320, 240, mainBackground);
    
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
        tft.pushImage(0, 0, 320, 240, mainBackground);
        
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
}