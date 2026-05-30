#include "common.h"

//ADC1 初始化
void ADC1_Init(void) {
    GPIO_InitTypeDef  GPIO_InitStruct;
    ADC_InitTypeDef   ADC_InitStruct;

    //开时钟：PA0/PA1 在 GPIOA 上，ADC1 有自己的外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    //ADC 时钟分频：72MHz / 6 = 12MHz，满足 ADC 最大 14MHz 限制
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    //PA0(光敏) 和 PA1(温度) 设为模拟输入模式
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AIN;     // 模拟输入（ADC 专用）
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    //ADC1 参数配置
    ADC_InitStruct.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStruct.ADC_ScanConvMode       = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStruct.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStruct.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStruct);

    //使能 ADC1
    ADC_Cmd(ADC1, ENABLE);

    //ADC 校准 
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));    // 等待复位完成

    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));         // 等待校准完成
}

//读取指定通道的 ADC 值

uint16_t ADC1_ReadChannel(uint8_t ch) {

    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_239Cycles5);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

    return ADC_GetConversionValue(ADC1);
}
