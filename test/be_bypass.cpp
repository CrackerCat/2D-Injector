#include "pch.h"
#include "be_bypass.h"
#include "utils.h"
#include "hooks.h"
#include "forte_api.h"

#define BATTLEYE_NAME L"test-anticheat.exe"

LONG BreakpointRemoverVEH(_EXCEPTION_POINTERS* ExceptionInfo)
{
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        if (*(BYTE*)ExceptionInfo->ContextRecord->Rip == 0xCC)
        {
            UNICODE_STRING mod_name;

            auto module_base = (uintptr_t)Utils::ModuleFromAddress(ExceptionInfo->ContextRecord->Rip, &mod_name);
            auto file_patch_offset = Utils::RvaToOffset((void*)module_base, (uintptr_t)ExceptionInfo->ContextRecord->Rip - module_base);

            Utils::log("breakpoint hit! RIP = %p\n", ExceptionInfo->ContextRecord->Rip);

            auto file_handle = CreateFileW(
                mod_name.Buffer, GENERIC_ALL, 0, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
            );

            auto rip_page = PAGE_ALIGN(ExceptionInfo->ContextRecord->Rip);
            DWORD size = 1;
            ULONG old_protect, old_protect2;

            auto status = ZwProtectVirtualMemory((HANDLE)-1, (PVOID*)&rip_page, &size, PAGE_EXECUTE_READWRITE, &old_protect);

            /*  Restore the byte from disk  */ 

            DWORD bytes_read;

            SetFilePointer(
                file_handle, file_patch_offset,
                NULL, FILE_BEGIN
            );

            ReadFile(
                file_handle,
                (void*)ExceptionInfo->ContextRecord->Rip,
                1, &bytes_read, NULL
            );

            ZwProtectVirtualMemory((HANDLE)-1, (PVOID*)&rip_page, &size, old_protect, &old_protect2);
        }

        return EXCEPTION_CONTINUE_EXECUTION;
    }
    else if (ExceptionInfo->ExceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C)
    {
        // for OutputDebugString
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

//uint64_t (__fastcall* RtlpAddVectoredHandler)(
//    int a1,
//    __int64 a2,
//    unsigned int a3
//) = NULL;
//
//uint64_t RtlAddVectoredExceptionHandler_hook(int first, __int64 handler_addr, unsigned int a3)
//{            
//    auto retaddr = *(uintptr_t*)_AddressOfReturnAddress();
//
//    UNICODE_STRING mod_name;
//    auto module_base = Utils::ModuleFromAddress(retaddr, &mod_name);
//
//    if (!module_base || !wcscmp(mod_name.Buffer, BATTLEYE_NAME))
//    {
//        if (!module_base)
//        {
//            Utils::log("RtlAddVectoredExceptionHandler was called from 0x%p \n", retaddr);
//        }
//        else
//        {
//            Utils::log("RtlAddVectoredExceptionHandler was called from %wZ+0x%p \n", mod_name, retaddr-(uintptr_t)module_base);
//        }
//
//        static_cast<decltype(RtlpAddVectoredHandler)>(addveh_hook->original_bytes)(
//            first, 
//            (uint64_t)BreakpointRemoverVEH,
//            a3
//        );
//
//        return (uint64_t)BreakpointRemoverVEH;
//    }
//    else
//    {
//        return static_cast<decltype(RtlpAddVectoredHandler)>(addveh_hook->original_bytes)(
//            first,
//            (uint64_t)handler_addr,
//            a3
//        );
//    }
//}

Hooks::JmpRipCode* is_bad_read_hk = NULL;

BOOL IsBadRead_caller(CONST VOID* param1, UINT_PTR param2)
{
    auto kernel32 = GetModuleHandle(L"kernel32.dll");

    auto is_bad_read = (decltype(&IsBadReadPtr))GetProcAddress(kernel32, "IsBadReadPtr");

    return is_bad_read(param1, param2);
}

BOOL IsBadRead_caller_handler(_In_opt_ CONST VOID* lp,
    _In_     UINT_PTR ucb)
{
    auto result = static_cast<decltype(&IsBadRead_caller)>((void*)is_bad_read_hk->original_bytes)(lp, ucb);

    __debugbreak();

    return result;
}


void BypassBattleye()
{
    /*  testing procedure:
        1. launch hypervisor
        2. launch test-anticheat
        3. attach debugger/open dbgview
        4. use CE to inject test bypass .dll
    */
    
    Disasm::Init();

    auto kernel32 = GetModuleHandle(L"kernel32.dll");

    auto is_bad_read = (decltype(&IsBadReadPtr))GetProcAddress(kernel32, "IsBadReadPtr");

   // is_bad_read_hk = new Hooks::JmpRipCode{ (uintptr_t)IsBadRead_caller, (uintptr_t)IsBadRead_caller_handler };

    // ForteVisor::SetNptHook((uintptr_t)IsBadRead_caller, is_bad_read_hk->hook_code, is_bad_read_hk->hook_size);
    ForteVisor::SetNptHook((uintptr_t)is_bad_read, (uint8_t*)"\xCC", 1);

    __debugbreak();
    IsBadRead_caller(NULL, 0);

    while (1)
    {
        Sleep(600);
        IsBadRead_caller(NULL, 0);
    }
}