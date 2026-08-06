#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "background.h" 

class app{
  private:
  static int totalNumApps;
  static std::vector<String> namesList;
  String name;
  public:
  app(){}
  app(String appName){
    totalNumApps++;
    name = appName;
    namesList.push_back(appName);
  }

  int getNumApps(){
    return totalNumApps;
  }
  std::vector<String> getNamesList(){
    return namesList;
  }
  void loadScreen(){
    // Use same background as menu (temporary)
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    tft.pushImage(0, 0, 320, 240, mainBackground);
  }
};
int app::totalNumApps = 0;
std::vector<String> app::namesList;