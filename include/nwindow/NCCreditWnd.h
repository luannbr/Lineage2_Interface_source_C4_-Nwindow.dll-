#pragma once
// NCCreditWnd - the credits (UClass "CCreditWnd", 0xBBC; C2 only, NCLoginWnd's "Credits" button):
// a full-screen picture per page with an HTML text over it (NCHtmlViewer at +0x1A8), and one
// of five sounds of ALAudio.int ([Credit] CreditSound1..5) as music. The pages come from
// FL2GameData::CreditTable (a map of page number -> record: seconds shown, picture name, text
// file). A click or the timer goes to the next page, Escape ends it and gives the music and the
// volume back.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlViewer;

// One page of the credits (name ours): FL2GameData::CreditTable's values (from the code that reads them)
struct NCreditPage
{
    INT Unknown0;                    // +0x00
    FString File;                    // +0x04 the text file, under "..\L2Text\"
    FString Texture;                 // +0x10 the picture's name
    INT Seconds;                     // +0x1C how long the page stays
    INT Align;                       // +0x20 where the text goes: 1..9, rows of three over the screen (1 = top left,
                                     //   5 = centered), anything else as 1
};
NW_ASSERT_SIZE(NCreditPage, 0x24);

// a pair of the table's TMap<INT, NCreditPage*> (name ours): UE2's TMap keeps the pairs in an array and hashes
// their indexes
struct NCreditPair
{
    INT HashNext;                    // +0x00 the next pair of the same hash, -1 = none
    UINT Key;                        // +0x04 the page's number
    NCreditPage* Value;              // +0x08
};
NW_ASSERT_SIZE(NCreditPair, 0xC);

// FL2GameData::CreditTable (name ours): the map of page number -> page; its element count is at +4
struct NCreditTable
{
    NCreditPair* Pairs;              // +0x00 the pairs (TArray: Data, Num, Max)
    INT Num;                         // +0x04
    INT Max;                         // +0x08
    INT* Hash;                       // +0x0C the first pair of each hash value
    INT HashCount;                   // +0x10 a power of two

    // 0x1006e300 (ret 4): the address of the value of Key's pair, NULL when there is none
    NCreditPage** Find(const UINT* Key);
};

class NCCreditWnd : public NControl
{
    NW_DECLARE_CLASS(NCCreditWnd, NControl, 0, 0x102D2C30)

    UTexture* BackTex;               // +0x1F4 the page's picture (stretched over the window)
    NCHtmlViewer* Viewer;            // +0x1F8 the text (30, 30, 400 x 550)
    FLOAT Elapsed;                   // +0x1FC seconds on this page (name ours)
    INT Page;                        // +0x200 page number (name ours)
    TCHAR SoundName[5][255];         // +0x204 the five [Credit] CreditSound<n> entries ("intro" when missing)   // C2: offset derived, no evidence
    FLOAT SavedVolume;               // +0xBFC the sound volume before the credits (ShowWindow; C2: truncated to an INT)
    INT PageSeconds;                 // +0xC00 how long this page stays (the table's field at +0x1C)
    INT MusicHandle;                 // +0xC04 what PlayMusic returned (0 = none)
    INT PageCount;                   // +0xC08 the table's element count (OnCreate counts it one by one)

    NCCreditWnd();                                                   // 0x10084670
    // ~NCCreditWnd                                                  // 0x1006e100 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT OnCreate();                                          // 37 0x100844e0
    virtual INT OnDestroy();                                         // 38 0x10083e40
    virtual void ShowWindow();                                       // 48 0x10083dd0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10083e80
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x10083f20
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10084260

    // names ours
    void FreeFont();                                                 // 0x10083dc0 NCGDevice::DestroyFont(2)
    __declspec(noinline) void PlaceViewer(INT Align);                // 0x10083c00 the text's rectangle by a page's Align (ret 4)
    void NextPage();                                                 // 0x10084060 the next page, or the end of the credits
    void Tick(FLOAT DeltaTime);                                      // 0x10084280 time on the page; the music; NextPage when over (ret 4)
    void Start();                                                    // 0x10084370 loads "CreditFont.gly" and the first page
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCCreditWnd, BackTex, 0x1F4);
NW_ASSERT_OFFSET(NCCreditWnd, Viewer, 0x1F8);
NW_ASSERT_OFFSET(NCCreditWnd, Page, 0x200);
NW_ASSERT_OFFSET(NCCreditWnd, SoundName, 0x204);
NW_ASSERT_OFFSET(NCCreditWnd, SavedVolume, 0xBFC);
NW_ASSERT_OFFSET(NCCreditWnd, PageCount, 0xC08);
NW_ASSERT_SIZE(NCCreditWnd, 0xC0C);

#pragma pack(pop)
