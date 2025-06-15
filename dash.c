#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>


#define TFT_CS   10
#define TFT_RST  8  
#define TFT_DC   9


#define sif_pin 2
#define DEBUG_MODE false

#define RPM_LOW_THRESHOLD 2000
#define RPM_MED_THRESHOLD 4000  
#define RPM_HIGH_THRESHOLD 6000
#define RPM_REV_LIMIT 8000

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

short battery = 0;
short current = 0;
short currentPercent = 0;
short rpm = 0;
long faultCode = 0;
bool regen = false;
bool brake = false;
bool reverseMode = false;
byte speedMode = 0;
unsigned long lastTime;
unsigned long lastDuration = 0;
byte lastCrc = 0;
byte data[12];
int bitIndex = -1;

bool newDataAvailable = false;
int displayedRpm = 0;
int targetRpm = 0;
int lastActiveSegments = -1;
bool revLimitFlash = false;
unsigned long flashTimer = 0;
bool archDrawn = false;
bool testMode = false;
unsigned long testTimer = 0;
int testStep = 0;

void setup() {
  Serial.begin(115200);
  
  tft.init(240, 320);
  tft.setRotation(1);
  
  pinMode(sif_pin, INPUT);
  lastTime = micros();
  attachInterrupt(digitalPinToInterrupt(sif_pin), sifChange, CHANGE);
  
  drawInitialUI();
  
  Serial.println("Type 'help' for commands or 'test' to start auto-cycle");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "test") {
      testMode = true;
      testStep = 0;
      testTimer = millis();
      Serial.println("Starting cluster test cycle...");
    } else if (command == "stop") {
      testMode = false;
      Serial.println("Test mode stopped");
    } else if (command.startsWith("rpm=")) {
      int testRpm = command.substring(4).toInt();
      rpm = constrain(testRpm, 0, 8000);
      Serial.println("RPM set to: " + String(rpm));
    } else if (command.startsWith("mode=")) {
      int testModeVal = command.substring(5).toInt();
      speedMode = constrain(testModeVal, 1, 3);
      Serial.println("Speed mode set to: " + String(speedMode));
    } else if (command == "reverse") {
      reverseMode = !reverseMode;
      Serial.println("Reverse: " + String(reverseMode ? "ON" : "OFF"));
    } else if (command.startsWith("battery=")) {
      int testBatt = command.substring(8).toInt();
      battery = constrain(testBatt, 0, 100);
      Serial.println("Battery set to: " + String(battery) + "%");
    } else if (command == "help") {
      Serial.println("Commands:");
      Serial.println("test - Start auto test cycle");
      Serial.println("stop - Stop test mode");
      Serial.println("rpm=#### - Set RPM (0-8000)");
      Serial.println("mode=# - Set speed mode (1-3)");
      Serial.println("reverse - Toggle reverse");
      Serial.println("battery=## - Set battery % (0-100)");
      Serial.println("help - Show this help");
    }
  }
  
  if (testMode && millis() - testTimer > 1000) {
    runTestCycle();
    testTimer = millis();
  }
  
  updateDisplay();
  delay(10);
}

void drawInitialUI() {
  tft.fillScreen(ST77XX_BLACK);
  
  tft.fillCircle(160, 120, 60, ST77XX_YELLOW);
  tft.fillCircle(140, 100, 8, ST77XX_BLACK);
  tft.fillCircle(180, 100, 8, ST77XX_BLACK);
  tft.fillCircle(160, 140, 25, ST77XX_BLACK);
  tft.drawCircle(160, 140, 25, ST77XX_YELLOW);
  
  delay(800);
  tft.fillScreen(ST77XX_BLACK);
  
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 200);
  tft.print("BATTERY:");
  tft.setCursor(170, 200);
  tft.print("VOLTAGE:");
}

void updateDisplay() {
  updateSpeedMode();
  updateRpm();
  
  if (reverseMode) {
    drawReverse();
  } else {
    drawRpmGauge();
  }
  
  updateBottomInfo();
}

