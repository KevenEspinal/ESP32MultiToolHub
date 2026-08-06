#pragma once

class navigation {
  private:
    int pins[4] = {12, 14, 27, 26};
    
    // Variables for auto repeat
    unsigned long lastActionTime = 0;
    int currentInterval = 500;        // Starts at half a second
    int activePin = -1;               // Tracks which button is currently held (-1 means none)

    int lastSteadyState = HIGH;
    int lastFlickerableState = HIGH;
    unsigned long lastDebounceTime = 0;
    unsigned long debounceDelay = 50;

    int clickCount = 0;
    unsigned long lastPressTime = 0;
    unsigned long doubleClickWindow = 600; // 600ms time frame

  public:
    navigation() {}
    
    void setupButtons() {
      for(int i = 0; i < 4; i++){
        pinMode(pins[i], INPUT_PULLUP);
      }
    }
    


    int navigate(){

      int currentlyPressedIndex = -1;
      for(int i = 0; i < 4; i++){      // Checks for any button thats pressed
        if(digitalRead(pins[i]) == LOW){
          currentlyPressedIndex = i;
          break; // Stop scanning as to handle one button at a time
        }
      }

      if (currentlyPressedIndex != -1) {          // If a button was pressed
        if (activePin != currentlyPressedIndex) { // Only runs on a brand new press (different button from the last one that was pressed or if its the absolute first one to be pressed)
          activePin = currentlyPressedIndex;
          currentInterval = 500;        // Reset interval to 500ms
          lastActionTime = millis();    // Set the timestamp
          return currentlyPressedIndex;
        }else {                         // The button is being held down
          if (millis() - lastActionTime >= currentInterval) {          // Check if enough time has passed based on shrinking interval
            lastActionTime = millis();                // Reset the stopwatch
            currentInterval = currentInterval - 60;   // Subtract 60ms from the interval
            if (currentInterval < 30) {               // Hard cap the speed so it doesn't drop below 30ms and crash/spam
              currentInterval = 30; 
            }
            return currentlyPressedIndex;
          }
          return -1;
        }
      }else { // No buttons are pressed at all
        activePin = -1; // Reset the state, ready for the next press
        return activePin;
      }

    
    }

    int checkClick(uint8_t buttonPin) {
        int currentState = digitalRead(buttonPin);
        unsigned long currentMillis = millis();
        int returnVal = 0;

        // 1. Standard Debouncing
        if (currentState != lastFlickerableState) {
            lastDebounceTime = currentMillis;
            lastFlickerableState = currentState;
        }

        if ((currentMillis - lastDebounceTime) > debounceDelay) {
            if (currentState != lastSteadyState) {
                lastSteadyState = currentState;
                
                // If button is physically pushed down
                if (currentState == LOW) { 
                    clickCount++;
                    lastPressTime = currentMillis;
                }
            }
        }

        // 2. Evaluate the clicks based on the stopwatch
        if (clickCount > 0) {
            // If they click twice quickly, trigger immediately!
            if (clickCount >= 2) {
                returnVal = 2; // Double click
                clickCount = 0; // Reset for next time
            }
            // If 600ms passes and they haven't clicked a second time, confirm the single click
            else if ((currentMillis - lastPressTime) > doubleClickWindow) {
                returnVal = 1; // Single click
                clickCount = 0; // Reset for next time
            }
        }

        return returnVal;
    }
};