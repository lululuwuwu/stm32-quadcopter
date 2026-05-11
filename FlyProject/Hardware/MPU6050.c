/*
 * 文件名: MPU6050.c
 * 功能说明:
 * 1. MPU6050_Write_Reg_Value  通过软件 I2C 写 MPU6050 指定寄存器。
 * 2. MPU6050_ReadReg          通过软件 I2C 读 MPU6050 指定寄存器，模块内部使用。
 * 3. MPU6050_ReadRegs         通过软件 I2C 从指定寄存器开始连续读取多个字节。
 * 4. MPU6050_Read_Reg_Value   对外提供单寄存器读取接口。
 * 5. MPU6050_Init             初始化软件 I2C，并配置 MPU6050 采样率、滤波和量程。
 * 6. MPU6050_Who_Am_I         读取 WHO_AM_I，正常应返回 0x68。
 * 7. MPU6050_GetRawData       连续读取加速度、温度、陀螺仪原始数据。
 * 8. MPU6050_Calibrate        静止采样并计算加速度/陀螺仪零偏。
 * 9. MPU6050_GetFilterData    扣除零偏后，对加速度做一维卡尔曼，对角速度做一阶低通。
 *
 * 通信说明:
 * - 当前不使用 STM32 硬件 I2C，全部走 MyI2C 软件 IIC。
 * - MPU6050 单字节读流程：写设备地址 -> 写寄存器地址 -> 重复起始 -> 读设备地址 -> 读数据 -> NACK -> STOP。
 * - 连续读取最后 1 字节必须发送 NACK，否则从机会认为主机还要继续读。
 */
#include "board.h"
#include "MPU6050.h"

#define MPU6050_WRITE_ADDR ((MPU6050_ADDR << 1) | 0x00)
#define MPU6050_READ_ADDR  ((MPU6050_ADDR << 1) | 0x01)
#define MPU6050_GYRO_LPF_LAST_WEIGHT 0.85f
#define MPU6050_GYRO_LPF_NOW_WEIGHT  0.15f
#define MPU6050_CALIBRATE_INTERVAL_MS 2
#define MPU6050_CALIBRATE_STABLE_INTERVAL_MS 10
#define MPU6050_CALIBRATE_STABLE_COUNT 30
#define MPU6050_CALIBRATE_STABLE_MAX_CHECKS 600
#define MPU6050_CALIBRATE_DISCARD_COUNT 100
#define MPU6050_GYRO_QUIET_RAW 5

typedef struct
{
    float LastP; // 上一次估计误差协方差
    float Now_P; // 当前预测误差协方差
    float out;   // 当前滤波输出值
    float Kg;    // 卡尔曼增益
    float Q;     // 过程噪声
    float R;     // 测量噪声
} MPU6050_Kalman1Typedef;

static MPU6050_Kalman1Typedef MPU6050_AccelEkf[3] = {
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.543f},
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.543f},
    {0.02f, 0.0f, 0.0f, 0.0f, 0.001f, 0.543f}
};

static int16_t MPU6050_Offset[6] = {0};
static int16_t MPU6050_LastGyro[3] = {0};
static uint8_t MPU6050_IsCalibrated = 0;

static int32_t MPU6050_Abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void MPU6050_Kalman1(MPU6050_Kalman1Typedef *ekf, float input)
{
    ekf->Now_P = ekf->LastP + ekf->Q;
    ekf->Kg = ekf->Now_P / (ekf->Now_P + ekf->R);
    ekf->out = ekf->out + ekf->Kg * (input - ekf->out);
    ekf->LastP = (1.0f - ekf->Kg) * ekf->Now_P;
}

static void MPU6050_FilterReset(void)
{
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        MPU6050_AccelEkf[i].LastP = 0.02f;
        MPU6050_AccelEkf[i].Now_P = 0.0f;
        MPU6050_AccelEkf[i].out = 0.0f;
        MPU6050_AccelEkf[i].Kg = 0.0f;
        MPU6050_LastGyro[i] = 0;
    }
}

