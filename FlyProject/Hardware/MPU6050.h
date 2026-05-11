/*
 * 文件名: MPU6050.h
 * 功能说明:
 * 1. MPU6050 寄存器宏定义       定义采样率、滤波、量程、数据输出、电源管理等寄存器地址。
 * 2. MPU6050_DataTypedef        保存 MPU6050 原始/滤波后的加速度、温度和角速度数据。
 * 3. MPU6050_Init               初始化软件 I2C 和 MPU6050 寄存器配置。
 * 4. MPU6050_Who_Am_I           读取 WHO_AM_I 芯片 ID。
 * 5. MPU6050_Write_Reg_Value    写指定寄存器。
 * 6. MPU6050_Read_Reg_Value     读指定寄存器。
 * 7. MPU6050_GetRawData         连续读取 14 字节原始传感器数据。
 * 8. MPU6050_Calibrate          静止采样并计算加速度/陀螺仪零偏。
 * 9. MPU6050_GetFilterData      获取扣零偏并滤波后的加速度/角速度数据。
 */
#ifndef __MPU6050_H
#define __MPU6050_H

// MPU6050 配置寄存器。
#define MPU6050_SMPLRT_DIV     0x19
#define MPU6050_CONFIG         0x1A
#define MPU6050_GYRO_CONFIG    0x1B
#define MPU6050_ACCEL_CONFIG   0x1C

// MPU6050 连续数据输出寄存器，从 ACCEL_XOUT_H 到 GYRO_ZOUT_L 共 14 字节。
#define MPU6050_ACCEL_XOUT_H   0x3B
#define MPU6050_ACCEL_XOUT_L   0x3C
#define MPU6050_ACCEL_YOUT_H   0x3D
#define MPU6050_ACCEL_YOUT_L   0x3E
#define MPU6050_ACCEL_ZOUT_H   0x3F
#define MPU6050_ACCEL_ZOUT_L   0x40
#define MPU6050_TEMP_OUT_H     0x41
#define MPU6050_TEMP_OUT_L     0x42
#define MPU6050_GYRO_XOUT_H    0x43
#define MPU6050_GYRO_XOUT_L    0x44
#define MPU6050_GYRO_YOUT_H    0x45
#define MPU6050_GYRO_YOUT_L    0x46
#define MPU6050_GYRO_ZOUT_H    0x47
#define MPU6050_GYRO_ZOUT_L    0x48

// 电源管理和芯片 ID 寄存器。
#define MPU6050_PWR_MGMT_1     0x6B
#define MPU6050_PWR_MGMT_2     0x6C
#define MPU6050_WHO_AM_I       0x75

#define MPU6050_ADDR           0x68    // AD0 接低电平时的 7 位 I2C 地址。
#define MPU6050_ACCEL_1G_RAW   8192    // 当前加速度量程为正负 4g，1g 对应 8192 LSB。

typedef struct
{
    int16_t ACCEL_XOUT; // X 轴加速度原始值或滤波值
    int16_t ACCEL_YOUT; // Y 轴加速度原始值或滤波值
    int16_t ACCEL_ZOUT; // Z 轴加速度原始值或滤波值
    int16_t TEMP_OUT;   // 温度原始值
    int16_t GYRO_XOUT;  // X 轴角速度原始值或滤波值
    int16_t GYRO_YOUT;  // Y 轴角速度原始值或滤波值
    int16_t GYRO_ZOUT;  // Z 轴角速度原始值或滤波值
} MPU6050_DataTypedef;

uint8_t MPU6050_Init(void);
uint8_t MPU6050_Who_Am_I(void);
uint8_t MPU6050_Write_Reg_Value(uint8_t regAddr, uint8_t regValue);
uint8_t MPU6050_Read_Reg_Value(uint8_t regAddr);
uint8_t MPU6050_GetRawData(MPU6050_DataTypedef *data);
uint8_t MPU6050_Calibrate(uint16_t sampleCount);
uint8_t MPU6050_GetFilterData(MPU6050_DataTypedef *data);

#endif
