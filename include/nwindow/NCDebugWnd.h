#pragma once
// NCDebugWnd - the debug log list (UClass "CDebugWnd", 0x1E8), fed by NCObject::Debugf.
// Lines are prefixed with their category; the categories come from .\L2Debug.ini
// ([DebugInfo] count, then Library%d: name%d, color%d = R=,G=,B=, IsEnable%d).

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// one debug category (names ours)
struct NDebugCategory
{
    TCHAR Name[32];            // +0x00 (entry 0: "NWindow")
    UBOOL bEnabled;            // +0x40
    DWORD Color;               // +0x44
};
NW_ASSERT_SIZE(NDebugCategory, 0x48);

#if NW_HOOK_MODE
#define GDebugCategories (reinterpret_cast<NDebugCategory*>(::nw::Addr(0x10244660)))
#define GNumDebugCategories NW_GLOBAL(INT, 0x102D3160)
#else
extern NDebugCategory GDebugCategories[];   // 20 in the stock image
extern INT GNumDebugCategories;
#endif

class NCDebugWnd : public NCListBox
{
    NW_DECLARE_CLASS(NCDebugWnd, NCListBox, 0, 0x102D3168)

    NCDebugWnd();                                                    // 0x10084af0 (50, 10)
    NCDebugWnd(INT InMaxLines, INT InVisibleLines);                  // 0x10084b90
    // ~NCDebugWnd                                                   // 0x10084b40

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890
    virtual INT OnCreate();                                          // 37 0x10084760
    virtual INT OnDestroy();                                         // 38 0x100849f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10084770

    // names ours
    void SetBackTexture(const TCHAR* Name);                          // 0x10084790
    void AddLog(INT Category, const TCHAR* Text, INT Unused);        // 0x100847d0
    void LoadCategories();                                           // 0x10084850
};
NW_ASSERT_SIZE(NCDebugWnd, 0x240);

#pragma pack(pop)