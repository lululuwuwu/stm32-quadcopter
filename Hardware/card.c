#include "stm32f10x.h"                  // Device header
#include "Serial1.h"
#include "Delay.h"

uint8_t ReadID_Command[]={0x01,0x08,0xA1,0x20,0x00,0x00,0x00,0x00}; 
uint8_t MoneyInit_Command[]={0x01,0x0B,0xA6,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

//初始化方法
void Card_Init(void)
{
	Serial1_Init();
}	


//校验和设置
void Card_SetCheckCode(uint8_t *command)
{
	char checksum; //校验和
	char i;
	checksum = 0;
	for(i = 0;i < (command[1] - 1); i++)
	{
	checksum ^= command[i]; //异或
	}
	command[command[1] - 1] = ~checksum ; //按位取反

}

//检验校验和是否正确： 返回1就是正确的，返回0就是错误的。
uint8_t Card_CheckCodeSum(uint8_t *command)
{
	
	char checksum; //校验和
	char i;
	checksum = 0;
	for(i = 0;i < (command[1] - 1); i++)
	{
	checksum ^= command[i]; //异或
	}
	
	if(command[command[1] - 1] == (uint8_t)(~checksum))
	{
		return 1;
	}
	return 0;
}


//读卡号: 返回1就是读取成功，返回0就是读取失败
uint8_t Card_ReadID(char *cardid)
{
	//先填写校验和
	Card_SetCheckCode(ReadID_Command);
	
	//发送指令
	Serial1_SendArray(ReadID_Command,ReadID_Command[1]);
	
	
	//接收指令
	Delay_ms(200); //等待一下
	
	if(Serial1_GetComplateFlagStatus()==1) //如果完整的接收到数据
	{
		uint8_t *result = Serial1_GetRxArray(); //获取接收到的数据
		
		if(result[4]==0x00 && Card_CheckCodeSum(result)==1) //数据成功 且 校验和正确
		{
			snprintf(cardid,20,"%02X%02X%02X%02X%02X%02X",result[5],result[6],result[7],result[8],result[9],result[10]);
		}else
		{
			return 0;
		}
		
		
		return 1;
	
	}else{
	
		Serial1_ClearRx();//清除
	}
	
	return 0;
	
}

//初始化钱包功能: 返回1就是成功，返回0就是失败
//resultBanlanceMsg: 初始化后余额值
uint8_t CardMoney_Init(uint8_t blockID,uint32_t moneyInit,char *resultBanlanceMsg)
{
	//放置参数块号
	MoneyInit_Command[4] = blockID;
	//放置初始化的钱，低位字节先行
	MoneyInit_Command[6] =  moneyInit&0x000000FF;
	MoneyInit_Command[7] = (moneyInit&0x0000FF00)>>8;
	MoneyInit_Command[8] = (moneyInit&0x00FF0000)>>16;
	MoneyInit_Command[9] = (moneyInit&0xFF000000)>>24;
	//放置校验和
	Card_SetCheckCode(MoneyInit_Command);
	//发送指令
	Serial1_SendArray(MoneyInit_Command,MoneyInit_Command[1]);		
	//接收指令
	Delay_ms(200); //等待一下
	
	if(Serial1_GetComplateFlagStatus()==1) //如果完整的接收到数据
	{
		uint8_t *result = Serial1_GetRxArray(); //获取接收到的数据
		
		if(result[4]==0x00 && Card_CheckCodeSum(result)==1) //数据成功 且 校验和正确			
		{
			uint32_t result_money = 0x00000000;
			result_money+=result[5];
			result_money+=((uint32_t)result[6])<<8;
			result_money+=((uint32_t)result[7])<<16;
			result_money+=((uint32_t)result[8])<<24;
			
			snprintf(resultBanlanceMsg,20,"%d",result_money);			
			
		}else
		{
			return 0;
		}
		
		
		return 1;
	
	}else{
	
		Serial1_ClearRx();//清除
	}
	
	return 0;
	
}


