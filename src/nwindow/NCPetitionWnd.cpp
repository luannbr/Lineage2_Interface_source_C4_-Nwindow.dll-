/*=============================================================================
    NCPetitionWnd.cpp - the petition windows, the player's and the GM's. Stock: 0x100472e0 ..
    0x1004737f, 0x10094380 .. 0x100947d3, 0x100949c0 .. 0x10094bcd, 0x10094f10 ..
    0x10095587 and NCGMPetitionWnd's 0x1003e810 .. 0x1003e8af (ShowWindow is
    NCSystemMenuWnd's body, HideWindow NCChatWnd's).
=============================================================================*/
#include "nwindow/NCPetitionWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCTextListBoxC4.h"
#include "nwindow/NCommandParser.h"
#include "nwindow/NConsoleWnd.h"

// chat types of the petition conversation (names ours)
enum { CHAT_Petition = 6, CHAT_GMPetition = 7 };

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// the conversation list of both windows (repeated in the stock OnCreates): C4: 17 lines seen, 200 kept, an NCTextListBox,
// a bare list (NWS_NOHITTEST, not NWS_VISIBLE) that follows the last line
static FORCEINLINE NCListBox* CreatePetitionList(NCWnd* Parent, INT ID)
{
    NCListBox* List = NewTextListBox(200, 17, 15, 1);
    List->ID = ID;
    List->SetPosition(8, 37, 294, 312);
    Parent->CreateChild(List, NWS_NOHITTEST);
    List->bAutoScroll = 1;
    List->LineGap = 0;
    return List;
}

// the line to write in (repeated in the stock OnCreates): C4 takes 1024 characters
static FORCEINLINE NCEditBox* CreatePetitionEdit(NCWnd* Parent, INT ID)
{
    NCEditBox* Edit = new NCEditBox(1, NULL, 1, 0, 0);
    Edit->ID = ID;
    Edit->SetPosition(7, 357, 296, 17);
    Parent->CreateChild(Edit, NWS_VISIBLE);
    Edit->SetMaxLength(0x400);
    return Edit;
}

// a button under the list (repeated in the stock OnCreates): 76 x 23
static FORCEINLINE NCButton* CreatePetitionButton(NCWnd* Parent, INT ID, INT X, const TCHAR* Normal, const TCHAR* Pushed)
{
    NCButton* Button = new NCButton(Normal, Pushed, 0);
    Button->ID = ID;
    Button->SetPosition(X, 383, 76, 23);
    Parent->CreateChild(Button, NWS_VISIBLE);
    return Button;
}

// NCFrameCtrl's constructor (0x100126E0, three arguments, ret 0xc) runs on the object that `new` allocates
static NCFrameCtrl* NewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// the title bar of both windows (repeated in the stock OnCreates): as wide as the window, 20 high
static FORCEINLINE void CreatePetitionFrame(NCWnd* Wnd, INT ID)
{
    Wnd->FrameCtrl = NewFrameCtrl(1, 0, 0);
    Wnd->FrameCtrl->ID = ID;
    Wnd->FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Wnd->Width, 20);
    Wnd->CreateChild(Wnd->FrameCtrl, NWS_VISIBLE);
}

// the background under the title bar (one stock body for both windows, 0x100d0c20); the texture is not
// checked. C3: the size is the window's (the title bar's 20 pixels off the height)
static FORCEINLINE INT PaintPetitionWnd(NCWnd* Wnd, UTexture* BackTex, NCGDevice* Device)
{
    Device->BeginPaint(Wnd->ScreenX, Wnd->ScreenY);
    Device->SetClip(Wnd->ScreenX, Wnd->ScreenY, (INT)Wnd->Width, (INT)Wnd->Height);
    INT H = (INT)(Wnd->Height - 20.0f);
    INT W = (INT)Wnd->Width;
    Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}

// a line of the conversation: the player's own petition lines (type 6) in white
static FORCEINLINE void AddPetitionMessage(NCListBox* List, TCHAR* Text, DWORD Color, INT Type)
{
    if (!List)
        return;
    if (Type == CHAT_Petition)
        List->AddString(Text, 0xFFDCDCDC, -1);
    else
        List->AddString(Text, Color, -1);
}

