#ifndef __USART2_H__
#define __USART2_H__

#include "stm32f10x.h"

void USART2_Init(void);
void USART2_SendString(const char *str);

#endif  //__USART2_H__
