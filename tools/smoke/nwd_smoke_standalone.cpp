// nwd_smoke --standalone: loads a System folder's NWindow.dll the way the engine does, with no
// NWindowDecomp.dll and no game (tools/smoke_standalone.py; phase 5, docs/notas/Standalone.md,
// docs/PLANO_C2.md).
//
// The engine never imports NWindow.dll. When Engine.dll asks Core for NWindow.ConsoleWnd and
// NWindow.L2Font (StaticLoadClass of "ini:Engine.Engine.L2Console" / "ini:Engine.Engine.L2Font"),
// Core creates the package "NWindow", UPackage::Bind loads NWindow.dll with appGetDllHandle and
// takes its exports with appGetDllExport; the static initializers of the DLL queued its UClass
// objects (178 in the C1 build, 225 in C2, 341 in C4) in UObject::GAutoRegister, and
// UObject::ProcessRegistrants registers them.
// Each stage below runs one step of that path with Core's own functions, under SEH:
//
//   bind        appGetDllHandle("NWindow.dll") (Core.dll loaded first, as by L2.exe)
//   dllmain     the DLL's DllMain kept its module handle (--instance-rva: GNWindowInstance)
//   exports     every export found by name through appGetDllExport
//   queue       the package's UClass objects waiting in GAutoRegister (their contents: difftest
//               case "classes")
//   init        the part of appInit the registration needs: GMalloc, GLog, GError, GWarn and a
//               GConfig that only knows the two L2.ini keys, FName::StaticInit, then
//               UObject::StaticInit, which runs ProcessRegistrants (Core, Engine and NWindow),
//               and GSys (a USystem without search paths)
//   registered  each class as registered: name, package, parent, flags, properties size
//   load        StaticLoadClass of the two classes with the engine's own arguments
//   construct   StaticConstructObject of both, as Engine.dll does right after loading them
//               (UGameEngine::Init for the console, UCanvas::Init for the font): our
//               InternalConstructor and C++ constructor run
//
// Where it stops: the engine's next calls are Console->Init(UGameEngine*) (with the engine being
// built: client, viewport, audio, network handler) and NL2Font::Load of a font texture (packages
// read from disk through GFileManager and GSys->Paths). Both need the game.
//
// Nothing here prints an address: the output of the stock NWindow.dll and of ours must be the
// same text (tools/smoke_standalone.py runs both and compares). Core's log lines go to stdout
// prefixed "log| ".
//
// C4: the stand-ins below (FMalloc, FOutputDevice, FConfigCache vtables) are the C1 ones and serve the C4 Core.dll
// (27/05/2006) as they are; the UObject field offsets are the C1 ones up to +0x24, but UObject grew to 0x34 in C4, so
// UField::SuperField sits at +0x34 (SuperOff below; +0x28 is a field Core sets to -1 until the object is registered).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <ctype.h>
#include <psapi.h>
#include <float.h>
#include <math.h>
#include <stdint.h>


namespace {

typedef wchar_t TCHAR_;   // Core's TCHAR (unsigned short), same bits

// ---- Core entry points (by decorated name: this exe does not link Core.lib) ----------------

HMODULE GCore, GEngine, GNw;

template <class T> T CoreProc(const char* Name)
{
    FARPROC P = GetProcAddress(GCore, Name);
    if (!P)
        printf("  Core.dll has no %s\n", Name);
    return (T)P;
}
template <class T> T* CoreData(const char* Name) { return (T*)GetProcAddress(GCore, Name); }

// ---- report ---------------------------------------------------------------------------------

std::string Narrow(const TCHAR_* W, int Max = 512)
{
    std::string S;
    if (!W)
        return "(null)";
    for (int i = 0; W[i] && i < Max; i++)
        S += (W[i] >= 32 && W[i] < 127) ? (char)W[i] : '?';
    return S;
}

// "Module+rva" of an address, for the crash line (a module-relative place is stable)
std::string Where(const void* P)
{
    HMODULE M = NULL;
    char Buf[MAX_PATH + 32];
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)P, &M) && M)
    {
        char Name[MAX_PATH];
        GetModuleFileNameA(M, Name, MAX_PATH);
        const char* Base = strrchr(Name, '\\');
        sprintf(Buf, "%s+%lx", Base ? Base + 1 : Name, (DWORD)((BYTE*)P - (BYTE*)M));
    }
    else
        sprintf(Buf, "%p", P);
    return Buf;
}

