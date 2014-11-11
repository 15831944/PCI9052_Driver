/************************************************************************
* 文件名称:NCSunDriver.cpp                                                 
* 作    者:孙  冬
* 完成日期:2014-9-28
* 功能描述:NC_PCI_SUN板驱动程序源文件
*************************************************************************/
#include "NCSunDriver.h"
#include "NCSunDriverGuid.h"
#include "NCSunDriverIoctls.h"

/************************************************************************
* 函数名称:DriverEntry
* 功能描述:NC_PCI_SUN板驱动程序的入口函数，初始化驱动程序，定位和申请硬件资源，创建内核对象
* 参数列表:
          pDriverObject:从I/O管理器中传进来的驱动对象
          pRegistryPath:驱动程序在注册表的中的路径
* 返回值 :返回初始化驱动状态
*************************************************************************/
#pragma INITCODE 
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,
								IN PUNICODE_STRING pRegistryPath)
{
	KdPrint(("Enter DriverEntry\n"));

	pDriverObject->DriverExtension->AddDevice = NCDriverAddDevice;
	pDriverObject->MajorFunction[IRP_MJ_PNP] = NCDriverPnp;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NCDriverDeviceControl;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = NCDriverDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = NCDriverDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = NCDriverDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = NCDriverDispatchRoutine;
	pDriverObject->DriverUnload = NCDriverUnload;

	KdPrint(("Leave DriverEntry\n"));

	return STATUS_SUCCESS;
}

/************************************************************************
* 函数名称:NCDriverAddDevice
* 功能描述:由PNP管理器调用，负责新设备的添加，此后PNP管理器会调用PNP_MN_START_DEVICE的响应函数
* 参数列表:
		  DriverObject:从I/O管理器中传进来的驱动对象
          PhysicalDeviceObject:从I/O管理器中传进来的物理设备对象
* 返回 值:返回添加新设备状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverAddDevice(IN PDRIVER_OBJECT DriverObject,
                           IN PDEVICE_OBJECT PhysicalDeviceObject)
{ 
	PAGED_CODE();
	KdPrint(("Enter NCDriverAddDevice\n"));

	NTSTATUS status;
	PDEVICE_OBJECT fdo;
	status = IoCreateDevice(
		DriverObject,
		sizeof(DEVICE_EXTENSION),         //声明DeviceExtension域的大小
		NULL,//没有指定设备名
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&fdo);
	if( !NT_SUCCESS(status))
		return status;
	//获得功能设备对象fdo的DeviceExtension域指针（地址）,用以初始化其中的全局变量
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION)fdo->DeviceExtension;
	//保存设备扩展和设备对象的链接关系
	pdx->fdo = fdo;
	//在设备对象扩展保存设备堆栈中下层设备的设备对象指针
	pdx->NextStackDevice = IoAttachDeviceToDeviceStack(fdo, PhysicalDeviceObject);
	//创建设备接口,用以在用户层或其他设备驱动程序打开本驱动,设备接口名MotionCtrlDevice由 NCGuid.h 定义
	//本函数由设备接口生成的设备符号链接名，并存放在设备扩展中
	status = IoRegisterDeviceInterface(PhysicalDeviceObject, &NCCtrlDevice, NULL, &pdx->InterfaceName);
	if( !NT_SUCCESS(status))
	{
		IoDeleteDevice(fdo);
		return status;
	}
	//	KdPrint(("%wZ\n",&pdx->InterfaceName));
	if(!NT_SUCCESS(status))
	{
		return status;
	}
	//初始化设备对象内置的 DPC 对象,启动此对象的代码见 OnRequest() 注释
	//InitializeDpcRequest(fdo,DpcForIsr);

	//使用缓冲IO方式
	fdo->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;
	fdo->Flags &= ~DO_DEVICE_INITIALIZING;

	//表明设备已经处于存在但停止状态，见NCDriver.h文件中DEVICE_EXTENSION结构体的定义,此时不会响应非PNP的IRP指令
	pdx->uPnpStateFlag=1;
	//表明共享内存尚未建立，此时不允许对共享内存区进行读写操作
	pdx->bParaShmCreat=FALSE;
	

	KdPrint(("Leave NCDriverAddDevice\n"));
	return STATUS_SUCCESS;
}

/************************************************************************
* 函数名称:DefaultPnpHandler
* 功能描述:对PNP IRP进行缺省处理,在 NCDriverPnp() 派遣函数中被使用
* 参数列表:
		  pdx:设备对象的扩展
          Irp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS DefaultPnpHandler(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//本层驱动不处理的PNP玛完全交由下层PDO，即PCI总线驱动处理
	//	KdPrint(("Enter DefaultPnpHandler\n"));
	IoSkipCurrentIrpStackLocation(Irp);
	//	KdPrint(("Leave DefaultPnpHandler\n"));
	return IoCallDriver(pdx->NextStackDevice, Irp);
}


/************************************************************************
* 函数名称:OnRequestComplete
* 功能描述:当派遣函数 NCDriverPnp() 把IRP_MN_START_DEVICE传递给PCI驱动处理完成时，由PCI驱动引发的IRP完成函数
* 参数列表:
		  DeviceObject:设备对象
          Irp:从IO请求包
		  pEvent:下层PCI驱动传来的同步事件对象
* 返回 值:返回状态
*************************************************************************/
#pragma LOCKEDCODE
NTSTATUS OnRequestComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PKEVENT pEvent)
{
	//在完成例程中激发等待事件，使本层功能驱动对IRP_MN_START_DEVICE命令的派遣函数得以继续执行，以获得下层PCI总线
	//驱动传递来的各项 PCI 资源参数，本层驱动对IRP_MN_START_DEVICE命令的IRP响应为同步响应方式。
	KeSetEvent(pEvent, 0, FALSE);
	//标志本IRP还需要再次被完成
	return STATUS_MORE_PROCESSING_REQUIRED;
}

