# NRF24L01 遥控通信协议

本文档统一管理遥控器端和飞控端之间的 NRF24L01 通信协议。当前协议格式以 `MY_上位机 遥控器  飞控  通讯协议.xlsx` 中 `协议` 工作表为准，表内版本为 `V4.01`。

## 1. 通信角色

- 遥控器端：PTX，主动发送遥控数据包。
- 飞控端：PRX，常驻接收模式。
- 回传方向：飞控端通过 NRF24L01 ACK payload 回传状态数据，不单独主动发包。

通信时序：

```text
遥控器  -- 25 字节 RCDATA 遥控包 -->  飞控端
遥控器  <-- FUNC=01/02/03/05 回传 --  飞控端 ACK payload
```

当前遥控器任务周期为 20ms，飞控端 NRF 任务周期也为 20ms。ACK payload 是 NRF24L01 自动应答携带的数据，飞控端会轮流预装 `01 STATUS`、`02 SENSER`、`03 RCDATA`、`05 POWER`。

## 2. NRF24L01 基础参数

| 项目 | 当前值 | 说明 |
| --- | --- | --- |
| 地址宽度 | 5 字节 | SETUP_AW = 0x03 |
| 地址 | A1 A2 A3 01 02 | TX/RX 使用同一组地址 |
| 默认信道 | 72 | RF_CH = 72 |
| RF_SETUP | 0x0F | 当前沿用工程配置 |
| 自动应答 | 开启 pipe0 | EN_AA = 0x01 |
| 动态 payload | 开启 pipe0 | DYNPD = 0x01 |
| ACK payload | 开启 | FEATURE = 0x06 |
| SPI 模式 | Mode 0 | CPOL=0, CPHA=0 |
| 单包上限 | 32 字节 | NRF24L01 payload 最大 32 字节 |

当前双方必须统一使用：`RF_CH = 72`，地址 `A1 A2 A3 01 02`。

## 3. 通用帧格式

```text
HEAD0 HEAD1 FUNC LEN DATA... SUM
```

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| HEAD0/HEAD1 | 2 字节 | 帧头，区分方向 |
| FUNC | 1 字节 | 功能字 |
| LEN | 1 字节 | DATA 区长度，不包含帧头、功能字、长度字节和校验 |
| DATA | LEN 字节 | 业务数据 |
| SUM | 1 字节 | 从 HEAD0 累加到最后一个 DATA 字节，取低 8 位 |

| 方向 | 帧头 | 说明 |
| --- | --- | --- |
| 飞控 -> 遥控器 | `AA AA` | ACK payload 回传 |
| 遥控器 -> 飞控 | `AA AF` | 遥控器主动发送 |

多字节整数统一小端格式。校验示例：

```c
uint8_t checksum = 0;
for (i = 0; i < length_without_checksum; i++)
{
    checksum += packet[i];
}
```

## 4. 遥控器到飞控：03 RCDATA

总长度：25 字节。标准 `LEN = 20`，表示 DATA 区为 10 个 `int16_t`。当前飞控端兼容接收 `LEN = 21`，用于适配把 `SUM` 也计入长度的旧写法。

| 字节 | 字段 | 类型 | 当前工程含义 |
| --- | --- | --- | --- |
| 0 | HEAD0 | uint8_t | 固定 `0xAA` |
| 1 | HEAD1 | uint8_t | 固定 `0xAF` |
| 2 | FUNC | uint8_t | 固定 `0x03`，RCDATA |
| 3 | LEN | uint8_t | 固定 `20` |
| 4~5 | THR | int16_t | 油门，默认安全值 1000 |
| 6~7 | YAW | int16_t | 偏航，默认回中 1500 |
| 8~9 | ROL | int16_t | 翻滚，默认回中 1500 |
| 10~11 | PIT | int16_t | 俯仰，默认回中 1500 |
| 12~13 | AUX1 | int16_t | `OffSet_Pit`，俯仰微调 |
| 14~15 | AUX2 | int16_t | `OffSet_Rol`，翻滚微调 |
| 16~17 | AUX3 | int16_t | `OffSet_Yaw`，偏航微调 |
| 18~19 | AUX4 | int16_t | `windows`，遥控器当前 OLED 窗口号 |
| 20~21 | AUX5 | int16_t | bit0 表示 `RC_low_power` |
| 22~23 | AUX6 | int16_t | `NRF_Channel`，当前默认 72 |
| 24 | SUM | uint8_t | 前 24 字节累加校验 |

