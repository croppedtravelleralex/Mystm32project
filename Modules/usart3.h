#ifndef __USART3_H__
#define __USART3_H__

#include "stm32f10x.h"

void USART3_Init(void);
void USART3_SendString(const char *str);

#endif  //__USART3_H__
