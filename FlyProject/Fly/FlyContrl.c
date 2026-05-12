/*
 * 文件名: FlyContrl.c
 * 功能说明:
 * 1. MOTOR1~4              四路电机 PWM 目标值，供控制任务写入，也供调试日志观察。
 * 2. FlyContrl_ThrottleToPwm 将遥控器油门通道 1000~2000 映射成 PWM 比较值。
 * 3. FlyContrl_Init        初始化飞控执行层依赖的 PWM 外设。
 * 4. FlyContrl_Unlock_10ms 10ms 周期执行解锁、手动锁定、失联锁定和低油门自动锁定。
 * 5. Limit                 将数值限制在指定上下限之间，避免越界写入 PWM 比较寄存器。
 * 6. FlyContrl_Motor_3ms   3ms 周期刷新四路电机 PWM 输出。
 *
 * 模块定位:
 * - 本文件处在遥控输入、飞控状态机和 PWM 底层驱动之间。
 * - NRF24L01 任务负责更新 FlyRemoter，解锁任务负责更新 FlyContrlFlag.unlock，电机任务负责把
 *   当前油门转换为四路 PWM 输出。
 * - 当前还没有接入姿态 PID 和四轴混控，所以解锁后四个电机使用同一个油门值同步输出。
 * - 后续加入姿态控制后，应以当前 throttlePwm 作为基础油门，再叠加 Pitch/Roll/Yaw 控制量。
 *
 * 安全约束:
 * - 上电默认锁定，未收到有效遥控数据时锁定，失联时锁定。
 * - 锁定状态下不仅清 FlyContrlFlag.unlock，还会把 MOTOR1~4 清 0，防止沿用旧输出。
 * - 当前 FLY_MOTOR_PWM_MAX 保守限制为 100，而 MyPWM 的 ARR 为 999，最大约 10% 占空比；
 *   这样便于拆桨或低功率阶段验证链路和解锁流程。需要更大转速时应先上板确认电机、电调、
 *   供电和螺旋桨方向，再逐步调大该值。
 */
#include "FlyContrl.h"
#include "FlyData.h"

/*
 * 解锁流程中的油门阈值。
 * 遥控器主通道已经在 NRF24L01 解析时限制到 1000~2000，这里再按动作语义设置判定点:
 * - LOW_START 用于第一步“油门低”的进入确认，略高于 1000，容忍摇杆和 ADC 的小抖动。
 * - LOW_DONE 用于第三步“油门回低”的确认，略高于 LOW_START，避免卡在临界值附近无法完成。
 * - HIGH 用于第二步“油门高 1 秒以上”的确认，要求接近满油门，降低误触发概率。
 */
#define FLY_UNLOCK_THR_LOW_START 1040
#define FLY_UNLOCK_THR_LOW_DONE 1060
#define FLY_UNLOCK_THR_HIGH 1940

/*
 * 手动锁定阈值。
 * 需求为“油门最低且偏航小于 1100 或大于 1900”立即上锁。这里油门最低使用 1100，
 * 比解锁低油门阈值更宽一些，方便用户在低油门附近快速执行偏航打杆锁定。
 */
#define FLY_LOCK_THR_LOW 1100
#define FLY_LOCK_YAW_LOW 1100
#define FLY_LOCK_YAW_HIGH 1900

/*
 * 解锁后清除自动上锁计数的油门阈值。
 * 解锁成功后，如果油门一直停在低位，说明用户可能只是解锁后未起飞；持续 6 秒会自动上锁。
 * 当油门超过该阈值时认为用户已经开始给油，低油门等待计数清零，允许正常飞行或电机测试。
 */
#define FLY_UNLOCK_THR_ACTIVE 1300

/*
 * FlyContrl_Unlock_10ms 的计数阈值。
 * 该函数由 FlyUnlockTask_10ms 每 10ms 调用一次，因此:
 * - 20 次约 200ms，用于确认低油门动作稳定存在。
 * - 100 次约 1000ms，用于确认高油门动作持续 1 秒以上。
 * - 600 次约 6000ms，用于解锁后低油门不动的自动上锁保护。
 */
#define FLY_UNLOCK_LOW_COUNT 20
#define FLY_UNLOCK_HIGH_COUNT 100
#define FLY_AUTO_LOCK_COUNT 600

