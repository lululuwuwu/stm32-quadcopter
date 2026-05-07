#include "stm32f10x.h"                  // Device header
#include "Delay.h"

//硬件： SCL - PB12 (开漏输出)， SDA-PB13 (开漏输出)

//操作SCL
void MyI2C_SCL_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_12,(BitAction)bitValue);
	//操作的时候，时钟频率小于100KHz
	Delay_us(5);
}


//操作SDA
void MyI2C_SDA_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_13,(BitAction)bitValue);
}

//读取SDA
uint8_t MyI2C_SDA_R(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13); //开漏模式下，输入是打开的。
}

void MyI2C_Init(void)
{
	//开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	//初始化引脚
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_OD; //开漏输出
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12 |GPIO_Pin_13 ;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);
	
	//指定默认值
	MyI2C_SCL_W(1);//高
	MyI2C_SDA_W(1);//高
}


//开始时序
void MyI2C_Start(void)
{
	//1.保证SDA，SCL是高电平
	MyI2C_SDA_W(1);//高
	MyI2C_SCL_W(1);//高
	
	//2.SCL高电平期间，SDA产生一个下降沿
	MyI2C_SDA_W(0);//低
	//3.拉低SCL--拼接下一个时序
	MyI2C_SCL_W(0);
	
}

//停止条件
void MyI2C_Stop(void)
{
	//1.确保SDA是低电平
	MyI2C_SDA_W(0);//低
	//2.确保SCL是高电平，然后拉高SDA 产生结束条件
	MyI2C_SCL_W(1);
	MyI2C_SDA_W(1);	
}

//发送一个字节
void MyI2C_SendByte(uint8_t byteValue)
{
	//1.SCL 在上一个时序已经是低电平了。
	
	for(uint8_t i=0;i<8;i++)
	{
		//2.SDA 放入字节的高位
		MyI2C_SDA_W(byteValue&(0x80>>i)); // byteValue&0x80 结果如果是0 SDA就写入0， 其他值则写入1 --- BitAction强转为枚举类型
		//3.SCL 拉高 ---> MPU6050 会自动读取
		MyI2C_SCL_W(1);
		//4.SCL 拉低--->放入下一个bit
		MyI2C_SCL_W(0);
	}
}

//接收一个字节
uint8_t MyI2C_RecByte(void)
{
	uint8_t byte_temp=0x00;
	//1.上个时序的结尾SCL已经是低电平了，MPU6050会自动放入高bit位
	//2. 主机STM32必须放手SDA
	MyI2C_SDA_W(1);
	
	
	for(uint8_t i=0;i<8;i++)
	{
		//3. 拉高时钟线
		MyI2C_SCL_W(1);
		//4. STM32读取高位bit
		if(MyI2C_SDA_R()==1)
		{
			byte_temp|=0x80>>i;
		}
		//5.SCL拉低-->MPU6050会自动放入下一个bit
		MyI2C_SCL_W(0);
	}
	
	return byte_temp;

}

//发送应答: 0 就是有应答，1就是没有应答
void MyI2C_SendACK(uint8_t ackValue)
{
	//1.上个时序的结尾SCL已经是低电平了
	//2.STM32写入应答位
	MyI2C_SDA_W(ackValue);
	//3.拉高SCL，MPU6050会自动读取应答
	MyI2C_SCL_W(1);
	//4.拉低SCL，拼接后续时序
	MyI2C_SCL_W(0);	
}

//接收应答
uint8_t MyI2C_RecACK(void)
{
	//1.上个时序的结尾SCL已经是低电平了 -- MPU6050会自动放入应答位
	//2. 主机必须放手
	MyI2C_SDA_W(1);
	//3. SCL拉高--STM32读取应答位
	MyI2C_SCL_W(1);
	uint8_t ack = MyI2C_SDA_R();
	//4.拉低SCL，拼接后续时序
	MyI2C_SCL_W(0);	
	
	return ack;
}




