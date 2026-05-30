#include "common.h"

QueueHandle_t xWIFIQueue = NULL;
QueueHandle_t xWIFIConfigQueue = NULL;      // 蓝牙→WiFi：配网信息
QueueHandle_t xSensorDataQueue = NULL;      // 传感器数据上传队列（预留）

static SemaphoreHandle_t xWiFiConnectSem = NULL;  // 二进制信号量：通知 "WiFi 连上了"

// WiFi 接收缓冲区：拼行用
static char wifi_rx_buffer[256];
static uint16_t wifi_rx_index = 0;

//WiFi 接收任务

static void ProcessWiFiData(char *data) {

    if (strstr(data, "+CWJAP:") != NULL) {
        // 格式：+CWJAP:"ssid","mac",ch,rssi
        LOG("  -> ESP already on WiFi!\r\n");
        LED_ON(LED_GREEN_PORT, LED_GREEN_PIN);    // 绿灯常亮 = 已连

        if (xWiFiConnectSem != NULL) {
            xSemaphoreGive(xWiFiConnectSem);       // 告诉 WiFiSend：不用配网了
        }

    } else if (strstr(data, "WIFI CONNECTED") != NULL) {
        LOG("  -> WiFi Connected!\r\n");
        LED_ON(LED_GREEN_PORT, LED_GREEN_PIN);    // 绿灯常亮

        if (xWiFiConnectSem != NULL) {
            xSemaphoreGive(xWiFiConnectSem);       // 唤醒正在等配网结果的 WiFiSend
        }

    } else if (strstr(data, "WIFI DISCONNECT") != NULL) {
        LOG("  -> WiFi Disconnected!\r\n");
        LED_OFF(LED_GREEN_PORT, LED_GREEN_PIN);   // 绿灯灭

    } else if (strstr(data, "ERROR") != NULL) {
        LOG("  -> AT Command ERROR\r\n");
        // ERROR 也喂信号量，让 WiFiSend 及时知道失败，不用傻等 15 秒 
        if (xWiFiConnectSem != NULL) {
            xSemaphoreGive(xWiFiConnectSem);
        }

    } else if (strstr(data, "OK") != NULL) {
        LOG("  -> AT Command OK\r\n");

    } else {
        // 未知内容，直接打印出来便于调试
        LOG("  -> Data: %s\r\n", data);
    }
}

// WiFi 发送任务
void vTaskWiFiSend(void *pvParameters) {
    LOG("WiFi Send Task Started!\r\n");
    uint32_t count = 0;

    xWiFiConnectSem = xSemaphoreCreateBinary();

    vTaskDelay(pdMS_TO_TICKS(3000));

    LOG("Checking ESP-01S...\r\n");
    USART2_SendString("AT\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG("Checking WiFi status...\r\n");
    USART2_SendString("AT+CWJAP?\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (xSemaphoreTake(xWiFiConnectSem, pdMS_TO_TICKS(3000)) == pdTRUE) {
        LOG("ESP already connected to WiFi!\r\n");

        //初始化 ESP 参数
        USART2_SendString("ATE0\r\n");              // 关回显
        vTaskDelay(pdMS_TO_TICKS(500));
        USART2_SendString("AT+CWMODE=1\r\n");       // STA
        vTaskDelay(pdMS_TO_TICKS(500));

        // 蓝牙回 "已连上保存的 WiFi"
        USART3_SendString("{\"status\":0,\"wifi_name\":\"saved\"}\r\n");
        LOG("BT reply: {\"status\":0,\"wifi_name\":\"saved\"}\r\n");

    } else {
        // 没连上 → 需要用蓝牙配网 
        LOG("ESP not connected, need BT provisioning.\r\n");

        // 初始化 ESP 
        USART2_SendString("ATE0\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        USART2_SendString("AT+CWMODE=1\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));

        //阻塞等蓝牙配网（portMAX_DELAY = 永远等，直到手机发来配网命令）
        LOG("Waiting for BT WiFi config...\r\n");
        WiFiConfig_t cfg;
        xQueueReceive(xWIFIConfigQueue, &cfg, portMAX_DELAY);
        LOG("Got BT config: SSID=%s, PWD=%s\r\n", cfg.ssid, cfg.password);

        //发 AT+CWJAP 连接 WiFi 
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 cfg.ssid, cfg.password);
        USART2_SendString(cmd);
        LOG("Sent: %s", cmd);

        // 先清掉信号量
        xSemaphoreTake(xWiFiConnectSem, 0);

        // 等 "WIFI CONNECTED" 最长 15 秒
        if (xSemaphoreTake(xWiFiConnectSem, pdMS_TO_TICKS(15000)) == pdTRUE) {
            char reply[64];
            snprintf(reply, sizeof(reply),
                     "{\"status\":0,\"wifi_name\":\"%s\"}\r\n", cfg.ssid);
            USART3_SendString(reply);
            LOG("BT reply: %s", reply);
        } else {
            USART3_SendString("{\"status\":1}\r\n");  // 蓝牙回失败
            LOG("WiFi connect timeout (15s)!\r\n");
        }
    }

    //进入保活循环
    LOG("Entering keepalive loop.\r\n");

    while (1) {
        count++;
        WiFiConfig_t newCfg;
        if (xQueueReceive(xWIFIConfigQueue, &newCfg, 0) == pdPASS) {
            LOG("New WiFi config from BT: SSID=%s\r\n", newCfg.ssid);

            // 断开当前 WiFi
            USART2_SendString("AT+CWQAP\r\n");
            vTaskDelay(pdMS_TO_TICKS(2000));

            // 重新初始化 ESP 
            USART2_SendString("ATE0\r\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            USART2_SendString("AT+CWMODE=1\r\n");
            vTaskDelay(pdMS_TO_TICKS(500));

            //连接新 WiFi
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
                     newCfg.ssid, newCfg.password);
            USART2_SendString(cmd);
            LOG("Sent: %s", cmd);

            xSemaphoreTake(xWiFiConnectSem, 0);      // 清信号量

            if (xSemaphoreTake(xWiFiConnectSem, pdMS_TO_TICKS(15000)) == pdTRUE) {
                char reply[64];
                snprintf(reply, sizeof(reply),
                         "{\"status\":0,\"wifi_name\":\"%s\"}\r\n", newCfg.ssid);
                USART3_SendString(reply);
                LOG("BT reply: %s", reply);
            } else {
                USART3_SendString("{\"status\":1}\r\n");
                LOG("WiFi switch timeout (15s)!\r\n");
            }
        }

        //定期 AT 保活（每 60 秒发一次 AT 确认 ESP 还在线）
        if (count % 600 == 0) {
            USART2_SendString("AT\r\n");
            LOG("[%d] Send: AT\r\n", count);
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_CMD_INTERVAL_MS));  // 100ms
    }
}

