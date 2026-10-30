#pragma once
// NL2Font - L2's bitmap font (UClass "L2Font", 0x40; implements Engine's UL2Font, not an
// NCObject). Load reads the glyph table file: a version word ("v1.0", "v1.1", or none: v1.0), two
// header ints, the page count, then per page its first character, its size and that many glyphs;
// the glyphs index into Texture.
//
// C2 has TWO page tables (Kind, +0x30, says which one a font has): v1.0 pages (FL2FontPage: 16-byte
// glyphs, as in C1) and v1.1 pages (FL2FontPageEx: 20-byte glyphs with a mask of variants and a table
// of 32 words per page). Both page classes have a vtable whose only slot is Load.

#include "nwindow/NWindow.h"

#include <stdio.h>
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// one glyph of the font file (field names tentative: the wrap code uses Width as the advance)
struct FL2FontChar
{
    INT X;
    INT Width;
    INT Y;
    INT Height;
};

// one glyph of a v1.1 font file: the 16 bytes above and the mask of the variants it has (name ours)
struct FL2FontCharEx : public FL2FontChar
{
    INT Mask;               // +0x10
};
NW_ASSERT_SIZE(FL2FontCharEx, 0x14);

// a run of consecutive characters (names ours)
struct FL2FontPage
{
    INT First;                                              // +0x04 (the vtable is +0x00)
    INT Count;                                              // +0x08
    FL2FontChar* Chars;                                     // +0x0C

    FL2FontPage();                                          // 0x10072460
    ~FL2FontPage();                                         // 0x100728e0 (vtable, then Free)
    virtual UBOOL Load(INT InFirst, INT InCount, FILE* File);   // vslot 0, 0x10072540 (both page classes)
    FL2FontChar* Find(TCHAR C);                             // 0x10072220
    void Free();                                            // 0x100725c0
};
NW_ASSERT_SIZE(FL2FontPage, 0x10);

// a page of a v1.1 font: the glyphs have the extra word, and the 128 bytes the file keeps before
// them are the page's table (name ours)
struct FL2FontPageEx : public FL2FontPage
{
    FL2FontCharEx* ExChars;                                 // +0x10 (Count glyphs; Free does not free it: stock leak)
    INT Table[32];                                          // +0x14 one word per mask bit

    FL2FontPageEx();                                        // 0x100728f0
    ~FL2FontPageEx();                                       // 0x10072910
    UBOOL LoadEx(INT InFirst, INT InCount, FILE* File, const INT* InTable);   // 0x100725f0
    FL2FontCharEx* FindEx(TCHAR C);                         // 0x10072250
    INT GetMaskValue(TCHAR C, INT Mask);                    // 0x10072280
};
NW_ASSERT_SIZE(FL2FontPageEx, 0x94);

#if NW_HOOK_MODE
#define GFontBytesRead NW_GLOBAL(INT, 0x102D4B68)   // bytes fread by the font loader (name ours)
#define GFontBlendA    NW_GLOBAL(UFinalBlend*, 0x102D50A0)   // GetMaterial(1) (names ours)
#define GFontBlendB    NW_GLOBAL(UFinalBlend*, 0x102D509C)   // GetMaterial(other)
#else
extern INT GFontBytesRead;
extern UFinalBlend* GFontBlendA;
extern UFinalBlend* GFontBlendB;
#endif

// UE2's global list of the temporary objects the engine clears at a level change; the C2 build has its
// AddGlobalObject out of line (the engine class is only forward declared in include/ue2; the array is
// at +0x28)
class UGlobalTempObjects : public UObject
{
public:
    TArray<UObject**> GlobalObjects;    // +0x34 (C3 +0x28: UObject grew by 0xC)

    void AddGlobalObject(UObject** Object);                // 0x10072850
};
NW_ASSERT_OFFSET(UGlobalTempObjects, GlobalObjects, 0x34);

class NL2Font : public UL2Font
{
    NW_DECLARE_CLASS(NL2Font, UL2Font, CLASS_Config, 0x102D4B70)

    UTexture* Texture;         // +0x38
    INT Kind;                  // +0x3C 0 = v1.0 file (Pages), 1 = v1.1 file (PagesEx)
    INT Header0;               // +0x40 first int of the font file after the version word (name ours)
    INT Header1;               // +0x44 second int
    INT NumPages;              // +0x48
    FL2FontPage* Pages;        // +0x4C NumPages pages of a v1.0 font (new[], GMalloc)
    FL2FontPageEx* PagesEx;    // +0x50 NumPages pages of a v1.1 font

    NL2Font();                                                             // 0x100883c0
    // ~NL2Font                                                            // 0x100885d0

    virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) { return 0; }  // FExec 0x100016f0 (folded)
    virtual INT Load(const TCHAR* TextureName, const TCHAR* FontFile);     // 26 0x10088a50
    virtual UTexture* GetTexture();                                        // 27 0x10088580
    virtual FL2FontChar* GetChar(TCHAR C);                                 // 28 0x10088400
    virtual INT GetCharMask(TCHAR C);                                      // 29 0x100884a0
    virtual void Clear();                                                  // 30 0x10088e60
    virtual UMaterial* GetMaterial(INT Mode);                              // 31 0x10088790
    virtual UBOOL HasTexture();                                            // 32 0x10088590
    virtual INT GetCharMaskValue(TCHAR C, INT Mask);                       // 33 0x10088500
};
NW_ASSERT_OFFSET(NL2Font, Texture, 0x38);
NW_ASSERT_OFFSET(NL2Font, Kind, 0x3C);
NW_ASSERT_OFFSET(NL2Font, NumPages, 0x48);
NW_ASSERT_OFFSET(NL2Font, Pages, 0x4C);
NW_ASSERT_OFFSET(NL2Font, PagesEx, 0x50);
NW_ASSERT_SIZE(NL2Font, 0x54);

#pragma pack(pop)
