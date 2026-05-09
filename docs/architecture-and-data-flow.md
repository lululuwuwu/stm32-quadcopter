# 项目组织架构图与数据流图

本文档根据当前工程目录和源码生成，重点描述遥控器侧当前已经接入的模块、FreeRTOS 任务和数据流。

## 1. 项目组织架构图

```mermaid
flowchart TB
    Project["四轴飞行器遥控器工程<br/>Project.uvprojx"]

    Project --> User["User<br/>应用入口"]
    Project --> Remoter["Remoter<br/>遥控器业务层"]
    Project --> Hardware["Hardware<br/>板级外设驱动"]
    Project --> System["System<br/>通用系统组件"]
    Project --> FreeRTOS["FreeRTOS<br/>实时操作系统"]
    Project --> RTE["RTE / CMSIS / Device<br/>芯片启动与库配置"]

    User --> Main["main.c<br/>NVIC 分组 / 队列创建 / 启动调度"]

    Remoter --> Board["board.h<br/>统一包含与板级宏"]
    Remoter --> Task["remotetask.c/.h<br/>任务创建与任务函数"]
    Remoter --> Data["remoterData.c/.h<br/>全局遥控状态 RemoterData"]
    Remoter --> Stick["remoterstick.c/.h<br/>摇杆业务与校准状态"]
    Remoter --> KeyBiz["remoterkey.c/.h<br/>按键事件处理与反馈"]
    Remoter --> Show["show.c/.h<br/>OLED 页面刷新"]
    Remoter --> NRF["NRF24L01.c<br/>无线收发驱动雏形"]

    Hardware --> ADC["MyADC.c/.h<br/>ADC1 + DMA 摇杆/电池采样"]
    Hardware --> Flash["MyFLASH.c/.h<br/>校准参数 Flash 读写"]
    Hardware --> KeyDrv["key.c/.h<br/>GPIO 按键扫描/消抖/长短按"]
    Hardware --> Serial["Serial1.c/.h<br/>USART1 日志队列发送"]
    Hardware --> LED["led.c/.h<br/>LED 反馈"]
    Hardware --> Buzzer["buzzer.c/.h<br/>蜂鸣器反馈"]
    Hardware --> HSPI["MyHSPI.c/.h<br/>SPI1 访问外设"]
    Hardware --> OtherHW["其他外设驱动<br/>RTC / I2C / W25Q64 / MPU6050 等"]

    System --> OLED["OLED.c/.h<br/>OLED 底层显示"]
    System --> Delay["Delay.c/.h<br/>延时工具"]

    FreeRTOS --> Kernel["tasks / queue / timers / heap_4 / port"]

    Task --> FreeRTOS
    Stick --> ADC
    ADC --> Flash
    KeyBiz --> KeyDrv
    KeyBiz --> LED
    KeyBiz --> Buzzer
    Show --> OLED
    Serial --> FreeRTOS
    NRF --> HSPI
    NRF --> Serial
```

## 2. 运行时任务组织图

```mermaid
flowchart TB
    Boot["上电复位"]
    Main["main()<br/>NVIC_PriorityGroupConfig<br/>xQueueSerial1 = xQueueCreate(...)"]
    Scheduler["vTaskStartScheduler()"]
    CreateTask["RemoterCreateTask<br/>初始化 RemoterData<br/>创建子任务后删除自身"]

    Boot --> Main --> Scheduler
    Main --> CreateTask

    CreateTask --> SerialTask["vTaskSerial1SendLogCode<br/>优先级: max-1<br/>阻塞等待日志队列"]
    CreateTask --> OledTask["vTaskOLEDShow<br/>优先级: 1<br/>500ms 刷新 OLED"]
    CreateTask --> KeyTask["vTaskKeyProcess<br/>优先级: 2<br/>20ms 扫描按键"]
    CreateTask --> StickTask["vTaskStickScan<br/>优先级: 2<br/>20ms 更新摇杆/电池"]

    SerialTask --> USART1["USART1"]
    OledTask --> OLED["OLED"]
    KeyTask --> KeyGPIO["GPIOB 按键"]
    StickTask --> ADCDMA["ADC1 + DMA1_Channel1"]
```

## 3. 核心数据流图

