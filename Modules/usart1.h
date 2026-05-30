#ifndef __USART1_H__
#define __USART1_H__

#include "stm32f10x.h"
#include <stdarg.h>

void USART1_Init(void);
void printf1(const char *format, ...);

#endif  //__USART1_H__

