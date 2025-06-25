#include "logic.h"

VehicleLogic::VehicleLogic() {
  data.battery = 0;
  data.current = 0;
  data.voltage = 0;
  data.rpm = 0;
  data.regen = false;
  data.brake = false;
  data.reverseMode = false;
  data.speedMode = 1;
  data.usingSifData = true;
  data.mph = 0;
  data.displayedRpm = 0;
  lastPythonData = 0;
  targetRpm = 0;
}

void VehicleLogic::init() {
  Serial.println("Vehicle Logic initialized");
}

void VehicleLogic::parsePythonData(String inputData) {
  if (inputData.startsWith("DATA,")) {
    data.usingSifData = false;
    lastPythonData = millis();
    
    String values[8];
    int valueIndex = 0;
    int startPos = 5; 
    
    for (int i = startPos; i < inputData.length() && valueIndex < 8; i++) {
      if (inputData.charAt(i) == ',' || i == inputData.length() - 1) {
        values[valueIndex] = inputData.substring(startPos, i + (i == inputData.length() - 1 ? 1 : 0));
        valueIndex++;
        startPos = i + 1;
      }
    }
    
    if (valueIndex >= 8) {
      data.battery = constrain(values[0].toFloat(), 0, 100);
      
      int newRpm = constrain(values[1].toInt(), MIN_RPM, MAX_RPM);
      data.rpm = newRpm;
      targetRpm = newRpm;
      
      byte newSpeedMode = constrain(values[2].toInt(), 1, MAX_SPEED_MODES);
      if (isValidSpeedMode(newSpeedMode)) {
        data.speedMode = newSpeedMode;
      }
      
      data.reverseMode = values[3].toInt() == 1;
      data.brake = values[4].toInt() == 1;
      data.regen = values[5].toInt() == 1;
      data.current = values[6].toInt();
      data.voltage = values[7].toFloat();
      data.mph = calculateMph(data.rpm);
    }
  }
}

void VehicleLogic::parseSifData(byte sifData[12]) {
  // Use exact Arduino Nano mapping that worked
  data.battery = sifData[9];
  data.current = sifData[6];
 data.rpm = ((sifData[7] << 8) + sifData[8]) * 1.91;
targetRpm = data.rpm;
data.displayedRpm = data.rpm;  // Remove animation for testing
  data.voltage = sifData[1] * 0.75;  // Fixed: was sifData[10], should be sifData[1]
  
  data.brake = bitRead(sifData[4], 5);
  data.regen = bitRead(sifData[4], 3);
  data.reverseMode = (sifData[5] == 4);
  data.speedMode = sifData[4] & 0x07;
  
  data.mph = calculateMph(data.rpm);
}

void VehicleLogic::updateDataSource() {
  if (millis() - lastPythonData > PYTHON_DATA_TIMEOUT) {
    data.usingSifData = true;
  }
  updateRpmAnimation();
}

void VehicleLogic::updateRpmAnimation() {
  int rpmDiff = targetRpm - data.displayedRpm;
  if (abs(rpmDiff) > 1) {
    data.displayedRpm += rpmDiff / RPM_ANIMATION_SPEED + (rpmDiff > 0 ? 1 : -1);
  } else {
    data.displayedRpm = targetRpm;
  }
  data.displayedRpm = constrain(data.displayedRpm, MIN_RPM, MAX_RPM);
}

float VehicleLogic::calculateMph(int rpm) {
  const float gearRatio = REAR_SPROCKET_TEETH / FRONT_SPROCKET_TEETH;
  const float wheelRPM = rpm / gearRatio;
  const float wheelCircumferenceInches = WHEEL_DIAMETER_INCHES * 3.14159265f;
  const float inchesPerMile = 63360.0f;
  return (wheelRPM * wheelCircumferenceInches * 60.0f) / inchesPerMile;
}

bool VehicleLogic::isValidSpeedMode(byte mode) {
  return (mode >= 1 && mode <= MAX_SPEED_MODES);
}

VehicleData VehicleLogic::getVehicleData() {
  return data;
}

bool VehicleLogic::isUsingSifData() {
  return data.usingSifData;
}

void VehicleLogic::setUsingSifData(bool usingSif) {
  data.usingSifData = usingSif;
}