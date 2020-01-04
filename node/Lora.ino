int initLora()
{
  xTaskCreate(
    loraTask
    , (const portCHAR *)"LoraTask"
    , 512
    , NULL
    , 1
    , NULL );
  return 0;
}


void loraTask(void* args)
{
  StaticJsonDocument<1024> root;
  float currentTemp, currentHum;
  String data;
  unsigned long lastTime = 0;
  unsigned int sendingCycle;
  
  // Get sending cycle from EEPROM
  if (!configObject.sendingCycle || configObject.sendingCycle == 4294967295) {
    sendingCycle = 5;
  } else {
    sendingCycle = configObject.sendingCycle;
  }
  sendingCycle *= 1000;

  while (1) {
    // Read data from gateway
    while (Serial.available()) {
      char inChar = (char)Serial.read();
      data += inChar;

      // Check begin character
      if(data[0] != '{') {
        data = "";
        continue;
      } else {
        // Check end character
        if(data.indexOf("\r\n") > 0) {
          handleReceivedData(data);
//          LOG(data);
//          delay(2000);
          data = "";
        }
      }
    }

    if (millis() - lastTime >= sendingCycle && can_sending_flag) {
      root["device_id"]                 = DEVICE_ID;
      //currentTemp = root["temperature"] = am2315.readTemperature();
      //currentHum  = root["humidity"]    = am2315.readHumidity();
      currentTemp = root["temperature"] = 100;
      currentHum  = root["humidity"]    = 100;
      root["battery"]                   = readBattery();

      serializeJson(root, dataPackage);
      Serial.println(dataPackage);
      delay(3000);
      dataPackage = "";

      // Active solenoid if any
//      if (configObject.autoMode) {
//        if (configObject.minTemp <= currentTemp || currentTemp <= configObject.maxTemp) {
//          turnOnSolenoid();
//        }
//      }
      lastTime = millis();
      can_sending_flag = false;
    }
  }
}

void handleReceivedData(String data)
{
  DynamicJsonDocument jsonObject(2048);
  DeserializationError jsonError;

  jsonError = deserializeJson(jsonObject, data);
  if (jsonError) {
    String message;
    message = "DeserializeJson failed: " + String(jsonError.c_str());
    ERROR(message);
    return;
  }

  uint8_t deviceID = jsonObject["device_id"];
  if (deviceID == DEVICE_ID) {
    uint8_t command = jsonObject["type"];
    switch (command) {
      case 1: {
          unsigned int autoMode = jsonObject["auto_mode"];
          float minTemp = jsonObject["temp"]["min_temp"];
          float maxTemp = jsonObject["temp"]["max_temp"];
          float minHum = jsonObject["hum"]["min_hum"];
          float maxHum = jsonObject["hum"]["max_hum"];
          unsigned int sendingCycle = jsonObject["sending_cycle"];

          // Save config data into EPPROM
          configObject.autoMode       = autoMode;
          configObject.minTemp        = minTemp;
          configObject.maxTemp        = maxTemp;
          configObject.minHum         = minHum;
          configObject.maxHum         = maxHum;
          configObject.sendingCycle   = sendingCycle;

          int address = 0;

          EEPROM.put(address, configObject);
          LOG("Written config data successful !");
          break;
        }
      case 2: {
          uint8_t solenoid = jsonObject["solenoid"];
          if (solenoid) {
            turnOnSolenoid();
            LOG("Turn on solenoid successful");
          } else {
            turnOffSolenoid();
            LOG("Turn off solenoid successful");
          }
          break;
        }
      case 3: {
        can_sending_flag = true;
        //LOG("Received can_sending_flag");
      }
    }
  }
  jsonObject.clear();
}
