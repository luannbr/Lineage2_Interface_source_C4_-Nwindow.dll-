// nwd_smoke - loads a System folder's NWindow.dll outside the game.
//
// The patched stock NWindow.dll imports NWindowDecomp.dll, so loading it runs our DllMain: the
// stock image check + hook installation, reported in NWindowDecomp.log. Core.dll and Engine.dll
// load too (their static initializers only), nothing else of the engine runs. The C2 NWindow.dll
// also pulls in npkcrypt.dll (imported by ordinal), WS2_32, ADVAPI32 and WINMM.
//
//   nwd_smoke.exe "<...>\Lineage II - Chronicle 3 - Rise of Darkness\system_nwtest"
//   nwd_smoke.exe --standalone <System> ...   a NWindow.dll without NWindowDecomp.dll, loaded
//                                             and registered the engine's way
//                                             (nwd_smoke_standalone.cpp; phase 5)
//
// Exit code: 0 when NWindow.dll loaded and NWindowDecomp.dll is in the process, 1 otherwise.
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int StandaloneMain(int argc, wchar_t** argv);
void HookModeStandIns();

static void Try(const wchar_t* name)
{
    HMODULE h = LoadLibraryW(name);
    printf("  %-20ls %s (%lu)\n", name, h ? "ok" : "FAILED", h ? 0 : GetLastError());
}

// First-chance exceptions during the load (a static initializer that faults makes LoadLibrary fail
// with 1114): code, address as module+RVA and, for an access violation, what it touched.
static LONG CALLBACK LoadFault(EXCEPTION_POINTERS* Info)
{
    const EXCEPTION_RECORD* R = Info->ExceptionRecord;
    if (R->ExceptionCode == 0x406D1388 || (R->ExceptionCode & 0xF0000000) == 0x40000000)
        return EXCEPTION_CONTINUE_SEARCH; // thread name / debugger messages
    HMODULE Mod = NULL;
    wchar_t Name[MAX_PATH] = L"?";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)R->ExceptionAddress, &Mod))
    {
        GetModuleFileNameW(Mod, Name, MAX_PATH);
        const wchar_t* Slash = wcsrchr(Name, L'\\');
        if (Slash)
            wmemmove(Name, Slash + 1, wcslen(Slash));
    }
    printf("  EXCEPTION %08lx at %p (%ls+%lx)", R->ExceptionCode, R->ExceptionAddress, Name,
           (unsigned long)((BYTE*)R->ExceptionAddress - (BYTE*)Mod));
    if (R->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && R->NumberParameters >= 2)
        printf(" %s %p", R->ExceptionInformation[0] ? "write to" : "read of", (void*)R->ExceptionInformation[1]);
    const CONTEXT* C = Info->ContextRecord;
    printf("\n    eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx\n", C->Eax,
           C->Ebx, C->Ecx, C->Edx, C->Esi, C->Edi, C->Ebp, C->Esp);
    // the words of the stack that point into a loaded module's code (return addresses, in order)
    printf("    stack:");
    int Shown = 0;
    for (int i = 0; i < 160 && Shown < 14; i++)
    {
        DWORD W = ((DWORD*)C->Esp)[i];
        HMODULE M = NULL;
        wchar_t N[MAX_PATH];
        if (W > 0x10000 && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)W, &M) &&
            GetModuleFileNameW(M, N, MAX_PATH))
        {
            const wchar_t* Slash = wcsrchr(N, L'\\');
            printf(" %ls+%lx", Slash ? Slash + 1 : N, (unsigned long)(W - (DWORD)M));
            Shown++;
        }
    }
    printf("\n");
    return EXCEPTION_CONTINUE_SEARCH;
}

int wmain(int argc, wchar_t** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    AddVectoredExceptionHandler(1, LoadFault);
    if (argc >= 2 && !wcscmp(argv[1], L"--standalone"))
        return StandaloneMain(argc, argv);
    if (argc < 2)
    {
        fwprintf(stderr, L"usage: nwd_smoke <System folder>\n");
        return 2;
    }
    printf("System: %ls\n", argv[1]);
    if (!SetCurrentDirectoryW(argv[1]) || !SetDllDirectoryW(argv[1]))
    {
        printf("bad folder (%lu)\n", GetLastError());
        return 2;
    }
    // NWindowDecomp.dll must come from the System folder through NWindow.dll's import (loading
    // it first would run its DllMain before NWindow.dll exists). This exe is built into its own
    // folder so the application directory holds no other copy.
    HookModeStandIns();
    HMODULE stock = LoadLibraryW(L"NWindow.dll");
    if (!stock)
    {
        printf("LoadLibrary(NWindow.dll) failed: %lu - dependencies one by one:\n", GetLastError());
        const wchar_t* deps[] = {L"MSVCR70.dll", L"ogg.dll", L"vorbis.dll", L"vorbisfile.dll", L"IFC23.dll",
                                 L"npkcrypt.dll", L"Core.dll", L"Engine.dll", L"NWindowDecomp.dll"};
        for (const wchar_t* d : deps)
            Try(d);
        return 1;
    }
    HMODULE decomp = GetModuleHandleW(L"NWindowDecomp.dll");
    printf("NWindow.dll at %p, NWindowDecomp.dll at %p\n", (void*)stock, (void*)decomp);

    FILE* f = _wfopen(L"NWindowDecomp.log", L"rb");
    if (f)
    {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, n, stdout);
        fclose(f);
    }
    else
        printf("no NWindowDecomp.log\n");
    fflush(stdout);
    // No FreeLibrary: unloading the engine DLLs outside the game is not supported.
    ExitProcess(decomp ? 0 : 1);
}
