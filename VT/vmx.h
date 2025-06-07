#pragma once
#include <ntifs.h>

typedef struct _VMXCPUPCB 
{
	ULONG cpuNumber;
	PVOID VmxOnAddr;
	PHYSICAL_ADDRESS VmxOnAddrPhys;

	PVOID VmxcsAddr;
	PHYSICAL_ADDRESS VmxcsAddrPhys;

	PVOID VmxHostStackTop;  //ջ�� С
	PVOID VmxHostStackBase; //ջ�� ��

	PVOID MsrBitMap;  //
	PHYSICAL_ADDRESS MsrBitMapAddr; 

}VMXCPUPCB,*PVMXCPUPCB;

#pragma pack(push,1)
typedef struct _GdtTable
{
	USHORT limit;
	ULONG64 Base;
}GdtTable,*PGdtTable;
#pragma pack(pop)

PVMXCPUPCB VmxGetCPUPCB(ULONG cpuNumber);

PVMXCPUPCB VmxGetCurrentCPUPCB();


void FullGdtDataItem(int index, short selector);

int VmxInit(ULONG64 hostEip);

void VmxDestory();