/*
 * 文件名: NRF24L01.c
 * 功能说明:
 * 1. 使用 SPI2 驱动飞行器端 NRF24L01，实际引脚为 PB12/PB13/PB14/PB15/PB2/PA8。
 * 2. 飞行器端工作在 PRX 接收模式，接收遥控器 25 字节 RCDATA 遥控包。
 * 3. 通过 ACK payload 回传 17 字节飞行器状态，便于遥控器 OLED 做初步链路测试。
 */
#include "board.h"
#include "FlyData.h"
#include "NRF24L01.h"
#include <string.h>

/*
 * NRF24L01 飞控端驱动说明
 *
 * NRF24L01 不是串口字节流外设，而是“SPI 寄存器 + TX/RX FIFO”的 2.4GHz 无线芯片。
 * MCU 通过 SPI 配置地址、信道、自动应答、动态 payload 等寄存器，再从 FIFO 写入或读出
 * 单帧 1~32 字节 payload。当前飞控端使用 Enhanced ShockBurst 的 PRX 模式：
 *
 * 1. 遥控器作为 PTX，周期发送 25 字节 RCDATA 遥控包。
 * 2. 飞控端作为 PRX，常驻接收模式，收到有效 RCDATA 后更新 FlyRemoter。
 * 3. 飞控端预先写入 ACK payload，NRF24L01 在硬件 ACK 中自动回传 17 字节 STATUS 包。
 * 4. ACK payload 是由 NRF 芯片在应答时发出，不需要飞控端切换到主动发送模式。
 *
 * 重要约束：
 * - 两端地址、信道、地址宽度、RF_SETUP、自动应答、动态 payload 必须完全一致。
 * - 当前飞控端任务周期为 20ms，断链计数按 NRF_LINK_LOST_LIMIT 折算约 500ms。
 * - FlyRemoter 是飞控业务共享状态，当前由 NRF 任务写入，控制任务读取时要注意并发访问。
 */

/* SETUP_AW 写入 0x03 时 NRF24L01 地址宽度为 5 字节，两端必须保持一致。 */
#define NRF_TX_ADR_WIDTH 5
#define NRF_RX_ADR_WIDTH 5

/* NRF24L01 单帧 payload 硬件上限为 32 字节，这里收发缓存都按最大长度分配。 */
#define NRF_RX_PLOAD_WIDTH 32
#define NRF_TX_PLOAD_WIDTH 32

/* 遥控器 -> 飞控 RCDATA 包，以及飞控 -> 遥控器 STATUS ACK 包的固定协议字段。 */
#define NRF_REMOTER_FRAME_HEAD_0 0xAA
#define NRF_REMOTER_FRAME_HEAD_1 0xAF
#define NRF_FLY_FRAME_HEAD_0 0xAA
#define NRF_FLY_FRAME_HEAD_1 0xAA
#define NRF_FRAME_ID_STATUS 0x01
#define NRF_FRAME_ID_SENSOR 0x02
#define NRF_FRAME_ID_RCDATA 0x03
#define NRF_FRAME_ID_POWER 0x05
#define NRF_FLY_STATUS_DATA_LENGTH 12
#define NRF_FLY_SENSOR_DATA_LENGTH 18
#define NRF_FLY_RCDATA_DATA_LENGTH 20
#define NRF_FLY_POWER_DATA_LENGTH 4
#define NRF_REMOTER_RCDATA_DATA_LENGTH 20
#define NRF_REMOTER_RCDATA_LEGACY_LENGTH 21

/* AUX5 bit0 表示遥控器低电量；飞控端只缓存并上报，不在这里直接做保护动作。 */
#define NRF_REMOTER_FLAG_LOW_POWER 0x01

/* NRF 任务 20ms 调用一次，25 个周期约 500ms，无有效包时判定断链。 */
#define NRF_LINK_LOST_LIMIT 25

/* 解析错误码用于串口日志定位：长度不足、帧头/功能字错误、校验和错误。 */
#define NRF_PARSE_ERROR_LENGTH 1
#define NRF_PARSE_ERROR_MAGIC 2
#define NRF_PARSE_ERROR_CHECKSUM 3
#define NRF_PARSE_ERROR_FUNC 4
#define NRF_PARSE_ERROR_DATA_LENGTH 5

/*
 * 无线地址配置。
 *
 * 当前工程是一对一通信，TX/RX 使用同一组 5 字节地址，方便遥控器 PTX 和飞控 PRX
 * 在 pipe0 上完成自动应答。后续如果做对频或多机，需要同时修改两端地址和信道策略。
 */
