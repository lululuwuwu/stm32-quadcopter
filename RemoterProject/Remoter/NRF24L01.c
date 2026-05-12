#include "board.h"
#include <string.h>
#include "task.h"
#include "MyHSPI.h"
#include "Serial1.h"
#include "NRF24L01.h"
#include "remoterData.h"

/*
 * NRF24L01 无线模块驱动说明
 *
 * 本文件负责通过 SPI1 配置和访问 NRF24L01 芯片。NRF24L01 本身不是
 * UART 这种字节流外设，而是“寄存器 + FIFO”的无线收发芯片：
 *
 * 1. MCU 通过 SPI 写寄存器，配置地址、信道、速率、自动应答等参数。
 * 2. 发送时，MCU 把一帧 1~32 字节数据写入 TX FIFO，NRF 自动发射。
 * 3. 接收时，NRF 把收到的无线数据放入 RX FIFO，MCU 再通过 SPI 读出。
 * 4. 如果开启 Enhanced ShockBurst，接收方可以在 ACK 包里带一段返回数据，
 *    也就是本文件使用的 ACK payload。
 *
 * 当前遥控器侧的工作方式：
 *   - 遥控器初始化为 PTX 发送模式。
 *   - 周期性发送 25 字节 RCDATA 遥控数据包。
 *   - 飞行器如果在 ACK payload 里返回 17 字节状态包，本文件会解析后写入 RemoterData。
 */

/* NRF24L01 地址宽度，SETUP_AW=0x03 时地址为 5 字节。 */
#define NRF_TX_ADR_WIDTH 5
#define NRF_RX_ADR_WIDTH 5

/* NRF24L01 单帧 payload 最大长度为 32 字节。 */
#define NRF_RX_PLOAD_WIDTH 32
#define NRF_TX_PLOAD_WIDTH 32

/* 遥控器发给飞行器的数据包标记，以及飞行器 ACK 回传数据包标记。 */
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

/*
 * 本机发送地址和接收地址。
 *
 * 双向通信时，两端需要约定一致的地址。这里 TX/RX 都使用同一个地址，
 * 适合一对一遥控器和飞行器通信。后续如果要支持多个飞机或对频，
 * 可以把地址和信道做成可配置参数。
 */
uint8_t NRF_TX_ADDRESS[NRF_TX_ADR_WIDTH] = {0xA1, 0xA2, 0xA3, 0x01, 0x02};
uint8_t NRF_RX_ADDRESS[NRF_RX_ADR_WIDTH] = {0xA1, 0xA2, 0xA3, 0x01, 0x02};

/* 接收和发送缓存。 */
uint8_t NRF_RX_DATA[NRF_RX_PLOAD_WIDTH];
uint8_t NRF_TX_DATA[NRF_TX_PLOAD_WIDTH];

/* 上一次发送后收到的 ACK payload 长度。无线任务用它判断是否需要解析回传数据。 */
static uint8_t NRFLastAckLength;

/*
 * 写 NRF24L01 单字节寄存器。
 *
 * SPI 时序：CSN 拉低 -> 发送写寄存器命令和地址 -> 发送 1 字节数据 -> CSN 拉高。
 */
void NRF24L01_Write_Reg(uint8_t reg, uint8_t value)
{
    MyHSPI_Start();
    MyHSPI_SwapByte(reg);
    MyHSPI_SwapByte(value);
    MyHSPI_Stop();
}

/*
 * 读 NRF24L01 单字节寄存器。
 *
 * SPI 是全双工通信，发送读命令后再发送 0xFF，才能从 MISO 换回寄存器值。
 */
uint8_t NRF24L01_Read_Reg(uint8_t reg)
{
    uint8_t data;

    MyHSPI_Start();
    MyHSPI_SwapByte(reg);
    data = MyHSPI_SwapByte(0xFF);
    MyHSPI_Stop();

    return data;
}

/* 连续写多个字节，常用于写地址寄存器、TX payload、ACK payload。 */
void NRF24L01_Write_Buffer(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    MyHSPI_Start();
    MyHSPI_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        MyHSPI_SwapByte(buffer[i]);
    }
    MyHSPI_Stop();
}

/* 连续读多个字节，常用于读取 RX FIFO 中的一帧 payload。 */
void NRF24L01_Read_Buffer(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    MyHSPI_Start();
    MyHSPI_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        buffer[i] = MyHSPI_SwapByte(0xFF);
    }
    MyHSPI_Stop();
}

/* 只发送一个 NRF 命令，例如 FLUSH_TX/FLUSH_RX。 */
static void NRF24L01_Send_Command(uint8_t command)
{
    MyHSPI_Start();
    MyHSPI_SwapByte(command);
    MyHSPI_Stop();
}

/* 简单校验和：用于发现包头错位或常见数据损坏。 */
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