int GFailures;
std::string GLastLog;   // the last line Core logged: where a stage stopped

DWORD GCode;
std::string GCrash;
int Filter(EXCEPTION_POINTERS* E)
{
    GCode = E->ExceptionRecord->ExceptionCode;
    char Buf[256];
    std::string At = Where(E->ExceptionRecord->ExceptionAddress);
    if (GCode == EXCEPTION_ACCESS_VIOLATION)
        sprintf(Buf, "access violation (%s %08lx) at %s", E->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                (DWORD)E->ExceptionRecord->ExceptionInformation[1], At.c_str());
    else if (GCode == 0xE06D7363)
        sprintf(Buf, "C++ exception at %s", At.c_str());
    else
        sprintf(Buf, "exception %08lx at %s", GCode, At.c_str());
    GCrash = Buf;
    return EXCEPTION_EXECUTE_HANDLER;
}

// runs Fn under SEH; false (and the stage FAILED) on any exception
bool Guarded(bool (*Fn)())
{
    __try
    {
        return Fn();
    }
    __except (Filter(GetExceptionInformation()))
    {
        return false;
    }
}

bool Stage(const char* Name, bool (*Fn)())
{
    GCrash.clear();
    printf("== %s\n", Name);
    fflush(stdout);
    bool Ok = Guarded(Fn);
    if (!GCrash.empty())
    {
        printf("STAGE %s: FAILED, %s\n", Name, GCrash.c_str());
        if (!GLastLog.empty())
            printf("  last log line: %s\n", GLastLog.c_str());
    }
    else
        printf("STAGE %s: %s\n", Name, Ok ? "OK" : "FAILED");
    fflush(stdout);
    if (!Ok)
        GFailures++;
    return Ok;
}

// ---- the engine interfaces Core calls (vtables as in Core.dll's ??_7F...@@6B@) -------------

// FMalloc: 7 slots, all pure in Core
struct FMallocHeap
{
    virtual void* Malloc(DWORD Count, const TCHAR_*) { return HeapAlloc(GetProcessHeap(), 0, Count ? Count : 1); }
    virtual void* Realloc(void* Ptr, DWORD Count, const TCHAR_*)
    {
        if (!Ptr)
            return HeapAlloc(GetProcessHeap(), 0, Count ? Count : 1);
        if (!Count)
        {
            HeapFree(GetProcessHeap(), 0, Ptr);
            return NULL;
        }
        return HeapReAlloc(GetProcessHeap(), 0, Ptr, Count);
    }
    virtual void Free(void* Ptr)
    {
        if (Ptr)
            HeapFree(GetProcessHeap(), 0, Ptr);
    }
    virtual void DumpAllocs() {}
    virtual void HeapCheck() {}
    virtual void Init(int) {}
    virtual void Exit() {}
} GHeap;

// FOutputDevice: Serialize (pure), Flush, Release
struct FLogDevice
{
    virtual void Serialize(const TCHAR_* V, int Event)
    {
        GLastLog = Narrow(V);
        printf("log| %s\n", GLastLog.c_str());
    }
    virtual void Flush() {}
    virtual void Release() {}
} GLogDevice;

// FOutputDeviceError: + HandleError (pure). A critical error ends the stage: Core's guard
// blocks unwind (recording their names) up to the stage's SEH frame.
const DWORD SMOKE_CRITICAL_ERROR = 0xE0534D4B;   // 'SMK'
struct FErrorDevice
{
    virtual void Serialize(const TCHAR_* V, int Event)
    {
        GLastLog = "CRITICAL: " + Narrow(V);
        printf("log| %s\n", GLastLog.c_str());
        RaiseException(SMOKE_CRITICAL_ERROR, 0, 0, NULL);
    }
    virtual void Flush() {}
    virtual void Release() {}
    virtual void HandleError() {}
} GErrorDevice;

