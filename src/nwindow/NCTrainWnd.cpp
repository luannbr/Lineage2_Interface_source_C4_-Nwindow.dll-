/*=============================================================================
    NCTrainWnd.cpp - the skill trainer window and its two pages. Stock: 0x100baae0 ..
    0x100bb4bf, 0x100bb4f0 .. 0x100bb53f, 0x100bb580 .. 0x100bc50a, 0x100bc540 .. 0x100bc54a,
    0x100bc7a0 .. 0x100bca96, 0x100bcc50 .. 0x100bcd19, 0x10089310 .. 0x1008937e and
    NTrainSkillInfo's 0x10047260 .. 0x100472de, 0x1004ddf0 .. 0x1004de67.
=============================================================================*/
#include "nwindow/NCTrainWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCItemWnd.h"
#include "nwindow/NCTrainListBox.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NWndCreate.h"

#include <wchar.h>

// the player's SP at the bottom right of both pages (repeated in the stock OnPaints)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE void DrawPlayerSP(NCGDevice* Device)
{
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        TCHAR Text[1024];
        SIZE Size;
        appSprintf(Text, TEXT("%d"), Me->SP);
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        Device->DrawText(220 - Size.cx, 290, 0xFFFFFFFF, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    }
}
#endif

/*-----------------------------------------------------------------------------
    NTrainSkillInfo
-----------------------------------------------------------------------------*/

// 0x1006e920 - empty strings, no icon, no required items, SP / MP / range 0, skill and level -1 (the two fields at +0x4C4 and
// +0x4C8 are left as they were)
NTrainSkillInfo::NTrainSkillInfo()
{
    Name[0] = 0;
    IconName[0] = 0;
    Unknown20C[0] = 0;
    Icon = NULL;
    Items[0] = NULL;
    Items[1] = NULL;
    Items[2] = NULL;
    Items[3] = NULL;
    SkillID = 0;
    SP = 0;
    MpCost = 0;
    Range = 0;
    Unknown2BC = 0;
    Unknown2B8 = 0;
    bEnchanted = 0;
    SkillID = -1;
    Level = -1;
    Unknown2A8 = -1;
    Unknown4CC = -1;
}
NW_HOOK_CTOR(0x1006e920, NTrainSkillInfo)

// 0x10082320 - the required items go: the stock destroys each one's tool tip record (the whole of the entry's destructor,
// 0x10050030) and frees it
NTrainSkillInfo::~NTrainSkillInfo()
{
    for (INT i = 0; i < 4; i++)
    {
        if (Items[i])
        {
            Items[i]->~NItemEntry();   // the stock calls 0x10002E40, ~NToolTipInfo, which is all of ~NItemEntry (0x10050030)
            GMalloc->Free(Items[i]);
            Items[i] = NULL;
        }
    }
}
NW_HOOK_DTOR(0x10082320, NTrainSkillInfo)

/*-----------------------------------------------------------------------------
    NCTrainListWnd
-----------------------------------------------------------------------------*/

// 0x1011e410
NCTrainListWnd::NCTrainListWnd()
{
    BackTex = NULL;
    ListBox = NULL;
}
NW_HOOK_CTOR(0x1011e410, NCTrainListWnd)

// 0x1011e460
NW_HOOK_DTOR(0x1011e460, NCTrainListWnd)

// 0x1011b8b0 (vslot 25) - C2 has no IsEnabled check of its own
INT NCTrainListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_TRAIN_LISTBOX)
        return OnListCommand(lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1011b8b0, NCTrainListWnd, WindowProc)

// 0x1011e4b0 (vslot 37) - 7 rows seen, 100 at most (the list box's constructor 0x1011E3C0 takes the three numbers)
INT NCTrainListWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SkillTrainWnd.SkillTrain1"));
    ListBox = new NCTrainListBox(100, 7, 16);   // 0x1011E3C0
    ListBox->ID = NID_TRAIN_LISTBOX;
    ListBox->SetPosition(20, 27, 230, 274);
    CreateChild(ListBox, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1011e4b0, NCTrainListWnd, OnCreate)

