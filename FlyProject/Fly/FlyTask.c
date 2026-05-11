/*
 * 文件名: FlyTask.c
 * 功能说明:
 * 1. xQueueSerial3              串口 3 日志队列句柄定义。
 * 2. FlyTask_AngleToCentidegree 将浮点角度转换为“百分之一度”的整数，便于串口打印。
 * 3. FlyTask_AngleSign          根据角度整数返回正负号。
 * 4. FlyTask_AngleAbs           返回角度整数绝对值。
 * 5. FlyTask_WaitMPU6050Ready   等待 MPU6050 上电稳定，完成 ID 检查和静止零偏校准。
 * 6. FlyTask_UpdateAngle        使用四元数融合滤波后的加速度和陀螺仪数据，估算 Pitch/Roll/Yaw。
 * 7. FlyTask_PrintAngle         把当前姿态角格式化后通过串口 3 打印。
 * 8. TaskSerial3                串口日志发送任务，从队列取字符串并发送到 USART3。
 * 9. LEDTask                    LED 状态灯任务，周期执行 LED_Blink。
 * 10. FlyContrlMotorTask_3ms    电机输出任务，每 3ms 刷新一次四路 PWM。
 * 11. MPU6050AttitudeTask_20ms  姿态读取任务，每 20ms 读取 MPU6050，每 200ms 打印一次姿态。
 * 12. FlyCreateTask             飞控任务统一创建入口，创建完成后删除自身。
 *
 * 调度说明:
 * - 电机输出任务优先级高于 LED 和 MPU6050 打印任务，避免日志和灯效影响电机刷新。
 * - MPU6050 姿态读取任务中串口打印做了分频，防止频繁日志填满队列。
 */
#include "board.h"
#include "FlyData.h"
#include <math.h>

#define FLY_ATTITUDE_PERIOD_MS 20       // 姿态读取周期，单位 ms。
#define FLY_ATTITUDE_LOG_DIVIDER 10     // 每读取 10 次打印一次，即 200ms 打印一次。
#define FLY_MPU6050_POWER_ON_DELAY_MS 500 // MPU6050 上电后先等待一段时间再访问寄存器。
#define FLY_MPU6050_RETRY_DELAY_MS 500    // MPU6050 初始化失败后的重试间隔。
#define FLY_MPU6050_EXPECTED_ID 0x68      // AD0 接低电平时 WHO_AM_I 应返回 0x68。
#define FLY_MPU6050_CALIBRATE_SAMPLES 256 // 启动时正式平均采样次数，用于计算零偏。
#define FLY_RAD_TO_DEG 57.2957795f      // 弧度转角度系数。
#define FLY_GYRO_TO_DEG 0.06097561f     // 陀螺仪正负 2000 deg/s 量程下，原始值转 deg/s。
#define FLY_GYRO_TO_RAD 0.00106465f     // 陀螺仪正负 2000 deg/s 量程下，原始值转 rad/s。
#define FLY_YAW_GYRO_DEAD_ZONE_DPS 1.0f // Z 轴角速度死区，小抖动不参与 Yaw 积分。
#define FLY_IMU_KP 0.8f                 // 加速度误差对陀螺仪的比例修正系数。
#define FLY_IMU_KI 0.0003f              // 加速度误差对陀螺仪的积分修正系数。

// 串口 3 日志队列句柄。main.c 创建队列，本文件定义句柄存储空间。
QueueHandle_t xQueueSerial3;

typedef struct
{
    float x;
    float y;
    float z;
} FlyTask_Vector3Typedef;

typedef struct
{
    float q0;
    float q1;
    float q2;
    float q3;
} FlyTask_QuaternionTypedef;

static FlyTask_QuaternionTypedef FlyTask_Quaternion = {1.0f, 0.0f, 0.0f, 0.0f};
static FlyTask_Vector3Typedef FlyTask_GyroIntegError = {0.0f, 0.0f, 0.0f};

static float FlyTask_InvSqrt(float value)
{
    if (value <= 0.0f)
    {
        return 0.0f;
    }

    return 1.0f / sqrtf(value);
}

static float FlyTask_NormalizeYaw(float yaw)
{
    if (yaw > 180.0f)
    {
        yaw -= 360.0f;
    }
    else if (yaw < -180.0f)
    {
        yaw += 360.0f;
    }

    return yaw;
}

