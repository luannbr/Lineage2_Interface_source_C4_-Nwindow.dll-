/*=============================================================================
    NwMain.cpp - NWindowDecomp.dll entry point (hook mode).

    Load order: the engine LoadLibrary()s NWindow.dll (stock image + an import of
    NWindowDecomp_Attach added by tools/install_test.py) -> the loader maps
    NWindowDecomp.dll and runs our DllMain first -> we verify the stock image and
    patch the reimplemented functions -> the stock DllMain / static
    initializers run, already on top of our code where hooked.
=============================================================================*/
#include <intrin.h>
#include "re/NwHook.h"
#include "re/NwLog.h"
#include "re/NwDebug.h"

namespace nw {

// the C4 client's NWindow.dll (md5 afab3ae3fa4f85f7f4c323715e5155e3, 13/01/2006): orig/NWindow.dll
static const DWORD kStockTimeDateStamp = 0x43C73652;
static const DWORD kStockSizeOfCode = 0x0019C000;
static const DWORD kStockEntryPoint = 0x00172B4A;

static const char* StateText(int s)
{
    switch (s)
    {
    case HOOK_INSTALLED: return "ok";
    case HOOK_DISABLED: return "disabled (ini)";
    case HOOK_ERR_UNKNOWN: return "ERROR not a stock function entry";
    case HOOK_ERR_BYTES: return "ERROR stock bytes differ";
    case HOOK_ERR_SPACE: return "ERROR no room for the jump";
    case HOOK_ERR_PROTECT: return "ERROR VirtualProtect";
    case HOOK_ERR_DUPLICATE: return "ERROR duplicate";
    default: return "pending";
    }
}

static bool IsStockImage(HMODULE mod)
{
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)mod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)((const BYTE*)mod + dos->e_lfanew);
    Log("stock NWindow.dll at %08lx: TimeDateStamp %08lx SizeOfCode %08lx Entry %08lx",
        (DWORD)mod, nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfCode,
        nt->OptionalHeader.AddressOfEntryPoint);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.TimeDateStamp == kStockTimeDateStamp &&
           nt->OptionalHeader.SizeOfCode == kStockSizeOfCode &&
           nt->OptionalHeader.AddressOfEntryPoint == kStockEntryPoint;
}

// link time of this DLL (PE TimeDateStamp: seconds since 1970, UTC) in local time; __DATE__
// would only tell when this file was last compiled
static SYSTEMTIME BuildTime(HMODULE self)
{
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)self;
    const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)((const BYTE*)self + dos->e_lfanew);
    ULONGLONG t = (ULONGLONG)nt->FileHeader.TimeDateStamp * 10000000ULL + 116444736000000000ULL;
    FILETIME utc, local;
    utc.dwLowDateTime = (DWORD)t;
    utc.dwHighDateTime = (DWORD)(t >> 32);
    SYSTEMTIME st = {};
    FileTimeToLocalFileTime(&utc, &local);
    FileTimeToSystemTime(&local, &st);
    return st;
}

// Fatal-looking exceptions (access violation, breakpoint, illegal instruction, divide, stack overflow) are logged
// with the registers and the code addresses found on the stack, as module+RVA: the RVA inside NWindowDecomp.dll
// is looked up in build/bin/Release/NWindowDecomp.map (tools/crash_lookup.py). The engine handles some of them
// itself (its first-chance AVs), so this only writes; at most 40 are logged. Not stock behavior.
static void DescribeAddress(DWORD addr, char* out, int outLen)
{
    HMODULE mod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &mod) && mod)
    {
        char path[MAX_PATH] = "?";
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* base = path;
        for (const char* q = path; *q; q++)
            if (*q == '\\' || *q == '/')
                base = q + 1;
        wsprintfA(out, "%s+%06lx", base, addr - (DWORD)mod);
    }
    else
        wsprintfA(out, "%08lx", addr);
    (void)outLen;
}