static uint8_t NRF_TX_ADDRESS[NRF_TX_ADR_WIDTH] = {0xA1, 0xA2, 0xA3, 0x01, 0x02};
static uint8_t NRF_RX_ADDRESS[NRF_RX_ADR_WIDTH] = {0xA1, 0xA2, 0xA3, 0x01, 0x02};

/*
 * 全局收发缓存。
 *
 * NRF_RX_DATA 保存最近一次从 RX FIFO 读出的 payload；NRF_TX_DATA 保存即将写入 ACK payload
 * 的飞控状态包。数组在头文件中 extern，便于调试或其它模块临时查看，但正常业务应优先
 * 通过 FlyRemoter/FlyAngle 传递状态。
 */
uint8_t NRF_RX_DATA[NRF_RX_PLOAD_WIDTH];
uint8_t NRF_TX_DATA[NRF_TX_PLOAD_WIDTH];

/* 上一次收到 ACK payload 的长度；飞控 PRX 侧通常由 LoadAckPayload 设置为 17。 */
static uint8_t NRFLastAckLength;

/* 简单链路质量计数，收到有效包时递增，断链后清零，当前仅在本文件内维护。 */
static uint8_t NRFLinkQuality;

/* 最近一次 RCDATA 解析错误原因，用于限频打印串口日志。 */
static uint8_t NRFParseError;

/* 控制 CSN 片选脚。CSN 为低时 NRF24L01 接受 SPI 命令，高电平结束本次命令。 */
static void NRF24L01_CSN(uint8_t bitValue)
{
    GPIO_WriteBit(NRF24L01_CSN_GPIO, NRF24L01_CSN_PIN, (BitAction)bitValue);
}

/* 控制 CE 使能脚。PRX 模式下 CE=1 进入接收；PTX 模式下 CE 脉冲触发发送。 */
static void NRF24L01_CE(uint8_t bitValue)
{
    GPIO_WriteBit(NRF24L01_CE_GPIO, NRF24L01_CE_PIN, (BitAction)bitValue);
}

/*
 * 初始化飞控端 NRF24L01 使用的 SPI2 和控制引脚。
 *
 * 当前硬件连接：CSN=PB12，SCK=PB13，MISO=PB14，MOSI=PB15，IRQ=PB2，CE=PA8。
 * SPI 配置为 Mode 0（CPOL=0，CPHA=1Edge），8 位，MSB first，软件 NSS。
 */
static void NRF24L01_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    SPI_InitTypeDef SPI_InitTypeStructure;

    /* 先打开 GPIOA/GPIOB/AFIO 和 SPI2 时钟，否则后续 GPIO/SPI 配置不会生效。 */
    RCC_APB2PeriphClockCmd(NRF24L01_GPIOA_RCC | NRF24L01_GPIOB_RCC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(NRF24L01_SPI_RCC, ENABLE);

    /* CSN/CE 是普通推挽输出，由软件严格控制 NRF 的 SPI 片选和收发状态。 */
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_CSN_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24L01_CSN_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_CE_PIN;
    GPIO_Init(NRF24L01_CE_GPIO, &GPIO_InitTypeStructure);

    /* SCK/MOSI 由 SPI2 外设驱动，配置为复用推挽输出。 */
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_SCK_PIN | NRF24L01_MOSI_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24L01_SCK_GPIO, &GPIO_InitTypeStructure);

    /* MISO 和 IRQ 都来自 NRF24L01，使用上拉输入；当前 IRQ 只配置输入，接收仍采用轮询 STATUS。 */
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_MISO_PIN | NRF24L01_IRQ_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24L01_MISO_GPIO, &GPIO_InitTypeStructure);

    /* 重新初始化 SPI2，避免前面模块残留配置影响 NRF 通信。 */
    SPI_I2S_DeInit(NRF24L01_SPI);

    /* APB1 上 SPI2 时钟经 8 分频后通信，速度低于 NRF24L01 10MHz SPI 上限。 */
    SPI_InitTypeStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitTypeStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitTypeStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitTypeStructure.SPI_CRCPolynomial = 7;
    SPI_InitTypeStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitTypeStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitTypeStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitTypeStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitTypeStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(NRF24L01_SPI, &SPI_InitTypeStructure);
    SPI_Cmd(NRF24L01_SPI, ENABLE);

    /* 默认不选中芯片、不进入收发，等待 NRF24L01_Init 写完寄存器后再拉 CE。 */
    NRF24L01_CSN(1);
    NRF24L01_CE(0);
}

