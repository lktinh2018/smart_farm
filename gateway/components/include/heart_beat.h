#ifndef HEART_BEAT_H
#define HEART_BEAT_H

#define SUPPORTED_NODE  3
#define CHECK_TIME      30

unsigned long last_time[SUPPORTED_NODE];


void check_heart_beat(void);

void reset_heart_beat(int device_id);

#endif