// The dwords of a stack that point into a module's code: the callers (guessed, not unwound)
static void LogStackCodeAddresses(const char* tag, const DWORD* sp, int words, int maxShown)
{
    int shown = 0;
    for (int i = 0; i < words && shown < maxShown; i++)
    {
        DWORD v;
        __try { v = sp[i]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        MEMORY_BASIC_INFORMATION mbi;
        if (v > 0x10000 && VirtualQuery((LPCVOID)v, &mbi, sizeof(mbi)) &&
            (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
            mbi.Type == MEM_IMAGE)
        {
            char w2[300];
            DescribeAddress(v, w2, sizeof(w2));
            Log("%s   stack+%03x %s", tag, i * 4, w2);
            shown++;
        }
    }
}

// ---- [Debug] Watch*: a hardware write watchpoint on the first dword of an object (NwLog.h) ---------------------------
// Armed from the call logger when WatchFn is called (and WatchRet matches the caller): the vtable pointer of a window
// that is found zeroed later was written by someone, and the write's trap names the writer. Debug registers are per
// thread, so a helper thread suspends the calling thread, sets DR0..DR3 and resumes it. Not stock behavior.
static volatile LONG g_watchHits = 0;
static volatile LONG g_watchArms = 0;
static DWORD g_watchAddr[4] = {};
static int g_watchNext = 0;

struct WatchArm
{
    HANDLE thread;
    DWORD addr;
    int slot;
};

static DWORD WINAPI WatchArmProc(LPVOID param)
{
    WatchArm* a = (WatchArm*)param;
    if (SuspendThread(a->thread) == (DWORD)-1)
        return 1;
    CONTEXT c;
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(a->thread, &c))
    {
        DWORD* dr = &c.Dr0;     // Dr0..Dr3 are consecutive in the x86 CONTEXT
        dr[a->slot] = a->addr;
        c.Dr7 &= ~((3u << (a->slot * 2)) | (0xFu << (16 + a->slot * 4)));
        c.Dr7 |= (1u << (a->slot * 2)) | (0xDu << (16 + a->slot * 4));   // local enable; RW = 01 (write), LEN = 11 (4 bytes)
        c.Dr6 = 0;
        SetThreadContext(a->thread, &c);
    }
    ResumeThread(a->thread);
    return 0;
}

void WatchMaybeArm(DWORD fnVa, DWORD self, DWORD retStockVa)
{
    const Config& cfg = GetConfig();
    if (fnVa != cfg.watchFn || !self || (self & 3))
        return;
    if (cfg.watchRetCount)
    {
        bool ok = false;
        for (int i = 0; i < cfg.watchRetCount; i++)
            if (cfg.watchRet[i] == retStockVa)
                ok = true;
        if (!ok)
            return;
    }
    if (g_watchHits >= (LONG)cfg.watchMax || InterlockedIncrement(&g_watchArms) > 256)
        return;
    int slot = g_watchNext;
    g_watchNext = (g_watchNext + 1) & 3;
    g_watchAddr[slot] = self;
    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, GetCurrentThreadId());
    if (!th)
        return;
    WatchArm a = {th, self, slot};
    HANDLE h = CreateThread(NULL, 0, WatchArmProc, &a, 0, NULL);
    if (h)
    {
        WaitForSingleObject(h, 3000);
        CloseHandle(h);
    }
    CloseHandle(th);
    Log("[watch] armed slot %d on %08lx (call from %08lx)", slot, self, retStockVa);
}

static bool WatchHandle(EXCEPTION_POINTERS* ep)
{
    CONTEXT* c = ep->ContextRecord;
    if (!(c->Dr6 & 0xF))
        return false;
    int slot = 0;
    while (!(c->Dr6 & (1u << slot)))
        slot++;
    LONG n = InterlockedIncrement(&g_watchHits);
    c->Dr6 = 0;
    if (n > (LONG)GetConfig().watchMax)
    {
        c->Dr7 = 0;         // enough: all four off
        return true;
    }
    DWORD addr = g_watchAddr[slot];
    DWORD now = 0;
    __try { now = *(const DWORD*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    char where[300];
    DescribeAddress((DWORD)ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));
    Log("[watch] #%ld write to %08lx (slot %d): now %08lx; next instruction %s (%08lx)", n, addr, slot, now, where,
        (DWORD)ep->ExceptionRecord->ExceptionAddress);
    Log("[watch]   eax %08lx ecx %08lx edx %08lx ebx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx", c->Eax, c->Ecx,
        c->Edx, c->Ebx, c->Esi, c->Edi, c->Ebp, c->Esp);
    LogStackCodeAddresses("[watch]", (const DWORD*)c->Esp, 96, 8);
    DumpTrace("[watch]", 6);
    return true;
}

static LONG CALLBACK NwExceptionLogger(EXCEPTION_POINTERS* ep)
{
    static LONG count = 0;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_SINGLE_STEP && GetConfig().watchFn && WatchHandle(ep))
        return EXCEPTION_CONTINUE_EXECUTION;
    // every error status (0xC...) and the breakpoint; not the C++ throws (0xE06D7363) nor the debugger's names
    if ((code & 0xF0000000) != 0xC0000000 && code != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&count) > 40)
        return EXCEPTION_CONTINUE_SEARCH;
    const CONTEXT* c = ep->ContextRecord;
    char where[300];
    DescribeAddress((DWORD)ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));
    Log("[exception] code %08lx at %s (%08lx) info %08lx %08lx", code, where,
        (DWORD)ep->ExceptionRecord->ExceptionAddress,
        ep->ExceptionRecord->NumberParameters > 0 ? (DWORD)ep->ExceptionRecord->ExceptionInformation[0] : 0,
        ep->ExceptionRecord->NumberParameters > 1 ? (DWORD)ep->ExceptionRecord->ExceptionInformation[1] : 0);
    Log("[exception]   eax %08lx ecx %08lx edx %08lx ebx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx", c->Eax,
        c->Ecx, c->Edx, c->Ebx, c->Esi, c->Edi, c->Ebp, c->Esp);
    LogStackCodeAddresses("[exception]", (const DWORD*)c->Esp, 256, 14);
    if (count <= 6)
        DumpTrace("[exception]", 40);   // the last hooked functions entered (newest first)
    return EXCEPTION_CONTINUE_SEARCH;
}

