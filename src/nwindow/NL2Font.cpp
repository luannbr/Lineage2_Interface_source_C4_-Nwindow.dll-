/*=============================================================================
    NL2Font.cpp - L2 bitmap font. Stock: 0x10072220 .. 0x10072e39 (+ the engine templates the
    font code needs: UGlobalTempObjects::AddGlobalObject 0x10072850, ConstructObject<UFinalBlend>
    0x100727e0).
=============================================================================*/
#include "nwindow/NL2Font.h"

#include <string.h>

// (GFontBytesRead, GFontBlendA, GFontBlendB: src/standalone/NwGlobals.cpp in the standalone build)

/*-----------------------------------------------------------------------------
    FL2FontPage: the v1.0 pages (16-byte glyphs).
-----------------------------------------------------------------------------*/

// 0x10088550
FL2FontPage::FL2FontPage()
{
    First = 0;
    Count = 0;
    Chars = NULL;
}
NW_HOOK_CTOR(0x10088550, FL2FontPage)

// 0x100889d0 - the vtable, then Free (a tail jump). The vector destructor of Clear and the font's
// destructor reach it
FL2FontPage::~FL2FontPage()
{
    Free();
}
NW_HOOK_DTOR(0x100889d0, FL2FontPage)

// 0x10088580 (vslot 27) - the font's page texture (a 4-byte body that only the vtable refers to)
UTexture* NL2Font::GetTexture()
{
    return Texture;
}
NW_HOOK_METHOD(0x10088580, NL2Font, GetTexture)

// 0x10088310 - the glyph of C when it is on this page (the stock NL2Font::GetChar keeps ecx across this
// call: the callers are ours too)
FL2FontChar* FL2FontPage::Find(TCHAR C)
{
    INT Index = (_WORD)C;
    if (Index >= First && Index < First + Count)
        return &Chars[Index - First];
    return NULL;
}
NW_HOOK_METHOD(0x10088310, FL2FontPage, Find)

// 0x10088630 (vslot 0) - C2: only a missing file fails (C1 also refused an empty page); Count glyphs of 16
// bytes read one by one, counted in GFontBytesRead
UBOOL FL2FontPage::Load(INT InFirst, INT InCount, FILE* File)
{
    if (!File)
        return 0;
    First = InFirst;
    Count = InCount;
    Chars = (FL2FontChar*)appMalloc(InCount << 4, TEXT("new"));
    for (INT i = 0; i < Count; i++)
        GFontBytesRead += (INT)::nw::StockFread((BYTE*)Chars + i * 0x10, 1, 0x10, File);
    return 1;
}
NW_HOOK_METHOD(0x10088630, FL2FontPage, Load)

// 0x100886b0
void FL2FontPage::Free()
{
    if (Chars)
    {
        appFree(Chars);
        Chars = NULL;
    }
}
NW_HOOK_METHOD(0x100886b0, FL2FontPage, Free)

/*-----------------------------------------------------------------------------
    FL2FontPageEx: the v1.1 pages (20-byte glyphs and a table of 32 words).
-----------------------------------------------------------------------------*/

// 0x100889e0 - the vtable of this class and the base's three zeros (ExChars and Table are left alone)
FL2FontPageEx::FL2FontPageEx()
{
}
NW_HOOK_CTOR(0x100889e0, FL2FontPageEx)

// 0x10088a00 - Free (of the base's Chars, which a v1.1 page never fills: ExChars leaks, as in the stock
// code), then the base destructor (Free again)
FL2FontPageEx::~FL2FontPageEx()
{
    Free();
}
NW_HOOK_DTOR(0x10088a00, FL2FontPageEx)

// 0x100886e0 - like Load, with the page's 128-byte table (read by NL2Font::Load before the glyphs) and
// 20-byte glyphs; a missing file or table fails
UBOOL FL2FontPageEx::LoadEx(INT InFirst, INT InCount, FILE* File, const INT* InTable)
{
    if (!File)
        return 0;
    if (!InTable)
        return 0;
    First = InFirst;
    Count = InCount;
    ExChars = (FL2FontCharEx*)appMalloc(InCount * 0x14, TEXT("new"));
    for (INT i = 0; i < 32; i++)
        Table[i] = InTable[i];
    for (INT i = 0; i < Count; i++)
        GFontBytesRead += (INT)::nw::StockFread((BYTE*)ExChars + i * 0x14, 1, 0x14, File);
    return 1;
}
NW_HOOK_METHOD(0x100886e0, FL2FontPageEx, LoadEx)