//WiFi 接收任务
void vTaskWiFiRecv(void *pvParameters) {
    LOG("WiFi Recv Task Started!\r\n");
    char ch;

    while (1) {
        if (xQueueReceive(xWIFIQueue, &ch, pdMS_TO_TICKS(10)) == pdPASS) {

            if (ch == '\n') {
                wifi_rx_buffer[wifi_rx_index] = '\0';

                if (wifi_rx_index > 0) {
                    // 去掉行尾的 \r
                    if (wifi_rx_buffer[wifi_rx_index - 1] == '\r') {
                        wifi_rx_buffer[wifi_rx_index - 1] = '\0';
                    }

                    ProcessWiFiData(wifi_rx_buffer);
                }

                wifi_rx_index = 0;       // 重置缓冲区

            } else if (ch != '\r') {
                // 普通字符，拼入缓冲区（忽略 \r）
                if (wifi_rx_index < sizeof(wifi_rx_buffer) - 1) {
                    wifi_rx_buffer[wifi_rx_index++] = ch;
                } else {
                    // 缓冲区溢出 → 直接丢弃
                    wifi_rx_index = 0;
                }
            }
        }
    }
}


void USART2_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch;

    // 清除溢出错误标志（否则 RXNE 中断不会再触发）
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART2);    // 读 DR 清除 ORE
    }

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        ch = USART_ReceiveData(USART2);
        xQueueSendFromISR(xWIFIQueue, &ch, &xHigherPriorityTaskWoken);
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//HTTP_UploadData — HTTP POST 上传函数
int HTTP_UploadData(const char *data) {
    char cmd[512];     // 放 AT 指令
    char http_req[256];// 放完整 HTTP 请求

    //空指针保护
    if (data == NULL) {
        LOG("HTTP Error: data is NULL\r\n");
        return 0;
    }

    //先关闭旧 TCP 连接
    USART2_SendString("AT+CIPCLOSE\r\n");
    LOG("HTTP: AT+CIPCLOSE\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    //建立 TCP 连接
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
             SERVER_IP, SERVER_PORT);
    USART2_SendString(cmd);
    LOG("HTTP: %s", cmd);
    vTaskDelay(pdMS_TO_TICKS(3000));

    //构造 HTTP 请求
    int reqLen = snprintf(http_req, sizeof(http_req),
                          "POST /x HTTP/1.0\r\n"
                          "\r\n"
                          "%s",
                          data);

    if (reqLen <= 0 || reqLen >= sizeof(http_req)) {
        LOG("HTTP Error: request too long!\r\n");
        USART2_SendString("AT+CIPCLOSE\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", reqLen);
    USART2_SendString(cmd);
    LOG("HTTP: %s", cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));          // 等 ESP 回复 ">"

    //发送 HTTP 请求正文
    USART2_SendString(http_req);
    LOG("HTTP: %s", http_req);
    vTaskDelay(pdMS_TO_TICKS(2500));          // 等服务器处理完

    USART2_SendString("AT+CIPCLOSE\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    return 1;   // 按流程执行完毕，默认成功
}
