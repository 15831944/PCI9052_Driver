/************************************************************************
* 文件名称:NCSunDriver.h                                                 
* 作    者:孙  冬
* 完成日期:2014-7-8
* 功能描述:NC板驱动程序头文件
*************************************************************************/


#ifdef __cplusplus
extern "C"
{
#endif
#include <wdm.h>
#ifdef __cplusplus
}
#endif 

#define INTERRUPT_INCLUDE
#define PCI9052_MEMINCLUDE
//#define PCI9052_IOINCLUDE
#define  DECODE_INCLUDE

#include <initguid.h> 

#define KdPrint(_x_) DbgPrint _x_ 
#define DSP_MEMORY_BASE_ADR 0X00000000
#define DSP_DECODE_BASE_ADR 0xA0000000
#define DSP_PARAMETER_BASE_ADR 0xa0010000
#define DSP_TAPEPARAMETER_BASE_ADR 0xa00104c8
#define NC_FPGA_BASE_ADR 0X80000000

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT fdo;                     //指明设备接口所关联的设备对象，在此是指功能设备对象
	PDEVICE_OBJECT NextStackDevice;         //保存下层驱动对象
	UNICODE_STRING InterfaceName;           //保存由设备接口生成的设备符号链接名，由IoRegisterDeviceInterface函数完成

	const static ULONG ulHPILen=0x0010;	
	PULONG pHPIC;										
	PULONG pHPIA;
	PULONG pHPIDIncrease;						  
	PULONG pHPIDStatic;

	//PCI9052内部寄存器映射的内存资源
#ifdef PCI9052_MEMINCLUDE
	const static ULONG ulPCI9052MemLen = 0x0080;
	PULONG pPCI9052Mem;
#endif

#ifdef PCI9052_IOINCLUDE
	const static ULONG ulPCI9052IOLen = 0x0080;
	PULONG pPCI9052IO;
	BOOLEAN mappedport;
#endif

#ifdef DECODE_INCLUDE
	HANDLE hUserDecodeEvent;                //用户层译码线程句柄
	PKEVENT pDecodeEvent;                   //用于对译码线程进行同步的内核事件
	HANDLE hSysThread;                      //内核空间译码线程句柄
	KEVENT DecodeEvent;                     //用于对译码线程进行同步的内核事件
#endif

#ifdef INTERRUPT_INCLUDE
	PKINTERRUPT InterruptObject;			// address of interrupt object
#endif

	signed long			UsageCount;				//The pending I/O Count
	BOOLEAN				bStopping;			
	KEVENT				StoppingEvent;			// Set when all pending I/O complete
	BOOLEAN				GotResource;			//
	//事件对象指针
	PKEVENT				pWaitEvent;
	BOOLEAN				bSetWaitEvent;

	UCHAR uPnpStateFlag;                     //PNP状态机标志位，用以管理ＮＣ板的即插即用状态：0＝不存在　1＝存在但停止　2＝存在且正常工作　3＝等待删除　4＝等待停止　转换图见Ｐ374
	PUCHAR pParaShm;                        //用于在用户空间和内核空间相互传递参数用的共享内存区基地址
	ULONG ulParaShmLen;                     //用于在用户空间和内核空间相互传递参数用的共享内存区长度，由用户层程序传来，建议为0x100长度
	BOOLEAN bParaShmCreat;                  //用于标志共享内存建立与否，这样可以在驱动层次上防止无效地址的访问

	
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _NC_FPGA_stru{
	ULONG adr;
	ULONG val;
}NC_FPGA_stru, *LP_NC_FPGA_stru; 

typedef struct _NC_MEM_stru{
	ULONG adr;
	ULONG val[16];
}NC_MEM_stru, *LP_NC_MEM_stru; 

#define PAGEDCODE code_seg("PAGE")
#define LOCKEDCODE code_seg()
#define INITCODE code_seg("INIT")

#define PAGEDDATA data_seg("PAGE")
#define LOCKEDDATA data_seg()
#define INITDATA data_seg("INIT")

#define arraysize(p) (sizeof(p)/sizeof((p)[0]))
//定义在用户层和内核层共享内存区的长度，以字节为单位
#define PARAMETER_SHM_LENGTH 0x100

NTSTATUS NCDriverAddDevice(IN PDRIVER_OBJECT DriverObject,
                           IN PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS NCDriverPnp(IN PDEVICE_OBJECT fdo,
                        IN PIRP Irp);
NTSTATUS NCDriverDispatchRoutine(IN PDEVICE_OBJECT fdo,
								 IN PIRP Irp);
NTSTATUS NCDriverDeviceControl(PDEVICE_OBJECT fdo, PIRP Irp);
NTSTATUS NCDriverRemoveDevice(PDEVICE_EXTENSION pdx, PIRP Irp);
void NCDriverUnload(IN PDRIVER_OBJECT DriverObject);


extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
                                IN PUNICODE_STRING RegistryPath);
//译码线程，运行在内核空间
void DecodeThread(USHORT uFlag,PVOID pFileLoc);