// FFeedbackContext: FOutputDevice + YesNof, BeginSlowTask, EndSlowTask, StatusUpdatef,
// SetContext (pure) + the five MapCheck_ (Core's own, not called by the registration)
struct FWarnDevice
{
    virtual void Serialize(const TCHAR_* V, int Event)
    {
        GLastLog = "warn: " + Narrow(V);
        printf("log| %s\n", GLastLog.c_str());
    }
    virtual void Flush() {}
    virtual void Release() {}
    virtual int YesNof(const TCHAR_* Fmt, ...)
    {
        printf("log| YesNof: %s -> no\n", Narrow(Fmt).c_str());
        return 0;
    }
    virtual void BeginSlowTask(const TCHAR_* Task, int StatusWindow) {}
    virtual void EndSlowTask() {}
    virtual int StatusUpdatef(int Num, int Den, const TCHAR_* Fmt, ...) { return 1; }
    virtual void SetContext(void*) {}
    virtual void MapCheck_Show() {}
    virtual void MapCheck_ShowConditionally() {}
    virtual void MapCheck_Hide() {}
    virtual void MapCheck_Clear() {}
    virtual void MapCheck_Add(int, void*, const TCHAR_*) {}
} GWarnDevice;

// FConfigCache (include/ue2/UE2Core.h order; the two GetString overloads are slots 3 and 4).
// It knows only what L2.ini tells Engine.dll about NWindow; every other key is missing, as in
// an empty ini, and each question is logged.
struct FConfigStub
{
    static bool Is(const TCHAR_* A, const wchar_t* B) { return A && !_wcsicmp(A, B); }
    static void Ask(const char* What, const TCHAR_* Section, const TCHAR_* Key, const TCHAR_* File, const char* Answer)
    {
        printf("log| GConfig->%s [%s] %s (%s) -> %s\n", What, Narrow(Section).c_str(), Narrow(Key).c_str(),
               File ? Narrow(File).c_str() : "system ini", Answer);
    }
    virtual int GetBool(const TCHAR_* S, const TCHAR_* K, int& V, const TCHAR_* F) { Ask("GetBool", S, K, F, "missing"); return 0; }
    virtual int GetInt(const TCHAR_* S, const TCHAR_* K, int& V, const TCHAR_* F) { Ask("GetInt", S, K, F, "missing"); return 0; }
    virtual int GetFloat(const TCHAR_* S, const TCHAR_* K, float& V, const TCHAR_* F) { Ask("GetFloat", S, K, F, "missing"); return 0; }
    virtual int GetStringF(const TCHAR_* S, const TCHAR_* K, void* Str, const TCHAR_* F) { Ask("GetString(FString)", S, K, F, "missing"); return 0; }
    virtual int GetString(const TCHAR_* S, const TCHAR_* K, TCHAR_* V, int Size, const TCHAR_* F)
    {
        const wchar_t* Value = NULL;
        if (Is(S, L"Engine.Engine") && Is(K, L"L2Console"))
            Value = L"NWindow.ConsoleWnd";
        else if (Is(S, L"Engine.Engine") && Is(K, L"L2Font"))
            Value = L"NWindow.L2Font";
        Ask("GetString", S, K, F, Value ? Narrow(Value).c_str() : "missing");
        if (!Value)
            return 0;
        lstrcpynW(V, Value, Size);
        return 1;
    }
    virtual const TCHAR_* GetStr(const TCHAR_* S, const TCHAR_* K, const TCHAR_* F) { Ask("GetStr", S, K, F, "\"\""); return L""; }
    // Result[0] = 0 first, as the real FConfigCacheIni::GetSection does (L2.exe 0x10911421): FURL::LoadURLConfig ignores the return value and reads the buffer anyway
    virtual int GetSection(const TCHAR_* S, TCHAR_* V, int Size, const TCHAR_* F) { Ask("GetSection", S, L"", F, "missing"); if (V && Size > 0) V[0] = 0; return 0; }
    virtual void* GetSectionPrivate(const TCHAR_* S, int Force, int Const, const TCHAR_* F) { Ask("GetSectionPrivate", S, L"", F, "NULL"); return NULL; }
    virtual void EmptySection(const TCHAR_* S, const TCHAR_* F) { Ask("EmptySection", S, L"", F, "-"); }
    virtual void SetBool(const TCHAR_* S, const TCHAR_* K, int V, const TCHAR_* F) { Ask("SetBool", S, K, F, "ignored"); }
    virtual void SetInt(const TCHAR_* S, const TCHAR_* K, int V, const TCHAR_* F) { Ask("SetInt", S, K, F, "ignored"); }
    virtual void SetFloat(const TCHAR_* S, const TCHAR_* K, float V, const TCHAR_* F) { Ask("SetFloat", S, K, F, "ignored"); }
    virtual void SetString(const TCHAR_* S, const TCHAR_* K, const TCHAR_* V, const TCHAR_* F) { Ask("SetString", S, K, F, "ignored"); }
    // UE2's remaining slots (not confirmed: stop loudly if one is reached)
    virtual void Flush(int Read, const TCHAR_* F) { Unknown("Flush"); }
    virtual void UnloadFile(const TCHAR_* F) { Unknown("UnloadFile"); }
    virtual void Detach(const TCHAR_* F) { Unknown("Detach"); }
    static void Unknown(const char* What)
    {
        printf("log| GConfig->%s: slot not confirmed\n", What);
        RaiseException(SMOKE_CRITICAL_ERROR, 0, 0, NULL);
    }
} GConfigStub;

