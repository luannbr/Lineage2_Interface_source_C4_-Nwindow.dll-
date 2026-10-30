#pragma once
// NCLoadingWnd - the loading screen while entering the world (UClass "CLoadingWnd", 0x98): a
// 1024 x 768 picture stretched over the whole window. NConsoleWnd::RequestGameStart picks the
// picture: in Korean one per age rating of the chosen server, elsewhere the country's entry of
// FontInfo.ini.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCLoadingWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLoadingWnd, NCWnd, 0, 0x102DC308)

    UTexture* LoadingTex;            // +0xE8

    NCLoadingWnd();                                                  // 0x1009a850
    // ~NCLoadingWnd                                                 // 0x1009a8a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate() { return 1; }                             // 37 0x100f7850 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009a7b0

    void SetLoadingTexture(INT ScreenID);                            // 0x1009a600 (name ours)
    void SetTextureName(FString Name);                               // 0x1009a700 (new in C4, name ours; ret 0xC) the picture by name
};
NW_ASSERT_OFFSET(NCLoadingWnd, LoadingTex, 0xE8);
NW_ASSERT_SIZE(NCLoadingWnd, 0xEC);

#pragma pack(pop)