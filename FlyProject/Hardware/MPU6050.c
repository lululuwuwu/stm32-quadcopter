#include "board.h"
#include "MPU6050.h"

/*
 * 文件目录:
 * 1. MPU6050_Write_Reg_Value  软件 I2C 写 MPU6050 指定寄存器。
 * 2. MPU6050_ReadReg          软件 I2C 读 MPU6050 指定寄存器。
 * 3. MPU6050_ReadRegs         软件 I2C 连续读取多个寄存器。
 * 4. MPU6050_Init             初始化软件 I2C 和 MPU6050 工作参数。
 * 5. MPU6050_Who_Am_I         读取芯片 ID。
 * 6. MPU6050_GetRawData       读取加速度、温度、角速度原始值。
 */

#define MPU6050_WRITE_ADDR ((MPU6050_ADDR << 1) | 0x00)
#define MPU6050_READ_ADDR  ((MPU6050_ADDR << 1) | 0x01)

// 封装指定寄存器写入值，返回 1 表示成功，0 表示从机无应答。
uint8_t MPU6050_Write_Reg_Value(uint8_t regAddr, uint8_t regValue)
{
    MyI2C_Start();

    MyI2C_SendByte(MPU6050_WRITE_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_SendByte(regAddr);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_SendByte(regValue);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_Stop();
    return 1;
}

static uint8_t MPU6050_ReadReg(uint8_t regAddr, uint8_t *regValue)
{
    if (regValue == 0)
    {
        return 0;
    }

    MyI2C_Start();

    MyI2C_SendByte(MPU6050_WRITE_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_SendByte(regAddr);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_Start();

    MyI2C_SendByte(MPU6050_READ_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    *regValue = MyI2C_RecByte();
    MyI2C_SendACK(1); // 单字节读取结束，主机发送 NACK。
    MyI2C_Stop();

    return 1;
}

static uint8_t MPU6050_ReadRegs(uint8_t startRegAddr, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    if ((buffer == 0) || (length == 0))
    {
        return 0;
    }

    MyI2C_Start();

    MyI2C_SendByte(MPU6050_WRITE_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_SendByte(startRegAddr);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    MyI2C_Start();

    MyI2C_SendByte(MPU6050_READ_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    for (i = 0; i < length; i++)
    {
        buffer[i] = MyI2C_RecByte();

        if (i < (length - 1))
        {
            MyI2C_SendACK(0); // 还要继续读，主机发送 ACK。
        }
        else
        {
            MyI2C_SendACK(1); // 最后 1 字节，主机发送 NACK。
        }
    }

    MyI2C_Stop();
    return 1;
}

// 封装指定寄存器读值，读取失败时返回 0。
uint8_t MPU6050_Read_Reg_Value(uint8_t regAddr)
{
    uint8_t regValue = 0;

    MPU6050_ReadReg(regAddr, &regValue);
    return regValue;
}

uint8_t MPU6050_Init(void)
{
    uint8_t regValue;
    uint32_t timeout;

    MyI2C_Init();

    // 先软复位，再等待复位位自动清零。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_1, 0x80) == 0)
    {
        return 0;
    }

    for (timeout = 0; timeout < 999999; timeout++);

    timeout = 100;
    do
    {
        if (MPU6050_ReadReg(MPU6050_PWR_MGMT_1, &regValue) == 0)
        {
            return 0;
        }

        if ((regValue & 0x80) == 0x00)
        {
            break;
        }
    } while (timeout-- != 0);

    if ((regValue & 0x80) != 0x00)
    {
        return 0;
    }

    // 禁用 Sleep，使用陀螺仪 X 轴作为时钟源。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_1, 0x01) == 0)
    {
        return 0;
    }

    // 所有轴都不进入 standby 模式。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_2, 0x00) == 0)
    {
        return 0;
    }

    // DLPF 开启后陀螺仪输出频率为 1KHz，分频后采样率为 500Hz。
    if (MPU6050_Write_Reg_Value(MPU6050_SMPLRT_DIV, 0x01) == 0)
    {
        return 0;
    }

    // 开启低通滤波。
    if (MPU6050_Write_Reg_Value(MPU6050_CONFIG, 0x03) == 0)
    {
        return 0;
    }

    // 角速度量程配置为 ±2000 °/s，对应 16.4 LSB/(°/s)。
    if (MPU6050_Write_Reg_Value(MPU6050_GYRO_CONFIG, 0x18) == 0)
    {
        return 0;
    }

    // 加速度量程配置为 ±4g，对应 8192 LSB/g。
    if (MPU6050_Write_Reg_Value(MPU6050_ACCEL_CONFIG, 0x08) == 0)
    {
        return 0;
    }

    return 1;
}

uint8_t MPU6050_Who_Am_I(void)
{
    return MPU6050_Read_Reg_Value(MPU6050_WHO_AM_I);
}

// 获取 MPU6050 的原始寄存器数据，返回 1 表示成功，0 表示读取失败。
uint8_t MPU6050_GetRawData(MPU6050_DataTypedef *data)
{
    uint8_t buffer[14];

    if (data == 0)
    {
        return 0;
    }

    if (MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, buffer, sizeof(buffer)) == 0)
    {
        return 0;
    }

    data->ACCEL_XOUT = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
    data->ACCEL_YOUT = (int16_t)(((uint16_t)buffer[2] << 8) | buffer[3]);
    data->ACCEL_ZOUT = (int16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);
    data->TEMP_OUT = (int16_t)(((uint16_t)buffer[6] << 8) | buffer[7]);
    data->GYRO_XOUT = (int16_t)(((uint16_t)buffer[8] << 8) | buffer[9]);
    data->GYRO_YOUT = (int16_t)(((uint16_t)buffer[10] << 8) | buffer[11]);
    data->GYRO_ZOUT = (int16_t)(((uint16_t)buffer[12] << 8) | buffer[13]);

    return 1;
}