// ---- Core calls -----------------------------------------------------------------------------

typedef void* (__cdecl* AppGetDllHandleFn)(const TCHAR_*);
typedef void* (__cdecl* AppGetDllExportFn)(void*, const TCHAR_*);
typedef void(__cdecl* StaticVoidFn)();
typedef const TCHAR_* (__fastcall* GetNameFn)(const void* Self, void*);
typedef int(__fastcall* GetPropertiesSizeFn)(const void* Self, void*);
typedef void* (__cdecl* StaticLoadClassFn)(void* Base, void* Outer, const TCHAR_* Name, const TCHAR_* File, DWORD Flags, void* Sandbox);
typedef void* (__cdecl* StaticConstructObjectFn)(void* Class, void* Outer, int Name, DWORD Flags, void* Template, void* Error, void* SubobjectRoot);
typedef void* (__cdecl* GetTransientPackageFn)();

// UObject layout (include/ue2/UE2Core.h, checked on the C4 Core): Outer +0x18, ObjectFlags +0x1C, Name +0x20,
// Class +0x24; UField::SuperField +0x34 (C1-C3: +0x28); before registration Outer and Name hold the package
// and class name strings and _LinkerIndex (+0x14) links GAutoRegister.
template <class T> T& At(const void* Obj, DWORD Off) { return *(T*)((BYTE*)Obj + Off); }
const DWORD SuperOff = 0x34;   // UField::SuperField (C4)

struct Package
{
    std::map<std::string, BYTE*> Classes;   // "NCButton" -> autoclass value
    std::map<BYTE*, std::string> Names;     // UClass* -> "NCButton"
    std::vector<std::string> Exports;       // in ordinal order
    std::map<std::string, void*> ExportAddr;
    const TCHAR_* GPackage = NULL;
    DWORD InstanceRva = 0;
} GPkg;

const wchar_t* GSystemDir;

// ---- stages ---------------------------------------------------------------------------------

void SetStandIns();

bool Bind()
{
    GCore = LoadLibraryW(L"Core.dll");
    if (!GCore)
    {
        printf("  LoadLibrary(Core.dll) failed: %lu\n", GetLastError());
        return false;
    }
    // C3: the static initializers of NWindow.dll build FString globals, which allocate through Core's
    // GMalloc, so the heap has to be there when the image is mapped (in the game appInit did it long before)
    SetStandIns();
    AppGetDllHandleFn GetDllHandle = CoreProc<AppGetDllHandleFn>("?appGetDllHandle@@YAPAXPBG@Z");
    if (!GetDllHandle)
        return false;
    // UPackage::Bind: the package name + ".dll"
    GNw = (HMODULE)GetDllHandle(L"NWindow.dll");
    GEngine = GetModuleHandleW(L"Engine.dll");
    if (!GNw)
    {
        printf("  appGetDllHandle(NWindow.dll): NULL (%lu)\n", GetLastError());
        return false;
    }
    wchar_t Path[MAX_PATH];
    GetModuleFileNameW(GNw, Path, MAX_PATH);
    const wchar_t* File = wcsrchr(Path, L'\\');
    bool InSystem = !_wcsnicmp(Path, GSystemDir, wcslen(GSystemDir));
    IMAGE_NT_HEADERS* Nt = (IMAGE_NT_HEADERS*)((BYTE*)GNw + ((IMAGE_DOS_HEADER*)GNw)->e_lfanew);
    bool Stock = Nt->FileHeader.TimeDateStamp == 0x43C73652;
    printf("  appGetDllHandle(NWindow.dll): %ls from the System folder: %s\n", File ? File + 1 : Path,
           InSystem ? "yes" : "NO");
    printf("  image: %s, at its preferred base: %s\n", Stock ? "stock C4 build" : "not the stock build",
           (DWORD)GNw == Nt->OptionalHeader.ImageBase ? "yes" : "no");
    printf("  NWindowDecomp.dll loaded: %s\n", GetModuleHandleW(L"NWindowDecomp.dll") ? "YES" : "no");
    printf("  Engine.dll loaded with it: %s\n", GEngine ? "yes" : "NO");
    return InSystem && GEngine && !GetModuleHandleW(L"NWindowDecomp.dll");
}