/*-----------------------------------------------------------------------------
    NCPetitionWnd
-----------------------------------------------------------------------------*/

// 0x100823b0 - NCWnd's constructor and the vtable
NCPetitionWnd::NCPetitionWnd()
{
}
NW_HOOK_CTOR(0x100823b0, NCPetitionWnd)

// 0x10082400
NW_HOOK_DTOR(0x10082400, NCPetitionWnd)

// 0x100dfb40 (vslot 25) - C3: the buttons are Send (69), Cancel (70), the new one (71) and the line (68)
INT NCPetitionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_PETITION_SEND)
            return OnSend() != 0;
        if (wParam == NID_PETITION_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_PETITION_NEWBUTTON)
            return OnNewButton() != 0;
        if (wParam == NID_PETITION_EDIT)
            return OnEdit((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100dfb40, NCPetitionWnd, WindowProc)

// 0x100e13a0 (vslot 37) - C4: the list 294 x 312 at (8, 37), the line at (7, 357) and the buttons "Cancel petition" (473)
// and a new one (text 849, disabled)
INT NCPetitionWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PetitionWnd.petition2_back"), 1);
    CreatePetitionFrame(this, NID_PETITION_FRAME);
    ListBox = CreatePetitionList(this, NID_PETITION_LIST);
    EditBox = CreatePetitionEdit(this, NID_PETITION_EDIT);
    CancelButton = CreatePetitionButton(this, NID_PETITION_CANCEL, 80, TEXT("L2ui_ch3.button.btn1_normal"),
                                        TEXT("L2ui_ch3.button.btn1_normalon"));
    CancelButton->SetTextID(473);
    NewButton = CreatePetitionButton(this, NID_PETITION_NEWBUTTON, 160, TEXT("L2ui_ch3.button.btn1_normal"),
                                     TEXT("L2ui_ch3.button.btn1_normalon"));
    NewButton->SetTextID(849);
    NewButton->EnableWindow(0);
    return 1;
}
NW_HOOK_METHOD(0x100e13a0, NCPetitionWnd, OnCreate)

