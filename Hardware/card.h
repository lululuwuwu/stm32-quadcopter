#ifndef __CARD_H
#define __CARD_H


//初始化方法
void Card_Init(void);

//读卡号: 返回1就是读取成功，返回0就是读取失败
uint8_t Card_ReadID(char *cardid);


uint8_t CardMoney_Init(uint8_t blockID,uint32_t moneyInit,char *resultBanlanceMsg);

#endif
