#include "vmx.h"
VMX_CPU vmx_cpus[128];

PVMX_CPU VmxGetCPUPCB(ULONG cpuNumber)
{
	return &vmx_cpus[cpuNumber];
}

PVMX_CPU VmxGetCurrentCPUPCB()
{
	return VmxGetCPUPCB(KeGetCurrentProcessorNumberEx(NULL));
}

int VmxInitVmOn()
{
	PVMX_CPU pVcpu = VmxGetCurrentCPUPCB();
	PHYSICAL_ADDRESS lowphys, heiPhy;
	lowphys.QuadPart = 0;
	heiPhy.QuadPart = -1;
	pVcpu->VmxOnAddr = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, lowphys, heiPhy, lowphys, MmCached);
	if (!pVcpu->VmxOnAddr)
	{
		//申请内存失败
		return -1;
	}
	memset(pVcpu->VmxOnAddr, 0, PAGE_SIZE);
	pVcpu->VmxOnAddrPhys = MmGetPhysicalAddress(pVcpu->VmxOnAddr);
	//填充ID
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);
	*(PULONG)pVcpu->VmxOnAddr = (ULONG)vmxBasic;
	//CR0,CR4
	ULONG64 vcr00 = __readmsr(IA32_VMX_CR0_FIXED0);//00000000`80000021
	ULONG64 vcr01 = __readmsr(IA32_VMX_CR0_FIXED1);//00000000`ffffffff
	ULONG64 vcr40 = __readmsr(IA32_VMX_CR4_FIXED0);//00000000`00002000
	ULONG64 vcr41 = __readmsr(IA32_VMX_CR4_FIXED1);//00000000`001727ff
	ULONG64 mcr4 = __readcr4();//00000000`000406f8
	ULONG64 mcr0 = __readcr0();//00000000`80050031
	//相当于cr4.vmxe=1
	mcr4 |= vcr40;
	mcr4 &= vcr41;
	//运算未修改cr0的值
	mcr0 |= vcr00;
	mcr0 &= vcr01;
	__writecr0(mcr0);
	__writecr4(mcr4);
	int error = __vmx_on(&pVcpu->VmxOnAddrPhys.QuadPart);
	return error;
}

void FullGdtDataItem(int index, short selector)
{
	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);
	ULONG limit = __segmentlimit(selector);
	USHORT cleanSelector = selector & 0xFFF8;
	SEGMENT_DESCRIPTOR* desc = (SEGMENT_DESCRIPTOR*)(gdtTable.Base + cleanSelector);
	// 构造段基址（Base = BaseLow | BaseMiddle << 16 | BaseHigh << 24）
	LARGE_INTEGER itemBase = { 0 };
	itemBase.LowPart = desc->Fields.BaseLow | (desc->Fields.BaseMiddle << 16) | (desc->Fields.BaseHigh << 24);
	// 构造段属性（AR 字节）
	ULONG attr = ((desc->Fields.Access) | ((desc->Fields.Granularity & 0xF0) << 4));
	// 如果是 NULL 段选择子，设置 unusable 标志（bit 16）
	if (cleanSelector == 0) {
		attr |= (1 << 16);
	}
	// 写入 VMCS 中对应段字段
	__vmx_vmwrite(GUEST_ES_BASE + index * 2, itemBase.QuadPart);
	__vmx_vmwrite(GUEST_ES_LIMIT + index * 2, limit);
	__vmx_vmwrite(GUEST_ES_AR_BYTES + index * 2, attr);
	__vmx_vmwrite(GUEST_ES_SELECTOR + index * 2, selector);
}

