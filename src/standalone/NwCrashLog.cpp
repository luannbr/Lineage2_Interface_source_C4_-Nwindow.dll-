/*=============================================================================
    NwCrashLog.cpp - what the standalone NWindow.dll writes when the process faults (standalone only).

    The engine catches a GPF with its guard()/unguard() and shows its own crash box: l2.log keeps only
    the History: line, no address. A vectored handler sees the fault first: it appends the exception,
    the registers, the frame-pointer chain and the return addresses found on the stack to
    NWindow_crash.log (next to l2.exe), each address as module+RVA; for this DLL
    `python tools/port/map_addr.py <rva>...` names the functions from build/standalone/Release/NWindow.map.
    Only access violations, illegal instructions, int3, divisions by zero and stack overflows are logged
    (the first 40 per run); nothing is handled or changed.
=============================================================================*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    LONG GCrashCount = 0;
    HANDLE GCrashFile = INVALID_HANDLE_VALUE;

    void Out(const char* Text)
    {
        if (GCrashFile == INVALID_HANDLE_VALUE)
            return;
        DWORD Written;
        WriteFile(GCrashFile, Text, lstrlenA(Text), &Written, NULL);
    }

    void Where(char* Buf, DWORD Address)
    {
        HMODULE Module = NULL;
        char Name[MAX_PATH];
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)Address, &Module)
            && GetModuleFileNameA(Module, Name, MAX_PATH))
        {
            const char* Base = Name;
            for (const char* p = Name; *p; p++)
                if (*p == '\\' || *p == '/')
                    Base = p + 1;
            wsprintfA(Buf, "%08X %s+%X", Address, Base, Address - (DWORD)Module);
        }
        else
            wsprintfA(Buf, "%08X (no module)", Address);
    }

    LONG CALLBACK NwVectoredHandler(EXCEPTION_POINTERS* Info)
    {
        DWORD Code = Info->ExceptionRecord->ExceptionCode;
        if (Code != EXCEPTION_ACCESS_VIOLATION && Code != EXCEPTION_ILLEGAL_INSTRUCTION && Code != EXCEPTION_BREAKPOINT
            && Code != EXCEPTION_INT_DIVIDE_BY_ZERO && Code != EXCEPTION_STACK_OVERFLOW && Code != EXCEPTION_PRIV_INSTRUCTION
            && Code != EXCEPTION_IN_PAGE_ERROR)
            return EXCEPTION_CONTINUE_SEARCH;
        if (InterlockedIncrement(&GCrashCount) > 40)
            return EXCEPTION_CONTINUE_SEARCH;
        GCrashFile = CreateFileA("NWindow_crash.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, NULL);
        if (GCrashFile == INVALID_HANDLE_VALUE)
            return EXCEPTION_CONTINUE_SEARCH;
        char Line[512], Place[MAX_PATH + 40];
        SYSTEMTIME Time;
        GetLocalTime(&Time);
        const CONTEXT* C = Info->ContextRecord;
        Where(Place, (DWORD)Info->ExceptionRecord->ExceptionAddress);
        wsprintfA(Line, "=== %02d:%02d:%02d exception %08X at %s (thread %lu)\r\n", Time.wHour, Time.wMinute, Time.wSecond, Code,
                  Place, GetCurrentThreadId());
        Out(Line);
        if (Code == EXCEPTION_ACCESS_VIOLATION && Info->ExceptionRecord->NumberParameters >= 2)
        {
            wsprintfA(Line, "    %s address %08X\r\n",
                      Info->ExceptionRecord->ExceptionInformation[0] == 0 ? "read of" :
                      Info->ExceptionRecord->ExceptionInformation[0] == 1 ? "write to" : "execute of",
                      (DWORD)Info->ExceptionRecord->ExceptionInformation[1]);
            Out(Line);
        }
        wsprintfA(Line, "    eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\r\n", C->Eax, C->Ebx, C->Ecx,
                  C->Edx, C->Esi, C->Edi, C->Ebp, C->Esp);
        Out(Line);
        // the code bytes at eip
        __try
        {
            const BYTE* Code = (const BYTE*)C->Eip;
            wsprintfA(Line, "    code at eip: %02X %02X %02X %02X %02X %02X %02X %02X\r\n", Code[0], Code[1], Code[2], Code[3], Code[4],
                      Code[5], Code[6], Code[7]);
            Out(Line);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        // the frame-pointer chain (our code keeps ebp frames)
        Out("    frames (ebp chain):\r\n");
        __try
        {
            DWORD Ebp = C->Ebp;
            for (int i = 0; i < 24 && Ebp >= C->Esp - 0x10000 && Ebp != 0 && (Ebp & 3) == 0; i++)
            {
                DWORD Ret = ((DWORD*)Ebp)[1];
                Where(Place, Ret);
                wsprintfA(Line, "      %s\r\n", Place);
                Out(Line);
                DWORD Next = ((DWORD*)Ebp)[0];
                if (Next <= Ebp)
                    break;
                Ebp = Next;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        // return addresses seen on the stack (code of any module), for frames without ebp
        Out("    stack scan (words that point into a module's code):\r\n");
        __try
        {
            const DWORD* Sp = (const DWORD*)C->Esp;
            int Found = 0;
            for (int i = 0; i < 160 && Found < 28; i++)
            {
                HMODULE Module = NULL;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)Sp[i], &Module)
                    && Module)
                {
                    // keep it to words that sit in an executable section: the byte before is a call (E8 or FF /2)
                    const BYTE* Ret = (const BYTE*)Sp[i];
                    BOOL IsRet = (Ret[-5] == 0xE8) || (Ret[-2] == 0xFF) || (Ret[-3] == 0xFF) || (Ret[-6] == 0xFF);
                    if (IsRet)
                    {
                        Where(Place, Sp[i]);
                        wsprintfA(Line, "      [esp+%03X] %s\r\n", i * 4, Place);
                        Out(Line);
                        Found++;
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        Out("\r\n");
        CloseHandle(GCrashFile);
        GCrashFile = INVALID_HANDLE_VALUE;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // registered when the DLL is loaded (a static initializer: before DllMain returns)
    struct FNwCrashLogInstaller
    {
        FNwCrashLogInstaller()
        {
            AddVectoredExceptionHandler(1, NwVectoredHandler);
        }
    } GNwCrashLogInstaller;
}
