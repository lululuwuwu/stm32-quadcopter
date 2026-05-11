#include "stm32f10x.h"                  // Device header




void MyFLASH_Init(void)
{
	
}


//读操作：内置闪存模块可以在通用地址空间直接寻址，任何32位数据的读操作都能访问闪存模块的内容并得到相应的数据。


uint8_t MyFLASH_ReadByte(uint32_t address)
{

	return *(__IO uint8_t *)address; //volatile 防止编译器优化 ,每次从地址读
} 

uint16_t MyFLASH_ReadHalfWord(uint32_t address)
{

	return *(__IO uint16_t *)address; //volatile 防止编译器优化 ,每次从地址读
}

uint32_t MyFLASH_ReadWord(uint32_t address)
{

	return *(__IO uint32_t *)address; //volatile 防止编译器优化 ,每次从地址读
}

void MyFLASH_ReadByteArray(uint32_t address,uint8_t *arrary,uint16_t length)
{
	for(uint16_t i=0;i<length;i++)
	{
		arrary[i]= MyFLASH_ReadByte(address+i);
	}
}


/*

页擦除 
闪存的任何一页都可以通过FPEC的页擦除功能擦除；擦除一页应遵守下述过程： 
z 检查FLASH_SR寄存器的BSY位，以确认没有其他正在进行的闪存操作； 
z 设置FLASH_CR寄存器的PER位为’1’； 
z 用FLASH_AR寄存器选择要擦除的页； 
z 设置FLASH_CR寄存器的STRT位为’1’； 
z 等待BSY位变为’0’； 
z 读出被擦除的页并做验证。

*/

/*
先解锁：
复位后，FPEC模块是被保护的，不能写入FLASH_CR寄存器；通过写入特定的序列到FLASH_KEYR寄存器可以打开FPEC模块，这个特定的序列是在FLASH_KEYR写入两个键值(KEY1和KEY2，见2.3.1节)；错误的操作序列都会在下次复位前锁死FPEC模块和FLASH_CR
寄存器。
*/

//返回值： FLASH_COMPLETE or FLASH_TIMEOUT

FLASH_Status MyFLASH_ErasePage(uint32_t Page_Address)
{
	FLASH_Unlock();//解除锁定


	//擦除
	FLASH_Status status = FLASH_ErasePage(Page_Address); //1.等待BSY 2.PER位为’1’,3.选择要擦除的页 4.STRT位为’1’5.BSY位变为’0’
	
	
	FLASH_Lock();//上锁
	
	
	return status;
}



//页编程
/*
这种模式下CPU以标准的写【半字】的方式烧写闪存，FLASH_CR寄存器的PG位必须置’1’。FPEC
先读出指定地址的内容并检查它【是否被擦除】，如未被擦除则不执行编程并在FLASH_SR寄存器的PGERR位提出警告(唯一的例外是当要烧写的数值是0x0000时，0x0000可被正确烧入且PGERR位不被置位)；如果指定的地址在FLASH_WRPR中设定为写保护，则不执行编程并在FLASH_SR寄存器的WRPRTERR位置’1’提出警告。FLASH_SR寄存器的EOP为’1’时表示编程结束。 
标准的闪存编程顺序如下： 
z 检查FLASH_SR寄存器的BSY位，以确认没有其他正在进行的编程操作； 
z 设置FLASH_CR寄存器的PG位为’1’； 
z 在指定的地址写入要编程的半字； 
z 等待BSY位变为’0’； 
z 读出写入的地址并验证数据。 
注意： 当FLASH_SR寄存器的【BSY位为’1’时，不能对任何寄存器执行写】操作。

*/


FLASH_Status MyFLASH_PageProgram(uint32_t Address, uint16_t Data)
{
	FLASH_Unlock();//解除锁定
	
	FLASH_Status status =  FLASH_ProgramHalfWord( Address,  Data);//1.BSY位判断 2.PG位为’1’ 3.写入要编程的半字 4. BSY位变为’0’ 
	
	FLASH_Lock();//上锁
	
	return status;	
}



//编写Flash按字节数组存取函数
FLASH_Status MyFLASH_ByteArrayProgram(uint32_t Address,uint8_t *arrary,uint16_t length)
{
	FLASH_Status status = FLASH_COMPLETE;
	FLASH_Unlock();//解除锁定
	
	for(uint16_t i=0;i<length;i+=2)
	{
		if(i==length-1)
		{
			status = FLASH_ProgramHalfWord(Address+i, arrary[i] | 0xFF00 );
		}else
		{
			status = FLASH_ProgramHalfWord(Address+i, arrary[i] | arrary[i+1]<<8 );		
		}
	}
	
	
	FLASH_Lock();//上锁
	return status;	
}