/* 按小端格式写入 16 位数据，便于两端协议保持一致。 */
static void NRF24L01_PutInt16(uint8_t *buffer, uint8_t index, int16_t value)
{
    buffer[index] = (uint8_t)(value & 0xFF);
    buffer[index + 1] = (uint8_t)(((uint16_t)value >> 8) & 0xFF);
}


static int16_t NRF24L01_GetInt16(const uint8_t *buffer, uint8_t index)
{
    return (int16_t)((uint16_t)buffer[index] | ((uint16_t)buffer[index + 1] << 8));
}

static uint16_t NRF24L01_GetUInt16(const uint8_t *buffer, uint8_t index)
{
    return (uint16_t)((uint16_t)buffer[index] | ((uint16_t)buffer[index + 1] << 8));
}

static int32_t NRF24L01_GetInt32(const uint8_t *buffer, uint8_t index)
{
    return (int32_t)((uint32_t)buffer[index] |
                     ((uint32_t)buffer[index + 1] << 8) |
                     ((uint32_t)buffer[index + 2] << 16) |
                     ((uint32_t)buffer[index + 3] << 24));
}

/*
 * 把当前 RemoterData 打包成 Excel 协议中的 25 字节 RCDATA 遥控数据。
 *
 * 包格式：
 *   [0:1]  帧头 = 0xAA 0xAF
 *   [2]    功能字 = 0x03
 *   [3]    数据长度 = 20
 *   [4:23] THR/YAW/ROL/PIT/AUX1~AUX6，10 个 int16 小端
 *   [24]   前 24 字节校验和
 */
static void NRF24L01_BuildRemoterPacket(uint8_t *packet)
{
    memset(packet, 0, NRF_REMOTER_TX_PAYLOAD_LENGTH);

    packet[0] = NRF_REMOTER_FRAME_HEAD_0;
    packet[1] = NRF_REMOTER_FRAME_HEAD_1;
    packet[2] = NRF_FRAME_ID_RCDATA;
    packet[3] = NRF_REMOTER_RCDATA_DATA_LENGTH;

    NRF24L01_PutInt16(packet, 4, RemoterData.THR);
    NRF24L01_PutInt16(packet, 6, RemoterData.YAW);
    NRF24L01_PutInt16(packet, 8, RemoterData.ROL);
    NRF24L01_PutInt16(packet, 10, RemoterData.PIT);
    NRF24L01_PutInt16(packet, 12, RemoterData.OffSet_Pit);
    NRF24L01_PutInt16(packet, 14, RemoterData.OffSet_Rol);
    NRF24L01_PutInt16(packet, 16, RemoterData.OffSet_Yaw);
    NRF24L01_PutInt16(packet, 18, RemoterData.windows);
    NRF24L01_PutInt16(packet, 20, RemoterData.RC_low_power ? 1 : 0);
    NRF24L01_PutInt16(packet, 22, RemoterData.NRF_Channel);

    packet[24] = NRF24L01_Checksum(packet, 24);
}
static void NRF24L01_MarkFlyAckOk(void)
{
    RemoterData.fly_test_flag |= (1 << 2);

    if (RemoterData.NRF_RSSI_count <= 245)
    {
        RemoterData.NRF_RSSI_count += 5;
    }
    else
    {
        RemoterData.NRF_RSSI_count = 250;
    }
}

static uint8_t NRF24L01_CheckFlyAckFrame(const uint8_t *packet, uint8_t length, uint8_t func, uint8_t dataLength)
{
    uint8_t frameLength;

    frameLength = (uint8_t)(dataLength + 5);
    if (length < frameLength)
    {
        return 0;
    }

    if (packet[0] != NRF_FLY_FRAME_HEAD_0 || packet[1] != NRF_FLY_FRAME_HEAD_1 ||
        packet[2] != func || packet[3] != dataLength)
    {
        return 0;
    }

    if (packet[frameLength - 1] != NRF24L01_Checksum(packet, frameLength - 1))
    {
        return 0;
    }

    return 1;
}

/* 解析 01 STATUS：姿态、高度、飞行模式和解锁状态。 */
static uint8_t NRF24L01_ParseStatusAck(const uint8_t *packet, uint8_t length)
{
    int16_t rollCentidegree;
    int16_t pitchCentidegree;
    int16_t yawCentidegree;

    if (!NRF24L01_CheckFlyAckFrame(packet, length, NRF_FRAME_ID_STATUS, NRF_FLY_STATUS_DATA_LENGTH))
    {
        return 0;
    }

    rollCentidegree = NRF24L01_GetInt16(packet, 4);
    pitchCentidegree = NRF24L01_GetInt16(packet, 6);
    yawCentidegree = NRF24L01_GetInt16(packet, 8);

    RemoterData.X = pitchCentidegree / 100;
    RemoterData.Y = rollCentidegree / 100;
    RemoterData.Z = yawCentidegree / 100;
    RemoterData.H = (int)NRF24L01_GetInt32(packet, 10);

    return 1;
}

