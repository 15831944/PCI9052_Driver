/************************************************************************
* �ļ�����:NCSunDriver.cpp                                                 
* ��    ��:��  ��
* �������:2014-9-28
* ��������:NC_PCI_SUN����������Դ�ļ�
*************************************************************************/
#include "NCSunDriver.h"
#include "NCSunDriverGuid.h"
#include "NCSunDriverIoctls.h"

/************************************************************************
* ��������:DriverEntry
* ��������:NC_PCI_SUN�������������ں�������ʼ���������򣬶�λ������Ӳ����Դ�������ں˶���
* �����б�:
          pDriverObject:��I/O�������д���������������
          pRegistryPath:����������ע�����е�·��
* ����ֵ :���س�ʼ������״̬
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
* ��������:NCDriverAddDevice
* ��������:��PNP���������ã��������豸����ӣ��˺�PNP�����������PNP_MN_START_DEVICE����Ӧ����
* �����б�:
		  DriverObject:��I/O�������д���������������
          PhysicalDeviceObject:��I/O�������д������������豸����
* ���� ֵ:����������豸״̬
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
		sizeof(DEVICE_EXTENSION),         //����DeviceExtension��Ĵ�С
		NULL,//û��ָ���豸��
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&fdo);
	if( !NT_SUCCESS(status))
		return status;
	//��ù����豸����fdo��DeviceExtension��ָ�루��ַ��,���Գ�ʼ�����е�ȫ�ֱ���
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION)fdo->DeviceExtension;
	//�����豸��չ���豸��������ӹ�ϵ
	pdx->fdo = fdo;
	//���豸������չ�����豸��ջ���²��豸���豸����ָ��
	pdx->NextStackDevice = IoAttachDeviceToDeviceStack(fdo, PhysicalDeviceObject);
	//�����豸�ӿ�,�������û���������豸��������򿪱�����,�豸�ӿ���MotionCtrlDevice�� NCGuid.h ����
	//���������豸�ӿ����ɵ��豸��������������������豸��չ��
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
	//��ʼ���豸�������õ� DPC ����,�����˶���Ĵ���� OnRequest() ע��
	//InitializeDpcRequest(fdo,DpcForIsr);

	//ʹ�û���IO��ʽ
	fdo->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;
	fdo->Flags &= ~DO_DEVICE_INITIALIZING;

	//�����豸�Ѿ����ڴ��ڵ�ֹͣ״̬����NCDriver.h�ļ���DEVICE_EXTENSION�ṹ��Ķ���,��ʱ������Ӧ��PNP��IRPָ��
	pdx->uPnpStateFlag=1;
	//���������ڴ���δ��������ʱ������Թ����ڴ������ж�д����
	pdx->bParaShmCreat=FALSE;
	

	KdPrint(("Leave NCDriverAddDevice\n"));
	return STATUS_SUCCESS;
}

/************************************************************************
* ��������:DefaultPnpHandler
* ��������:��PNP IRP����ȱʡ����,�� NCDriverPnp() ��ǲ�����б�ʹ��
* �����б�:
		  pdx:�豸�������չ
          Irp:��IO�����
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS DefaultPnpHandler(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//���������������PNP����ȫ�����²�PDO����PCI������������
	//	KdPrint(("Enter DefaultPnpHandler\n"));
	IoSkipCurrentIrpStackLocation(Irp);
	//	KdPrint(("Leave DefaultPnpHandler\n"));
	return IoCallDriver(pdx->NextStackDevice, Irp);
}


/************************************************************************
* ��������:OnRequestComplete
* ��������:����ǲ���� NCDriverPnp() ��IRP_MN_START_DEVICE���ݸ�PCI�����������ʱ����PCI����������IRP��ɺ���
* �����б�:
		  DeviceObject:�豸����
          Irp:��IO�����
		  pEvent:�²�PCI����������ͬ���¼�����
* ���� ֵ:����״̬
*************************************************************************/
#pragma LOCKEDCODE
NTSTATUS OnRequestComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PKEVENT pEvent)
{
	//����������м����ȴ��¼���ʹ���㹦��������IRP_MN_START_DEVICE�������ǲ�������Լ���ִ�У��Ի���²�PCI����
	//�����������ĸ��� PCI ��Դ����������������IRP_MN_START_DEVICE�����IRP��ӦΪͬ����Ӧ��ʽ��
	KeSetEvent(pEvent, 0, FALSE);
	//��־��IRP����Ҫ�ٴα����
	return STATUS_MORE_PROCESSING_REQUIRED;
}

