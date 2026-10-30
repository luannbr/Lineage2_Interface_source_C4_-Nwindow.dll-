/*=============================================================================
    NCPawnCreateWnd.cpp - the character creation screen: NCPawnCreateWnd, its choices panel
    (NCPawnSetupWnd), the class description (NCPawnInfoWnd), the selectors (NCSelectCtrl) and
    (C3) the window of its two buttons (NCPawnCreateFunctionWnd). Stock (C3): 0x100bdc80 ..
    0x100c21d0, the five classes interleaved; three bodies are shared with other classes
    (0x10004e90, 0x10077fd0, 0x10083930).
=============================================================================*/
#include "nwindow/NCPawnCreateWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"
#include "nwindow/NConsoleWnd.h"

#include <string.h>
#include <wchar.h>

// NWM_COMMAND ids (names ours)
enum
{
    NID_SELECT_PREV = 0xF0EF01,        // NCSelectCtrl's side buttons
    NID_SELECT_NEXT = 0xF0EF02,
    NID_SETUP_NAME = 0x1503,           // NCPawnSetupWnd's children
    NID_SETUP_RACE = 0xF0A3B2,
    NID_SETUP_CLASSTYPE = 0xF0A4B3,
    NID_SETUP_SEX = 0xF0A5B4,
    NID_SETUP_HAIRCOLOR = 0xF0A6B5,
    NID_SETUP_HAIRSTYLE = 0xF0A7B6,
    NID_SETUP_FACE = 0xF0A8B7,
    NID_CREATE_CREATE = 0x1009,        // NCPawnCreateFunctionWnd's buttons (C3: a window of their own)
    NID_CREATE_PREVIOUS = 0x100A,
    NID_CREATE_LEFT = 0x100D,         // NCPawnCreateWnd's
    NID_CREATE_RIGHT = 0x100E,
    NID_CREATE_ZOOM = 0x1010,          // zoom in and zoom out alike
};

// systemmsg lines (names ours)
enum
{
    MSG_BadNameLength = 80,            // "Please insert 1~16 English characters."
    MSG_SelectRace = 81,               // "Please select your race."
    MSG_SelectClassType = 82,          // "Please select your occupation."
    MSG_SelectSex = 83,                // "Please select your gender."
    MSG_SelectHairStyle = 91,          // "Please select your hairstyle."
    MSG_IncorrectName = 204,           // "Incorrect name. Please try again. "
    MSG_SelectHairColor = 366,         // "Select hair color."
};

// the race index (names ours)
enum { RACE_Orc = 3, RACE_Dwarf = 4 };

// the choices in panel order (names ours)
enum { CHOICE_Race, CHOICE_ClassType, CHOICE_Sex, CHOICE_HairStyle, CHOICE_HairColor, CHOICE_Face };

// the parts of the lobby map's event names (stock tables, names ours)
#if NW_HOOK_MODE
#define GRaceEventNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10262098)))      // "HUMAN" .. "DWARF"
#define GClassTypeEventNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x102620AC))) // "KNIGHT", "WIZARD"
#define GSexEventNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x102620B4)))       // "MAN", "WOMAN"
#else
extern const TCHAR* const GRaceEventNames[];
extern const TCHAR* const GClassTypeEventNames[];
extern const TCHAR* const GSexEventNames[];
#endif

// the console's creation window, and its 18 default characters of the creation stage (User
// records, Pawn the actor; not placed in NConsoleWnd's declaration yet, name ours)
static NCPawnCreateWnd* ConsolePawnCreateWnd() { return GConsole->PawnCreateWnd; }
static User* ConsoleDefaultCharacters() { return GConsole->DefaultCharacterInfo; }

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

// An object made as the stock code makes it: StaticAllocateObject of its class, then the constructor at Ctor of
// this client (the headers' constructors are the C2 ones or are not safe to run: the base class' is a stub)
// The standalone has no stock code: the constructor that runs is T's own (hooked at Ctor with the same arguments, so the
// hook build runs the same one through the stock address).
template <class T, class... A> static FORCEINLINE T* NewStock(DWORD Ctor, A... a)
{
#if NW_HOOK_MODE
    void* Mem = T::operator new(sizeof(T));
    return Mem ? ::nw::StockCallAt<T*>(Ctor, Mem, a...) : NULL;
#else
    (void)Ctor;
    return new T(a...);
#endif
}

// the text of a systemmsg line
static FORCEINLINE const TCHAR* SystemMessage(INT ID)
{
    return *GL2GameData.SystemMsg[ID].Message;
}

/*-----------------------------------------------------------------------------
    NCSelectCtrl.
-----------------------------------------------------------------------------*/

// 0x100cf3c0 - NControl's fields (inline in the stock constructor), nothing selected, the items emptied
NCSelectCtrl::NCSelectCtrl()
{
    Selected = -1;
    Items.Empty();
}
NW_HOOK_CTOR(0x100cf3c0, NCSelectCtrl)

// 0x100cf480
NW_HOOK_DTOR(0x100cf480, NCSelectCtrl)

