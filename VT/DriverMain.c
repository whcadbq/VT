#include <ntifs.h>
#include "VMXTools.h"
#include "vmx.h"
#include "vmxs.h"

VOID KeGenericCallDpc(__in PKDEFERRED_ROUTINE Routine,__in_opt PVOID Context);

VOID KeSignalCallDpcDone(__in PVOID SystemArgument1);

LOGICAL KeSignalCallDpcSynchronize(__in PVOID SystemArgument2);

VOID VmxStartVT(_In_ struct _KDPC *Dpc,_In_opt_ PVOID DeferredContext,_In_opt_ PVOID SystemArgument1,_In_opt_ PVOID SystemArgument2)
{
	do 
	{
		
		if (!VmxIsCheckSupportVTCPUID())
		{
			DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTCPUID  number = %d\r\n", KeGetCurrentProcessorNumber());
			break;
		}

		if (!VmxIsCheckSupportVTBIOS())
		{
			DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTBIOS  number = %d\r\n", KeGetCurrentProcessorNumber());
			break;
		}

		if (!VmxIsCheckSupportVTCr4())
		{
			DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTCr4  number = %d\r\n", KeGetCurrentProcessorNumber());
			break;
		}

		VmxInit(DeferredContext);
	} while (0);
	

	KeSignalCallDpcDone(SystemArgument1);
	KeSignalCallDpcSynchronize(SystemArgument2);
}

VOID VmxStopVT(_In_ struct _KDPC *Dpc, _In_opt_ PVOID DeferredContext, _In_opt_ PVOID SystemArgument1, _In_opt_ PVOID SystemArgument2)
{
	AsmVmCall('babq');
	VmxDestory();
	KeSignalCallDpcDone(SystemArgument1);
	KeSignalCallDpcSynchronize(SystemArgument2);
}




VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	KeGenericCallDpc(VmxStopVT, NULL);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pReg)
{

	KeGenericCallDpc(VmxStartVT, AsmVmxExitHandler);
	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}