bool DllMainRan()
{
    if (!GPkg.InstanceRva)
    {
        printf("  no --instance-rva: not checked\n");
        return true;
    }
    HINSTANCE Kept = At<HINSTANCE>(GNw, GPkg.InstanceRva);
    printf("  GNWindowInstance == the module handle: %s\n", Kept == GNw ? "yes" : "NO");
    return Kept == GNw;
}

bool Exports()
{
    AppGetDllExportFn GetDllExport = CoreProc<AppGetDllExportFn>("?appGetDllExport@@YAPAXPAXPBG@Z");
    if (!GetDllExport)
        return false;
    BYTE* Base = (BYTE*)GNw;
    IMAGE_NT_HEADERS* Nt = (IMAGE_NT_HEADERS*)(Base + ((IMAGE_DOS_HEADER*)Base)->e_lfanew);
    IMAGE_DATA_DIRECTORY& D = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY* E = (IMAGE_EXPORT_DIRECTORY*)(Base + D.VirtualAddress);
    DWORD* Funcs = (DWORD*)(Base + E->AddressOfFunctions);
    DWORD* Names = (DWORD*)(Base + E->AddressOfNames);
    WORD* Ords = (WORD*)(Base + E->AddressOfNameOrdinals);
    std::vector<std::pair<WORD, std::string>> ByOrdinal;
    int Bad = 0, Autoclass = 0;
    for (DWORD i = 0; i < E->NumberOfNames; i++)
    {
        std::string Name = (const char*)(Base + Names[i]);
        void* Table = Base + Funcs[Ords[i]];
        std::wstring W(Name.begin(), Name.end());
        void* Found = GetDllExport(GNw, W.c_str());   // Core: TCHAR -> ANSI, GetProcAddress
        if (Found != Table)
        {
            printf("  %s: appGetDllExport gives %s\n", Name.c_str(), Found ? "another address" : "NULL");
            Bad++;
        }
        ByOrdinal.push_back(std::make_pair((WORD)(Ords[i] + E->Base), Name));
        GPkg.ExportAddr[Name] = Found;
        if (!Name.compare(0, 9, "autoclass"))
        {
            Autoclass++;
            BYTE* Class = Found ? *(BYTE**)Found : NULL;
            GPkg.Classes[Name.substr(9)] = Class;
            GPkg.Names[Class] = Name.substr(9);
        }
        else if (Name == "GPackage")
            GPkg.GPackage = (const TCHAR_*)Found;
    }
    std::sort(ByOrdinal.begin(), ByOrdinal.end());
    printf("  %lu exports, %d found through appGetDllExport, %d autoclass\n", E->NumberOfNames,
           (int)E->NumberOfNames - Bad, Autoclass);
    printf("  ordinals: 1 %s, 2 %s, 3..%d autoclass in name order: %s\n",
           ByOrdinal.size() > 0 ? ByOrdinal[0].second.c_str() : "-", ByOrdinal.size() > 1 ? ByOrdinal[1].second.c_str() : "-",
           (int)ByOrdinal.size(), [&] {
               for (size_t k = 3; k < ByOrdinal.size(); k++)
                   if (ByOrdinal[k].second < ByOrdinal[k - 1].second || ByOrdinal[k].first != k + 1)
                       return "NO";
               return "yes";
           }());
    printf("  GPackage = \"%s\"\n", Narrow(GPkg.GPackage).c_str());
    int NullClasses = 0;
    for (const auto& C : GPkg.Classes)
        NullClasses += C.second == NULL;
    if (NullClasses)
        printf("  %d autoclass exports hold NULL\n", NullClasses);
    return !Bad && GPkg.GPackage && !NullClasses && GPkg.ExportAddr.count("_DllMain@12");
}