// 0x101566e0 (vslot 38)
INT NCTrainListWnd::OnDestroy()
{
    DestroyChild(this, ListBox);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x101566e0, NCTrainListWnd, OnDestroy)

// the player's SP at the bottom of both pages (repeated in the stock OnPaints): the number right-aligned at 224 and
// "SP" (sysstring 92) at 130
static FORCEINLINE void DrawPlayerSP(NCGDevice* Device)
{
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        TCHAR Text[128];
        SIZE Size;
        appSprintf(Text, TEXT("%d"), Me->SP);
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        Device->DrawText(0xE0 - Size.cx, 0x146, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(0x82, 0x146, 0xFFDCDCDC, *GL2GameData.SysString[92], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
}

// 0x1011cd20 (vslot 62) - the background (256 x 381), "Skill list" (sysstring 364) and, unless the rows are enchanted routes,
// the SP
INT NCTrainListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTrainListWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 256, 381, 0, 0, 256, 381, BackTex, 0xFF, 1);
    if (*GL2GameData.SysString[364])
        Device->DrawText(11, 12, 0xFFDCDCDC, *GL2GameData.SysString[364], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (!bEnchantList)
        DrawPlayerSP(Device);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011cd20, NCTrainListWnd, OnPaint)

// 0x1011cf10 (vslot 74) - the selected row's skill and level to the server (an enchanted route's row: the request of its own,
// NConsoleWnd slot 436; else slot 434 with the row's field at +0x1498 as well); lParam (the row) is not used
INT NCTrainListWnd::OnListCommand(LPARAM lParam)
{
    NTrainEntry Entry;
    // the stock frame keeps the parameter stack of each branch in a slot of its own (the compiler would give both one)
    BYTE EnchantStack[sizeof(L2ParamStack)];
    BYTE NormalStack[sizeof(L2ParamStack)];
    if (ListBox->GetSelectedEntry(&Entry))
    {
        if (Entry.bEnchanted)
        {
            L2ParamStack* Params = new ((void*)EnchantStack) L2ParamStack(10);
            Params->PushBack((void*)(INT_PTR)Entry.SkillID);
            Params->PushBack((void*)(INT_PTR)Entry.Level);
            ::nw::VirtualCall<void>(GConsole, 436, Params);
            Params->~L2ParamStack();
        }
        else
        {
            L2ParamStack* Params = new ((void*)NormalStack) L2ParamStack(10);
            Params->PushBack((void*)(INT_PTR)Entry.SkillID);
            Params->PushBack((void*)(INT_PTR)Entry.Level);
            Params->PushBack((void*)(INT_PTR)Entry.UnknownA98);
            ::nw::VirtualCall<void>(GConsole, 434, Params);
            Params->~L2ParamStack();
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1011cf10, NCTrainListWnd, OnListCommand)

// 0x100b2b90 - the list box's Clear (vslot 70) as a tail jump; the stock body is shared with
// NCPetInventoryWnd::ClearItems (hooked there). Out of line: NCTrainWnd::Reset calls it
__declspec(noinline) void NCTrainListWnd::Clear()
{
    ListBox->Clear();
}

// 0x1011dfd0 (an anonymous function; name ours) - on to the list box, remembering whether the rows are enchanted routes
void NCTrainListWnd::AddEntry(const NTrainEntry* Entry)
{
    bEnchantList = (Entry->bEnchanted != 0);
    ListBox->AddEntry(Entry);
}
NW_HOOK_METHOD(0x1011dfd0, NCTrainListWnd, AddEntry)

NW_CHECK_VTABLE(NCTrainListWnd, 0x101cf520, 75)

/*-----------------------------------------------------------------------------
    NCTrainInfoWnd
-----------------------------------------------------------------------------*/

// 0x1011e590 - the background and the two buttons are left to OnCreate; Info is made by its own constructor
NCTrainInfoWnd::NCTrainInfoWnd()
{
    BackTex = NULL;
    LearnButton = NULL;
    ListButton = NULL;
}
NW_HOOK_CTOR(0x1011e590, NCTrainInfoWnd)

// 0x1011e620 - the vtable, ConditionalDestroy, Info's destructor, NCWnd's
NW_HOOK_DTOR(0x1011e620, NCTrainInfoWnd)


// 0x1011b920 (vslot 25) - C2 has no IsEnabled check of its own
INT NCTrainInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_TRAIN_LEARN)
            return OnLearn() != 0;
        if (wParam == NID_TRAIN_LIST)
            return OnList() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1011b920, NCTrainInfoWnd, WindowProc)

// a button of the page (repeated inline in the stock OnCreate)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE NCButton* CreatePageButton(NCWnd* Parent, INT ID, INT X, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI.control.DefaultButton"),
                                    TEXT("L2UI.control.DefaultButton_click"), 0);
    Button->ID = ID;
    Button->SetPosition(X, 313, 67, 19);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}
