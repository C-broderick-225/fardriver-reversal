//SIF Loggy

#define sif_pin 2
#define DEBUG_MODE false  


short battery = 0;
short current = 0;
short currentPercent = 0;
short rpm = 0;
bool regen = false;
bool brake = false;
bool reverseMode = false;
byte speedMode = 0;
unsigned long lastTime;
unsigned long lastDuration = 0;
byte lastCrc = 0;
byte data[12];
int bitIndex = -1;


unsigned long startTime;
bool headerPrinted = false;

void setup() {
  Serial.begin(115200);

  pinMode(sif_pin, INPUT);
  lastTime = micros();
  attachInterrupt(digitalPinToInterrupt(sif_pin), sifChange, CHANGE);
  
  startTime = millis();
  
  Serial.println("SIF Vehicle Data Logger Started");
  Serial.println("Waiting for SIF data...");
  delay(1000);
}

void loop() {
  delay(100);
}

void printCSVHeader() {
  Serial.println("Timestamp,B0,B1,B2,B3,B4,B5,B6,B7,B8,B9,B10,B11,Battery%,LoadVoltage,RPM,SpeedMode,Reverse,Brake,Regen,PowerState,B2_Direction,EstPower,B4_Bits,B5_Hex");
  headerPrinted = true;
}

void logData() {
  if (!headerPrinted) {
    printCSVHeader();
  }
  

  float timestamp = (millis() - startTime) / 1000.0;
  

  Serial.print(timestamp, 1);
  Serial.print(",");
  
 
  for (int i = 0; i < 12; i++) {
    Serial.print(data[i]);
    Serial.print(",");
  }
  

  Serial.print(battery);
  Serial.print(",");
  Serial.print(data[10]);  // B10 = Load voltage (your discovery!)
  Serial.print(",");
  Serial.print(rpm);
  Serial.print(",");
  Serial.print(speedMode);
  Serial.print(",");
  Serial.print(reverseMode ? 1 : 0);
  Serial.print(",");
  Serial.print(brake ? 1 : 0);
  Serial.print(",");
  Serial.print(regen ? 1 : 0);
  Serial.print(",");
  

  if (rpm > 0 && data[10] > 0) {
    Serial.print("LOAD");
  } else if (regen) {
    Serial.print("REGEN");
  } else if (rpm == 0) {
    Serial.print("IDLE");
  } else {
    Serial.print("COAST");
  }
  Serial.print(",");
  
  Serial.print(data[2]);  
  Serial.print(",");
  
  // rpm may be slightly off... very slightly
  float estPower = (rpm > 0 && data[10] > 0) ? (rpm * data[10]) / 100.0 : 0;
  Serial.print(estPower, 1);
  Serial.print(",");
  
  Serial.print("0b");
  for (int i = 7; i >= 0; i--) {
    Serial.print(bitRead(data[4], i));
  }
  Serial.print(",");

  Serial.print("0x");
  if (data[5] < 16) Serial.print("0");
  Serial.println(data[5], HEX);
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
      } else {
        if (DEBUG_MODE) {
          Serial.println(String(duration) + "-" + String(lastDuration));
        }
      }
      
      if (bitComplete) {
        bitIndex++;
        if (bitIndex == 96) {
          bitIndex = 0;
          byte crc = 0;
          for (int i = 0; i < 11; i++) {
            crc ^= data[i];
          }
          
          if (crc != data[11]) {
            if (DEBUG_MODE) {
              Serial.println("CRC FAILURE: " + String(crc) + "-" + String(data[11]));
            }
          }
          
          if (crc == data[11] && crc != lastCrc) {
            lastCrc = crc;
            
   
            battery = data[9];
            current = data[6];
            currentPercent = data[10];
            rpm = ((data[7] << 8) + data[8]) * 1.91;
            brake = bitRead(data[4], 5);
            regen = bitRead(data[4], 3);
            reverseMode = (data[5] == 4);
            speedMode = data[4] & 0x07;
            
            // Log the data
            logData();
          }
        }
      }
    }
  }
  lastDuration = duration;
}