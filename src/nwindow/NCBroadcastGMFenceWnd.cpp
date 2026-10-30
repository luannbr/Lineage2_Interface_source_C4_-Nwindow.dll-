/*=============================================================================
    NCBroadcastGMFenceWnd.cpp - the fence dialog of the GM's broadcast tools (C3 only; C3 addresses). Stock: 0x1010BC20 ..
    0x1010C3CF (the handlers, WindowProc, OnDestroy, OnPaint), OnCreate 0x1010C3D0 .. 0x1010CA2F, the constructor
    0x1010CA70, the destructor 0x1010CB10. The scalar deleting destructor (0x1010CAE0) and InternalConstructor (0x1010CB60)
    stay stock. The stock guard strings of OnPaint and OnCreate say NCBroadcastGMWnd (copied from the other window).
=============================================================================*/
#include "nwindow/NCBroadcastGMFenceWnd.h"
#include "nwindow/NCBroadcastGMWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"

// NWM_COMMAND ids of the controls (names ours) and the title bar's id
enum
{
    NID_FENCE_OK = 0xFFFFE001,
    NID_FENCE_CANCEL = 0xFFFFE002,
    NID_FENCE_MYPOS = 0xFFFFE003,
    NID_FENCE_EDIT = 0xFFFFD001,
    NID_FENCE_FRAME = 0xEFFFF001,
};

// sysstring lines the window shows (GL2GameData + 0x65A8C + 12 * N)
enum
{
    STR_FenceOK = 140,
    STR_FenceCancel = 141,
    STR_FenceMyPos = 1064,
    STR_FenceLabelPos = 1065,
    STR_FenceLabelWidth = 1066,
    STR_FenceLabelHeight = 1067,
};

enum { MSG_FenceBadSize = 1414 };   // systemmsg line: the size must be 100 .. 4999

enum { FENCE_MinSize = 100, FENCE_MaxSize = 5000 };

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x101229b0 - NCDialogBox's constructor, then the nine fields cleared
NCBroadcastGMFenceWnd::NCBroadcastGMFenceWnd()
{
    BackTex = NULL;
    OKButton = NULL;
    CancelButton = NULL;
    MyPosButton = NULL;
    XEdit = NULL;
    YEdit = NULL;
    ZEdit = NULL;
    WidthEdit = NULL;
    HeightEdit = NULL;
}
NW_HOOK_CTOR(0x101229b0, NCBroadcastGMFenceWnd)

// 0x10122a50
NW_HOOK_DTOR(0x10122a50, NCBroadcastGMFenceWnd)

// 0x10121b10 (guard) - both sizes must be 100 .. 4999: the box closes and the five texts go to the GM window (it sends
// "eventmatch create 1 ..."); otherwise a message
INT NCBroadcastGMFenceWnd::OnOKBtnClick()
{
    guard(NCBroadcastGMFenceWnd::OnOKBtnClick);
    INT Size1 = appAtoi(*WidthEdit->Left);
    if (Size1 >= FENCE_MinSize && Size1 < FENCE_MaxSize)
    {
        INT Size2 = appAtoi(*HeightEdit->Left);
        if (Size2 >= FENCE_MinSize && Size2 < FENCE_MaxSize)
        {
            HideModal();
            static_cast<NCBroadcastGMWnd*>(GDesktop->BroadcastGMWnd)->NotifyFenceInfo(
                XEdit->Left, YEdit->Left, ZEdit->Left, WidthEdit->Left, HeightEdit->Left);
            return 1;
        }
    }
    GConsole->ShowWarningDialog((INT)(INT_PTR)*GL2GameData.SystemMsg[MSG_FenceBadSize].Message, 0);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10121b10, NCBroadcastGMFenceWnd, OnOKBtnClick)

// 0x10121cc0 (guard) - the modal box closes
INT NCBroadcastGMFenceWnd::OnCancelBtnClick()
{
    guard(NCBroadcastGMFenceWnd::OnCancelBtnClick);
    HideModal();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10121cc0, NCBroadcastGMFenceWnd, OnCancelBtnClick)