// 0x100cc260 (vslot 25) - the side buttons step through the items
INT NCSelectCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_SELECT_PREV)
            return SelectPrev() != 0;
        if (wParam == NID_SELECT_NEXT)
            return SelectNext() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cc260, NCSelectCtrl, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100ce7b0 (vslot 37) - C3: the three pieces of the text box and the two side buttons ("PREV1" 33 pixels from the
// right edge, "NEXT1" 15) are in the control itself now; the buttons are NCPushButtons
INT NCSelectCtrl::OnCreate()
{
    NControl::OnCreate();
    TextBoxLeft = FindTexture(TEXT("L2ui_ch3.etc.textbox1"));
    TextBoxMiddle = FindTexture(TEXT("L2ui_ch3.etc.textbox2"));
    TextBoxRight = FindTexture(TEXT("L2ui_ch3.etc.textbox3"));

    LeftButton = NewStock<NCPushButton>(0x10003940, TEXT("L2ui_ch3.Button.PREV1"), TEXT("L2ui_ch3.Button.PREV1_down"));
    LeftButton->ID = NID_SELECT_PREV;
    LeftButton->SetPosition((INT)(Width - 15.0f - 15.0f - 3.0f), 1, 15, 15);
    CreateChild(LeftButton, NWS_VISIBLE);

    RightButton = NewStock<NCPushButton>(0x10003940, TEXT("L2ui_ch3.Button.NEXT1"), TEXT("L2ui_ch3.Button.NEXT1_down"));
    RightButton->ID = NID_SELECT_NEXT;
    RightButton->SetPosition((INT)(Width - 15.0f), 1, 15, 15);
    CreateChild(RightButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100ce7b0, NCSelectCtrl, OnCreate)

// 0x100ccb20 (vslot 38) - the two buttons go, the choices are emptied (inline here: nothing is freed, C4 keeps ids)
INT NCSelectCtrl::OnDestroy()
{
    guard(NCSelectCtrl::OnDestroy);
    DestroyChild(this, LeftButton);
    DestroyChild(this, RightButton);
    Items.Empty();
    Selected = -1;
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ccb20, NCSelectCtrl, OnDestroy)

// 0x100cc980 (vslot 62) - the box in three pieces (8 pixels each side, the middle stretched), then the choice made: the
// SysString of the id at Selected
INT NCSelectCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCSelectCtrl::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, 8, 17, 0, 0, 8, 17, TextBoxLeft);
    Device->DrawTexture(8, 0, (INT)(Width - 38.0f - 8.0f - 8.0f), 17, 0, 0, 8, 17, TextBoxMiddle);
    Device->DrawTexture((INT)(Width - 38.0f - 8.0f), 0, 8, 17, 0, 0, 8, 17, TextBoxRight);
    if (Selected >= 0 && Selected < Items.Num())
        Device->DrawText(3, 3, 0xFFDCDCDC, *GL2GameData.SysString[Items(Selected)], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cc980, NCSelectCtrl, OnPaint)

// 0x100cc2e0 (vslot 75) - the previous item, from the first to the last
INT NCSelectCtrl::SelectPrev()
{
    if (Items.Num() > 0 && IsEnabled())
    {
        if (Selected > 0)
            Selected--;
        else
            Selected = Items.Num() - 1;
        NotifyParent(Selected);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cc2e0, NCSelectCtrl, SelectPrev)

// 0x100cc330 (vslot 76) - the next item, from the last to the first
INT NCSelectCtrl::SelectNext()
{
    if (Items.Num() > 0 && IsEnabled())
    {
        if (Selected < Items.Num() - 1)
            Selected++;
        else
            Selected = 0;
        NotifyParent(Selected);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cc330, NCSelectCtrl, SelectNext)

// 0x100cc950 - no choices (C4: the ids are just dropped; C3 deleted the NString of each). The stock inlines this in OnRace,
// InitSelectors and the like; OnSex calls this instance
__declspec(noinline) void NCSelectCtrl::ClearItems()
{
    Items.Empty();
    Selected = -1;
}
NW_HOOK_METHOD(0x100cc950, NCSelectCtrl, ClearItems)

// 0x100ce990 - a choice by the SysString id (an id below 0 is not added; C3 refused 0 too). Not inlined: the stock callers
// call it.
__declspec(noinline) void NCSelectCtrl::AddItem(INT SysStringID)
{
    if (SysStringID >= 0)
        Items.AddItem(SysStringID);
}
NW_HOOK_METHOD(0x100ce990, NCSelectCtrl, AddItem)

// 0x100cf540 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100cf540, NCPawnInfoWnd)
// 0x100cf630 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100cf630, NCPawnSetupWnd)
// 0x100cc200 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100cc200, NCPawnCreateFunctionWnd)
// 0x100cfb80 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100cfb80, NCPawnCreateWnd)

// 0x100cbe30 - CastChecked<NCPawnCreateWnd>, the stock's out-of-line instance (the function window's buttons)
static NCPawnCreateWnd* CastCheckedNCPawnCreateWnd(UObject* Object)
{
    return CastChecked<NCPawnCreateWnd>(Object);
}
NW_HOOK_FUNC(0x100cbe30, CastCheckedNCPawnCreateWnd)

NW_CHECK_VTABLE(NCSelectCtrl, 0x101c0f18, 77)

/*-----------------------------------------------------------------------------
    NCPawnInfoWnd.
-----------------------------------------------------------------------------*/

// 0x100cf4e0 - the background is looked up in OnCreate; the text starts empty
NCPawnInfoWnd::NCPawnInfoWnd()
{
    BackTex = NULL;
    Text[0] = 0;
}
NW_HOOK_CTOR(0x100cf4e0, NCPawnInfoWnd)


// stock body shared with NControl's (0x10004e90)
INT NCPawnInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100ccc10 (vslot 37) - starts with classinfo 0, "Select a race and occupation."
INT NCPawnInfoWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.select2_back"));
    LineTex = FindTexture(TEXT("L2UI_CH3.Etc.textbackline"));
    SetText(*GL2GameData.ClassInfo[0]);
    return 1;
}
NW_HOOK_METHOD(0x100ccc10, NCPawnInfoWnd, OnCreate)

// 0x100cc390 (vslot 38) - NCWnd's OnDestroy and nothing else
INT NCPawnInfoWnd::OnDestroy()
{
    guard(NCPawnInfoWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cc390, NCPawnInfoWnd, OnDestroy)

// 0x100ccc70 (vslot 62) - the background, a line texture behind every second of 13 rows (15 pixels each, 10 pixels
// narrower than the window), the text in the corner wrapped 14 pixels short of the width
INT NCPawnInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPawnInfoWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    for (INT i = 0; i < 13; i++)
    {
        if (i % 2 == 1)
            Device->DrawTexture(5, i * 15 + 5, (INT)(Width - 10.0f), 15, 0, 0, 8, 15, LineTex);
    }
    if (Text && Text[0])
        Device->DrawText(7, 7, 0xFFDCDCDC, Text, (INT)(Width - 14.0f), 0, 1, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ccc70, NCPawnInfoWnd, OnPaint)

// 0x100cc420 - an empty text leaves the old one. Not inlined: the stock callers call it.
__declspec(noinline) void NCPawnInfoWnd::SetText(const TCHAR* InText)
{
    if (InText && *InText)
        appStrcpy(Text, InText);
}
NW_HOOK_METHOD(0x100cc420, NCPawnInfoWnd, SetText)

// 0x100cc410 - kept out of line, the stock callers call it
__declspec(noinline) void NCPawnInfoWnd::ClearText()
{
    Text[0] = 0;
}
NW_HOOK_METHOD(0x100cc410, NCPawnInfoWnd, ClearText)

NW_CHECK_VTABLE(NCPawnInfoWnd, 0x101c1050, 74)

/*-----------------------------------------------------------------------------
    NCPawnSetupWnd.
-----------------------------------------------------------------------------*/

// the creation window this panel belongs to (read without a check in the stock code)
inline NCPawnCreateWnd* NCPawnSetupWnd::CreateWnd()
{
    return static_cast<NCPawnCreateWnd*>(Entry->Parent->Wnd);
}

// the selectors from First on show nothing (inlined in each handler in the stock code)
static void ClearChoicesFrom(NCPawnSetupWnd* Wnd, INT First)
{
    switch (First)
    {
    case CHOICE_Race:
        Wnd->RaceSelect->SetSelected(-1);
    case CHOICE_ClassType:
        Wnd->ClassTypeSelect->SetSelected(-1);
    case CHOICE_Sex:
        Wnd->SexSelect->SetSelected(-1);
    case CHOICE_HairStyle:
        Wnd->HairStyleSelect->SetSelected(-1);
    case CHOICE_HairColor:
        Wnd->HairColorSelect->SetSelected(-1);
    case CHOICE_Face:
        Wnd->FaceSelect->SetSelected(-1);
    }
}

// the base class of a race and class type, -1 while they are incomplete. The stock switch lacks two breaks: dark elves and
// orcs without a class type get 53 (dwarven fighter). Inline in the stock OnClassType; GetClassID is its out-of-line
// instance.
static FORCEINLINE INT BaseClassID(INT Race, INT ClassType)
{
    switch (Race)
    {
    case 0:
        if (ClassType == 0)
            return 0;                  // human fighter
        if (ClassType == 1)
            return 10;                 // human mystic
        break;
    case 1:
        if (ClassType == 0)
            return 18;                 // elven fighter
        if (ClassType == 1)
            return 25;                 // elven mystic
        break;
    case 2:
        if (ClassType == 0)
            return 31;                 // dark fighter
        if (ClassType == 1)
            return 38;                 // dark mystic
        // falls through (sic)
    case 3:
        if (ClassType == 0)
            return 44;                 // orc fighter
        if (ClassType == 1)
            return 49;                 // orc mystic
        // falls through (sic)
    case 4:
        return 53;                     // dwarven fighter
    }
    return -1;
}

// the choices emptied, as the stock code does it inline (the array freed, nothing selected)
static FORCEINLINE void EmptyChoices(NCSelectCtrl* Select)
{
    Select->Items.Empty();
    Select->Selected = -1;
}

// the hair styles of a man: types A .. D and E (the empty is inline in the stock OnSex)
static FORCEINLINE void FillHairStylesMan(NCSelectCtrl* Select)
{
    EmptyChoices(Select);
    Select->AddItem(179);              // "Type A" .. "Type D"
    Select->AddItem(180);
    Select->AddItem(181);
    Select->AddItem(182);
    Select->AddItem(186);              // "Type E"
}

// ... and of a woman: the same with F and 803 (the stock calls ClearItems for the empty)
static FORCEINLINE void FillHairStylesWoman(NCSelectCtrl* Select)
{
    Select->ClearItems();
    Select->AddItem(179);
    Select->AddItem(180);
    Select->AddItem(181);
    Select->AddItem(182);
    Select->AddItem(186);
    Select->AddItem(187);              // "Type F"
    Select->AddItem(803);
}

// the default character of Type wears the given hair and face (inlined in the stock code). It
// works on a copy of the console's record: only the actor changes.
static FORCEINLINE void DressDefaultCharacter(INT Type, INT HairStyle, INT HairColor, INT Face)
{
    User Preview = ConsoleDefaultCharacters()[Type];
    if (Preview.Pawn)
    {
        Preview.HairStyle = HairStyle;
        Preview.HairColor = HairColor;
        Preview.Face = Face;
        Preview.SetPawnResource();
    }
}

// 0x100cf590 - the selectors, the name box and the background are made in OnCreate (the hair style, hair color and
// face selectors are not cleared: the allocator zeroes them); no choice yet
NCPawnSetupWnd::NCPawnSetupWnd()
{
    RaceSelect = NULL;
    ClassTypeSelect = NULL;
    SexSelect = NULL;
    UnknownE0 = 0;
    BackTex = NULL;
    NameEdit = NULL;
    CameraStep = 0;
    Race = -1;
    ClassType = -1;
    Sex = -1;
    HairStyle = -1;
    HairColor = -1;
    Face = -1;
}
NW_HOOK_CTOR(0x100cf590, NCPawnSetupWnd)


// 0x100cc450 (vslot 25) - each selector reports its new index
INT NCPawnSetupWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_SETUP_RACE)
            return OnRace((INT)lParam) != 0;
        if (wParam == NID_SETUP_CLASSTYPE)
            return OnClassType((INT)lParam) != 0;
        if (wParam == NID_SETUP_SEX)
            return OnSex((INT)lParam) != 0;
        if (wParam == NID_SETUP_HAIRSTYLE)
            return OnHairStyle((INT)lParam) != 0;
        if (wParam == NID_SETUP_HAIRCOLOR)
            return OnHairColor((INT)lParam) != 0;
        if (wParam == NID_SETUP_FACE)
            return OnFace((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cc450, NCPawnSetupWnd, WindowProc)

// 0x100cf680 (vslot 37) - C3: no title bar; the name box (up to 128 px past its width) and the six selectors, 24
// pixels apart (the hair color's before the hair style's id, as in the stock order of the ids)
INT NCPawnSetupWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.select2_back"));

    NameEdit = NewStock<NCEditBox>(0x100112c0, 0, (const TCHAR*)NULL, 0, 128, 0);
    NameEdit->ID = NID_SETUP_NAME;
    NameEdit->SetPosition(66, 17, 128, 17);
    CreateChild(NameEdit, NWS_VISIBLE);

    RaceSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    RaceSelect->ID = NID_SETUP_RACE;
    RaceSelect->SetPosition(86, 52, 136, 17);
    CreateChild(RaceSelect, NWS_VISIBLE);

    ClassTypeSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    ClassTypeSelect->ID = NID_SETUP_CLASSTYPE;
    ClassTypeSelect->SetPosition(86, 76, 136, 17);
    CreateChild(ClassTypeSelect, NWS_VISIBLE);

    SexSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    SexSelect->ID = NID_SETUP_SEX;
    SexSelect->SetPosition(86, 100, 136, 17);
    CreateChild(SexSelect, NWS_VISIBLE);

    HairStyleSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    HairStyleSelect->ID = NID_SETUP_HAIRSTYLE;
    HairStyleSelect->SetPosition(86, 124, 136, 17);
    CreateChild(HairStyleSelect, NWS_VISIBLE);

    HairColorSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    HairColorSelect->ID = NID_SETUP_HAIRCOLOR;
    HairColorSelect->SetPosition(86, 148, 136, 17);
    CreateChild(HairColorSelect, NWS_VISIBLE);

    FaceSelect = NewStock<NCSelectCtrl>(0x100cf3c0);
    FaceSelect->ID = NID_SETUP_FACE;
    FaceSelect->SetPosition(86, 172, 136, 17);
    CreateChild(FaceSelect, NWS_VISIBLE);

    InitSelectors();
    return 1;
}
NW_HOOK_METHOD(0x100cf680, NCPawnSetupWnd, OnCreate)

// 0x100cd050 (vslot 38) - the selectors are enabled again first (no NULL check)
INT NCPawnSetupWnd::OnDestroy()
{
    RaceSelect->EnableWindow(1);
    ClassTypeSelect->EnableWindow(1);
    SexSelect->EnableWindow(1);
    HairStyleSelect->EnableWindow(1);
    HairColorSelect->EnableWindow(1);
    FaceSelect->EnableWindow(1);
    DestroyChild(this, NameEdit);
    DestroyChild(this, RaceSelect);
    DestroyChild(this, ClassTypeSelect);
    DestroyChild(this, SexSelect);
    DestroyChild(this, HairStyleSelect);
    DestroyChild(this, HairColorSelect);
    DestroyChild(this, FaceSelect);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100cd050, NCPawnSetupWnd, OnDestroy)

// 0x100cc550 (vslot 48) - the name box takes the keyboard
void NCPawnSetupWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    NameEdit->SetFocus();
}
NW_HOOK_METHOD(0x100cc550, NCPawnSetupWnd, ShowWindow)

// 0x100cce00 (vslot 62) - C3: the background over the whole panel and the labels "Name" (sysstring 872, new),
// "Race", "Class", "Gender", "Hairstyle", "Hair Color", "Face" (164 .. 169), 24 pixels apart
INT NCPawnSetupWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPawnSetupWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    Device->DrawText(33, 20, 0xFFDCDCDC, *GL2GameData.SysString[872], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 55, 0xFFDCDCDC, *GL2GameData.SysString[164], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 79, 0xFFDCDCDC, *GL2GameData.SysString[165], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 103, 0xFFDCDCDC, *GL2GameData.SysString[166], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 127, 0xFFDCDCDC, *GL2GameData.SysString[167], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 151, 0xFFDCDCDC, *GL2GameData.SysString[168], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(33, 175, 0xFFDCDCDC, *GL2GameData.SysString[169], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cce00, NCPawnSetupWnd, OnPaint)

// 0x100ceba0 (vslot 74) - a race: the later choices are cleared, dwarves get the warrior type only, the class type is
// enabled and every default character comes back on the stage. The stock empties the list inline in both branches and
// ends them with one shared AddItem (orcs and the other races: warrior, then spellcaster)
INT NCPawnSetupWnd::OnRace(INT Index)
{
    ResetPreviews();
    Race = Index;
    ClassType = -1;
    Sex = -1;
    HairStyle = -1;
    HairColor = -1;
    Face = -1;
    INT Second;
    if (Index == RACE_Orc || Index != RACE_Dwarf)
    {
        EmptyChoices(ClassTypeSelect);
        ClassTypeSelect->AddItem(175);           // "Warrior Type"
        Second = 176;                            // "Spellcaster Type"
    }
    else
    {
        EmptyChoices(ClassTypeSelect);
        Second = 175;
    }
    ClassTypeSelect->AddItem(Second);
    ClassTypeSelect->SetSelected(ClassType);
    SexSelect->SetSelected(Sex);
    HairStyleSelect->SetSelected(HairStyle);
    HairColorSelect->SetSelected(HairColor);
    FaceSelect->SetSelected(Face);
    RaceSelect->EnableWindow(1);
    ClassTypeSelect->EnableWindow(1);
    SexSelect->EnableWindow(0);
    HairStyleSelect->EnableWindow(0);
    HairColorSelect->EnableWindow(0);
    FaceSelect->EnableWindow(0);
    CreateWnd()->ShowViewButtons(0);
    ConsolePawnCreateWnd()->InfoWnd->ClearText();
    ExecCameraEvent(1);
    GConsole->ShowAllDefaultCharacter();
    return 1;
}
NW_HOOK_METHOD(0x100ceba0, NCPawnSetupWnd, OnRace)

// 0x100ced90 (vslot 75) - a class type: the gender is enabled and the description shows the base class (classinfo
// 1..9). A class id outside the nine keeps Index as the classinfo line.
INT NCPawnSetupWnd::OnClassType(INT Index)
{
    ResetPreviews();
    ExecCameraEvent(2);
    ClassType = Index;
    Sex = -1;
    HairStyle = -1;
    HairColor = -1;
    Face = -1;
    SexSelect->SetSelected(Sex);
    HairStyleSelect->SetSelected(HairStyle);
    HairColorSelect->SetSelected(HairColor);
    FaceSelect->SetSelected(Face);
    if (Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        ClassTypeSelect->SetSelected(-1);
        return 1;
    }
    switch (BaseClassID(Race, ClassType))
    {
    case 0:  Index = 1; break;         // human fighter
    case 10: Index = 2; break;         // human mystic
    case 18: Index = 3; break;         // elven fighter
    case 25: Index = 4; break;         // elven mystic
    case 31: Index = 5; break;         // dark fighter
    case 38: Index = 6; break;         // dark mystic
    case 44: Index = 7; break;         // orc fighter
    case 49: Index = 8; break;         // orc mystic
    case 53: Index = 9; break;         // dwarven fighter
    }
    RaceSelect->EnableWindow(1);
    ClassTypeSelect->EnableWindow(1);
    SexSelect->EnableWindow(1);
    HairStyleSelect->EnableWindow(0);
    HairColorSelect->EnableWindow(0);
    FaceSelect->EnableWindow(0);
    CreateWnd()->ShowViewButtons(0);
    ConsolePawnCreateWnd()->InfoWnd->ClearText();
    ConsolePawnCreateWnd()->InfoWnd->SetText(*GL2GameData.ClassInfo[Index]);
    GConsole->ShowAllDefaultCharacter();
    return 1;
}
NW_HOOK_METHOD(0x100ced90, NCPawnSetupWnd, OnClassType)

// 0x100ceff0 (vslot 76) - a gender: the hair style list for it (orc women get theirs only with a class type), hair
// and face start at type A, everything is enabled, the view buttons appear and the stage keeps only the chosen
// default character
INT NCPawnSetupWnd::OnSex(INT Index)
{
    ResetPreviews();
    CreateWnd()->ShowViewButtons(0);
    if (Race == RACE_Orc)
    {
        if (Index == 0)
            FillHairStylesMan(HairStyleSelect);
        else if (Index == 1 && (ClassType == 0 || ClassType == 1))
            FillHairStylesWoman(HairStyleSelect);
    }
    else
    {
        if (Index == 0)
            FillHairStylesMan(HairStyleSelect);
        else if (Index == 1)
            FillHairStylesWoman(HairStyleSelect);
    }
    Sex = Index;
    HairStyle = 0;
    HairColor = 0;
    Face = 0;
    HairStyleSelect->SetSelected(HairStyle);
    HairColorSelect->SetSelected(HairColor);
    FaceSelect->SetSelected(Face);
    if (Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        ClearChoicesFrom(this, CHOICE_ClassType);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (ClassType < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectClassType));
        ClearChoicesFrom(this, CHOICE_Sex);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    RaceSelect->EnableWindow(1);
    ClassTypeSelect->EnableWindow(1);
    SexSelect->EnableWindow(1);
    HairStyleSelect->EnableWindow(1);
    HairColorSelect->EnableWindow(1);
    FaceSelect->EnableWindow(1);
    CreateWnd()->ShowViewButtons(1);
    ExecCameraEvent(3);
    GConsole->ShowAllDefaultCharacter();
    GConsole->ShowOnlyOneDefaultCharacter(GetCharacterType());
    return 1;
}
NW_HOOK_METHOD(0x100ceff0, NCPawnSetupWnd, OnSex)

// 0x100cd350 (vslot 77) - a hair style, worn at once by the chosen default character
INT NCPawnSetupWnd::OnHairStyle(INT Index)
{
    ResetPreviews();
    HairStyle = Index;
    HairColorSelect->SetSelected(HairColor);
    FaceSelect->SetSelected(Face);
    if (Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        ClearChoicesFrom(this, CHOICE_ClassType);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (ClassType < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectClassType));
        ClearChoicesFrom(this, CHOICE_Sex);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (Sex < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectSex));
        ClearChoicesFrom(this, CHOICE_HairStyle);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    DressDefaultCharacter(GetCharacterType(), HairStyle, HairColor, Face);
    return 1;
}
NW_HOOK_METHOD(0x100cd350, NCPawnSetupWnd, OnHairStyle)

// 0x100cd660 (vslot 78) - a hair color
INT NCPawnSetupWnd::OnHairColor(INT Index)
{
    ResetPreviews();
    HairColor = Index;
    FaceSelect->SetSelected(Face);
    if (Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        ClearChoicesFrom(this, CHOICE_ClassType);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (ClassType < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectClassType));
        ClearChoicesFrom(this, CHOICE_Sex);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (Sex < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectSex));
        ClearChoicesFrom(this, CHOICE_HairStyle);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (HairStyle < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectHairStyle));
        return 1;
    }
    DressDefaultCharacter(GetCharacterType(), HairStyle, HairColor, Face);
    return 1;
}
NW_HOOK_METHOD(0x100cd660, NCPawnSetupWnd, OnHairColor)

