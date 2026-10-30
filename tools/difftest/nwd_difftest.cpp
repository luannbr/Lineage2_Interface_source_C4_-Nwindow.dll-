// nwd_difftest - the differential test runner (see difftest.h and tools/difftest.py).
//
//   nwd_difftest.exe <System folder> <output folder> [--case NAME]... [--verbose]
//                    [--pages NAME=DIR]... [--only PAGE]... [--list]
//
// The System folder holds the patched stock NWindow.dll (it imports NWindowDecomp.dll) and the
// engine DLLs; its NWindowDecomp.ini decides which code runs. Each case writes
// <output folder>\<case>.txt; "--pages NAME=DIR" adds the case pages_NAME (the *.htm under DIR).
//
// "--standalone TABLE": the System's NWindow.dll is our standalone build (phase 5) and TABLE
// ("va rva name" lines, tools/difftest.py) says where each stock address the cases use went.
// A case that needs an address the table lacks (a function the standalone inlined away) stops
// there and writes "SKIPPED: ..." instead of its results.
// Exit code 0, 2 on a bad command line or System, 3 when a case crashed.
#include "difftest.h"

#include <stdarg.h>
#include <dbghelp.h>
#include <algorithm>
#include <map>

#pragma comment(lib, "dbghelp.lib")

