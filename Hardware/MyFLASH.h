#ifndef __MYFLASH_H
#define __MYFLASH_H

void MyFLASH_Init(void);

uint8_t MyFLASH_ReadByte(uint32_t address);

uint16_t MyFLASH_ReadHalfWord(uint32_t address);

uint32_t MyFLASH_ReadWord(uint32_t address);


FLASH_Status MyFLASH_PageProgram(uint32_t Address, uint16_t Data);


FLASH_Status MyFLASH_ErasePage(uint32_t Page_Address);


FLASH_Status MyFLASH_ByteArrayProgram(uint32_t Address,uint8_t *arrary,uint16_t length);

void MyFLASH_ReadByteArray(uint32_t address,uint8_t *arrary,uint16_t length);
#endif
