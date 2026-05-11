#include "board.h"
#include "FlyData.h"
#include <math.h>

/*
 * 文件目录:
 * 1. xQueueSerial3              串口 3 日志队列句柄。
 * 2. FlyTask_UpdateAngle        根据 MPU6050 原始值计算姿态角。
 * 3. FlyTask_PrintAngle         通过串口打印姿态角。
 * 4. TaskSerial3                串口 3 日志发送任务。
 * 5. LEDTask                    LED 状态灯任务。
 * 6. FlyContrlMotorTask_3ms     3ms 电机输出刷新任务。
 * 7. MPU6050AttitudeTask_20ms   20ms 姿态读取任务。
 * 8. FlyCreateTask              统一创建飞控相关任务。
 */

#define FLY_ATTITUDE_PERIOD_MS 20
#define FLY_ATTITUDE_LOG_DIVIDER 10
#define FLY_RAD_TO_DEG 57.2957795f
#define FLY_ACCEL_SCALE_4G 8192.0f
#define FLY_GYRO_SCALE_2000DPS 16.4f

// 串口 3 日志队列句柄
QueueHandle_t xQueueSerial3;

static int32_t FlyTask_AngleToCentidegree(float angle)
{
	if (angle >= 0.0f)
	{
		return (int32_t)(angle * 100.0f + 0.5f);
	}
	else
	{
		return (int32_t)(angle * 100.0f - 0.5f);
	}
}

static char FlyTask_AngleSign(int32_t angleValue)
{
	return (angleValue < 0) ? '-' : '+';
}

static int32_t FlyTask_AngleAbs(int32_t angleValue)
{
	return (angleValue < 0) ? -angleValue : angleValue;
}

// MPU6050 当前配置为加速度 ±4g、陀螺仪 ±2000°/s。
static void FlyTask_UpdateAngle(MPU6050_DataTypedef *rawData)
{
	float ax = (float)rawData->ACCEL_XOUT / FLY_ACCEL_SCALE_4G;
	float ay = (float)rawData->ACCEL_YOUT / FLY_ACCEL_SCALE_4G;
	float az = (float)rawData->ACCEL_ZOUT / FLY_ACCEL_SCALE_4G;
	float gyroZ = (float)rawData->GYRO_ZOUT / FLY_GYRO_SCALE_2000DPS;

	// Pitch/Roll 使用重力方向估算；飞机加减速时会受到线加速度影响。
	FlyAngle.Pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * FLY_RAD_TO_DEG;
	FlyAngle.Roll = atan2f(ay, az) * FLY_RAD_TO_DEG;

	// MPU6050 没有磁力计，Yaw 只能由 Z 轴角速度积分，长时间运行会漂移。
	FlyAngle.Yaw += gyroZ * ((float)FLY_ATTITUDE_PERIOD_MS / 1000.0f);
	if (FlyAngle.Yaw > 180.0f)
	{
		FlyAngle.Yaw -= 360.0f;
	}
	else if (FlyAngle.Yaw < -180.0f)
	{
		FlyAngle.Yaw += 360.0f;
	}
}

static void FlyTask_PrintAngle(void)
{
	int32_t pitch = FlyTask_AngleToCentidegree(FlyAngle.Pitch);
	int32_t roll = FlyTask_AngleToCentidegree(FlyAngle.Roll);
	int32_t yaw = FlyTask_AngleToCentidegree(FlyAngle.Yaw);
	int32_t pitchAbs = FlyTask_AngleAbs(pitch);
	int32_t rollAbs = FlyTask_AngleAbs(roll);
	int32_t yawAbs = FlyTask_AngleAbs(yaw);

	Serial3_SendLog("Angle P:%c%d.%02d R:%c%d.%02d Y:%c%d.%02d\r\n",
		FlyTask_AngleSign(pitch), (int)(pitchAbs / 100), (int)(pitchAbs % 100),
		FlyTask_AngleSign(roll), (int)(rollAbs / 100), (int)(rollAbs % 100),
		FlyTask_AngleSign(yaw), (int)(yawAbs / 100), (int)(yawAbs % 100));
}

