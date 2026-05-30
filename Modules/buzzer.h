#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);
void Buzzer_Ring(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

void Buzzer_Shutup(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);


#endif //__BUZZER_H
 








