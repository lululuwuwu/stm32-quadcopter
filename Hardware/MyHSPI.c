#include "MyHSPI.h"

// 使用硬件 SPI1 驱动 NRF24L01。
// CSN：PA4，由 MyHSPI_Start/Stop 控制，低电平选中模块。
// CE ：PA15，普通推挽输出，高电平进入收发工作状态。
// SCK：PA5，SPI1 复用推挽输出。
// MISO：PA6，SPI1 输入。
// MOSI：PA7，SPI1 复用推挽输出。
// IRQ：PA8，当前代码暂时轮询 STATUS，先配置为上拉输入备用。

static void MyHSPI_SS_W(uint8_t bitValue)
{
    GPIO_WriteBit(NRF24L01_CSN_GPIO, NRF24L01_CSN_PIN, (BitAction)bitValue);
}

void MyHSPI_NRF_CE(uint8_t bitValue)
{
    GPIO_WriteBit(NRF24L01_CE_GPIO, NRF24L01_CE_PIN, (BitAction)bitValue);
}

void MyHSPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    SPI_InitTypeDef SPI_InitTypeStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /*
     * PA15 默认是 JTAG 的 JTDI 引脚。关闭 JTAG 后 PA15 才能作为普通 GPIO 使用，
     * 同时保留 SWD 调试接口，方便后续下载和调试程序。
     */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // CSN 和 CE 都是普通推挽输出。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_CSN_PIN | NRF24L01_CE_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitTypeStructure);

    // SCK 和 MOSI 使用 SPI1 复用推挽输出。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_SCK_PIN | NRF24L01_MOSI_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitTypeStructure);

    // MISO 和 IRQ 使用上拉输入。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitTypeStructure.GPIO_Pin = NRF24L01_MISO_PIN | NRF24L01_IRQ_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitTypeStructure);

    // NRF24L01 支持 SPI 模式 0：CPOL=0，CPHA=0。
    SPI_InitTypeStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitTypeStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitTypeStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitTypeStructure.SPI_CRCPolynomial = 7;
    SPI_InitTypeStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitTypeStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitTypeStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitTypeStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitTypeStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI1, &SPI_InitTypeStructure);

    SPI_Cmd(SPI1, ENABLE);

    // 默认不选中 NRF24L01，且 CE 拉低，避免初始化期间误收发。
    MyHSPI_SS_W(1);
    MyHSPI_NRF_CE(0);
}

void MyHSPI_Start(void)
{
    MyHSPI_SS_W(0);
}

void MyHSPI_Stop(void)
{
    MyHSPI_SS_W(1);
}

uint8_t MyHSPI_SwapByte(uint8_t byteValue)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) != SET)
        ;
    SPI_I2S_SendData(SPI1, byteValue);

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != SET)
        ;

    return SPI_I2S_ReceiveData(SPI1);
}