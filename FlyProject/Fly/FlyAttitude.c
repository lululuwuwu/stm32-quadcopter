/*
 * 文件名: FlyAttitude.c
 * 功能说明:
 * 1. FlyAttitude_InvSqrt             计算平方根倒数，用于向量和四元数归一化。
 * 2. FlyAttitude_NormalizeYaw        把 Yaw 限制在 -180 到 180 度之间。
 * 3. FlyAttitude_Reset               清空四元数、积分误差和当前欧拉角。
 * 4. FlyAttitude_AngleToCentidegree  将浮点角度转换为百分之一度整数，便于串口打印。
 * 5. FlyAttitude_WaitMPU6050Ready    等待 MPU6050 上电稳定、检查 ID 并完成静止校准。
 * 6. FlyAttitude_UpdateAngle         使用四元数融合滤波后的加速度和陀螺仪数据，估算 Pitch/Roll/Yaw。
 * 7. FlyAttitude_PrintAngle          把当前姿态角格式化后通过串口 3 打印。
 * 8. FlyAttitudeTask_20ms            20ms 姿态读取任务，每 200ms 打印一次姿态。
 */
#include "board.h"
#include "FlyData.h"
#include "FlyAttitude.h"
#include <math.h>

#define FLY_ATTITUDE_PERIOD_MS 20         // 姿态读取周期，单位 ms。
#define FLY_ATTITUDE_LOG_DIVIDER 10       // 每读取 10 次打印一次，即 200ms 打印一次。
#define FLY_MPU6050_POWER_ON_DELAY_MS 500 // MPU6050 上电后先等待一段时间再访问寄存器。
#define FLY_MPU6050_RETRY_DELAY_MS 500    // MPU6050 初始化失败后的重试间隔。
#define FLY_MPU6050_EXPECTED_ID 0x68      // AD0 接低电平时 WHO_AM_I 应返回 0x68。
#define FLY_MPU6050_CALIBRATE_SAMPLES 256 // 启动时正式平均采样次数，用于计算零偏。
#define FLY_RAD_TO_DEG 57.2957795f        // 弧度转角度系数。
#define FLY_GYRO_TO_DEG 0.06097561f       // 陀螺仪正负 2000 deg/s 量程下，原始值转 deg/s。
#define FLY_GYRO_TO_RAD 0.00106465f       // 陀螺仪正负 2000 deg/s 量程下，原始值转 rad/s。
#define FLY_YAW_GYRO_DEAD_ZONE_DPS 1.0f   // Z 轴角速度死区，小抖动不参与 Yaw 积分。
#define FLY_IMU_KP 0.8f                   // 加速度误差对陀螺仪的比例修正系数。
#define FLY_IMU_KI 0.0003f                // 加速度误差对陀螺仪的积分修正系数。

typedef struct
{
    float x;
    float y;
    float z;
} FlyAttitude_Vector3Typedef;

typedef struct
{
    float q0;
    float q1;
    float q2;
    float q3;
} FlyAttitude_QuaternionTypedef;

static FlyAttitude_QuaternionTypedef FlyAttitude_Quaternion = {1.0f, 0.0f, 0.0f, 0.0f};
static FlyAttitude_Vector3Typedef FlyAttitude_GyroIntegError = {0.0f, 0.0f, 0.0f};

static float FlyAttitude_InvSqrt(float value)
{
    if (value <= 0.0f)
    {
        return 0.0f;
    }

    return 1.0f / sqrtf(value);
}

static float FlyAttitude_NormalizeYaw(float yaw)
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

static void FlyAttitude_Reset(void)
{
    FlyAttitude_Quaternion.q0 = 1.0f;
    FlyAttitude_Quaternion.q1 = 0.0f;
    FlyAttitude_Quaternion.q2 = 0.0f;
    FlyAttitude_Quaternion.q3 = 0.0f;
    FlyAttitude_GyroIntegError.x = 0.0f;
    FlyAttitude_GyroIntegError.y = 0.0f;
    FlyAttitude_GyroIntegError.z = 0.0f;
    FlyAngle.Pitch = 0.0f;
    FlyAngle.Roll = 0.0f;
    FlyAngle.Yaw = 0.0f;
}