// A /GS cookie that no longer matches ends the process through __fastfail: no exception is dispatched, so the
// vectored handler never sees it and Windows leaves only "NWindowDecomp.dll at __report_gsfailure". The CRT's
// own __report_gsfailure is patched at startup to jump here, which writes where it came from and then fails
// fast the same way. The failing function is the code address that follows the call of
// __security_check_cookie on the stack. Not stock behavior.
extern "C" void __cdecl NwGsFailureLog(const DWORD* regs)
{
    // pushad order from the lowest address: edi esi ebp esp ebx edx ecx eax, then the return address
    Log("[gsfailure] stack cookie check failed: ecx (the bad cookie) %08lx", regs[6]);
    Log("[gsfailure]   eax %08lx ebx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx", regs[7], regs[4], regs[1],
        regs[0], regs[2], regs[3]);
    LogStackCodeAddresses("[gsfailure]", regs + 8, 96, 14);
    DumpTrace("[gsfailure]", 40);
    __fastfail(2);      // FAST_FAIL_STACK_COOKIE_CHECK_FAILURE
}

extern "C" __declspec(naked) void __cdecl NwReportGsFailure()
{
    __asm
    {
        pushad
        push esp
        call NwGsFailureLog
        int 3
    }
}

extern "C" void __cdecl __report_gsfailure(void);   // the CRT's: its first bytes become a jump to ours

static void HookGsFailure()
{
    BYTE* target = reinterpret_cast<BYTE*>(&__report_gsfailure);
    DWORD old = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old))
        return;
    target[0] = 0xE9;
    *reinterpret_cast<INT*>(target + 1) = reinterpret_cast<BYTE*>(&NwReportGsFailure) - (target + 5);
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
}

static void Startup(HMODULE self)
{
    LogOpen(self);
    LoadConfig(self);
    AddVectoredExceptionHandler(1, NwExceptionLogger);
    HookGsFailure();
    g_buildTime = BuildTime(self);
    Log("NWindowDecomp build %04u-%02u-%02u %02u:%02u:%02u attach (pid %lu)", g_buildTime.wYear,
        g_buildTime.wMonth, g_buildTime.wDay, g_buildTime.wHour, g_buildTime.wMinute,
        g_buildTime.wSecond, GetCurrentProcessId());
    if (GetConfig().debugMarker)
        Log("[Debug] Marker=1: proof marks on screen (re/NwDebug.h)");

    HMODULE stock = GetModuleHandleW(L"NWindow.dll");
    if (!stock)
    {
        Log("NWindow.dll is not loaded: nothing to do (load NWindowDecomp.dll through the patched NWindow.dll)");
        return;
    }
    if (!IsStockImage(stock))
    {
        Log("NWindow.dll is not the stock C2 build this project decompiles: no hook installed");
        return;
    }
    g_nwBase = (DWORD)stock;

    int total = 0;
    for (HookEntry* h = FirstHook(); h; h = h->next)
        total++;
    if (!GetConfig().hooksEnabled)
        Log("[Hooks] Enabled=0: running the stock code only");
    int installed = InstallHooks();
    g_hooksInstalled = installed;
    if (GetConfig().logCalls[0])
        Log("[Debug] LogCalls=%s: %d stock functions get a logging detour (hooked ones log from their stub)",
            GetConfig().logCalls, InstallCallLogs(self));

    int errors = 0;
    for (HookEntry* h = FirstHook(); h; h = h->next)
    {
        if (h->state < 0)
            errors++;
        // "ours=" is the RVA of the replacement in NWindowDecomp.dll (tools/verify_hooks.py)
        Log("  %08lx %-48s ours=%06lx %s%s", h->va, h->name, (DWORD)h->fn - (DWORD)self,
            StateText(h->state), h->state == HOOK_INSTALLED && !h->trampoline ? " (no trampoline)" : "");
    }
    Log("hooks: %d installed, %d registered, %d errors", installed, total, errors);

    // vtables compiled here, slot by slot: stock VA of the target / RVA in this DLL
    for (VtableCheck* v = FirstVtable(); v; v = v->next)
    {
        const DWORD* stock = (const DWORD*)Addr(v->stockVA);
        for (int k = 0; k < v->slots; k++)
            Log("  vt %s %d stock=%08lx ours=%06lx", v->name, k, stock[k] - g_nwBase + kStockBase,
                (DWORD)v->ours[k] - (DWORD)self);
    }
}

}  // namespace nw

// Imported by the patched stock NWindow.dll only to make the loader bring us in.
extern "C" __declspec(dllexport) void __cdecl NWindowDecomp_Attach() {}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(inst);
        nw::Startup(inst);
    }
    return TRUE;
}