```mermaid
flowchart LR
    subgraph Input["输入采集"]
        Sticks["摇杆电位器<br/>THR/YAW/PIT/ROL<br/>PA1/PA0/PA3/PA2"]
        Battery["遥控器电池分压<br/>PB0"]
        Keys["按键<br/>PB3~PB8"]
    end

    subgraph Drivers["驱动层"]
        ADC["MyADC<br/>ADC 扫描 + DMA 循环缓存"]
        Filter["滑动窗口滤波<br/>8 点平均"]
        Cal["校准映射<br/>Flash 读取/保存 min/center/max<br/>输出 1000~2000"]
        KeyScan["key.c<br/>消抖 + 长短按事件"]
        FeedbackHW["LED / Buzzer"]
    end

    subgraph RemoterLayer["遥控器业务层"]
        StickBiz["RemoterStick_Update<br/>更新 THR/YAW/PIT/ROL/Battery_RC/RC_low_power"]
        KeyBiz["RemoterKey_Update<br/>窗口切换 / 校准 / 微调"]
        State["RemoterData<br/>全局遥控状态"]
        ShowBiz["Show_Refresh<br/>按 windows 选择页面"]
        LogApi["Serial1_SendLog<br/>格式化日志入队"]
    end

    subgraph Output["输出与展示"]
        OLED["OLED 显示<br/>主界面/传感器/微调/姿态"]
        LogQueue["xQueueSerial1<br/>日志字符串队列"]
        USART["USART1<br/>115200 日志输出"]
        NRF["NRF24L01<br/>无线收发"]
    end

    Sticks --> ADC --> Filter --> Cal --> StickBiz --> State
    Battery --> ADC --> StickBiz --> State
    Keys --> KeyScan --> KeyBiz --> State
    KeyBiz --> FeedbackHW
    KeyBiz --> Cal

    State --> ShowBiz --> OLED
    LogApi --> LogQueue --> USART

    State -. "当前未在任务中调用" .-> NRF
    NRF -. "ACK/接收日志" .-> LogApi
```

## 4. RemoterData 字段流向

```mermaid
flowchart TB
    RemoterData["RemoterData 全局状态"]

    StickUpdate["RemoterStick_Update<br/>生产"]
    KeyUpdate["RemoterKey_Update<br/>生产/修改"]
    NRFDriver["NRF24L01_RX/TX<br/>预期生产/消费"]
    ShowRefresh["Show_Refresh<br/>消费"]

    StickUpdate -->|"THR/YAW/PIT/ROL<br/>Battery_RC<br/>RC_low_power"| RemoterData
    KeyUpdate -->|"windows<br/>OffSet_Pit/OffSet_Rol/OffSet_Yaw"| RemoterData
    RemoterData -->|"windows 选择页面<br/>电量/通道/摇杆/姿态/传感器状态"| ShowRefresh
    RemoterData -. "NRF_Channel / NRF_Connect / RSSI<br/>Battery_Fly / Fly_low_power<br/>X/Y/Z/H / fly_test_flag" .- NRFDriver
```

## 5. 当前状态备注

- 当前工程入口在 `User/main.c`，只直接创建 `RemoterCreateTask`，实际业务任务由 `Remoter/remotetask.c` 统一创建。
- `RemoterData` 是遥控器业务层的中心状态对象；摇杆任务和按键任务写入，OLED 任务读取。
- 串口日志走 `xQueueSerial1` 队列，`Serial1_SendLog()` 负责入队，`vTaskSerial1SendLogCode()` 负责从队列取出并通过 USART1 发送。
- `NRF24L01.c` 已加入 Keil 工程分组，但当前没有看到对应的 `NRF24L01.h` 文件；同时该文件使用的 `MyHSPI_NRF_CE()` 在当前 `MyHSPI.c/.h` 中尚未定义。它现在更像是待接入的无线驱动模块，还没有被 `remotetask.c` 调度。
- `RemoterData` 中飞行器回传字段，例如 `Battery_Fly`、`Fly_low_power`、`X/Y/Z/H`、`fly_test_flag`，目前主要被 OLED 消费；当前代码里尚未看到稳定的生产者，后续大概率应由 NRF 接收链路写入。
