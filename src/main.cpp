#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "USB.h"
#include "ui.h"
#include "logic.h"

#define TFT_CS   12  // GPIO12 (Chip Select)
#define TFT_DC   13  // GPIO13 (Data/Command)
#define TFT_RST  14  // GPIO14 (Reset)
#define SIF_PIN  4   // GPIO4 (SIF input)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
VehicleLogic vehicleLogic;
VehicleUI vehicleUI;

// Simple interrupt variables (matching Nano approach)
volatile unsigned long lastTime;
volatile unsigned long lastDuration = 0;
volatile byte lastCrc = 0;
volatile byte sifData[12];
volatile int bitIndex = -1;
volatile bool newDataAvailable = false;

unsigned long lastUpdate = 0;

void IRAM_ATTR sifChange();

void setup() {
  // Initialize USB for ESP32-S2
  USB.begin();
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SPI with ESP32-S2 specific pins
  SPI.begin(7, -1, 11, -1); // SCK=7, MISO=unused, MOSI=11
  
  // Initialize display
  tft.init(240, 320);
  tft.setRotation(3); // Landscape 320x240
  
  // Initialize vehicle systems
  vehicleLogic.init();
  vehicleUI.init(&tft);
  
  vehicleUI.drawStartupScreen();
  
  // Setup interrupt pin - same as working Nano
  pinMode(SIF_PIN, INPUT);
  lastTime = micros();
  
  // Attach interrupt
  attachInterrupt(digitalPinToInterrupt(SIF_PIN), sifChange, CHANGE);
}

void loop() {
  // Handle serial data
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    vehicleLogic.parsePythonData(command);
  }
  
  // Process SIF data if available
  if (newDataAvailable && vehicleLogic.isUsingSifData()) {
    newDataAvailable = false;
    
    // Copy volatile data to local array
    byte localSifData[12];
    noInterrupts();
    for (int i = 0; i < 12; i++) {
      localSifData[i] = sifData[i];
    }
    interrupts();
    
    // Parse the data
    vehicleLogic.parseSifData(localSifData);
  }
  
  vehicleLogic.updateDataSource();
  
  // Update display
  if (millis() - lastUpdate > 10) {
    VehicleData data = vehicleLogic.getVehicleData();
    vehicleUI.updateDisplay(data);
    lastUpdate = millis();
  }
  
  delay(5);
}

// Simple interrupt handler (exactly like working Nano)
void IRAM_ATTR sifChange() {
  int val = digitalRead(SIF_PIN);
  unsigned long duration = micros() - lastTime;
  lastTime = micros();
  
  if (val == LOW) {
    if (lastDuration > 0) {
      bool bitComplete = false;
      float ratio = float(lastDuration) / float(duration);
      
      if (round(lastDuration / duration) >= 31) {
        bitIndex = 0;
      } else if (ratio > 1.5) {
        bitClear(sifData[bitIndex / 8], 7 - (bitIndex % 8));
        bitComplete = true;
      } else if (1 / ratio > 1.5) {
        bitSet(sifData[bitIndex / 8], 7 - (bitIndex % 8));
        bitComplete = true;
      }
      
      if (bitComplete) {
        bitIndex++;
        if (bitIndex == 96) {
          bitIndex = 0;
          byte crc = 0;
          for (int i = 0; i < 11; i++) {
            crc ^= sifData[i];
          }
          
          if (crc == sifData[11] && crc != lastCrc) {
            lastCrc = crc;
            newDataAvailable = true;
          }
        }
      }
    }
  }
  lastDuration = duration;
}