/************************************************************************
* 函数名称:ForwardAndWait
* 功能描述:把本层IRP（IRP_MN_START_DEVICE）先交由PCI总线驱动处理，等待PCI驱动完成返回后，获得返回状态
* 参数列表:
		  pdx:设备对象的扩展
          Irp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS ForwardAndWait(PDEVICE_EXTENSION pdx, PIRP Irp)
{							// ForwardAndWait
	PAGED_CODE();
	
	KEVENT event;
	//初始化事件
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	//将本层堆栈拷贝到下一层堆栈
	IoCopyCurrentIrpStackLocationToNext(Irp);
	//设置完成例程
	IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) OnRequestComplete,
		(PVOID) &event, TRUE, TRUE, TRUE);
	//调用底层驱动，即PCI总线驱动PDO
	IoCallDriver(pdx->NextStackDevice, Irp);
	//等待PDO完成，IRP完成后由其完成函数OnRequestComplete激发同步事件event
	KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
	//Irp->IoStatus.Status中保存的是下层PCI驱动完成本层IRP_MN_START_DEVICE命令IRP的返回值，由于完成函数返回的是
	//STATUS_MORE_PROCESSING_REQUIRED状态码，所以本层驱动才能在完成函数OnRequestComplete被调用后重新获得对此IRP
	//的控制权
	return Irp->IoStatus.Status;
}


/************************************************************************
* 函数名称:HandleRemoveDevice
* 功能描述:对IRP_MN_REMOVE_DEVICE IRP进行处理,用以停止设备，使设备由“1=存在但停止” 状态变为“0=不存在”状态,与AddDevice函数对应
* 参数列表:
		  pdx:设备对象的扩展
          Irp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverRemoveDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//KdPrint(("Enter IODriverRemoveDevice\n"));
	Irp->IoStatus.Status = STATUS_SUCCESS;
	//调用底层PCI驱动完成总线设备删除的一般性操作
	NTSTATUS status = DefaultPnpHandler(pdx, Irp);
	//	KdPrint(("Leave HandleRemoveDevice\n"));

	//关闭设备接口，此时用户和其他驱动将不能再使用此驱动
	IoSetDeviceInterfaceState(&pdx->InterfaceName, FALSE);
	//释放存放设备接口符号链接名（本链接存放在动态申请的内存中）
	RtlFreeUnicodeString(&pdx->InterfaceName);
	
	//表明共享内存尚未建立，此时不允许对共享内存区进行读写操作
	pdx->bParaShmCreat=FALSE;

    //调用IoDetachDevice()把fdo从设备栈中脱开：
    if (pdx->NextStackDevice)
        IoDetachDevice(pdx->NextStackDevice);
    //删除fdo：
    IoDeleteDevice(pdx->fdo);
	pdx->uPnpStateFlag=0;

	
	return status;
}


/************************************************************************
* 函数名称:DpcForIsr
* 功能描述:PCI中断响应函数的派遣函数，将来如果PCI中断响应函数的代码量有大变动，可以使用此例程完成这些中断功能
* 参数列表:
		  Dpc:功能设备对象fdo内嵌的DPC对象
		  fdo:当前功能设备对象
		  Irp:当前的IRP对象
		  Context:由IoRequestDpc()函数传递来的参数
* 返回 值:返回状态

#pragma LOCKEDCODE
void DpcForIsr(PKDPC Dpc,PDEVICE_OBJECT fdo,PIRP Irp,PVOID Context)
{
	//DPC例程
}
*************************************************************************/

/************************************************************************
* 函数名称:OnInterrupt
* 功能描述:PCI中断响应函数，用以通过激发内核模式的同步事件来启动译码线程，电脑运行卡是因为中断吗？
* 参数列表:
		  InterruptObject:中断对象
		  pdx:设备扩展
* 返回 值:返回状态
*************************************************************************/
#pragma LOCKEDCODE
BOOLEAN OnInterrupt(PKINTERRUPT InterruptObject, PDEVICE_EXTENSION pdx)
{
	//	KdPrint(("Entering PCI Interrupt!!!\n"));
	//检查中断源
	//激活同步事件以启动译码线程
	//KeSetEvent();

	//如果今后中断处理程序变得复杂，则可以通过使能下面代码调用与fdo相关联的DPC例程，须在NCDriverAddDevice中启用
	//	IoRequestDpc(pdx->fdo, NULL, pdx);

	return TRUE;
}


