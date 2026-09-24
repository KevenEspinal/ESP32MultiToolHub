#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "background.h"
#include "theme.h"

extern TFT_eSPI tft; // NOTE: every sibling app header declares this; it was
                      // missing here, which meant loadScreen() below could
                      // not actually see `tft`. Added to match the rest of
                      // the project (no behaviour change, just makes the
                      // existing code buildable).

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

  virtual ~app() {}

  int getNumApps(){
    return totalNumApps;
  }
  std::vector<String> getNamesList(){
    return namesList;
  }
  void loadScreen(){
    tft.setSwapBytes(true);
    tft.fillScreen(UI_BG);
  }
};
int app::totalNumApps = 0;
std::vector<String> app::namesList;
