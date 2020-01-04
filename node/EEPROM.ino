int readConfigData() 
{
  int address = 0; 
  EEPROM.get(address, configObject);
  return 0;
}
