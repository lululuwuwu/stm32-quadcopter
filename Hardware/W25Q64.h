#ifndef __W25Q64_H
#define __W25Q64_H

void W25Q64_Init(void);
void W25Q64_MID_DID(uint8_t *MID,uint16_t *DID);

void W25Q64_Read_Data(uint32_t address,uint8_t *array,uint8_t length);

void W25Q64_Page_Program(uint32_t address,uint8_t *array,uint8_t length);

void W25Q64_Sector_Erase(uint32_t address);

#endif