bool Queue()
{
    BYTE** Head = CoreData<BYTE*>("?GAutoRegister@UObject@@1PAV1@A");
    if (!Head)
        return false;
    int Ours = 0, Others = 0, Unknown = 0, BadName = 0;
    std::map<BYTE*, bool> Seen;
    for (BYTE* P = *Head; P && Ours + Others < 100000; P = At<BYTE*>(P, 0x14))
    {
        if (At<const TCHAR_*>(P, 0x18) != GPkg.GPackage)
        {
            Others++;
            continue;
        }
        Ours++;
        auto It = GPkg.Names.find(P);
        if (It == GPkg.Names.end())
        {
            Unknown++;
            continue;
        }
        Seen[P] = true;
        // TEXT("NCButton")+1
        if (Narrow(At<const TCHAR_*>(P, 0x20)) != It->second.substr(1))
            BadName++;
    }
    printf("  GAutoRegister: %d objects of package NWindow (%d named by an autoclass export), %s of Core/Engine\n",
           Ours, Ours - Unknown, Others ? "and those" : "none");
    {
        // diagnostic (stage 5): a class object whose vtable pointer is still NULL was not built by the UClass constructor
        int NullVptr = 0;
        for (auto& KV : GPkg.Names)
            if (!At<void*>(KV.first, 0))
            {
                if (NullVptr++ < 400)
                    printf("  NULL vptr: %s at %p (_LinkerIndex %p, Outer %p)\n", KV.second.c_str(), KV.first, At<void*>(KV.first, 0x14), At<void*>(KV.first, 0x18));
            }
        // the dump holds addresses (and the pick depends on where the loader put the DLL): it is printed
        // only when something is wrong, so a good run prints exactly what the stock DLL prints
        if (NullVptr)
        {
            int Dumped = 0, DumpedOk = 0;
            for (auto& KV : GPkg.Names)
            {
                bool Null = !At<void*>(KV.first, 0);
                if ((Null && Dumped < 2) || (!Null && DumpedOk < 2))
                {
                    (Null ? Dumped : DumpedOk)++;
                    printf("  dump %s %s:", Null ? "NULL" : "ok", KV.second.c_str());
                    for (int i = 0; i < 20; i++) printf(" %08X", At<DWORD>(KV.first, i * 4));
                    printf("\n");
                }
            }
        }
        printf("  class objects with a NULL vtable pointer: %d of %d\n", NullVptr, (int)GPkg.Names.size());
    }
    printf("  every autoclass queued: %s; names TEXT(\"NClass\")+1: %s\n",
           (int)Seen.size() == (int)GPkg.Classes.size() ? "yes" : "NO", BadName ? "NO" : "yes");
    return Ours == (int)GPkg.Classes.size() && !Unknown && !BadName && Seen.size() == GPkg.Classes.size();
}

// what appInit would have set: the heap, the output devices and the stub GConfig
void SetStandIns()
{
    *CoreData<void*>("?GMalloc@@3PAVFMalloc@@A") = &GHeap;
    *CoreData<void*>("?GLog@@3PAVFOutputDevice@@A") = &GLogDevice;
    *CoreData<void*>("?GError@@3PAVFOutputDeviceError@@A") = &GErrorDevice;
    *CoreData<void*>("?GWarn@@3PAVFFeedbackContext@@A") = &GWarnDevice;
    *CoreData<void*>("?GConfig@@3PAVFConfigCache@@A") = &GConfigStub;
}

bool Init()
{
    SetStandIns();
    StaticVoidFn NameInit = CoreProc<StaticVoidFn>("?StaticInit@FName@@SAXXZ");
    StaticVoidFn ObjectInit = CoreProc<StaticVoidFn>("?StaticInit@UObject@@SAXXZ");
    if (!NameInit || !ObjectInit)
        return false;
    NameInit();
    printf("  FName::StaticInit: done\n");
    ObjectInit();   // GObjInitialized = 1, ProcessRegistrants, the transient package
    printf("  UObject::StaticInit: done\n");
    BYTE** Head = CoreData<BYTE*>("?GAutoRegister@UObject@@1PAV1@A");
    printf("  GAutoRegister after it: %s\n", *Head ? "NOT empty" : "empty");
    // appInit's GSys = new USystem: StaticLoadObject looks for a package file through its Paths
    // (appFindPackageFile) before it settles for the classes of a bound DLL. No paths here (the
    // stub ini has none), so nothing is searched on disk.
    StaticConstructObjectFn Construct = CoreProc<StaticConstructObjectFn>(
        "?StaticConstructObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@1@Z");
    GetTransientPackageFn Transient = CoreProc<GetTransientPackageFn>("?GetTransientPackage@UObject@@SAPAVUPackage@@XZ");
    void* SystemClass = GetProcAddress(GCore, "?PrivateStaticClass@USystem@@0VUClass@@A");
    void** GSys = CoreData<void*>("?GSys@@3PAVUSystem@@A");
    if (!Construct || !Transient || !SystemClass || !GSys)
        return false;
    *GSys = Construct(SystemClass, Transient(), 0, 0, NULL, &GErrorDevice, NULL);
    printf("  GSys = new USystem: %s\n", *GSys ? "done" : "NULL");
    return !*Head && *GSys;
}

