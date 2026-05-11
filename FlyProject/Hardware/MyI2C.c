#include "board.h"

/*
 * 文件目录:
 * 1. MyI2C_Delay      软件 I2C 时序延时。
 * 2. MyI2C_SCL_W      控制 SCL 电平。
 * 3. MyI2C_SDA_W      控制 SDA 电平。
 * 4. MyI2C_SDA_R      读取 SDA 电平。
 * 5. MyI2C_BusRecover 总线异常恢复。
 * 6. MyI2C_Init       初始化 PB6/PB7 软件 I2C。
 * 7. MyI2C_Start      起始条件。
 * 8. MyI2C_Stop       停止条件。
 * 9. MyI2C_SendByte   发送字节。
 * 10. MyI2C_RecByte   接收字节。
 * 11. MyI2C_SendACK   发送 ACK/NACK。
 * 12. MyI2C_RecACK    接收 ACK/NACK。
 */

static void MyI2C_Delay(void)
{
    volatile uint16_t i;

    // 粗略延时，保证软件 I2C 速率低于 100KHz；上板后可按示波器波形微调。
    for (i = 0; i < 60; i++)
    {
        __NOP();
    }
}

// 操作 SCL：开漏输出写 1 表示释放总线，写 0 表示拉低。
void MyI2C_SCL_W(uint8_t bitValue)
{
    GPIO_WriteBit(MPU6050_GPIO, MPU6050_SCL_Pin, bitValue ? Bit_SET : Bit_RESET);
    MyI2C_Delay();
}

// 操作 SDA：开漏输出写 1 表示释放总线，写 0 表示拉低。
void MyI2C_SDA_W(uint8_t bitValue)
{
    GPIO_WriteBit(MPU6050_GPIO, MPU6050_SDA_Pin, bitValue ? Bit_SET : Bit_RESET);
    MyI2C_Delay();
}

// 读取 SDA，开漏输出模式下输入缓冲仍可读取外部电平。
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

    // 如果从机异常拉低 SDA，最多给 9 个时钟让它释放当前字节。
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

    // 默认释放总线，依赖外部上拉或模块板载上拉形成高电平。
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(1);
    MyI2C_BusRecover();
}

// 开始时序：SCL 高电平期间，SDA 从高到低。
void MyI2C_Start(void)
{
    MyI2C_SDA_W(1);
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(0);
    MyI2C_SCL_W(0);
}

// 停止时序：SCL 高电平期间，SDA 从低到高。
void MyI2C_Stop(void)
{
    MyI2C_SDA_W(0);
    MyI2C_SCL_W(1);
    MyI2C_SDA_W(1);
}

// 发送一个字节，高位先发。
void MyI2C_SendByte(uint8_t byteValue)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        MyI2C_SDA_W(byteValue & (0x80 >> i));
        MyI2C_SCL_W(1);
        MyI2C_SCL_W(0);
    }
}

// 接收一个字节，高位先收。
uint8_t MyI2C_RecByte(void)
{
    uint8_t i;
    uint8_t byteValue = 0x00;

    // 主机释放 SDA，让从机驱动数据线。
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

// 发送应答：0 为 ACK，1 为 NACK。
void MyI2C_SendACK(uint8_t ackValue)
{
    MyI2C_SDA_W(ackValue);
    MyI2C_SCL_W(1);
    MyI2C_SCL_W(0);
}

// 接收应答：返回 0 表示从机 ACK，返回 1 表示从机 NACK。
uint8_t MyI2C_RecACK(void)
{
    uint8_t ackValue;

    MyI2C_SDA_W(1);
    MyI2C_SCL_W(1);
    ackValue = MyI2C_SDA_R();
    MyI2C_SCL_W(0);

    return ackValue;
}
