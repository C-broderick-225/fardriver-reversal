#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "USB.h"
#include "ui.h"
#include "logic.h"

#define TFT_CS   12
#define TFT_DC   13
#define TFT_RST  14
#define SIF_PIN  4

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
VehicleLogic vehicleLogic;
VehicleUI vehicleUI;

volatile unsigned long lastTime;
volatile unsigned long lastDuration = 0;
volatile byte lastCrc = 0;
volatile byte data[12];
volatile int bitIndex = -1;
volatile bool newDataAvailable = false;

unsigned long lastDataSent = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long sifPacketCount = 0;
bool debugMode = false;

void IRAM_ATTR sifChange();
void sendDataToLogger(byte rawSifData[12]);

void sendDataToLogger(byte rawSifData[12]) {
  VehicleData vehicleData = vehicleLogic.getVehicleData();
  float timestamp = millis() / 1000.0;
  
  String powerState = "IDLE";
  if (vehicleData.regen) powerState = "REGEN";
  else if (vehicleData.brake) powerState = "COAST"; 
  else if (vehicleData.current > 10) powerState = "LOAD";
  
  float estPower = abs(vehicleData.current * vehicleData.voltage / 1000.0);
  int b2Direction = (vehicleData.rpm > 100) ? 1 : 0;
  
  Serial.print(timestamp, 3);
  Serial.print(",");
  
  for (int i = 0; i < 12; i++) {
    Serial.print(rawSifData[i]);
    Serial.print(",");
  }
  
  Serial.print((int)vehicleData.battery);           
  Serial.print(",");
  Serial.print((int)(vehicleData.voltage * 1.33));  
  Serial.print(",");
  Serial.print(vehicleData.rpm);                    
  Serial.print(",");
  Serial.print(vehicleData.speedMode);              
  Serial.print(",");
  Serial.print(vehicleData.reverseMode ? 1 : 0);    
  Serial.print(",");
  Serial.print(vehicleData.brake ? 1 : 0);          
  Serial.print(",");
  Serial.print(vehicleData.regen ? 1 : 0);          
  Serial.print(",");
  Serial.print(powerState);                  
  Serial.print(",");
  Serial.print(b2Direction);                 
  Serial.print(",");
  Serial.print(estPower, 2);                 
  Serial.println();
  Serial.flush();
}

void setup() {
  USB.begin();
  Serial.begin(115200);
  delay(1000);
  
  SPI.begin(7, -1, 11, -1);
  tft.init(240, 320);
  tft.setRotation(3);
  
  vehicleLogic.init();
  vehicleUI.init(&tft);
  vehicleUI.drawStartupScreen();
  
  pinMode(SIF_PIN, INPUT);
  lastTime = micros();
  attachInterrupt(digitalPinToInterrupt(SIF_PIN), sifChange, CHANGE);
  
  Serial.println("Timestamp,Byte0,Byte1,Byte2,Byte3,Byte4,Byte5,Byte6,Byte7,Byte8,Byte9,Byte10,Byte11,Battery,LoadVoltage,RPM,SpeedMode,Reverse,Brake,Regen,PowerState,B2Direction,EstPower");
  Serial.println("# ESP32-S2 SIF Reader Started");
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (newDataAvailable && vehicleLogic.isUsingSifData()) {
    newDataAvailable = false;
    sifPacketCount++;
    
    byte localData[12];
    noInterrupts();
    for (int i = 0; i < 12; i++) {
      localData[i] = data[i];
    }
    interrupts();
    
    vehicleLogic.parseSifData(localData);
    sendDataToLogger(localData);
    lastDataSent = currentMillis;
  }
  
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.equals("DEBUG_ON")) {
      debugMode = true;
      Serial.println("# Debug mode enabled");
    } else if (command.equals("DEBUG_OFF")) {
      debugMode = false;
      Serial.println("# Debug mode disabled");
    } else if (command.equals("STATUS")) {
      Serial.print("# SIF Packets: ");
      Serial.print(sifPacketCount);
      Serial.print(", Using SIF: ");
      Serial.println(vehicleLogic.isUsingSifData() ? "YES" : "NO");
    } else if (command.equals("SIF_OFF")) {
      vehicleLogic.setUsingSifData(false);
      Serial.println("# SIF disabled - using Python data");
    } else if (command.equals("SIF_ON")) {
      vehicleLogic.setUsingSifData(true);
      Serial.println("# SIF enabled");
    } else {
      vehicleLogic.parsePythonData(command);
    }
  }
  
  vehicleLogic.updateDataSource();
  
  if (currentMillis - lastDataSent > 200) {
    byte localData[12];
    noInterrupts();
    for (int i = 0; i < 12; i++) {
      localData[i] = data[i];
    }
    interrupts();
    
    sendDataToLogger(localData);
    lastDataSent = currentMillis;
  }
  
  if (currentMillis - lastDisplayUpdate > 50) {
    VehicleData vehicleData = vehicleLogic.getVehicleData();
    vehicleUI.updateDisplay(vehicleData);
    lastDisplayUpdate = currentMillis;
  }
  
  if (debugMode && currentMillis % 1000 == 0) {
    Serial.print("# DEBUG - Packets: ");
    Serial.print(sifPacketCount);
    Serial.print(", Bit index: ");
    Serial.println(bitIndex);
  }
}

// Use working Arduino Nano interrupt logic
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
        for (int i = 0; i < 12; i++) data[i] = 0;
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
            newDataAvailable = true;
          }
        }
      }
    }
  }
  lastDuration = duration;
}