// 0x10088340
FL2FontCharEx* FL2FontPageEx::FindEx(TCHAR C)
{
    INT Index = (_WORD)C;
    if (Index >= First && Index < First + Count)
        return &ExChars[Index - First];
    return NULL;
}
NW_HOOK_METHOD(0x10088340, FL2FontPageEx, FindEx)

// 0x10088370 - when C is on this page and its glyph has the variant Mask names (a single bit; the loop finds
// the highest set bit, 32 when there is none), the table's word of that bit; else -1
__declspec(noinline) INT FL2FontPageEx::GetMaskValue(TCHAR C, INT Mask)
{
    INT Index = (_WORD)C;
    if (Index >= First && Index < First + Count)
    {
        INT Bit = 0;
        do
        {
            if ((Mask >> Bit) == 1)
                break;
            Bit++;
        } while (Bit < 32);
        if (ExChars[Index - First].Mask & Mask)
            return Table[Bit];
    }
    return -1;
}
NW_HOOK_METHOD(0x10088370, FL2FontPageEx, GetMaskValue)

/*-----------------------------------------------------------------------------
    The engine templates the font code instantiates.
-----------------------------------------------------------------------------*/

// 0x10088940 (ret 4) - the object's address goes on the list (guarded)
void UGlobalTempObjects::AddGlobalObject(UObject** Object)
{
    guard(UGlobalTempObjects::AddGlobalObject);
    GlobalObjects(GlobalObjects.Add()) = Object;
    unguard;
}
NW_HOOK_METHOD(0x10088940, UGlobalTempObjects, AddGlobalObject)

// 0x100888d0 - UE2's ConstructObject<UFinalBlend> (UnObjBas.h 1194): the class must be a UFinalBlend (or a
// child of it); an Outer of -1 is the transient package
static __declspec(noinline) UFinalBlend* ConstructFinalBlend(UClass* Class, UObject* Outer, FName Name, DWORD SetFlags)
{
    UBOOL bChild = 0;
    for (UField* Test = reinterpret_cast<UField*>(Class); Test; Test = Test->SuperField)
    {
        if (Test == reinterpret_cast<UField*>(UFinalBlend::StaticClass()))
        {
            bChild = 1;
            break;
        }
    }
    NW_CHECK_AT(bChild, "e:\\work\\localization\\2004_12_01\\warfare2110\\core\\inc\\UnObjBas.h", 1229);
    if (Outer == (UObject*)-1)
        Outer = (UObject*)UObject::GetTransientPackage();
    return (UFinalBlend*)UObject::StaticConstructObject(Class, Outer, Name, SetFlags, NULL, GError, NULL);
}
NW_HOOK_FUNC(0x100888d0, ConstructFinalBlend)

/*-----------------------------------------------------------------------------
    NL2Font.
-----------------------------------------------------------------------------*/

// 0x100883c0
NL2Font::NL2Font()
{
    Texture = NULL;
    Header0 = 0;
    Header1 = 0;
    NumPages = 0;
    Pages = NULL;
    PagesEx = NULL;
    Kind = 0;
}
NW_HOOK_CTOR(0x100883c0, NL2Font)

// 0x100885d0 - ConditionalDestroy, then Engine's ~UL2Font
NW_HOOK_DTOR(0x100885d0, NL2Font)

