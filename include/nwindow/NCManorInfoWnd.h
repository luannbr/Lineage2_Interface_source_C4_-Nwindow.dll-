#pragma once
// NCManorInfoWnd - the manor information window (new in C4; UClass "CManorInfoWnd", 0xF8; the desktop keeps it at +0x1EC):
// a title bar, the tab bar (NCManorInfoUpWnd, on top) and the three pages the tabs show, one at a time over the same place
// (NCManorSeedInfoWnd, NCManorCropInfoWnd, NCManorDefaultInfoWnd). The console's ReceiveManorSeedInfo / CropInfo /
// DefaultInfo hand the packet to the matching page here, the window comes up and the tab of the page is chosen.

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCManorInfoUpWnd;
class NCManorSeedInfoWnd;
class NCManorCropInfoWnd;
class NCManorDefaultInfoWnd;

class NCManorInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorInfoWnd, NCWnd, 0, 0x10317B88)

    NCManorInfoUpWnd* UpWnd;         // +0xE8 the tab bar, at (0, 20), the width of this window x 28
    NCWnd* Pages[3];                 // +0xEC NCManorSeedInfoWnd, NCManorCropInfoWnd, NCManorDefaultInfoWnd: at (0, 48), the width x 278

    NCManorInfoWnd();                                                // 0x1005f430
    // ~NCManorInfoWnd                                               // 0x1005f480

    // WindowProc (25) is NControl's: this class does not override it
    virtual INT OnCreate();                                          // 37 0x10137a20
    virtual INT OnDestroy();                                         // 38 0x10137620

    // names ours
    NCWnd* GetPage(INT Index);                                       // 0x10137600 (ret 4) page 0..2, NULL outside (anonymous in the db)
    void OnShowSeedInfo(L2ParamStack* Params);                       // 0x10137730 (ret 4) the seed page filled, shown, tab 0
    void OnShowCropInfo(L2ParamStack* Params);                       // 0x10137800 (ret 4) the crop page filled, shown, tab 1
    void OnShowManorDefaultInfo(L2ParamStack* Params);               // 0x101378c0 (ret 4) the default page filled, shown, tab 2
};
// the casts the manor windows make through the stock's out-of-line instances (cdecl, anonymous in the db; UE2's Cast: the window
// when it is of the class or of a derived one, else NULL; CastChecked logs "Cast of %s to %s failed" and gives the window back as
// it is)
NCManorInfoWnd* __cdecl NManorCastChecked(UObject* Src);              // 0x10137100 CastChecked<NCManorInfoWnd>
NCManorSeedInfoWnd* __cdecl NManorCastSeedInfoWnd(UObject* Src);      // 0x101370A0
NCManorCropInfoWnd* __cdecl NManorCastCropInfoWnd(UObject* Src);      // 0x101370D0
NCManorDefaultInfoWnd* __cdecl NManorCastDefaultInfoWnd(UObject* Src);   // 0x10137700

NW_ASSERT_OFFSET(NCManorInfoWnd, UpWnd, 0xE8);
NW_ASSERT_OFFSET(NCManorInfoWnd, Pages, 0xEC);
NW_ASSERT_SIZE(NCManorInfoWnd, 0xF8);

#pragma pack(pop)