/************************************************************************
* ��������:ForwardAndWait
* ��������:�ѱ���IRP��IRP_MN_START_DEVICE���Ƚ���PCI�������������ȴ�PCI������ɷ��غ󣬻�÷���״̬
* �����б�:
		  pdx:�豸�������չ
          Irp:��IO�����
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS ForwardAndWait(PDEVICE_EXTENSION pdx, PIRP Irp)
{							// ForwardAndWait
	PAGED_CODE();
	
	KEVENT event;
	//��ʼ���¼�
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	//�������ջ��������һ���ջ
	IoCopyCurrentIrpStackLocationToNext(Irp);
	//�����������
	IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) OnRequestComplete,
		(PVOID) &event, TRUE, TRUE, TRUE);
	//���õײ���������PCI��������PDO
	IoCallDriver(pdx->NextStackDevice, Irp);
	//�ȴ�PDO��ɣ�IRP��ɺ�������ɺ���OnRequestComplete����ͬ���¼�event
	KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
	//Irp->IoStatus.Status�б�������²�PCI������ɱ���IRP_MN_START_DEVICE����IRP�ķ���ֵ��������ɺ������ص���
	//STATUS_MORE_PROCESSING_REQUIRED״̬�룬���Ա���������������ɺ���OnRequestComplete�����ú����»�öԴ�IRP
	//�Ŀ���Ȩ
	return Irp->IoStatus.Status;
}


/************************************************************************
* ��������:HandleRemoveDevice
* ��������:��IRP_MN_REMOVE_DEVICE IRP���д���,����ֹͣ�豸��ʹ�豸�ɡ�1=���ڵ�ֹͣ�� ״̬��Ϊ��0=�����ڡ�״̬,��AddDevice������Ӧ
* �����б�:
		  pdx:�豸�������չ
          Irp:��IO�����
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverRemoveDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//KdPrint(("Enter IODriverRemoveDevice\n"));
	Irp->IoStatus.Status = STATUS_SUCCESS;
	//���õײ�PCI������������豸ɾ����һ���Բ���
	NTSTATUS status = DefaultPnpHandler(pdx, Irp);
	//	KdPrint(("Leave HandleRemoveDevice\n"));

	//�ر��豸�ӿڣ���ʱ�û�������������������ʹ�ô�����
	IoSetDeviceInterfaceState(&pdx->InterfaceName, FALSE);
	//�ͷŴ���豸�ӿڷ����������������Ӵ���ڶ�̬������ڴ��У�
	RtlFreeUnicodeString(&pdx->InterfaceName);
	
	//���������ڴ���δ��������ʱ������Թ����ڴ������ж�д����
	pdx->bParaShmCreat=FALSE;

    //����IoDetachDevice()��fdo���豸ջ���ѿ���
    if (pdx->NextStackDevice)
        IoDetachDevice(pdx->NextStackDevice);
    //ɾ��fdo��
    IoDeleteDevice(pdx->fdo);
	pdx->uPnpStateFlag=0;

	
	return status;
}


/************************************************************************
* ��������:DpcForIsr
* ��������:PCI�ж���Ӧ��������ǲ�������������PCI�ж���Ӧ�����Ĵ������д�䶯������ʹ�ô����������Щ�жϹ���
* �����б�:
		  Dpc:�����豸����fdo��Ƕ��DPC����
		  fdo:��ǰ�����豸����
		  Irp:��ǰ��IRP����
		  Context:��IoRequestDpc()�����������Ĳ���
* ���� ֵ:����״̬

#pragma LOCKEDCODE
void DpcForIsr(PKDPC Dpc,PDEVICE_OBJECT fdo,PIRP Irp,PVOID Context)
{
	//DPC����
}
*************************************************************************/