std::string ObjName(const void* Obj)
{
    static GetNameFn GetName = CoreProc<GetNameFn>("?GetName@UObject@@QBEPBGXZ");
    return Obj ? Narrow(GetName(Obj, NULL)) : "None";
}

bool Registered()
{
    GetPropertiesSizeFn PropSize = CoreProc<GetPropertiesSizeFn>("?GetPropertiesSize@UStruct@@UAEHXZ");
    BYTE* ClassClass = (BYTE*)GetProcAddress(GCore, "?PrivateStaticClass@UClass@@0V1@A");
    if (!PropSize || !ClassClass)
        return false;
    int Bad = 0;
    for (const auto& C : GPkg.Classes)
    {
        BYTE* K = C.second;
        if (!K)
            continue;
        std::string Name = ObjName(K);
        void* Outer = At<void*>(K, 0x18);
        void* Super = At<void*>(K, SuperOff);
        std::string SuperText = Super ? ObjName(At<void*>(Super, 0x18)) + "." + ObjName(Super) : "None";
        bool Ok = Name == C.first.substr(1) && ObjName(Outer) == "NWindow" && At<BYTE*>(K, 0x24) == ClassClass;
        printf("  %-28s %s.%s super=%s size=0x%x flags=%08lx%s\n", C.first.c_str(), ObjName(Outer).c_str(),
               Name.c_str(), SuperText.c_str(), PropSize(K, NULL), At<DWORD>(K, 0x1C), Ok ? "" : "  <- WRONG");
        Bad += !Ok;
    }
    return !Bad;
}

void* GConsoleClass;
void* GFontClass;

bool Load()
{
    StaticLoadClassFn LoadClass = CoreProc<StaticLoadClassFn>("?StaticLoadClass@UObject@@SAPAVUClass@@PAV2@PAV1@PBG2KPAVUPackageMap@@@Z");
    void* L2ConsoleWnd = GetProcAddress(GEngine, "?PrivateStaticClass@UL2ConsoleWnd@@0VUClass@@A");
    void* L2Font = GetProcAddress(GEngine, "?PrivateStaticClass@UL2Font@@0VUClass@@A");
    if (!LoadClass || !L2ConsoleWnd || !L2Font)
        return false;
    // Engine.dll: StaticLoadClass(UL2ConsoleWnd::StaticClass(), NULL,
    // "ini:Engine.Engine.L2Console", NULL, LOAD_NoFail, NULL); the font with flags 0
    GConsoleClass = LoadClass(L2ConsoleWnd, NULL, L"ini:Engine.Engine.L2Console", NULL, 1, NULL);
    printf("  ini:Engine.Engine.L2Console -> %s (autoclassNConsoleWnd: %s)\n",
           GConsoleClass ? (ObjName(At<void*>(GConsoleClass, 0x18)) + "." + ObjName(GConsoleClass)).c_str() : "None",
           GConsoleClass == GPkg.Classes["NConsoleWnd"] ? "yes" : "NO");
    GFontClass = LoadClass(L2Font, NULL, L"ini:Engine.Engine.L2Font", NULL, 0, NULL);
    printf("  ini:Engine.Engine.L2Font -> %s (autoclassNL2Font: %s)\n",
           GFontClass ? (ObjName(At<void*>(GFontClass, 0x18)) + "." + ObjName(GFontClass)).c_str() : "None",
           GFontClass == GPkg.Classes["NL2Font"] ? "yes" : "NO");
    return GConsoleClass == GPkg.Classes["NConsoleWnd"] && GFontClass == GPkg.Classes["NL2Font"];
}