/* 开始一次 SPI 命令事务。 */
static void NRF24L01_Start(void)
{
    NRF24L01_CSN(0);
}

/* 结束一次 SPI 命令事务。 */
static void NRF24L01_Stop(void)
{
    NRF24L01_CSN(1);
}

/*
 * SPI 全双工交换 1 字节。
 *
 * NRF24L01 的读操作也必须同时写出一个哑字节，才能从 MISO 换回寄存器或 FIFO 数据。
 * 这里轮询 TXE/RXNE，调用者应避免在中断中使用，防止阻塞中断上下文。
 */
static uint8_t NRF24L01_SwapByte(uint8_t byteValue)
{
    while (SPI_I2S_GetFlagStatus(NRF24L01_SPI, SPI_I2S_FLAG_TXE) != SET)
        ;
    SPI_I2S_SendData(NRF24L01_SPI, byteValue);

    while (SPI_I2S_GetFlagStatus(NRF24L01_SPI, SPI_I2S_FLAG_RXNE) != SET)
        ;

    return (uint8_t)SPI_I2S_ReceiveData(NRF24L01_SPI);
}

/*
 * 写 NRF24L01 单字节寄存器。
 *
 * reg 需要由调用者传入 NRF_WRITE_REG | 寄存器地址；这样也能复用本函数发送某些
 * 单字节命令格式。STATUS 清标志时同样通过写寄存器完成，写 1 清对应中断位。
 */
void NRF24L01_Write_Reg(uint8_t reg, uint8_t value)
{
    NRF24L01_Start();
    NRF24L01_SwapByte(reg);
    NRF24L01_SwapByte(value);
    NRF24L01_Stop();
}

/*
 * 读 NRF24L01 单字节寄存器或特殊读命令返回值。
 *
 * 普通寄存器读取传入 NRF_READ_REG | 寄存器地址；读取动态 payload 宽度时传入
 * NRF_R_RX_PL_WID。第二个 SPI 字节发送 NRF_NOP，仅用于换回目标数据。
 */
uint8_t NRF24L01_Read_Reg(uint8_t reg)
{
    uint8_t data;

    NRF24L01_Start();
    NRF24L01_SwapByte(reg);
    data = NRF24L01_SwapByte(NRF_NOP);
    NRF24L01_Stop();

    return data;
}

/* 连续写多个字节，常用于写 5 字节地址、TX payload 或 ACK payload。 */
void NRF24L01_Write_Buffer(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    NRF24L01_Start();
    NRF24L01_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        NRF24L01_SwapByte(buffer[i]);
    }
    NRF24L01_Stop();
}

/* 连续读多个字节，常用于从 RX FIFO 读取一帧 payload。 */
void NRF24L01_Read_Buffer(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    NRF24L01_Start();
    NRF24L01_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        buffer[i] = NRF24L01_SwapByte(NRF_NOP);
    }
    NRF24L01_Stop();
}

/* 只发送一个 NRF24L01 命令，例如 FLUSH_TX/FLUSH_RX。 */
static void NRF24L01_Send_Command(uint8_t command)
{
    NRF24L01_Start();
    NRF24L01_SwapByte(command);
    NRF24L01_Stop();
}

/*
 * 解锁 FEATURE 寄存器。
 *
 * 部分 NRF24L01 兼容芯片上电后需要先发送 ACTIVATE(0x50) + 0x73，FEATURE/DYNPD
 * 才能被正常写入。这里用于开启动态 payload 和 ACK payload。
 */
static void NRF24L01_ActivateFeature(void)
{
    NRF24L01_Start();
    NRF24L01_SwapByte(0x50);
    NRF24L01_SwapByte(0x73);
    NRF24L01_Stop();
}

/* 简单累加校验：从帧头累加到最后一个数据字节，取低 8 位作为 SUM。 */
static uint8_t NRF24L01_Checksum(const uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    uint8_t checksum = 0;

    for (i = 0; i < length; i++)
    {
        checksum += buffer[i];
    }

    return checksum;
}

/* 按小端格式从协议包中读取 int16_t，多字节字段必须和遥控器端保持一致。 */
static int16_t NRF24L01_GetInt16(const uint8_t *buffer, uint8_t index)
{
    return (int16_t)((uint16_t)buffer[index] | ((uint16_t)buffer[index + 1] << 8));
}

/* 按小端格式写入 int16_t，用于构建飞控 STATUS ACK 包。 */
static void NRF24L01_PutInt16(uint8_t *buffer, uint8_t index, int16_t value)
{
    buffer[index] = (uint8_t)(value & 0xFF);
    buffer[index + 1] = (uint8_t)(((uint16_t)value >> 8) & 0xFF);
}

