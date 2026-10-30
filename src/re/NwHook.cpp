#include "re/NwHook.h"

#include "re/NwLog.h"
#include "re/NwPrologue.h"

#include <stdlib.h>
#include <string.h>

extern "C" DWORD g_nwBase = 0;

namespace nw {

static HookEntry* g_first = NULL;

HookReg::HookReg(DWORD va, void* fn, const char* name, const char* file)
{
    // runs during our static initialization; no allocation, the list is intrusive
    e.va = va;
    e.fn = fn;
    e.name = name;
    e.file = file;
    e.state = HOOK_PENDING;
    e.trampoline = NULL;
    e.next = g_first;
    g_first = &e;
}

HookEntry* FirstHook() { return g_first; }

static VtableCheck* g_firstVtable = NULL;

VtableReg::VtableReg(const char* name, const void* const* ours, DWORD stockVA, int slots)
{
    v.name = name;
    v.ours = ours;
    v.stockVA = stockVA;
    v.slots = slots;
    v.next = g_firstVtable;
    g_firstVtable = &v;
}

VtableCheck* FirstVtable() { return g_firstVtable; }

const HookEntry* FindHook(DWORD va)
{
    for (HookEntry* h = g_first; h; h = h->next)
        if (h->va == va && h->state == HOOK_INSTALLED)
            return h;
    return NULL;
}

static const Prologue* FindPrologue(DWORD va)
{
    unsigned lo = 0, hi = kPrologueCount;
    while (lo < hi)
    {
        unsigned mid = (lo + hi) / 2;
        if (kPrologues[mid].va < va)
            lo = mid + 1;
        else
            hi = mid;
    }
    return (lo < kPrologueCount && kPrologues[lo].va == va) ? &kPrologues[lo] : NULL;
}

// One block of executable memory holds every trampoline (<= 3561 x ~16 bytes).
static BYTE* g_pool = NULL;
static DWORD g_poolUsed = 0;
static const DWORD kPoolSize = 0x100000;

static BYTE* AllocExec(DWORD n)
{
    if (!g_pool)
        g_pool = (BYTE*)VirtualAlloc(NULL, kPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_pool || g_poolUsed + n > kPoolSize)
        return NULL;
    BYTE* p = g_pool + g_poolUsed;
    g_poolUsed += (n + 15) & ~15u;
    return p;
}

static bool StockBytesMatch(const Prologue& p, const BYTE* code)
{
    for (int i = 0; i < 8; i++)
        if (!(p.relmask & (1 << i)) && code[i] != p.expect[i])
            return false;
    return true;
}

// Copy of the stolen instructions + jmp back, so the stock implementation stays callable.
static BYTE* MakeTrampoline(const Prologue& p, const BYTE* code)
{
    if (p.flags & PF_NOTRAMP)
        return NULL;
    BYTE* t = AllocExec(p.steal + 5);
    if (!t)
        return NULL;
    memcpy(t, code, p.steal);
    if (p.flags & PF_REL32)
    {
        // call/jmp rel32 (E8/E9) or jcc rel32 (0F 8x): keep the absolute target
        int opLen = (code[p.relat] == 0x0F) ? 2 : 1;
        const BYTE* insEnd = code + p.relat + opLen + 4;
        const BYTE* target = insEnd + *(const INT*)(code + p.relat + opLen);
        *(INT*)(t + p.relat + opLen) = (INT)(target - (t + p.relat + opLen + 4));
    }
    if (!(p.flags & PF_ENDS))
    {
        t[p.steal] = 0xE9;
        *(INT*)(t + p.steal + 1) = (INT)((code + p.steal) - (t + p.steal + 5));
    }
    return t;
}

// ---- [Debug] LogCalls: every call of the chosen functions goes to the log -------------------------------------------
// Hooked functions get the logging code in front of their entry stub; stock functions nobody ported get a logging-only
// detour (their trampoline is the continuation). The names of stock functions come from NWindowDecomp.names (written
// beside the dll by tools/install_test.py: `va name` per line).
struct LogRec
{
    DWORD va;
    DWORD count;
    char name[104];
};

static void __cdecl LogCallEntry(LogRec* r, DWORD self, const DWORD* frame)
{
    DWORD ret = frame[0];
    if (ret - g_nwBase < 0x200000)
        ret = ret - g_nwBase + kStockBase;
    if (GetConfig().watchFn)
        WatchMaybeArm(r->va, self, ret);
    DWORD n = ++r->count;
    DWORD cap = GetConfig().logMax;
    if (n > cap)
    {
        if (n == cap + 1)
            Log("[call] %s: further calls are not logged (LogMax=%lu)", r->name, cap);
        return;
    }
    Log("[call] %08lx %s this=%08lx ret=%08lx args=%08lx %08lx %08lx %08lx %08lx", r->va, r->name, self, ret, frame[1],
        frame[2], frame[3], frame[4], frame[5]);
}

static bool LogPatternMatches(DWORD va, const char* name)
{
    const char* p = GetConfig().logCalls;
    char tok[128];
    while (*p)
    {
        while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
            p++;
        int n = 0;
        while (*p && *p != ' ' && *p != ',' && *p != ';' && *p != '\t' && n < (int)sizeof(tok) - 1)
            tok[n++] = *p++;
        tok[n] = 0;
        if (!n)
            continue;
        const char* hex = (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) ? tok + 2 : tok;
        char* end = NULL;
        unsigned long v = strtoul(hex, &end, 16);
        if (end && *end == 0 && end != hex && n <= 10 && v == va)
            return true;
        if (!name)
            continue;
        if (tok[n - 1] == '*')
        {
            if (strncmp(tok, name, (size_t)(n - 1)) == 0)
                return true;
        }
        else if (lstrcmpA(tok, name) == 0)
            return true;
    }
    return false;
}

static LogRec* NewLogRec(DWORD va, const char* name)
{
    LogRec* r = (LogRec*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(LogRec));
    if (!r)
        return NULL;
    r->va = va;
    lstrcpynA(r->name, name ? name : "?", sizeof(r->name));
    return r;
}

void TraceCall(const char* name, const void* self, int a0, int a1, int a2)
{
    if (!GetConfig().logCalls[0])
        return;
    struct Slot
    {
        const char* name;   // the literal's address identifies the call site's function
        DWORD count;
        bool wanted;
    };
    static Slot tab[256];
    int i = 0;
    while (i < 256 && tab[i].name && tab[i].name != name)
        i++;
    if (i == 256)
        return;
    if (!tab[i].name)
    {
        tab[i].name = name;
        tab[i].wanted = LogPatternMatches(0, name);
    }
    if (!tab[i].wanted)
        return;
    DWORD n = ++tab[i].count;
    if (n > GetConfig().logMax)
        return;
    Log("[trace] %s this=%08lx args=%08lx %08lx %08lx", name, (DWORD)self, (DWORD)a0, (DWORD)a1, (DWORD)a2);
}

// pushad / pushfd / lea eax,[esp+0x24] / push eax / push ecx / push rec / call LogCallEntry / add esp,12 / popfd / popad
static BYTE* EmitLogCall(BYTE* q, LogRec* r)
{
    *q++ = 0x60;
    *q++ = 0x9C;
    *q++ = 0x8D; *q++ = 0x44; *q++ = 0x24; *q++ = 0x24;
    *q++ = 0x50;
    *q++ = 0x51;
    *q++ = 0x68;  *(DWORD*)q = (DWORD)r;  q += 4;
    *q++ = 0xE8;  *(INT*)q = (INT)((BYTE*)&LogCallEntry - (q + 4));  q += 4;
    *q++ = 0x83; *q++ = 0xC4; *q++ = 0x0C;
    *q++ = 0x9D;
    *q++ = 0x61;
    return q;
}

int InstallHooks()
{
    int installed = 0;
    for (HookEntry* h = g_first; h; h = h->next)
    {
        if (h->state != HOOK_PENDING)
            continue;
        for (HookEntry* o = h->next; o; o = o->next)
            if (o->va == h->va)
            {
                o->state = HOOK_ERR_DUPLICATE;
                Log("  ERROR duplicate hook %08lx: %s (%s) and %s (%s)", h->va, h->name, h->file, o->name, o->file);
            }
        if (!GetConfig().hooksEnabled || IsDisabledByConfig(h->va, h->name))
        {
            h->state = HOOK_DISABLED;
            continue;
        }
        const Prologue* p = FindPrologue(h->va);
        if (!p)
        {
            h->state = HOOK_ERR_UNKNOWN;
            continue;
        }
        BYTE* code = (BYTE*)Addr(h->va);
        if (!StockBytesMatch(*p, code))
        {
            h->state = HOOK_ERR_BYTES;
            continue;
        }
        if (p->room < 5)
        {
            h->state = HOOK_ERR_SPACE;
            continue;
        }
        h->trampoline = MakeTrampoline(*p, code);
        DWORD old;
        if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old))
        {
            h->state = HOOK_ERR_PROTECT;
            continue;
        }
        BYTE* target = (BYTE*)h->fn;
        LogRec* logRec = GetConfig().logCalls[0] && LogPatternMatches(h->va, h->name) ? NewLogRec(h->va, h->name) : NULL;
        if (GetConfig().traceHooks || logRec)
        {
            // push eax / mov eax,[seq] / inc eax / mov [seq],eax / and eax,0xFF / mov [ring+eax*4],h / pop eax / jmp ours
            BYTE* s = AllocExec(112);
            if (s)
            {
                BYTE* q = s;
                if (logRec)
                    q = EmitLogCall(q, logRec);
                if (GetConfig().traceHooks)
                {
                    *q++ = 0x50;
                    *q++ = 0xA1;  *(DWORD*)q = (DWORD)&g_nwTraceSeq;  q += 4;
                    *q++ = 0x40;
                    *q++ = 0xA3;  *(DWORD*)q = (DWORD)&g_nwTraceSeq;  q += 4;
                    *q++ = 0x25;  *(DWORD*)q = 0xFF;                  q += 4;
                    *q++ = 0xC7;  *q++ = 0x04;  *q++ = 0x85;
                    *(DWORD*)q = (DWORD)g_nwTraceRing;                q += 4;
                    *(DWORD*)q = (DWORD)h;                            q += 4;
                    *q++ = 0x58;
                }
                *q++ = 0xE9;  *(INT*)q = (INT)((BYTE*)h->fn - (q + 4));  q += 4;
                target = s;
            }
        }
        code[0] = 0xE9;
        *(INT*)(code + 1) = (INT)(target - (code + 5));
        VirtualProtect(code, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), code, 5);
        h->state = HOOK_INSTALLED;
        installed++;
    }
    return installed;
}

