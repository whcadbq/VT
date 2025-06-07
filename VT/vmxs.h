#pragma once
#include <ntifs.h>

EXTERN_C VOID AsmGetGdtTable(PVOID tableBaseAddr);
EXTERN_C USHORT AsmReadES();
EXTERN_C USHORT AsmReadCS();
EXTERN_C USHORT AsmReadSS();
EXTERN_C USHORT AsmReadDS();
EXTERN_C USHORT AsmReadFS();
EXTERN_C USHORT AsmReadGS();
EXTERN_C USHORT AsmReadTR();
EXTERN_C USHORT AsmReadLDTR();
EXTERN_C VOID AsmInvd();
EXTERN_C VOID AsmVmCall(ULONG exitCode);
EXTERN_C VOID AsmJmpRet(ULONG64 rip,ULONG64 rsp);

EXTERN_C void AsmVmxExitHandler();