/* 按小端格式写入 int32_t；当前 STATUS 中 ALT_USE 高度字段暂填 0。 */
static void NRF24L01_PutInt32(uint8_t *buffer, uint8_t index, int32_t value)
{
    buffer[index] = (uint8_t)((uint32_t)value & 0xFF);
    buffer[index + 1] = (uint8_t)(((uint32_t)value >> 8) & 0xFF);
    buffer[index + 2] = (uint8_t)(((uint32_t)value >> 16) & 0xFF);
    buffer[index + 3] = (uint8_t)(((uint32_t)value >> 24) & 0xFF);
}

/* 按小端格式写入 uint16_t，用于 POWER 电压等无符号字段。 */
static void NRF24L01_PutUInt16(uint8_t *buffer, uint8_t index, uint16_t value)
{
    buffer[index] = (uint8_t)(value & 0xFF);
    buffer[index + 1] = (uint8_t)((value >> 8) & 0xFF);
}

/*
 * 遥控通道限幅。
 *
 * 即使遥控器端已经做过通道约束，飞控端仍再次限制到 1000~2000，避免无线干扰、
 * 协议错位或上位机测试数据导致控制量越界。
 */
static int16_t NRF24L01_LimitChannel(int16_t value)
{
    if (value < 1000)
    {
        return 1000;
    }
    if (value > 2000)
    {
        return 2000;
    }
    return value;
}

/*
 * 将浮点姿态角转换为协议中的 int16_t。
 *
 * 调用处会先乘以 100，把“度”转换为“0.01 度”；这里负责四舍五入并防止 int16_t 溢出。
 */
static int16_t NRF24L01_AngleToInt16(float angle)
{
    if (angle > 32767.0f)
    {
        return 32767;
    }
    if (angle < -32768.0f)
    {
        return -32768;
    }
    if (angle >= 0.0f)
    {
        return (int16_t)(angle + 0.5f);
    }
    return (int16_t)(angle - 0.5f);
}

/*
 * 解析遥控器发来的 25 字节 RCDATA 遥控包。
 *
 * 包格式：
 *   [0:1]  帧头 = 0xAA 0xAF
 *   [2]    功能字 = 0x03，表示 RCDATA
 *   [3]    DATA 长度 = 20，即 10 个 int16_t
 *   [4:5]  THR，油门通道，正常范围 1000~2000
 *   [6:7]  YAW，偏航通道，正常范围 1000~2000
 *   [8:9]  ROL，横滚通道，正常范围 1000~2000
 *   [10:11] PIT，俯仰通道，正常范围 1000~2000
 *   [12:13] AUX1，俯仰微调 OffSet_Pit
 *   [14:15] AUX2，横滚微调 OffSet_Rol
 *   [16:17] AUX3，偏航微调 OffSet_Yaw
 *   [18:19] AUX4，遥控器当前 OLED 窗口号
 *   [20:21] AUX5，bit0 表示遥控器低电量
 *   [22:23] AUX6，当前 NRF 信道号
 *   [24]    前 24 字节累加校验
 *
 * 返回 1 表示有效包已经写入 FlyRemoter；返回 0 表示包无效，NRFParseError 会记录原因。
 */