/************************************************************************
* ��������:OnInterrupt
* ��������:PCI�ж���Ӧ����������ͨ�������ں�ģʽ��ͬ���¼������������̣߳��������п�����Ϊ�ж���
* �����б�:
		  InterruptObject:�ж϶���
		  pdx:�豸��չ
* ���� ֵ:����״̬
*************************************************************************/
#pragma LOCKEDCODE
BOOLEAN OnInterrupt(PKINTERRUPT InterruptObject, PDEVICE_EXTENSION pdx)
{
	//	KdPrint(("Entering PCI Interrupt!!!\n"));
	//����ж�Դ
	//����ͬ���¼������������߳�
	//KeSetEvent();

	//�������жϴ�������ø��ӣ������ͨ��ʹ��������������fdo�������DPC���̣�����NCDriverAddDevice������
	//	IoRequestDpc(pdx->fdo, NULL, pdx);

	return TRUE;
}


/************************************************************************
* ��������:InitNCBoard
* ��������:PCI��ʼ����������������IRP_MN_START_DEVICE�������ǲ�����б����ã������²�PCI�������÷��غ��ɱ�������
*                        ���PCI��Դ�ĳ�ʼ������
* �����б�:
		  pdx:�豸��չ
		  list�����²�PCI�����õ��ĳ�ʼPCI��Դ�б�
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS InitNCBoard(IN PDEVICE_EXTENSION pdx,IN PCM_PARTIAL_RESOURCE_LIST list)
{
	PAGED_CODE();

	PDEVICE_OBJECT fdo = pdx->fdo;
	//�ݴ��PCI��������õ��ж���Ϣ��ʹ�õı���
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KAFFINITY affinity;
	BOOLEAN irqshare;
	BOOLEAN gotinterrupt = FALSE;

	//PHYSICAL_ADDRESS portbase;
	BOOLEAN gotport = FALSE;

	//������Դ��������׵�ַ
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
			//�Ѵ�PCI��õ������ڴ�ӳ�䵽�Ƿ�ҳ�������ڴ�
			if (resource->u.Memory.Length == pdx->ulHPILen) 
			{
				KdPrint((" HPI BASE ADDRESS X%X\n",resource->u.Memory.Start));



				pdx->pHPIC = (PULONG)MmMapIoSpace(resource->u.Memory.Start,
					resource->u.Memory.Length,
					MmNonCached);

				pdx->pHPIA = pdx->pHPIC + 4;
				pdx->pHPIDIncrease = pdx->pHPIC + 8;
				pdx->pHPIDStatic = pdx->pHPIC + 12;

				//��ʼ��HPI���ƼĴ���HWOB
				WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0X00010001);
			}
			break;

			////case CmResourceTypePort:
			////	//I/O�˿���Դ
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
				//�Ѵ��²�PCI���ߴ���õ��ж���Ϣ��������ʱ������

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

	//�����жϲ�ʹ��
	NTSTATUS status = IoConnectInterrupt(&pdx->InterruptObject, (PKSERVICE_ROUTINE) OnInterrupt,(PVOID) pdx, NULL, vector, irql, irql, LevelSensitive, TRUE, affinity, FALSE);

	if (!NT_SUCCESS(status))
	{

		KdPrint(("IoConnectInterrupt failed - %X\n", status));
		return status;
	}

	
	return STATUS_SUCCESS;	
}


/************************************************************************
* ��������:NCDriverStartDevice
* ��������:PNP����IRP IRP_MN_START_DEVICE ����Ӧ���������������豸��ʹ�豸����2=��������������״̬
* �����б�:
		  pdx:�豸��չ
		  Irp:��ǰIRP
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverStartDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();

	KdPrint(("Enter HandleStartDevice\n"));

	//��IRPת�����²�PCI����PDO���ȴ����أ�����ֵ��PDO���IRP��״̬���ҷ���ʱ����FDO�����»����IRP�Ŀ���Ȩ
	NTSTATUS status = ForwardAndWait(pdx,Irp);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	//�õ���ǰ IO ��ջ
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	//�ӵ�ǰIO��ջ�õ��²�PDO�����ķ�����PCI��Դ��Ϣ
	PCM_PARTIAL_RESOURCE_LIST translated;
	if (stack->Parameters.StartDevice.AllocatedResourcesTranslated)
		translated = &stack->Parameters.StartDevice.AllocatedResourcesTranslated->List[0].PartialResourceList;
	else
	{
		translated = NULL;
		Irp->IoStatus.Status=STATUS_UNSUCCESSFUL;
		return STATUS_UNSUCCESSFUL;
	}
	//ӳ��IO����ռ�õ�PCI�洢���ռ�
	//KdPrint(("Init the PCI card!\n"));
	status=InitNCBoard(pdx,translated);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	//�����豸�ӿڣ������û���������������ſ���ʹ�ñ�����
	status=IoSetDeviceInterfaceState(&pdx->InterfaceName, TRUE);
	if (!NT_SUCCESS(status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	//���IRP,��ʱ�豸������������״̬=2
	pdx->uPnpStateFlag=2;                               
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	KdPrint(("Leave HandleStartDevice\n"));
	return status;
}

/************************************************************************
* ��������:NCDriverStopDevice
* ��������:PNP����IRP IRP_MN_STOP_DEVICE ����Ӧ����������ֹͣ�豸��ʹ�豸����1=���ڵ�ֹͣ״̬����ʱ��Ҫ�����豸����ʹ֮��������
* �����б�:
		  pdx:�豸��չ
		  Irp:��ǰIRP
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverStopDevice(PDEVICE_EXTENSION pdx, PIRP Irp)
{
	PAGED_CODE();
	//KdPrint(("Enter IODriverStopDevice\n"));
	//�ر��豸�ӿڣ���ʱ�û�������������������ʹ�ô�����
	IoSetDeviceInterfaceState(&pdx->InterfaceName, FALSE);
	//�ͷŴ���豸�ӿڷ����������������Ӵ���ڶ�̬������ڴ��У�
	RtlFreeUnicodeString(&pdx->InterfaceName);
	//�Ͽ��ж����ӣ�������ӦPCI�ж�                        
	IoDisconnectInterrupt(pdx->InterruptObject);

	pdx->uPnpStateFlag=1;
	//���������ڴ���δ��������ʱ������Թ����ڴ������ж�д����
	pdx->bParaShmCreat=FALSE;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	//�ȵ���PNP��ͨ�ô���������IRP�����²�PCI��������
	NTSTATUS status = DefaultPnpHandler(pdx, Irp);
	return status;
}

/************************************************************************
* ��������:HelloWDMPnp
* ��������:���弴��IRP����ǲ���������Զ�IRP_MJ_PNP��IRP������IRP���д���
* �����б�:
      fdo:�����豸����
      Irp:��IO�����
* ���� ֵ:����״̬
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
		NCDriverStartDevice,	// IRP_MN_START_DEVICE=0x0,����ͨ������Ҽ�->ת�����壬���鿴��Щ�ӹ��ܵ�ֵ
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
	{						// δ֪���ӹ��ܴ���
		status = DefaultPnpHandler(pdx, Irp); // some function we don't know about
		return status;
	}						

	//�˺�DBG=1���ڱ���׶ζ���ģ�/D DBG=1(��Ŀ���� -> C/C++ -> ������)
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

	//ͨ�������������Ӧ��Ӧ����
	status = (*fcntab[fcn])(pdx, Irp);

	//KdPrint(("Leave HelloWDMPnp\n"));
	return status;
}

/************************************************************************
* ��������:HelloWDMDispatchRoutine
* ��������:�Գ�IRP_MJ_DEVICE_CONTROL�����IRP���д���
* �����б�:
		  fdo:�����豸����
          Irp:��IO�����
* ���� ֵ:����״̬
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
* ��������:HelloWDMUnload
* ��������:�������������ж�ز���
* �����б�:
		DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:CompleteRequest
* ��������:NCDriverDeviceControl������������ɲ�����IRP�Ĵ��빲������
* �����б�:
		DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:C6x_Write_Word
* ��������:�û��������� DeviceIoControl() ����λ�����Ϳ��������Ӧ����
* �����б�:
DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:C6x_Write_NoIncrement_Section
* ��������:�û��������� DeviceIoControl() ����λ�����Ϳ��������Ӧ����
* �����б�:
DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:C6x_Write_Increment_Section
* ��������:�û��������� DeviceIoControl() ����λ�����Ϳ��������Ӧ����
* �����б�:
DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:C6x_Read_Increament_Section
* ��������:�û��������� DeviceIoControl() ����λ�����Ϳ��������Ӧ����
* �����б�:
DriverObject:��������
* ���� ֵ:����״̬
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
* ��������:NCDriverDeviceControl
* ��������:�û��������� DeviceIoControl() ����λ�����Ϳ��������Ӧ����
* �����б�:
		DriverObject:��������
* ���� ֵ:����״̬
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS NCDriverDeviceControl(PDEVICE_OBJECT fdo, PIRP Irp)
{
	PAGED_CODE();

	NTSTATUS status=STATUS_SUCCESS;
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	//��ű��β������ֽ����ı���
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


	//����豸δ��������������ʹ�ñ�����
	if(pdx->uPnpStateFlag!=2)
	{
		return CompleteRequest(Irp,STATUS_UNSUCCESSFUL,info);
	}

	//Ϊ����ϵķ��㣬����IO�����붼����ֱ���ڴ�ģʽ���ʣ��������ǲ�����Ŀ�ʼ���ֿ���ͳһ��ȡ�������������������ַ��Ϣ
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	//�õ����뻺�����Ļ���ַ�ʹ�С��Ϣ
	ULONG uInBufLen = stack->Parameters.DeviceIoControl.InputBufferLength;
	//ӳ��������������Ƿ�ҳ��ϵͳ�ڴ棬����¼�´˿��ڴ����Ϣ
	ULONG uOutBufLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
	
	//pInBuf = (LPIOstru)Irp->AssociatedIrp.SystemBuffer;
	
	
	//�õ�IO������
	ULONG uIoctrlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	PULONG adrAdr = pdx->pHPIA;
	PULONG dataNoIncrementAdr = pdx->pHPIDStatic;
	PULONG dataIncrementAdr = pdx->pHPIDIncrease;

	switch (uIoctrlCode)
	{
		//����λ����Ϣ��ȡ��������������������������Ϊ����������������Ľṹ�壬��������������������IRP���û��巽ʽ
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

			// ����EMIF��ʼ�����ݵ�DSP

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
			// ����DSP����DSP
			//for(i=0;i<800000; i++);  

			KdPrint((" count1\n" ));
			//C6x_Write_Increament_Section(pdx->pCtrlSpace,adrAdr,dataIncrementAdr,dataNoIncrementAdr,0x00000000,PUhpidata,0X0400/4);

			//C6x_Write_Increament_Section(pdx->pCtrlSpace,adrAdr,dataIncrementAdr,dataNoIncrementAdr,0xb0000000,PUhpidata,uInBufLen/4);

			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0x00000000,PUhpidata,0X0400/4);

			C6x_Write_NoIncreament_Section(pdx->pHPIC,adrAdr,dataNoIncrementAdr,0xb0000000,PUhpidata,uInBufLen/4);

			// ����DSPINT��DSP������DSP
			WRITE_REGISTER_ULONG((PULONG)pdx->pHPIC,0X00030003); 
			KdPrint((" count2\n" ));			
			//����PCI2040��CSR���ж���Ч
			//WRITE_REGISTER_ULONG((PULONG)pdx->pHpiCsr+0X08/4,0X80000001);
			break;
	
		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}	//switch (code)

	//�����Զ������̷��ز�����Ϣ
	return CompleteRequest(Irp, status, info);
}

