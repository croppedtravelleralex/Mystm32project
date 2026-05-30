#ifndef __I2C_H__
#define __I2C_H__

#include "stm32f10x.h"

/*
 * ===== I2C1 驱动接口 =====
 *
 * 硬件：STM32F103C8T6 的 I2C1 外设
 * 引脚：PB6 = SCL, PB7 = SDA（复用开漏输出）
 * 速率：100kHz 标准模式
 * 用途：驱动 SSD1306 OLED 显示屏
 *
 * 注意：STM32F103 的 I2C 硬件存在一些已知 bug（尤其是 EV6/EV8 事件标志处理），
 *       本驱动中使用限时等待 + 超时退出的方式避免死锁。
 */

/*
 * ===== I2C 超时宏 =====
 *
 * I2C 外设读写操作的最大等待时间。
 * 标准模式下传输 1 字节约 80μs（10 个 SCL 周期），
 * 这里给 10000 循环 ≈ 足够传几十字节仍有余量。
 * 超时后返回错误，不卡死主循环。
 */
#define I2C_TIMEOUT_CYCLES     10000

/*
 * ===== I2C 传输方向常量 =====
 * 在 I2C 协议中，7 位从机地址左移 1 位后，
 * 最低位 = 0 表示写，= 1 表示读。
 */
#define I2C_DIR_WRITE          0x00
#define I2C_DIR_READ           0x01

/*
 * ===== 函数原型 =====
 */

/**
 * @brief  初始化 I2C1 外设
 * @note   配置 PB6(SCL)/PB7(SDA) 为复用开漏输出
 *         I2C1 时钟来自 APB1（36MHz），设置 CCR 使 SCL=100kHz
 *         使能 I2C1 外设
 */
void I2C1_Init(void);

/**
 * @brief  I2C1 发送多字节数据
 * @param  slave_addr  7 位从机地址（如 SSD1306 的 7 位地址 0x3C）
 * @param  buf         数据缓冲区指针
 * @param  len         发送字节数
 * @retval 0=成功, 1=超时/错误
 *
 * @note   完整流程：起始位 → 发从机地址(写) → 等应答 →
 *         逐个发数据 → 等应答 → 停止位
 *         如果 len=0，只发地址+停止（用于检测设备是否存在）
 */
uint8_t I2C1_Write(uint8_t slave_addr, const uint8_t *buf, uint16_t len);

/**
 * @brief  I2C1 先写寄存器地址再读数据（复合事务）
 * @param  slave_addr  7 位从机地址
 * @param  reg_addr    要读取的寄存器地址（写阶段发送）
 * @param  buf         读数据缓冲区指针
 * @param  len         期望读取的字节数
 * @retval 0=成功, 1=超时/错误
 *
 * @note   完整流程：
 *         起始位 → 发地址(写) → 发寄存器地址 → 停止（重启）
 *         → 起始位 → 发地址(读) → 逐个收数据 → 发 NACK → 停止
 */
uint8_t I2C1_Read(uint8_t slave_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len);

#endif  //__I2C_H__