/* 解析 02 SENSER：当前遥控器端暂不显示原始值，只用它刷新 MPU 状态位。 */
static uint8_t NRF24L01_ParseSensorAck(const uint8_t *packet, uint8_t length)
{
    int16_t accX;
    int16_t accY;
    int16_t accZ;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;

    if (!NRF24L01_CheckFlyAckFrame(packet, length, NRF_FRAME_ID_SENSOR, NRF_FLY_SENSOR_DATA_LENGTH))
    {
        return 0;
    }

    accX = NRF24L01_GetInt16(packet, 4);
    accY = NRF24L01_GetInt16(packet, 6);
    accZ = NRF24L01_GetInt16(packet, 8);
    gyroX = NRF24L01_GetInt16(packet, 10);
    gyroY = NRF24L01_GetInt16(packet, 12);
    gyroZ = NRF24L01_GetInt16(packet, 14);

    if (accX != 0 || accY != 0 || accZ != 0 || gyroX != 0 || gyroY != 0 || gyroZ != 0)
    {
        RemoterData.fly_test_flag |= (1 << 0);
    }
    else
    {
        RemoterData.fly_test_flag &= (uint16_t)~(1 << 0);
    }

    return 1;
}

/* 解析 03 RCDATA：确认飞控端实际收到的控制数据格式正确。 */
static uint8_t NRF24L01_ParseRcDataAck(const uint8_t *packet, uint8_t length)
{
    return NRF24L01_CheckFlyAckFrame(packet, length, NRF_FRAME_ID_RCDATA, NRF_FLY_RCDATA_DATA_LENGTH);
}

/* 解析 05 POWER：电压单位按本项目约定为 mV，电流字段当前未使用。 */
static uint8_t NRF24L01_ParsePowerAck(const uint8_t *packet, uint8_t length)
{
    if (!NRF24L01_CheckFlyAckFrame(packet, length, NRF_FRAME_ID_POWER, NRF_FLY_POWER_DATA_LENGTH))
    {
        return 0;
    }

    RemoterData.Battery_Fly = NRF24L01_GetUInt16(packet, 4);
    RemoterData.Fly_low_power = 0;

    return 1;
}

static uint8_t NRF24L01_ParseFlyAck(const uint8_t *packet, uint8_t length)
{
    uint8_t result;

    if (length < 5)
    {
        return 0;
    }

    switch (packet[2])
    {
        case NRF_FRAME_ID_STATUS:
            result = NRF24L01_ParseStatusAck(packet, length);
            break;
        case NRF_FRAME_ID_SENSOR:
            result = NRF24L01_ParseSensorAck(packet, length);
            break;
        case NRF_FRAME_ID_RCDATA:
            result = NRF24L01_ParseRcDataAck(packet, length);
            break;
        case NRF_FRAME_ID_POWER:
            result = NRF24L01_ParsePowerAck(packet, length);
            break;
        default:
            result = 0;
            break;
    }

    if (result)
    {
        NRF24L01_MarkFlyAckOk();
    }

    return result;
}/*
 * 初始化 NRF24L01。
 *
 * mode:
 *   1：接收模式 PRX，用于等待对端发来的数据。
 *   0：发送模式 PTX，用于主动发射遥控数据。
 *
 * chanle:
 *   RF 信道号，范围 0~125。2.4GHz 实际频点约为 2400MHz + chanle MHz。
 */
void NRF24L01_Init(uint8_t mode, uint8_t chanle)
{
    uint32_t i;

    /* NRF24L01 上电后需要一段稳定时间。 */
    for (i = 0; i < 999999; i++)
        ;

    MyHSPI_Init();
    MyHSPI_NRF_CE(0);

    /* 清空可能残留的收发 FIFO 和状态位，避免上电后读到旧状态。 */
    NRF24L01_Send_Command(NRF_FLUSH_RX);
    NRF24L01_Send_Command(NRF_FLUSH_TX);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_STATUS, (1 << RX_DR_BIT) | (1 << TX_DS_BIT) | (1 << MAX_RT_BIT));

    /* CONFIG：上电、2 字节 CRC、开启 CRC，并由 mode 决定 PTX/PRX。 */
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_CONFIG, 0x0E | mode);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_EN_AA, 0x01);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_EN_RXADDR, 0x01);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_SETUP_AW, 0x03);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_SETUP_RETR, 0x1A);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_RF_CH, chanle);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_RF_SETUP, 0x0F);
    NRF24L01_Write_Buffer(NRF_WRITE_REG | NRF_RX_ADDR_P0, NRF_TX_ADDRESS, NRF_TX_ADR_WIDTH);
    NRF24L01_Write_Buffer(NRF_WRITE_REG | NRF_TX_ADDR, NRF_RX_ADDRESS, NRF_RX_ADR_WIDTH);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_DYNPD, 0x01);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_FEATURE, 0x06);

    MyHSPI_NRF_CE(1);

    for (i = 0; i < 999999; i++)
        ;
}