bool ConstructOne(void* Class, const char* What)
{
    StaticConstructObjectFn Construct = CoreProc<StaticConstructObjectFn>(
        "?StaticConstructObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@1@Z");
    GetTransientPackageFn Transient = CoreProc<GetTransientPackageFn>("?GetTransientPackage@UObject@@SAPAVUPackage@@XZ");
    void* Error = *CoreData<void*>("?GError@@3PAVFOutputDeviceError@@A");
    // as Engine.dll: (Class, GetTransientPackage(), NAME_None, 0, NULL, GError, NULL)
    BYTE* Obj = (BYTE*)Construct(Class, Transient(), 0, 0, NULL, Error, NULL);
    if (!Obj)
    {
        printf("  %s: NULL\n", What);
        return false;
    }
    void* Vtbl = *(void**)Obj;
    bool InNw = (BYTE*)Vtbl >= (BYTE*)GNw && (BYTE*)Vtbl < (BYTE*)GNw + 0x400000;
    printf("  %s: %s.%s, class %s, vtable in NWindow.dll: %s\n", What, ObjName(At<void*>(Obj, 0x18)).c_str(),
           ObjName(Obj).c_str(), ObjName(At<void*>(Obj, 0x24)).c_str(), InNw ? "yes" : "NO");
    return At<void*>(Obj, 0x24) == Class && InNw;
}

bool ConstructFont() { return ConstructOne(GFontClass, "L2Font object"); }
bool ConstructConsole() { return ConstructOne(GConsoleClass, "ConsoleWnd object"); }

// Ends the process without detaching the DLLs. Once UObject::StaticInit ran, the atexit
// ~UClass of each PrivateStaticClass (Core's, Engine's, ours) reaches UObject::~UObject while
// GObjInitialized is still 1 and stops on "Other object in slot"; the game runs
// UObject::StaticExit (appPreExit) first, which needs the rest of appInit.
void Quit(UINT Code)
{
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), Code);
}

}  // namespace

// The hook-mode smoke (nwd_smoke.cpp) loads NWindow.dll with nothing of appInit done. The C2 image's
// static initializers did not need the engine; the C3 image builds FString globals at load time, which
// allocate through Core's GMalloc. The engine sets it before it loads NWindow.dll in the game, so the
// smoke gives Core the same heap and output devices the standalone stages use.
void HookModeStandIns()
{
    GCore = LoadLibraryW(L"Core.dll");
    if (!GCore)
    {
        printf("Core.dll: LoadLibrary failed (%lu)\n", GetLastError());
        return;
    }
    SetStandIns();
}

int StandaloneMain(int argc, wchar_t** argv)
{
    // nwd_smoke --standalone <System> [--instance-rva HEX] [--until STAGE]
    if (argc < 3)
    {
        fwprintf(stderr, L"usage: nwd_smoke --standalone <System folder> [--instance-rva HEX] [--until STAGE]\n");
        return 2;
    }
    GSystemDir = argv[2];
    std::string Until;
    for (int i = 3; i + 1 < argc; i += 2)
    {
        if (!wcscmp(argv[i], L"--instance-rva"))
            GPkg.InstanceRva = wcstoul(argv[i + 1], NULL, 16);
        else if (!wcscmp(argv[i], L"--until"))
            Until = Narrow(argv[i + 1]);
    }
    printf("System: %ls\n", GSystemDir);
    if (!SetCurrentDirectoryW(GSystemDir) || !SetDllDirectoryW(GSystemDir))
    {
        printf("bad folder (%lu)\n", GetLastError());
        return 2;
    }
    struct
    {
        const char* Name;
        bool (*Fn)();
    } Stages[] = {
        {"bind", Bind},           {"dllmain", DllMainRan},     {"exports", Exports},
        {"queue", Queue},         {"init", Init},              {"registered", Registered},
        {"load", Load},           {"construct-font", ConstructFont}, {"construct-console", ConstructConsole},
    };
    const char* Reached = "none";
    for (const auto& S : Stages)
    {
        if (!Stage(S.Name, S.Fn))
        {
            printf("STOPPED at stage %s\n", S.Name);
            fflush(stdout);
            Quit(1);
        }
        Reached = S.Name;
        if (Until == S.Name)
            break;
    }
    printf("ALL STAGES OK (last: %s)\n", Reached);
    fflush(stdout);
    Quit(GFailures ? 1 : 0);
    return 0;
}
