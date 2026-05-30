#ifndef __TASK_BT_H__
#define __TASK_BT_H__

#include "FreeRTOS.h"
#include "queue.h"

void vTaskBTRecv(void *pvParameters);     // 蓝牙配网接收任务

extern QueueHandle_t xBTQueue;            // 蓝牙接收队列句柄

#endif  //__TASK_BT_H__