static int32_t FlyAttitude_AngleToCentidegree(float angle)
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

static char FlyAttitude_AngleSign(int32_t angleValue)
{
    return (angleValue < 0) ? '-' : '+';
}

static int32_t FlyAttitude_AngleAbs(int32_t angleValue)
{
    return (angleValue < 0) ? -angleValue : angleValue;
}

static void FlyAttitude_WaitMPU6050Ready(void)
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
                    FlyAttitude_Reset();
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

static void FlyAttitude_UpdateAngle(MPU6050_DataTypedef *rawData)
{
    FlyAttitude_Vector3Typedef gravity;
    FlyAttitude_Vector3Typedef acc;
    FlyAttitude_Vector3Typedef gyro;
    FlyAttitude_Vector3Typedef accGravity;
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
    gravity.x = 2.0f * (FlyAttitude_Quaternion.q1 * FlyAttitude_Quaternion.q3 - FlyAttitude_Quaternion.q0 * FlyAttitude_Quaternion.q2);
    gravity.y = 2.0f * (FlyAttitude_Quaternion.q0 * FlyAttitude_Quaternion.q1 + FlyAttitude_Quaternion.q2 * FlyAttitude_Quaternion.q3);
    gravity.z = 1.0f - 2.0f * (FlyAttitude_Quaternion.q1 * FlyAttitude_Quaternion.q1 + FlyAttitude_Quaternion.q2 * FlyAttitude_Quaternion.q2);

    norm = FlyAttitude_InvSqrt((float)rawData->ACCEL_XOUT * rawData->ACCEL_XOUT +
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

    FlyAttitude_GyroIntegError.x += accGravity.x * FLY_IMU_KI;
    FlyAttitude_GyroIntegError.y += accGravity.y * FLY_IMU_KI;
    FlyAttitude_GyroIntegError.z += accGravity.z * FLY_IMU_KI;

    gyro.x = rawData->GYRO_XOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.x + FlyAttitude_GyroIntegError.x;
    gyro.y = rawData->GYRO_YOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.y + FlyAttitude_GyroIntegError.y;
    gyro.z = rawData->GYRO_ZOUT * FLY_GYRO_TO_RAD + FLY_IMU_KP * accGravity.z + FlyAttitude_GyroIntegError.z;

    // 一阶龙格库塔更新四元数，随后归一化，避免累计数值误差。
    q0Temp = (-FlyAttitude_Quaternion.q1 * gyro.x - FlyAttitude_Quaternion.q2 * gyro.y - FlyAttitude_Quaternion.q3 * gyro.z) * halfTime;
    q1Temp = ( FlyAttitude_Quaternion.q0 * gyro.x - FlyAttitude_Quaternion.q3 * gyro.y + FlyAttitude_Quaternion.q2 * gyro.z) * halfTime;
    q2Temp = ( FlyAttitude_Quaternion.q3 * gyro.x + FlyAttitude_Quaternion.q0 * gyro.y - FlyAttitude_Quaternion.q1 * gyro.z) * halfTime;
    q3Temp = (-FlyAttitude_Quaternion.q2 * gyro.x + FlyAttitude_Quaternion.q1 * gyro.y + FlyAttitude_Quaternion.q0 * gyro.z) * halfTime;

    FlyAttitude_Quaternion.q0 += q0Temp;
    FlyAttitude_Quaternion.q1 += q1Temp;
    FlyAttitude_Quaternion.q2 += q2Temp;
    FlyAttitude_Quaternion.q3 += q3Temp;

    norm = FlyAttitude_InvSqrt(FlyAttitude_Quaternion.q0 * FlyAttitude_Quaternion.q0 +
                               FlyAttitude_Quaternion.q1 * FlyAttitude_Quaternion.q1 +
                               FlyAttitude_Quaternion.q2 * FlyAttitude_Quaternion.q2 +
                               FlyAttitude_Quaternion.q3 * FlyAttitude_Quaternion.q3);
    if (norm <= 0.0f)
    {
        FlyAttitude_Reset();
        return;
    }

    FlyAttitude_Quaternion.q0 *= norm;
    FlyAttitude_Quaternion.q1 *= norm;
    FlyAttitude_Quaternion.q2 *= norm;
    FlyAttitude_Quaternion.q3 *= norm;

    vecxZ = 2.0f * FlyAttitude_Quaternion.q0 * FlyAttitude_Quaternion.q2 - 2.0f * FlyAttitude_Quaternion.q1 * FlyAttitude_Quaternion.q3;
    vecyZ = 2.0f * FlyAttitude_Quaternion.q2 * FlyAttitude_Quaternion.q3 + 2.0f * FlyAttitude_Quaternion.q0 * FlyAttitude_Quaternion.q1;
    veczZ = 1.0f - 2.0f * FlyAttitude_Quaternion.q1 * FlyAttitude_Quaternion.q1 - 2.0f * FlyAttitude_Quaternion.q2 * FlyAttitude_Quaternion.q2;

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
        FlyAngle.Yaw = FlyAttitude_NormalizeYaw(FlyAngle.Yaw);
    }
}

