/*=============================================================================
    TaskBox.cpp - the list of open conversations (C3 only; C3 addresses). Stock: HitTest 0x10119ea0, OnPaint
    0x1011ada0 .. 0x1011afdf, OnLButtonUp 0x1011afe0, UpdateString 0x1011b060 .. 0x1011b0ff, the constructors /
    destructor 0x1011e7a0 .. 0x1011e8bf, WindowProc 0x10124390 and OnCreate 0x10124720 (both L2TaskBox's too).
=============================================================================*/
#include "nwindow/TaskBox.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"
#include "nwindow/Task.h"

// 0x10143330 - the list box's default constructor, the three textures empty, 200 lines kept and 16 shown (C4: the list box has no
// field at +0x20C any more, so the store of 15 that the C3 constructor made is gone)
TaskBox::TaskBox()
{
    NormalTex = NULL;
    SelectedTex = NULL;
    AlertTex = NULL;
    MaxLines = 200;
    VisibleLines = 0x10;
}
NW_HOOK_CTOR(0x10143330, TaskBox)

// 0x101433f0 (ret 0xC) - the same with the numbers given (TaskWnd makes its list with 300, 6, 15; the third is not stored in C4)
TaskBox::TaskBox(INT InMaxLines, INT InVisibleLines, INT /*InUnknown20C: not stored*/)
{
    MaxLines = InMaxLines;
    NormalTex = NULL;
    SelectedTex = NULL;
    AlertTex = NULL;
    VisibleLines = InVisibleLines;
}
NW_HOOK_CTOR_ARGS(0x101433f0, TaskBox, (INT A, INT B, INT C), (A, B, C))

// 0x101433a0
NW_HOOK_DTOR(0x101433a0, TaskBox)

// 0x10148eb0 (vslot 25) - the list box's, then a press (0x201) goes to the click handler
INT TaskBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCListBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDOWN)
        return OnLButtonUp(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10148eb0, TaskBox, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1013f6e0 (vslot 37) - the three button textures; the text at (4, 4)
INT TaskBox::OnCreate()
{
    NCListBox::OnCreate();
    NormalTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton"));
    SelectedTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton_down"));
    AlertTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton_light"));
    TextX = 4;
    TextY = 4;
    return 1;
}
NW_HOOK_METHOD(0x1013f6e0, TaskBox, OnCreate)

// 0x1013e740 (vslot 69, ret 8) - the line under the point, the lines 21 pixels high from the top of the box; the selection
// is left alone when none is hit, -1 outside the box
INT TaskBox::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0)
        return -1;
    INT W = (INT)Width;
    if (RX > W)
        return -1;
    if (RY < 0)
        return -1;
    if (RY > (INT)Height)
        return -1;
    INT Top = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i < NumLines; i++, Top += 21)
    {
        if (RX < W && RY >= Top && RY < Top + 21)
        {
            Selected = i;
            return Selected;
        }
    }
    return Selected;
}
NW_HOOK_METHOD(0x1013e740, TaskBox, HitTest)

// 0x1013f740 (vslot 56, guard: TaskBox::OnPaint) - a button (140 x 21) per visible line: the pressed one when selected,
// the lit one while its Task has an unread message and blinks on, the plain one otherwise; the line's icon (15 x 15) and
// the name cut to 116 pixels
INT TaskBox::OnPaint(NCGDevice* Device)
{
    guard(TaskBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        Task* Entry = (Task*)Item.Param;
        UTexture* Tex;
        if (i == Selected)
            Tex = SelectedTex;
        else if (Entry == NULL || Entry->bAlert == 0)
            Tex = NormalTex;
        else if (Entry->bBlink == 0)
            Tex = NormalTex;
        else
            Tex = AlertTex;
        Device->DrawTexture(0, Line * 0x15, 0x8C, 0x15, 0, 0, 0x8C, 0x15, Tex, 0xFF, 1);
        if (Item.Unknown8)
            Device->DrawTexture(TextX, TextY + Line * 0x15, 0xF, 0xF, 0, 0, 0xF, 0xF, (UTexture*)Item.Unknown8, 0xFF, 1);
        TCHAR Text[256];
        NFitText(Item.Text->GetAt(0), Text, 0x74, 0xC);
        Device->DrawText(TextX + 0x12, TextY + Line * 0x15, Item.Text->Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013f740, TaskBox, OnPaint)

// 0x1013f980 (vslot 69) - the line under the point, when it is the selected one: its Task opens the conversation (the
// alert is over, the window shown and focused); the selection is cleared whatever happened
INT TaskBox::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT Hit = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected == Hit && Selected >= 0 && Selected < Items.Num())
    {
        Task* Entry = (Task*)Items(Selected).Param;
        if (Entry && Entry->ChatWindow)
        {
            NCWnd* Chat = Entry->ChatWindow;
            Entry->bAlert = 0;
            Chat->ShowWindow();
            Chat->SetFocus();
        }
    }
    Selected = -1;
    return 1;
}
NW_HOOK_METHOD(0x1013f980, TaskBox, OnLButtonUp)

// 0x1013fa00 (ret 8, guard: TaskBox::UpdateString; the stock body goes on at 0x1011b090) - the line of Task gets this
// text instead of its own
void TaskBox::UpdateString(const TCHAR* Text, NCWnd* TaskWindow)
{
    guard(TaskBox::UpdateString);
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (Items(i).Param == (INT)TaskWindow)
        {
            Items(i).Text->Empty();
            Items(i).Text->Insert(Text, 0);
            break;
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1013fa00, TaskBox, UpdateString)

NW_CHECK_VTABLE(TaskBox, 0x101d9b68, 82)