/*
 * 遥控器油门通道范围。
 * NRF24L01_ParseRemoterPacket 已经将主通道限幅到 1000~2000，这里保留宏定义，
 * 让油门到 PWM 的映射关系集中可调，后续若遥控协议范围变化，只需要改这里。
 */
#define FLY_REMOTER_THR_MIN 1000
#define FLY_REMOTER_THR_MAX 2000

/*
 * 当前电机 PWM 输出范围。
 * MyPWM 底层定时器 ARR 为 999，理论 CCR 范围为 0~999。当前飞控层先保守使用 0~100，
 * 也就是约 0%~10% 占空比，避免刚接通油门映射时电机突然大功率转动。
 */
#define FLY_MOTOR_PWM_MIN 0
#define FLY_MOTOR_PWM_MAX 100

/*
 * 四路电机 PWM 目标值。
 * 当前单位与 MyPWM 的 CCR 比较值一致。变量保持为全局，是为了后续姿态控制器、调试日志
 * 或电机校准流程可以统一观察和调整目标输出。写入硬件前仍会经过 Limit 限幅。
 */
uint16_t MOTOR1, MOTOR2, MOTOR3, MOTOR4;

/*
 * 将遥控器油门值线性映射为 PWM 比较值。
 *
 * 输入:
 * - thr: FlyRemoter.THR，正常范围 1000~2000。
 *
 * 输出:
 * - FLY_MOTOR_PWM_MIN~FLY_MOTOR_PWM_MAX 范围内的 CCR 目标值。
 *
 * 设计说明:
 * - 低于最小油门直接返回 0，保证低油门和失联保护下不会出现负值或残余输出。
 * - 高于最大油门直接返回最大 PWM，防止协议异常或测试数据越界。
 * - 中间区间使用整数线性映射，不引入浮点计算，适合 STM32F103 + Keil 工程。
 * - 当前四个电机使用同一个 throttlePwm；后续姿态闭环接入后，建议保留本函数作为基础油门换算。
 */
static uint16_t FlyContrl_ThrottleToPwm(int16_t thr)
{
    int32_t pwm;

    if (thr <= FLY_REMOTER_THR_MIN)
    {
        return FLY_MOTOR_PWM_MIN;
    }
    if (thr >= FLY_REMOTER_THR_MAX)
    {
        return FLY_MOTOR_PWM_MAX;
    }

    pwm = ((int32_t)(thr - FLY_REMOTER_THR_MIN) * (FLY_MOTOR_PWM_MAX - FLY_MOTOR_PWM_MIN)) /
          (FLY_REMOTER_THR_MAX - FLY_REMOTER_THR_MIN);
    return (uint16_t)(pwm + FLY_MOTOR_PWM_MIN);
}

void FlyContrl_Init(void)
{
    /*
     * PWM 属于电机执行层的底层资源。
     * 初始化顺序放在电机刷新任务入口中，确保定时器和 GPIO 配置完成后，
     * FlyContrl_Motor_3ms 才会周期性写入四路 CCR。
     */
    MyPWM_Init();
}

/*
 * 飞控解锁和上锁状态机。
 *
 * 调用周期:
 * - 由 FlyUnlockTask_10ms 每 10ms 调用一次，内部所有计数都按 10ms 计算。
 *
 * 解锁流程:
 * - status 0: 等待油门低，并要求稳定约 200ms。
 * - status 1: 等待油门高，并要求稳定约 1000ms。
 * - status 2: 等待油门再次回低，并要求稳定约 200ms。
 * - status 3: 已解锁，允许电机任务按油门输出。
 *
 * 上锁条件:
 * - 没有遥控连接: 立即上锁，并回到 status 0。
 * - 手动锁定: 油门低于 1100 且偏航打到小于 1100 或大于 1900，立即上锁。
 * - 自动锁定: 解锁后油门持续低位约 6 秒，认为没有起飞或测试动作，自动上锁。
 *
 * 计数策略:
 * - 每个状态只在目标动作满足时递增 count，动作离开阈值后清零。
 * - 这样可以过滤摇杆抖动和无线数据瞬间跳变，避免误解锁。
 */
