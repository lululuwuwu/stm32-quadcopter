#include "stm32f10x.h"                  // Device header
#include "MyHSPI.h"


void W25Q64_Init(void)
{
	MyHSPI_Init();
	
}


void W25Q64_MID_DID(uint8_t *MID,uint16_t *DID)
{

	//测试一下时序
	MyHSPI_Start();
	
	MyHSPI_SwapByte(0x9F); //指令
	
	uint8_t mid = MyHSPI_SwapByte(0xFF); //交换MID
	uint8_t did_h = MyHSPI_SwapByte(0xFF);//交换ID高字节
	uint8_t did_l = MyHSPI_SwapByte(0xFF);//交换ID低字节
	
	MyHSPI_Stop();

	
	*MID = mid;
	*DID = (did_h<<8)+did_l;
	
}

//不忙的时候再发其他指令
/*
The Read Status Register instructions allow the 8-bit Status Registers to be read. 
The instruction is entered by driving /CS low and shifting the instruction code “05h” 
for Status Register-1 and “35h” for Status Register-2 into the DI pin on the rising edge of CLK. 
The status register bits are then shifted out on the DO pin at the falling edge of 
CLK with most significant bit (MSB) first as shown in figure 6.
The Status Register bits are shown in figure 3a and 3b and include 
the BUSY, WEL, BP2-BP0, TB, SEC, SRP0, SRP1 and QE bits (see description of the Status Register earlier in this datasheet). 



The Read Status Register instruction may be used at any time,
even while a Program, Erase or Write Status Register cycle is in progress.
This allows the BUSY status bit to be checked to determine when the cycle is 
complete and if the device can accept another instruction. 
The Status Register can be read continuously, as shown in Figure 6. 
The instruction is completed by driving /CS high.

任何时候都可以执行读取状态寄存器指令
可以连续读取状态寄存器
 
*/
void W25Q64_WaitBUSY(void)
{
	MyHSPI_Start();
	MyHSPI_SwapByte(0x05);
	while( (MyHSPI_SwapByte(0xFF) & 0x01) ==0x01);
	MyHSPI_Stop();
}

//等待写使能标志位置位
void W25Q64_WaitWriteEnalbe(void)
{
	MyHSPI_Start();
	MyHSPI_SwapByte(0x05);
	while( (MyHSPI_SwapByte(0xFF) & 0x02) !=0x02);
	MyHSPI_Stop();
}




//写使能
/*
The Write Enable instruction (Figure 4) sets the Write Enable Latch (WEL) bit in the Status Register to a 1.
The WEL bit must be set prior to every
Page Program, Sector Erase, Block Erase, Chip Erase and Write Status Register instruction. 
The Write Enable instruction is entered by driving /CS low, 
shifting the instruction code “06h” into the Data Input (DI) pin on the rising edge of CLK, and then driving /CS high.

在Page Program, Sector Erase, Block Erase, Chip Erase and Write Status Register之前都要进行写使能


*/
void W25Q64_Write_Enable(void)
{
	//1.不忙的时候再发送指令
	W25Q64_WaitBUSY();
	//2.发送写使能
	MyHSPI_Start();
	MyHSPI_SwapByte(0x06);
	MyHSPI_Stop();
	//3.等待标志位置位
	W25Q64_WaitWriteEnalbe();
	
}




//擦除
/*
The Sector Erase instruction sets all memory within a specified sector (4K-bytes) to the erased state of all 1s (FFh).
A Write Enable instruction must be executed before the device will accept the Sector Erase Instruction (Status Register bit WEL must equal 1). 
The instruction is initiated by driving the /CS pin low and shifting the instruction code “20h” 
followed a 24-bit sector address (A23-A0) (see Figure 2). The Sector Erase instruction sequence is shown in figure 17.

1.擦除Sector，4K ，变为FFh
2.先写使能 ,然后等待Status Register bit WEL must equal 1


The /CS pin must be driven high after the eighth bit of the last byte has been latched. 
If this is not done the Sector Erase instruction will not be executed. After /CS is driven high, 
the self-timed Sector Erase instruction will commence for a time duration of tSE (See AC Characteristics).
While the Sector Erase cycle is in progress, 
the Read Status Register instruction may still be accessed for checking the status of the BUSY bit.
The BUSY bit is a 1 during the Sector Erase cycle and becomes a 0 when the cycle is finished and the device 
is ready to accept other instructions again.
After the Sector Erase cycle has finished the Write Enable Latch (WEL) bit in the Status Register is cleared to 0. 
The Sector Erase instruction will not be executed if the addressed page is protected 
by the Block Protect (SEC, TB, BP2, BP1, and BP0) bits (see Status Register Memory Protection table).

1.擦除过程中可以读取状态寄存器，判断BUSY bit 。  --- BUSY忙的时候不响应其他指令
2.擦除完成后WEL变为0

*/
void W25Q64_Sector_Erase(uint32_t address)
{
	
	//1.写使能
	W25Q64_Write_Enable();
	//2.擦除扇区
	MyHSPI_Start();
	MyHSPI_SwapByte(0x20);
	MyHSPI_SwapByte((address&0x00FF0000)>>16);//address[23:16]
	MyHSPI_SwapByte((address&0x0000FF00)>>8);//address[15:8]
	MyHSPI_SwapByte((address&0x000000FF)>>0);//address[7:0]
	MyHSPI_Stop();
	
}