// 0x100cd980 (vslot 79) - a face
INT NCPawnSetupWnd::OnFace(INT Index)
{
    ResetPreviews();
    Face = Index;
    if (Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        ClearChoicesFrom(this, CHOICE_ClassType);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (ClassType < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectClassType));
        ClearChoicesFrom(this, CHOICE_Sex);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (Sex < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectSex));
        ClearChoicesFrom(this, CHOICE_HairStyle);
        CreateWnd()->ShowViewButtons(0);
        return 1;
    }
    if (HairStyle < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectHairStyle));
        ClearChoicesFrom(this, CHOICE_HairColor);
        return 1;
    }
    if (HairColor < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectHairColor));
        return 1;
    }
    DressDefaultCharacter(GetCharacterType(), HairStyle, HairColor, Index);
    return 1;
}
NW_HOOK_METHOD(0x100cd980, NCPawnSetupWnd, OnFace)

// 0x100cc570 - index of the default character of the race / class type / gender chosen. Kept out of line, the stock
// callers call it.
__declspec(noinline) INT NCPawnSetupWnd::GetCharacterType()
{
    return (Race * 2 + ClassType) * 2 + Sex;
}
NW_HOOK_METHOD(0x100cc570, NCPawnSetupWnd, GetCharacterType)