// 0x100e0290 (vslot 38) - the list, the line, the two buttons; the title bar is left alone
INT NCPetitionWnd::OnDestroy()
{
    guard(NCPetitionWnd::OnDestroy);
    DestroyChild(this, ListBox);
    DestroyChild(this, EditBox);
    DestroyChild(this, CancelButton);
    DestroyChild(this, NewButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100e0290, NCPetitionWnd, OnDestroy)

// 0x100aa860 (vslot 44) - NCSystemMenuWnd's body, shared (restored from its icon, the window comes back where the icon was);
// the hook build jumps to it (hooked in NCSystemMenuWnd.cpp), the standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED void NCPetitionWnd::ShowWindow() NW_STOCK(0x100b71f0)
#else
void NCPetitionWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (DWORD)(INT)Width, (DWORD)(INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}
#endif

// 0x100e0460 (vslot 49) - C3: the new button is disabled again
void NCPetitionWnd::HideWindow()
{
    NewButton->EnableWindow(0);
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100e0460, NCPetitionWnd, HideWindow)

// 0x100d0c20 (vslot 56) - NCGMPetitionWnd's body, shared (hooked there); the standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED INT NCPetitionWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100e03c0)
#else
INT NCPetitionWnd::OnPaint(NCGDevice* Device)
{
    return PaintPetitionWnd(this, BackTex, Device);
}
#endif

// 0x100dfc00 (vslot 74, ret 4) - Enter: a line goes to the GM (chat type 6) when it is not empty, then the line is emptied
INT NCPetitionWnd::OnEdit(LPARAM lParam)
{
    if (lParam == 1)
    {
        const TCHAR* Text = EditBox->CommitText();
        if (Text && appStrlen(Text) > 0)
            GConsole->SendChatMsgToServer(CHAT_Petition, (TCHAR*)Text, NULL);
        EditBox->ClearText();
    }
    return 1;
}
NW_HOOK_METHOD(0x100dfc00, NCPetitionWnd, OnEdit)

// 0x100e0480 (vslot 76) - the new button goes disabled, the server is asked to cancel the petition
INT NCPetitionWnd::OnCancel()
{
    NewButton->EnableWindow(0);
    GConsole->RequestPetitionCancel();
    return 1;
}
NW_HOOK_METHOD(0x100e0480, NCPetitionWnd, OnCancel)

// 0x100dfca0 (vslot 77, name ours) - the console's window at +0x3C04 is shown (the header names that field PetitionWnd and
// the next one WarningDialogBox; the stock code of this client reads +0x3C04 here)
INT NCPetitionWnd::OnNewButton()
{
    ::nw::Field<NCWnd*>(GConsole, 0x3C04)->ShowWindow();
    return 1;
}
NW_HOOK_METHOD(0x100dfca0, NCPetitionWnd, OnNewButton)

// 0x100dfe90 (ret 0x10) - Param is not used; also the desktop's calls on NCGMPetitionWnd. Not inlined: the stock OnEdits
// of C3 called it
__declspec(noinline) void NCPetitionWnd::AddMessage(TCHAR* Text, DWORD Color, INT Type, INT Param)
{
    AddPetitionMessage(ListBox, Text, Color, Type);
}
NW_HOOK_METHOD(0x100dfe90, NCPetitionWnd, AddMessage)
// 0x100dfc60 (ret 0x10; anonymous in the db, wave C) - the same code a second time: this client has the player's window's own copy (it sits
// between OnEdit 0x100dfc00 and OnNewButton 0x100dfca0; the one above lies among NCGMPetitionWnd's methods, 0x100dfcc0 .. 0x100dfed0, and is
// the GM window's). The conversation line goes to the list at +0xF0 with the type's color (6: light grey) and the line kept at -1. The stock
// callers (NCChatWnd::AddChatMessage, NCConsole::Say, NCPetitionBeginWnd::OnSend) reach it through StockCallAt
NW_HOOK_METHOD(0x100dfc60, NCPetitionWnd, AddMessage)

// 0x100dfed0 - the line and the conversation empty; also NCGMPetitionWnd's
void NCPetitionWnd::Clear()
{
    EditBox->ClearText();
    ListBox->Clear();
}
NW_HOOK_METHOD(0x100dfed0, NCPetitionWnd, Clear)

NW_CHECK_VTABLE(NCPetitionWnd, 0x101b2178, 78)

/*-----------------------------------------------------------------------------
    NCGMPetitionWnd
-----------------------------------------------------------------------------*/

// 0x1005f230 - NCWnd's constructor and the vtable
NCGMPetitionWnd::NCGMPetitionWnd()
{
}
NW_HOOK_CTOR(0x1005f230, NCGMPetitionWnd)

// 0x1005f280
NW_HOOK_DTOR(0x1005f280, NCGMPetitionWnd)

// 0x100dfcc0 (vslot 25) - C2 no longer refuses the messages of a disabled window here
INT NCGMPetitionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_GMPETITION_START)
            return OnStart() != 0;
        if (wParam == NID_GMPETITION_END)
            return OnEnd() != 0;
        if (wParam == NID_GMPETITION_EDIT)
            return OnEdit((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100dfcc0, NCGMPetitionWnd, WindowProc)

// 0x100e1780 (vslot 37) - the same layout as the player's, with the buttons "Start" (474) and "End" (475)
INT NCGMPetitionWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PetitionWnd.petition2_back"), 1);
    CreatePetitionFrame(this, NID_GMPETITION_FRAME);
    ListBox = CreatePetitionList(this, NID_GMPETITION_LIST);
    EditBox = CreatePetitionEdit(this, NID_GMPETITION_EDIT);
    StartButton = CreatePetitionButton(this, NID_GMPETITION_START, 80, TEXT("L2UI_ch3.button.btn1_normal"),
                                       TEXT("L2UI_ch3.button.btn1_normalon"));
    StartButton->SetTextID(474);
    EndButton = CreatePetitionButton(this, NID_GMPETITION_END, 160, TEXT("L2UI_ch3.button.btn1_normal"),
                                     TEXT("L2UI_ch3.button.btn1_normalon"));
    EndButton->SetTextID(475);
    return 1;
}
NW_HOOK_METHOD(0x100e1780, NCGMPetitionWnd, OnCreate)