static uint8_t NRF24L01_ParseRemoterPacket(const uint8_t *packet, uint8_t length)
{
    int16_t aux4;
    int16_t aux5;
    int16_t aux6;

    NRFParseError = 0;

    /* 动态 payload 可能因为干扰读到异常长度，先挡掉短包，避免后续访问 packet[24] 越界。 */
    if (length < NRF_REMOTER_TX_PAYLOAD_LENGTH)
    {
        NRFParseError = NRF_PARSE_ERROR_LENGTH;
        return 0;
    }

    /* 校验方向帧头、功能字和 DATA 长度，防止把 STATUS 或错位数据误当遥控通道。 */
    if (packet[0] != NRF_REMOTER_FRAME_HEAD_0 || packet[1] != NRF_REMOTER_FRAME_HEAD_1)
    {
        NRFParseError = NRF_PARSE_ERROR_MAGIC;
        return 0;
    }

    if (packet[2] != NRF_FRAME_ID_RCDATA)
    {
        NRFParseError = NRF_PARSE_ERROR_FUNC;
        return 0;
    }

    if (packet[3] != NRF_REMOTER_RCDATA_DATA_LENGTH && packet[3] != NRF_REMOTER_RCDATA_LEGACY_LENGTH)
    {
        NRFParseError = NRF_PARSE_ERROR_DATA_LENGTH;
        return 0;
    }

    /* 协议校验和只做轻量完整性检查，不能替代 NRF24L01 自带 CRC，但能发现格式错位。 */
    if (packet[24] != NRF24L01_Checksum(packet, 24))
    {
        NRFParseError = NRF_PARSE_ERROR_CHECKSUM;
        return 0;
    }

    /* AUX 字段先取出再解释，便于后续扩展更多状态位。 */
    aux4 = NRF24L01_GetInt16(packet, 18);
    aux5 = NRF24L01_GetInt16(packet, 20);
    aux6 = NRF24L01_GetInt16(packet, 22);

    /* 收到有效包后立即刷新链路状态和遥控通道。油门等主通道再次限幅，微调量保留原始值。 */
    FlyRemoter.Connected = 1;
    FlyRemoter.Windows = (uint8_t)aux4;
    FlyRemoter.NRF_Channel = (uint8_t)aux6;
    FlyRemoter.RC_low_power = (aux5 & NRF_REMOTER_FLAG_LOW_POWER) ? 1 : 0;
    FlyRemoter.THR = NRF24L01_LimitChannel(NRF24L01_GetInt16(packet, 4));
    FlyRemoter.YAW = NRF24L01_LimitChannel(NRF24L01_GetInt16(packet, 6));
    FlyRemoter.ROL = NRF24L01_LimitChannel(NRF24L01_GetInt16(packet, 8));
    FlyRemoter.PIT = NRF24L01_LimitChannel(NRF24L01_GetInt16(packet, 10));
    FlyRemoter.OffSet_Pit = NRF24L01_GetInt16(packet, 12);
    FlyRemoter.OffSet_Rol = NRF24L01_GetInt16(packet, 14);
    FlyRemoter.OffSet_Yaw = NRF24L01_GetInt16(packet, 16);
    FlyRemoter.RxCount++;
    FlyRemoter.LostCount = 0;

    return 1;
}

static void NRF24L01_FillFrameHeader(uint8_t *packet, uint8_t func, uint8_t dataLength)
{
    packet[0] = NRF_FLY_FRAME_HEAD_0;
    packet[1] = NRF_FLY_FRAME_HEAD_1;
    packet[2] = func;
    packet[3] = dataLength;
}

/* 构建 01 STATUS：姿态、高度、飞行模式和解锁状态。 */
static uint8_t NRF24L01_BuildStatusAck(uint8_t *packet)
{
    NRF24L01_FillFrameHeader(packet, NRF_FRAME_ID_STATUS, NRF_FLY_STATUS_DATA_LENGTH);
    NRF24L01_PutInt16(packet, 4, NRF24L01_AngleToInt16(FlyAngle.Roll * 100.0f));
    NRF24L01_PutInt16(packet, 6, NRF24L01_AngleToInt16(FlyAngle.Pitch * 100.0f));
    NRF24L01_PutInt16(packet, 8, NRF24L01_AngleToInt16(FlyAngle.Yaw * 100.0f));
    NRF24L01_PutInt32(packet, 10, 0);
    packet[14] = 0;
    packet[15] = 0;
    packet[16] = NRF24L01_Checksum(packet, 16);
    return 17;
}

/* 构建 02 SENSER：MPU6050 六轴数据，MAG 当前无硬件来源时填 0。 */
static uint8_t NRF24L01_BuildSensorAck(uint8_t *packet)
{
    NRF24L01_FillFrameHeader(packet, NRF_FRAME_ID_SENSOR, NRF_FLY_SENSOR_DATA_LENGTH);
    if (FlySensor.MPU6050_Ready)
    {
        NRF24L01_PutInt16(packet, 4, FlySensor.ACC_X);
        NRF24L01_PutInt16(packet, 6, FlySensor.ACC_Y);
        NRF24L01_PutInt16(packet, 8, FlySensor.ACC_Z);
        NRF24L01_PutInt16(packet, 10, FlySensor.GYRO_X);
        NRF24L01_PutInt16(packet, 12, FlySensor.GYRO_Y);
        NRF24L01_PutInt16(packet, 14, FlySensor.GYRO_Z);
    }
    NRF24L01_PutInt16(packet, 16, FlySensor.MAG_X);
    NRF24L01_PutInt16(packet, 18, FlySensor.MAG_Y);
    NRF24L01_PutInt16(packet, 20, FlySensor.MAG_Z);
    packet[22] = NRF24L01_Checksum(packet, 22);
    return 23;
}