void updateSpeedMode() {
  static byte lastMode = 255;
  if (speedMode != lastMode) {
    tft.fillRect(80, 0, 160, 40, ST77XX_BLACK);
    
    if (speedMode == 3) {
      tft.drawRect(130, 3, 60, 27, ST77XX_RED);
      tft.drawRect(131, 4, 58, 25, ST77XX_YELLOW);
      tft.drawRect(132, 5, 56, 23, ST77XX_RED);
    }
    
    tft.setTextColor(speedMode == 3 ? ST77XX_YELLOW : ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(135, 10);
    tft.print("MODE ");
    tft.print(speedMode);
    
    lastMode = speedMode;
  }
}

void drawReverse() {
  static bool lastReverseState = false;
  if (reverseMode != lastReverseState) {
    if (reverseMode) {
      tft.fillRect(20, 35, 280, 130, ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(4);
      tft.setCursor(90, 100);
      tft.print("REVERSE");
    }
    lastReverseState = reverseMode;
  }
}

void updateRpm() {
  static bool lastReverseState = false;
  if (lastReverseState && !reverseMode) {
    tft.fillRect(20, 35, 280, 130, ST77XX_BLACK);
    lastActiveSegments = -1; 
    archDrawn = false; 
  }
  lastReverseState = reverseMode;
  
  if (targetRpm != rpm) {
    targetRpm = rpm;
  }
  
  // Much faster interpolation
  int rpmDiff = targetRpm - displayedRpm;
  if (abs(rpmDiff) > 1) {
    displayedRpm += rpmDiff / 3 + (rpmDiff > 0 ? 2 : -2); 
  } else {
    displayedRpm = targetRpm;
  }
}

void drawRpmGauge() {
  int centerX = 160;
  int centerY = 140;
  int segments = 20;
  int activeSegments = map(displayedRpm, 0, 8000, 0, segments); 
  
  if (!archDrawn) {
    for (int r = 108; r <= 112; r++) {
      for (int angle = -10; angle <= 190; angle += 2) {
        float rad = angle * PI / 180.0;
        int x = centerX + cos(rad) * r;
        int y = centerY - sin(rad) * r;
        tft.drawPixel(x, y, 0x4208); 
      }
    }
    archDrawn = true;
  }
  

  if (activeSegments != lastActiveSegments) {
    for (int i = 0; i < segments; i++) {
      float angle = map(i, 0, segments-1, -10, 190) * PI / 180.0;
      int x = centerX + cos(angle) * 110;
      int y = centerY - sin(angle) * 110;
      tft.fillRect(x-2, y-8, 4, 16, ST77XX_BLACK);
    }
    
    for (int i = 0; i < activeSegments; i++) {
      float angle = map(i, 0, segments-1, -10, 190) * PI / 180.0;
      int x = centerX + cos(angle) * 110;
      int y = centerY - sin(angle) * 110;
      
      uint16_t segColor = ST77XX_GREEN;
      if (displayedRpm >= RPM_HIGH_THRESHOLD) segColor = ST77XX_RED;
      else if (displayedRpm >= RPM_MED_THRESHOLD) segColor = ST77XX_YELLOW;
      
      tft.fillRect(x-2, y-8, 4, 16, segColor);
    }
    
    lastActiveSegments = activeSegments;
  }

  static bool wasOverRevLimit = false;
  if (displayedRpm >= RPM_REV_LIMIT) {
    wasOverRevLimit = true;
    if (millis() - flashTimer > 100) {
      revLimitFlash = !revLimitFlash;
      flashTimer = millis();
      if (revLimitFlash) {
        tft.drawRect(0, 0, 320, 240, ST77XX_RED);
        tft.drawRect(1, 1, 318, 238, ST77XX_RED);
        tft.drawRect(2, 2, 316, 236, ST77XX_RED);
      } else {
        tft.drawRect(0, 0, 320, 240, ST77XX_BLACK);
        tft.drawRect(1, 1, 318, 238, ST77XX_BLACK);
        tft.drawRect(2, 2, 316, 236, ST77XX_BLACK);
      }
    }
  } else if (wasOverRevLimit) {
    tft.drawRect(0, 0, 320, 240, ST77XX_BLACK);
    tft.drawRect(1, 1, 318, 238, ST77XX_BLACK);
    tft.drawRect(2, 2, 316, 236, ST77XX_BLACK);
    wasOverRevLimit = false;
  }
  
  static int lastRpmShown = -999;
  if (abs(displayedRpm - lastRpmShown) > 50) {
    tft.fillRect(60, 70, 200, 70, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(6);
    tft.setCursor(95, 90);
    tft.print(displayedRpm);
    
    tft.setTextSize(1);
    tft.setCursor(200, 110);
    tft.print("RPM");
    
    lastRpmShown = displayedRpm;
  }
}

void updateBottomInfo() {
  static int lastBatt = -1;
  static float lastVolt = -1;
  
  if (battery != lastBatt) {
    tft.fillRect(0, 205, 120, 35, ST77XX_BLACK);
    
    uint16_t battColor = ST77XX_GREEN;
    if (battery < 20) battColor = ST77XX_RED;
    else if (battery < 50) battColor = ST77XX_YELLOW;
    
    tft.setTextColor(battColor);
    tft.setTextSize(2);
    tft.setCursor(10, 215);
    tft.print(battery);
    tft.print("%");
    
    lastBatt = battery;
  }
  
  float voltage = data[1] * 0.75;
  if (abs(voltage - lastVolt) > 0.1) {
    tft.fillRect(150, 205, 170, 35, ST77XX_BLACK);
    
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(2);
    tft.setCursor(170, 215);
    tft.print(voltage, 1);
    tft.print("V");
    
    lastVolt = voltage;
  }
}

void runTestCycle() {
  switch (testStep) {
    case 0:
      rpm = 0;
      battery = 85;
      speedMode = 1;
      reverseMode = false;
      brake = false;
      regen = false;
      data[1] = 97; 
      Serial.println("Test Step 0: Reset to defaults");
      break;
      
    case 1:
      rpm = 1500;
      Serial.println("Test Step 1: Low RPM (1500) - Green zone");
      break;
      
    case 2:
      rpm = 3000;
      speedMode = 2;
      Serial.println("Test Step 2: Medium RPM (3000) - Yellow zone, Mode 2");
      break;
      
    case 3:
      rpm = 5000;
      speedMode = 3;
      Serial.println("Test Step 3: High RPM (5000) - Red zone, Mode 3");
      break;
      
    case 4:
      rpm = 8200;
      Serial.println("Test Step 4: Over rev limit (8200) - Should flash red");
      break;
      
    case 5:
      rpm = 800;
      reverseMode = true;
      speedMode = 1;
      Serial.println("Test Step 5: Reverse mode");
      break;
      
    case 6:
      reverseMode = false;
      rpm = 2000;
      battery = 15; 
      Serial.println("Test Step 6: Low battery (15%) - Should be red");
      break;
      
    case 7:
      battery = 45; 
      Serial.println("Test Step 7: Medium battery (45%) - Should be yellow");
      break;
      
    case 8:
      battery = 90; 
      Serial.println("Test Step 8: High battery (90%) - Should be green");
      break;
      
    case 9:
      data[1] = 80;
      Serial.println("Test Step 9: Lower voltage test (~60V)");
      break;
      
    case 10:
      rpm = 1000;
      brake = true;
      regen = true;
      Serial.println("Test Step 10: Brake + Regen active");
      break;
      
    case 11:

      static int sweepRpm = 0;
      sweepRpm += 200;
      if (sweepRpm > 6000) sweepRpm = 0;
      rpm = sweepRpm;
      brake = false;
      regen = false;
      Serial.println("Test Step 11: RPM sweep (" + String(rpm) + ")");
      break;
      
    case 12:
 
      static int modeTest = 1;
      speedMode = modeTest;
      modeTest++;
      if (modeTest > 3) modeTest = 1;
      rpm = 3000;
      Serial.println("Test Step 12: Mode cycling (Mode " + String(speedMode) + ")");
      break;
      
    default:
   
      testStep = -1;
      Serial.println("Test cycle complete! Type 'test' to restart or 'stop' to end");
      break;
  }
  
  testStep++;
  if (testStep > 12) {
    testStep = 0; // Loop the test
  }
}

void sifChange() {
  int val = digitalRead(sif_pin);
  unsigned long duration = micros() - lastTime;
  lastTime = micros();
  
  if (val == LOW) {
    if (lastDuration > 0) {
      bool bitComplete = false;
      float ratio = float(lastDuration) / float(duration);
      
      if (round(lastDuration / duration) >= 31) {
        bitIndex = 0;
      } else if (ratio > 1.5) {
        bitClear(data[bitIndex / 8], 7 - (bitIndex % 8));
        bitComplete = true;
      } else if (1 / ratio > 1.5) {
        bitSet(data[bitIndex / 8], 7 - (bitIndex % 8));
        bitComplete = true;
      }
      
      if (bitComplete) {
        bitIndex++;
        if (bitIndex == 96) {
          bitIndex = 0;
          byte crc = 0;
          for (int i = 0; i < 11; i++) {
            crc ^= data[i];
          }
          
          if (crc == data[11] && crc != lastCrc) {
            lastCrc = crc;

            if (!testMode) {
              battery = data[9];
              current = data[6];
              currentPercent = data[10];
              rpm = ((data[7] << 8) + data[8]) * 1.91;
              brake = bitRead(data[4], 5);
              regen = bitRead(data[4], 3);
              reverseMode = (data[5] == 4);
              speedMode = data[4] & 0x07;
            }
            
            newDataAvailable = true;
          }
        }
      }
    }
  }
  lastDuration = duration;
}