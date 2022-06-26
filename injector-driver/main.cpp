#include "communicate.h"
#include "disassembly.h"
#include "kernel_structs.h"
#include "hooking.h"
#include "forte_api_kernel.h"
#include "util.h"
#include "offsets.h"

struct DllParams
{
	uint32_t header;
	size_t dll_size;
};

enum INJECTOR_CONSTANTS
{
	mapped_dll_header = 0x12345678,
	entrypoint_npt_hook = 0xAAAA
};

using namespace Interface;

void CommandHandler(void* system_buffer, void* output_buffer)
{
	auto request = (Msg*)system_buffer;

	auto msg_id = request->message_id;

	DbgPrint("msg_id %i \n", msg_id);

	switch (request->message_id)
	{
		case Interface::EXIT_CLEANUP:
		{
			DbgPrint("Exit request \n");
			return;
		}
		case Interface::START_THREAD:
		{
			auto msg = *(InvokeRemoteFunctionCmd*)request;

			DbgPrint("receieved request %i start thread ProcessID %i msg.map_base %p \n", msg_id, msg.proc_id, msg.map_base);

			auto apc = Utils::AttachToProcess(msg.proc_id);
			
			auto dll_params = (DllParams*)msg.map_base;

			dll_params->dll_size = msg.image_size;
			dll_params->header = mapped_dll_header;

			UNICODE_STRING d3d11_name = RTL_CONSTANT_STRING(L"dxgi.dll");

			auto dxgi = (uintptr_t)Utils::GetUserModule(PsGetCurrentProcess(), &d3d11_name);

			auto present = (uintptr_t)dxgi + DXGI_OFFSET::swapchain_present;

			auto present_hk = Hooks::JmpRipCode{ present, msg.address };

			// NPT hook on dxgi.dll!CDXGISwapChain::Present
			ForteVisor::SetNptHook(present, present_hk.hook_code, present_hk.hook_size, entrypoint_npt_hook);

			KeUnstackDetachProcess(&apc);

			break;
		}
		case Interface::ALLOC_MEM:
		{
			auto msg = *(AllocMemCmd*)request;

			DbgPrint("receieved request %i, process id %i size 0x%p \n", msg_id, msg.proc_id, msg.size);

			uintptr_t address = NULL;

			auto apc = Utils::AttachToProcess(msg.proc_id);

			auto status = ZwAllocateVirtualMemory(
				NtCurrentProcess(),
				(void**)&address,
				0,
				(size_t*)&msg.size,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_EXECUTE_READWRITE
			);

			memset((void*)address, 0x00, msg.size);
	
			KeUnstackDetachProcess(&apc);

			*(uintptr_t*)output_buffer = address;

			break;
		}
		case Interface::MODULE_BASE:
		{
			auto msg = *(GetModuleMsg*)request;

			DbgPrint("receieved request %i module name %ws\n", msg_id, msg.module);

			auto apcstate = Utils::AttachToProcess(msg.proc_id);

			UNICODE_STRING mod_name;
			RtlInitUnicodeString(&mod_name, msg.module);
				
			auto module_base = (void*)Utils::GetUserModule(IoGetCurrentProcess(), &mod_name);

			KeUnstackDetachProcess(&apcstate);

			DbgPrint("retrieved module base %p \n", module_base);

			*(void**)output_buffer = module_base;

			break;
		}
		case Interface::SET_NPT_HOOK:
		{
			auto hook_cmd = *(NptHookMsg*)request;

			DbgPrint("receieved request %i hook_cmd.shellcode 0x%p hook_cmd.size %i \n", hook_cmd.message_id, hook_cmd.shellcode, hook_cmd.size);

			auto apcstate = Utils::AttachToProcess(hook_cmd.proc_id);

			ForteVisor::SetNptHook(hook_cmd.hook_address, hook_cmd.shellcode, hook_cmd.size, NULL);

			KeUnstackDetachProcess(&apcstate);

			break;
		}
		case Interface::WRITE_MEM:
		{		
			auto msg = (WriteCmd*)request;

			auto status = Utils::WriteMem(msg->proc_id, msg->address, msg->buffer, msg->size);
		
			break;
		}
		default:
		{
			break;
		}
	}
}

NTSTATUS DriverEntry(uintptr_t driver_base, uintptr_t driver_size)
{
	DbgPrint("hello, driver_base %p, driver_size %p \n", driver_base, driver_size);

	Disasm::Init();
	HANDLE thread_handle;

	PsCreateSystemThread(
		&thread_handle,
		GENERIC_ALL, NULL, NULL, NULL,
		(PKSTART_ROUTINE)Interface::Init,
		NULL
	);

    return STATUS_SUCCESS;
}

NTSTATUS MapperEntry(uintptr_t driver_base, uintptr_t driver_size)
{
    return DriverEntry(driver_base, driver_size);
}