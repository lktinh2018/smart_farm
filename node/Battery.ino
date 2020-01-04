int initBattery()
{
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);
  return 0;
}

int readBattery()
{
  return analogRead(BATTERY_PIN);
}
