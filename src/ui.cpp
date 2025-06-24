#include "ui.h"
#include <Fonts/FreeSerif9pt7b.h>
VehicleUI::VehicleUI() {
  display = nullptr;
  lastSpeedMode = 255;
  lastBrake = false;
  lastRegen = false;
  lastReverse = false;
  lastActiveSegments = -1;
  lastBattery = -1;
  lastVoltage = -1;
  lastCurrent = -999;
  lastMphShown = -999;
  wasInReverse = false;
  revLimiterActive = false;
  lastRevLimiterFlash = 0;
  revLimiterBorderShown = false;
}

void VehicleUI::init(Adafruit_ST7789* tft) {
  display = tft;
  Serial.println("Vehicle UI initialized");
}

void VehicleUI::drawStartupScreen() {
  if (!display) return;

  display->fillScreen(COLOR_BACKGROUND);

  // Draw yellow face
  uint16_t faceColor = 0xFFE0; // Bright yellow (RGB565)
  uint16_t eyeColor = 0x0000;  // Black
  uint16_t mouthColor = 0x0000; // Black

  int centerX = GAUGE_CENTER_X;
  int centerY = GAUGE_CENTER_Y;
  int radius = 60;

  // Face circle
  display->fillCircle(centerX, centerY, radius, faceColor);

  // Eyes
  int eyeOffsetX = 20;
  int eyeOffsetY = 20;
  display->fillCircle(centerX - eyeOffsetX, centerY - eyeOffsetY, 6, eyeColor); // Left eye
  display->fillCircle(centerX + eyeOffsetX, centerY - eyeOffsetY, 6, eyeColor); // Right eye

  // Mouth (simple smile arc)
  for (int angle = 45; angle <= 135; angle++) {
    float rad = angle * 3.14159 / 180;
    int x = centerX + cos(rad) * 30;
    int y = centerY + sin(rad) * 20;
    display->drawPixel(x, y, mouthColor);
  }

  // Text
  display->setTextColor(COLOR_TEXT_PRIMARY); // Or eyeColor for black
  display->setTextSize(1);
  display->setCursor(centerX - 40, centerY + radius + 10);
  display->print("Cheap Shit Dash");

  delay(1500);
  display->fillScreen(COLOR_BACKGROUND);
  drawStaticElements();
}

void VehicleUI::drawBattery(int x, int y, int width, int height) {
  int tipWidth = 4;
  int tipHeight = height / 2;
  display->drawRect(x, y, width, height, ST77XX_WHITE);
  int tipX = x + width;
  int tipY = y + (height - tipHeight) / 2;
  display->drawRect(tipX, tipY, tipWidth, tipHeight, ST77XX_WHITE);
}

void VehicleUI::drawLightningBolt(int x, int y, int size) {
  int half = size / 2;
  int third = size / 3;
  display->fillTriangle(
    x, y,
    x + third, y + half,
    x + third - 2, y + half,
    ST77XX_WHITE
  );
  display->fillTriangle(
    x + third - 2, y + half,
    x + size, y + size,
    x + third + 2, y + half,
    ST77XX_WHITE
  );
  display->fillTriangle(
    x + size - 3, y + size - 2,
    x + half, y + size * 3 / 2,
    x + size - 5, y + size,
    ST77XX_WHITE
  );
}

void VehicleUI::drawStaticElements() {
  if (!display) return;
  display->setTextColor(COLOR_TEXT_SECONDARY);
  display->setTextSize(2);
  display->setCursor(120, MODE_Y);
  display->print("MODE ");
  display->setTextSize(2);
  display->setCursor(200, RIGHT_INFO_Y);
  display->print("VOLT ");
  display->setCursor(200, RIGHT_INFO_Y + 20);
  display->print("CURR ");
}

void VehicleUI::updateDisplay(VehicleData data) {
  if (!display) return;
  
  updateSpeedMode(data.speedMode);
  updateStatusIndicators(data.brake, data.regen, data.reverseMode);
  
  if (data.reverseMode) {
    drawReverse(data.reverseMode);
  } else {
    drawMphGauge(data.mph, data.displayedRpm, data.reverseMode);
  }
  
  updateBottomInfo(data.battery, data.voltage, data.current);
  updateRevLimiterWarning(data.displayedRpm);
}