/************************************************************************
* 函数名称:InitNCBoard
* 功能描述:PCI初始化函数，本函数在IRP_MN_START_DEVICE命令的派遣函数中被调用，用于下层PCI驱动调用返回后，由本层驱动
*                        完成PCI资源的初始化工作
* 参数列表:
		  pdx:设备扩展
		  list：由下层PCI驱动得到的初始PCI资源列表
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS InitNCBoard(IN PDEVICE_EXTENSION pdx,IN PCM_PARTIAL_RESOURCE_LIST list)
{
	PAGED_CODE();

	PDEVICE_OBJECT fdo = pdx->fdo;
	//暂存从PCI驱动处获得的中断信息所使用的变量
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KAFFINITY affinity;
	BOOLEAN irqshare;
	BOOLEAN gotinterrupt = FALSE;

	//PHYSICAL_ADDRESS portbase;
	BOOLEAN gotport = FALSE;

	//保存资源描述表的首地址
	//pdx->pIoBase = 0;
	pdx->pHPIC = 0;
	pdx->pHPIA = 0;
	pdx->pHPIDIncrease = 0;
	pdx->pHPIDStatic = 0;
	
	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = &list->PartialDescriptors[0];
	ULONG uNumRes = list->Count;
	
	for (ULONG i = 0; i < uNumRes; i++,resource++)
	{
		switch (resource->Type)
		{
		case CmResourceTypeMemory:
			//把从PCI获得的物理内存映射到非分页的虚拟内存
			if (resource->u.Memory.Length == pdx->ulHPILen) 
			{
				KdPrint((" HPI BASE ADDRESS X%X\n",resource->u.Memory.Start));



				pdx->pHPIC = (PULONG)MmMapIoSpace(resource->u.Memory.Start,
					resource->u.Memory.Length,
					MmNonCached);

				pdx->pHPIA = pdx->pHPIC + 4;
				pdx->pHPIDIncrease = pdx->pHPIC + 8;
				pdx->pHPIDStatic = pdx->pHPIC + 12;

				//初始化HPI控制寄存器HWOB
				WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0X00010001);
			}
			break;

			////case CmResourceTypePort:
			////	//I/O端口资源
			////	//KdPrint(("I/O resource type %d\n", resource->Type));
			////	if(resource->u.Port.Length==pdx->ulHPICLen)
			////	{
			////		//pdx->ulHPICLen = resource->u.Port.Length;
   ////                 portbase = resource->u.Port.Start;
			////		pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			////		gotport = TRUE;
			////		KdPrint(("get into 0x04 I/O resource type %d\n", resource->Type));

			////		if(pdx->mappedport)
			////		{
			////			pdx->pHPIC = (PULONG)MmMapIoSpace(portbase,pdx->ulHPICLen,MmNonCached);
			////			KdPrint((" get into pHPIC iomapped\n" ));
			////			//KdPrint((" IOBASE ADDRESS1 %I64X,length1 %X\n",*(pdx->pIoBase),pdx->ulIoLen ));

			////			if(!pdx->mappedport)
			////			{
			////				//KdPrint((" Unable to map port range %I64X,length %X\n",portbase,pdx->ulIoLen ));
			////				return STATUS_INSUFFICIENT_RESOURCES;
			////			}
			////		}
			////		else
			////		{
			////			pdx->pHPIC = (PULONG) portbase.QuadPart;
			////			KdPrint((" get into no-iomapped\n" ));
			////			KdPrint((" IOBASE ADDRESS %X,length %X\n",pdx->pHPIC,pdx->ulHPICLen ));
			////		}
			////	}
			////	else if (resource->u.Port.Length==pdx->ulHPIALen)
			////	{
			////		//pdx->ulHPIALen = resource->u.Port.Length;
			////		portbase = resource->u.Port.Start;
			////		pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			////		gotport = TRUE;
			////		KdPrint(("get into 0x08 I/O resource type %d\n", resource->Type));

			////		if(pdx->mappedport)
			////		{
			////			pdx->pHPIA = (PULONG)MmMapIoSpace(portbase,pdx->ulHPIALen,MmNonCached);
			////			KdPrint((" get into pHPIA iomapped\n" ));
			////			//KdPrint((" IOBASE ADDRESS1 %I64X,length1 %X\n",*(pdx->pIoBase),pdx->ulIoLen ));

			////			if(!pdx->mappedport)
			////			{
			////				//KdPrint((" Unable to map port range %I64X,length %X\n",portbase,pdx->ulIoLen ));
			////				return STATUS_INSUFFICIENT_RESOURCES;
			////			}
			////		}
			////		else
			////		{
			////			pdx->pHPIC = (PULONG) portbase.QuadPart;
			////			KdPrint((" get into no-iomapped\n" ));
			////			
			////			KdPrint((" IOBASE ADDRESS %X,length %X\n",pdx->pHPIA,pdx->ulHPIALen ));
			////		}
			////	}
			////	else if (resource->u.Port.Length==pdx->ulHPIDILen)
			////	{
			////		//pdx->ulHPIDILen = resource->u.Port.Length;
			////		portbase = resource->u.Port.Start;
			////		pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			////		gotport = TRUE;
			////		KdPrint(("get into 0x0C I/O resource type %d\n", resource->Type));

			////		if(pdx->mappedport)
			////		{
			////			pdx->pHPIDIncrease = (PULONG)MmMapIoSpace(portbase,pdx->ulHPIDILen,MmNonCached);
			////			KdPrint((" get into pHPIDIncrease iomapped\n" ));
			////			//KdPrint((" IOBASE ADDRESS1 %I64X,length1 %X\n",*(pdx->pIoBase),pdx->ulIoLen ));

			////			if(!pdx->mappedport)
			////			{
			////				//KdPrint((" Unable to map port range %I64X,length %X\n",portbase,pdx->ulIoLen ));
			////				return STATUS_INSUFFICIENT_RESOURCES;
			////			}
			////		}
			////		else
			////		{
			////			pdx->pHPIDIncrease = (PULONG) portbase.QuadPart;
			////			KdPrint((" get into no-iomapped\n" ));
			////			KdPrint((" IOBASE ADDRESS %X,length %X\n",pdx->pHPIDIncrease,pdx->ulHPIDILen ));
			////		}
			////	}
			////	else if (resource->u.Port.Length==pdx->ulHPIDSLen)
			////	{
			////		//pdx->ulHPIDSLen = resource->u.Port.Length;
			////		portbase = resource->u.Port.Start;
			////		pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			////		gotport = TRUE;
			////		KdPrint(("get into 0x0C I/O resource type %d\n", resource->Type));

			////		if(pdx->mappedport)
			////		{
			////			pdx->pHPIDStatic = (PULONG)MmMapIoSpace(portbase,pdx->ulHPIDSLen,MmNonCached);
			////			KdPrint((" get into pHPIDStatic iomapped\n" ));
			////			//KdPrint((" IOBASE ADDRESS1 %I64X,length1 %X\n",*(pdx->pIoBase),pdx->ulIoLen ));

			////			if(!pdx->mappedport)
			////			{
			////				//KdPrint((" Unable to map port range %I64X,length %X\n",portbase,pdx->ulIoLen ));
			////				return STATUS_INSUFFICIENT_RESOURCES;
			////			}
			////		}
			////		else
			////		{
			////			pdx->pHPIDStatic = (PULONG) portbase.QuadPart;
			////			KdPrint((" get into no-iomapped\n" ));
			////			KdPrint((" IOBASE ADDRESS %X,length %X\n",pdx->pHPIDStatic,pdx->ulHPIDSLen ));
			////		}
			////	}
			////	break;

			case CmResourceTypeInterrupt:
				//把从下层PCI总线处获得的中断信息保存在临时变量中

				irql = (KIRQL) resource->u.Interrupt.Level;
				vector = resource->u.Interrupt.Vector;
				affinity = resource->u.Interrupt.Affinity;
				mode = (resource->Flags == CM_RESOURCE_INTERRUPT_LATCHED) ? Latched : LevelSensitive;
				irqshare = (resource->ShareDisposition == CmResourceShareShared);
				gotinterrupt = TRUE;
				break;

			default:
				KdPrint(("Unexpected I/O resource type %d\n", resource->Type));
				break;
		} //switch (resource->Type)
	} //for (ULONG i = 0; i < uNumRes; i++,resource++)

	if ((pdx->pHPIC==0)||(gotinterrupt==FALSE))
	{
		KdPrint((" Didn't get expected Memory resources\n"));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	//连接中断并使能
	NTSTATUS status = IoConnectInterrupt(&pdx->InterruptObject, (PKSERVICE_ROUTINE) OnInterrupt,(PVOID) pdx, NULL, vector, irql, irql, LevelSensitive, TRUE, affinity, FALSE);

	if (!NT_SUCCESS(status))
	{

		KdPrint(("IoConnectInterrupt failed - %X\n", status));
		return status;
	}

	
	return STATUS_SUCCESS;	
}


/************************************************************************
* 函数名称:NCDriverStartDevice
* 功能描述:PNP类子IRP IRP_MN_START_DEVICE 的响应函数，用以启动设备，使设备处于2=存在且正常工作状态
* 参数列表:
		  pdx:设备扩展
		  Irp:当前IRP
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverStartDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();

	KdPrint(("Enter HandleStartDevice\n"));

	//把IRP转发给下层PCI驱动PDO并等待返回，返回值是PDO完成IRP的状态，且返回时本层FDO已重新获得了IRP的控制权
	NTSTATUS status = ForwardAndWait(pdx,Irp);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	//得到当前 IO 堆栈
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	//从当前IO堆栈得到下层PDO传来的翻译后的PCI资源信息
	PCM_PARTIAL_RESOURCE_LIST translated;
	if (stack->Parameters.StartDevice.AllocatedResourcesTranslated)
		translated = &stack->Parameters.StartDevice.AllocatedResourcesTranslated->List[0].PartialResourceList;
	else
	{
		translated = NULL;
		Irp->IoStatus.Status=STATUS_UNSUCCESSFUL;
		return STATUS_UNSUCCESSFUL;
	}
	//映射IO板所占用的PCI存储器空间
	//KdPrint(("Init the PCI card!\n"));
	status=InitNCBoard(pdx,translated);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	//启用设备接口，至此用户程序或其它驱动才可以使用本驱动
	status=IoSetDeviceInterfaceState(&pdx->InterfaceName, TRUE);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	//完成IRP,此时设备处于正常工作状态=2
	pdx->uPnpStateFlag=2;                               
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	KdPrint(("Leave HandleStartDevice\n"));
	return status;
}

/************************************************************************
* 函数名称:NCDriverStopDevice
* 功能描述:PNP类子IRP IRP_MN_STOP_DEVICE 的响应函数，用以停止设备，使设备处于1=存在但停止状态，此时需要启动设备才能使之正常工作
* 参数列表:
		  pdx:设备扩展
		  Irp:当前IRP
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverStopDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//KdPrint(("Enter IODriverStopDevice\n"));
	//关闭设备接口，此时用户和其他驱动将不能再使用此驱动
	IoSetDeviceInterfaceState(&pdx->InterfaceName, FALSE);
	//释放存放设备接口符号链接名（本链接存放在动态申请的内存中）
	RtlFreeUnicodeString(&pdx->InterfaceName);
	//断开中断连接，不在响应PCI中断                        
	IoDisconnectInterrupt(pdx->InterruptObject);

	pdx->uPnpStateFlag=1;
	//表明共享内存尚未建立，此时不允许对共享内存区进行读写操作
	pdx->bParaShmCreat=FALSE;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	//先调用PNP的通用处理函数，把IRP交由下层PCI驱动处理
	NTSTATUS status = DefaultPnpHandler(pdx, Irp);
	return status;
}

/************************************************************************
* 函数名称:HelloWDMPnp
* 功能描述:即插即用IRP的派遣函数，用以对IRP_MJ_PNP类IRP的子类IRP进行处理
* 参数列表:
      fdo:功能设备对象
      Irp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverPnp(IN PDEVICE_OBJECT fdo,
                        IN PIRP Irp)
{
	PAGED_CODE();

	//KdPrint(("Enter HelloWDMPnp\n"));
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	static NTSTATUS (*fcntab[])(PDEVICE_EXTENSION pdx, PIRP Irp) = 
	{
		NCDriverStartDevice,	// IRP_MN_START_DEVICE=0x0,可以通过点击右键->转到定义，来查看这些子功能的值
		DefaultPnpHandler,		// IRP_MN_QUERY_REMOVE_DEVICE
		NCDriverRemoveDevice,	// IRP_MN_REMOVE_DEVICE
		DefaultPnpHandler,		// IRP_MN_CANCEL_REMOVE_DEVICE
		NCDriverStopDevice,		// IRP_MN_STOP_DEVICE
		DefaultPnpHandler,		// IRP_MN_QUERY_STOP_DEVICE
		DefaultPnpHandler,		// IRP_MN_CANCEL_STOP_DEVICE
		DefaultPnpHandler,		// IRP_MN_QUERY_DEVICE_RELATIONS
		DefaultPnpHandler,		// IRP_MN_QUERY_INTERFACE
		DefaultPnpHandler,		// IRP_MN_QUERY_CAPABILITIES
		DefaultPnpHandler,		// IRP_MN_QUERY_RESOURCES
		DefaultPnpHandler,		// IRP_MN_QUERY_RESOURCE_REQUIREMENTS
		DefaultPnpHandler,		// IRP_MN_QUERY_DEVICE_TEXT
		DefaultPnpHandler,		// IRP_MN_FILTER_RESOURCE_REQUIREMENTS
		DefaultPnpHandler,		// 
		DefaultPnpHandler,		// IRP_MN_READ_CONFIG
		DefaultPnpHandler,		// IRP_MN_WRITE_CONFIG
		DefaultPnpHandler,		// IRP_MN_EJECT
		DefaultPnpHandler,		// IRP_MN_SET_LOCK
		DefaultPnpHandler,		// IRP_MN_QUERY_ID
		DefaultPnpHandler,		// IRP_MN_QUERY_PNP_DEVICE_STATE
		DefaultPnpHandler,		// IRP_MN_QUERY_BUS_INFORMATION
		DefaultPnpHandler,		// IRP_MN_DEVICE_USAGE_NOTIFICATION
		DefaultPnpHandler,		// IRP_MN_SURPRISE_REMOVAL
	};

	ULONG fcn = stack->MinorFunction;
	if (fcn >= arraysize(fcntab))
	{						// 未知的子功能代码
		status = DefaultPnpHandler(pdx, Irp); // some function we don't know about
		return status;
	}						

	//此宏DBG=1是在编译阶段定义的，/D DBG=1(项目属性 -> C/C++ -> 命令行)
	/*
	#if DBG
		static char* fcnname[] = 
		{
			"IRP_MN_START_DEVICE",
			"IRP_MN_QUERY_REMOVE_DEVICE",
			"IRP_MN_REMOVE_DEVICE",
			"IRP_MN_CANCEL_REMOVE_DEVICE",
			"IRP_MN_STOP_DEVICE",
			"IRP_MN_QUERY_STOP_DEVICE",
			"IRP_MN_CANCEL_STOP_DEVICE",
			"IRP_MN_QUERY_DEVICE_RELATIONS",
			"IRP_MN_QUERY_INTERFACE",
			"IRP_MN_QUERY_CAPABILITIES",
			"IRP_MN_QUERY_RESOURCES",
			"IRP_MN_QUERY_RESOURCE_REQUIREMENTS",
			"IRP_MN_QUERY_DEVICE_TEXT",
			"IRP_MN_FILTER_RESOURCE_REQUIREMENTS",
			"",
			"IRP_MN_READ_CONFIG",
			"IRP_MN_WRITE_CONFIG",
			"IRP_MN_EJECT",
			"IRP_MN_SET_LOCK",
			"IRP_MN_QUERY_ID",
			"IRP_MN_QUERY_PNP_DEVICE_STATE",
			"IRP_MN_QUERY_BUS_INFORMATION",
			"IRP_MN_DEVICE_USAGE_NOTIFICATION",
			"IRP_MN_SURPRISE_REMOVAL",
		};
	//KdPrint(("PNP Request (%s)\n", fcnname[fcn]));
	#endif // DBG
	*/

	//通过函数表调用相应响应函数
	status = (*fcntab[fcn])(pdx, Irp);

	//KdPrint(("Leave HelloWDMPnp\n"));
	return status;
}

