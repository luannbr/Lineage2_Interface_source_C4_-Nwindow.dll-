// What Core received from the package's static initializers (IMPLEMENT_CLASS): the 178 UClass
// objects the autoclass exports name, dword by dword, and Core's list of objects waiting to be
// registered (UObject::GAutoRegister). Core is not initialized here, so the native UClass
// constructor keeps what it was given and queues the class: this is the state
// UObject::ProcessRegistrants starts from when the game loads the package.
//
// The stock and the hook-mode runs build the same objects (the initializers are stock code);
// against the standalone NWindow.dll (tools/difftest.py --standalone) this checks our
// IMPLEMENT_CLASS at run time. Pointers are written as what they point to, not as addresses:
// "class:<name>" for a UClass of the package, "GPackage", the text of a string of the package,
// "code" for the package's code, "<module>!<export>" or "<module>+rva" for Core and Engine;
// anything else is a value (flags, sizes: nothing is allocated yet).
#include "difftest.h"

#include <algorithm>
#include <map>

using namespace dt;

namespace {

const DWORD UCLASS_SIZE = 0x520;

struct Module
{
    std::string Name;
    BYTE* Lo;
    BYTE* Hi;
    std::map<DWORD, std::string> Exports;   // rva -> name
};

IMAGE_NT_HEADERS* Nt(BYTE* Base)
{
    return (IMAGE_NT_HEADERS*)(Base + ((IMAGE_DOS_HEADER*)Base)->e_lfanew);
}

Module Load(const char* Name, BYTE* Base)
{
    Module M;
    M.Name = Name;
    M.Lo = Base;
    M.Hi = Base ? Base + Nt(Base)->OptionalHeader.SizeOfImage : NULL;
    if (!Base)
        return M;
    IMAGE_DATA_DIRECTORY& D = Nt(Base)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!D.Size)
        return M;
    IMAGE_EXPORT_DIRECTORY* E = (IMAGE_EXPORT_DIRECTORY*)(Base + D.VirtualAddress);
    DWORD* Funcs = (DWORD*)(Base + E->AddressOfFunctions);
    DWORD* Names = (DWORD*)(Base + E->AddressOfNames);
    WORD* Ords = (WORD*)(Base + E->AddressOfNameOrdinals);
    for (DWORD i = 0; i < E->NumberOfNames; i++)
        M.Exports[Funcs[Ords[i]]] = (const char*)(Base + Names[i]);
    return M;
}

bool InCode(BYTE* Base, BYTE* P)
{
    IMAGE_NT_HEADERS* N = Nt(Base);
    IMAGE_SECTION_HEADER* S = IMAGE_FIRST_SECTION(N);
    for (WORD i = 0; i < N->FileHeader.NumberOfSections; i++, S++)
        if (P >= Base + S->VirtualAddress && P < Base + S->VirtualAddress + S->Misc.VirtualSize)
            return (S->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    return false;
}

struct Classes
{
    std::map<BYTE*, std::string> ByObject;   // UClass* -> class name (autoclass export)
    BYTE* Package = NULL;                    // GPackage
    Module Nw, Core, Engine;

    // a dword of an object, as what it points to
    std::string Show(DWORD V)
    {
        BYTE* P = (BYTE*)(DWORD_PTR)V;
        char Buf[64];
        if (P >= Nw.Lo && P < Nw.Hi)
        {
            std::map<BYTE*, std::string>::iterator It = ByObject.find(P);
            if (It != ByObject.end())
                return "class:" + It->second;
            if (P == Package)
                return "GPackage";
            if (InCode(Nw.Lo, P))
                return "code";
            // a name: TEXT("NCButton")+1, the literal starts one character before
            const wchar_t* W = (const wchar_t*)P;
            if (!((DWORD_PTR)P & 1) && W[-1] == L'N' && W[0] >= L'A' && W[0] <= L'Z')
                return "\"" + Esc(W, -1, 64) + "\"";
            sprintf(Buf, "nwindow-data");
            return Buf;
        }
        for (Module* M : {&Core, &Engine})
        {
            if (P >= M->Lo && P < M->Hi)
            {
                DWORD Rva = (DWORD)(P - M->Lo);
                std::map<DWORD, std::string>::iterator It = M->Exports.find(Rva);
                if (It != M->Exports.end())
                    return M->Name + "!" + It->second;
                sprintf(Buf, "%s+%x", M->Name.c_str(), Rva);
                return Buf;
            }
        }
        sprintf(Buf, "%lx", V);   // a value (flags, sizes): nothing is allocated at this point
        return Buf;
    }
};

}  // namespace

DT_CASE(classes, "the 178 UClass objects Core got from IMPLEMENT_CLASS, and its registration queue")
{
    Classes C;
    C.Nw = Load("NWindow", Base);
    C.Core = Load("Core", (BYTE*)GetModuleHandleW(L"Core.dll"));
    C.Engine = Load("Engine", (BYTE*)GetModuleHandleW(L"Engine.dll"));

    std::vector<std::string> Names;
    for (const auto& E : C.Nw.Exports)
    {
        if (E.second == "GPackage")
            C.Package = Base + E.first;
        else if (!E.second.compare(0, 9, "autoclass"))
        {
            BYTE* Object = *(BYTE**)(Base + E.first);
            C.ByObject[Object] = E.second.substr(9);
            Names.push_back(E.second.substr(9));
        }
    }
    std::sort(Names.begin(), Names.end());
    Print("exports: %d autoclass, GPackage %s\n", (int)Names.size(),
          C.Package ? ("\"" + Esc((const wchar_t*)C.Package) + "\"").c_str() : "missing");

    // every class object, dword by dword (the zero ones left out)
    std::map<std::string, BYTE*> ByName;
    for (const auto& O : C.ByObject)
        ByName[O.second] = O.first;
    for (const std::string& Name : Names)
    {
        BYTE* Object = ByName[Name];
        Where = Name;
        Hashed H;
        if (!Object)
        {
            Print("%s: NULL\n", Name.c_str());
            continue;
        }
        for (DWORD Off = 0; Off < UCLASS_SIZE; Off += 4)
        {
            DWORD V = *(DWORD*)(Object + Off);
            if (!V)
                continue;
            char Buf[16];
            sprintf(Buf, "+%03lx ", Off);
            H.Add(Buf + C.Show(V));
        }
        H.Flush(Name);
    }
    Where.clear();

    // Core's queue of native objects to register (UObject::GAutoRegister, linked through
    // _LinkerIndex at +0x14; Outer at +0x18 still holds the package name)
    BYTE** Head = (BYTE**)GetProcAddress(GetModuleHandleW(L"Core.dll"), "?GAutoRegister@UObject@@1PAV1@A");
    if (!Head)
    {
        Print("GAutoRegister: not exported by Core\n");
        return;
    }
    std::string Order;
    int Ours = 0, Others = 0;
    for (BYTE* P = *Head; P && Ours + Others < 100000; P = *(BYTE**)(P + 0x14))
    {
        if (*(BYTE**)(P + 0x18) == C.Package)
        {
            Ours++;
            Order += C.Show((DWORD)(DWORD_PTR)P) + "\n";
        }
        else
            Others++;
    }
    Print("GAutoRegister: %d objects of package NWindow, %s other\n", Ours, Others ? "some" : "no");
    Hashed H;
    H.Text = Order;
    H.Flush("registration order (last registered first)");
}