void FlyContrl_Unlock_10ms(void)
{
    static uint16_t count = 0;
    static uint8_t status = 0;
    uint8_t connected;
    int16_t thr;
    int16_t yaw;

    /*
     * 先把共享状态拷贝到局部变量。
     * FlyRemoter 由 NRF 任务更新，本函数由解锁任务读取。当前字段是 8/16 位基本类型，
     * 在 Cortex-M3 上读写成本很低；后续如果一次性读取更多复合状态，再考虑临界区或队列快照。
     */
    connected = FlyRemoter.Connected;
    thr = FlyRemoter.THR;
    yaw = FlyRemoter.YAW;

    if (connected == 0)
    {
        /*
         * 无线断链优先级最高。
         * 只在从解锁变为锁定时打印一次日志，避免失联期间 10ms 高频刷满串口队列。
         */
        if (FlyContrlFlag.unlock != 0)
        {
            Serial3_SendLog("fly lock: nrf lost\r\n");
        }
        FlyContrlFlag.unlock = 0;
        status = 0;
        count = 0;
        return;
    }

    if ((thr < FLY_LOCK_THR_LOW) && ((yaw < FLY_LOCK_YAW_LOW) || (yaw > FLY_LOCK_YAW_HIGH)))
    {
        /*
         * 手动锁定用于用户主动急停。
         * 条件要求“低油门 + 偏航大幅打杆”，避免正常飞行中单独偏航动作触发锁定。
         */
        if (FlyContrlFlag.unlock != 0)
        {
            Serial3_SendLog("fly lock: manual thr:%d yaw:%d\r\n", thr, yaw);
        }
        FlyContrlFlag.unlock = 0;
        status = 0;
        count = 0;
        return;
    }

    switch (status)
    {
        case 0:
            /*
             * 第一步: 油门低。
             * 解锁流程必须从低油门开始，保证用户在解锁前没有保持高油门，降低解锁瞬间电机转动风险。
             */
            if (thr < FLY_UNLOCK_THR_LOW_START)
            {
                if (++count >= FLY_UNLOCK_LOW_COUNT)
                {
                    count = 0;
                    status = 1;
                    Serial3_SendLog("fly unlock step:%d unlock:%d\r\n", status, FlyContrlFlag.unlock);
                }
            }
            else
            {
                /* 油门离开低位则重新计时，要求低油门动作连续稳定。 */
                count = 0;
            }
            break;

        case 1:
            /*
             * 第二步: 油门高持续 1 秒以上。
             * 使用较长持续时间是为了把“有意解锁动作”和普通摇杆扫过高位区分开。
             */
            if (thr > FLY_UNLOCK_THR_HIGH)
            {
                if (++count >= FLY_UNLOCK_HIGH_COUNT)
                {
                    count = 0;
                    status = 2;
                    Serial3_SendLog("fly unlock step:%d unlock:%d\r\n", status, FlyContrlFlag.unlock);
                }
            }
            else
            {
                /* 高油门未持续满 1 秒则重新计时，避免瞬时满油门误通过。 */
                count = 0;
            }
            break;

        case 2:
            /*
             * 第三步: 油门回低。
             * 真正设置 unlock=1 前，要求用户把油门重新收到底部，这样解锁成功后电机仍保持低输出。
             */
            if (thr < FLY_UNLOCK_THR_LOW_DONE)
            {
                if (++count >= FLY_UNLOCK_LOW_COUNT)
                {
                    FlyContrlFlag.unlock = 1;
                    status = 3;
                    count = 0;
                    Serial3_SendLog("fly unlock step:%d unlock:%d\r\n", status, FlyContrlFlag.unlock);
                }
            }
            else
            {
                count = 0;
            }
            break;

        case 3:
            /*
             * 已解锁状态。
             * 这里不直接写电机 PWM，只维护 unlock 标志和自动上锁计数；实际电机输出由
             * FlyContrl_Motor_3ms 统一完成，避免两个任务同时直接操作 PWM 外设。
             */
            if (thr < FLY_UNLOCK_THR_LOW_DONE)
            {
                if (++count >= FLY_AUTO_LOCK_COUNT)
                {
                    FlyContrlFlag.unlock = 0;
                    status = 0;
                    count = 0;
                    Serial3_SendLog("fly lock: idle thr low\r\n");
                }
            }
            else if (thr > FLY_UNLOCK_THR_ACTIVE)
            {
                /* 用户已经明显给油，说明进入正常控制阶段，清除低油门等待计数。 */
                count = 0;
            }
            break;

        default:
            /*
             * 理论上 status 只会是 0~3。进入 default 说明内存被破坏或未来修改状态值时遗漏分支，
             * 这里选择保守锁定并回到初始状态。
             */
            status = 0;
            count = 0;
            FlyContrlFlag.unlock = 0;
            break;
    }
}