// 0x100cc590 - every default character back to hair style A and facing the camera; works on copies of the console's
// records, so only the actors change (this is not used)
void NCPawnSetupWnd::ResetPreviews()
{
    for (INT i = 0; i < 18; i++)
    {
        User Preview = ConsoleDefaultCharacters()[i];
        if (Preview.Pawn)
        {
            Preview.HairStyle = 0;
            Preview.SetPawnResource();
            GConsole->ResetDefaultCharacterYaw(i);
        }
    }
}
NW_HOOK_METHOD(0x100cc590, NCPawnSetupWnd, ResetPreviews)

// 0x100cc610 - the base class of the race and class type, -1 while they are incomplete. The stock switch lacks two
// breaks: dark elves and orcs without a class type get 53 (dwarven fighter), so creating one then complains about the
// gender instead of the class type.
INT NCPawnSetupWnd::GetClassID()
{
    return BaseClassID(Race, ClassType);
}
NW_HOOK_METHOD(0x100cc610, NCPawnSetupWnd, GetClassID)

// 0x100cc6b0 - the engine learns the race being made (this is not used). Not inlined: the
// stock caller calls it.
__declspec(noinline) void NCPawnSetupWnd::SetMakingRace(INT InRace)
{
    GConsole->SetCurrentMakingRace(InRace);
}
NW_HOOK_METHOD(0x100cc6b0, NCPawnSetupWnd, SetMakingRace)

