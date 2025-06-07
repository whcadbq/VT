#include "vmxHandler.h"
#include <intrin.h>
#include "VMXDefine.h"
#include "vmxs.h"

#define MAKE_REG(XXX1,XXX2) ((XXX1 & 0xFFFFFFFF) | (XXX2<<32))

VOID VmxHandlerCpuid(PGuestContext context)
{
	DbgPrintEx(77, 0, "[db]:GETSEC reason");
	if (context->mRax == 0x8888)
	{
		context->mRax = 0x11111111;
		context->mRbx = 0x22222222;
		context->mRcx = 0x33333333;
		context->mRdx = 0x44444444;
	}
	else 
	{
		int cpuids[4] = {0};
		__cpuidex(cpuids,context->mRax, context->mRcx);
		context->mRax = cpuids[0];
		context->mRbx = cpuids[1];
		context->mRcx = cpuids[2];
		context->mRdx = cpuids[3];
	}
}

VOID VmxHandlerReadMsr(PGuestContext context)
{
	ULONG64 value = __readmsr(context->mRcx);
	context->mRax = value & 0xFFFFFFFF;
	context->mRdx = (value>>32) & 0xFFFFFFFF;
}


VOID VmxHandlerWriteMsr(PGuestContext context)
{
	ULONG64 value = MAKE_REG(context->mRax, context->mRdx);
	__writemsr(context->mRcx, value);
	
}


EXTERN_C VOID VmxExitHandler(PGuestContext context)
{
	ULONG64 reason = 0;
	ULONG64 instLen = 0;
	ULONG64 instinfo = 0;
	ULONG64 mrip = 0;
	ULONG64 mrsp = 0;
	
	__vmx_vmread(VM_EXIT_REASON, &reason);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instLen); // 获取指令长度
	__vmx_vmread(VMX_INSTRUCTION_INFO, &instinfo); //指令详细信息
	__vmx_vmread(GUEST_RIP, &mrip); //获取客户机触发VT事件的地址
	__vmx_vmread(GUEST_RSP, &mrsp); 

	//获取事件码
	reason = reason & 0xFFFF;

	if (reason != 0x1f && reason != 0x20)
	{
		DbgPrintEx(77, 0, "[db]:reason = %x\r\n", reason);
	}
	

	switch (reason)
	{
		case EXIT_REASON_CPUID:
			VmxHandlerCpuid(context);
			break;

		case EXIT_REASON_GETSEC:
		{
			DbgBreakPoint();
			DbgPrintEx(77, 0, "[db]:GETSEC reason = %x rip = %llx\r\n", reason, mrip);
		}
			break;

		case EXIT_REASON_TRIPLE_FAULT:
		{
			DbgBreakPoint();
			DbgPrintEx(77, 0, "[db]:GETSEC reason = %x rip = %llx\r\n", reason, mrip);
		}
			break;

		case EXIT_REASON_INVD:
		{
			AsmInvd();
		}
			break;

		case EXIT_REASON_VMCALL:
		{
			if (context->mRax == 'babq')
			{
				__vmx_off();
				AsmJmpRet(mrip + instLen, mrsp);
				return;
			}
			else 
			{
				ULONG64 rfl = 0;
				__vmx_vmread(GUEST_RFLAGS, &rfl);
				rfl |= 0x41;
				__vmx_vmwrite(GUEST_RFLAGS, &rfl);
			}
		}
	
			break;
		case EXIT_REASON_VMCLEAR		:
		case EXIT_REASON_VMLAUNCH		:
		case EXIT_REASON_VMPTRLD		:
		case EXIT_REASON_VMPTRST		:
		case EXIT_REASON_VMREAD			:
		case EXIT_REASON_VMRESUME		:
		case EXIT_REASON_VMWRITE		:
		case EXIT_REASON_VMXOFF			:
		case EXIT_REASON_VMXON			:
		{
			ULONG64 rfl = 0;
			__vmx_vmread(GUEST_RFLAGS, &rfl);
			rfl |= 0x41;
			__vmx_vmwrite(GUEST_RFLAGS, &rfl);
		}
		break;

		case EXIT_REASON_MSR_READ:
		{
			VmxHandlerReadMsr(context);
		}
			break;

		case EXIT_REASON_MSR_WRITE:
		{
			VmxHandlerWriteMsr(context);
		}
			break;

		case EXIT_REASON_XSETBV:
		{
			ULONG64 value = MAKE_REG(context->mRax, context->mRdx);
			_xsetbv(context->mRcx, value);
		}
		break;

		
	}


	__vmx_vmwrite(GUEST_RIP, mrip + instLen);
	__vmx_vmwrite(GUEST_RSP, mrsp);
}