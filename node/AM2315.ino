int initAM2315()
{
  if(!am2315.begin()) {
    return INIT_AM2315_FAILED;
  }
  delay(1500);
  return 0;
}
