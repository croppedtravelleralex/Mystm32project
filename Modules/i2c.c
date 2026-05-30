#include "i2c.h"

/*
 * ===== I2C1 驱动实现 =====
 *
 * 使用 STM32 标准外设库的 I2C 外设，不模拟 GPIO。
 * I2C1 挂在 APB1 上（36MHz），配置为 100kHz 标准模式。
 *
 * 关键寄存器/事件说明：
 *   I2C_SR1 (状态寄存器1) 中的标志：
 *     SB       — 起始条件已发送
 *     ADDR     — 地址已发送 + 收到应答
 *     TXE      — 数据寄存器空（可以写新数据）
 *     BTF      — 字节发送完成
 *     RXNE     — 数据寄存器非空（收到新数据）
 *     AF       — 应答失败（NACK）
 *
 *   I2C_SR2 必须在 ADDR 事件后读取（清除 ADDR 标志）
 */

/*
 * 等待事件函数：等待 I2C 标志位置位，超时返回 1
 * event 可以是多个标志的"或"，如 I2C_FLAG_SB | I2C_FLAG_AF
 */
static uint8_t I2C_WaitEvent(uint32_t event, uint32_t timeout) {
    while (timeout--) {
        if (I2C_CheckEvent(I2C1, event)) {
            return 0;  // 事件发生，成功
        }
        /* 检查应答失败标志 */
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF)) {
            return 1;  // NACK，直接返回错误
        }
    }
    return 1;  // 超时
}

/*
 * 最后一个标志检查：在发送停止位前，等待 BTF（字节发送完成）或 TXE
 */
static uint8_t I2C_WaitBTF(uint32_t timeout) {
    while (timeout--) {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF) ||
            I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
            return 0;
        }
    }
    return 1;
}

/*
 * 如果 AF 标志置位，需要软件清除后再发停止位
 */
static void I2C_ClearAF(void) {
    I2C_ClearFlag(I2C1, I2C_FLAG_AF);
}

/*
 * ===== I2C1 初始化 =====
 *
 * 配置步骤：
 *   1. 使能 I2C1 和 GPIOB 时钟
 *   2. PB6(SCL)/PB7(SDA) 配置为复用开漏输出
 *   3. 复位 I2C1，配置时序（36MHz APB1 → 100kHz）
 *   4. 使能 I2C1
 */
void I2C1_Init(void) {
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    /* 1. 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // GPIOB 时钟（APB2）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);    // I2C1 时钟（APB1）

    /* 2. 配置 PB6(SCL)/PB7(SDA) 复用开漏输出 */
    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;     // 复用开漏输出：I2C 协议要求 SCL/SDA 为开漏
    gpio.GPIO_Speed = GPIO_Speed_10MHz;    // 10MHz 对于 100kHz I2C 足够
    GPIO_Init(GPIOB, &gpio);

    /* 3. 复位 I2C1（避免上电后脏状态） */
    I2C_DeInit(I2C1);

    /* 4. 配置 I2C1 参数 */
    i2c.I2C_Mode              = I2C_Mode_I2C;          // I2C 模式（非 SMBus）
    i2c.I2C_DutyCycle         = I2C_DutyCycle_2;       // 标准模式占空比 2:1
    i2c.I2C_OwnAddress1       = 0x00;                  // 本机地址（主模式不需要）
    i2c.I2C_Ack               = I2C_Ack_Enable;        // 使能应答
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;  // 7 位地址
    i2c.I2C_ClockSpeed        = 100000;                // 100kHz 标准模式

    I2C_Init(I2C1, &i2c);

    /* 5. 使能 I2C1 */
    I2C_Cmd(I2C1, ENABLE);
}

/*
 * ===== I2C1 写数据 =====
 *
 * 时序：
 *   Master: [START] [SLAVE_ADDR+W] [DATA0] [DATA1] ... [DATAn] [STOP]
 *   Slave:  [       ACK         ] [ ACK ] [ ACK ] ... [ ACK ]
 *
 * 注意：STM32F103 I2C 的 EV6 处理比较特殊——
 *       地址发送完成（ADDR 置位）后，必须先读 SR2 才能清除 ADDR 标志。
 *       所以 I2C_CheckEvent() 内部会读 SR2，不能重复调用。
 *       因此我们先等 SR1 的 ADDR 位置位，然后直接读 SR2。
 */
