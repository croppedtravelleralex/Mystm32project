#include "common.h"

void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOC, &GPIO_InitStruct);
    LED_OFF(LED_GREEN_PORT, LED_GREEN_PIN);    // 初始化 = 灭

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_0;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    LED_OFF(LED_RED_PORT, LED_RED_PIN);        // 红灯灭
    LED_OFF(LED_YELLOW_PORT, LED_YELLOW_PIN);  // 黄灯灭
}
