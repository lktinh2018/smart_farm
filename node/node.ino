#include "Configuration.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_AM2315.h>
#include <STM32FreeRTOS.h>
#include "STM32LowPower.h"
#include <EEPROM.h>

// AM2315
Adafruit_AM2315 am2315;

// JSON
String dataPackage;

// Semaphore declaration
SemaphoreHandle_t xSemaphore = NULL;


void setup()
{
  // Return value variable
  int ret;

  // Enable serial 
  Serial.begin(BAUD_RATE);
  delay(3000);
  
  ret = readConfigData();
  if(READ_CONFIG_DATA_FAILED == ret) {
    ERROR("Read config data in EEPROM failed");
    return;
  } else {
    LOG("Read config data in EEPROM successful");
  }
  delay(3000);
  
//  ret = initAM2315();
//  if(INIT_AM2315_FAILED == ret) {
//    ERROR("Init temperature & humidity sensor failed");
//    return;
//  } else {
//    LOG("Init temperature & humidity sensor successful");
//  }
//  delay(3000);
  
  ret = initSolenoid();
  if(INIT_SOLENOID_FAILED == ret) {
    ERROR("Init solenoid failed");
    return;
  } else {
    LOG("Init solenoid successful");
  }
  delay(3000);

  ret = initBattery();
  if(INIT_BATTERY_FAILED == ret) {
    ERROR("Init battery failed");
    return;
  } else {
    LOG("Init battery successful");
  }
  delay(3000);

  ret = initLora();
  if(INIT_LORA_FAILED == ret) {
    ERROR("Init lora failed");
    return;
  } else {
    LOG("Init lora successful");
  }
  delay(3000);
  
//  ret = initPowerManagement();
//  if(INIT_POWER_FAILED == ret) {
//    ERROR("Init power failed");
//  } else {
//    LOG("Init power successful");
//  }
//  delay(3000);
  LOG("Init all devices successful");
  delay(3000);
  vTaskStartScheduler();
}

void loop() {}
