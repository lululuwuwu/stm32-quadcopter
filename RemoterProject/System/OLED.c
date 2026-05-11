#include "board.h"
#include "OLED_Font.h"

//Data or Command
void OLED_DC_W(uint8_t bitValue)
{
	GPIO_WriteBit(OLED_GPIO_PORT,OLED_DC,(BitAction)bitValue);
}

//SCLK
void OLED_SCLK_W(uint8_t bitValue)
{
	GPIO_WriteBit(OLED_GPIO_PORT,OLED_SCLK,(BitAction)bitValue);
}


//RES
void OLED_RES_W(uint8_t bitValue)
{
	GPIO_WriteBit(OLED_GPIO_PORT,OLED_RES,(BitAction)bitValue);
}

//MOSI
void OLED_MOSI_W(uint8_t bitValue)
{
	GPIO_WriteBit(OLED_GPIO_PORT,OLED_MOSI,(BitAction)bitValue);
}


/*引脚初始化*/
void OLED_SPI_Init(void)
{
	//1.开启GPIO时钟
    RCC_APB2PeriphClockCmd(OLED_RCC, ENABLE);
	//2.GPIO初始化
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = OLED_DC | OLED_SCLK |OLED_RES|OLED_MOSI;
 	GPIO_Init(OLED_GPIO_PORT, &GPIO_InitStructure);

	
	//3.硬件默认状态（高电平）
	OLED_SCLK_W(1);//时钟线默认高电平
	/*
	This pin is reset signal input. When the pin is pulled LOW, initialization of the chip is 
	executed. Keep this pin HIGH (i.e. connect to VDD) during normal operation. 
	*/
	OLED_RES_W(1);//正常模式 
}



/**
  * @brief  SPI发送一个字节
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_SPI_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_SCLK_W(0);
		
		OLED_MOSI_W(Byte & (0x80 >> i));
		
		OLED_SCLK_W(1);	
	}

}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	
	OLED_DC_W(OLED_CMD);
	OLED_SPI_SendByte(Command);
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_DC_W(OLED_DATA);
	OLED_SPI_SendByte(Data);	

}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7   page
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127 column
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置 --> Set Page Start Address
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位 --> 列的高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位 --> 列的低4位
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++) // j就是page
	{
		OLED_SetCursor(j, 0); //设置光标位置 
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00); // 0就是不显示 ，连续写入数据，column地址会自增
		}
	}
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;	

	
	OLED_SPI_Init();			//端口初始化
	
	OLED_RES_W(0);//初始化芯片
	
	for (i = 0; i < 1000; i++)			//延时一下
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_RES_W(1);//普通模式
	
	for (i = 0; i < 1000; i++);
	
	
	OLED_WriteCommand(0xAE);	//关闭显示
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//设置显示开始行
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/倒转显示

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏
}

//先显示单个中文

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~8
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_Show_Chinese(uint8_t Line, uint8_t Column, char *CH)
{   
	//1.先把传入的中文存起来
	char chinese_temp[3];
	chinese_temp[0] = CH[0];
	chinese_temp[1] = CH[1];
	chinese_temp[2] ='\0';	

	
	//2.遍历字模数组
	for(uint16_t j=0;j<sizeof(OLED_CH16x16)/sizeof(OLED_CH_Typedef);j++)
	{
		if(strcmp(chinese_temp,OLED_CH16x16[j].ch)==0)
		{
			uint8_t i;
			OLED_SetCursor((Line - 1) * 2, (Column - 1) * 16);		//设置光标位置在上半部分
			for (i = 0; i < 16; i++)
			{
				OLED_WriteData(OLED_CH16x16[j].data[i]);			//显示上半部分内容
			}
			OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 16);	//设置光标位置在下半部分
			for (i = 0; i < 16; i++)
			{
				OLED_WriteData(OLED_CH16x16[j].data[i + 16]);		//显示下半部分内容
			}
		}
	}
	
	
   	
	
}



//显示多个中文
void OLED_Show_Multiple_Chinese(uint8_t Line, uint8_t Column, char *CH)
{
	for(uint16_t i=0;i<(strlen(CH)/2);i++)
	{
		OLED_Show_Chinese(Line,Column+i,CH+2*i); //每次显示一个中文
	}
}



//显示浮点数--不带符号
//decimal_point: 小数点位数
void OLED_Show_Float(uint8_t Line, uint8_t Column,double f,uint8_t decimal_point,uint8_t length)
{
	char f_str[20];
	snprintf(f_str,20,"%0*.*lf",length,decimal_point,f);
	OLED_ShowString(Line,Column,f_str);
	
}



//显示浮点数--带符号

void OLED_Show_SignedFloat(uint8_t Line, uint8_t Column,double f,uint8_t decimal_point,uint8_t length)
{
	char f_str[20];
	snprintf(f_str,20,"%+0*.*lf",length+1,decimal_point,f);
	OLED_ShowString(Line,Column,f_str);
	
}


