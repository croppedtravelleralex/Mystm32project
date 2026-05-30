#include "common.h"

int main(void) {
    //使用优先级分组4，FreeRTOS要求Cortex-M3内核使用FreeRTOS必须选择中断优先级分组4
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    //初始化
    LED_Init();
    Buzzer_Init();
    Sensor_Init();
    USART1_Init();
    USART2_Init();
    USART3_Init();
    Buzzer_Shutup(GPIOB, GPIO_Pin_14);  //默认闭嘴

    LOG("\r\n==============================================\r\n");
    LOG("\r\nWiFi + BT Intelligent Detection System Online!\r\n");
    LOG("\r\nSTM32F103 + FreeRTOS\r\n");
    LOG("\r\n==============================================\r\n");

    //创建队列
    xWIFIQueue = xQueueCreate(128, sizeof(char));
    xBTQueue = xQueueCreate(128, sizeof(char));
    xWIFIConfigQueue = xQueueCreate(2, sizeof(WiFiConfig_t));
	xSensorDataQueue =xQueueCreate(4,sizeof(SensorData_t));

    if (xWIFIQueue == NULL ||
            xBTQueue == NULL ||
            xWIFIConfigQueue == NULL ||
            xSensorDataQueue == NULL) {

        LOG("Queue creation failed, system dead loop.");

        while (1);
    }

    LOG("\r\n Queues created successfully!!\r\n");

    xTaskCreate(vTaskWiFiSend, "WiFiSend", configMINIMAL_STACK_SIZE * 2,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskWiFiRecv, "WiFiRecv", configMINIMAL_STACK_SIZE * 3,
                NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskBTRecv, "BTRecv", configMINIMAL_STACK_SIZE * 3,
                NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vTaskSensor, "Sensors", configMINIMAL_STACK_SIZE * 4,
                NULL, tskIDLE_PRIORITY + 2, NULL);

    //启动 FreeRTOS 调度器
    vTaskStartScheduler();

    //理论上不会运行到这里
    while (1) {
    }
}

