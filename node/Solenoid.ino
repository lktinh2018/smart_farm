int initSolenoid()
{
  pinMode(SOLENOID_PIN, OUTPUT);
  return 0;
}

void turnOnSolenoid()
{
  digitalWrite(SOLENOID_PIN, HIGH);
}

void turnOffSolenoid()
{
  digitalWrite(SOLENOID_PIN, LOW);
}
