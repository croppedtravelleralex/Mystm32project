#include "common.h"

//蜂鸣器模块
void Buzzer_Init() {
    // 开 GPIOB 时钟（PB14）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    Buzzer_Shutup(GPIOB, GPIO_Pin_14);              // 初始化 = 不响
}

// 响：输出低电平
void Buzzer_Ring(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    GPIO_ResetBits(GPIOx, GPIO_Pin);
}

// 停：输出高电平 
void Buzzer_Shutup(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    GPIO_SetBits(GPIOx, GPIO_Pin);
}
