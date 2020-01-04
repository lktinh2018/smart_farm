#ifndef CONFIGURATION_H
#define CONFIGURATION_H

// Define Device ID for each Node
#define DEVICE_ID 2

#define BAUD_RATE  115200

// Enable/Disable debug feature
#define LOG_PRINT       (0)
#define ERROR_PRINT     (1)

// Battery calculate pin
#define BATTERY_PIN PB1

// Control solenoid pin
#define SOLENOID_PIN PB10

// Config struct saved in EEPROM
typedef struct ConfigType {
  unsigned int autoMode; 
  float minTemp, maxTemp;
  float minHum, maxHum;
  unsigned int sendingCycle;
};
ConfigType configObject;

// Error Type
enum Error_Type {
  ERROR_TYPE_OK = 0,
  READ_CONFIG_DATA_FAILED,
  INIT_AM2315_FAILED,
  INIT_SOLENOID_FAILED,
  INIT_BATTERY_FAILED,
  INIT_LORA_FAILED,
  INIT_POWER_FAILED
};

// Sending flag
bool can_sending_flag = false;

// Config sleep time 
#define SLEEP_TIME 10000

#if LOG_PRINT
  #define LOG(message) Serial.println("[LOG][Device_" + String(DEVICE_ID) + "] " + message)
#else
  #define LOG(message)
#endif

#if ERROR_PRINT
  #define ERROR(message) Serial.println("[ERROR][Device_" + String(DEVICE_ID) + "] " + message)
#else
  #define ERROR(message)
#endif

#endif
