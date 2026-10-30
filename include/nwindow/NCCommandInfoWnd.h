#pragma once
// NCCommandInfoWnd - the command channel's information page (UClass "CCommandInfoWnd", 0x118; C4 only): the channel master's name,
// the number of parties and of members, the list of the parties' leaders (the master can oust one) and three buttons: refresh
// (ask the server again), oust the selected party, leave the channel. The command channel window (NCCommandChannelWnd) makes it
// and shows it on its second tab; the server's channel info fills it (the console).
//
// Layout and virtuals from the stock constructor 0x10052200, OnCreate 0x10051C50, OnDestroy 0x100514E0, OnPaint
// 0x10051610, WindowProc 0x10051020 and the three handlers (new virtuals 74 .. 76).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;
class NCListCtrl;

class NCCommandInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCCommandInfoWnd, NCWnd, 0, 0x102CFD40)

    INT Unknown0E8;                  // +0xE8 (nothing reads it)
    NCListCtrl* PartyList;           // +0xEC the parties' leaders, two columns: the name and the number of members (names ours)
    NCButton* RefreshButton;         // +0xF0 id 0xF0A0C7, SysString 938 at (244, 62); disabled for a while after a request
    NCButton* OustButton;            // +0xF4 id 0xF0A0C8, SysString 681 at (244, 86)
    NCButton* LeaveButton;           // +0xF8 id 0xF0A0C9, SysString 1268 at (244, 110)
    FString MasterName;              // +0xFC the channel master's name (ClearInfo empties it)
    INT PartyCount;                  // +0x108 the numbers OnPaint draws after SysString 1266 and 1267
    INT MemberCount;                 // +0x10C
    INT RefreshTimer;                // +0x110 SetTimer(0x1F9, 3000)'s result (OnCreate)
    BYTE bCanRefresh;                // +0x114 the refresh button works (set by ClearInfo and by the timer, cleared by a request)

    NCCommandInfoWnd();                                              // 0x10052200
    // ~NCCommandInfoWnd                                             // 0x10052280

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10051020
    virtual INT OnTimer(INT TimerID);                                // 36 0x10051270
    virtual INT OnCreate();                                          // 37 0x10051C50
    virtual INT OnDestroy();                                         // 38 0x100514E0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10051610
    // names ours
    virtual INT OnRefreshBtnClick();                                 // 74 0x100517E0 the request, see RequestInfo
    virtual INT OnOustBtnClick();                                    // 75 0x100510C0 the selected party's leader is thrown out of the channel
    virtual INT OnLeaveBtnClick();                                   // 76 0x10051170 the player leaves the channel

    // 0x10051220 (anonymous in the db; name ours): nobody, nothing in the list, the refresh button on
    __declspec(noinline) void ClearInfo();
    // 0x100512B0 (anonymous in the db; name ours): when the refresh button works, the server is asked for the channel's info
    // (user command 0x61) and the button is off until the timer turns it on again
    __declspec(noinline) void RequestInfo();
};
NW_ASSERT_OFFSET(NCCommandInfoWnd, PartyList, 0xEC);
NW_ASSERT_OFFSET(NCCommandInfoWnd, MasterName, 0xFC);
NW_ASSERT_OFFSET(NCCommandInfoWnd, PartyCount, 0x108);
NW_ASSERT_OFFSET(NCCommandInfoWnd, RefreshTimer, 0x110);
NW_ASSERT_OFFSET(NCCommandInfoWnd, bCanRefresh, 0x114);
NW_ASSERT_SIZE(NCCommandInfoWnd, 0x118);

#pragma pack(pop)