/*
 * 通用限幅函数。
 *
 * 用途:
 * - 防止电机目标值超过 MyPWM 底层定时器允许的 CCR 范围。
 * - 防止未来叠加 PID 控制量后出现负值或过大值写入硬件。
 *
 * 注意:
 * - 当前参数类型为 uint32_t，因此调用方不要直接把有符号负数传进来。
 * - 本项目当前 MOTOR1~4 是 uint16_t，油门映射结果也是非负值，因此符合该函数假设。
 */
uint32_t Limit(uint32_t value, uint32_t min, uint32_t max)
{
    if (value >= max)
        value = max;
    if (value <= min)
        value = min;

    return value;
}

/*
 * 电机 PWM 周期刷新入口。
 *
 * 调用周期:
 * - 由 FlyContrlMotorTask_3ms 每 3ms 调用一次，频率高于遥控接收和解锁状态机。
 *
 * 输出策略:
 * - 锁定时: MOTOR1~4 全部清 0，并向 PWM 写 0，确保电机停转。
 * - 解锁时: 将 FlyRemoter.THR 映射为 throttlePwm，并同步写入四个电机。
 *
 * 为什么电机输出放在这里统一处理:
 * - 失联保护、手动锁定、自动锁定都只需要维护 FlyContrlFlag.unlock。
 * - 不同任务不直接争用 MyPWM_PWMx，PWM 外设写入路径更清晰。
 * - 后续接入姿态 PID 时，可在 else 分支里以 throttlePwm 为基础计算四个电机的混控输出。
 */
void FlyContrl_Motor_3ms(void)
{
    uint16_t throttlePwm;

    if (FlyContrlFlag.unlock == 0)
    {
        /* 锁定状态下清目标值，避免再次解锁前保留上一次油门输出。 */
        MOTOR1 = 0;
        MOTOR2 = 0;
        MOTOR3 = 0;
        MOTOR4 = 0;
    }
    else
    {
        /*
         * 当前阶段只实现油门直通。
         * 由于 FlyRemoter.THR 已在无线解析层限幅，这里再经过 FlyContrl_ThrottleToPwm 做输出限幅映射。
         */
        throttlePwm = FlyContrl_ThrottleToPwm(FlyRemoter.THR);
        MOTOR1 = throttlePwm;
        MOTOR2 = throttlePwm;
        MOTOR3 = throttlePwm;
        MOTOR4 = throttlePwm;
    }

    /*
     * 写入硬件前再次限幅。
     * 即使未来 MOTOR1~4 被姿态控制器或调试命令修改，也不会越过当前允许的 PWM 输出范围。
     */
    MyPWM_PWM1(Limit(MOTOR1, FLY_MOTOR_PWM_MIN, FLY_MOTOR_PWM_MAX));
    MyPWM_PWM2(Limit(MOTOR2, FLY_MOTOR_PWM_MIN, FLY_MOTOR_PWM_MAX));
    MyPWM_PWM3(Limit(MOTOR3, FLY_MOTOR_PWM_MIN, FLY_MOTOR_PWM_MAX));
    MyPWM_PWM4(Limit(MOTOR4, FLY_MOTOR_PWM_MIN, FLY_MOTOR_PWM_MAX));

    /*
     * 高频任务中串口日志会影响实时性，需要调试电机输出时再临时打开。
     * Serial3_SendLog 使用队列非阻塞投递，但大量日志仍会挤占串口任务和队列空间。
     */
    // Serial3_SendLog("m1:%d,m2:%d,m3:%d,m4:%d\n", MOTOR1, MOTOR2, MOTOR3, MOTOR4);
}