void VmxInitGuest(ULONG64 GuestEip, ULONG64 GuestEsp)
{
	FullGdtDataItem(0, AsmReadES());
	FullGdtDataItem(1, AsmReadCS());
	FullGdtDataItem(2, AsmReadSS());
	FullGdtDataItem(3, AsmReadDS());
	FullGdtDataItem(4, AsmReadFS());
	FullGdtDataItem(5, AsmReadGS());
	FullGdtDataItem(6, AsmReadLDTR());
	//写tr
	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);
	ULONG trSelector = AsmReadTR();
	trSelector &= 0xFFF8;
	ULONG trlimit = __segmentlimit(trSelector);
	LARGE_INTEGER trBase = { 0 };
	PULONG trItem = (PULONG)(gdtTable.Base + trSelector);
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];
	ULONG attr = (trItem[1] & 0x00F0FF00) >> 8;
	__vmx_vmwrite(GUEST_TR_BASE, trBase.QuadPart);
	__vmx_vmwrite(GUEST_TR_LIMIT, trlimit);
	__vmx_vmwrite(GUEST_TR_AR_BYTES, attr);
	__vmx_vmwrite(GUEST_TR_SELECTOR, trSelector);

	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR4, __readcr4());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_DR7, __readdr(7));
	__vmx_vmwrite(GUEST_RFLAGS, __readeflags());
	__vmx_vmwrite(GUEST_RSP, GuestEsp);
	__vmx_vmwrite(GUEST_RIP, GuestEip);

	__vmx_vmwrite(VMCS_LINK_POINTER, -1);

	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(IA32_MSR_DEBUGCTL));
	__vmx_vmwrite(GUEST_IA32_PAT, __readmsr(IA32_MSR_PAT));
	__vmx_vmwrite(GUEST_IA32_EFER, __readmsr(IA32_MSR_EFER));

	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(IA32_GS_BASE));

	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(0x174));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(0x175));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(0x176));


	//IDT GDT

	GdtTable idtTable;
	__sidt(&idtTable);

	__vmx_vmwrite(GUEST_GDTR_BASE, gdtTable.Base);
	__vmx_vmwrite(GUEST_GDTR_LIMIT, gdtTable.limit);
	__vmx_vmwrite(GUEST_IDTR_LIMIT, idtTable.limit);
	__vmx_vmwrite(GUEST_IDTR_BASE, idtTable.Base);

}

void VmxInitHost(ULONG64 HostEip)
{
	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);

	PVMX_CPU pVcpu = VmxGetCurrentCPUPCB();

	ULONG trSelector = AsmReadTR();

	trSelector &= 0xFFF8;

	LARGE_INTEGER trBase = { 0 };

	PULONG trItem = (PULONG)(gdtTable.Base + trSelector);


	//读TR
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];

	//属性
	__vmx_vmwrite(HOST_TR_BASE, trBase.QuadPart);
	__vmx_vmwrite(HOST_TR_SELECTOR, trSelector);

	__vmx_vmwrite(HOST_ES_SELECTOR, AsmReadES() & 0xfff8);
	__vmx_vmwrite(HOST_CS_SELECTOR, AsmReadCS() & 0xfff8);
	__vmx_vmwrite(HOST_SS_SELECTOR, AsmReadSS() & 0xfff8);
	__vmx_vmwrite(HOST_DS_SELECTOR, AsmReadDS() & 0xfff8);
	__vmx_vmwrite(HOST_FS_SELECTOR, AsmReadFS() & 0xfff8);
	__vmx_vmwrite(HOST_GS_SELECTOR, AsmReadGS() & 0xfff8);



	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR4, __readcr4());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_RSP, (ULONG64)pVcpu->VmxHostStackBase);
	__vmx_vmwrite(HOST_RIP, HostEip);


	__vmx_vmwrite(HOST_IA32_PAT, __readmsr(IA32_MSR_PAT));
	__vmx_vmwrite(HOST_IA32_EFER, __readmsr(IA32_MSR_EFER));

	__vmx_vmwrite(HOST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(IA32_GS_KERNEL_BASE));

	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(0x174));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(0x175));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(0x176));


	//IDT GDT

	GdtTable idtTable;
	__sidt(&idtTable);

	__vmx_vmwrite(HOST_GDTR_BASE, gdtTable.Base);
	__vmx_vmwrite(HOST_IDTR_BASE, idtTable.Base);
}

void VmxInitEntry()
{
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);
	ULONG64 mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;

	ULONG64 value = VmxAdjustContorls(0x200, mseregister);
	__vmx_vmwrite(VM_ENTRY_CONTROLS, value);
	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);
}

void VmxInitExit()
{
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	ULONG64 mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_EXIT_CTLS : IA32_MSR_VMX_EXIT_CTLS;

	ULONG64 value = VmxAdjustContorls(0x200 | 0x8000, mseregister);
	__vmx_vmwrite(VM_EXIT_CONTROLS, value);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_INTR_INFO, 0);
}