// 串口任务：从队列取出日志字符串并通过 USART3 发送。
void TaskSerial3(void *pvParameters)
{
	char msg[uxQueueSerial3ItemSize];

	Serial3_Init();

	while (1)
	{
		// 如果队列里面有数据就获取一个，发送串口；如果没有数据就阻塞无限等待

		/*

		如果从队列中成功接收项目，则返回 pdTRUE； 否则返回 pdFALSE。
		BaseType_t xQueueReceive(
						  QueueHandle_t xQueue, //要从中接收项目的队列的句柄。
						  void *pvBuffer,  //指向要将所接收项目复制到缓冲区的指针。
						  TickType_t xTicksToWait //如果队列为空，将 xTicksToWait 设置为 0 将导致函数立即返回 ;portMAX_DELAY 会导致任务无限期地阻塞 （没有超时）
		);

		*/

		xQueueReceive(xQueueSerial3, msg, portMAX_DELAY);
		Serial3_SendString(msg);
	}
}

// LED 状态灯任务，用于飞控运行状态提示。
void LEDTask(void *pvParameters)
{
	LED_Init();
	// 当前使用流水灯测试状态，后续可由飞控状态机统一切换。
	setLED.status = OneByOne; // OneByOne,AllFlashLight,AlternateFlash
	
	Serial3_SendLog("LED_Init ... \n");
	while (1)
	{
		LED_Blink();
	}
}


// 电机输出任务，按 3ms 周期把飞控目标值刷新到 PWM 外设。
void FlyContrlMotorTask_3ms(void *pvParameters)
{
	FlyContrl_Init();
	// 手动测试电机时再打开，避免上电误转。
//	MOTOR1 = 60;
//	MOTOR2 = 60;
//	MOTOR3 = 60;
//	MOTOR4 = 60;

	while (1)
	{
		vTaskDelay(3);

		FlyContrl_Motor_3ms();
	}
}

// 姿态读取任务：20ms 读取一次 MPU6050，200ms 打印一次欧拉角。
void MPU6050AttitudeTask_20ms(void *pvParameters)
{
	MPU6050_DataTypedef rawData;
	uint8_t logDivider = 0;
	uint8_t failCount = 0;

	if (MPU6050_Init() == 0)
	{
		Serial3_SendLog("MPU6050 init fail\r\n");
	}
	else
	{
		Serial3_SendLog("MPU6050 ID:0x%02X\r\n", MPU6050_Who_Am_I());
	}

	while (1)
	{
		vTaskDelay(FLY_ATTITUDE_PERIOD_MS);

		if (MPU6050_GetRawData(&rawData) != 0)
		{
			FlyTask_UpdateAngle(&rawData);
			failCount = 0;

			if (++logDivider >= FLY_ATTITUDE_LOG_DIVIDER)
			{
				logDivider = 0;
				FlyTask_PrintAngle();
			}
		}
		else
		{
			// 读取失败只低频打印，避免 I2C 异常时刷满串口队列。
			if (++failCount >= 50)
			{
				failCount = 0;
				Serial3_SendLog("MPU6050 read fail\r\n");
			}
		}
	}
}


// 统一创建飞控相关任务，创建完成后删除自身。
void FlyCreateTask(void *pvParameters)
{
	// 创建任务期间进入临界区，避免任务表尚未完整时被打断。
	taskENTER_CRITICAL();

	xTaskCreate(TaskSerial3, "TaskSerial3", 128, NULL, (configMAX_PRIORITIES -1), NULL);
	xTaskCreate(LEDTask, "LEDTask", 128, NULL, 1, NULL);
	xTaskCreate(FlyContrlMotorTask_3ms, "FlyContrlMotorTask_3ms", 128, NULL, 2, NULL);
	xTaskCreate(MPU6050AttitudeTask_20ms, "MPU6050Task", 256, NULL, 1, NULL);


	// 退出临界
	taskEXIT_CRITICAL();

	vTaskDelete(NULL);
}
