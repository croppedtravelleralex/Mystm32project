#ifndef __ADC_H__
#define __ADC_H__

#include "stm32f10x.h"

//ADC 通道定义
#define ADC_CH_LIGHT     ADC_Channel_0    // 光敏传感器 → PA0 → ADC1_IN0
#define ADC_CH_TEMP      ADC_Channel_1    // 温度传感器 → PA1 → ADC1_IN1

void ADC1_Init(void);                          // 初始化 ADC1（时钟、GPIO、校准）
uint16_t ADC1_ReadChannel(uint8_t ch);         // 读指定通道的 ADC 原始值(0~4095)

#endif  //__ADC_H__