void VmxInitControls()
{
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	ULONG64 mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_PINBASED_CTLS : IA32_MSR_VMX_PINBASED_CTLS;

	ULONG64 value = VmxAdjustContorls(0, mseregister);

	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, value);



	mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_PROCBASED_CTLS : IA32_MSR_VMX_PROCBASED_CTLS;

	value = VmxAdjustContorls(0, mseregister);

	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, value);

	/*
	//扩展部分
	mseregister = IA32_MSR_VMX_PROCBASED_CTLS2;

	value = VmxAdjustContorls(0, mseregister);

	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, value);
	*/
}

int VmxInitVmcs(ULONG64 GuestEip, ULONG64 GuestEsp, ULONG64 hostEip)
{
	//初始化vmcs
	PVMX_CPU pVcpu = VmxGetCurrentCPUPCB();
	PHYSICAL_ADDRESS lowphys, heiPhy;
	lowphys.QuadPart = 0;
	heiPhy.QuadPart = -1;
	pVcpu->VmxCsAddr = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, lowphys, heiPhy, lowphys, MmCached);
	if (!pVcpu->VmxCsAddr)
	{
		return -1;
	}
	memset(pVcpu->VmxCsAddr, 0, PAGE_SIZE);
	pVcpu->VmxCsAddrPhys = MmGetPhysicalAddress(pVcpu->VmxCsAddr);

	//初始化栈
	pVcpu->VmxHostStackTop = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE * 36, lowphys, heiPhy, lowphys, MmCached);
	if (!pVcpu->VmxHostStackTop)
	{
		return -1;
	}
	memset(pVcpu->VmxHostStackTop, 0, PAGE_SIZE * 36);
	pVcpu->VmxHostStackBase = (ULONG64)pVcpu->VmxHostStackTop + PAGE_SIZE * 36 - 0x200;


	//填充ID
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);
	*(PULONG)pVcpu->VmxCsAddr = (ULONG)vmxBasic;
	//加载VMCS
	__vmx_vmclear(&pVcpu->VmxCsAddrPhys.QuadPart);
	__vmx_vmptrld(&pVcpu->VmxCsAddrPhys.QuadPart);

	//设置5个区域
	VmxInitGuest(GuestEip, GuestEsp);

	VmxInitHost(hostEip);

	VmxInitEntry();

	VmxInitExit();

	VmxInitControls();

	return 0;
}
void VmxDestory()
{
	PVMX_CPU pVcpu = VmxGetCurrentCPUPCB();
	//释放VmxOnAddr
	if (pVcpu->VmxOnAddr && MmIsAddressValid(pVcpu->VmxOnAddr))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxOnAddr, PAGE_SIZE, MmCached);
	}
	pVcpu->VmxOnAddr = NULL;
	pVcpu->VmxOnAddrPhys.QuadPart = 0;
	//释放VmxCsAddr
	if (pVcpu->VmxCsAddr && MmIsAddressValid(pVcpu->VmxCsAddr))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxCsAddr, PAGE_SIZE, MmCached);
	}
	pVcpu->VmxCsAddr = NULL;
	pVcpu->VmxCsAddrPhys.QuadPart = 0;
	//释放VmxHostStackTop
	if (pVcpu->VmxHostStackTop && MmIsAddressValid(pVcpu->VmxHostStackTop))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxHostStackTop, PAGE_SIZE * 36, MmCached);
	}
	pVcpu->VmxHostStackTop = NULL;
	pVcpu->VmxHostStackBase = NULL;
	//恢复cr4
	CR4 mcr4;
	mcr4.value = __readcr4();
	mcr4.flags.VMXE = 0;
	__writecr4(mcr4.value);
}
int VmxInit(ULONG64 hostEip)
{
	PVMX_CPU pVcpu = VmxGetCurrentCPUPCB();
	pVcpu->cpuNumber = KeGetCurrentProcessorNumberEx(NULL);
	PULONG64 retAddr = (PULONG64)_AddressOfReturnAddress();
	ULONG64 guestEsp = retAddr + 1;
	ULONG64 guestEip = *retAddr;
	int error = VmxInitVmOn();
	if (error)
	{
		DbgPrintEx(77, 0, "[db]:vmon 初始化失败 error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);
		VmxDestory();
		return error;
	}
	error = VmxInitVmcs(guestEip, guestEsp, hostEip);
	if (error)
	{
		DbgPrintEx(77, 0, "[db]:vmcs 初始化失败 error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);
		VmxDestory();
		return error;
	}
	//开启VT
	error = __vmx_vmlaunch();
	if (error)
	{
		DbgPrintEx(77, 0, "[db]:__vmx_vmlaunch失败 error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);
		VmxDestory();
	}
	return 0;
}