/************************************************************************
* 函数名称:HelloWDMDispatchRoutine
* 功能描述:对除IRP_MJ_DEVICE_CONTROL以外的IRP进行处理
* 参数列表:
		  fdo:功能设备对象
          Irp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverDispatchRoutine(IN PDEVICE_OBJECT fdo,
								 IN PIRP Irp)
{
	PAGED_CODE();

	//KdPrint(("Enter HelloWDMDispatchRoutine\n"));
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;	// no bytes xfered
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	//KdPrint(("Leave HelloWDMDispatchRoutine\n"));
	return STATUS_SUCCESS;
}

/************************************************************************
* 函数名称:HelloWDMUnload
* 功能描述:负责驱动程序的卸载操作
* 参数列表:
		DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
void NCDriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();
	//KdPrint(("Enter HelloWDMUnload\n"));
	//KdPrint(("Leave HelloWDMUnload\n"));
	return;
}


/************************************************************************
* 函数名称:CompleteRequest
* 功能描述:NCDriverDeviceControl函数中用以完成并返回IRP的代码共享例程
* 参数列表:
		DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS CompleteRequest(IN PIRP Irp, IN NTSTATUS status, IN ULONG_PTR info)
{
	PAGED_CODE();

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}



/************************************************************************
* 函数名称:C6x_Write_Word
* 功能描述:用户层程序调用 DeviceIoControl() 向下位机发送控制码的响应函数
* 参数列表:
DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma 
void C6x_Write_Word(PULONG Hpic_adr,PULONG Hpia_adr,PULONG Hpid_Noincrement_adr,
					ULONG Source_adr,ULONG Source_data)
{
	PAGED_CODE();
	ULONG j;
	ULONG val1;
	j=0;

	WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
	WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr);
	WRITE_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr,Source_data);

	WRITE_REGISTER_ULONG(Hpic_adr,0x00110011);
	WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr);
	val1 = READ_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr);
	
	while((val1&0xfff) != (Source_data&0xfff))
	{
		WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
		WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr);
		WRITE_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr,Source_data);

		WRITE_REGISTER_ULONG(Hpic_adr,0x00110011);
		WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr);
		val1 = READ_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr);

		j++;
		if(j > 10)
		{
			KdPrint(("error2!!!!!!!!!!!!!\n"));
			j=0;
			break;
		}
	}

}

/************************************************************************
* 函数名称:C6x_Write_NoIncrement_Section
* 功能描述:用户层程序调用 DeviceIoControl() 向下位机发送控制码的响应函数
* 参数列表:
DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
void C6x_Write_NoIncreament_Section(PULONG Hpic_adr,PULONG Hpia_adr,
	PULONG Hpid_Noincrement_adr,ULONG Source_adr,PULONG Source_data,ULONG length)
{
	PAGED_CODE();
	ULONG i,j;
	ULONG val1;

	for(i=0;i<length;i++)
	{
		WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
		WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr+4*i);
		WRITE_REGISTER_ULONG(Hpid_Noincrement_adr,*(Source_data+i));

		j=0;
		WRITE_REGISTER_ULONG(Hpic_adr,0x00110011);
		WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr+4*i);
		val1 = READ_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr);

		while(val1 != *(Source_data+i))
		{
			WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
			WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr+4*i);
			WRITE_REGISTER_ULONG(Hpid_Noincrement_adr,*(Source_data+i));

			WRITE_REGISTER_ULONG(Hpic_adr,0x00110011);
			WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr+4*i);
			val1 = READ_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr);

			j++;
			if(j > 10)
			{
				KdPrint((" error1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" ));
				j=0;
				break;
			}
		}
	}
}

/************************************************************************
* 函数名称:C6x_Write_Increment_Section
* 功能描述:用户层程序调用 DeviceIoControl() 向下位机发送控制码的响应函数
* 参数列表:
DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
void C6x_Write_Increment_Section(PULONG Hpic_adr,PULONG Hpia_adr,PULONG Hpid_increment_adr,
			PULONG Hpid_Noincrement_adr,ULONG Source_adr,PULONG Source_data,ULONG length)
{
	PAGED_CODE();
	ULONG i;

	WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
	WRITE_REGISTER_ULONG((PULONG)Hpic_adr,Source_adr);

	for(i=0; i<length; i++)
	{
		if(i != length - 1)
			WRITE_REGISTER_ULONG((PULONG)Hpid_increment_adr,*(Source_data+i));
		else
			WRITE_REGISTER_ULONG((PULONG)Hpid_Noincrement_adr,*(Source_data+i));
	}
}
/************************************************************************
* 函数名称:C6x_Read_Increament_Section
* 功能描述:用户层程序调用 DeviceIoControl() 向下位机发送控制码的响应函数
* 参数列表:
DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma  PAGEDCODE
void C6x_Read_Increament_Section(PULONG Hpic_adr,PULONG Hpia_adr,PULONG Hpid_increment_adr,
								 ULONG Source_adr,ULONG Source_data,ULONG length)
{
	PAGED_CODE();
	ULONG i;

	for(i=0; i<16; i++)
	{
		WRITE_REGISTER_ULONG(Hpic_adr,0x00010001);
		WRITE_REGISTER_ULONG((PULONG)Hpia_adr,Source_adr+4*i);
		*(&Source_data+i) = READ_REGISTER_ULONG((PULONG)Hpid_increment_adr)&0xffff;
	}
}


/************************************************************************
* 函数名称:NCDriverDeviceControl
* 功能描述:用户层程序调用 DeviceIoControl() 向下位机发送控制码的响应函数
* 参数列表:
		DriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverDeviceControl(PDEVICE_OBJECT fdo, PIRP Irp)
{
	PAGED_CODE();

	NTSTATUS status=STATUS_SUCCESS;
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	//存放本次操作总字节数的变量
	ULONG info = 0;
	ULONG val1;
	LP_NC_FPGA_stru pInBuf;
	
	PULONG PUhpidata,PUdecodedata,PUTapeParameterData,OutputBuffer;

	//unsigned int temp;
	ULONG i,j;
	//int datasize;
	ULONG *HpiA;
	ULONG HpiA_Temp;
	//ULONG Test_val;


	//如果设备未正常工作，则不能使用本驱动
	if(pdx->uPnpStateFlag!=2)
	{
		return CompleteRequest(Irp,STATUS_UNSUCCESSFUL,info);
	}

	//为编程上的方便，所有IO控制码都采用直接内存模式访问，因此在派遣函数的开始部分可以统一获取输入和输出缓冲区虚拟地址信息
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	//得到输入缓冲区的基地址和大小信息
	ULONG uInBufLen = stack->Parameters.DeviceIoControl.InputBufferLength;
	//映射输出缓冲区至非分页的系统内存，并记录下此块内存的信息
	ULONG uOutBufLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
	
	//pInBuf = (LPIOstru)Irp->AssociatedIrp.SystemBuffer;
	
	
	//得到IO控制码
	ULONG uIoctrlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	PULONG adrAdr = pdx->pHPIA;
	PULONG dataNoIncrementAdr = pdx->pHPIDStatic;
	PULONG dataIncrementAdr = pdx->pHPIDIncrease;

	switch (uIoctrlCode)
	{
		//机床位置信息读取，此命令无输入参数，输出参数为包含机床绝对坐标的结构体，结果放入输出缓冲区，本IRP采用缓冲方式
		case NC_POSITION_READ:
			info = uOutBufLen;
			break;

		case NC_VELOCITY_READ:
			info = uOutBufLen;
			break;
		
		case NC_FPGA_READ:
			pInBuf = (LP_NC_FPGA_stru)Irp->AssociatedIrp.SystemBuffer;
			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00010001);
			WRITE_REGISTER_ULONG((PULONG)adrAdr,NC_FPGA_BASE_ADR+((pInBuf->adr)<<1));
			pInBuf->val = READ_REGISTER_ULONG((PULONG)dataNoIncrementAdr)&0xffff;
			info = uOutBufLen;
			break;

		case NC_FPGA_WRITE:
			pInBuf = (LP_NC_FPGA_stru)Irp->AssociatedIrp.SystemBuffer;
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,
				NC_FPGA_BASE_ADR+(pInBuf->adr<<1),pInBuf->val&0xffff);
			info = uOutBufLen;
			break;

		case DSP_MEMORY_READ:
			HpiA = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
			HpiA_Temp = *HpiA;
			OutputBuffer = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

			for(i=0; i<80; i++)
			{
				WRITE_REGISTER_ULONG(adrAdr,HpiA_Temp+i*4);
				*(OutputBuffer+i) = READ_REGISTER_ULONG((PULONG)dataNoIncrementAdr);
			}
			info = uOutBufLen;
			break;
		case DSP_PARAMETER_WRITE:
			j=0;
			pInBuf = (LP_NC_FPGA_stru)Irp->AssociatedIrp.SystemBuffer;

			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00010001);
			WRITE_REGISTER_ULONG((PULONG)adrAdr,DSP_TAPEPARAMETER_BASE_ADR+pInBuf->adr);
			WRITE_REGISTER_ULONG((PULONG)dataNoIncrementAdr,pInBuf->val&0xffff);

			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00110011);
			WRITE_REGISTER_ULONG((PULONG)adrAdr,DSP_TAPEPARAMETER_BASE_ADR+pInBuf->adr);
			val1 = READ_REGISTER_ULONG((PULONG)dataNoIncrementAdr)&0xffff;

			while(val1 != ((pInBuf->val)&0xffff))
			{
				WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00010001);
				WRITE_REGISTER_ULONG((PULONG)adrAdr,DSP_TAPEPARAMETER_BASE_ADR+pInBuf->adr);
				WRITE_REGISTER_ULONG((PULONG)dataNoIncrementAdr,pInBuf->val&0xffff);

				WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00110011);
				WRITE_REGISTER_ULONG((PULONG)adrAdr,DSP_TAPEPARAMETER_BASE_ADR+pInBuf->adr);
				val1 = READ_REGISTER_ULONG((PULONG)dataNoIncrementAdr)&0xffff;

				j++;
				if(j > 10)
				{
					KdPrint((" error2!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" ));
					j=0;
					break;
				}
			}
			break;

		case DSP_PARAMETER_READ:
			pInBuf = (LP_NC_FPGA_stru)Irp->AssociatedIrp.SystemBuffer;
			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0x00010001);
			WRITE_REGISTER_ULONG((PULONG)adrAdr,DSP_PARAMETER_BASE_ADR+pInBuf->adr);
			pInBuf->val = READ_REGISTER_ULONG((PULONG)dataNoIncrementAdr)&0xffff;
			info = uOutBufLen;
			break;

		case DSP_TAPEPARAMETER_WRITE:
			PUTapeParameterData = (PULONG)Irp->AssociatedIrp.SystemBuffer;
			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataNoIncrementAdr,
				DSP_TAPEPARAMETER_BASE_ADR,PUTapeParameterData,uInBufLen/4);
			info = uOutBufLen;
			break;
		case NC_TRANSMIT_EVENT:
			KdPrint(("get into event test\n"));
			pdx->hUserDecodeEvent = *(HANDLE*)Irp->AssociatedIrp.SystemBuffer;
			status = ObReferenceObjectByHandle(pdx->hUserDecodeEvent, EVENT_MODIFY_STATE,
				*ExEventObjectType, KernelMode, (PVOID*) &pdx->pDecodeEvent, NULL);
			break;

		case NC_HPI_DECODE_LOAD:
			PUdecodedata = (PULONG)Irp->AssociatedIrp.SystemBuffer;
			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataIncrementAdr,
				DSP_DECODE_BASE_ADR,PUdecodedata,uInBufLen/4);
			pdx->bParaShmCreat=1;
			info = uInBufLen;
			KdPrint((" NC_HPI_DECODE_LOAD\n" ));
			break;

		case NC_HPIBOOTLOAD:
			KdPrint((" NC_HPI_BOOT_LOAD\n" ));

			//WRITE_REGISTER_ULONG((PULONG)pdx->pCtrlSpace,0X00010001);

			PUhpidata = (PULONG)Irp->AssociatedIrp.SystemBuffer;

			// 传送EMIF初始化数据到DSP

			//*  Global Control Reg. (GBLCTL) 
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800000,(ULONG)0x00003768);		

			//*  CE1 Space Control Reg. (CECTL1)  
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800004,0x513a8502);

			//*  CE0 Space Control Reg. (CECTL0)    setup 2, Wstrobe 10, Rstrobe 7,hold 2 
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800008,(ULONG)0x22920712);//0xffffbf13

			//*  CE2 Space Control Reg. (CECTL2)
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800010,0x10510191);

			//*  CE3 Space Control Reg. (CECTL 3) 0xFFFFFF23 
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800014,(ULONG)0x10510191);//0xffffbf93
			//*  SDRAM Control Reg.(SDCTL)
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800018,(ULONG)0x6b227000);//0x6b227000		

			//*  SDRAM Extended Reg.(SDEXT)
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x0180001c,(ULONG)0x00000300);

			//*  SDRAM Extended Reg.(SDEXT) 
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800020,(ULONG)0x00000128);

			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800014,(ULONG)0x10510191);//0xffffbf93
			C6x_Write_Word(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x01800018,(ULONG)0x6b227000);//0x6b227000		

			//for(i=0;i<120;i++)
			//         KdPrint((" NC VAL  %X\n",*(PUhpidata +i)));
			// 传送DSP程序到DSP
			//for(i=0;i<800000; i++);  

			KdPrint((" count1\n" ));
			//C6x_Write_Increament_Section(pdx->pCtrlSpace,adrAdr,dataIncrementAdr,dataNoIncrementAdr,0x00000000,PUhpidata,0X0400/4);

			//C6x_Write_Increament_Section(pdx->pCtrlSpace,adrAdr,dataIncrementAdr,dataNoIncrementAdr,0xb0000000,PUhpidata,uInBufLen/4);

			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x00000000,PUhpidata,0X0400/4);

			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0xb0000000,PUhpidata,uInBufLen/4);

			// 传送DSPINT到DSP，启动DSP
			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0X00030003); 
			KdPrint((" count2\n" ));			
			//设置PCI2040的CSR的中断有效
			//WRITE_REGISTER_ULONG((PULONG)pdx->pHpiCsr+0X08/4,0X80000001);
			break;
	
		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}	//switch (code)

	//调用自定义例程返回操作信息
	return CompleteRequest(Irp, status, info);
}

