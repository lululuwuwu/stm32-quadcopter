#ifndef __MPU6050_H
#define __MPU6050_H



// 自定义MPU6050寄存器地址宏定义
#define	MPU6050_SMPLRT_DIV		0x19
#define	MPU6050_CONFIG			0x1A
#define	MPU6050_GYRO_CONFIG		0x1B
#define	MPU6050_ACCEL_CONFIG	0x1C

#define	MPU6050_ACCEL_XOUT_H	0x3B
#define	MPU6050_ACCEL_XOUT_L	0x3C
#define	MPU6050_ACCEL_YOUT_H	0x3D
#define	MPU6050_ACCEL_YOUT_L	0x3E
#define	MPU6050_ACCEL_ZOUT_H	0x3F
#define	MPU6050_ACCEL_ZOUT_L	0x40
#define	MPU6050_TEMP_OUT_H		0x41
#define	MPU6050_TEMP_OUT_L		0x42
#define	MPU6050_GYRO_XOUT_H		0x43
#define	MPU6050_GYRO_XOUT_L		0x44
#define	MPU6050_GYRO_YOUT_H		0x45
#define	MPU6050_GYRO_YOUT_L		0x46
#define	MPU6050_GYRO_ZOUT_H		0x47
#define	MPU6050_GYRO_ZOUT_L		0x48

#define	MPU6050_PWR_MGMT_1		0x6B
#define	MPU6050_PWR_MGMT_2		0x6C
#define	MPU6050_WHO_AM_I		0x75


typedef struct{
	
	int16_t ACCEL_XOUT;
	int16_t ACCEL_YOUT;
	int16_t ACCEL_ZOUT;
	int16_t TEMP_OUT;
	int16_t GYRO_XOUT;
	int16_t GYRO_YOUT;
	int16_t GYRO_ZOUT;
	
}MPU6050_DataTypedef;



void MPU6050_Init(void);
uint8_t MPU6050_Who_Am_I(void);
void MPU6050_GetRawData(MPU6050_DataTypedef *data);


#endif