// The font file is opened, read and closed with the stock image's own C runtime (a FILE* of one runtime is no use to
// the other: NwHook.h, StockFread; FL2FontPage::Load reads the same stream): _wfopen 0x1014DD4C, _fopen 0x1014DCDD,
// _fseek 0x1014DC38, _fclose 0x1014DB58
#if NW_HOOK_MODE
static FORCEINLINE FILE* StockOpenW(const TCHAR* Path)
{
    typedef FILE*(__cdecl * Fn)(const TCHAR*, const TCHAR*);
    return reinterpret_cast<Fn>(::nw::Addr(0x10172706))(Path, TEXT("rb"));
}
static FORCEINLINE FILE* StockOpen(const char* Path)
{
    typedef FILE*(__cdecl * Fn)(const char*, const char*);
    return reinterpret_cast<Fn>(::nw::Addr(0x10172697))(Path, "rb");
}
static FORCEINLINE INT StockSeek(FILE* File, INT Offset, INT Origin)
{
    typedef INT(__cdecl * Fn)(FILE*, INT, INT);
    return reinterpret_cast<Fn>(::nw::Addr(0x101725F2))(File, Offset, Origin);
}
static FORCEINLINE INT StockClose(FILE* File)
{
    typedef INT(__cdecl * Fn)(FILE*);
    return reinterpret_cast<Fn>(::nw::Addr(0x10172512))(File);
}
static FORCEINLINE size_t StockRead(void* Buffer, size_t Size, size_t Count, FILE* File)
{
    return ::nw::StockFread(Buffer, Size, Count, File);
}
#else
static FORCEINLINE FILE* StockOpenW(const TCHAR* Path) { return _wfopen(Path, L"rb"); }
static FORCEINLINE FILE* StockOpen(const char* Path) { return fopen(Path, "rb"); }
static FORCEINLINE INT StockSeek(FILE* File, INT Offset, INT Origin) { return fseek(File, Offset, Origin); }
static FORCEINLINE INT StockClose(FILE* File) { return fclose(File); }
static FORCEINLINE size_t StockRead(void* Buffer, size_t Size, size_t Count, FILE* File)
{
    return fread(Buffer, Size, Count, File);
}
#endif

// 0x10088a50 (vslot 26) - 0 = loaded, -1 = no file or a failing page (the file stays open then, as in the stock code).
// A version word ("v1.0" or "v1.1"; any other first four bytes are the first header int, the file is read again from the
// start as a v1.0 one) picks the page table, and the texture is looked up after the file is read
INT NL2Font::Load(const TCHAR* TextureName, const TCHAR* FontFile)
{
    FILE* File;
    if (GUnicodeOS)
        File = StockOpenW(FontFile);
    else
        File = StockOpen(TCHAR_TO_ANSI(FontFile));
    if (!File)
        return -1;

    char Version[8];
    StockRead(Version, 1, 4, File);
    Version[4] = 0;
    if (strcmp(Version, "v1.0") == 0)
    {
        Kind = 0;
        GFontBytesRead += 4;
    }
    else if (strcmp(Version, "v1.1") == 0)
    {
        GFontBytesRead += 4;
        Kind = 1;
    }
    else
    {
        Kind = 0;
        StockSeek(File, 0, SEEK_SET);
    }

    GFontBytesRead += (INT)StockRead(&Header0, 1, 4, File);
    GFontBytesRead += (INT)StockRead(&Header1, 1, 4, File);
    GFontBytesRead += (INT)StockRead(&NumPages, 1, 4, File);
    if (NumPages)
    {
        if (Kind == 0)
        {
            Pages = new FL2FontPage[NumPages];
            for (INT i = 0; i < NumPages; i++)
            {
                INT PageFirst, PageCount;
                GFontBytesRead += (INT)StockRead(&PageFirst, 1, 4, File);
                GFontBytesRead += (INT)StockRead(&PageCount, 1, 4, File);
                FL2FontPage* Page = &Pages[i];
                if (!Page->Load(PageFirst, PageCount, File))
                    return -1;
            }
        }
        else if (Kind == 1)
        {
            PagesEx = new FL2FontPageEx[NumPages];
            for (INT i = 0; i < NumPages; i++)
            {
                INT PageFirst, PageCount;
                INT Table[32];
                GFontBytesRead += (INT)StockRead(&PageFirst, 1, 4, File);
                GFontBytesRead += (INT)StockRead(&PageCount, 1, 4, File);
                GFontBytesRead += (INT)StockRead(Table, 1, 0x80, File);
                if (!PagesEx[i].LoadEx(PageFirst, PageCount, File, Table))
                    return -1;
            }
        }
    }
    StockClose(File);

    Texture = (UTexture*)StaticFindObject(UTexture::StaticClass(), ANY_PACKAGE, TextureName, 0);
    if (!Texture)
        Texture = (UTexture*)StaticLoadObject(UTexture::StaticClass(), NULL, TextureName, NULL, 1, NULL);
    if (Texture)
        Texture->SetFlags(RF_Native);
    return 0;
}
NW_HOOK_METHOD(0x10088a50, NL2Font, Load)