/* 构建 03 RCDATA：飞控端实际收到并限幅后的遥控通道。 */
static uint8_t NRF24L01_BuildRcDataAck(uint8_t *packet)
{
    NRF24L01_FillFrameHeader(packet, NRF_FRAME_ID_RCDATA, NRF_FLY_RCDATA_DATA_LENGTH);
    NRF24L01_PutInt16(packet, 4, FlyRemoter.THR);
    NRF24L01_PutInt16(packet, 6, FlyRemoter.YAW);
    NRF24L01_PutInt16(packet, 8, FlyRemoter.ROL);
    NRF24L01_PutInt16(packet, 10, FlyRemoter.PIT);
    NRF24L01_PutInt16(packet, 12, FlyRemoter.OffSet_Pit);
    NRF24L01_PutInt16(packet, 14, FlyRemoter.OffSet_Rol);
    NRF24L01_PutInt16(packet, 16, FlyRemoter.OffSet_Yaw);
    NRF24L01_PutInt16(packet, 18, FlyRemoter.Windows);
    NRF24L01_PutInt16(packet, 20, FlyRemoter.RC_low_power ? 1 : 0);
    NRF24L01_PutInt16(packet, 22, FlyRemoter.NRF_Channel);
    packet[24] = NRF24L01_Checksum(packet, 24);
    return 25;
}

/* 构建 05 POWER：飞控电压单位按本项目约定使用 mV，电流暂填 0。 */
static uint8_t NRF24L01_BuildPowerAck(uint8_t *packet)
{
    NRF24L01_FillFrameHeader(packet, NRF_FRAME_ID_POWER, NRF_FLY_POWER_DATA_LENGTH);
    NRF24L01_PutUInt16(packet, 4, FlySensor.BatteryMv);
    NRF24L01_PutUInt16(packet, 6, 0);
    packet[8] = NRF24L01_Checksum(packet, 8);
    return 9;
}

static uint8_t NRF24L01_BuildFlyAck(uint8_t *packet)
{
    static uint8_t ackIndex;
    uint8_t length;

    memset(packet, 0, NRF_TX_PLOAD_WIDTH);

    switch (ackIndex)
    {
        case 0:
            length = NRF24L01_BuildStatusAck(packet);
            break;
        case 1:
            length = NRF24L01_BuildSensorAck(packet);
            break;
        case 2:
            length = NRF24L01_BuildRcDataAck(packet);
            break;
        default:
            length = NRF24L01_BuildPowerAck(packet);
            break;
    }

    ackIndex++;
    if (ackIndex >= 4)
    {
        ackIndex = 0;
    }

    return length;
}

/*
 * 将最新飞控状态写入 NRF24L01 ACK payload FIFO。
 *
 * PRX 模式下 ACK payload 必须在对端下一帧到来前预先写入 NRF 芯片；遥控器收到的
 * 可能是上一次或最近一次预装载的数据，这是 ACK payload 机制本身的时序特点。
 */
static void NRF24L01_LoadAckPayload(void)
{
    uint8_t length;

    length = NRF24L01_BuildFlyAck(NRF_TX_DATA);
    NRF24L01_Write_Buffer(NRF_W_ACK_PAYLOAD, NRF_TX_DATA, length);
    NRFLastAckLength = length;
}
/*
 * 初始化 NRF24L01。
 *
 * mode:
 *   NRF_MODEL_RX：PRX 接收模式，飞控端常用，等待遥控器发送 RCDATA。
 *   NRF_MODEL_TX：PTX 发送模式，保留给通用发送接口或临时测试。
 *
 * chanle:
 *   RF 信道号，范围 0~125，实际中心频点约为 2400MHz + chanle MHz。
 */