uint8_t I2C1_Write(uint8_t slave_addr, const uint8_t *buf, uint16_t len) {
    uint16_t timeout;
    uint8_t addr_7bit = slave_addr;  // 传入的是 7 位地址，左移由库函数处理

    /* --- 1. 发送起始条件 --- */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
        if (--timeout == 0) {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }
    }

    /* --- 2. 发送从机地址(写) --- */
    I2C_Send7bitAddress(I2C1, addr_7bit, I2C_Direction_Transmitter);

    /* 等待地址发送完成（ADDR 标志置位） */
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF)) {  // NACK
            I2C_ClearAF();
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }
        if (--timeout == 0) {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }
    }
    /* 读 SR2 清除 ADDR 标志（硬件要求） */
    (void)I2C_GetFlagStatus(I2C1, I2C_FLAG_AF);  // 等价于读 SR2

    /* --- 3. 逐个发送数据字节 --- */
    while (len--) {
        /* 等待 TXE（数据寄存器空） */
        timeout = I2C_TIMEOUT_CYCLES;
        while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
            if (--timeout == 0) {
                I2C_GenerateSTOP(I2C1, ENABLE);
                return 1;
            }
        }
        /* 发送一个字节 */
        I2C_SendData(I2C1, *buf++);
    }

    /* --- 4. 等待最后字节发送完成 --- */
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF)) {
        if (--timeout == 0) {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }
    }

    /* --- 5. 发送停止条件 --- */
    I2C_GenerateSTOP(I2C1, ENABLE);

    return 0;  // OK
}

/*
 * ===== I2C1 读数据 =====
 *
 * 时序：
 *   Phase1(写寄存器地址):
 *     Master: [START] [SLAVE_ADDR+W] [REG_ADDR] [RESTART]
 *     Slave:  [       ACK         ] [  ACK   ]
 *
 *   Phase2(读数据):
 *     Master: [RESTART] [SLAVE_ADDR+R] [DATA0] [DATA1] ... [DATAn][NACK][STOP]
 *     Slave:  [          ACK         ] [  OK  ] [  OK  ] ... [ OK ]
 *
 * 注意：读最后一个字节前要关 ACK，让从机知道不用再发了。
 *       最后一个字节收到后发 NACK+STOP。
 */
uint8_t I2C1_Read(uint8_t slave_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len) {
    uint16_t timeout;

    /* ===== Phase 1：写寄存器地址 ===== */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
        if (--timeout == 0) return 1;
    }

    I2C_Send7bitAddress(I2C1, slave_addr, I2C_Direction_Transmitter);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF)) {
            I2C_ClearAF();
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 1;
        }
        if (--timeout == 0) return 1;
    }
    (void)I2C_GetFlagStatus(I2C1, I2C_FLAG_AF);  // clear ADDR via SR2 read

    /* 发寄存器地址 */
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
        if (--timeout == 0) return 1;
    }
    I2C_SendData(I2C1, reg_addr);

    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF)) {
        if (--timeout == 0) return 1;
    }

    /* ===== Phase 2：重新起始 → 读数据 ===== */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
        if (--timeout == 0) return 1;
    }

    I2C_Send7bitAddress(I2C1, slave_addr, I2C_Direction_Receiver);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
        if (--timeout == 0) return 1;
    }

    /*
     * 如果只读 1 个字节：
     *   在清除 ADDR 前就要关 ACK，这样地址应答后立即 NACK
     * 如果读 >1 个字节：
     *   保持 ACK，读到最后一个字节前再关
     */
    if (len <= 1) {
        I2C_AcknowledgeConfig(I2C1, DISABLE);   // 关 ACK
    }
    (void)I2C_GetFlagStatus(I2C1, I2C_FLAG_AF);  // clear ADDR

    /* 逐个收数据 */
    while (len--) {
        timeout = I2C_TIMEOUT_CYCLES;
        while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE)) {
            if (--timeout == 0) return 1;
        }

        if (len == 1) {
            /* 倒数第二个字节收到后关 ACK，让从机准备停止 */
            I2C_AcknowledgeConfig(I2C1, DISABLE);
        }

        *buf++ = I2C_ReceiveData(I2C1);
    }

    /* 发停止位 */
    I2C_GenerateSTOP(I2C1, ENABLE);

    /* 恢复 ACK（为下次传输做准备） */
    I2C_AcknowledgeConfig(I2C1, ENABLE);

    return 0;
}
