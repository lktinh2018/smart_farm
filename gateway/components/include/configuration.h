#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define LOG_PRINT(message)  printf("%s \n", message)
#define TRACE_PRINT()       printf("Line number: %d", __LINE__)
#define ERROR_PRINT(error_message)  printf("Error occurred at line: %d. Message = %s \n", __LINE__, error_message)

int wifi_connected_flag;
int mqtt_connected_flag;

SemaphoreHandle_t xSemaphoreLora;

#endif