// 0x100cd260 - an empty panel (the console calls it through NCPawnCreateWnd::Reset). Not
// inlined: the stock caller calls it.
__declspec(noinline) void NCPawnSetupWnd::Reset()
{
    CameraStep = 0;
    NameEdit->ClearText();
    Race = -1;
    ClassType = -1;
    Sex = -1;
    HairStyle = -1;
    HairColor = -1;
    Face = -1;
    ClearChoicesFrom(this, CHOICE_Race);
    CreateWnd()->ShowViewButtons(0);
}
NW_HOOK_METHOD(0x100cd260, NCPawnSetupWnd, Reset)

// the name typed as the stock code measures it: bytes in the ANSI code page (C3: a buffer of 800 bytes)
static FORCEINLINE INT AnsiLength(const TCHAR* Text)
{
    ANSICHAR Ansi[800];
    WideCharToMultiByte(CP_ACP, 0, Text, -1, Ansi, 800, NULL, NULL);
    return (INT)strlen(Ansi);
}

// 0x100cdd00 - the name and the choices, or a system message and 0. Race, gender and hair style are read from the
// selectors, the class from GetClassID; hair color and face are left to the caller. The name may be 12 bytes (Thai
// client) or 16 bytes long, in the ANSI code page.
UBOOL NCPawnSetupWnd::GetCreateInfo(NCharacterInfo* Info)
{
    if (!NameEdit->CommitText())
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_BadNameLength));
        return 0;
    }
    INT Length = AnsiLength(NameEdit->CommitText());
    if (Length == 0 || Length > (GLanguageType != 5) * 4 + 12)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_BadNameLength));
        NameEdit->SetFocus();
        return 0;
    }
    wcscpy(Info->Name, NameEdit->CommitText());
    Info->Race = RaceSelect->Selected;
    if (Info->Race < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectRace));
        return 0;
    }
    Info->ClassID = GetClassID();
    if (Info->ClassID < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectClassType));
        return 0;
    }
    Info->Sex = SexSelect->Selected;
    if (Info->Sex < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectSex));
        return 0;
    }
    Info->HairStyle = HairStyleSelect->Selected;
    if (Info->HairStyle < 0)
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_SelectHairStyle));
        return 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x100cdd00, NCPawnSetupWnd, GetCreateInfo)

