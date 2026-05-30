#ifndef __LED_H_
#define __LED_H_

#include "stm32f10x.h"

//LED 引脚宏
#define LED_GREEN_PORT    GPIOC
#define LED_GREEN_PIN     GPIO_Pin_13     // PC13 绿色=WiFi状态

#define LED_YELLOW_PORT   GPIOB
#define LED_YELLOW_PIN    GPIO_Pin_0      // PB0 黄色=配网/处理中

#define LED_RED_PORT      GPIOB
#define LED_RED_PIN       GPIO_Pin_1      // PB1 红色=异常

//统一操作宏
#define LED_ON(port, pin)   GPIO_ResetBits(port, pin)   // 低电平亮
#define LED_OFF(port, pin)  GPIO_SetBits(port, pin)     // 高电平灭

void LED_Init(void);


#endif
