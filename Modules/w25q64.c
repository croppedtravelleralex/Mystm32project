#include "w25q64.h"

/* W25Q64 常用指令 */
#define W25Q64_WRITE_ENABLE    0x06
#define W25Q64_READ_STATUS     0x05
#define W25Q64_READ_DATA       0x03
#define W25Q64_PAGE_PROGRAM    0x02
#define W25Q64_SECTOR_ERASE    0x20
#define W25Q64_READ_ID         0x9F

// W25Q64 状态寄存器 BUSY 位：1 表示芯片正在擦除/写入，0 表示空闲
#define W25Q64_STATUS_BUSY     0x01

// SPI 读数据时需要发送一个无意义字节，用来产生 SCK 时钟
#define W25Q64_DUMMY_BYTE      0xFF

// PA4 作为片选引脚：低电平选中 W25Q64，高电平释放 W25Q64
#define W25Q64_CS_LOW()        GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define W25Q64_CS_HIGH()       GPIO_SetBits(GPIOA, GPIO_Pin_4)

static uint8_t SPI1_SendByte(uint8_t byte);
static void W25Q64_WriteEnable(void);
static void W25Q64_WaitBusy(void);

static uint8_t SPI1_SendByte(uint8_t byte)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        // 等待发送缓冲区为空，空了才能写入下一个字节
    }

    SPI_I2S_SendData(SPI1, byte);

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        // 等待接收缓冲区非空，非空说明本次传输已经完成
    }

    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

//初始化 W25Q64 
void W25Q64_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    // 打开 GPIOA 和 SPI1 的外设时钟。外设时钟不打开，后续配置不会生效
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    // PA4 配置为普通推挽输出，用来手动控制 CS 片选。
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 空闲时 CS 拉高，表示不选中 W25Q64。
    W25Q64_CS_HIGH();

    // PA5(SCK)、PA7(MOSI) 配置为复用推挽输出，由 SPI1 外设接管输出。
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA6(MISO) 配置为浮空输入，用来接收 W25Q64 返回的数据。/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置 SPI1。PCLK2 通常为 72MHz，4 分频后 SPI 时钟为 18MHz。
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);
}

//读取 W25Q64 的 JEDEC ID 前 2 个字节
void W25Q64_ReadID(uint8_t *id)
{
    W25Q64_CS_LOW();

    SPI1_SendByte(W25Q64_READ_ID);
    id[0] = SPI1_SendByte(W25Q64_DUMMY_BYTE);
    id[1] = SPI1_SendByte(W25Q64_DUMMY_BYTE);

    W25Q64_CS_HIGH();
}

//从 W25Q64 读取任意长度数据
void W25Q64_Read(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint16_t i;

    W25Q64_CS_LOW();

    SPI1_SendByte(W25Q64_READ_DATA);
    SPI1_SendByte((uint8_t)(addr >> 16));
    SPI1_SendByte((uint8_t)(addr >> 8));
    SPI1_SendByte((uint8_t)addr);

    for (i = 0; i < len; i++) {
        data[i] = SPI1_SendByte(W25Q64_DUMMY_BYTE);
    }

    W25Q64_CS_HIGH();
}

//向 W25Q64 写入一页内的数据
void W25Q64_Write(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint16_t i;

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();

    SPI1_SendByte(W25Q64_PAGE_PROGRAM);
    SPI1_SendByte((uint8_t)(addr >> 16));
    SPI1_SendByte((uint8_t)(addr >> 8));
    SPI1_SendByte((uint8_t)addr);

    for (i = 0; i < len; i++) {
        SPI1_SendByte(data[i]);
    }

    W25Q64_CS_HIGH();

    W25Q64_WaitBusy();
}

//擦除 W25Q64 的 4KB 扇区
void W25Q64_EraseSector(uint32_t addr)
{
    W25Q64_WriteEnable();

    W25Q64_CS_LOW();

    SPI1_SendByte(W25Q64_SECTOR_ERASE);
    SPI1_SendByte((uint8_t)(addr >> 16));
    SPI1_SendByte((uint8_t)(addr >> 8));
    SPI1_SendByte((uint8_t)addr);

    W25Q64_CS_HIGH();

    W25Q64_WaitBusy();
}

//发送写使能指令
static void W25Q64_WriteEnable(void)
{
    W25Q64_CS_LOW();
    SPI1_SendByte(W25Q64_WRITE_ENABLE);
    W25Q64_CS_HIGH();
}

//等待 W25Q64 忙状态结束
static void W25Q64_WaitBusy(void)
{
    uint8_t status;

    W25Q64_CS_LOW();
    SPI1_SendByte(W25Q64_READ_STATUS);

    do {
        status = SPI1_SendByte(W25Q64_DUMMY_BYTE);
    } while ((status & W25Q64_STATUS_BUSY) != 0);

    W25Q64_CS_HIGH();
}