## 5. 飞控到遥控器：01 STATUS

总长度：17 字节。`LEN = 12`。

| 字节 | 字段 | 类型 | 当前工程含义 |
| --- | --- | --- | --- |
| 0 | HEAD0 | uint8_t | 固定 `0xAA` |
| 1 | HEAD1 | uint8_t | 固定 `0xAA` |
| 2 | FUNC | uint8_t | 固定 `0x01`，STATUS |
| 3 | LEN | uint8_t | 固定 `12` |
| 4~5 | ROL | int16_t | 翻滚角 `Roll * 100` |
| 6~7 | PIT | int16_t | 俯仰角 `Pitch * 100` |
| 8~9 | YAW | int16_t | 偏航角 `Yaw * 100` |
| 10~13 | ALT_USE | int32_t | 高度，单位 cm；当前暂填 0 |
| 14 | FLY_MODEL | uint8_t | 飞行模式，当前暂填 0 |
| 15 | ARMED | uint8_t | 解锁状态，当前暂填 0 |
| 16 | SUM | uint8_t | 前 16 字节累加校验 |

遥控器 OLED 当前映射：`PIT/100 -> X`，`ROL/100 -> Y`，`YAW/100 -> Z`，`ALT_USE -> H`。

## 6. 飞控到遥控器：02 SENSER

总长度：23 字节。`LEN = 18`。

| 字节 | 字段 | 类型 | 当前工程含义 |
| --- | --- | --- | --- |
| 0 | HEAD0 | uint8_t | 固定 `0xAA` |
| 1 | HEAD1 | uint8_t | 固定 `0xAA` |
| 2 | FUNC | uint8_t | 固定 `0x02`，SENSER |
| 3 | LEN | uint8_t | 固定 `18` |
| 4~5 | ACC_X | int16_t | MPU6050 加速度 X 轴滤波后原始量 |
| 6~7 | ACC_Y | int16_t | MPU6050 加速度 Y 轴滤波后原始量 |
| 8~9 | ACC_Z | int16_t | MPU6050 加速度 Z 轴滤波后原始量 |
| 10~11 | GYRO_X | int16_t | MPU6050 角速度 X 轴滤波后原始量 |
| 12~13 | GYRO_Y | int16_t | MPU6050 角速度 Y 轴滤波后原始量 |
| 14~15 | GYRO_Z | int16_t | MPU6050 角速度 Z 轴滤波后原始量 |
| 16~17 | MAG_X | int16_t | 当前无磁力计，暂填 0 |
| 18~19 | MAG_Y | int16_t | 当前无磁力计，暂填 0 |
| 20~21 | MAG_Z | int16_t | 当前无磁力计，暂填 0 |
| 22 | SUM | uint8_t | 前 22 字节累加校验 |

遥控器端当前不显示原始六轴数值，只用此帧刷新传感器页 MPU 状态位。

## 7. 飞控到遥控器：03 RCDATA

总长度：25 字节。`LEN = 20`。字段顺序与遥控器发送的 RCDATA 一致，但方向为飞控回传，用于确认“飞控实际收到并限幅后的控制数据”。

| 字节 | 字段 | 类型 | 当前工程含义 |
| --- | --- | --- | --- |
| 0~3 | HEAD/FUNC/LEN | - | `AA AA 03 20` |
| 4~5 | THR | int16_t | 飞控端缓存的油门 |
| 6~7 | YAW | int16_t | 飞控端缓存的偏航 |
| 8~9 | ROL | int16_t | 飞控端缓存的翻滚 |
| 10~11 | PIT | int16_t | 飞控端缓存的俯仰 |
| 12~13 | AUX1 | int16_t | `OffSet_Pit` |
| 14~15 | AUX2 | int16_t | `OffSet_Rol` |
| 16~17 | AUX3 | int16_t | `OffSet_Yaw` |
| 18~19 | AUX4 | int16_t | `windows` |
| 20~21 | AUX5 | int16_t | bit0 表示 `RC_low_power` |
| 22~23 | AUX6 | int16_t | `NRF_Channel` |
| 24 | SUM | uint8_t | 前 24 字节累加校验 |

