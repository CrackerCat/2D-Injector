#pragma once
#include "includes.h"
#include "kernel_structs.h"

#define	RELATIVE_ADDR(insn, operand_offset, size) (ULONG64)(*(int*)((uint32_t*)insn + operand_offset) + (uint32_t*)insn + (int)size)

namespace Utils
{
	uint32_t Random();

    void SwapEndianess(
		PCHAR dest, 
		PCHAR src
	);

	uintptr_t FindPattern(
		uintptr_t region_base,
		size_t region_size,
		const char* pattern,
		size_t pattern_size,
		char wildcard
	);
	
	NTSTATUS WriteMem(
		int32_t target_pid, 
		uintptr_t address, 
		void* buffer, 
		size_t size
	);

	NTSTATUS ReadMem(
		int32_t target_pid, 
		uintptr_t address,
		void* buffer, 
		size_t size
	);

	HANDLE GetProcessId(
		const char* process_name
	);

	void* WriteFile(
		void* buffer,
		const wchar_t* file_name,
		uint64_t size
	);

	void* GetKernelModule(
		size_t* out_size,
		UNICODE_STRING driver_name
	);

	void* GetUserModule(
		PEPROCESS pProcess,
		PUNICODE_STRING ModuleName
	);

	inline PMDL LockPages(PVOID VirtualAddress, LOCK_OPERATION  operation, int size = PAGE_SIZE)
	{
		PMDL mdl = IoAllocateMdl(VirtualAddress, size, FALSE, FALSE, nullptr);

		MmProbeAndLockPages(mdl, KernelMode, operation);

		return mdl;
	}
	
	KAPC_STATE AttachToProcess(int32_t pid);
}