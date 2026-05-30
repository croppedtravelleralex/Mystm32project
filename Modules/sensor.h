#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "stm32f10x.h"

typedef struct SensorData SensorData_t;

//传感器校准参数

#define LDR_R_DIVIDER     10000.0f    // 光敏分压电阻 10kΩ
#define LDR_R_10LUX       8000.0f     // 10 lux 时光敏阻值(Ω)，仅供参考
#define VREF              3.3f        // ADC 参考电压（STM32F103 = 3.3V），保留原宏名
#define NTC_B_VALUE       3950.0f     // NTC B 值：描述热敏电阻阻值随温度变化的参数
#define NTC_R_25          10000.0f    // NTC 在 25°C 时的阻值 10kΩ
#define NTC_R_DIVIDER     10000.0f    // NTC 分压电阻 10kΩ
#define ADC_REF_VOLTAGE   3.3f        // ADC 参考电压 3.3V
#define NTC_T0_KELVIN     298.15f	  // 25℃开氏温度
#define ABSOLUTE_ZERO     273.15f	  // 0℃ = 273.15K

void Sensor_Init(void);                  // 初始化传感器（实质是初始化 ADC1）
uint16_t Sensor_ReadLightRaw(void);       // 读光敏 ADC 原始值(0~4095)
uint16_t Sensor_ReadTempRaw(void);        // 读温度 ADC 原始值(0~4095)
float Sensor_ReadLightVoltage(void);      // 读光敏电压(V)
float Sensor_ReadTempVoltage(void);       // 读温度电压(V)
float Sensor_ReadLightLux(void);          // 读光照强度(lux)，由光敏电压换算得到
float Sensor_ReadTempCelsius(void);       // 读温度(°C)，由 NTC 电压换算得到
void Sensor_ReadAll(SensorData_t *data);  // 一次性读取所有传感器数据

#endif  //__SENSOR_H__