void NRF24L01_Init(uint8_t mode, uint8_t chanle)
{
    uint32_t i;

    /* NRF24L01 上电后需要等待晶振和内部状态稳定。 */
    for (i = 0; i < 999999; i++)
        ;

    NRF24L01_SPI_Init();
    NRF24L01_CE(0);

    /* 清空可能残留的收发 FIFO，并写 1 清除 RX_DR/TX_DS/MAX_RT 三个状态位。 */
    NRF24L01_Send_Command(NRF_FLUSH_RX);
    NRF24L01_Send_Command(NRF_FLUSH_TX);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_STATUS, (1 << RX_DR_BIT) | (1 << TX_DS_BIT) | (1 << MAX_RT_BIT));

    /* FEATURE/DYNPD 相关功能需要先 ACTIVATE 解锁，兼容部分国产 NRF24L01+ 模块。 */
    NRF24L01_ActivateFeature();

    /* CONFIG=0x0E|mode：PWR_UP=1，CRC 开启，2 字节 CRC，由 mode 决定 PRIM_RX。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_CONFIG, 0x0E | mode);
    /* pipe0 开启自动应答和接收地址，当前通信只使用 pipe0。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_EN_AA, 0x01);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_EN_RXADDR, 0x01);

    /* 地址宽度 5 字节；自动重发参数主要服务 PTX 模式，PRX 侧保持两端配置一致。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_SETUP_AW, 0x03);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_SETUP_RETR, 0x1A);

    /* 信道和射频参数必须和遥控器端一致，否则双方无法建立链路。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_RF_CH, chanle);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_RF_SETUP, 0x0F);

    /* pipe0 接收地址和 TX_ADDR 都写同一组地址，满足自动 ACK 和 ACK payload 的地址匹配要求。 */
    NRF24L01_Write_Buffer(NRF_WRITE_REG | NRF_RX_ADDR_P0, NRF_TX_ADDRESS, NRF_RX_ADR_WIDTH);
    NRF24L01_Write_Buffer(NRF_WRITE_REG | NRF_TX_ADDR, NRF_RX_ADDRESS, NRF_TX_ADR_WIDTH);

    /* DYNPD bit0 开启 pipe0 动态 payload；FEATURE=0x06 开启 ACK payload 和动态 payload。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_DYNPD, 0x01);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_FEATURE, 0x06);

    if (mode == NRF_MODEL_RX)
    {
        /* PRX 模式先预装一帧 ACK payload，再拉高 CE 进入持续接收。 */
        NRF24L01_LoadAckPayload();
        NRF24L01_CE(1);
    }
    else
    {
        NRF24L01_CE(0);
    }

    for (i = 0; i < 999999; i++)
        ;
}

/*
 * 发送一帧 payload，并读取可能随 ACK 返回的数据。
 *
 * 飞控端当前主流程不主动发送，但保留该接口便于调试或未来扩展。PTX 发送过程为：
 * 写 TX FIFO -> CE 拉高触发发送 -> 等待自动重发/ACK 完成 -> 读取 STATUS -> 处理 ACK payload。
 *
 * 返回值：
 *   NRF_TX_RESULT_OK             发送成功，可能同时收到 ACK payload
 *   NRF_TX_RESULT_MAX_RT         达到最大重发次数，链路未确认
 *   NRF_TX_RESULT_INVALID_LENGTH 参数长度为 0 或超过 32 字节
 *   NRF_TX_RESULT_FAIL           未读到成功或最大重发标志
 */
uint8_t NRF24L01_TX(uint8_t *txBuffer, uint8_t length)
{
    uint8_t status;
    uint8_t width;
    uint8_t result = NRF_TX_RESULT_FAIL;

    NRFLastAckLength = 0;

    if (length == 0 || length > NRF_TX_PLOAD_WIDTH)
    {
        return NRF_TX_RESULT_INVALID_LENGTH;
    }

    NRF24L01_CE(0);
    NRF24L01_Write_Buffer(NRF_W_TX_PLOAD, txBuffer, length);
    NRF24L01_CE(1);

    /* SETUP_RETR=0x1A 时最大等待约数毫秒，这里等待 6ms 覆盖自动重发和 ACK 返回时间。 */
    vTaskDelay(pdMS_TO_TICKS(6));

    status = NRF24L01_Read_Reg(NRF_READ_REG | NRF_STATUS);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_STATUS, status);
    NRF24L01_CE(0);

    if (status & (1 << TX_DS_BIT))
    {
        result = NRF_TX_RESULT_OK;
    }

    if (status & (1 << RX_DR_BIT))
    {
        /* 动态 payload 模式下先读取本帧实际长度，再按长度读取 RX FIFO。 */
        width = NRF24L01_Read_Reg(NRF_R_RX_PL_WID);
        if (width > 0 && width <= NRF_RX_PLOAD_WIDTH)
        {
            NRF24L01_Read_Buffer(NRF_R_RX_PLOAD, NRF_RX_DATA, width);
            NRFLastAckLength = width;
            result = NRF_TX_RESULT_OK;
        }
        else
        {
            NRF24L01_Send_Command(NRF_FLUSH_RX);
        }
    }

    if (status & (1 << MAX_RT_BIT))
    {
        NRF24L01_Send_Command(NRF_FLUSH_TX);
        result = NRF_TX_RESULT_MAX_RT;
    }

    return result;
}

