/************************************************************************
* �ļ�����:NCSunDriver.h                                                 
* ��    ��:��  ��
* �������:2014-7-8
* ��������:NC����������ͷ�ļ�
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
	PDEVICE_OBJECT fdo;                     //ָ���豸�ӿ����������豸�����ڴ���ָ�����豸����
	PDEVICE_OBJECT NextStackDevice;         //�����²���������
	UNICODE_STRING InterfaceName;           //�������豸�ӿ����ɵ��豸��������������IoRegisterDeviceInterface�������

	const static ULONG ulHPILen=0x0010;	
	PULONG pHPIC;										
	PULONG pHPIA;
	PULONG pHPIDIncrease;						  
	PULONG pHPIDStatic;

	//PCI9052�ڲ��Ĵ���ӳ����ڴ���Դ
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
	HANDLE hUserDecodeEvent;                //�û��������߳̾��
	PKEVENT pDecodeEvent;                   //���ڶ������߳̽���ͬ�����ں��¼�
	HANDLE hSysThread;                      //�ں˿ռ������߳̾��
	KEVENT DecodeEvent;                     //���ڶ������߳̽���ͬ�����ں��¼�
#endif

#ifdef INTERRUPT_INCLUDE
	PKINTERRUPT InterruptObject;			// address of interrupt object
#endif

	signed long			UsageCount;				//The pending I/O Count
	BOOLEAN				bStopping;			
	KEVENT				StoppingEvent;			// Set when all pending I/O complete
	BOOLEAN				GotResource;			//
	//�¼�����ָ��
	PKEVENT				pWaitEvent;
	BOOLEAN				bSetWaitEvent;

	UCHAR uPnpStateFlag;                     //PNP״̬����־λ�����Թ���Σð�ļ��弴��״̬��0�������ڡ�1�����ڵ�ֹͣ��2������������������3���ȴ�ɾ����4���ȴ�ֹͣ��ת��ͼ����374
	PUCHAR pParaShm;                        //�������û��ռ���ں˿ռ��໥���ݲ����õĹ����ڴ�������ַ
	ULONG ulParaShmLen;                     //�������û��ռ���ں˿ռ��໥���ݲ����õĹ����ڴ������ȣ����û��������������Ϊ0x100����
	BOOLEAN bParaShmCreat;                  //���ڱ�־�����ڴ潨�����������������������Ϸ�ֹ��Ч��ַ�ķ���

	
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
//�������û�����ں˲㹲���ڴ����ĳ��ȣ����ֽ�Ϊ��λ
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
//�����̣߳��������ں˿ռ�
void DecodeThread(USHORT uFlag,PVOID pFileLoc);