void VehicleUI::updateSpeedMode(byte speedMode) {
  if (speedMode != lastSpeedMode) {
    display->fillRect(195, MODE_Y - 5, 70, 30, COLOR_BACKGROUND);
    
    uint16_t modeColor = getSpeedModeColor(speedMode);
    
    display->setTextColor(modeColor);
    display->setTextSize(2);
    display->setCursor(200, MODE_Y);
    display->print(speedMode);
    
    lastSpeedMode = speedMode;
  }
}

void VehicleUI::updateStatusIndicators(bool brake, bool regen, bool reverseMode) {
  if (brake != lastBrake) {
    display->fillRect(75, 175, 62, 15, brake ? COLOR_BRAKE : COLOR_BACKGROUND);
    if (brake) {
      display->setTextColor(COLOR_BACKGROUND);
      display->setTextSize(2);
      display->setCursor(80, 176);
      display->print("BRAKE ");
    }
    lastBrake = brake;
  }
  
  if (regen != lastRegen) {
    display->fillRect(140, 175, 62, 15, regen ? COLOR_REGEN : COLOR_BACKGROUND);
    if (regen) {
      display->setTextColor(COLOR_BACKGROUND);
      display->setTextSize(2);
      display->setCursor(145, 176);
      display->print("REGEN ");
    }
    lastRegen = regen;
  }
}

void VehicleUI::drawReverse(bool reverseMode) {
  if (reverseMode != wasInReverse) {
    if (reverseMode) {
      // Clear the entire center area when entering reverse
      display->fillRect(35, 35, 250, 140, COLOR_BACKGROUND);
      
      // Draw REVERSE text
      display->setTextColor(COLOR_GAUGE_RED);
      display->setTextSize(4);
      display->setCursor(90, 90);
      display->print("REVERSE");
    }
    wasInReverse = reverseMode;
  }
}

void VehicleUI::drawMphGauge(float mph, int displayedRpm, bool reverseMode) {
  // Don't draw MPH gauge at all when in reverse mode
  if (reverseMode) {
    return;
  }
  
  if (wasInReverse && !reverseMode) {
    // Only clear the center area, NOT the side gauges
    display->fillRect(35, 35, 250, 140, COLOR_BACKGROUND);
    lastActiveSegments = -1;
    
    // Force redraw of all UI elements by resetting their "last" values
    lastSpeedMode = 255;  // Force speed mode redraw
    lastMphShown = -999;  // Force MPH redraw
    lastBrake = !lastBrake;  // Force brake indicator redraw
    lastRegen = !lastRegen;  // Force regen indicator redraw
    
    // Redraw static elements immediately
    drawStaticElements();
  }
  wasInReverse = reverseMode;
  
  if (reverseMode) return;
  
  int activeSegments = map(displayedRpm, 0, MAX_RPM, 0, 36);
  if (activeSegments != lastActiveSegments) {
    display->fillRect(0, 10, 30, 191, COLOR_BACKGROUND);
    display->fillRect(290, 10, 30, 191, COLOR_BACKGROUND);
    
    uint16_t segColor = getGaugeColor(displayedRpm);
    for (int i = 0; i < activeSegments; i++) {
      int barHeight = 4;
      int barSpacing = 1;
      int totalBarHeight = barHeight + barSpacing;
      int yPos = 190 - (i * totalBarHeight);
      display->fillRect(2, yPos - barHeight, 26, barHeight, segColor);
      display->fillRect(292, yPos - barHeight, 26, barHeight, segColor);
    }
    
    lastActiveSegments = activeSegments;
  }
  int displayMph = (int)mph;
  if (abs(displayMph - lastMphShown) > 1) {
    display->fillRect(50, 45, 220, 100, COLOR_BACKGROUND);
    
    display->setTextColor(COLOR_TEXT_PRIMARY);
    display->setTextSize(8);
    display->setCursor(80, 45);
    display->print(displayMph);
    
    display->setTextSize(3);
    display->setCursor(200, 85);
    display->print("MPH");
    display->setTextSize(2);
    display->setTextColor(COLOR_TEXT_SECONDARY);
    display->setCursor(120, 130);
    display->print(displayedRpm);
    display->print(" RPM");
    
    lastMphShown = displayMph;
  }
}