// 0x100cdee0 - the lobby event that moves the camera after a choice (Step 1 race, 2 class type, 3 gender), named from
// the selectors (already on the new item) and the map's name parts:
//   forward:   every choice so far, "ELF", "ELF_WIZARD", "ELF_WIZARD_WWOMAN";
//   same step: the change, "ELF_KNIGHT_WIZARD" (class type), "ELF_WMAN_WWOMAN" (gender); the old item is the other
//              of the two, taken from the new one;
//   back to 2: "ELF_WIZARD_KWOMAN" played in reverse: new class type, then the old gender view, whose _K / _W again
//              comes from the new class type.
// Dwarves (one class type) skip the class type change and the step back to it.
void NCPawnSetupWnd::ExecCameraEvent(INT Step)
{
    if (Step > CameraStep)
    {
        INT SelClassType = ClassTypeSelect->Selected;
        INT SelRace = RaceSelect->Selected;
        INT SelSex = SexSelect->Selected;
        NString Event;
        if (SelRace >= 0)
        {
            Event.Append(GRaceEventNames[SelRace]);
            CameraStep = 1;
        }
        if (SelClassType >= 0)
        {
            Event.Append(TEXT("_"));
            Event.Append(GClassTypeEventNames[SelClassType]);
            CameraStep = 2;
        }
        if (SelSex >= 0)
        {
            if (SelClassType == 0)
                Event.Append(TEXT("_K"));
            else if (SelClassType == 1)
                Event.Append(TEXT("_W"));
            Event.Append(GSexEventNames[SelSex]);
            CameraStep = 3;
        }
        SetMakingRace(SelRace);
        GConsole->ExecLobbyEvent(Event.GetAt(0), 0);
    }
    else if (Step == CameraStep)
    {
        INT SelRace = RaceSelect->Selected;
        INT SelSex = SexSelect->Selected;
        INT SelClassType = ClassTypeSelect->Selected;
        NString Event;
        if (Step == 1)
        {
            Event.Append(GRaceEventNames[SelRace]);
            SetMakingRace(SelRace);
            GConsole->ExecLobbyEvent(Event.GetAt(0), 0);
        }
        else if (Step == 2)
        {
            if (SelRace != RACE_Dwarf)
            {
                Event.Append(GRaceEventNames[SelRace]);
                Event.Append(TEXT("_"));
                if (SelClassType == 0)
                    Event.Append(GClassTypeEventNames[1]);
                else if (SelClassType == 1)
                    Event.Append(GClassTypeEventNames[0]);
                Event.Append(TEXT("_"));
                Event.Append(GClassTypeEventNames[SelClassType]);
                SetMakingRace(SelRace);
                GConsole->ExecLobbyEvent(Event.GetAt(0), 0);
            }
        }
        else if (Step == 3)
        {
            Event.Append(GRaceEventNames[SelRace]);
            if (SelClassType == 0)
                Event.Append(TEXT("_K"));
            else if (SelClassType == 1)
                Event.Append(TEXT("_W"));
            if (SelSex == 0)
                Event.Append(GSexEventNames[1]);
            else if (SelSex == 1)
                Event.Append(GSexEventNames[0]);
            if (SelClassType == 0)
                Event.Append(TEXT("_K"));
            else if (SelClassType == 1)
                Event.Append(TEXT("_W"));
            Event.Append(GSexEventNames[SelSex]);
            SetMakingRace(SelRace);
            GConsole->ExecLobbyEvent(Event.GetAt(0), 0);
        }
    }
    else if (Step < CameraStep)
    {
        INT SelRace = RaceSelect->Selected;
        INT SelSex = SexSelect->Selected;
        INT SelClassType = ClassTypeSelect->Selected;
        NString Event;
        if (Step == 1)
        {
            Event.Append(GRaceEventNames[SelRace]);
            SetMakingRace(SelRace);
            GConsole->ExecLobbyEvent(Event.GetAt(0), 0);
            CameraStep = Step;
        }
        else if (Step == 2 && SelRace != RACE_Dwarf)
        {
            Event.Append(GRaceEventNames[SelRace]);
            Event.Append(TEXT("_"));
            Event.Append(GClassTypeEventNames[SelClassType]);
            if (SelClassType == 1)
                Event.Append(TEXT("_K"));
            else if (SelClassType == 0)
                Event.Append(TEXT("_W"));
            Event.Append(GSexEventNames[SelSex]);
            SetMakingRace(SelRace);
            GConsole->ExecLobbyEvent(Event.GetAt(0), 1);
            CameraStep = 2;
        }
    }
}
NW_HOOK_METHOD(0x100cdee0, NCPawnSetupWnd, ExecCameraEvent)

// 0x100ce9c0 - the items of the six selectors (OnRace and OnSex redo the class type and hair
// style lists); only the race is enabled
void NCPawnSetupWnd::InitSelectors()
{
    RaceSelect->AddItem(170);          // "Human", "Elf", "Dark elf", "Orc", "Dwarf"
    RaceSelect->AddItem(171);
    RaceSelect->AddItem(172);
    RaceSelect->AddItem(173);
    RaceSelect->AddItem(174);
    ClassTypeSelect->AddItem(175);     // "Warrior Type", "Spellcaster Type"
    ClassTypeSelect->AddItem(176);
    SexSelect->AddItem(177);           // "Male", "Female"
    SexSelect->AddItem(178);
    HairStyleSelect->AddItem(179);     // "Type A" .. "Type D", "Type E", "Type F", and 803
    HairStyleSelect->AddItem(180);
    HairStyleSelect->AddItem(181);
    HairStyleSelect->AddItem(182);
    HairStyleSelect->AddItem(186);
    HairStyleSelect->AddItem(187);
    HairStyleSelect->AddItem(803);
    HairColorSelect->AddItem(179);     // "Type A" .. "Type D" (C1: A, B)
    HairColorSelect->AddItem(180);
    HairColorSelect->AddItem(181);
    HairColorSelect->AddItem(182);
    FaceSelect->AddItem(179);          // "Type A" .. "Type C" (C1: A, B)
    FaceSelect->AddItem(180);
    FaceSelect->AddItem(181);
    RaceSelect->EnableWindow(1);
    ClassTypeSelect->EnableWindow(0);
    SexSelect->EnableWindow(0);
    HairStyleSelect->EnableWindow(0);
    HairColorSelect->EnableWindow(0);
    FaceSelect->EnableWindow(0);
    CameraStep = 0;
}
NW_HOOK_METHOD(0x100ce9c0, NCPawnSetupWnd, InitSelectors)

NW_CHECK_VTABLE(NCPawnSetupWnd, 0x101c1178, 80)

/*-----------------------------------------------------------------------------
    NCPawnCreateFunctionWnd (C3).
-----------------------------------------------------------------------------*/

// 0x100cc1b0
NCPawnCreateFunctionWnd::NCPawnCreateFunctionWnd()
{
}
NW_HOOK_CTOR(0x100cc1b0, NCPawnCreateFunctionWnd)

// 0x100cbbf0 (vslot 25) - the two buttons call the creation window's virtuals 68 and 69 of this window
INT NCPawnCreateFunctionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CREATE_PREVIOUS)
            return OnGameCancelClick() != 0;
        if (wParam == NID_CREATE_CREATE)
            return OnAccountClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cbbf0, NCPawnCreateFunctionWnd, WindowProc)