namespace dt {

bool Verbose = false;
BYTE* Base = NULL;
static BYTE* Decomp = NULL;
static std::map<DWORD, DWORD> OurRva;     // stock va -> our replacement's RVA
static FILE* Out = NULL;
static std::string CurrentCase;

// --standalone: stock va -> RVA in our NWindow.dll, and what the table calls it
static bool Standalone = false;
static std::map<DWORD, DWORD> StandaloneRva;
static const DWORD DT_EXCEPTION_MISSING = 0xE0445401;   // an address the standalone lacks
static DWORD MissingVa = 0;

/*-----------------------------------------------------------------------------
    Output.
-----------------------------------------------------------------------------*/

void Print(const char* Fmt, ...)
{
    va_list Ap;
    va_start(Ap, Fmt);
    vfprintf(Out, Fmt, Ap);
    va_end(Ap);
}

std::string Esc(const wchar_t* Text, int Len, int Max)
{
    std::string S;
    if (!Text)
        return "(null)";
    for (int i = 0; (Len < 0 ? Text[i] != 0 : i < Len) && i < Max; i++)
    {
        wchar_t C = Text[i];
        char Buf[16];
        if (C >= 32 && C < 127 && C != '\\' && C != '"')
            S += (char)C;
        else
        {
            sprintf(Buf, C < 0x100 ? "\\x%02x" : "\\u%04x", (unsigned)C);
            S += Buf;
        }
    }
    return S;
}

std::string EscA(const char* Text, int Len)
{
    std::string S;
    if (!Text)
        return "(null)";
    for (int i = 0; Len < 0 ? Text[i] != 0 : i < Len; i++)
    {
        unsigned char C = (unsigned char)Text[i];
        char Buf[8];
        if (C >= 32 && C < 127 && C != '\\' && C != '"')
            S += (char)C;
        else
        {
            sprintf(Buf, "\\x%02x", C);
            S += Buf;
        }
    }
    return S;
}

void PrintW(const wchar_t* Text, int Max)
{
    fputs(Esc(Text, -1, Max).c_str(), Out);
}

unsigned Hash32(const std::string& Text)
{
    unsigned H = 2166136261u;
    for (unsigned char C : Text)
    {
        H ^= C;
        H *= 16777619u;
    }
    return H;
}

std::string Hex32(unsigned V)
{
    char Buf[16];
    sprintf(Buf, "%08x", V);
    return Buf;
}

void Hashed::Flush(const std::string& Label)
{
    if (Verbose)
        Print("%s\n%s", Label.c_str(), Text.c_str());
    else
        Print("%s h=%s\n", Label.c_str(), Hex32(Hash32(Text)).c_str());
    Text.clear();
}

/*-----------------------------------------------------------------------------
    The images.
-----------------------------------------------------------------------------*/

void* Stock(DWORD Va)
{
    if (!Standalone)
        return Base + (Va - 0x10000000);
    std::map<DWORD, DWORD>::iterator It = StandaloneRva.find(Va);
    if (It == StandaloneRva.end())
    {
        MissingVa = Va;
        RaiseException(DT_EXCEPTION_MISSING, 0, 0, NULL);   // the case stops (RunCase)
    }
    return Base + It->second;
}

// "va rva name" lines (tools/difftest.py --standalone)
static bool ReadStandaloneTable(const wchar_t* Path)
{
    FILE* F = _wfopen(Path, L"rb");
    if (!F)
        return false;
    char Line[2048];
    while (fgets(Line, sizeof(Line), F))
    {
        unsigned Va, Rva;
        if (sscanf(Line, "%x %x", &Va, &Rva) == 2)
            StandaloneRva[Va] = Rva;
    }
    fclose(F);
    return true;
}

void* Ours(DWORD Va)
{
    std::map<DWORD, DWORD>::iterator It = OurRva.find(Va);
    return It == OurRva.end() || !Decomp ? NULL : Decomp + It->second;
}

void Patch(void* At, void* To)
{
    BYTE* P = (BYTE*)At;
    DWORD Old;
    VirtualProtect(P, 5, PAGE_EXECUTE_READWRITE, &Old);
    P[0] = 0xE9;
    *(DWORD*)(P + 1) = (DWORD)((BYTE*)To - (P + 5));
    VirtualProtect(P, 5, Old, &Old);
    FlushInstructionCache(GetCurrentProcess(), P, 5);
}

void* OurSymbol(const char* Name)
{
    static bool bLoaded = false;
    HANDLE P = GetCurrentProcess();
    if (!Decomp)
        return NULL;
    if (!bLoaded)
    {
        wchar_t Path[MAX_PATH];
        GetModuleFileNameW((HMODULE)Decomp, Path, MAX_PATH);
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
        SymInitialize(P, NULL, FALSE);
        SymLoadModuleExW(P, NULL, Path, NULL, (DWORD64)(DWORD_PTR)Decomp, 0, NULL, 0);
        bLoaded = true;
    }
    BYTE Buf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* S = (SYMBOL_INFO*)Buf;
    memset(Buf, 0, sizeof(Buf));
    S->SizeOfStruct = sizeof(SYMBOL_INFO);
    S->MaxNameLen = 500;
    if (!SymFromName(P, Name, S))
        return NULL;
    return (void*)(DWORD_PTR)S->Address;
}

bool PatchBoth(DWORD Va, void* To, const char* Name)
{
    Patch(Stock(Va), To);
    if (Standalone)
        return true;   // Stock(Va) is the method itself: every caller reaches the patch
    if (void* O = Ours(Va))
        Patch(O, To);
    if (!Name)
        return true;
    void* Method = OurSymbol(Name);
    if (!Method)
    {
        Print("symbol %s not found in NWindowDecomp.pdb\n", Name);
        return false;
    }
    Patch(Method, To);
    return true;
}

// "  1002bd60 NCHtmlViewer::LoadHtml    ours=045b70 ok": the replacements, installed or not
static void ReadHookLog(const wchar_t* System)
{
    std::wstring Path = std::wstring(System) + L"\\NWindowDecomp.log";
    FILE* F = _wfopen(Path.c_str(), L"rb");
    if (!F)
        return;
    char Line[1200];
    while (fgets(Line, sizeof(Line), F))
    {
        unsigned Va, Rva;
        char* P = strstr(Line, " ours=");
        if (!P || sscanf(P + 6, "%x", &Rva) != 1)
            continue;
        char* Q = Line;
        while (*Q && *Q != ' ')
            Q++;        // the time
        if (sscanf(Q, " %x", &Va) == 1 && Va >= 0x10000000 && Va < 0x10200000)
            OurRva[Va] = Rva;
    }
    fclose(F);
}

/*-----------------------------------------------------------------------------
    Memory.
-----------------------------------------------------------------------------*/

static std::string Allocs;

struct FTestMalloc
{
    virtual void* Malloc(DWORD Count, const wchar_t* Tag)
    {
        char Buf[32];
        sprintf(Buf, "m%lu ", Count);
        Allocs += Buf;
        void* P = HeapAlloc(GetProcessHeap(), 0, Count ? Count : 1);
        memset(P, 0xCD, Count);   // never zero: what the code forgets to set shows
        return P;
    }
    virtual void* Realloc(void* Original, DWORD Count, const wchar_t* Tag)
    {
        char Buf[32];
        sprintf(Buf, "r%lu ", Count);
        Allocs += Buf;
        if (!Original)
        {
            void* P = HeapAlloc(GetProcessHeap(), 0, Count ? Count : 1);
            memset(P, 0xCD, Count);
            return P;
        }
        if (!Count)
        {
            HeapFree(GetProcessHeap(), 0, Original);
            return NULL;
        }
        return HeapReAlloc(GetProcessHeap(), 0, Original, Count);
    }
    virtual void Free(void* Original)
    {
        Allocs += Original ? "f " : "f0 ";
        if (Original)
            HeapFree(GetProcessHeap(), 0, Original);
    }
    virtual void DumpAllocs() {}
    virtual void HeapCheck() {}
    virtual void Init(int) {}
    virtual void Exit() {}
};
static FTestMalloc GTestMalloc;

std::string TakeAllocs()
{
    std::string S = Allocs;
    if (!S.empty())
        S.pop_back();
    Allocs.clear();
    return S;
}
void ClearAllocs()
{
    Allocs.clear();
}
void* TestMalloc(DWORD Size)
{
    return GTestMalloc.Malloc(Size, L"test");
}
void TestFree(void* P)
{
    GTestMalloc.Free(P);
}

/*-----------------------------------------------------------------------------
    Engine types.
-----------------------------------------------------------------------------*/

std::string Show(const FStr& S)
{
    char Buf[32];
    sprintf(Buf, "%d/%d\"", S.Num, S.Max);
    return Buf + Esc(S.Data, S.Num > 0 ? S.Num - 1 : 0) + "\"";
}

void Set(FStr& S, const wchar_t* Text)
{
    typedef void*(__fastcall * AssignFn)(void*, void*, const wchar_t*);
    static AssignFn Fn = (AssignFn)GetProcAddress(GetModuleHandleW(L"Core.dll"),
                                                  "??4FString@@QAEAAV0@PBG@Z");
    Fn(&S, 0, Text);
}

/*-----------------------------------------------------------------------------
    Exceptions of the stock library.
-----------------------------------------------------------------------------*/

static char GType[256];
static char GWhat[1024];

// the type from the throw information, the message of a logic_error (its std::string at +0xC)
// The standalone NWindow.dll throws its own std exceptions (include/nwindow/NStdLib.h, the
// !NW_HOOK_MODE branch): same layout, message at +0xC, but its type names are our structs'.
// Only catch(...) ever sees them (ours and UE2's guard/unguard), so the type is compared by the
// name the stock library gave it.
static const char* StockTypeName(const char* Name)
{
    static const char* const Names[][2] = {
        {".?AUNStdOutOfRange@@", ".?AVout_of_range@std@@"},
        {".?AUNStdLengthError@@", ".?AVlength_error@std@@"},
        {".?AUNStdLogicError@@", ".?AVlogic_error@std@@"},
        {".?AUNStdException@@", ".?AVexception@@"},
    };
    if (Standalone)
        for (const auto& N : Names)
            if (!strcmp(Name, N[0]))
                return N[1];
    return Name;
}

static void Describe(const EXCEPTION_RECORD* R)
{
    GType[0] = 0;
    GWhat[0] = 0;
    if (R->ExceptionCode != 0xE06D7363 || R->NumberParameters < 3)
    {
        sprintf(GType, "SEH %08lx", R->ExceptionCode);
        return;
    }
    BYTE* Object = (BYTE*)R->ExceptionInformation[1];
    DWORD* ThrowInfo = (DWORD*)R->ExceptionInformation[2];
    const char* Name = "?";
    if (ThrowInfo)
    {
        DWORD* Types = (DWORD*)ThrowInfo[3];
        if (Types && Types[0] > 0)
        {
            DWORD* Catchable = (DWORD*)Types[1];
            Name = StockTypeName((const char*)(Catchable[1] + 8));
        }
    }
    strncpy(GType, Name, sizeof(GType) - 1);
    if (Object && (strstr(Name, "error@std") || strstr(Name, "out_of_range@std")))
    {
        BYTE* Str = Object + 0xC;
        DWORD Size = *(DWORD*)(Str + 0x14);
        DWORD Res = *(DWORD*)(Str + 0x18);
        const char* Text = Res >= 16 ? *(const char**)(Str + 4) : (const char*)(Str + 4);
        if (Size < sizeof(GWhat))
        {
            memcpy(GWhat, Text, Size);
            GWhat[Size] = 0;
        }
    }
}

static int Filter(EXCEPTION_POINTERS* E)
{
    if (E->ExceptionRecord->ExceptionCode == DT_EXCEPTION_MISSING)
        return EXCEPTION_CONTINUE_SEARCH;   // not the code under test: the whole case stops
    Describe(E->ExceptionRecord);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int TryInner(void (*Fn)(void*), void* Arg)
{
    __try
    {
        Fn(Arg);
        return 0;
    }
    __except (Filter(GetExceptionInformation()))
    {
        return 1;
    }
}

Caught TryRaw(void (*Fn)(void*), void* Arg)
{
    GType[0] = 0;
    GWhat[0] = 0;
    int Thrown = TryInner(Fn, Arg);
    Caught C;
    C.bThrown = Thrown != 0;
    C.Type = GType;
    C.What = GWhat;
    return C;
}

std::string Show(const Caught& C)
{
    if (!C.bThrown)
        return "no exception";
    return "throws " + C.Type + (C.What.empty() ? "" : " \"" + C.What + "\"");
}

/*-----------------------------------------------------------------------------
    Cases.
-----------------------------------------------------------------------------*/

static std::vector<Case>& Cases()
{
    static std::vector<Case> List;
    return List;
}
void Register(const Case& C)
{
    Cases().push_back(C);
}

std::string Where;

// runs a case; 1 when it needed an address the standalone table lacks
static int RunCase(const Case& C)
{
    __try
    {
        C.Run(C.Args);
        return 0;
    }
    __except (GetExceptionCode() == DT_EXCEPTION_MISSING ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return 1;
    }
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* E)
{
    Describe(E->ExceptionRecord);
    std::string Text = std::string("CRASH ") + GType + (GWhat[0] ? std::string(" \"") + GWhat + "\"" : "") +
                       " in case " + CurrentCase + (Where.empty() ? "" : " at " + Where);
    if (Out)
    {
        fprintf(Out, "%s\n", Text.c_str());
        fclose(Out);
    }
    fprintf(stderr, "%s\n", Text.c_str());
    fflush(stderr);
    ExitProcess(3);
}

}  // namespace dt

using namespace dt;

int wmain(int argc, wchar_t** argv)
{
    std::vector<std::wstring> Selected;
    bool bList = false;
    std::vector<std::wstring> Pos;
    for (int i = 1; i < argc; i++)
    {
        std::wstring A = argv[i];
        if (A == L"--verbose")
            Verbose = true;
        else if (A == L"--list")
            bList = true;
        else if (A == L"--case" && i + 1 < argc)
            Selected.push_back(argv[++i]);
        else if (A == L"--pages" && i + 1 < argc)
        {
            std::wstring Set = argv[++i];
            size_t Eq = Set.find(L'=');
            if (Eq == std::wstring::npos || !Eq)
            {
                fwprintf(stderr, L"--pages NAME=DIR, not %ls\n", Set.c_str());
                return 2;
            }
            std::string Name = "pages_";
            for (size_t k = 0; k < Eq; k++)
                Name += (char)Set[k];
            Register({Name, RunPages, "HTML tokenizer: every *.htm under a folder (hashes only)",
                      {Set.substr(Eq + 1)}});
        }
        else if (A == L"--only" && i + 1 < argc)
            OnlyPages.push_back(argv[++i]);
        else if (A == L"--standalone" && i + 1 < argc)
        {
            Standalone = true;
            if (!ReadStandaloneTable(argv[++i]))
            {
                fwprintf(stderr, L"cannot read the standalone table %ls\n", argv[i]);
                return 2;
            }
        }
        else
            Pos.push_back(A);
    }
    std::sort(Cases().begin(), Cases().end(),
              [](const Case& A, const Case& B) { return A.Name < B.Name; });
    if (bList)
    {
        for (const Case& C : Cases())
            printf("%-16s %s\n", C.Name.c_str(), C.About);
        return 0;
    }
    if (Pos.size() < 2)
    {
        fwprintf(stderr, L"usage: nwd_difftest <System folder> <output folder> [--case NAME]... "
                         L"[--verbose] [--pages NAME=DIR]... [--only PAGE]... [--standalone TABLE] [--list]\n");
        return 2;
    }
    const std::wstring& System = Pos[0];
    const std::wstring& OutDir = Pos[1];
    if (!SetCurrentDirectoryW(System.c_str()) || !SetDllDirectoryW(System.c_str()))
    {
        fwprintf(stderr, L"bad System folder %ls\n", System.c_str());
        return 2;
    }
    HMODULE Nw = LoadLibraryW(L"NWindow.dll");
    if (!Nw)
    {
        fwprintf(stderr, L"LoadLibrary(NWindow.dll) failed: %lu\n", GetLastError());
        return 2;
    }
    Base = (BYTE*)Nw;
    if (Standalone)
        Decomp = Base;   // our code is NWindow.dll itself (OurSymbol reads its PDB)
    else
    {
        Decomp = (BYTE*)GetModuleHandleW(L"NWindowDecomp.dll");
        ReadHookLog(System.c_str());
    }
    void** GMallocPtr = (void**)GetProcAddress(GetModuleHandleW(L"Core.dll"), "?GMalloc@@3PAVFMalloc@@A");
    *GMallocPtr = &GTestMalloc;
    SetUnhandledExceptionFilter(Crash);
    CreateDirectoryW(OutDir.c_str(), NULL);

    int Ran = 0, Skipped = 0;
    for (const Case& C : Cases())
    {
        std::wstring W(C.Name.begin(), C.Name.end());
        if (!Selected.empty() && std::find(Selected.begin(), Selected.end(), W) == Selected.end())
            continue;
        std::wstring Path = OutDir + L"\\" + W + L".txt";
        Out = _wfopen(Path.c_str(), L"wb");
        if (!Out)
        {
            fwprintf(stderr, L"cannot write %ls\n", Path.c_str());
            return 2;
        }
        CurrentCase = C.Name;
        Where.clear();
        ClearAllocs();
        if (RunCase(C))
        {
            // what the case wrote so far is dropped: the file says why it did not run
            fclose(Out);
            Out = _wfopen(Path.c_str(), L"wb");
            fprintf(Out, "SKIPPED: stock %08lx has no function in the standalone NWindow.dll\n", MissingVa);
            Skipped++;
        }
        fclose(Out);
        Out = NULL;
        Ran++;
    }
    if (Skipped)
        printf("%d case(s), %d skipped\n", Ran, Skipped);
    else
        printf("%d case(s)\n", Ran);
    fflush(stdout);
    // no FreeLibrary: unloading the engine DLLs outside the game is not supported
    ExitProcess(0);
}