static void FlyTask_AttitudeReset(void)
{
    FlyTask_Quaternion.q0 = 1.0f;
    FlyTask_Quaternion.q1 = 0.0f;
    FlyTask_Quaternion.q2 = 0.0f;
    FlyTask_Quaternion.q3 = 0.0f;
    FlyTask_GyroIntegError.x = 0.0f;
    FlyTask_GyroIntegError.y = 0.0f;
    FlyTask_GyroIntegError.z = 0.0f;
    FlyAngle.Pitch = 0.0f;
    FlyAngle.Roll = 0.0f;
    FlyAngle.Yaw = 0.0f;
}

static int32_t FlyTask_AngleToCentidegree(float angle)
{
    // 避免直接使用浮点 printf，降低 microlib 下的链接和代码体积风险。
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

static void FlyTask_WaitMPU6050Ready(void)
{
    uint8_t id;

    // 断电再上电时 MPU6050 可能还没完成内部复位，先给传感器留出稳定时间。
    vTaskDelay(FLY_MPU6050_POWER_ON_DELAY_MS);

    while (1)
    {
        if (MPU6050_Init() != 0)
        {
            id = MPU6050_Who_Am_I();
            if (id == FLY_MPU6050_EXPECTED_ID)
            {
                Serial3_SendLog("MPU6050 ID:0x%02X\r\n", id);
                Serial3_SendLog("MPU6050 calibrating, keep still...\r\n");
                if (MPU6050_Calibrate(FLY_MPU6050_CALIBRATE_SAMPLES) != 0)
                {
                    FlyTask_AttitudeReset();
                    Serial3_SendLog("MPU6050 calibrate ok\r\n");
                    return;
                }

                Serial3_SendLog("MPU6050 calibrate fail\r\n");
            }
            else
            {
                // 能通信但 ID 不对，通常是总线读值异常、地址不对或模块状态异常。
                Serial3_SendLog("MPU6050 ID error:0x%02X\r\n", id);
            }
        }
        else
        {
            Serial3_SendLog("MPU6050 init fail\r\n");
        }

        // 初始化或校准失败时不进入姿态读取流程，稍后重新初始化，避免一直打印无效角度。
        vTaskDelay(FLY_MPU6050_RETRY_DELAY_MS);
    }
}

static void FlyTask_UpdateAngle(MPU6050_DataTypedef *rawData)
{
    FlyTask_Vector3Typedef gravity;
    FlyTask_Vector3Typedef acc;
    FlyTask_Vector3Typedef gyro;
    FlyTask_Vector3Typedef accGravity;
    float q0Temp;
    float q1Temp;
    float q2Temp;
    float q3Temp;
    float norm;
    float halfTime = ((float)FLY_ATTITUDE_PERIOD_MS / 1000.0f) * 0.5f;
    float vecxZ;
    float vecyZ;
    float veczZ;
    float yawRateDeg;

    if (rawData == 0)
    {
        return;
    }

    // 当前四元数对应的重力方向，用它和加速度计测到的重力方向做误差修正。
    gravity.x = 2.0f * (FlyTask_Quaternion.q1 * FlyTask_Quaternion.q3 - FlyTask_Quaternion.q0 * FlyTask_Quaternion.q2);
    gravity.y = 2.0f * (FlyTask_Quaternion.q0 * FlyTask_Quaternion.q1 + FlyTask_Quaternion.q2 * FlyTask_Quaternion.q3);
    gravity.z = 1.0f - 2.0f * (FlyTask_Quaternion.q1 * FlyTask_Quaternion.q1 + FlyTask_Quaternion.q2 * FlyTask_Quaternion.q2);

    norm = FlyTask_InvSqrt((float)rawData->ACCEL_XOUT * rawData->ACCEL_XOUT +
                           (float)rawData->ACCEL_YOUT * rawData->ACCEL_YOUT +
                           (float)rawData->ACCEL_ZOUT * rawData->ACCEL_ZOUT);
    if (norm <= 0.0f)
    {
        return;
    }

    acc.x = rawData->ACCEL_XOUT * norm;
    acc.y = rawData->ACCEL_YOUT * norm;
    acc.z = rawData->ACCEL_ZOUT * norm;

    // 加速度方向和估算重力方向的叉乘，就是 Pitch/Roll 修正陀螺仪的误差量。
    accGravity.x = acc.y * gravity.z - acc.z * gravity.y;
    accGravity.y = acc.z * gravity.x - acc.x * gravity.z;
    accGravity.z = acc.x * gravity.y - acc.y * gravity.x;

    FlyTask_GyroIntegError.x += accGravity.x * FLY_IMU_KI;
    FlyTask_GyroIntegError.y += accGravity.y * FLY_IMU_KI;
    FlyTask_GyroIntegError.z += accGravity.z * FLY_IMU_KI;

    gyro.x = rawData->GYRO_XOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.x + FlyTask_GyroIntegError.x;
    gyro.y = rawData->GYRO_YOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.y + FlyTask_GyroIntegError.y;
    gyro.z = rawData->GYRO_ZOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.z + FlyTask_GyroIntegError.z;

    // 一阶龙格库塔更新四元数，随后归一化，避免累计数值误差。
    q0Temp = (-FlyTask_Quaternion.q1 * gyro.x - FlyTask_Quaternion.q2 * gyro.y - FlyTask_Quaternion.q3 * gyro.z) * halfTime;
    q1Temp = ( FlyTask_Quaternion.q0 * gyro.x - FlyTask_Quaternion.q3 * gyro.y + FlyTask_Quaternion.q2 * gyro.z) * halfTime;
    q2Temp = ( FlyTask_Quaternion.q3 * gyro.x + FlyTask_Quaternion.q0 * gyro.y - FlyTask_Quaternion.q1 * gyro.z) * halfTime;
    q3Temp = (-FlyTask_Quaternion.q2 * gyro.x + FlyTask_Quaternion.q1 * gyro.y + FlyTask_Quaternion.q0 * gyro.z) * halfTime;

    FlyTask_Quaternion.q0 += q0Temp;
    FlyTask_Quaternion.q1 += q1Temp;
    FlyTask_Quaternion.q2 += q2Temp;
    FlyTask_Quaternion.q3 += q3Temp;

    norm = FlyTask_InvSqrt(FlyTask_Quaternion.q0 * FlyTask_Quaternion.q0 +
                           FlyTask_Quaternion.q1 * FlyTask_Quaternion.q1 +
                           FlyTask_Quaternion.q2 * FlyTask_Quaternion.q2 +
                           FlyTask_Quaternion.q3 * FlyTask_Quaternion.q3);
    if (norm <= 0.0f)
    {
        FlyTask_AttitudeReset();
        return;
    }

    FlyTask_Quaternion.q0 *= norm;
    FlyTask_Quaternion.q1 *= norm;
    FlyTask_Quaternion.q2 *= norm;
    FlyTask_Quaternion.q3 *= norm;

    vecxZ = 2.0f * FlyTask_Quaternion.q0 * FlyTask_Quaternion.q2 - 2.0f * FlyTask_Quaternion.q1 * FlyTask_Quaternion.q3;
    vecyZ = 2.0f * FlyTask_Quaternion.q2 * FlyTask_Quaternion.q3 + 2.0f * FlyTask_Quaternion.q0 * FlyTask_Quaternion.q1;
    veczZ = 1.0f - 2.0f * FlyTask_Quaternion.q1 * FlyTask_Quaternion.q1 - 2.0f * FlyTask_Quaternion.q2 * FlyTask_Quaternion.q2;

    if (vecxZ > 1.0f)
    {
        vecxZ = 1.0f;
    }
    else if (vecxZ < -1.0f)
    {
        vecxZ = -1.0f;
    }

    FlyAngle.Pitch = asinf(vecxZ) * FLY_RAD_TO_DEG;
    FlyAngle.Roll = atan2f(vecyZ, veczZ) * FLY_RAD_TO_DEG;

    // 没有磁力计时 Yaw 仍只能积分陀螺仪；死区可以压住静止时的小零偏抖动。
    yawRateDeg = rawData->GYRO_ZOUT * FLY_GYRO_TO_DEG;
    if ((yawRateDeg > FLY_YAW_GYRO_DEAD_ZONE_DPS) || (yawRateDeg < -FLY_YAW_GYRO_DEAD_ZONE_DPS))
    {
        FlyAngle.Yaw += yawRateDeg * ((float)FLY_ATTITUDE_PERIOD_MS / 1000.0f);
        FlyAngle.Yaw = FlyTask_NormalizeYaw(FlyAngle.Yaw);
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

    // 输出格式示例：Angle P:+1.23 R:-0.45 Y:+12.34
    // 使用整数拆分小数部分，避免 %f 在嵌入式工程里带来额外库依赖。
    Serial3_SendLog("Angle P:%c%d.%02d R:%c%d.%02d Y:%c%d.%02d\r\n",
        FlyTask_AngleSign(pitch), (int)(pitchAbs / 100), (int)(pitchAbs % 100),
        FlyTask_AngleSign(roll), (int)(rollAbs / 100), (int)(rollAbs % 100),
        FlyTask_AngleSign(yaw), (int)(yawAbs / 100), (int)(yawAbs % 100));
}

void TaskSerial3(void *pvParameters)
{
    char msg[uxQueueSerial3ItemSize];

    Serial3_Init();

    while (1)
    {
        // 没有日志时永久阻塞，有日志时取出一条并串口发送。
        // 串口实际发送是阻塞的，因此它独立成任务，避免业务任务被发送耗时影响。
        xQueueReceive(xQueueSerial3, msg, portMAX_DELAY);
        Serial3_SendString(msg);
    }
}

void LEDTask(void *pvParameters)
{
    LED_Init();

    // 当前使用流水灯作为运行状态提示，后续可由飞控状态机切换 AlwaysOn/AlwaysOff 等模式。
    setLED.status = OneByOne;

    Serial3_SendLog("LED_Init ... \n");
    while (1)
    {
        LED_Blink();
    }
}

void FlyContrlMotorTask_3ms(void *pvParameters)
{
    FlyContrl_Init();

    // 手动测试电机时再打开，避免上电后电机误转。
    // MOTOR1 = 60;
    // MOTOR2 = 60;
    // MOTOR3 = 60;
    // MOTOR4 = 60;

    while (1)
    {
        vTaskDelay(3);
        FlyContrl_Motor_3ms();
    }
}

void MPU6050AttitudeTask_20ms(void *pvParameters)
{
    MPU6050_DataTypedef rawData;
    uint8_t logDivider = 0;
    uint8_t failCount = 0;

    // 初始化失败通常是软件 IIC 无 ACK：优先检查供电、GND、PB6/PB7 接线和上拉电阻。
    // 这里会等待上电稳定并自动重试，直到读到正确 ID 后才进入姿态读取循环。
    FlyTask_WaitMPU6050Ready();

    while (1)
    {
        vTaskDelay(FLY_ATTITUDE_PERIOD_MS);

        if (MPU6050_GetFilterData(&rawData) != 0)
        {
            FlyTask_UpdateAngle(&rawData);
            failCount = 0;

            // 姿态使用校准和滤波后的数据做四元数融合；20ms 更新一次，200ms 打印一次。
            if (++logDivider >= FLY_ATTITUDE_LOG_DIVIDER)
            {
                logDivider = 0;
                FlyTask_PrintAngle();
            }
        }
        else
        {
            // 读取失败只低频打印，避免 IIC 异常时刷满串口队列。
            if (++failCount >= 50)
            {
                failCount = 0;
                Serial3_SendLog("MPU6050 read fail\r\n");
            }
        }
    }
}

void FlyCreateTask(void *pvParameters)
{
    // 创建多个任务时进入临界区，避免任务创建到一半就被调度打断。
    taskENTER_CRITICAL();

    xTaskCreate(TaskSerial3, "TaskSerial3", 128, NULL, (configMAX_PRIORITIES - 1), NULL);
    xTaskCreate(LEDTask, "LEDTask", 128, NULL, 1, NULL);
    xTaskCreate(FlyContrlMotorTask_3ms, "FlyContrlMotorTask_3ms", 128, NULL, 2, NULL);
    xTaskCreate(MPU6050AttitudeTask_20ms, "MPU6050Task", 256, NULL, 1, NULL);

    taskEXIT_CRITICAL();

    // 创建器任务只负责启动其他任务，完成后删除自身释放栈空间。
    vTaskDelete(NULL);
}
