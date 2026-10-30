#pragma once
// L2TaskWnd - TaskWnd's Lineage II skin (UClass "L2TaskWnd", 0xEC; C3 only, the messenger): the same narrow list window,
// its list an L2TaskBox and its entries L2Tasks (the map goes from the conversation window (L2ChatWnd) to its L2Task).

#include "nwindow/TaskWnd.h"
#pragma pack(push, 4)

class L2TaskBox;

class L2TaskWnd : public NCWnd
{
    NW_DECLARE_CLASS(L2TaskWnd, NCWnd, 0, 0x10320808)

    NTaskMap Tasks;                  // +0xE8 (name ours) conversation window -> its L2Task window
    L2TaskBox* List;                 // +0xFC 140 x 126 at (14, 3), 6 lines shown
    UTexture* BackTex1;              // +0x100 "L2UI_CH3.msn6_back1"   // C2: offset derived, no evidence
    UTexture* BackTex2;              // +0x104 "L2UI_CH3.msn6_back2"   // C2: offset derived, no evidence
    UTexture* BackTex3;              // +0x108 "L2UI_CH3.msn6_back3"   // C2: offset derived, no evidence

    L2TaskWnd();                                                     // 0x1014aec0
    // ~L2TaskWnd                                                    // 0x1014af60

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x1014ab70
    virtual INT OnDestroy();                                         // 38 0x10149f50
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013fab0 (TaskWnd's, shared)

    // names ours (anonymous in the db)
    void RemoveTask(NCWnd* ChatWindow);                              // 0x1014a030 (ret 4) the L2Task of that conversation window goes
    NCWnd* FindTask(FString Name);                                   // 0x1014a080 (ret 0xC) the L2Task of that friend, or NULL
};
NW_ASSERT_OFFSET(L2TaskWnd, Tasks, 0xE8);
NW_ASSERT_OFFSET(L2TaskWnd, List, 0xFC);
NW_ASSERT_OFFSET(L2TaskWnd, BackTex1, 0x100);
NW_ASSERT_SIZE(L2TaskWnd, 0x10C);

#pragma pack(pop)