extern "C" DWORD g_nwTraceRing[256];
extern "C" volatile DWORD g_nwTraceSeq;
DWORD g_nwTraceRing[256];
volatile DWORD g_nwTraceSeq = 0;

void DumpTrace(const char* tag, int count)
{
    DWORD seq = g_nwTraceSeq;
    if (count > 256)
        count = 256;
    for (int i = 0; i < count; i++)
    {
        const HookEntry* h = (const HookEntry*)g_nwTraceRing[(seq - i) & 255];
        if (!h)
            break;
        Log("%s   trace-%02d %08lx %s", tag, i, h->va, h->name);
    }
}

// Logging-only detours on stock functions nobody ported (the ones LogCalls names that are not hooks).
int InstallCallLogs(HMODULE self)
{
    if (!GetConfig().logCalls[0])
        return 0;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(self, path, MAX_PATH);
    char* slash = n ? strrchr(path, '\\') : NULL;
    if (!slash)
        return 0;
    lstrcpynA(slash + 1, "NWindowDecomp.names", (int)(MAX_PATH - (slash + 1 - path)));
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
    {
        Log("[Debug] LogCalls: NWindowDecomp.names not found beside the dll (only hooked functions are logged)");
        return 0;
    }
    DWORD size = GetFileSize(f, NULL), got = 0;
    char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, size + 1);
    if (!buf || !ReadFile(f, buf, size, &got, NULL))
    {
        CloseHandle(f);
        return 0;
    }
    CloseHandle(f);
    buf[got] = 0;
    int logged = 0;
    for (char* line = buf; *line;)
    {
        char* eol = line;
        while (*eol && *eol != '\n')
            eol++;
        char* next = *eol ? eol + 1 : eol;
        if (eol > line && eol[-1] == '\r')
            eol--;
        *eol = 0;
        char* end = NULL;
        unsigned long va = strtoul(line, &end, 16);
        if (end && *end == ' ' && va && LogPatternMatches(va, end + 1))
        {
            bool hooked = false;
            for (HookEntry* h = g_first; h; h = h->next)
                if (h->va == va && h->state == HOOK_INSTALLED)
                    hooked = true;
            const Prologue* p = hooked ? NULL : FindPrologue(va);
            BYTE* code = (BYTE*)Addr(va);
            if (p && p->room >= 5 && StockBytesMatch(*p, code))
            {
                BYTE* tramp = MakeTrampoline(*p, code);
                LogRec* r = NewLogRec(va, end + 1);
                BYTE* s = tramp && r ? AllocExec(64) : NULL;
                DWORD old;
                if (s && VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old))
                {
                    BYTE* q = EmitLogCall(s, r);
                    *q++ = 0xE9;  *(INT*)q = (INT)(tramp - (q + 4));  q += 4;
                    code[0] = 0xE9;
                    *(INT*)(code + 1) = (INT)(s - (code + 5));
                    VirtualProtect(code, 5, old, &old);
                    FlushInstructionCache(GetCurrentProcess(), code, 5);
                    logged++;
                }
            }
        }
        line = next;
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return logged;
}

void* Stock(DWORD va)
{
    const HookEntry* h = FindHook(va);
    if (h)
        return h->trampoline;  // NULL when the prologue could not be relocated
    return Addr(va);
}

}  // namespace nw
