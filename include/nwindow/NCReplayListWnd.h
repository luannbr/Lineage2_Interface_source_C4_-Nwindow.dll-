#pragma once
// NCReplayListWnd - the list of the saved replays (UClass "CReplayListWnd", 0x104; new in C4): the files of ..\REPLAY\*.L2R in a
// list (NCReplayListCtrl, 300 x 273 at (8, 11)), two check boxes under it (the options OnPublicLoad takes) and three buttons
// (play / delete / cancel). Playing hands the selected name to the engine's replay manager (FL2ReplayManager::OnPublicLoad), deleting
// removes the file and fills the list again. Return plays, Escape cancels.
//
// Layout and virtuals from the stock constructor 0x100F7F30, OnCreate 0x100F7FD0, OnDestroy 0x100F7860, OnPaint 0x100F79E0, WindowProc
// 0x100F7700 and the small methods 0x100F77E0 .. 0x100F7DE0 (names ours: the stock has no strings for them).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCReplayListCtrl;
class NCCheckBox;
class NCButton;

class NCReplayListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCReplayListWnd, NCWnd, 0, 0x102FFA78)

    NCReplayListCtrl* ListCtrl;      // +0xE8 (names ours for all the children) id 0xF030
    NCCheckBox* Check1;              // +0xEC id 0xF035, text 1249 (the second argument of OnPublicLoad is its state)
    NCCheckBox* Check2;              // +0xF0 id 0xF036, text 1253 (the third argument)
    NCButton* PlayButton;            // +0xF4 id 0xF031, text 140
    NCButton* DeleteButton;          // +0xF8 id 0xF032, text 1272
    NCButton* CancelButton;          // +0xFC id 0xF033, text 141
    UTexture* BackTex;               // +0x100 "L2UI_CH3.ReplayListWnd.replay_back" (the constructor clears it)

    NCReplayListWnd();                                               // 0x100f7f30
    // ~NCReplayListWnd                                              // 0x100f7f80

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100f7700
    virtual INT OnCreate();                                          // 37 0x100f7fd0
    virtual INT OnDestroy();                                         // 38 0x100f7860
    virtual void ShowWindow();                                       // 48 0x100f7dd0 the list is filled again, then NCWnd's
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f79e0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x100f77e0
    virtual INT OnListDblClick(INT Param);                           // 74 0x100f7820 a double click on a row plays it
    virtual INT OnPlayBtnClick();                                    // 75 0x100f7ae0 (names ours)
    virtual INT OnDeleteBtnClick();                                  // 76 0x100f7de0
    virtual INT OnCancelBtnClick();                                  // 77 0x100f7830
    virtual INT OnCheckClick();                                      // 78 0x100f7850 (folded: returns 1)

    void Refresh();                                                  // 0x100f7c70 (anonymous, name ours) the list from the files
};
NW_ASSERT_OFFSET(NCReplayListWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCReplayListWnd, Check1, 0xEC);
NW_ASSERT_OFFSET(NCReplayListWnd, PlayButton, 0xF4);
NW_ASSERT_OFFSET(NCReplayListWnd, CancelButton, 0xFC);
NW_ASSERT_OFFSET(NCReplayListWnd, BackTex, 0x100);
NW_ASSERT_SIZE(NCReplayListWnd, 0x104);

#pragma pack(pop)
