#ifndef __COMMON_H__
#define __COMMON_H__

#include "stm32f10x.h"

// C标准库
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// RTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//config
#define WIFI_UART_BAUDRATE       115200
#define BT_UART_BAUDRATE         9600

#define WIFI_CMD_INTERVAL_MS     100		//间隔时间100ms，

//蓝牙配网
typedef struct {
    char ssid[32];    						//wifi名
    char password[32];    					//wifi密码
    uint8_t valid;                         // 0xA5=有效, 其他=无效
} WiFiConfig_t;

//传感器
#define SENSOR_SAMPLE_INTERVAL_MS  10000   // 传感器采样间隔：10秒

typedef struct SensorData {
    float light_voltage;     		   // 光敏电压(V)
    float temp_voltage;      		   // 温度电压(V)
    float light_lux;        		   // 光照强度(lux)，由光敏电压换算得到
    float temp_celsius;     		   // 温度(°C)，由 NTC 电压换算得到
} SensorData_t;

//上传数据
#define SERVER_IP           "8.163.32.25"  // 服务器IP
#define SERVER_PORT          8866          // 服务器端口
#define DEVICE_ID            "STM32F103_001"  // 设备唯一标识：服务器用它区分是哪块板子上传的数据
#define DEVICE_API_VERSION   "2.0"          // 上传协议版本：本版本上传 light_lux/temp_celsius


// 驱动
#include "usart1.h"    						// 调试串口
#include "usart2.h"    						// WiFi串口
#include "usart3.h"    						// 蓝牙串口
#include "led.h"
#include "buzzer.h"
#include "w25q64.h"
#include "adc.h"
#include "sensor.h"

//task
#include "task_wifi.h"
#include "task_bt.h"
#include "task_sensor.h"



// debugging
#define DEBUG_ENABLE    1
#if DEBUG_ENABLE
#define LOG(fmt, ...)    printf1(fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#endif