// 0x10121d40 (name ours; the stock guard string is OnCancelBtnClick) - the three position boxes take the player's place
// (the integer part of the pawn's X, Y and Z). The player is not checked
INT NCBroadcastGMFenceWnd::OnMyPosBtnClick()
{
    guard(NCBroadcastGMFenceWnd::OnCancelBtnClick);
    User* Me = GConsole->GetMyUser();
    XEdit->ClearText();
    YEdit->ClearText();
    ZEdit->ClearText();
    XEdit->InsertText(*FString::Printf(TEXT("%d"), (INT)Me->Pawn->Location.X), 1);
    YEdit->InsertText(*FString::Printf(TEXT("%d"), (INT)Me->Pawn->Location.Y), 1);
    ZEdit->InsertText(*FString::Printf(TEXT("%d"), (INT)Me->Pawn->Location.Z), 1);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10121d40, NCBroadcastGMFenceWnd, OnMyPosBtnClick)

// 0x10121ed0 (vslot 25) - NCWnd's, then the three buttons by id
INT NCBroadcastGMFenceWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_FENCE_OK)
            return OnOKBtnClick() != 0;
        if (wParam == NID_FENCE_CANCEL)
            return OnCancelBtnClick() != 0;
        if (wParam == NID_FENCE_MYPOS)
            return OnMyPosBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10121ed0, NCBroadcastGMFenceWnd, WindowProc)

// 0x10121f60 (vslot 38, guard) - the nine children go in order, then NCDialogBox's OnDestroy (C4: the C3 code did not
// call it)
INT NCBroadcastGMFenceWnd::OnDestroy()
{
    guard(NCBroadcastGMFenceWnd::OnDestroy);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, MyPosButton);
    DestroyChild(this, XEdit);
    DestroyChild(this, YEdit);
    DestroyChild(this, ZEdit);
    DestroyChild(this, WidthEdit);
    DestroyChild(this, HeightEdit);
    NCDialogBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10121f60, NCBroadcastGMFenceWnd, OnDestroy)

// 0x10122140 (vslot 62, guard "NCBroadcastGMWnd::OnPaint" in the stock) - the panel under the title bar (20 px down, the
// window's width, its height less 20) and the three labels
INT NCBroadcastGMFenceWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastGMWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, 0x100, 0x6C, BackTex, 0xFF, 1);
    Device->DrawText(0x19, 0x23, 0xFFDCDCDC, *GL2GameData.SysString[STR_FenceLabelPos], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(0x19, 0x46, 0xFFDCDCDC, *GL2GameData.SysString[STR_FenceLabelWidth], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x96, 0x46, 0xFFDCDCDC, *GL2GameData.SysString[STR_FenceLabelHeight], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10122140, NCBroadcastGMFenceWnd, OnPaint)

// a text box of the window: 80 x 16 at (X, Y)
static FORCEINLINE NCEditBox* CreateFenceEdit(NCWnd* Parent, INT X, INT Y)
{
    NCEditBox* Edit = new NCEditBox(0, 0, 1, 0x108, 0);
    Edit->ID = NID_FENCE_EDIT;
    Edit->SetPosition(X, Y, 80, 16);
    Parent->CreateChild(Edit, NWS_VISIBLE);
    return Edit;
}

// 0x101222d0 (vslot 37, guard "NCBroadcastGMWnd::OnCreate" in the stock) - the panel texture, the title bar (20 high), the
// OK / Cancel / "my position" buttons and the five text boxes (X, Y, Z, then the two sizes); NCWnd's OnCreate only.
// C4: the button texts are set by their sysstring number (SetTextID 140 / 141 / 1064)
INT NCBroadcastGMFenceWnd::OnCreate()
{
    guard(NCBroadcastGMWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Minimap.MapBack"));
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_FENCE_FRAME;
    FrameCtrl->ID = NID_FENCE_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_FENCE_OK;
    OKButton->SetPosition(0x96, 0x64, 0x4C, 0x17);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_FenceOK);
    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_FENCE_CANCEL;
    CancelButton->SetPosition(0xFA, 0x64, 0x4C, 0x17);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_FenceCancel);
    MyPosButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    MyPosButton->ID = NID_FENCE_MYPOS;
    MyPosButton->SetPosition(0x19F, 0x1E, 0x4C, 0x17);
    CreateChild(MyPosButton, NWS_VISIBLE);
    MyPosButton->SetTextID(STR_FenceMyPos);

    XEdit = CreateFenceEdit(this, 0x73, 0x20);
    YEdit = CreateFenceEdit(this, 0xD7, 0x20);
    ZEdit = CreateFenceEdit(this, 0x13B, 0x20);
    WidthEdit = CreateFenceEdit(this, 0x37, 0x44);
    HeightEdit = CreateFenceEdit(this, 0xB4, 0x44);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101222d0, NCBroadcastGMFenceWnd, OnCreate)

NW_CHECK_VTABLE(NCBroadcastGMFenceWnd, 0x101d14a8, 76)