// 0x100cbfb0 (vslot 37) - the menu background and the two big buttons ("Create Character" above "Previous")
INT NCPawnCreateFunctionWnd::OnCreate()
{
    guard(NCPawnCreateFunctionWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.lobby_menuback"));

    CreateButton = NewStock<NCButton>(0x10003340, TEXT("L2UI_CH3.Button.BigButton"),
                                      TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    CreateButton->ID = NID_CREATE_CREATE;
    CreateButton->SetPosition(13, 20, 96, 23);
    CreateChild(CreateButton, NWS_VISIBLE);
    CreateButton->SetTextID(162);                               // "Create Character"

    PreviousButton = NewStock<NCButton>(0x10003340, TEXT("L2UI_CH3.Button.BigButton"),
                                        TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    PreviousButton->ID = NID_CREATE_PREVIOUS;
    PreviousButton->SetPosition(13, 45, 96, 23);
    CreateChild(PreviousButton, NWS_VISIBLE);
    PreviousButton->SetTextID(161);                             // "Previous"
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cbfb0, NCPawnCreateFunctionWnd, OnCreate)

// 0x100cbc70 (vslot 38) - the two buttons are deleted, then NCWnd's OnDestroy (C4; C3 did not call it)
INT NCPawnCreateFunctionWnd::OnDestroy()
{
    guard(NCPawnCreateFunctionWnd::OnDestroy);
    DestroyChild(this, CreateButton);
    DestroyChild(this, PreviousButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cbc70, NCPawnCreateFunctionWnd, OnDestroy)

// 0x100cbd40 (vslot 62) - the background (a 120 x 138 piece, stretched over the window)
INT NCPawnCreateFunctionWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPawnCreateFunctionWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, 120, 138, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cbd40, NCPawnCreateFunctionWnd, OnPaint)

// 0x100cbe90 (vslot 74) - the previous button: the creation window's slot 71 (OnPrevious); the cast is checked
INT NCPawnCreateFunctionWnd::OnGameCancelClick()
{
    guard(NCPawnCreateFunctionWnd::OnGameCancelClick);
    CastChecked<NCPawnCreateWnd>(Entry->Parent->Wnd)->NewVirtual77();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cbe90, NCPawnCreateFunctionWnd, OnGameCancelClick)

// 0x100cbf20 (vslot 75) - the create button: the creation window's slot 72 (OnCreateCharacter)
INT NCPawnCreateFunctionWnd::OnAccountClick()
{
    guard(NCPawnCreateFunctionWnd::OnAccountClick);
    CastChecked<NCPawnCreateWnd>(Entry->Parent->Wnd)->NewVirtual78();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100cbf20, NCPawnCreateFunctionWnd, OnAccountClick)

NW_CHECK_VTABLE(NCPawnCreateFunctionWnd, 0x101c0a40, 76)

/*-----------------------------------------------------------------------------
    NCPawnCreateWnd.
-----------------------------------------------------------------------------*/

// 0x100cfb30 - no zoom
NCPawnCreateWnd::NCPawnCreateWnd()
{
    bZoomed = 0;
}
NW_HOOK_CTOR(0x100cfb30, NCPawnCreateWnd)

// 0x100cc6c0 (vslot 25) - the turn buttons report 1 when pressed and 0 when released (C3: the create and previous
// buttons are in NCPawnCreateFunctionWnd now)
INT NCPawnCreateWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CREATE_LEFT)
            return OnTurnLeft((INT)lParam) != 0;
        if (wParam == NID_CREATE_RIGHT)
            return OnTurnRight((INT)lParam) != 0;
        if (wParam == NID_CREATE_ZOOM)
            return OnZoom() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100cc6c0, NCPawnCreateWnd, WindowProc)

// 0x100cfbd0 (vslot 37) - the panels on the left (the setup panel is titled by the SysString id: virtual 56 of this client),
// the turn and zoom buttons under the stage (hidden: the position is computed in floats) and the function window in the
// lower right corner; everything held by an anchor. C4: the buttons' textures are the lobby's and their third argument
// is 2
INT NCPawnCreateWnd::OnCreate()
{
    NCWnd::OnCreate();
    bZoomed = 0;

    SetupWnd = NewStock<NCPawnSetupWnd>(0x100cf590);
    SetupWnd->SetPosition(24, 27, 256, 205);
    CreateChild(SetupWnd, NWS_NOCLOSE | NWS_VISIBLE);
    ::nw::VirtualCall<void>(SetupWnd, 56, (INT)158);            // "Create"
    SetupWnd->SetAnchor(1, 24, 27);

    InfoWnd = NewStock<NCPawnInfoWnd>(0x100cf4e0);
    InfoWnd->SetPosition(24, 249, 256, 205);
    CreateChild(InfoWnd, NWS_VISIBLE);
    InfoWnd->SetAnchor(1, 24, 249);

    LeftButton = NewStock<NCExButton>(0x10003790, TEXT("l2ui.LobbyWnd.left"), TEXT("l2ui.LobbyWnd.left_click"), 2);
    LeftButton->ID = NID_CREATE_LEFT;
    LeftButton->SetPosition((INT)(Width * 0.5f - 88.0f), (INT)(Height - 100.0f), 44, 44);
    CreateChild(LeftButton, NWS_VISIBLE);
    LeftButton->HideWindow();
    LeftButton->SetAnchor(8, -66, -56);

    RightButton = NewStock<NCExButton>(0x10003790, TEXT("l2ui.LobbyWnd.right"), TEXT("l2ui.LobbyWnd.right_click"), 2);
    RightButton->ID = NID_CREATE_RIGHT;
    RightButton->SetPosition((INT)(Width * 0.5f + 44.0f), (INT)(Height - 100.0f), 44, 44);
    CreateChild(RightButton, NWS_VISIBLE);
    RightButton->HideWindow();
    RightButton->SetAnchor(8, 66, -56);

    ZoomInButton = NewStock<NCButton>(0x10003340, TEXT("l2ui.LobbyWnd.zoomin"), TEXT("l2ui.LobbyWnd.zoomin_click"), 2);
    ZoomInButton->ID = NID_CREATE_ZOOM;
    ZoomInButton->SetPosition((INT)(Width * 0.5f - 22.0f), (INT)(Height - 100.0f), 44, 44);
    CreateChild(ZoomInButton, NWS_VISIBLE);
    ZoomInButton->HideWindow();
    ZoomInButton->SetAnchor(8, 0, -56);

    ZoomOutButton = NewStock<NCButton>(0x10003340, TEXT("l2ui.LobbyWnd.zoomout"), TEXT("l2ui.LobbyWnd.zoomout_click"), 2);
    ZoomOutButton->ID = NID_CREATE_ZOOM;
    ZoomOutButton->SetPosition((INT)(Width * 0.5f - 22.0f), (INT)(Height - 100.0f), 44, 44);
    CreateChild(ZoomOutButton, NWS_VISIBLE);
    ZoomOutButton->HideWindow();
    ZoomOutButton->SetAnchor(8, 0, -56);

    FunctionWnd = NewStock<NCPawnCreateFunctionWnd>(0x100cc1b0);
    FunctionWnd->SetPosition(GSEKScreenX - 154, GSEKScreenY - 200, 120, 138);
    CreateChild(FunctionWnd, NWS_VISIBLE);
    FunctionWnd->SetAnchor(9, -34, -10);
    return 1;
}
NW_HOOK_METHOD(0x100cfbd0, NCPawnCreateWnd, OnCreate)

// 0x100ce310 (vslot 38)
INT NCPawnCreateWnd::OnDestroy()
{
    guard(NCPawnCreateWnd::OnDestroy);
    DestroyChild(this, SetupWnd);
    DestroyChild(this, InfoWnd);
    DestroyChild(this, FunctionWnd);
    DestroyChild(this, LeftButton);
    DestroyChild(this, RightButton);
    DestroyChild(this, ZoomInButton);
    DestroyChild(this, ZoomOutButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ce310, NCPawnCreateWnd, OnDestroy)

// 0x100cc8b0 (vslot 48) - the view buttons wait for a gender
void NCPawnCreateWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    ShowViewButtons(0);
}
NW_HOOK_METHOD(0x100cc8b0, NCPawnCreateWnd, ShowWindow)

// 0x100cc7c0 (vslot 49)
void NCPawnCreateWnd::HideWindow()
{
    NCWnd::HideWindow();
    SetupWnd->ResetPreviews();
}
NW_HOOK_METHOD(0x100cc7c0, NCPawnCreateWnd, HideWindow)

// 0x10001b10  empty, shared with every window that paints nothing (the stock body is `xor eax, eax`, ret 4); the hook build keeps
// the jump to the shared stock body, the standalone has its own
#if NW_HOOK_MODE
NW_NAKED INT NCPawnCreateWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100023d0)
#else
INT NCPawnCreateWnd::OnPaint(NCGDevice* Device)
{
    return 0;
}
#endif

