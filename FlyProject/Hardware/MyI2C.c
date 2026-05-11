/*
 * 文件名: MyI2C.c
 * 功能说明:
 * 1. MyI2C_Delay       软件 I2C 时序延时，限制总线速率。
 * 2. MyI2C_SCL_W       控制 SCL 电平。
 * 3. MyI2C_SDA_W       控制 SDA 电平。
 * 4. MyI2C_SDA_R       读取 SDA 电平。
 * 5. MyI2C_BusRecover  初始化时尝试释放被从机拉低的 SDA。
 * 6. MyI2C_Init        初始化 PB6/PB7 为开漏输出并释放总线。
 * 7. MyI2C_Start       产生起始条件。
 * 8. MyI2C_Stop        产生停止条件。
 * 9. MyI2C_SendByte    发送 1 字节数据，高位先发。
 * 10. MyI2C_RecByte    接收 1 字节数据，高位先收。
 * 11. MyI2C_SendACK    主机发送 ACK/NACK。
 * 12. MyI2C_RecACK     主机接收从机 ACK/NACK。
 *
 * 硬件说明:
 * - 当前软件 I2C 专供 MPU6050 使用，SCL=PB6，SDA=PB7。
 * - GPIO 使用开漏输出，写 1 表示释放总线，依赖外部上拉形成高电平。
 */
#include "board.h"

static void MyI2C_Delay(void)
{
    volatile uint16_t i;

    // 粗略延时，保证软件 I2C 速率低于 100KHz；上板后可按示波器波形微调。
    for (i = 0; i < 60; i++)
    {
        __NOP();
    }
}

void MyI2C_SCL_W(uint8_t bitValue)
{
    GPIO_WriteBit(MPU6050_GPIO, MPU6050_SCL_Pin, bitValue ? Bit_SET : Bit_RESET);
    MyI2C_Delay();
}

void MyI2C_SDA_W(uint8_t bitValue)
{
    GPIO_WriteBit(MPU6050_GPIO, MPU6050_SDA_Pin, bitValue ? Bit_SET : Bit_RESET);
    MyI2C_Delay();
}

uint8_t MyI2C_SDA_R(void)
{
    uint8_t bitValue;

    bitValue = GPIO_ReadInputDataBit(MPU6050_GPIO, MPU6050_SDA_Pin);
    MyI2C_Delay();

    return bitValue;
}

static void MyI2C_BusRecover(void)
{
    uint8_t i;

    MyI2C_SDA_W(1);
    MyI2C_SCL_W(1);

    // 如果上一次通信中途复位，从机可能仍然拉低 SDA；补 9 个时钟尝试让它释放。
    for (i = 0; (i < 9) && (MyI2C_SDA_R() == 0); i++)
    {
        MyI2C_SCL_W(0);
        MyI2C_SCL_W(1);
    }

    MyI2C_Stop();
}

void MyI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    RCC_APB2PeriphClockCmd(MPU6050_RCC, ENABLE);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitTypeStructure.GPIO_Pin = MPU6050_SCL_Pin | MPU6050_SDA_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MPU6050_GPIO, &GPIO_InitTypeStructure);

    // 释放 SCL/SDA，总线空闲态应为两线高电平。
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(1);
    MyI2C_BusRecover();
}

void MyI2C_Start(void)
{
    // I2C 起始条件：SCL 为高时，SDA 从高变低。
    MyI2C_SDA_W(1);
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(0);
    MyI2C_SCL_W(0);
}

void MyI2C_Stop(void)
{
    // I2C 停止条件：SCL 为高时，SDA 从低变高。
    MyI2C_SDA_W(0);
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(1);
}

void MyI2C_SendByte(uint8_t byteValue)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        // SCL 低电平期间改变 SDA，SCL 高电平期间从机采样 SDA。
        MyI2C_SDA_W(byteValue & (0x80 >> i));
        MyI2C_SCL_W(1);
        MyI2C_SCL_W(0);
    }
}

uint8_t MyI2C_RecByte(void)
{
    uint8_t i;
    uint8_t byteValue = 0x00;

    // 主机释放 SDA，让从机在 SCL 低电平期间准备数据。
    MyI2C_SDA_W(1);

    for (i = 0; i < 8; i++)
    {
        MyI2C_SCL_W(1);
        if (MyI2C_SDA_R() == 1)
        {
            byteValue |= (0x80 >> i);
        }
        MyI2C_SCL_W(0);
    }

    return byteValue;
}

void MyI2C_SendACK(uint8_t ackValue)
{
    // ackValue=0 表示 ACK，ackValue=1 表示 NACK。
    MyI2C_SDA_W(ackValue);
    MyI2C_SCL_W(1);
    MyI2C_SCL_W(0);
}

uint8_t MyI2C_RecACK(void)
{
    uint8_t ackValue;

    // 主机释放 SDA 后，从机在第 9 个时钟周期拉低表示 ACK。
    MyI2C_SDA_W(1);
    MyI2C_SCL_W(1);
    ackValue = MyI2C_SDA_R();
    MyI2C_SCL_W(0);

    return ackValue;
}