/*
页编程
The Page Program instruction allows from one byte to 256 bytes (a page) of data to be programmed at previously erased (FFh) memory locations. 
A Write Enable instruction must be executed before the device will accept the Page Program Instruction (Status Register bit WEL= 1). 
The instruction is initiated by driving the /CS pin low then shifting the instruction code “02h” followed by a 24-bit address (A23-A0) and at least one data byte, 
into the DI pin. The /CS pin must be held low for the entire length of the instruction while data is being sent to the device.
The Page Program instruction sequence is shown in figure 15.

1.写入之前要先擦除 ，擦除后的默认值是0xFF
2.执行写指令前必须执行Write Enable instruction ，状态寄存器的Status Register bit WEL= 1

If an entire 256 byte page is to be programmed,
the last address byte (the 8 least significant address bits) should be set to 0. 
If the last address byte is not zero, and the number of clocks exceed the remaining page length, 
the addressing will wrap to the beginning of the page.
In some cases, less than 256 bytes (a partial page) can be programmed without having any effect on other bytes within the same page. 
One condition to perform a partial page program is that the number of clocks can not exceed the remaining page length.
If more than 256 bytes are sent to the device the addressing will wrap to the beginning of the page and overwrite previously sent data. 

1.如果是写入的256字节，发送的起始地址最低位必须是00
2.如果写入的时候超过末尾会回到页首覆盖
3.如果超过256字节不执行

As with the write and erase instructions, 
the /CS pin must be driven high after the eighth bit of the last byte has been latched. 
If this is not done the Page Program instruction will not be executed.
After /CS is driven high,
the self-timed Page Program instruction will commence for a time duration of tpp (See AC Characteristics).
While the Page Program cycle is in progress,
the Read Status Register instruction may still be accessed for checking the status of the BUSY bit.
The BUSY bit is a 1 during the Page Program cycle and becomes a 0 when the cycle is finished and the device is ready to accept other instructions again.
After the Page Program cycle has finished the Write Enable Latch (WEL) bit in the Status Register is cleared to 0.
The Page Program instruction will not be executed if the addressed page is protected by the Block Protect (BP2, BP1, and BP0) bits. 

1.写入的字节发送完成后，CS片选拉高
2.写入字节过程中，可以Read Status Register 判断是否BUSY（忙就是1），BUSY变为0，就编程完成 ，然后才能接收其他指令
3.如果页编程结束，那么Write Enable Latch就会自动变为0 .   --- 也是每次编程前都要开WEL
4.如果设置了页保护就执行不了

*/

void W25Q64_Page_Program(uint32_t address,uint8_t *array,uint8_t length)
{
	//1.写使能
	W25Q64_Write_Enable();
	//2.写入数据
	MyHSPI_Start();
	MyHSPI_SwapByte(0x02);
	MyHSPI_SwapByte((address&0x00FF0000)>>16);//address[23:16]
	MyHSPI_SwapByte((address&0x0000FF00)>>8);//address[15:8]
	MyHSPI_SwapByte((address&0x000000FF)>>0);//address[7:0]
	//3.发送数据
	for(uint8_t i=0;i<length;i++)
	{
		MyHSPI_SwapByte(array[i]);
	}
	
	MyHSPI_Stop();
}


//读
/*
The Read Data instruction sequence is shown in figure 8. 
If a Read Data instruction is issued while an Erase,
Program or Write cycle is in process (BUSY=1) the instruction is ignored 
and will not have any effects on the current cycle. The Read Data instruction allows clock rates from D.C.
to a maximum of fR (see AC Electrical Characteristics).

忙的时候不能读

 

*/
void W25Q64_Read_Data(uint32_t address,uint8_t *array,uint8_t length)
{
	//1.不忙的时候才能读
	W25Q64_WaitBUSY();
	//2.读数据
	MyHSPI_Start();
	MyHSPI_SwapByte(0x03);
	MyHSPI_SwapByte((address&0x00FF0000)>>16);//address[23:16]
	MyHSPI_SwapByte((address&0x0000FF00)>>8);//address[15:8]
	MyHSPI_SwapByte((address&0x000000FF)>>0);//address[7:0]
	//3.发送数据
	for(uint8_t i=0;i<length;i++)
	{
		array[i] = MyHSPI_SwapByte(0xFF);
	}
	
	MyHSPI_Stop();
	
}

