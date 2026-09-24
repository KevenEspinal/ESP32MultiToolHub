#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

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

// ---------------------------------------------------------------------------------------
// NEW GATT SERVER ARCHITECTURE FOR COMPANION APP
// ---------------------------------------------------------------------------------------

uint8_t* imageBuffer = nullptr;
uint16_t expectedImageSize = 0;
uint16_t currentImageIndex = 0;
bool newImageReady = false;

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      uint8_t commandID = value[0];
      Serial.print("Config Received: ");
      Serial.println(commandID);
    }
  }
};

class ArtCommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    // Check if it's the "Start Transfer" handshake (0x01) followed by a 2-byte file size
    if (value.length() >= 3 && value[0] == 0x01) {
      
      // Combine byte 1 and byte 2 into a 16-bit integer
      expectedImageSize = (value[1] << 8) | (uint8_t)value[2];
      currentImageIndex = 0;

      // Free old memory just in case the user skipped a song mid-transfer
      if (imageBuffer != nullptr) {
        free(imageBuffer);
        imageBuffer = nullptr;
      }

      // Ask the ESP32 heap for exactly enough memory to hold the image
      imageBuffer = (uint8_t*)malloc(expectedImageSize);

      if (imageBuffer != nullptr) {
        Serial.print("Handshake Accepted! Allocated bytes: ");
        Serial.println(expectedImageSize);

        // Tell the phone we are locked, loaded, and ready for data
        uint8_t ready[] = {0x01};
        pCharacteristic->setValue(ready, 1);
        pCharacteristic->notify();
      } else {
        Serial.println("ERROR: ESP32 Out of Memory!");
      }
    }
  }
};

class ArtPayloadCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    size_t len = value.length();

    if (imageBuffer != nullptr && currentImageIndex + len <= expectedImageSize) {
      memcpy(&imageBuffer[currentImageIndex], value.data(), len);
      currentImageIndex += len;

      Serial.print("Received chunk: ");
      Serial.print(len);
      Serial.print(" bytes. Progress: ");
      Serial.print(currentImageIndex);
      Serial.print("/");
      Serial.println(expectedImageSize);

      if (currentImageIndex == expectedImageSize) {
        Serial.println(">>> IMAGE TRANSFER FULLY COMPLETE <<<");
        newImageReady = true;
      }
    }
  }
};

void setupBLE() {
  NimBLEDevice::init("Testing-AMS");                                                        // Boots up the underlying NimBLE host stack and assigns an internal name.
  NimBLEDevice::setCustomGapHandler(custom_gap_cb);                                         // Registers custom_gap_cb into NimBLE library
  NimBLEDevice::setSecurityAuth(true, false, true);                                         // Enables bonding (meaning paired devices will remember each other in the future), disbales MITM, enables LE Secure connections 
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);                                // Prohibits input or output capabilties, directly disabling the need to input a PIN for added security
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);             // Sets the keys that the initiator will receive or accept
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);             // Sets the keys that the responsed will receive or accept
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);                                                   // Sets the bluetooth transmission power on ESP32 to max level
  NimBLEDevice::setMTU(512);

  // Initialize as Server
  NimBLEServer* pServer = NimBLEDevice::createServer();
  
  // Standard Device Info Service
  NimBLEService* pDeviceInfoService = pServer->createService((uint16_t)0x180A);
  NimBLECharacteristic* pManufacturer = pDeviceInfoService->createCharacteristic((uint16_t)0x2A29, NIMBLE_PROPERTY::READ);
  pManufacturer->setValue("Keven Espinal");
  NimBLECharacteristic* pModel = pDeviceInfoService->createCharacteristic((uint16_t)0x2A24, NIMBLE_PROPERTY::READ);
  pModel->setValue("Do-All-Inator v1.0");
  NimBLECharacteristic* pFirmware = pDeviceInfoService->createCharacteristic((uint16_t)0x2A26, NIMBLE_PROPERTY::READ);
  pFirmware->setValue("1.0.0");
  pDeviceInfoService->start();

  // Custom Configuration & Artwork Service
  NimBLEService* pCustomService = pServer->createService("a1b2c3d4-e5f6-4a1b-8c2d-123456789abc");
  NimBLECharacteristic* pConfigChar = pCustomService->createCharacteristic("a1b2c3d4-e5f6-4a1b-8c2d-123456789ab1", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pConfigChar->setCallbacks(new ConfigCallbacks());
  NimBLECharacteristic* pArtCommandChar = pCustomService->createCharacteristic("a1b2c3d4-e5f6-4a1b-8c2d-123456789ab2", NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pArtCommandChar->setCallbacks(new ArtCommandCallbacks());
  NimBLECharacteristic* pArtPayloadChar = pCustomService->createCharacteristic("a1b2c3d4-e5f6-4a1b-8c2d-123456789ab3", NIMBLE_PROPERTY::WRITE_NR);
  pArtPayloadChar->setCallbacks(new ArtPayloadCallbacks());
  pCustomService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();                                         // Stores pointer to internal advertising object
  pAdv->setName("Testing-AMS");                                                                     // Registers public name  
  pAdv->addServiceUUID("a1b2c3d4-e5f6-4a1b-8c2d-123456789abc");                                     // Expose Custom Service to scanners
  pAdv->start();                                                                                    // Starts broadcasting
}

void runBLEStateMachine() {
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
}