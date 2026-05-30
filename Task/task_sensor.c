#include "common.h"

#define SENSOR_TEMP_ALARM_THRESHOLD_C     50.0f      // 温度报警阈值：超过 50°C 触发报警
#define SENSOR_LIGHT_ALARM_THRESHOLD_LUX  1000.0f    // 光照报警阈值：超过 1000 lux 触发报警
#define SENSOR_ALARM_BLINK_COUNT          3          // 红灯快闪次数
#define SENSOR_ALARM_BLINK_INTERVAL_MS    200        // 红灯每次亮/灭间隔 200ms
#define SENSOR_ALARM_BUZZER_MS            500        // 蜂鸣器持续 500ms

//阈值报警动作红灯快闪 3 次：每次亮 200ms、灭 200ms。蜂鸣器响 500ms。
static void Sensor_RunThresholdAlarm(void) {
    uint8_t i;

    LOG("ALARM: over threshold");

    for (i = 0; i < SENSOR_ALARM_BLINK_COUNT; i++) {
        LED_ON(LED_RED_PORT, LED_RED_PIN);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_ALARM_BLINK_INTERVAL_MS));

        LED_OFF(LED_RED_PORT, LED_RED_PIN);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_ALARM_BLINK_INTERVAL_MS));
    }

    Buzzer_Ring(GPIOB, GPIO_Pin_14);
    vTaskDelay(pdMS_TO_TICKS(SENSOR_ALARM_BUZZER_MS));
    Buzzer_Shutup(GPIOB, GPIO_Pin_14);
}

//判断当前传感器真实值是否超过阈值
static uint8_t Sensor_IsOverThreshold(const SensorData_t *sensorData) {
    if (sensorData == NULL) {
        return 0;
    }

    return (sensorData->temp_celsius > SENSOR_TEMP_ALARM_THRESHOLD_C) ||
           (sensorData->light_lux > SENSOR_LIGHT_ALARM_THRESHOLD_LUX);
}

/**
 * @brief  传感器采集+上传任务
 * @note   每10秒采一次光敏+温度 → 换算真实值 → 阈值报警 → HTTP上传
 *         上传时亮黄灯，失败亮红灯+蜂鸣器
 */
void vTaskSensor(void *pvParameters) {
    LOG("Sensor Task Started!");

    // 等系统稳定（WiFi连上后再开始），避免刚上电 WiFi 还没连好就上传失败
    vTaskDelay(pdMS_TO_TICKS(15000));               // 等15秒让WiFi连好

    while (1) {
        SensorData_t sensorData;
        char data[128];

        // 采集数据：黄灯亮表示正在读 ADC，读完马上熄灭
        LED_ON(LED_YELLOW_PORT, LED_YELLOW_PIN);
        sensorData = Sensor_ReadAll();
        LED_OFF(LED_YELLOW_PORT, LED_YELLOW_PIN);

        LOG("Sensor: light=%.2fV %.2flux, temp=%.2fV %.2fC",
            sensorData.light_voltage,
            sensorData.light_lux,
            sensorData.temp_voltage,
            sensorData.temp_celsius);

        // 采集后立即判断阈值，超过阈值就本地报警，并在日志中打出 ALARM 标记。
        if (Sensor_IsOverThreshold(&sensorData)) {
            Sensor_RunThresholdAlarm();
        }

        snprintf(data, sizeof(data),
                 "device=%s&light_lux=%.2f&temp_celsius=%.2f",
                 DEVICE_ID,
                 sensorData.light_lux,
                 sensorData.temp_celsius);

        // HTTP上传：黄灯亮表示正在通过 ESP-01S 发送数据
        LED_ON(LED_YELLOW_PORT, LED_YELLOW_PIN);

        if (HTTP_UploadData(data)) {
            LOG("Sensor: upload OK, %s", data);
        } else {
            // 上传失败：红灯 + 蜂鸣器报警，提示用户网络或服务器可能异常
            LOG("Sensor: upload FAILED!");
                LED_ON(LED_RED_PORT, LED_RED_PIN);
                Buzzer_Ring(GPIOB, GPIO_Pin_14);
                vTaskDelay(pdMS_TO_TICKS(500));
                Buzzer_Shutup(GPIOB, GPIO_Pin_14);
                LED_OFF(LED_RED_PORT, LED_RED_PIN);
        }

        LED_OFF(LED_YELLOW_PORT, LED_YELLOW_PIN);   // 黄灯灭=上传完成

        // 等10秒再采下一组；vTaskDelay 会让出 CPU，不影响其它 FreeRTOS 任务运行
        vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_INTERVAL_MS));
    }
}
