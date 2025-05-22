#pragma once
#include <ntifs.h>
#include <intrin.h>
#include "vmxDefine.h"
#include "vmxs.h"
typedef struct _VMX_CPU
{
    ULONG               cpuNumber;

    PVOID               VmxOnAddr;       
    PHYSICAL_ADDRESS    VmxOnAddrPhys;   

    PVOID               VmxCsAddr;   
    PHYSICAL_ADDRESS    VmxCsAddrPhys;   

    PVOID VmxHostStackTop;  
    PVOID VmxHostStackBase; 
}VMX_CPU, * PVMX_CPU;

PVMX_CPU VmxGetCPUPCB(ULONG cpuNumber);

PVMX_CPU VmxGetCurrentCPUPCB();

int VmxInit(ULONG64 hostEip);