void VehicleUI::updateBottomInfo(float battery, float voltage, int current) {
  if (abs(battery - lastBattery) > 0.5) {
    display->fillRect(10+5, BATTERY_Y + 8, 130, 40, COLOR_BACKGROUND);
    
    uint16_t battColor = getBatteryColor(battery);
    
    display->setTextColor(battColor);
    display->setTextSize(3);
    display->setCursor(13+5, BATTERY_Y + 11);
    display->print((int)battery, 1);
    drawBattery(10+5, BATTERY_Y + 10 , 65, 25);
    
    lastBattery = battery;
  }
  if (abs(voltage - lastVoltage) > 0.2) {
    display->fillRect(258, RIGHT_INFO_Y - 5, 62, 25, COLOR_BACKGROUND);
    display->setTextColor(COLOR_TEXT_SECONDARY);
    display->setTextSize(2);
    display->setCursor(270, RIGHT_INFO_Y);
    display->print((int)voltage);
    display->print("V");
    
    lastVoltage = voltage;
  }
  if (abs(current - lastCurrent) > 2) {
    display->fillRect(265, RIGHT_INFO_Y + 15, 60, 25, COLOR_BACKGROUND);
    
    uint16_t currColor = getCurrentColor(current);
    
    display->setTextColor(currColor);
    display->setTextSize(2);
    display->setCursor(275, RIGHT_INFO_Y + 20);
    display->print(current);
    display->print("A");
    
    lastCurrent = current;
  }
}

void VehicleUI::updateRevLimiterWarning(int rpm) {
  bool shouldBeActive = (rpm >= MAX_RPM);
  if (shouldBeActive && !revLimiterActive) {
    revLimiterActive = true;
    lastRevLimiterFlash = millis();
    revLimiterBorderShown = true;
    drawRevLimiterBorder(true);
  } else if (!shouldBeActive && revLimiterActive) {
    revLimiterActive = false;
    drawRevLimiterBorder(false);
    revLimiterBorderShown = false;
  } else if (revLimiterActive) {
    unsigned long currentTime = millis();
    if (currentTime - lastRevLimiterFlash > REV_LIMITER_FLASH_RATE) { // Flash every 250ms
      revLimiterBorderShown = !revLimiterBorderShown;
      drawRevLimiterBorder(revLimiterBorderShown);
      lastRevLimiterFlash = currentTime;
    }
  }
}

void VehicleUI::drawRevLimiterBorder(bool show) {
  if (!display) return;
  
  uint16_t borderColor = show ? COLOR_GAUGE_RED : COLOR_BACKGROUND;
  int borderWidth = REV_LIMITER_BORDER_WIDTH;
  display->fillRect(0, 0, SCREEN_WIDTH, borderWidth, borderColor);
  display->fillRect(0, SCREEN_HEIGHT - borderWidth, SCREEN_WIDTH, borderWidth, borderColor);
  display->fillRect(0, 0, borderWidth, SCREEN_HEIGHT, borderColor);
  display->fillRect(SCREEN_WIDTH - borderWidth, 0, borderWidth, SCREEN_HEIGHT, borderColor);
  if (!show) {
    drawStaticElements();
  }
}

uint16_t VehicleUI::getGaugeColor(int rpm) {
  if (rpm >= RPM_RED_THRESHOLD) return COLOR_GAUGE_RED;
  else if (rpm >= RPM_YELLOW_THRESHOLD) return COLOR_GAUGE_YELLOW;
  else return COLOR_GAUGE_GREEN;
}

uint16_t VehicleUI::getBatteryColor(float battery) {
  if (battery < BATTERY_LOW_THRESHOLD) return COLOR_BATTERY_LOW;
  else if (battery < BATTERY_MEDIUM_THRESHOLD) return COLOR_BATTERY_MED;
  else return COLOR_BATTERY_HIGH;
}

uint16_t VehicleUI::getCurrentColor(int current) {
  if (current < CURRENT_REGEN_THRESHOLD) return COLOR_CURRENT_REGEN;
  else if (current > CURRENT_HIGH_LOAD_THRESHOLD) return COLOR_CURRENT_HIGH;
  else return COLOR_CURRENT_NORMAL;
}

uint16_t VehicleUI::getSpeedModeColor(byte speedMode) {
  if (speedMode == 1) return COLOR_GAUGE_GREEN;
  else if (speedMode == 2) return COLOR_GAUGE_YELLOW;
  else if (speedMode >= 3) return COLOR_GAUGE_RED;
  else return COLOR_TEXT_PRIMARY;
}