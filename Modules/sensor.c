#include "common.h"
#include <math.h>

//初始化传感器（实际就是初始化 ADC1）
void Sensor_Init(void) {
    ADC1_Init();
}

//读光敏传感器 ADC 原始值（0~4095，越大=光照越弱）LDR 特性：暗 → 高阻 → 分压高 → ADC 值大
uint16_t Sensor_ReadLightRaw(void) {
    return ADC1_ReadChannel(ADC_CH_LIGHT);    // PA0
}

//读温度传感器 ADC 原始值（0~4095）NTC 特性：温度高 → 低阻 → 分压低 → ADC 值小
uint16_t Sensor_ReadTempRaw(void) {
    return ADC1_ReadChannel(ADC_CH_TEMP);     // PA1
}

//ADC 值转电压 公式：电压 = (ADC值 / 4095) × 3.3V

static float ADC2Voltage(uint16_t adc) {
    return (float)adc / 4095.0f * VREF;
}

//读光敏电压(V) 电压越高 = 光照越弱（LDR 暗阻大分摊电压高）
float Sensor_ReadLightVoltage(void) {
    return ADC2Voltage(Sensor_ReadLightRaw());
}

//读温度电压(V) 电压越高 = 温度越低（NTC 低温阻大分摊电压高）
float Sensor_ReadTempVoltage(void) {
    return ADC2Voltage(Sensor_ReadTempRaw());
}


//光敏电压转光照强度(lux)
float Sensor_ConvertLightLux(float voltage) {
    float ldr_resistance;

    //避免除以 0，这里返回 0.0f 作为保护值，防止程序产生无穷大或崩溃。
    if (voltage <= 0.0f) {
        return 0.0f;
    }

    ldr_resistance = LDR_R_DIVIDER * (ADC_REF_VOLTAGE / voltage - 1.0f);

    //阻值必须大于 0 才能代入 powf()。如果出现小于等于 0，通常表示 ADC 电压异常接近或超过参考电压。
    if (ldr_resistance <= 0.0f) {
        return 0.0f;
    }

    return 10.0f * powf(LDR_R_10LUX / ldr_resistance, 1.5f);
}

//读光照强度(lux)先读取光敏电压，再调用换算函数得到估算 lux。
float Sensor_ReadLightLux(void) {
    return Sensor_ConvertLightLux(Sensor_ReadLightVoltage());
}

//NTC 电压转温度(°C)
float Sensor_ConvertTempCelsius(float voltage) {
    float ntc_resistance;
    float resistance_ratio;
    float reciprocal_kelvin;
    float temperature_kelvin;

    //避免除以 0：
    if (voltage >= ADC_REF_VOLTAGE) {
        return 0.0f;
    }

    ntc_resistance = NTC_R_DIVIDER * (voltage / (ADC_REF_VOLTAGE - voltage));


    if (ntc_resistance <= 0.0f) {
        return 0.0f;
    }

    resistance_ratio = ntc_resistance / NTC_R_25;
    reciprocal_kelvin = (1.0f / 298.15f) + (1.0f / NTC_B_VALUE) * logf(resistance_ratio);


    if (reciprocal_kelvin <= 0.0f) {
        return 0.0f;
    }

    temperature_kelvin = 1.0f / reciprocal_kelvin;
    return temperature_kelvin - 273.15f;
}

//读温度(°C)先读取 NTC 电压，再调用换算函数得到摄氏温度。
float Sensor_ReadTempCelsius(void) {
    return Sensor_ConvertTempCelsius(Sensor_ReadTempVoltage());
}

//一次性读取所有传感器数据

void Sensor_ReadAll(SensorData_t *data) {
    if (data == NULL) {
        return;
    }

    data->light_voltage = Sensor_ReadLightVoltage();
    data->temp_voltage = Sensor_ReadTempVoltage();
    data->light_lux = Sensor_ConvertLightLux(data->light_voltage);
    data->temp_celsius = Sensor_ConvertTempCelsius(data->temp_voltage);
}


//返回一份完整的传感器数据结构。
SensorData_t Sensor_ReadData(void) {
    SensorData_t data;

    Sensor_ReadAll(&data);
    return data;
}
