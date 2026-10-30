// difftest - runs pieces of NWindow.dll outside the game on fixed inputs and writes what they
// produce, one text file per case. tools/difftest.py runs it twice, with every hook disabled
// (the stock code) and with ours, and compares the files; tests/golden/ keeps the stock ones.
//
// The runner only calls the stock entry points (base + va - 0x10000000): with the hooks on,
// those jump into NWindowDecomp.dll. Nothing here may print an address or depend on one.
#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace dt {

// ---- output ----------------------------------------------------------------------------------

extern bool Verbose;                      // --verbose: the details behind the hashed lines
extern std::string Where;                 // what a case is on, for the crash message
void Print(const char* Fmt, ...);
void PrintW(const wchar_t* Text, int Max = 4000);   // escaped, no newline
std::string Esc(const wchar_t* Text, int Len = -1, int Max = 4000);
std::string EscA(const char* Text, int Len = -1);
unsigned Hash32(const std::string& Text);
std::string Hex32(unsigned V);

// A line (or a group of lines) that is hashed unless --verbose: the golden files stay small and
// a difference still names the item.
struct Hashed
{
    std::string Text;
    void Add(const std::string& S) { Text += S; Text += '\n'; }
    void Flush(const std::string& Label);
};

// ---- the stock image and our DLL ---------------------------------------------------------------

extern BYTE* Base;                        // NWindow.dll
void* Stock(DWORD Va);
void* Ours(DWORD Va);                     // the forwarder our hook at Va installs (NWindowDecomp.log)
void* OurSymbol(const char* Name);        // a function of NWindowDecomp.dll by name (its PDB)
void Patch(void* At, void* To);           // jmp at At (the test's own stand-ins)
// a stock function made to jump to To, and ours: the hook's forwarder and, when our other code
// calls the method directly, the method itself (Name, e.g. "NCHtmlViewer::LoadHtml")
bool PatchBoth(DWORD Va, void* To, const char* Name = NULL);

template <class R, class... A> R This(DWORD Va, const void* Self, A... Args)
{
    typedef R(__fastcall * Fn)(const void*, void*, A...);
    return ((Fn)Stock(Va))(Self, 0, Args...);
}
template <class R, class... A> R Cdecl(DWORD Va, A... Args)
{
    typedef R(__cdecl * Fn)(A...);
    return ((Fn)Stock(Va))(Args...);
}
template <class T> T& Global(DWORD Va) { return *(T*)Stock(Va); }

// ---- memory: the engine's GMalloc replaced by a heap that records what is asked ----------------

std::string TakeAllocs();                 // "m16 m96 f ..." since the last call
void ClearAllocs();
void* TestMalloc(DWORD Size);             // through GMalloc like the code under test
void TestFree(void* P);

// ---- engine types read or built by the tests ------------------------------------------------

struct FStr                               // FString (TArray<TCHAR>)
{
    wchar_t* Data;
    int Num;                              // characters + terminator, 0 when empty
    int Max;
};
std::string Show(const FStr& S);
void Set(FStr& S, const wchar_t* Text);  // through Core's FString::operator=

// a C++ exception thrown by the stock library (std::length_error...): its type and message
struct Caught
{
    bool bThrown;
    std::string Type;
    std::string What;
};
template <class F> Caught Try(F Fn);
Caught TryRaw(void (*Fn)(void*), void* Arg);
template <class F> Caught Try(F Fn)
{
    struct Box
    {
        F* Fn;
        static void Run(void* P) { (*((Box*)P)->Fn)(); }
    } B = {&Fn};
    return TryRaw(&Box::Run, &B);
}
std::string Show(const Caught& C);

// ---- cases ---------------------------------------------------------------------------------

struct Case
{
    std::string Name;
    void (*Run)(const std::vector<std::wstring>& Args);
    const char* About;
    std::vector<std::wstring> Args;       // what the command line gave the case
};
void Register(const Case& C);
// the case pages_NAME the runner makes of "--pages NAME=DIR" (case_html.cpp); --only REL limits
// it to those pages, written out in full (tools/difftest.py, on a difference)
void RunPages(const std::vector<std::wstring>& Args);
extern std::vector<std::wstring> OnlyPages;
struct AutoCase
{
    AutoCase(const char* Name, void (*Run)(const std::vector<std::wstring>&), const char* About)
    {
        Register({Name, Run, About});
    }
};
#define DT_CASE(Name, About)                                                                  \
    static void Case_##Name(const std::vector<std::wstring>& Args);                           \
    static dt::AutoCase AutoCase_##Name(#Name, Case_##Name, About);                           \
    static void Case_##Name(const std::vector<std::wstring>& Args)

}  // namespace dt
