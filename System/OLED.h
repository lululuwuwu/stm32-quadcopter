#ifndef __OLED_H
#define __OLED_H

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

void OLED_Show_Chinese(uint8_t Line, uint8_t Column, char *CH);
void OLED_Show_Multiple_Chinese(uint8_t Line, uint8_t Column, char *CH);

void OLED_Show_Float(uint8_t Line, uint8_t Column,double f,uint8_t decimal_point,uint8_t length);
void OLED_Show_SignedFloat(uint8_t Line, uint8_t Column,double f,uint8_t decimal_point,uint8_t length);
#endif