/*
 * PRX 模式轮询接收一帧 payload。
 *
 * 返回 0 表示没有有效新包；返回非 0 表示 NRF_RX_DATA 中已有对应长度的数据。
 * 函数读取 STATUS 后会立即写回清标志，并在读完一帧后清空 RX FIFO，避免旧包堆积。
 */
uint8_t NRF24L01_RX(void)
{
    uint8_t status;
    uint8_t width = 0;

    status = NRF24L01_Read_Reg(NRF_READ_REG | NRF_STATUS);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_STATUS, status);

    if (status & (1 << RX_DR_BIT))
    {
        width = NRF24L01_Read_Reg(NRF_R_RX_PL_WID);
        if (width > 0 && width <= NRF_RX_PLOAD_WIDTH)
        {
            NRF24L01_Read_Buffer(NRF_R_RX_PLOAD, NRF_RX_DATA, width);
        }
        else
        {
            width = 0;
        }

        NRF24L01_Send_Command(NRF_FLUSH_RX);
    }

    if (status & (1 << MAX_RT_BIT))
    {
        NRF24L01_Send_Command(NRF_FLUSH_TX);
    }

    return width;
}

/* 获取最近一次 ACK payload 长度，主要给发送侧或调试代码判断回传数据是否存在。 */
uint8_t NRF24L01_GetLastAckLength(void)
{
    return NRFLastAckLength;
}

/*
 * 飞控端 NRF 周期更新入口。
 *
 * FlyNRF24L01Task_20ms 每 20ms 调用一次：
 * 1. 轮询 NRF24L01_RX，取出遥控器 RCDATA。
 * 2. 解析有效包并刷新 FlyRemoter，同时重装 ACK payload。
 * 3. 对格式错误做限频日志，避免串口任务被高频错误刷屏。
 * 4. 连续丢包超过 NRF_LINK_LOST_LIMIT 后执行失控保护，油门降到 1000，其余通道回中。
 */
void NRF24L01_FlyUpdate(void)
{
    uint8_t length;
    static uint8_t logDivider;
    static uint16_t formatErrorCount;

    length = NRF24L01_RX();
    if (length > 0)
    {
        if (NRF24L01_ParseRemoterPacket(NRF_RX_DATA, length))
        {
            /* 有效包提升链路质量计数，上限留在 250，避免 uint8_t 溢出。 */
            if (NRFLinkQuality <= 245)
            {
                NRFLinkQuality += 5;
            }
            else
            {
                NRFLinkQuality = 250;
            }

            /* 每次有效接收后刷新 ACK payload，让遥控器尽快看到最新姿态状态。 */
            NRF24L01_LoadAckPayload();
            formatErrorCount = 0;

            if (++logDivider >= 50)
            {
                logDivider = 0;
                Serial3_SendLog("NRF OK THR:%d YAW:%d PIT:%d ROL:%d RX:%d\r\n",
                                FlyRemoter.THR, FlyRemoter.YAW, FlyRemoter.PIT, FlyRemoter.ROL, FlyRemoter.RxCount);
            }
        }
        else
        {
            /* 错包不更新 FlyRemoter，只记录错误并限频打印，避免错误日志挤占串口队列。 */
            formatErrorCount++;
            if (formatErrorCount == 1 || (formatErrorCount % 20) == 0)
            {
                Serial3_SendLog("NRF packet err:%d len:%d head:%02X %02X func:%02X/%02X dlen:%d/%d sum:%02X/%02X cnt:%d\r\n",
                                NRFParseError, length, NRF_RX_DATA[0], NRF_RX_DATA[1],
                                NRF_RX_DATA[2], NRF_FRAME_ID_RCDATA,
                                NRF_RX_DATA[3], NRF_REMOTER_RCDATA_DATA_LENGTH,
                                NRF_RX_DATA[24], NRF24L01_Checksum(NRF_RX_DATA, 24), formatErrorCount);
            }
            NRF24L01_LoadAckPayload();
        }
    }
    else if (FlyRemoter.Connected)
    {
        if (++FlyRemoter.LostCount >= NRF_LINK_LOST_LIMIT)
        {
            /* 断链保护：油门降到最低，姿态通道回中，防止沿用最后一次有效油门。 */
            FlyRemoter.Connected = 0;
            FlyRemoter.THR = 1000;
            FlyRemoter.YAW = 1500;
            FlyRemoter.PIT = 1500;
            FlyRemoter.ROL = 1500;
            FlyRemoter.LostCount = 0;
            NRFLinkQuality = 0;
            Serial3_SendLog("NRF link lost\r\n");
        }
    }
}
