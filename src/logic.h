#ifndef LOGIC_H
#define LOGIC_H

#include <Arduino.h>
#define MAX_RPM 12000
#define MIN_RPM 0
#define MAX_SPEED_MODES 3

#define WHEEL_DIAMETER_INCHES 10.0f
#define FRONT_SPROCKET_TEETH 11.0f
#define REAR_SPROCKET_TEETH 54.0f
#define SIF_DATA_TIMEOUT 2000
#define PYTHON_DATA_TIMEOUT 2000
#define RPM_ANIMATION_SPEED 1
#define RPM_GREEN_THRESHOLD 4000
#define RPM_YELLOW_THRESHOLD 6000
#define RPM_RED_THRESHOLD 7000
#define BATTERY_LOW_THRESHOLD 20.0
#define BATTERY_MEDIUM_THRESHOLD 50.0
#define CURRENT_REGEN_THRESHOLD -10
#define CURRENT_HIGH_LOAD_THRESHOLD 100
#define SIF_VOLTAGE_MULTIPLIER 0.75

struct VehicleData {
  float battery;
  int current;
  float voltage;
  int rpm;
  bool regen;
  bool brake;
  bool reverseMode;
  byte speedMode;
  bool usingSifData;
  float mph;
  int displayedRpm;
};

class VehicleLogic {
private:
  VehicleData data;
  unsigned long lastPythonData;
  int targetRpm;
  
public:
  VehicleLogic();
  void init();
  void parsePythonData(String inputData);
  void parseSifData(byte sifData[12]);
  void updateDataSource();
  void updateRpmAnimation();
  float calculateMph(int rpm);
  bool isValidSpeedMode(byte mode);
  VehicleData getVehicleData();
  bool isUsingSifData();
  void setUsingSifData(bool usingSif);
};

#endif