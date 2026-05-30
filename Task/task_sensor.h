#ifndef __TASK_SENSOR_H__
#define __TASK_SENSOR_H__

//每 10 秒采集光敏+温度电压，HTTP 上传到服务器上传时亮黄灯，失败红灯+蜂鸣器

void vTaskSensor(void *pvParameters);

#endif  //__TASK_SENSOR_H__