/*
 * 发送一帧数据，并读取可能随 ACK 返回的 payload。
 *
 * 返回：
 *   NRF_TX_RESULT_OK             发送成功，可能有 ACK payload。
 *   NRF_TX_RESULT_MAX_RT         达到最大重发次数，链路失败。
 *   NRF_TX_RESULT_INVALID_LENGTH 参数长度非法。
 *   NRF_TX_RESULT_FAIL           未确认发送成功。
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

    NRF24L01_Write_Buffer(NRF_W_TX_PLOAD, txBuffer, length);

    /* 等待自动重发和 ACK 完成。当前重发设置最多约 5ms，因此等待 6ms。 */
    vTaskDelay(pdMS_TO_TICKS(6));

    status = NRF24L01_Read_Reg(NRF_READ_REG | NRF_STATUS);
    NRF24L01_Write_Reg(NRF_WRITE_REG | NRF_STATUS, status);

    if (status & (1 << TX_DS_BIT))
    {
        result = NRF_TX_RESULT_OK;
    }

    if (status & (1 << RX_DR_BIT))
    {
        width = NRF24L01_Read_Reg(NRF_R_RX_PL_WID);
        if (width > 0 && width <= NRF_RX_PLOAD_WIDTH)
        {
            NRF24L01_Read_Buffer(NRF_R_RX_PLOAD, NRF_RX_DATA, width);
            NRFLastAckLength = width;
            result = NRF_TX_RESULT_OK;
        }
        else
        {
            NRFLastAckLength = 0;
        }

        NRF24L01_Send_Command(NRF_FLUSH_RX);
    }

    if (status & (1 << MAX_RT_BIT))
    {
        NRF24L01_Send_Command(NRF_FLUSH_TX);
        result = NRF_TX_RESULT_MAX_RT;
    }

    return result;
}

/*
 * 接收模式下轮询读取一帧数据。
 *
 * 当前遥控器侧主要使用发送模式 + ACK payload；这个函数保留给飞行器侧或调试时使用。
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

        NRF_TX_DATA[0]++;
        NRF24L01_Write_Buffer(NRF_W_ACK_PAYLOAD, NRF_TX_DATA, NRF_TX_PLOAD_WIDTH);
        NRF24L01_Send_Command(NRF_FLUSH_RX);
    }

    if (status & (1 << MAX_RT_BIT))
    {
        NRF24L01_Send_Command(NRF_FLUSH_TX);
    }

    return width;
}

uint8_t NRF24L01_GetLastAckLength(void)
{
    return NRFLastAckLength;
}

/*
 * 遥控器无线周期处理函数。
 *
 * 由 FreeRTOS 无线任务周期调用：
 *   1. 从 RemoterData 打包当前摇杆和微调数据。
 *   2. 通过 NRF24L01 发给飞行器。
 *   3. 如果 ACK payload 中带有飞行器状态，则解析后写回 RemoterData。
 *   4. 根据发送结果更新 NRF_Connect 和链路质量显示。
 */
void NRF24L01_RemoterUpdate(void)
{
    uint8_t txPacket[NRF_REMOTER_TX_PAYLOAD_LENGTH];
    uint8_t txResult;

    NRF24L01_BuildRemoterPacket(txPacket);
    txResult = NRF24L01_TX(txPacket, NRF_REMOTER_TX_PAYLOAD_LENGTH);

    if (txResult == NRF_TX_RESULT_OK)
    {
        RemoterData.NRF_Connect = 1;

        if (NRFLastAckLength > 0)
        {
            if (!NRF24L01_ParseFlyAck(NRF_RX_DATA, NRFLastAckLength))
            {
                /* 能收到 ACK 但协议不匹配，链路仍然存在，回传字段暂不更新。 */
                Serial1_SendLog("NRF ACK format error,len=%d\r\n", NRFLastAckLength);
            }
        }
        else if (RemoterData.NRF_RSSI_count < 250)
        {
            /* 没有 ACK payload 时，用递增计数给 OLED 一个“仍有链路”的粗略提示。 */
            RemoterData.NRF_RSSI_count += 5;
        }
    }
    else
    {
        RemoterData.NRF_Connect = 0;
        RemoterData.NRF_RSSI_count = 0;

        if (txResult == NRF_TX_RESULT_MAX_RT)
        {
            Serial1_SendLog("NRF MAX_RT\r\n");
        }
    }
}
