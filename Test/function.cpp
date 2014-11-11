#include <windows.h>
#include <stdio.h>
#include <winioctl.h>
#include "function.h"

#define IMAGE_LENGTH 640*480
HANDLE GetDeviceViaInterface( GUID* pGuid, DWORD instance)
{
	// Get handle to relevant device information set
	HDEVINFO info = SetupDiGetClassDevs(pGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);	//返回一个设备信息集的句柄，包含本地机器的设备信息元素
	if(info==INVALID_HANDLE_VALUE)
	{
		printf("No HDEVINFO available for this GUID\n");
		return NULL;
	}

	// Get interface data for the requested instance
	SP_INTERFACE_DEVICE_DATA ifdata;
	ifdata.cbSize = sizeof(ifdata);
	if(!SetupDiEnumDeviceInterfaces(info, NULL, pGuid, instance, &ifdata))	//枚举设备信息集中的设备接口
	{
		printf("No SP_INTERFACE_DEVICE_DATA available for this GUID instance\n");
		SetupDiDestroyDeviceInfoList(info);
		return NULL;
	}

	// Get size of symbolic link name
	DWORD ReqLen;
	SetupDiGetDeviceInterfaceDetail(info, &ifdata, NULL, 0, &ReqLen, NULL);	//返回设备接口的细节，此处是为了得到符号链接名的大小
	PSP_INTERFACE_DEVICE_DETAIL_DATA ifDetail = (PSP_INTERFACE_DEVICE_DETAIL_DATA)(new char[ReqLen]);
	if( ifDetail==NULL)
	{
		SetupDiDestroyDeviceInfoList(info);
		return NULL;
	}

	// Get symbolic link name
	ifDetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	if( !SetupDiGetDeviceInterfaceDetail(info, &ifdata, ifDetail, ReqLen, NULL, NULL))	//此处是为了得到符号链接名的名字，存在ifDetail中
	{
		SetupDiDestroyDeviceInfoList(info);
		delete ifDetail;
		return NULL;
	}

	printf("Symbolic link is %s\n",ifDetail->DevicePath);
	// Open file
	HANDLE rv = CreateFile( ifDetail->DevicePath, 
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( rv==INVALID_HANDLE_VALUE) rv = NULL;

	delete ifDetail;
	SetupDiDestroyDeviceInfoList(info);
	return rv;
}


int Load_NC_TapeParameter_Program(HANDLE handle,LPVOID offset,UCHAR *buff,ULONG length)
{		
	BOOL bResult;
	DWORD junk;

	bResult = DeviceIoControl(handle,  // device to be queried
		DSP_TAPEPARAMETER_WRITE,  // operation to perform
		offset,			// input buffer
		length,				// input buffer bytes
		NULL,			// output buffer
		0,				// output buffer bytes
		&junk,			// # bytes returned
		NULL);			// synchronous I/O

	if(bResult==FALSE)
	{
		printf("can not get i/o data in fuction Load_NC_TapeParameter_Program");
		return 1;
	}	
	return 0;
}

int DspMemRead(HANDLE handle,LPVOID offset,LPVOID buff,ULONG length)
{
	BOOL bResult;
	DWORD junk; 

	bResult = DeviceIoControl(handle,  // device to be queried
		DSP_MEMORY_READ,		// operation to perform
		offset,					// input buffer
		4,						// input buffer bytes
		buff,					// output buffer
		length,						// output buffer bytes
		&junk,					// # bytes returned
		NULL);					// synchronous I/O

	if(bResult==FALSE)
	{
		printf("can not Read");
		return 1;
	}
	return 0;
}
////UCHAR ReadByteFromBase0(HANDLE handle,ULONG offset)
////{
////	UCHAR cRet;
////	ReadFromBase0(handle,offset,&cRet,1);
////	return cRet;
////}
////
////UCHAR ReadByteFromBase2(HANDLE handle,ULONG offset)
////{
////	UCHAR cRet;
////	ReadFromBase2(handle,offset,&cRet,1);
////	return cRet;
////}
////
////ULONG ReadFromBase0(HANDLE handle,ULONG offset,UCHAR *buff,ULONG length)
////{
////	ULONG nReadNumber;
////	//发送IOCTL到指定的设备驱动，使对应设备执行特定操作
////	//参数1：要执行操作的设备句柄，参数2：IOCTL代号，用于与驱动程序交互，参数3：指向缓冲区的指针，该缓冲区包含执行操作时需要的数据
////	//参数4：由参数3指定的缓冲区的字节数，参数5：接收输出数据的缓冲区指针，参数6：参数5指定的缓冲区的字节数
////	//参数7：指向一个变量的指针，变量为参数5接收缓冲区得到数据的大小（字节计），参数8：忽略，设为NULL
////	DeviceIoControl(handle, IOCTL_READ_BASE_BAR0, &offset, 4, buff, length, &nReadNumber, NULL);
////
////	return nReadNumber;
////}
////
////ULONG ReadFromBase2(HANDLE handle,ULONG offset,UCHAR *buff,ULONG length)
////{
////	ULONG nReadNumber;
////
////	DeviceIoControl(handle, IOCTL_READ_BASE_BAR2, &offset, 4, buff, length, &nReadNumber, NULL);
////
////	return nReadNumber;
////}
////
////ULONG WriteToBase2(HANDLE handle,ULONG offset,UCHAR *buff,ULONG length)
////{
////	ULONG nWriteNumber;
////
////	PVOID input[3] ;
////
////	input[0] = (PVOID) offset;
////
////	input[1] = (PVOID) buff;
////
////	input[2] = (PVOID) length;
////
////	DeviceIoControl(handle, IOCTL_WRITE_BASE_BAR2, input, 12, NULL, 0, &nWriteNumber, NULL);
////
////	return nWriteNumber;
////
////}
////
////ULONG WriteToBase0(HANDLE handle,ULONG offset,UCHAR *buff,ULONG length)
////{
////	ULONG nWriteNumber;
////
////	PVOID input[3] ;
////
////	input[0] = (PVOID) offset;
////
////	input[1] = (PVOID) buff;
////
////	input[2] = (PVOID) length;
////	//发送IOCTL到指定的设备驱动，使对应设备执行特定操作
////	//参数1：要执行操作的设备句柄，参数2：IOCTL代号，用于与驱动程序交互，参数3：指向缓冲区的指针，该缓冲区包含执行操作时需要的数据
////	//参数4：由参数3指定的缓冲区的字节数，参数5：接收输出数据的缓冲区指针，参数6：参数5指定的缓冲区的字节数
////	//参数7：指向一个变量的指针，变量为参数5接收缓冲区得到数据的大小（字节计），参数8：忽略，设为NULL
////	DeviceIoControl(handle, IOCTL_WRITE_BASE_BAR0, input, 12, NULL, 0, &nWriteNumber, NULL);
////
////	return nWriteNumber;
////
////}
////
////BOOL WriteByteToBase2(HANDLE handle,ULONG offset,UCHAR input)
////{
////	WriteToBase2(handle,offset,&input,1);
////	return TRUE;
////}
////
////BOOL WriteByteToBase0(HANDLE handle,ULONG offset,UCHAR input)
////{
////	WriteToBase0(handle,offset,&input,1);
////	return TRUE;
////}
////
////BOOL WriteDwordToBase2(HANDLE handle,ULONG offset,DWORD input)
////{
////	PUCHAR pBuff = (PUCHAR)&input;
////
////	//端口操作为big endian
////	WriteByteToBase2(handle,offset,pBuff[0]);
////	WriteByteToBase2(handle,offset+1,pBuff[1]);
////	WriteByteToBase2(handle,offset+2,pBuff[2]);
////	WriteByteToBase2(handle,offset+3,pBuff[3]);
////
////	return TRUE;
////}
////
////BOOL WriteDwordToBase0(HANDLE handle,ULONG offset,DWORD input)
////{
////	PUCHAR pBuff = (PUCHAR)&input;
////	
////	//内存操作为little endian
////	
////	WriteToBase0(handle,offset,pBuff,sizeof(DWORD));
////
////	return TRUE;
////}
////
////ULONG ReadImage(HANDLE handle,UCHAR *buff)
////{
////	ULONG nReadNumber;
////
////	DeviceIoControl(handle, IOCTL_READ_IMAGE, NULL, 0, buff, IMAGE_LENGTH, &nReadNumber, NULL);
////
////	return nReadNumber;
////}
////
////ULONG WriteImage(HANDLE handle,UCHAR *buff)
////{
////	ULONG nWriteNumber;
////
////	DeviceIoControl(handle, IOCTL_WRITE_IMAGE, buff, IMAGE_LENGTH, NULL, 0, &nWriteNumber, NULL);
////
////	return nWriteNumber;
////}
////BOOL EnableInt(HANDLE handle)
////{
////	ULONG dwReturn;
////	DeviceIoControl(handle, IOCTL_ENABLE_INT, NULL, 0, NULL, 0, &dwReturn, NULL);
////	return TRUE;
////}
////
////BOOL DisableInt(HANDLE handle)
////{
////	ULONG dwReturn;
////	DeviceIoControl(handle, IOCTL_DISABLE_INT, NULL, 0, NULL, 0, &dwReturn, NULL);
////	return TRUE;
////}