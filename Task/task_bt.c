#include "common.h"

QueueHandle_t xBTQueue = NULL;

// 蓝牙接收拼行缓冲区 
static char bt_rx_buffer[256];
static uint16_t bt_rx_index = 0;

static void ProcessBTCmd(char *data) {
    // 不以 ! 开头 → 不是配网命令，原样回显
    if (data[0] != '!') {
        USART3_SendString(data);
        USART3_SendString("\r\n");
        return;
    }

    // 找第 2 个 ! 定界
    char *end = strchr(data + 1, '!');
    if (end == NULL) {
        USART3_SendString("+WIFI:ERROR,BAD_FORMAT\r\n");
        return;
    }

    *end = '\0';                          // 在第二个 ! 处截断
    char *content = data + 1;             // 跳过第一个 !，得到 "ssid=password"
    char *equal = strchr(content, '=');   // 找等号

    if (equal == NULL) {
        USART3_SendString("+WIFI:ERROR,BAD_FORMAT\r\n");
        return;
    }

    *equal = '\0';                        // 在等号截断
    char *ssid = content;                 // 等号左边 = WiFi 名
    char *pw = equal + 1;                 // 等号右边 = 密码

    // 长度检查：ssid 和密码都必须 >0 且 <32
    int ssidLen = strlen(ssid);
    int pwLen   = strlen(pw);

    if (ssidLen == 0 || ssidLen >= 32 || pwLen == 0 || pwLen >= 32) {
        USART3_SendString("+WIFI:ERROR,EMPTY\r\n");
        return;
    }

    // 填入配置结构体
    WiFiConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.ssid, ssid);
    strcpy(cfg.password, pw);

    LOG("BT: Got WiFi config -> SSID=%s, PWD=%s\r\n", cfg.ssid, cfg.password);

    //塞入配网队列，WiFiSend 收到后会执行配网流程成功/失败的回 JSON 由 WiFiSend 发，这里不回
    if (xQueueSend(xWIFIConfigQueue, &cfg, pdMS_TO_TICKS(500)) != pdPASS) {
        USART3_SendString("+WIFI:ERROR,QUEUE_FULL\r\n");
    }
}

//BT 接收任务
void vTaskBTRecv(void *pvParameters) {
    LOG("BT Recv Task Started\r\n");
    char ch;

    while (1) {
        if (xQueueReceive(xBTQueue, &ch, pdMS_TO_TICKS(10)) == pdPASS) {

            if (ch == '\n') {
                // 换行符 → 结束一行
                bt_rx_buffer[bt_rx_index] = '\0';

                if (bt_rx_index > 0) {
                    // 去掉行尾 \r 
                    if (bt_rx_buffer[bt_rx_index - 1] == '\r') {
                        bt_rx_buffer[bt_rx_index - 1] = '\0';
                    }

                    if (bt_rx_index > 1) {        // 忽略空行
                        ProcessBTCmd(bt_rx_buffer);
                    }
                }

                bt_rx_index = 0;

            } else if (ch == '!' && bt_rx_index > 0) {
                //遇到 ! 且缓冲区已有内容说明 ! 可能出现在行中间把当前内容 + ! 拼完直接解析
                bt_rx_buffer[bt_rx_index++] = ch;
                bt_rx_buffer[bt_rx_index] = '\0';
                ProcessBTCmd(bt_rx_buffer);
                bt_rx_index = 0;

            } else if (ch != '\r') {
                if (bt_rx_index < sizeof(bt_rx_buffer) - 1) {
                    bt_rx_buffer[bt_rx_index++] = ch;
                } else {
                    bt_rx_index = 0;    // 溢出保护
                }
            }
        }
    }
}

void USART3_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch;

    // 清 ORE 溢出标志 
    if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART3);
    }

    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        ch = USART_ReceiveData(USART3);
        xQueueSendFromISR(xBTQueue, &ch, &xHigherPriorityTaskWoken);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
