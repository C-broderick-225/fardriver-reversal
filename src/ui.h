#ifndef UI_H
#define UI_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "logic.h"
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define MODE_Y 15
#define STATUS_AREA_Y 160
#define STATUS_AREA_HEIGHT 30
#define BATTERY_Y 200
#define RIGHT_INFO_Y 200
#define GAUGE_CENTER_X 160
#define GAUGE_CENTER_Y 90
#define GAUGE_RADIUS 120
#define MPH_DISPLAY_Y 140
#define GAUGE_SEGMENTS 23
#define STATUS_AREA_Y 175  // Moved down
#define STATUS_AREA_HEIGHT 15  // Made thinner
#define REV_LIMITER_FLASH_RATE 250  // Flash rate in milliseconds
#define REV_LIMITER_BORDER_WIDTH 5  // Border width in pixels
#define COLOR_BACKGROUND ST77XX_BLACK
#define COLOR_TITLE_BAR ST77XX_BLUE
#define COLOR_TEXT_PRIMARY ST77XX_WHITE
#define COLOR_TEXT_SECONDARY ST77XX_CYAN
#define COLOR_GAUGE_GREEN ST77XX_GREEN
#define COLOR_GAUGE_YELLOW ST77XX_YELLOW
#define COLOR_GAUGE_RED ST77XX_RED
#define COLOR_BRAKE ST77XX_RED
#define COLOR_REGEN ST77XX_GREEN
#define COLOR_REVERSE ST77XX_MAGENTA
#define COLOR_BATTERY_LOW ST77XX_RED
#define COLOR_BATTERY_MED ST77XX_YELLOW
#define COLOR_BATTERY_HIGH ST77XX_GREEN
#define COLOR_CURRENT_REGEN ST77XX_GREEN
#define COLOR_CURRENT_HIGH ST77XX_RED
#define COLOR_CURRENT_NORMAL ST77XX_WHITE
#define COLOR_ARCH_OUTLINE 0x4208

class VehicleUI {
private:
  Adafruit_ST7789* display;
  byte lastSpeedMode;
  bool lastBrake, lastRegen, lastReverse;
  int lastActiveSegments;
  float lastBattery;
  float lastVoltage;
  int lastCurrent;
  int lastMphShown;
  bool wasInReverse;
  bool revLimiterActive;
  unsigned long lastRevLimiterFlash;
  bool revLimiterBorderShown;
  
public:
  VehicleUI();
  void init(Adafruit_ST7789* tft);
  void drawStartupScreen();
  void updateDisplay(VehicleData data);
  
private:
  void drawStaticElements();
  void drawLightningBolt(int x, int y, int size = 20);
  void drawBattery(int x, int y, int width = 30, int height = 15);
  void updateSpeedMode(byte speedMode);
  void updateStatusIndicators(bool brake, bool regen, bool reverseMode);
  void drawReverse(bool reverseMode);
  void drawMphGauge(float mph, int displayedRpm, bool reverseMode);
  void updateBottomInfo(float battery, float voltage, int current);
  void updateRevLimiterWarning(int rpm);
  void drawRevLimiterBorder(bool show);
  uint16_t getGaugeColor(int rpm);
  uint16_t getBatteryColor(float battery);
  uint16_t getCurrentColor(int current);
  uint16_t getSpeedModeColor(byte speedMode);
};

#endif