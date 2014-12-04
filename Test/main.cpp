#include <windows.h>
#include <stdio.h>
#include <winioctl.h> 

#include "function.h"

#include "../MyDriver/source/NCSunDriverGuid.h"
#include "../MyDriver/source/NCSunDriverIoctls.h"

int main()
{
	int TapePara[4] = {1,2,3,4};
	int MemRead[100];
	int ReadAddr;
	HANDLE hDevice = GetDeviceViaInterface((LPGUID)&NCCtrlDevice,0);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to obtain file handle to device: "
			"%s with Win32 error code: %d\n",
			"MyWDMDevice", GetLastError() );
		return 1;
	}

	//调用function.h中提供的各个函数
	Load_NC_TapeParameter_Program(hDevice,TapePara,NULL,16);
	printf("Writing Completed\n");

	ReadAddr = 0xa00104c8;
	DspMemRead(hDevice,&ReadAddr,MemRead,400);
	printf("Reading Completed, the data is:%x,%x,%x,%x\n",MemRead[0],MemRead[1],MemRead[2],MemRead[3]);

	CloseHandle(hDevice);
	getchar();
	return 0;
}