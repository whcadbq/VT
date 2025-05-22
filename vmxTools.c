#include "vmxTools.h"
#include "vmxDefine.h"
#include <intrin.h>

//检测Bios是否开启VT
BOOLEAN VmxIsCheckSupportVTBIOS()
{
	IA32_FEATURE_CONTROL_MSR msr;
	msr.Value = __readmsr(IA32_FEATURE_CONTROL);
	return (msr.Lock == 1) && (msr.EnableVmxon == 1);
}


//检测CPU是否支持VT
BOOLEAN VmxIsCheckSupportVTCPUID()
{
	int cpuidinfo[4] = { 0 };
	__cpuidex(cpuidinfo, 1, 0);
	CPUID_ECX ecx;
	ecx.Value = (unsigned int)cpuidinfo[2];
	return ecx.VMX == 1;
}


//检测CR4VT是否开启
BOOLEAN VmxIsCheckSupportVTCr4()
{
	CR4 cr4;
	cr4.value = __readcr4();
	return cr4.flags.VMXE == 0;
}