// 0x100e04b0 (vslot 38) - the list, the line and the two buttons go; the title bar is left alone
INT NCGMPetitionWnd::OnDestroy()
{
    guard(NCGMPetitionWnd::OnDestroy);
    DestroyChild(this, ListBox);
    DestroyChild(this, EditBox);
    DestroyChild(this, StartButton);
    DestroyChild(this, EndButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100e04b0, NCGMPetitionWnd, OnDestroy)

// 0x100aa860 (vslot 44) - NCSystemMenuWnd's body, shared; the standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED void NCGMPetitionWnd::ShowWindow() NW_STOCK(0x100b71f0)
#else
void NCGMPetitionWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (DWORD)(INT)Width, (DWORD)(INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}
#endif

// 0x100412d0 (vslot 45) - NCChatWnd's body, shared: NCWnd::HideWindow (the stock body at 0x100471a0 is a `jmp 0x10035ab0`, NCWnd's)
#if NW_HOOK_MODE
NW_NAKED void NCGMPetitionWnd::HideWindow() NW_STOCK(0x100471a0)
#else
void NCGMPetitionWnd::HideWindow()
{
    NCWnd::HideWindow();
}
#endif

// 0x100e03c0 (vslot 62) - the body of both windows
INT NCGMPetitionWnd::OnPaint(NCGDevice* Device)
{
    return PaintPetitionWnd(this, BackTex, Device);
}
NW_HOOK_METHOD(0x100e03c0, NCGMPetitionWnd, OnPaint)

// 0x100dfd60 (vslot 74, ret 4) - Enter: a "//" line goes as a builder command, a plain one as the GM's petition chat
// (type 7), another /command nowhere; the line is emptied. C4 has no limit on the length here
INT NCGMPetitionWnd::OnEdit(LPARAM lParam)
{
    if (lParam != 1)
        return 1;
    const TCHAR* Text = EditBox->CommitText();
    if (!Text)
        return 1;
    TCHAR Line[1024];
    appStrcpy(Line, Text);
    NCommandParser Parser;
    INT Command = Parser.GetCommand(Line);
    if (Command == NCMD_Builder)
    {
        L2ParamStack Params(10);
        Params.PushBack(Line + 2);
        ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand (slot 194 of this console)
    }
    else if (Command == NCMD_Chat)
        GConsole->SendChatMsgToServer(CHAT_GMPetition, (TCHAR*)Text, NULL);
    EditBox->ClearText();
    return 1;
}
NW_HOOK_METHOD(0x100dfd60, NCGMPetitionWnd, OnEdit)

// 0x100dfef0 (vslot 75) - the builder command "startpet"
INT NCGMPetitionWnd::OnStart()
{
    L2ParamStack Params(10);
    Params.PushBack((void*)TEXT("startpet"));
    ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand (slot 194 of this console)
    return 1;
}
NW_HOOK_METHOD(0x100dfef0, NCGMPetitionWnd, OnStart)

// 0x100dff70 (vslot 76) - the builder command "endpet"
INT NCGMPetitionWnd::OnEnd()
{
    L2ParamStack Params(10);
    Params.PushBack((void*)TEXT("endpet"));
    ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand (slot 194 of this console)
    return 1;
}
NW_HOOK_METHOD(0x100dff70, NCGMPetitionWnd, OnEnd)

// the bodies of NCPetitionWnd's AddMessage and Clear (one stock body each, for both windows: the layout is the same); the
// hook build jumps to them, the standalone has its own copies
#if NW_HOOK_MODE
NW_NAKED void NCGMPetitionWnd::AddMessage(TCHAR* Text, DWORD Color, INT Type, INT Param) NW_STOCK(0x100dfe90)

NW_NAKED void NCGMPetitionWnd::Clear() NW_STOCK(0x100dfed0)
#else
void NCGMPetitionWnd::AddMessage(TCHAR* Text, DWORD Color, INT Type, INT Param)
{
    AddPetitionMessage(ListBox, Text, Color, Type);
}

void NCGMPetitionWnd::Clear()
{
    EditBox->ClearText();
    ListBox->Clear();
}
#endif

NW_CHECK_VTABLE(NCGMPetitionWnd, 0x101ad938, 77)