#endif

// 0x1011e000 (vslot 37) - the two backgrounds (the enchant view's is new in C4), the four rows of required items (item entries
// made empty), the Learn (id 2) and List (id 3) buttons under them and the two outlines of the skill's icon
INT NCTrainInfoWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SkillTrainWnd.SkillTrain2"));
    EnchantBackTex = FindTexture(TEXT("L2UI_CH3.SkillTrainWnd.skillenchant_back"));
    for (INT i = 0; i < 4; i++)
    {
        NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
        if (Item)
            Item = new ((void*)Item) NItemEntry();
        Info.Items[i] = Item;
    }
    LearnButton = NewButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    LearnButton->ID = NID_TRAIN_LEARN;
    LearnButton->SetPosition(0x33, 0x160, 0x4C, 0x17);
    CreateChild(LearnButton, NWS_VISIBLE);
    LearnButton->SetTextID(0x170);
    ListButton = NewButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    ListButton->ID = NID_TRAIN_LIST;
    ListButton->SetPosition(0x83, 0x160, 0x4C, 0x17);
    CreateChild(ListButton, NWS_VISIBLE);
    ListButton->SetTextID(0x171);
    OutLineDownTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine_down"));
    OutLineTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine"));
    return 1;
}
NW_HOOK_METHOD(0x1011e000, NCTrainInfoWnd, OnCreate)

// 0x1011d040 (vslot 38) - NCWnd's comes first here; the item entries stay (Info's destructor)
INT NCTrainInfoWnd::OnDestroy()
{
    NCWnd::OnDestroy();
    DestroyChild(this, LearnButton);
    DestroyChild(this, ListButton);
    return 1;
}
NW_HOOK_METHOD(0x1011d040, NCTrainInfoWnd, OnDestroy)