// 0x100cc780 (vslot 71, NewVirtual71) - "Previous": the voices fade out over 1.5 seconds, then back to the character
// selection
INT NCPawnCreateWnd::OnPrevious()
{
    if (GConsole && GConsole->Engine && GConsole->Engine->Audio)
        GConsole->Engine->Audio->StopAllVoice(1.5f, 0);
    GConsole->ReturnToCharacterSelect();
    return 1;
}
NW_HOOK_METHOD(0x100cc780, NCPawnCreateWnd, OnPrevious)

// 0x100ce4e0 (vslot 72, NewVirtual72) - "Create Character": the choices must be complete and the name valid. The stats
// sent are the last class template's the server sent (SetRecommendedData keeps one).
INT NCPawnCreateWnd::OnCreateCharacter()
{
    NCharacterInfo Info;
    if (!SetupWnd->GetCreateInfo(&Info))
        return 1;
    if (Info.Name[0] && (GConsole->HasInvalidChars(Info.Name) || !GConsole->IsValidName(Info.Name)))
    {
        GConsole->ShowMessageWnd(SystemMessage(MSG_IncorrectName));
        SetupWnd->NameEdit->ClearText();
        SetupWnd->NameEdit->SetFocus();
        return 1;
    }
    for (INT i = 0; i < 6; i++)
        Info.Stats[i] = Recommended.Stats[i].Value;
    Info.HairStyle = SetupWnd->HairStyle;
    Info.HairColor = SetupWnd->HairColor;
    Info.Face = SetupWnd->Face;
    GConsole->RequestCharacterCreate(Info);               // 0x1005ad90 (C2 CharacterInfo, 0x60 bytes by value)
    return 1;
}
NW_HOOK_METHOD(0x100ce4e0, NCPawnCreateWnd, OnCreateCharacter)

// 0x100cc8d0 (vslot 74) - the chosen default character turns while the button is held
INT NCPawnCreateWnd::OnTurnLeft(INT bPressed)
{
    INT Type = SetupWnd->GetCharacterType();
    if (bPressed)
        GConsole->DefaultCharacterTurn(Type, 2.0f);
    else
        GConsole->DefaultCharacterStop(Type);
    return 1;
}
NW_HOOK_METHOD(0x100cc8d0, NCPawnCreateWnd, OnTurnLeft)

// 0x100cc910 (vslot 75)
INT NCPawnCreateWnd::OnTurnRight(INT bPressed)
{
    INT Type = SetupWnd->GetCharacterType();
    if (bPressed)
        GConsole->DefaultCharacterTurn(Type, -2.0f);
    else
        GConsole->DefaultCharacterStop(Type);
    return 1;
}
NW_HOOK_METHOD(0x100cc910, NCPawnCreateWnd, OnTurnRight)

// 0x100ce610 (vslot 76) - zoom on the chest and back: the event is the gender view's name plus "_CHEST"
// ("ELF_WOMAN_CHEST"), played in reverse to zoom out. The stock code builds the name in both branches.
INT NCPawnCreateWnd::OnZoom()
{
    bZoomed = !bZoomed;
    INT SelRace = SetupWnd->RaceSelect->Selected;
    INT SelClassType = SetupWnd->ClassTypeSelect->Selected;
    INT SelSex = SetupWnd->SexSelect->Selected;
    NString Event;
    if (bZoomed)
    {
        ZoomOutButton->ShowWindow();
        ZoomInButton->HideWindow();
    }
    else
    {
        ZoomOutButton->HideWindow();
        ZoomInButton->ShowWindow();
    }
    if (SelRace >= 0)
        Event.Append(GRaceEventNames[SelRace]);
    if (SelSex >= 0)
    {
        if (SelClassType == 0)
            Event.Append(TEXT("_K"));
        else if (SelClassType == 1)
            Event.Append(TEXT("_W"));
        Event.Append(GSexEventNames[SelSex]);
        Event.Append(TEXT("_CHEST"));
    }
    GConsole->ExecLobbyEvent(Event.GetAt(0), bZoomed ? 0 : 1);
    return 1;
}
NW_HOOK_METHOD(0x100ce610, NCPawnCreateWnd, OnZoom)

// 0x100cc760 - the class template of the race / class chosen (C3: the structure is kept as a whole)
void NCPawnCreateWnd::SetRecommendedData(RecommandedDataStr Data)
{
    Recommended = Data;
}
NW_HOOK_METHOD(0x100cc760, NCPawnCreateWnd, SetRecommendedData)

// 0x100cc7e0 - the turn and zoom buttons (zoom in or out by bZoomed) with only the chosen default character on the
// stage; hiding them also forgets the zoom. Not inlined: the stock callers call it.
__declspec(noinline) void NCPawnCreateWnd::ShowViewButtons(UBOOL bShow)
{
    if (bShow)
    {
        LeftButton->ShowWindow();
        RightButton->ShowWindow();
        if (bZoomed)
        {
            ZoomOutButton->ShowWindow();
            ZoomInButton->HideWindow();
        }
        else
        {
            ZoomOutButton->HideWindow();
            ZoomInButton->ShowWindow();
        }
        GConsole->ShowOnlyOneDefaultCharacter(SetupWnd->GetCharacterType());
    }
    else
    {
        LeftButton->HideWindow();
        RightButton->HideWindow();
        ZoomInButton->HideWindow();
        ZoomOutButton->HideWindow();
        bZoomed = 0;
    }
}
NW_HOOK_METHOD(0x100cc7e0, NCPawnCreateWnd, ShowViewButtons)

// 0x100ce4c0 - NConsoleWnd::AcceptNewCharacter empties the screen before showing it
void NCPawnCreateWnd::Reset()
{
    SetupWnd->Reset();
    InfoWnd->ClearText();
}
NW_HOOK_METHOD(0x100ce4c0, NCPawnCreateWnd, Reset)

NW_CHECK_VTABLE(NCPawnCreateWnd, 0x101c12b8, 79)