// 0x10088400 (vslot 28) - the first page holding a glyph with a size, in the table the font has
FL2FontChar* NL2Font::GetChar(TCHAR C)
{
    if (Kind == 0)
    {
        if (Pages)
        {
            for (INT i = 0; i < NumPages; i++)
            {
                FL2FontChar* Char = Pages[i].Find(C);
                if (Char && Char->Width > 0 && Char->Height > 0)
                    return Char;
            }
        }
    }
    else if (Kind == 1)
    {
        if (PagesEx)
        {
            for (INT i = 0; i < NumPages; i++)
            {
                FL2FontCharEx* Char = PagesEx[i].FindEx(C);
                if (Char && Char->Width > 0 && Char->Height > 0)
                    return Char;
            }
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x10088400, NL2Font, GetChar)

// 0x100884a0 (vslot 29) - the variant mask of the glyph in the v1.1 table; 0 without that table
INT NL2Font::GetCharMask(TCHAR C)
{
    if (!PagesEx)
        return 0;
    for (INT i = 0; i < NumPages; i++)
    {
        FL2FontCharEx* Char = PagesEx[i].FindEx(C);
        if (Char && Char->Width > 0 && Char->Height > 0)
            return Char->Mask;
    }
    return 0;
}
NW_HOOK_METHOD(0x100884a0, NL2Font, GetCharMask)

// 0x10088500 (vslot 33) - the first page's table word for the mask bit (the glyph on that page has it); 0
INT NL2Font::GetCharMaskValue(TCHAR C, INT Mask)
{
    if (!PagesEx)
        return 0;
    for (INT i = 0; i < NumPages; i++)
    {
        INT Value = PagesEx[i].GetMaskValue(C, Mask);
        if (Value >= 0)
            return Value;
    }
    return 0;
}
NW_HOOK_METHOD(0x10088500, NL2Font, GetCharMaskValue)

// 0x10088590 (vslot 32)
UBOOL NL2Font::HasTexture()
{
    return Texture != NULL;
}
NW_HOOK_METHOD(0x10088590, NL2Font, HasTexture)

// 0x10088790 (vslot 31) - a UFinalBlend over the font texture: made the first time (kept in a global, listed
// in GGlobalTempObjects), refilled every time. Mode 1 and the others have the same code on two globals.
UMaterial* NL2Font::GetMaterial(INT Mode)
{
    if (!GetTexture())
        return NULL;
    UFinalBlend** Slot = (Mode == 1) ? &GFontBlendA : &GFontBlendB;
    if (!*Slot)
    {
        *Slot = ConstructFinalBlend(UFinalBlend::StaticClass(), (UObject*)-1, NAME_None, 0);
        GGlobalTempObjects->AddGlobalObject((UObject**)Slot);
    }
    // UFinalBlend (a UModifier): Material +0x44, FrameBufferBlending +0x48 (2: alpha blend), the bits at +0x4C
    // (ZWrite 1, ZTest 2, AlphaTest 4, TwoSided 8), AlphaRef +0x50 (the class is only forward declared; UObject grew
    // by 0xC in C4: C3 had them at +0x38 .. +0x44)
    nw::Field<UMaterial*>(*Slot, 0x44) = (UMaterial*)GetTexture();
    nw::Field<DWORD>(*Slot, 0x4C) |= 8;
    nw::Field<BYTE>(*Slot, 0x48) = 2;
    nw::Field<DWORD>(*Slot, 0x4C) |= 1;
    nw::Field<DWORD>(*Slot, 0x4C) |= 2;
    nw::Field<DWORD>(*Slot, 0x4C) |= 4;
    nw::Field<BYTE>(*Slot, 0x50) = 0;
    return *Slot;
}
NW_HOOK_METHOD(0x10088790, NL2Font, GetMaterial)

// 0x10088e60 (vslot 30) - the pages of both tables go, the counters and the texture too (the texture loses
// its RF_Native flag): C2 clears the pointers where C1 left them
void NL2Font::Clear()
{
    for (INT i = 0; i < NumPages; i++)
    {
        if (Pages)
            Pages[i].Free();
        if (PagesEx)
            PagesEx[i].Free();
    }
    NumPages = 0;
    Header0 = 0;
    Header1 = 0;
    if (Pages)
    {
        delete[] Pages;
        Pages = NULL;
    }
    if (PagesEx)
    {
        delete[] PagesEx;
        PagesEx = NULL;
    }
    if (Texture)
    {
        Texture->ClearFlags(RF_Native);
        Texture = NULL;
    }
}
NW_HOOK_METHOD(0x10088e60, NL2Font, Clear)

NW_CHECK_VTABLE_FOR(NL2Font, UObject, 0x101b3868, 34)