// "label : " (the label a sysstring, empty when missing) in grey, the colon in white; returns
// the X of the value, past "label : " (repeated inline for each such line of the stock code)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE INT DrawLabel(NCGDevice* Device, INT X, INT Y, TCHAR* Text, INT LabelID)
{
    SIZE Size;
    if (*NCObject::SysString(LabelID))
        appSprintf(Text, TEXT("%s"), *NCObject::SysString(LabelID));
    else
        Text[0] = 0;
    Device->DrawText(X, Y, 0xFFA3A3A3, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT(" : "));
    Device->DrawText(X + Size.cx, Y, 0xFFFFFFFF, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (*NCObject::SysString(LabelID))
        appSprintf(Text, TEXT("%s : "), *NCObject::SysString(LabelID));
    else
        Text[0] = 0;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    return X + Size.cx;
}
#endif

// "label" in grey, then " : " in light grey, the value after "label : " (the label is a sysstring, empty when it
// is missing; a line of the stock paint of the enchant view, in a character buffer: the same spelled out for the
// range and the SP); returns the X where the value goes
static FORCEINLINE INT DrawLabeledLine(NCGDevice* Device, TCHAR* Text, INT Y, INT LabelID)
{
    SIZE Size;
    if (*GL2GameData.SysString[LabelID])
        appSprintf(Text, TEXT("%s"), *GL2GameData.SysString[LabelID]);
    else
        Text[0] = 0;
    Device->DrawText(0xD, Y, 0xFFA3A3A3, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT(" : "));
    Device->DrawText(Size.cx + 0xD, Y, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (*GL2GameData.SysString[LabelID])
        appSprintf(Text, TEXT("%s : "), *GL2GameData.SysString[LabelID]);
    else
        Text[0] = 0;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    return Size.cx + 0xD;
}

// the same line of the normal view, in a string (FString::Printf and an assignment each time): the label, " : " and "label : "
// measured; returns the X where the value goes
static FORCEINLINE INT DrawLabeledLineF(NCGDevice* Device, FString& Text, INT Y, INT LabelID)
{
    SIZE Size;
    Text = TEXT("");
    if (*GL2GameData.SysString[LabelID])
        Text = FString::Printf(TEXT("%s"), *GL2GameData.SysString[LabelID]);
    Device->DrawText(0xD, Y, 0xFFA3A3A3, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0);
    Text = FString::Printf(TEXT(" : "));
    Device->DrawText(Size.cx + 0xD, Y, 0xFFDCDCDC, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Text = TEXT("");
    if (*GL2GameData.SysString[LabelID])
        Text = FString::Printf(TEXT("%s : "), *GL2GameData.SysString[LabelID]);
    NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0);
    return Size.cx + 0xD;
}

// 0x1011d0b0 (vslot 62) - the background (256 x 381), "Skill explanation" (366) and "Learning condition" (367), the skill's
// icon in its two outlines, its name, "Lv" and the level (and the text after it), then MP cost and range (when it has one),
// the description (wrapped at 230), "SP" and the required SP, the required items (an icon and "name X count" in 140 pixels,
// 34 apart) and the player's SP. An enchanted route has the page of its own (PaintEnchant)
INT NCTrainInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTrainInfoWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (Info.bEnchanted)
        PaintEnchant(Device);
    else
    {
        SIZE Size;
        if (BackTex)
            Device->DrawTexture(0, 0, 256, 381, 0, 0, 256, 381, BackTex, 0xFF, 1);
        if (*GL2GameData.SysString[366])
            Device->DrawText(11, 12, 0xFFDCDCDC, *GL2GameData.SysString[366], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (*GL2GameData.SysString[367])
            Device->DrawText(11, 200, 0xFFDCDCDC, *GL2GameData.SysString[367], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (Info.Icon)
        {
            Device->DrawTexture(13, 29, 34, 34, 0, 0, 34, 34, OutLineDownTex, 0xFF, 1);
            Device->DrawTexture(13, 29, 35, 35, 0, 0, 35, 35, OutLineTex, 0xFF, 1);
            Device->DrawTexture(14, 30, 32, 32, 0, 0, 32, 32, Info.Icon, 0xFF, 1);
        }
        const TCHAR* Name = Info.Name;
        if (Name && *Name)
            Device->DrawText(0x33, 0x20, 0xFFDCDCDC, Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        FString Text(TEXT(""));
        // "Lv" (sysstring 88), the level and the text after it
        if (*GL2GameData.SysString[88])
            Text = FString::Printf(TEXT("%s"), *GL2GameData.SysString[88]);
        Device->DrawText(0x33, 0x30, 0xFFA3A3A3, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0);
        INT LevelX = Size.cx + 0x35;
        Text = FString::Printf(TEXT("%d"), Info.Level);
        Device->DrawText(LevelX, 0x30, 0xFFB09B79, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0);
        const TCHAR* After = Info.Unknown20C;
        if (After && *After)
            Device->DrawText(LevelX + Size.cx + 0xA, 0x30, 0xFFB09B79, After, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        // MP cost
        INT ValueX = DrawLabeledLineF(Device, Text, 0x44, 320);
        Text = FString::Printf(TEXT("%d"), Info.MpCost);
        Device->DrawText(ValueX, 0x44, 0xFFB09B79, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        // range (a negative one is not shown)
        if (Info.Range >= 0)
        {
            ValueX = DrawLabeledLineF(Device, Text, 0x50, 321);
            Text = FString::Printf(TEXT("%d"), Info.Range);
            Device->DrawText(ValueX, 0x50, 0xFFB09B79, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
        // the description, wrapped at 230 pixels
        if (*Info.Desc)
            Device->DrawText(0xD, 0x66, 0xFFB2BECF, *Info.Desc, 0xE6, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        // SP needed
        ValueX = DrawLabeledLineF(Device, Text, 0xDC, 365);
        Text = FString::Printf(TEXT("%d"), Info.SP);
        Device->DrawText(ValueX, 0xDC, 0xFFB09B79, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        // the items needed: a row each that is shown
        for (INT i = 0; i < 4; i++)
        {
            NItemEntry* Item = Info.Items[i];
            if (Item->GetUnknownC34() > 0)
            {
                if (Item->GetIcon())
                    Device->DrawTexture(0xD, i * 0x22 + 0xF2, 0x20, 0x20, 0, 0, 0x20, 0x20, Item->GetIcon(), 0xFF, 1);
                Text = TEXT("");
                if (Item->GetName())
                    Text = FString::Printf(TEXT("%s X %d"), Item->GetName(), AsC4(Item)->Count);
                Device->DrawText(0x31, i * 0x22 + 0xF2, 0xFFDCDCDC, *Text, 0x8C, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            }
        }
        DrawPlayerSP(Device);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011d0b0, NCTrainInfoWnd, OnPaint)

// exp at the start of each level (index = level), a table of the stock image (the trainer's copy)
#if NW_HOOK_MODE
#define GTrainExpTable (reinterpret_cast<const INT*>(::nw::Addr(0x101CF0F4)))
#else
extern const INT GTrainExpTable[];
#endif

// 0x1011b990 (an anonymous function, name ours; ret 4) - the page of an enchanted route: its background, "Skill explanation" and
// "Learning condition" (at 155), the icon, the name, the route's name (FL2MagicSkillData +0x50), a percentage, the route's
// description (wrapped at 230), the labelled values at 175 and 191, the required items (four rows, 34 apart from 213) and the
// player's exp progress and SP
void NCTrainInfoWnd::PaintEnchant(NCGDevice* Device)
{
    TCHAR Text[256];
    TCHAR Line[128];
    SIZE Size;
    if (EnchantBackTex)
        Device->DrawTexture(0, 0, 256, 381, 0, 0, 256, 381, EnchantBackTex, 0xFF, 1);
    if (*GL2GameData.SysString[366])
        Device->DrawText(11, 12, 0xFFDCDCDC, *GL2GameData.SysString[366], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (*GL2GameData.SysString[367])
        Device->DrawText(11, 0x9B, 0xFFDCDCDC, *GL2GameData.SysString[367], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (Info.Icon)
    {
        Device->DrawTexture(13, 29, 34, 34, 0, 0, 34, 34, OutLineDownTex, 0xFF, 1);
        Device->DrawTexture(13, 29, 35, 35, 0, 0, 35, 35, OutLineTex, 0xFF, 1);
        Device->DrawTexture(14, 30, 32, 32, 0, 0, 32, 32, Info.Icon, 0xFF, 1);
    }
    const TCHAR* Name = Info.Name;
    if (Name && *Name)
        Device->DrawText(0x33, 0x20, 0xFFDCDCDC, Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    FL2MagicSkillData* Skill = GL2GameData.GetMSData(Info.SkillID, Info.Level);
    if (Skill)
        Device->DrawText(0x33, 0x30, 0xFFB09B79, *Skill->EnchantName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    // the percentage (sysstring 642 and the field at +0x4C4)
    if (*GL2GameData.SysString[642])
        appSprintf(Text, TEXT("%s"), *GL2GameData.SysString[642]);
    else
        Text[0] = 0;
    Device->DrawText(0xD, 0x44, 0xFFA3A3A3, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT("%d%%"), Info.Unknown4C4);
    Device->DrawText(Size.cx + 0x12, 0x44, 0xFFB09B79, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (Skill && *Skill->EnchantDescription)
        Device->DrawText(0xD, 0x5B, 0xFFB2BECF, *Skill->EnchantDescription, 0xE6, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    // the two labelled values (sysstring 1219 with the field at +0x4B8, and the SP)
    INT ValueX = DrawLabeledLine(Device, Text, 0xAF, 1219);
    appSprintf(Text, TEXT("%d"), Info.Unknown2B8);
    Device->DrawText(ValueX, 0xAF, 0xFFB09B79, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    ValueX = DrawLabeledLine(Device, Text, 0xBF, 365);
    appSprintf(Text, TEXT("%d"), Info.SP);
    Device->DrawText(ValueX, 0xBF, 0xFFB09B79, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    // the items needed: a row each that is shown
    for (INT i = 0; i < 4; i++)
    {
        NItemEntry* Item = Info.Items[i];
        INT Y = 213 + i * 0x22;
        if (Item->GetUnknownC34() > 0)
        {
            if (Item->GetIcon())
                Device->DrawTexture(0xD, Y, 0x20, 0x20, 0, 0, 0x20, 0x20, Item->GetIcon(), 0xFF, 1);
            if (Item->GetName())
                appSprintf(Text, TEXT("%s X %d"), Item->GetName(), AsC4(Item)->Count);
            else
                Text[0] = 0;
            Device->DrawText(0x31, Y, 0xFFDCDCDC, Text, 0x8C, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
    }
    // the player's exp since the start of the level (levels 1 .. 78) and SP
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        if (Me->Level > 0 && Me->Level <= 78)
        {
            appSprintf(Line, TEXT("%d"), Me->Exp - GTrainExpTable[Me->Level]);
            NCGDevice::GetTextExtent(&Size, Line, 0, 0, 0, 0, 0);
            Device->DrawText(0xE0 - Size.cx, 0x132, 0xFFDCDCDC, Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
        appSprintf(Line, TEXT("%d"), Me->SP);
        NCGDevice::GetTextExtent(&Size, Line, 0, 0, 0, 0, 0);
        Device->DrawText(0xE0 - Size.cx, 0x146, 0xFFDCDCDC, Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
}
NW_HOOK_METHOD(0x1011b990, NCTrainInfoWnd, PaintEnchant)

// 0x1011c3c0 (vslot 74) - "Learn": the skill and its level to the server (an enchanted route: NConsoleWnd slot 437; else slot
// 435 with the skill's field at +0x4C8 as well)
INT NCTrainInfoWnd::OnLearn()
{
    if (Info.bEnchanted)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Info.SkillID);
        Params.PushBack((void*)(INT_PTR)Info.Level);
        ::nw::VirtualCall<void>(GConsole, 437, &Params);
    }
    else
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Info.SkillID);
        Params.PushBack((void*)(INT_PTR)Info.Level);
        Params.PushBack((void*)(INT_PTR)Info.Unknown4C8);
        ::nw::VirtualCall<void>(GConsole, 435, &Params);
    }
    return 1;
}
NW_HOOK_METHOD(0x1011c3c0, NCTrainInfoWnd, OnLearn)

// 0x1011c4b0 (vslot 75) - "List": back to the list page (the buttons are hidden after the
// page)
INT NCTrainInfoWnd::OnList()
{
    HideWindow();
    GDesktop->TrainWnd->ListWnd->ShowWindow();
    LearnButton->HideWindow();
    ListButton->HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x1011c4b0, NCTrainInfoWnd, OnList)

// 0x1011c0f0 - empty strings and a NULL icon are not copied, the description always is; the icon is looked up again by name
// (NWindow.BlackTexture when missing). Info.Items stays.
void NCTrainInfoWnd::SetInfo(NTrainSkillInfo* Src)
{
    if (Src->Name && Src->Name[0])
        wcscpy(Info.Name, Src->Name);
    const TCHAR* IconName = Src->IconName;
    if (IconName && IconName[0])
        wcscpy(Info.IconName, IconName);
    Info.Desc = Src->Desc;
    if (Src->Unknown20C && Src->Unknown20C[0])
        wcscpy(Info.Unknown20C, Src->Unknown20C);
    if (Src->Icon)
        Info.Icon = Src->Icon;
    Info.SkillID = Src->SkillID;
    Info.Level = Src->Level;
    Info.Unknown2A8 = Src->Unknown2A8;
    Info.SP = Src->SP;
    Info.MpCost = Src->MpCost;
    Info.Range = Src->Range;
    Info.Unknown2BC = Src->Unknown2BC;
    Info.Unknown4CC = Src->Unknown4CC;
    Info.bEnchanted = Src->bEnchanted;
    Info.Unknown2B8 = Src->Unknown2B8;
    Info.Unknown4C4 = Src->Unknown4C4;
    Info.Unknown4C8 = Src->Unknown4C8;
    if (IconName && IconName[0])
        Info.Icon = FindTexture(IconName);
    if (!Info.Icon)
        Info.Icon = FindTexture(TEXT("NWindow.BlackTexture"));
}
NW_HOOK_METHOD(0x1011c0f0, NCTrainInfoWnd, SetInfo)

// 0x1011c340 - before the server's answer; the description, Unknown2A8, MP cost and range are left, the item rows hidden
// (NItemEntry::UnknownC34 -1)
void NCTrainInfoWnd::Reset()
{
    Info.Name[0] = 0;
    Info.IconName[0] = 0;
    Info.Unknown20C[0] = 0;
    Info.Icon = NULL;
    Info.SP = 0;
    Info.SkillID = -1;
    Info.Level = -1;
    Info.Unknown2BC = 0;
    Info.Unknown4CC = -1;
    Info.Unknown4C8 = 0;
    for (INT i = 0; i < 4; i++)
        Info.Items[i]->SetUnknownC34(-1);
}
NW_HOOK_METHOD(0x1011c340, NCTrainInfoWnd, Reset)

// 0x1011c250 (ret 4; NConsoleWndAnon.cpp hooks the address on a stand-in struct, NCTrainInfoWndExt2, with raw offsets: this is the class's
// own copy of that body, which the standalone calls) - a required item of the skill: the first of the four entries that is free (-1 at
// +0x1B40) takes the item (the icon by its texture's name, the black texture when missing), a copy made by the entry's constructor from the
// record; then it counts as taken. The stock leaves the temporary entry allocated after the copy, and so does this
void NCTrainInfoWnd::AddRequiredItem(NItemInfo* Item)
{
    INT i;
    for (i = 0; i < 4; i++)
    {
        if (Info.Items[i]->GetUnknownC34() < 0)
            break;
    }
    if (i >= 4)
        return;
    AsC4(Item)->Icon[0] = FindTexture(AsC4(Item)->IconName[0], 1);
    if (!AsC4(Item)->Icon[0])
        AsC4(Item)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    NItemEntry* Entry = (NItemEntry*)GMalloc->Malloc(sizeof(NItemEntry), TEXT("new"));
    if (Entry)
        Entry = new ((void*)Entry) NItemEntry(Item);
    Info.Items[i]->Copy(Entry);
    Info.Items[i]->SetUnknownC34(1);
}

NW_CHECK_VTABLE(NCTrainInfoWnd, 0x101cf698, 76)

/*-----------------------------------------------------------------------------
    NCTrainWnd
-----------------------------------------------------------------------------*/

// 0x1011e680
NCTrainWnd::NCTrainWnd()
{
    ListWnd = NULL;
    InfoWnd = NULL;
}
NW_HOOK_CTOR(0x1011e680, NCTrainWnd)

// 0x1011e6d0
NW_HOOK_DTOR(0x1011e6d0, NCTrainWnd)

// 0x10148f10 (vslot 25) - the stock slot is NControl's body: NCWnd's WindowProc as a flag, with no enabled check (C4's; the C3 body that was
// here, `Msg != WM_PAINT && !IsEnabled()` first, would have a disabled window swallow what the stock one handles: with NCTrainWnd's
// constructor hooked this is the slot of our vtable)
INT NCTrainWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1011e720 (vslot 37) - the title bar (id 4: "Skill Trainer", sysstring 477) over the two pages, in the same place (256 x 381
// under the bar), both shown (the list's own OnCreate and the console decide which one is up)
INT NCTrainWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameID = NID_TRAIN_FRAME;
    FrameCtrl->ID = NID_TRAIN_FRAME;
    FrameCtrl->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    FrameCtrl->SetTitleID(477);   // the title by sysstring id (the anonymous 0x10011810, ret 4)
    ListWnd = NewWindowOf<NCTrainListWnd>(0x1011E410);
    ListWnd->SetPosition(0, 0x14, 0x100, 0x17D);
    CreateChild(ListWnd, NWS_VISIBLE);
    InfoWnd = NewWindowOf<NCTrainInfoWnd>(0x1011E590);
    InfoWnd->SetPosition(0, 0x14, 0x100, 0x17D);
    CreateChild(InfoWnd, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1011e720, NCTrainWnd, OnCreate)

// 0x1011dc90 (vslot 38; 111 bytes) - the list page and the info page go (each: taken off the window, deleted, cleared), then NCWnd's OnDestroy; 1.
// The stock body is shared with NCRecipeBuyListWnd (its list and close button sit at the same +0xE8 / +0xEC; NCRecipeBuyListWnd::OnDestroy is
// a stub that jumps to this address)
INT NCTrainWnd::OnDestroy()
{
    DestroyChild(this, ListWnd);
    DestroyChild(this, InfoWnd);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x1011dc90, NCTrainWnd, OnDestroy)

// stock body shared with NCEditBox's (0x100a5d70)
void NCTrainWnd::ShowWindow()
{
    NCWnd::ShowWindow();
}

// stock body shared with NCChatWnd's (0x1003dab0)
void NCTrainWnd::HideWindow()
{
    NCWnd::HideWindow();
}

// 0x1011dd00 (vslot 62) - nothing is drawn (both pages are children that paint themselves)
INT NCTrainWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTrainWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011dd00, NCTrainWnd, OnPaint)

// 0x1011c560 - a new list is coming
void NCTrainWnd::Reset()
{
    ListWnd->Clear();
    InfoWnd->Reset();
}
NW_HOOK_METHOD(0x1011c560, NCTrainWnd, Reset)

// 0x1011c500
void NCTrainWnd::ShowList()
{
    InfoWnd->HideWindow();
    ListWnd->ShowWindow();
}
NW_HOOK_METHOD(0x1011c500, NCTrainWnd, ShowList)

// 0x1011c520
void NCTrainWnd::ShowInfo()
{
    ListWnd->HideWindow();
    InfoWnd->ShowWindow();
}
NW_HOOK_METHOD(0x1011c520, NCTrainWnd, ShowInfo)

// 0x1011c540
void NCTrainWnd::SetSkillInfo(NTrainSkillInfo* Info)
{
    InfoWnd->SetInfo(Info);
}
NW_HOOK_METHOD(0x1011c540, NCTrainWnd, SetSkillInfo)

// 0x1011c550 (ret 4) - on to the info page (NConsoleWndAnon.cpp hooks the address on a stand-in struct, NCTrainWndExt2)
#if NW_HOOK_MODE
NW_NAKED void NCTrainWnd::AddRequiredItem(NItemInfo* Item) NW_STOCK(0x1011c550)
#else
void NCTrainWnd::AddRequiredItem(NItemInfo* Item)
{
    InfoWnd->AddRequiredItem(Item);
}
#endif

// 0x1011e260 - on to the list page
void NCTrainWnd::AddEntry(const NTrainEntry* Entry)
{
    ListWnd->AddEntry(Entry);
}
NW_HOOK_METHOD(0x1011e260, NCTrainWnd, AddEntry)

NW_CHECK_VTABLE(NCTrainWnd, 0x101cf7c8, 74)