static void FlyAttitude_PrintAngle(void)
{
    int32_t pitch = FlyAttitude_AngleToCentidegree(FlyAngle.Pitch);
    int32_t roll = FlyAttitude_AngleToCentidegree(FlyAngle.Roll);
    int32_t yaw = FlyAttitude_AngleToCentidegree(FlyAngle.Yaw);
    int32_t pitchAbs = FlyAttitude_AngleAbs(pitch);
    int32_t rollAbs = FlyAttitude_AngleAbs(roll);
    int32_t yawAbs = FlyAttitude_AngleAbs(yaw);

    // 输出格式示例：Angle P:+1.23 R:-0.45 Y:+12.34
    // 使用整数拆分小数部分，避免 %f 在嵌入式工程里带来额外库依赖。
    Serial3_SendLog("Angle P:%c%d.%02d R:%c%d.%02d Y:%c%d.%02d\r\n",
        FlyAttitude_AngleSign(pitch), (int)(pitchAbs / 100), (int)(pitchAbs % 100),
        FlyAttitude_AngleSign(roll), (int)(rollAbs / 100), (int)(rollAbs % 100),
        FlyAttitude_AngleSign(yaw), (int)(yawAbs / 100), (int)(yawAbs % 100));
}

void FlyAttitudeTask_20ms(void *pvParameters)
{
    MPU6050_DataTypedef rawData;
    uint8_t logDivider = 0;
    uint8_t failCount = 0;

    // 初始化失败通常是软件 IIC 无 ACK：优先检查供电、GND、PB6/PB7 接线和上拉电阻。
    // 这里会等待上电稳定并自动重试，直到读到正确 ID 后才进入姿态读取循环。
    FlyAttitude_WaitMPU6050Ready();

    while (1)
    {
        vTaskDelay(FLY_ATTITUDE_PERIOD_MS);

        if (MPU6050_GetFilterData(&rawData) != 0)
        {
            FlyAttitude_UpdateAngle(&rawData);
            FlySensor.MPU6050_Ready = 1;
            FlySensor.ACC_X = rawData.ACCEL_XOUT;
            FlySensor.ACC_Y = rawData.ACCEL_YOUT;
            FlySensor.ACC_Z = rawData.ACCEL_ZOUT;
            FlySensor.GYRO_X = rawData.GYRO_XOUT;
            FlySensor.GYRO_Y = rawData.GYRO_YOUT;
            FlySensor.GYRO_Z = rawData.GYRO_ZOUT;
            failCount = 0;

            // 姿态使用校准和滤波后的数据做四元数融合；20ms 更新一次，200ms 打印一次。
            if (++logDivider >= FLY_ATTITUDE_LOG_DIVIDER)
            {
                logDivider = 0;
                FlyAttitude_PrintAngle();
            }
        }
        else
        {
            FlySensor.MPU6050_Ready = 0;

            // 读取失败只低频打印，避免 IIC 异常时刷满串口队列。
            if (++failCount >= 50)
            {
                failCount = 0;
                Serial3_SendLog("MPU6050 read fail\r\n");
            }
        }
    }
}