uint8_t MPU6050_Write_Reg_Value(uint8_t regAddr, uint8_t regValue)
{
    MyI2C_Start();

    // 发送写地址，等待 MPU6050 应答。无应答通常表示接线、地址或供电异常。
    MyI2C_SendByte(MPU6050_WRITE_ADDR);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    // 指定要写入的寄存器地址。
    MyI2C_SendByte(regAddr);
    if (MyI2C_RecACK() != 0)
    {
        MyI2C_Stop();
        return 0;
    }

    // 写入寄存器数据。
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

    // 重复起始，不释放总线，切换为读方向。
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
    MPU6050_IsCalibrated = 0;
    MPU6050_FilterReset();

    // 软复位 MPU6050，复位后内部寄存器恢复默认值。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_1, 0x80) == 0)
    {
        return 0;
    }

    // 简单等待复位过程完成，随后轮询 DEVICE_RESET 位自动清零。
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

    // 退出 Sleep，选择 X 轴陀螺仪 PLL 作为时钟源，稳定性优于内部 RC。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_1, 0x01) == 0)
    {
        return 0;
    }

    // 所有加速度轴和陀螺仪轴都保持工作，不进入 standby。
    if (MPU6050_Write_Reg_Value(MPU6050_PWR_MGMT_2, 0x00) == 0)
    {
        return 0;
    }

    // DLPF 开启后陀螺仪输出频率为 1KHz，SMPLRT_DIV=1 得到 500Hz 采样率。
    if (MPU6050_Write_Reg_Value(MPU6050_SMPLRT_DIV, 0x01) == 0)
    {
        return 0;
    }

    // DLPF_CFG=3，降低高频噪声，适合作为初期姿态打印和调试配置。
    if (MPU6050_Write_Reg_Value(MPU6050_CONFIG, 0x03) == 0)
    {
        return 0;
    }

    // 陀螺仪量程设置为正负 2000 deg/s，对应 16.4 LSB/(deg/s)。
    if (MPU6050_Write_Reg_Value(MPU6050_GYRO_CONFIG, 0x18) == 0)
    {
        return 0;
    }

    // 加速度量程设置为正负 4g，对应 8192 LSB/g。
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

uint8_t MPU6050_GetRawData(MPU6050_DataTypedef *data)
{
    uint8_t buffer[14];

    if (data == 0)
    {
        return 0;
    }

    // 从 ACCEL_XOUT_H 连续读取 14 字节，避免多次单寄存器读取导致数据不一致。
    if (MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, buffer, sizeof(buffer)) == 0)
    {
        return 0;
    }

    // MPU6050 输出为高字节在前的有符号 16 位数据。
    data->ACCEL_XOUT = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
    data->ACCEL_YOUT = (int16_t)(((uint16_t)buffer[2] << 8) | buffer[3]);
    data->ACCEL_ZOUT = (int16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);
    data->TEMP_OUT = (int16_t)(((uint16_t)buffer[6] << 8) | buffer[7]);
    data->GYRO_XOUT = (int16_t)(((uint16_t)buffer[8] << 8) | buffer[9]);
    data->GYRO_YOUT = (int16_t)(((uint16_t)buffer[10] << 8) | buffer[11]);
    data->GYRO_ZOUT = (int16_t)(((uint16_t)buffer[12] << 8) | buffer[13]);

    return 1;
}

uint8_t MPU6050_Calibrate(uint16_t sampleCount)
{
    MPU6050_DataTypedef rawData;
    MPU6050_DataTypedef lastData;
    int32_t sum[6] = {0};
    int32_t gyroError[3];
    uint16_t stableCount = 0;
    uint16_t checkCount = 0;
    uint16_t i;

    if (sampleCount == 0)
    {
        return 0;
    }

    // 先确认陀螺仪连续一段时间变化很小，避免手还在碰飞机时就把运动量当成零偏。
    if (MPU6050_GetRawData(&lastData) == 0)
    {
        return 0;
    }

    while (stableCount < MPU6050_CALIBRATE_STABLE_COUNT)
    {
        vTaskDelay(MPU6050_CALIBRATE_STABLE_INTERVAL_MS);

        if (MPU6050_GetRawData(&rawData) == 0)
        {
            return 0;
        }

        gyroError[0] = rawData.GYRO_XOUT - lastData.GYRO_XOUT;
        gyroError[1] = rawData.GYRO_YOUT - lastData.GYRO_YOUT;
        gyroError[2] = rawData.GYRO_ZOUT - lastData.GYRO_ZOUT;
        lastData = rawData;

        if ((MPU6050_Abs32(gyroError[0]) <= MPU6050_GYRO_QUIET_RAW) &&
            (MPU6050_Abs32(gyroError[1]) <= MPU6050_GYRO_QUIET_RAW) &&
            (MPU6050_Abs32(gyroError[2]) <= MPU6050_GYRO_QUIET_RAW))
        {
            stableCount++;
        }
        else
        {
            stableCount = 0;
        }

        if (++checkCount > MPU6050_CALIBRATE_STABLE_MAX_CHECKS)
        {
            return 0;
        }
    }

    // 传感器刚稳定后的前几次数据仍可能偏散，先丢弃，再进入正式平均。
    for (i = 0; i < MPU6050_CALIBRATE_DISCARD_COUNT; i++)
    {
        if (MPU6050_GetRawData(&rawData) == 0)
        {
            return 0;
        }
        vTaskDelay(MPU6050_CALIBRATE_INTERVAL_MS);
    }

    for (i = 0; i < sampleCount; i++)
    {
        if (MPU6050_GetRawData(&rawData) == 0)
        {
            return 0;
        }

        sum[0] += rawData.ACCEL_XOUT;
        sum[1] += rawData.ACCEL_YOUT;
        sum[2] += rawData.ACCEL_ZOUT - MPU6050_ACCEL_1G_RAW;
        sum[3] += rawData.GYRO_XOUT;
        sum[4] += rawData.GYRO_YOUT;
        sum[5] += rawData.GYRO_ZOUT;

        // 校准采样间隔留给传感器更新数据，同时不长时间霸占 CPU。
        vTaskDelay(MPU6050_CALIBRATE_INTERVAL_MS);
    }

    // 校准时飞机应尽量水平静止；Z 轴加速度要保留 1g，只扣除传感器零偏。
    MPU6050_Offset[0] = (int16_t)(sum[0] / sampleCount);
    MPU6050_Offset[1] = (int16_t)(sum[1] / sampleCount);
    MPU6050_Offset[2] = (int16_t)(sum[2] / sampleCount);
    MPU6050_Offset[3] = (int16_t)(sum[3] / sampleCount);
    MPU6050_Offset[4] = (int16_t)(sum[4] / sampleCount);
    MPU6050_Offset[5] = (int16_t)(sum[5] / sampleCount);

    MPU6050_FilterReset();
    MPU6050_IsCalibrated = 1;

    return 1;
}

