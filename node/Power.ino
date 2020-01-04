int initPowerManagement()
{
  LowPower.begin();
  return 0;
}

void loopLower()
{
  LowPower.deepSleep(SLEEP_TIME);
}
