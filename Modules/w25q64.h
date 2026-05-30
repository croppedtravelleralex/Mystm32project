#ifndef __W25Q64_H__
#define __W25Q64_H__

#include "stm32f10x.h"

void W25Q64_Init(void);
void W25Q64_ReadID(uint8_t *id);
void W25Q64_Read(uint32_t addr, uint8_t *data, uint16_t len);
void W25Q64_Write(uint32_t addr, uint8_t *data, uint16_t len);
void W25Q64_EraseSector(uint32_t addr);

#endif  /* __W25Q64_H__ */