uint8_t MPU6050_GetFilterData(MPU6050_DataTypedef *data)
{
    MPU6050_DataTypedef rawData;
    int16_t accel[3];
    int16_t gyro[3];

    if (data == 0)
    {
        return 0;
    }

    if (MPU6050_GetRawData(&rawData) == 0)
    {
        return 0;
    }

    // 如果上层忘记校准，仍可输出数据；只是零偏未扣除，Yaw 漂移会更明显。
    if (MPU6050_IsCalibrated == 0)
    {
        MPU6050_FilterReset();
    }

    accel[0] = rawData.ACCEL_XOUT - MPU6050_Offset[0];
    accel[1] = rawData.ACCEL_YOUT - MPU6050_Offset[1];
    accel[2] = rawData.ACCEL_ZOUT - MPU6050_Offset[2];
    gyro[0] = rawData.GYRO_XOUT - MPU6050_Offset[3];
    gyro[1] = rawData.GYRO_YOUT - MPU6050_Offset[4];
    gyro[2] = rawData.GYRO_ZOUT - MPU6050_Offset[5];

    // 加速度信号用于姿态角估算，先做一维卡尔曼滤波抑制随机噪声。
    MPU6050_Kalman1(&MPU6050_AccelEkf[0], (float)accel[0]);
    MPU6050_Kalman1(&MPU6050_AccelEkf[1], (float)accel[1]);
    MPU6050_Kalman1(&MPU6050_AccelEkf[2], (float)accel[2]);

    data->ACCEL_XOUT = (int16_t)MPU6050_AccelEkf[0].out;
    data->ACCEL_YOUT = (int16_t)MPU6050_AccelEkf[1].out;
    data->ACCEL_ZOUT = (int16_t)MPU6050_AccelEkf[2].out;
    data->TEMP_OUT = rawData.TEMP_OUT;

    // 陀螺仪信号做一阶低通滤波。权重越大越平滑，但响应会更慢。
    data->GYRO_XOUT = (int16_t)(MPU6050_GYRO_LPF_LAST_WEIGHT * MPU6050_LastGyro[0] + MPU6050_GYRO_LPF_NOW_WEIGHT * gyro[0]);
    data->GYRO_YOUT = (int16_t)(MPU6050_GYRO_LPF_LAST_WEIGHT * MPU6050_LastGyro[1] + MPU6050_GYRO_LPF_NOW_WEIGHT * gyro[1]);
    data->GYRO_ZOUT = (int16_t)(MPU6050_GYRO_LPF_LAST_WEIGHT * MPU6050_LastGyro[2] + MPU6050_GYRO_LPF_NOW_WEIGHT * gyro[2]);

    MPU6050_LastGyro[0] = data->GYRO_XOUT;
    MPU6050_LastGyro[1] = data->GYRO_YOUT;
    MPU6050_LastGyro[2] = data->GYRO_ZOUT;

    return 1;
}