## 8. 飞控到遥控器：05 POWER

当前工程总长度：9 字节。`LEN = 4`。

| 字节 | 字段 | 类型 | 当前工程含义 |
| --- | --- | --- | --- |
| 0 | HEAD0 | uint8_t | 固定 `0xAA` |
| 1 | HEAD1 | uint8_t | 固定 `0xAA` |
| 2 | FUNC | uint8_t | 固定 `0x05`，POWER |
| 3 | LEN | uint8_t | 固定 `4` |
| 4~5 | Votage | uint16_t | 飞控电压，单位 mV |
| 6~7 | Current | uint16_t | 电流，当前暂填 0 |
| 8 | SUM | uint8_t | 前 8 字节累加校验 |

Excel 中 POWER 写作 `uint16 Votage*100 / uint16 Current*100`。本项目为了直接匹配遥控器 `Battery_Fly` 字段，电压暂按 mV 解释；如果后续接入上位机兼容格式，需要重新统一单位。

## 9. ACK 轮询策略

飞控端每次重新装载 ACK payload 时按下面顺序轮询：

```text
01 STATUS -> 02 SENSER -> 03 RCDATA -> 05 POWER -> 01 STATUS ...
```

这样姿态仍能高频更新，同时低频带回传感器、飞控收到的控制数据和电压。

## 10. 解析与丢包处理

飞控端接收 RCDATA 时必须检查：

1. payload 长度至少为 25。
2. `packet[0] == 0xAA`，`packet[1] == 0xAF`。
3. `packet[2] == 0x03`。
4. 标准 `packet[3] == 20`；飞控端兼容接收 `packet[3] == 21`。
5. `packet[24] == checksum(packet[0..23])`。
6. 通道值解析后限幅到 `1000~2000`。

遥控器端解析 ACK payload 时必须检查：

1. payload 长度至少为 `LEN + 5`。
2. `packet[0] == 0xAA`，`packet[1] == 0xAA`。
3. `FUNC` 为当前支持的 `01/02/03/05`。
4. `packet[3] == LEN`。
5. `packet[LEN + 4] == checksum(packet[0..LEN+3])`。

## 11. 代码位置

| 工程 | 文件 | 职责 |
| --- | --- | --- |
| RemoterProject | `Remoter/NRF24L01.c` | 构建 25 字节 RCDATA，解析 `01/02/03/05` ACK payload |
| RemoterProject | `Remoter/remotetask.c` | 设置默认信道 72 |
| RemoterProject | `Remoter/show.c` | OLED 显示遥控状态、传感器状态、飞控姿态和电压 |
| FlyProject | `Hardware/NRF24L01.c` | 解析遥控器 RCDATA，轮询构建 `01/02/03/05` ACK payload |
| FlyProject | `Fly/FlyData.h` | 飞控端遥控通道、姿态和传感器快照 |
| FlyProject | `Fly/FlyAttitude.c` | 更新 MPU6050 数据快照 |
| FlyProject | `Fly/board.h` | 飞控端默认信道 72 |

## 12. 上板验证清单

- 遥控器窗口 0 显示 `S1~S5`，说明 NRF 链路建立。
- 飞控端串口出现 `NRF OK THR...`，说明飞控端收到有效 RCDATA。
- 遥控器窗口 3 姿态数据随飞机姿态实时变化，说明 `01 STATUS` 正常。
- 遥控器传感器页 MPU 状态变为正常，说明 `02 SENSER` 正常。
- 遥控器首页飞控电压字段能更新，说明 `05 POWER` 正常；当前飞控未接电压 ADC 时会显示 0。
- 如果出现 `NRF packet err` 或 `NRF ACK format error`，优先检查 SPI 频率、供电去耦、GND、地址、信道和两端包格式是否同时更新。

