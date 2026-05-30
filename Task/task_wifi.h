#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include "FreeRTOS.h"
#include "queue.h"

void vTaskWiFiSend(void *pvParameters);
void vTaskWiFiRecv(void *pvParameters);
int HTTP_UploadData(const char *data);

extern QueueHandle_t xWIFIQueue;
extern QueueHandle_t xWIFIConfigQueue;
extern QueueHandle_t xSensorDataQueue;

#endif
