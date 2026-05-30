#include "common.h"

//USART1 — 调试串口
static SemaphoreHandle_t xPrintfMutex = NULL;

void USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 开时钟：USART1 在 APB2 上，GPIOA 包含 PA9 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    // PA9 → USART1_TX：复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 8N1，115200，只发不收
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);

    // 创建互斥锁，保护 printf1 不被打断
    xPrintfMutex = xSemaphoreCreateMutex();
}

void printf1(const char *format, ...)
{
    static char buffer[256];
    va_list args;

    // 拿互斥锁
    if (xPrintfMutex != NULL) {
        xSemaphoreTake(xPrintfMutex, portMAX_DELAY);
    }

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 逐字节发送，把 \n 展开成 \r\n 
    for (uint16_t i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') {
            // 先发 \r 
            while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
            USART_SendData(USART1, '\r');
        }
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, buffer[i]);
    }

    //释放互斥锁 
    if (xPrintfMutex != NULL) {
        xSemaphoreGive(xPrintfMutex);
    }
}
