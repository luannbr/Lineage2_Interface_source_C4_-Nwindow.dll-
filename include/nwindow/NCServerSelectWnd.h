#pragma once
// NCServerSelectWnd - the server list of the login screen (UClass "CServerSelectWnd", 0xC8;
// NCLoginWnd +0xA0). The login server's list arrives a row at a time (AddServer: name, load,
// ping, type) and ends with OnServerListEnd; OK (or Enter, or a double click) logs in to the
// selected server, Cancel goes back to the login box, "Quality" orders the list: the server
// played last on top, the others by ping and, within a ping class, by users. Pings are asked
// of the network handler and kept in s_info.ini (section = server index, key "Time"), with
// the time they were taken ([Date] Time); the old ones are asked again.

#include "nwindow/NCWnd.h"
#include "nwindow/NCListCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class L2ParamStack;

// NCServerSelectListCtrl - the list itself (UClass "CServerSelectListCtrl", 0x20C): sorting by
// a column also keeps the window's LastServerRow on its server
class NCServerSelectListCtrl : public NCListCtrl
{
    NW_DECLARE_CLASS(NCServerSelectListCtrl, NCListCtrl, 0, 0x10305D40)

    NCServerSelectListCtrl();                                        // 0x101016d0
    NCServerSelectListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused); // 0x10101770
    // ~NCServerSelectListCtrl                                       // 0x10101720

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ffd10
    virtual INT SortByColumn(INT Column);                            // 81 0x101013a0
};
NW_ASSERT_SIZE(NCServerSelectListCtrl, 0x270);

class NCServerSelectWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCServerSelectWnd, NCWnd, 0, 0x10305808)

    UTexture* BackTex;               // +0xE8 "L2UI.lobbywnd.serverback"   // C2: offset derived, no evidence
    NCServerSelectListCtrl* ServerList; // +0xEC
    NCButton* OKButton;              // +0xF0   // C2: offset derived, no evidence
    NCButton* CancelButton;          // +0xF4   // C2: offset derived, no evidence
    NCButton* QualityButton;         // +0xF8   // C2: offset derived, no evidence
    INT LastServerID;                // +0xFC server the account played on last (login server)
    INT LastServerRow;               // +0x100 its row, -1 = none
    INT PendingPings;                // +0x104 ping answers still awaited
    INT TimerResult;                 // +0x108 what SetTimer returned (never read)   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UBOOL bRequested;                // +0x10C a server login went out (cleared by the timer)
    UBOOL bPingCacheOld;             // +0x110 the saved pings are too old: ask every server
    UBOOL bPingSaved;                // +0x114 a ping went to s_info.ini
    UBOOL UnknownC4;                 // +0x118 set at the end of the list when bPingSaved (never read)
    BYTE RowBits[0x10C - 0xFC];      // +0x11C (C3, name ours) a bit per row: AddServer colors the name of the marked   // C2: offset derived, no evidence
                                     //   rows red (set by another window; 128 rows at most)

    NCServerSelectWnd();                                             // 0x101017a0
    // ~NCServerSelectWnd                                            // 0x100c8740 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ffd70
    virtual INT OnTimer(INT TimerID);                                // 36 0x10100090
    virtual INT OnCreate();                                          // 37 0x10101860
    virtual INT OnDestroy();                                         // 38 0x101003a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101004d0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x10100060
    // names ours
    virtual INT OnOK();                                              // 74 0x10100bf0
    virtual INT OnCancel();                                          // 75 0x10100d80
    virtual INT SortByQuality();                                     // 76 0x10100df0
    virtual INT OnListNotify(INT Row);                               // 77 0x10100050

    // names ours
    void SetLastServerID(INT ServerID);                              // 0x100c81a0 (C1 only: no C2 match) (C1 only: no C2 match)
    void OnServerListEnd();                                          // 0x100fffb0
    void ClearServers();                                             // 0x10100020
    void AddServer(L2ParamStack& Params);                            // 0x101005d0
    // the row's number and name text (new in C2, a function of its own; name ours)
    void FormatServerLabel(INT Index, INT Flags, INT TypeIndex, TCHAR* Name, TCHAR* NameText); // 0x100ffe30
    void SetPing(INT IP, INT Ping);                                  // 0x10100b10
    UBOOL IsPingCacheOld();                                          // 0x10100230
    UBOOL HasNoPing(INT ServerIndex);                                // 0x10100280
    void SavePing(INT ServerIndex, INT Ping);                        // 0x101002e0
    void SavePingTime();                                             // 0x10100330

    // calendar of the ping cache (names ours)
    static UBOOL IsLeapYear(INT Year);                               // 0x101000b0
    static INT DaysInYears(INT Years);                               // 0x101000f0
    static INT DaysInMonths(INT Year, INT Month);                    // 0x10100130
    static INT HoursInDays(INT Day, INT Hour);                       // 0x101001a0
    static INT GetTimeStamp();                                       // 0x101001d0
};
NW_ASSERT_OFFSET(NCServerSelectWnd, ServerList, 0xEC);
NW_ASSERT_OFFSET(NCServerSelectWnd, LastServerID, 0xFC);
NW_ASSERT_OFFSET(NCServerSelectWnd, bRequested, 0x10C);
NW_ASSERT_OFFSET(NCServerSelectWnd, UnknownC4, 0x118);
NW_ASSERT_SIZE(NCServerSelectWnd, 0x12C);

class NCHtmlViewer;

// NCServerInfoWnd - the help page under the server list (UClass "CServerInfoWnd", 0xD0; NCLoginServerWnd +0xCC, new in
// C3): a backdrop and an HTML viewer (..\l2text\server_help.htm, loaded unless the Thai HTML mode is on) inside it.
class NCServerInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCServerInfoWnd, NCWnd, 0, 0x103052D8)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.Serverselectwnd.Serverinfo_back"
    NCHtmlViewer* Viewer;            // +0xEC 7 pixels in, the window's size less 14 (name ours)

    NCServerInfoWnd();                                               // 0x1009dfe0 (NCWnd's constructor and the vtable)
    // ~NCServerInfoWnd                                              // 0x1009e030

    virtual INT OnCreate();                                          // 37 0x101015d0
    virtual INT OnDestroy();                                         // 38 0x101012f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101011f0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCServerInfoWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCServerInfoWnd, Viewer, 0xEC);
NW_ASSERT_SIZE(NCServerInfoWnd, 0xF0);

#pragma pack(pop)