/*=============================================================================
    NCGMWnd.cpp - the GM's windows (C3 addresses). Stock: 0x1007e070 .. 0x1007f9db (their small functions),
    0x10081e00 .. 0x10083fc3 (paints, OnCreates, OnDestroys, the name searches), 0x10084330 .. 0x100852c1 (the
    constructors and destructors), 0x10136330 (the map iterator's destructor). NCGMStoreWnd's functions sit in
    between; NCGMActionWnd's OnDestroy is NCPetActionWnd's body, its OnPaint NCSkillWnd's.
=============================================================================*/
#include "nwindow/NCGMWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDetailStatusWnd.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCMagicSkillWnd.h"
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NCQuestInfoWnd.h"
#include "nwindow/NCQuestItemWnd.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NGameData.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NItemInfoC4.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/NWinBStock.h"   // NToolTipInfoLinesView
#include "nwindow/NMsnProtocol.h"   // NMsnIntMap (Rehash)

// sysstring
enum
{
    STR_Item = 2,
    STR_Name = 50,
    STR_Lv = 88,
    STR_Quest = 118,
    STR_Skills = 119,
    STR_Warehouse = 131,
    STR_Inventory = 138,
    STR_AcceptedQuest = 324,
    STR_Description = 325,
    STR_QuestItem = 326,
    STR_Cancel = 327,
    STR_Leader = 342,
    STR_Base = 343,
    STR_HideoutExists = 344,
    STR_CastleExists = 345,
    STR_Status = 433,
    STR_Clan = 439,
    STR_NewTeleport = 899,         // C3 (names ours): the texts of the builder buttons
    STR_NewInstantMove = 900,
    STR_NewAddPetiChat = 902,
};

enum { MSG_EnterUserName = 364 };      // systemmsg "Enter user's name to search."

enum { DIALOG_SystemMessage = 0x1F };  // NDialogInfo::ID: a message, OK only

enum { CLAN_TEXT_COLOR = 0xFFB09B79, CLAN_LABEL_COLOR = 0xFFA3A3A3 };

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

/*-----------------------------------------------------------------------------
    NCGMWnd
-----------------------------------------------------------------------------*/

// 0x100907e0 - NCWnd's constructor, then the eight words from +0xE8 on cleared one by one and the vtable
NCGMWnd::NCGMWnd()
{
    BottomTex = NULL;
    NameEdit = NULL;
    InventoryButton = NULL;
    SkillsButton = NULL;
    StatusButton = NULL;
    QuestButton = NULL;
    WarehouseButton = NULL;
    ClanButton = NULL;
}
NW_HOOK_CTOR(0x100907e0, NCGMWnd)

// 0x10090860
NW_HOOK_DTOR(0x10090860, NCGMWnd)

// 0x10089410 (vslot 25) - the buttons by id: slots 74 .. 90 (the six view buttons, force_peti, the builder commands, the
// searches by name, and the change of server that C4 added)
INT NCGMWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_GM_INVENTORY)
            return OnInventory() != 0;
        if (wParam == NID_GM_SKILLS)
            return OnSkills() != 0;
        if (wParam == NID_GM_STATUS)
            return OnStatus() != 0;
        if (wParam == NID_GM_QUEST)
            return OnQuest() != 0;
        if (wParam == NID_GM_WAREHOUSE)
            return OnWarehouse() != 0;
        if (wParam == NID_GM_CLAN)
            return OnClan() != 0;
        if (wParam == NID_GM_FORCEPETI)
            return OnForcePeti() != 0;
        if (wParam == NID_GM_NEW1)
            return OnNewButton1() != 0;
        if (wParam == NID_GM_NEW7)
            return ::nw::VirtualCall<INT>(this, 82) != 0;
        if (wParam == NID_GM_NEW4)
            return OnNewButton4() != 0;
        if (wParam == NID_GM_NEW5)
            return OnNewButton5() != 0;
        if (wParam == NID_GM_NEW6)
            return OnNewButton6() != 0;
        if (wParam == NID_GM_NEW8)
            return ::nw::VirtualCall<INT>(this, 86) != 0;
        if (wParam == NID_GM_NEW9)
            return ::nw::VirtualCall<INT>(this, 87) != 0;
        if (wParam == NID_GM_NEW2)
            return ::nw::VirtualCall<INT>(this, 88) != 0;
        if (wParam == NID_GM_NEW3)
            return ::nw::VirtualCall<INT>(this, 89) != 0;
        if (wParam == NID_GM_CHANGESERVER)
            return ::nw::VirtualCall<INT>(this, 90) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10089410, NCGMWnd, WindowProc)

// the six view buttons: the desktop's toggle with the name typed, or a message without one (the text is fetched
// twice; C3: the message is in 0xFFDCDCDC)
#define NCGMWND_VIEW_BUTTON(Method, Toggle)                                                   \
    INT NCGMWnd::Method()                                                                     \
    {                                                                                         \
        if (NameEdit->CommitText())                                                           \
            GDesktop->Toggle((TCHAR*)NameEdit->CommitText());                                 \
        else                                                                                  \
            GConsole->ShowSystemMessage(MSG_EnterUserName, 0xFFDCDCDC);                       \
        return 1;                                                                             \
    }

// 0x10089650 (vslot 74)
NCGMWND_VIEW_BUTTON(OnInventory, ToggleGMInventory)
NW_HOOK_METHOD(0x10089650, NCGMWnd, OnInventory)
// 0x100896b0 (vslot 75)
NCGMWND_VIEW_BUTTON(OnSkills, ToggleGMMagicSkill)
NW_HOOK_METHOD(0x100896b0, NCGMWnd, OnSkills)
// 0x10089710 (vslot 76)
NCGMWND_VIEW_BUTTON(OnStatus, ToggleGMDetailStatus)
NW_HOOK_METHOD(0x10089710, NCGMWnd, OnStatus)
// 0x10089770 (vslot 77)
NCGMWND_VIEW_BUTTON(OnQuest, ToggleGMQuest)
NW_HOOK_METHOD(0x10089770, NCGMWnd, OnQuest)
// 0x100897d0 (vslot 78)
NCGMWND_VIEW_BUTTON(OnWarehouse, ToggleGMStore)
NW_HOOK_METHOD(0x100897d0, NCGMWnd, OnWarehouse)
// 0x10089830 (vslot 79)
NCGMWND_VIEW_BUTTON(OnClan, ToggleGMAction)
NW_HOOK_METHOD(0x10089830, NCGMWnd, OnClan)

// 0x10089890 (vslot 80, name ours) - C4: the GM window has no close handler; its button 0xF0F016 asks for "force_peti
// <name> <system message 1528>" (the text in GL2GameData + 0x8414C)
INT NCGMWnd::OnForcePeti()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
    {
        FString Command(TEXT("force_peti "));
        Command += Name;
        Command += TEXT(" ");
        Command += *::nw::Field<FString>(&GL2GameData, 0x8414C);
        L2ParamStack Params(10);
        Params.PushBack((void*)*Command);
        ::nw::VirtualCall<void>(GConsole, 194, &Params);
    }
    return 1;
}
NW_HOOK_METHOD(0x10089890, NCGMWnd, OnForcePeti)

// a button of the window (repeated in the stock OnCreate): 54 x 18 at (X, Y), the two "sek" textures, its text a system
// string id
static FORCEINLINE NCButton* CreateGMButton(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("sek.cbui91"), TEXT("sek.cbui93"), 2);
    Button->ID = ID;
    Button->SetPosition(X, Y, 54, 18);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}

// 0x1008dac0 (vslot 37) - C4: the back picture, the decorated title bar (20 high, as wide as the window), the name's box
// and the buttons (17: the builder ones, the view ones and the two new ones at the end, "force_peti" and the server change)
INT NCGMWnd::OnCreate()
{
    NCWnd::OnCreate();
    BottomTex = FindTexture(TEXT("l2ui_ch3.siegewnd.siege_back31"), 1);
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GM_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    NameEdit = new NCEditBox(0, TEXT("NWindow.ChatBack"), 1, 128, 0);
    NameEdit->ID = NID_GM_NAME;
    NameEdit->SetPosition(35, 55, 112, 14);
    CreateChild(NameEdit, NWS_VISIBLE);

    NewButton1 = CreateGMButton(this, NID_GM_NEW1, 10, 75, 899);
    NewButton8 = CreateGMButton(this, NID_GM_NEW8, 74, 75, 900);
    NewButton7 = CreateGMButton(this, NID_GM_NEW7, 138, 75, 688);
    StatusButton = CreateGMButton(this, NID_GM_STATUS, 10, 96, 433);
    InventoryButton = CreateGMButton(this, NID_GM_INVENTORY, 74, 96, 138);
    SkillsButton = CreateGMButton(this, NID_GM_SKILLS, 138, 96, 119);
    QuestButton = CreateGMButton(this, NID_GM_QUEST, 10, 117, 118);
    NewButton3 = CreateGMButton(this, NID_GM_NEW3, 74, 117, 901);
    WarehouseButton = CreateGMButton(this, NID_GM_WAREHOUSE, 138, 117, 131);
    ClanButton = CreateGMButton(this, NID_GM_CLAN, 10, 138, 439);
    NewButton2 = CreateGMButton(this, NID_GM_NEW2, 74, 138, 902);
    NewButton9 = CreateGMButton(this, NID_GM_NEW9, 138, 138, 903);
    NewButton4 = CreateGMButton(this, NID_GM_NEW4, 10, 159, 690);
    NewButton5 = CreateGMButton(this, NID_GM_NEW5, 74, 159, 691);
    NewButton6 = CreateGMButton(this, NID_GM_NEW6, 138, 159, 692);
    ForcePetiButton = CreateGMButton(this, NID_GM_FORCEPETI, 10, 180, 1274);
    ChangeServerButton = CreateGMButton(this, NID_GM_CHANGESERVER, 74, 180, 1275);
    return 1;
}
NW_HOOK_METHOD(0x1008dac0, NCGMWnd, OnCreate)

// 0x1008d0c0 (vslot 38) - the name's box and the buttons go (the back picture and the title bar are left alone)
INT NCGMWnd::OnDestroy()
{
    guard(NCGMWnd::OnDestroy);
    DestroyChild(this, NameEdit);
    DestroyChild(this, InventoryButton);
    DestroyChild(this, SkillsButton);
    DestroyChild(this, StatusButton);
    DestroyChild(this, QuestButton);
    DestroyChild(this, WarehouseButton);
    DestroyChild(this, ClanButton);
    DestroyChild(this, ForcePetiButton);
    DestroyChild(this, NewButton1);
    DestroyChild(this, NewButton7);
    DestroyChild(this, NewButton4);
    DestroyChild(this, NewButton5);
    DestroyChild(this, NewButton6);
    DestroyChild(this, NewButton2);
    DestroyChild(this, NewButton8);
    DestroyChild(this, NewButton3);
    DestroyChild(this, NewButton9);
    DestroyChild(this, ChangeServerButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008d0c0, NCGMWnd, OnDestroy)

// 0x1008d450 (vslot 62) - the back picture under the title bar (the texture is not checked) and the "Name" label
INT NCGMWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BottomTex, 0xFF, 1);
    Device->DrawText(7, 55, 0xFFDCDCDC, *GL2GameData.SysString[STR_Name], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x1008d450, NCGMWnd, OnPaint)

// the builder command of a button: the text it adds to the name typed (the stock repeats it for each button)
static FORCEINLINE void BuilderWithName(const TCHAR* Prefix, const TCHAR* Name)
{
    FString Command(Prefix);
    Command += Name;
    L2ParamStack Params(10);
    Params.PushBack((void*)*Command);
    ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand: slot 194 of the C4 console is not declared (the header's method is a plain one)
}

// 0x10089980 (vslot 81) - the player named in the box: the builder command "teleportto <name>"
INT NCGMWnd::OnNewButton1()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
        BuilderWithName(TEXT("teleportto "), Name);
    return 1;
}
NW_HOOK_METHOD(0x10089980, NCGMWnd, OnNewButton1)

// 0x10089ae0 (slot 88, name ours) - "add_peti_chat <name>"
INT NCGMWnd::OnNewButton2()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
        BuilderWithName(TEXT("add_peti_chat "), Name);
    return 1;
}
NW_HOOK_METHOD(0x10089ae0, NCGMWnd, OnNewButton2)

// 0x10089ba0 (slot 89, name ours) - "debug <name>"
INT NCGMWnd::OnNewButton3()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
        BuilderWithName(TEXT("debug "), Name);
    return 1;
}
NW_HOOK_METHOD(0x10089ba0, NCGMWnd, OnNewButton3)

// 0x10089a40 (slot 86, name ours) - the builder command "instant_move" (no name)
INT NCGMWnd::OnNewButton8()
{
    FString Command(TEXT("instant_move"));
    L2ParamStack Params(10);
    Params.PushBack((void*)*Command);
    ::nw::VirtualCall<void>(GConsole, 194, &Params);
    return 1;
}
NW_HOOK_METHOD(0x10089a40, NCGMWnd, OnNewButton8)

// 0x10089c60 (slot 90; the db's name) - C4: the number typed is a server's; the console is told to go there, at the
// position of the GM's pawn (the GameGuard check of the lobby is switched off first)
INT NCGMWnd::OnGMChangeServerBtnClick()
{
    guard(NCGMWnd::OnGMChangeServerBtnClick);
    const TCHAR* Text = NameEdit->CommitText();
    INT Server;
    if (Text)
        Server = appAtoi(Text);
    else
        Server = 0;
    GL2DisableGCLobby = 1;
    FVector Location = GConsole->GetMyPawn()->Location;
    typedef void(__fastcall * ChangeServerFn)(void* This, void* Edx, INT InServer, FVector InLocation);   // ret 0x10
    ChangeServerFn ChangeServer = (*(ChangeServerFn**)GConsole)[410];
    ChangeServer(GConsole, 0, Server, Location);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10089c60, NCGMWnd, OnGMChangeServerBtnClick)

// a question about the name typed (the two buttons below): sysmsg Msg in an OK-only box of id ID, the name its parameter
static FORCEINLINE void AskAboutName(INT ID, INT Msg, const TCHAR* Name)
{
    NDialogInfo Info;
    Info.ID = ID;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.SetSystemMessage(Msg);
    Info.SetParamText(Name);
    GConsole->ShowDialog(&Info);
}

// 0x1008d530 (slot 76, name ours) - the dialog 0x4B with system message 0x4C4 and the name
INT NCGMWnd::OnNewButton7()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
        AskAboutName(0x4B, 0x4C4, Name);
    return 1;
}
NW_HOOK_METHOD(0x1008d530, NCGMWnd, OnNewButton7)

// 0x1008d5f0 (slot 81, name ours) - the dialog 0x4A with system message 0x4C5 and the name
INT NCGMWnd::OnNewButton9()
{
    const TCHAR* Name = NameEdit->CommitText();
    if (Name)
        AskAboutName(0x4A, 0x4C5, Name);
    return 1;
}
NW_HOOK_METHOD(0x1008d5f0, NCGMWnd, OnNewButton9)

// the match of a name search (the three buttons below): "ClassID:%d Name:%s" in an OK-only dialog box (no owner)
static FORCEINLINE void ShowClassID(INT ClassID, const TCHAR* Name)
{
    TCHAR Text[256];
    appSprintf(Text, TEXT("ClassID:%d Name:%s"), ClassID, Name);
    NDialogInfo Info;
    Info.ID = DIALOG_SystemMessage;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.SetText(Text);
    GConsole->ShowDialog(&Info);
}

// 0x10089d40 (MSNSocket.cpp; UE2's TMapBase::Rehash for the maps of {chain, key, value} pairs) - the number map's (NMsnIntMap::Rehash,
// NMsnProtocol.h: only the call is needed here)

// TMap's iterator as the stock keeps it on the stack (name ours): the map, its pairs, the index and the number of pairs
// removed while walking. The destructor (0x10136330) relaxes the hash and rehashes the map when something was
// removed; the searches below remove nothing, and the stock code runs it on the match path only
struct NSearchMapIterator
{
    INT* Map;            // +0x00 the map: Pairs (+0 data, +4 Num, +8 Max), Hash (+0xC), HashCount (+0x10)
    INT* PairsRef;       // +0x04
    INT Index;           // +0x08
    INT Removed;         // +0x0C

    __declspec(noinline) void Relax();   // 0x10150230: TMapBase::Relax, then the map's Rehash (0x10089D40)
};

// 0x10150230 (50 bytes; anonymous in the db, wave C) - the iterator's destructor body: when pairs were removed, the hash size is halved while
// it is above twice the number of pairs plus 8 (the map's HashCount +0x10, its Pairs.ArrayNum +4), then the map is rehashed (the stock tail-jumps
// into the number map's Rehash 0x10089D40, the one NMsnIntMap uses: the pairs are {chain, key, value}). Nothing removed: nothing
void NSearchMapIterator::Relax()
{
    if (Removed)
    {
        INT* M = Map;
        while (M[4] > M[1] * 2 + 8)   // HashCount > Pairs.Num() * 2 + 8
            M[4] /= 2;
        ((NMsnIntMap*)M)->Rehash();   // 0x10089D40
    }
}
NW_HOOK_METHOD(0x10150230, NSearchMapIterator, Relax)

// 0x1008eeb0 (vslot 77, name ours) - the first NPC of the NPC table (GL2GameData +0x180C, by id) whose name is the one
// typed: its class id (the NPC id plus 1,000,000) and name in a dialog box. The records are the table's walk by TMap's
// iterator, from the start; none found: nothing
INT NCGMWnd::OnNewButton4()
{
    const TCHAR* Search = NameEdit->CommitText();
    if (Search)
    {
        NDataMap<NViewNpcData>* Map = GL2GameData.NpcTable;
        NSearchMapIterator It = { (INT*)Map, (INT*)Map, 0, 0 };
        for (; It.Index < Map->Pairs.Num(); It.Index++)
        {
            NViewNpcData* Npc = Map->Pairs(It.Index).Value;
            if (Npc && *Npc->Name && !appStrcmp(*Npc->Name, Search))
            {
                ShowClassID(Npc->ID + 1000000, *Npc->Name);
                It.Relax();
                return 1;
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1008eeb0, NCGMWnd, OnNewButton4)

// 0x1008f020 (vslot 84, name ours) - the same for the item table (+0x1818): the item's id (+0xC) and name (an FString at
// +0x50 in C4, NViewItemData has it at +0x40: an L2FName)
INT NCGMWnd::OnNewButton5()
{
    const TCHAR* Search = NameEdit->CommitText();
    if (Search)
    {
        NDataMap<NViewItemData>* Map = GL2GameData.ItemTable;
        NSearchMapIterator It = { (INT*)Map, (INT*)Map, 0, 0 };
        for (; It.Index < Map->Pairs.Num(); It.Index++)
        {
            NViewItemData* Item = Map->Pairs(It.Index).Value;
            if (Item)
            {
                L2FName& Name = ::nw::Field<L2FName>(Item, 0x50);
                if (*Name && !appStrcmp(*Name, Search))
                {
                    ShowClassID(Item->ItemID, *Name);
                    It.Relax();
                    return 1;
                }
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1008f020, NCGMWnd, OnNewButton5)

// 0x1008f180 (vslot 79, name ours) - the same for the skill table (+0x3CDC: one record per level, the first level with the
// name is found). C3: the record starts with the name (an FString at +0) and the skill id is at +0x18
INT NCGMWnd::OnNewButton6()
{
    const TCHAR* Search = NameEdit->CommitText();
    if (Search)
    {
        NDataMap<FL2MagicSkillData>* Map = GL2GameData.SkillTable;
        NSearchMapIterator It = { (INT*)Map, (INT*)Map, 0, 0 };
        for (; It.Index < Map->Pairs.Num(); It.Index++)
        {
            FL2MagicSkillData* Skill = Map->Pairs(It.Index).Value;
            if (Skill)
            {
                FString& Name = *(FString*)Skill;
                if (*Name && !appStrcmp(*Name, Search))
                {
                    ShowClassID(*(INT*)((BYTE*)Skill + 0x18), *Name);
                    It.Relax();
                    return 1;
                }
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1008f180, NCGMWnd, OnNewButton6)

NW_CHECK_VTABLE(NCGMWnd, 0x101b58b8, 91)

/*-----------------------------------------------------------------------------
    NCGMInvenItemWnd, NCGMEquipItemWnd
-----------------------------------------------------------------------------*/

// 0x1008f6f0 - NCInvenItemWnd's default constructor (0x10099790, defined in NCInvenItemWnd.cpp: called, not inlined), then this
// class' vtable
NCGMInvenItemWnd::NCGMInvenItemWnd()
{
}
NW_HOOK_CTOR(0x1008f6f0, NCGMInvenItemWnd)

// 0x1008f790 (sub_10084480, ret 0x10) - NCInvenItemWnd's six-argument constructor with the gaps 1 and 1
NCGMInvenItemWnd::NCGMInvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize)
    : NCInvenItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, 1, 1)
{
}
NW_HOOK_CTOR_ARGS(0x1008f790, NCGMInvenItemWnd, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x1008f740
NW_HOOK_DTOR(0x1008f740, NCGMInvenItemWnd)

// 0x10089070 (vslot 25) - the base's answer as a plain 0 / 1
INT NCGMInvenItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCInvenItemWnd::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10089070, NCGMInvenItemWnd, WindowProc)

// 0x10089090 (vslots 69, 70, 81) - NCGMSkillItemWnd::OnLButtonUp's body, shared (hooked there): the carried item is only put down (the flag at +0x138)
INT NCGMInvenItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}
INT NCGMInvenItemWnd::OnRButtonDown(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}
INT NCGMInvenItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}

NW_CHECK_VTABLE(NCGMInvenItemWnd, 0x101b4a60, 82)

// 0x1008f640 (ret 4) - NCEquipItemWnd's, then the two flags of the earring and ring slots (+0x2A4, +0x2A5) cleared and
// this class' vtable
NCGMEquipItemWnd::NCGMEquipItemWnd(INT InCellSize)
    : NCEquipItemWnd(InCellSize)
{
    ::nw::Field<BYTE>(this, 0x2A4) = 0;
    ::nw::Field<BYTE>(this, 0x2A5) = 0;
}
NW_HOOK_CTOR_ARGS(0x1008f640, NCGMEquipItemWnd, (INT A), (A))

// 0x1008f6a0
NW_HOOK_DTOR(0x1008f6a0, NCGMEquipItemWnd)

// 0x10088f30 (vslot 25) - the base's answer as a plain 0 / 1
INT NCGMEquipItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCEquipItemWnd::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10088f30, NCGMEquipItemWnd, WindowProc)

// 0x10088f50 (vslot 69) - the carried item is only put down; also slots 64 and 69
INT NCGMEquipItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x10088f50, NCGMEquipItemWnd, OnLButtonUp)

// 0x10088f50 (vslots 70 and 75) - the same body, shared with OnLButtonUp
INT NCGMEquipItemWnd::OnRButtonDown(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}
INT NCGMEquipItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    return 1;
}

// 0x10088f70 (vslot 74) - the slot by the body part bits alone (the console is not asked): the first four bits are
// tested in turn (the ear, the other ear, the finger, the other finger), the rest by value; -1 for any other
INT NCGMEquipItemWnd::GetEquipSlot(INT BodyPart, INT ObjectID)
{
    if (BodyPart & 2)
        return 8;
    if (BodyPart & 4)
        return 9;
    if (BodyPart & 0x10)
        return 0xD;
    if (BodyPart & 0x20)
        return 0xE;
    switch (BodyPart)
    {
    case 0x1:
        return 0;
    case 0x8:
        return 4;
    case 0x40:
        return 1;
    case 0x80:
        return 5;
    case 0x100:
        return 7;
    case 0x200:
        return 0xA;
    case 0x400:
        return 6;
    case 0x800:
        return 0xB;
    case 0x1000:
        return 0xC;
    case 0x2000:
        return 2;
    case 0x4000:
        return 0x10;
    case 0x8000:
        return 0xF;
    case 0x10000:
        return 3;
    }
    return -1;
}
NW_HOOK_METHOD(0x10088f70, NCGMEquipItemWnd, GetEquipSlot)

// 0x1008d6b0 (ret 4) - NCEquipItemWnd's AddItem; then on the new entry's slot mask: the first of two bits 2 / 4 (0x30: 0x10 /
// 0x20, two earring or ring slots) found keeps its low bit and clears the other, the second one takes the other (the
// two flags remember which was given out, at +0x2A4 and +0x2A5 of the C4 window). C4 looks up five icons by name (the
// record has two names more than C3's) and the entry is the C4 one (NItemEntryC4.h)
UBOOL NCGMEquipItemWnd::AddItem(NItemInfo* Info)
{
    if (Items.Num() >= NumSlots)
        return 0;
    if (InfoIconNameN(Info, 0)[0])
        InfoIconN(Info, 0) = FindTexture(InfoIconNameN(Info, 0), 1);
    if (InfoIconNameN(Info, 1)[0])
        InfoIconN(Info, 1) = FindTexture(InfoIconNameN(Info, 1), 1);
    if (InfoIconNameN(Info, 2)[0])
        InfoIconN(Info, 2) = FindTexture(InfoIconNameN(Info, 2), 1);
    if (InfoIconNameN(Info, 3)[0])
        InfoIconN(Info, 3) = FindTexture(InfoIconNameN(Info, 3), 1);
    if (InfoIconNameN(Info, 4)[0])
        InfoIconN(Info, 4) = FindTexture(InfoIconNameN(Info, 4), 1);
    if (!InfoIconN(Info, 0))
        InfoIconN(Info, 0) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    if (!InfoIconN(Info, 1))
        InfoIconN(Info, 1) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    if (!InfoIconN(Info, 2))
        InfoIconN(Info, 2) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    if (!InfoIconN(Info, 3))
        InfoIconN(Info, 3) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    if (!InfoIconN(Info, 4))
        InfoIconN(Info, 4) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    NItemEntry* Item = NewItemEntry(Info);
    if (EntryBodyPart(Item) & 6)
    {
        if (::nw::Field<BYTE>(this, 0x2A4) == 0)
        {
            EntryBodyPart(Item) &= ~2;
            ::nw::Field<BYTE>(this, 0x2A4) = 1;
        }
        else
            EntryBodyPart(Item) &= ~4;
    }
    if (EntryBodyPart(Item) & 0x30)
    {
        if (::nw::Field<BYTE>(this, 0x2A5) == 0)
        {
            EntryBodyPart(Item) &= ~0x10;
            ::nw::Field<BYTE>(this, 0x2A5) = 1;
        }
        else
            EntryBodyPart(Item) &= ~0x20;
    }
    EntryToolTip(Item)->Fill(Item, 2, 0);   // NToolTipInfo::Fill (flags 2)
    Items.AddItem(Item);
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (EntryFlag1BA0(Items(i)))
        {
            bHasUnknown1C98 = 1;
            break;
        }
        bHasUnknown1C98 = 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x1008d6b0, NCGMEquipItemWnd, AddItem)

// the frame of an item of the paperdoll at (X, Y): the selection's, or the two outlines (the under one first)
static FORCEINLINE void DrawEquipFrame(NCGMEquipItemWnd* Wnd, NCGDevice* Device, NItemEntry* Item, INT X, INT Y)
{
    if (Item->IsSelected())
        Device->DrawTexture(X, Y, 0x22, 0x22, 0, 0, 0x22, 0x22, Wnd->SelectTex, 0xFF, 1);
    else
    {
        Device->DrawTexture(X, Y, 0x22, 0x22, 0, 0, 0x22, 0x22, Wnd->OutlineDownTex, 0xFF, 1);
        Device->DrawTexture(X, Y, 0x23, 0x23, 0, 0, 0x23, 0x23, Wnd->OutlineTex, 0xFF, 1);
    }
}

// 0x10089dd0 (vslot 62) - NCEquipItemWnd's paint as code of its own (same guard name): the icons in their slots (the
// console is not asked which slot: GetEquipSlot, the two-slot items in both slots 15 / 16, the second under the "dual" cap, and
// in C4 the four-slot items of slot 17 with their four icons; a bow in the right hand alone while an item with the flag at
// +0x1BA0 is equipped), the frames, and the tattoo boxes for three slots whatever the character may have
INT NCGMEquipItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCEquipItemWnd::OnPaint);
    Device->SetClip(ScreenX - 1, ScreenY - 1, (INT)(Width + 2.0f), (INT)(Height + 2.0f));
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 0;
    INT Y = 0;
    for (INT i = 0; i < Items.Num(); i++)
    {
        NItemEntry* Item = Items(i);
        if (!Item)
            continue;
        INT Slot = GetEquipSlot(EntryBodyPart(Item), EntryObjectID(Item));
        UBOOL bMulti = 0;
        switch (Slot)
        {
        case 0:  X = 7;    Y = 7;    break;
        case 1:  X = 0x2E; Y = 7;    break;
        case 2:  X = 0x55; Y = 7;    break;
        case 3:  X = 0x82; Y = 7;    break;
        case 4:  X = 0xA9; Y = 7;    break;
        case 5:  X = 7;    Y = 0x2D; break;
        case 6:  X = 0x2E; Y = 0x2D; break;
        case 7:  X = 0x55; Y = 0x2D; break;
        case 8:  X = 0x82; Y = 0x2D; break;
        case 9:  X = 0xA9; Y = 0x2D; break;
        case 10: X = 7;    Y = 0x53; break;
        case 11: X = 0x2E; Y = 0x53; break;
        case 12: X = 0x55; Y = 0x53; break;
        case 13: X = 0x82; Y = 0x53; break;
        case 14: X = 0xA9; Y = 0x53; break;
        case 15:
        case 16:
        case 17:
            bMulti = 1;
            break;
        default:
            continue;
        }
        if (EntryInt1B3C(Item) == 6 && bHasUnknown1C98)
        {
            X = 7;
            Y = 0x2D;
            bMulti = 0;
        }
        if (!bMulti)
        {
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, EntryIconN(Item, 0), 0xFF, 1);
            DrawEquipFrame(this, Device, Item, X - 1, Y - 1);
            continue;
        }
        // a multi-slot item: the icons are the item's own, or the second to the fifth when the names are there; the
        // missing ones take the one before (the first when it has none: all of them the item's own icon)
        UTexture* First = NULL;
        UTexture* Second = NULL;
        UTexture* Third = NULL;
        UTexture* Fourth = NULL;
        if (EntryIconNameN(Item, 1)[0] != 0)
            First = EntryIconN(Item, 1);
        if (EntryIconNameN(Item, 2)[0] != 0)
            Second = EntryIconN(Item, 2);
        if (EntryIconNameN(Item, 3)[0] != 0)
            Third = EntryIconN(Item, 3);
        if (EntryIconNameN(Item, 4)[0] != 0)
            Fourth = EntryIconN(Item, 4);
        if (First == NULL)
        {
            First = EntryIconN(Item, 0);
            Second = First;
            Third = First;
            Fourth = First;
        }
        else if (Second == NULL)
        {
            Second = First;
            Third = First;
            Fourth = First;
        }
        else if (Third == NULL)
        {
            Third = Second;
            Fourth = Second;
        }
        else if (Fourth == NULL)
            Fourth = Third;
        if (Slot == 16)
        {
            Device->DrawTexture(7, 0x2D, CellSize, CellSize, 0, 0, CellSize, CellSize, First, 0xFF, 1);
            Device->DrawTexture(0x55, 0x2D, CellSize, CellSize, 0, 0, CellSize, CellSize, Second, 0xFF, 1);
            Device->DrawTexture(0x55, 0x2D, CellSize, CellSize, 0, 0, CellSize, CellSize, DualCapTex, 0xFF, 1);
            DrawEquipFrame(this, Device, Item, 6, 0x2C);
        }
        else if (Slot == 15 || Slot == 17)
        {
            Device->DrawTexture(0x2E, 0x2D, CellSize, CellSize, 0, 0, CellSize, CellSize, First, 0xFF, 1);
            Device->DrawTexture(0x2E, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, Second, 0xFF, 1);
            if (Slot == 17)
            {
                Device->DrawTexture(0x2E, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, DualCapTex, 0xFF, 1);
                Device->DrawTexture(7, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, Third, 0xFF, 1);
                Device->DrawTexture(7, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, DualCapTex, 0xFF, 1);
                Device->DrawTexture(0x55, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, Fourth, 0xFF, 1);
                Device->DrawTexture(0x55, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, DualCapTex, 0xFF, 1);
            }
            else
                Device->DrawTexture(0x2E, 0x53, CellSize, CellSize, 0, 0, CellSize, CellSize, DualCapTex, 0xFF, 1);
            DrawEquipFrame(this, Device, Item, 0x2D, 0x2C);
        }
    }
    if (HennaBackTex)
    {
        for (INT i = 0; i < 3; i++)
        {
            Device->DrawTexture(0xD7, i * 0x1B + 8, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, HennaBackTex, 0xFF, 1);
            if (i < Hennas.Num() && ::nw::Field<UTexture*>(Hennas(i), 0))
            {
                UTexture* Icon = ::nw::Field<UTexture*>(Hennas(i), 0);
                INT TopY = i * 0x1B + 9;
                Device->DrawStretchedTex(0xD8, TopY, 0x18, 0x18, Icon, 1, 0xFF);
                if (::nw::Field<INT>(Hennas(i), 0x1B40) == 0)
                    Device->DrawTexture(0xD9, TopY, 0x18, 0x18, 0, 0, 0x18, 0x18, FindTexture(TEXT("NWindow.ChatBack"), 1), 0xFF, 1);
            }
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10089dd0, NCGMEquipItemWnd, OnPaint)

NW_CHECK_VTABLE(NCGMEquipItemWnd, 0x101b4930, 76)

/*-----------------------------------------------------------------------------
    NCGMInventoryWnd
-----------------------------------------------------------------------------*/

// 0x1008f7c0 - NCWnd's constructor, four fields cleared
NCGMInventoryWnd::NCGMInventoryWnd()
{
    BackTex = NULL;
    TabCtrl = NULL;
    ItemWnd[0] = NULL;
    ItemWnd[1] = NULL;
}
NW_HOOK_CTOR(0x1008f7c0, NCGMInventoryWnd)

// 0x1008f820
NW_HOOK_DTOR(0x1008f820, NCGMInventoryWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCGMInventoryWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100890b0 - the paperdoll and the two pages emptied, the two ring flags cleared
void NCGMInventoryWnd::ClearItems()
{
    EquipWnd->Clear();
    ItemWnd[0]->Clear();
    ItemWnd[1]->Clear();
    EquipWnd->bFirstRingTaken = 0;
    EquipWnd->bFirstFingerTaken = 0;
}
NW_HOOK_METHOD(0x100890b0, NCGMInventoryWnd, ClearItems)

// 0x10089100 (vslot 48)
void NCGMInventoryWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
}
NW_HOOK_METHOD(0x10089100, NCGMInventoryWnd, ShowWindow)

// 0x1008a830 (vslot 38)
INT NCGMInventoryWnd::OnDestroy()
{
    guard(NCGMInventoryWnd::OnDestroy);
    DestroyChild(this, ItemWnd[0]);
    DestroyChild(this, ItemWnd[1]);
    DestroyChild(this, EquipWnd);
    DestroyChild(this, TabCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008a830, NCGMInventoryWnd, OnDestroy)

// 0x1008f870 (vslot 37) - the title bar, the paperdoll (32-pixel cells), the two pages (6 x 4 icons of 32 pixels, 160
// items at most) and their tabs (C4's AddTab takes one argument more)
INT NCGMInventoryWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.Inventory_Back"), 1);
    AdenaTex = FindTexture(TEXT("L2UI.WindowIcon.WindowIconAdena"), 1);
    WeightTex = FindTexture(TEXT("L2UI.WindowIcon.Weight"), 1);

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMINVENTORY_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    EquipWnd = new NCGMEquipItemWnd(32);
    EquipWnd->SetPosition(8, 31, 240, 122);
    CreateChild(EquipWnd, NWS_VISIBLE | 0x20);

    ItemWnd[0] = new NCGMInvenItemWnd(6, 4, 160, 32);
    ItemWnd[0]->SetPosition(9, 189, 236, 139);
    CreateChild(ItemWnd[0], NWS_VISIBLE | 0x20);

    ItemWnd[1] = new NCGMInvenItemWnd(6, 4, 160, 32);
    ItemWnd[1]->SetPosition(9, 189, 236, 139);
    CreateChild(ItemWnd[1], NWS_VISIBLE | 0x20);

    TabCtrl = new NCTabControl;
    TabCtrl->ID = NID_GMINVENTORY_TABS;
    TabCtrl->SetPosition(12, 159, 189, 23);
    CreateChild(TabCtrl, NWS_VISIBLE);
    // NCTabControl::AddTab (0x1002DED0) takes a ninth argument (0) in this client
    TabCtrl->AddTab(STR_Item, ItemWnd[0], 94, 23, TEXT("L2UI_CH3.InventoryWnd.Inventory_tab2"),
                    TEXT("L2UI_CH3.InventoryWnd.Inventory_tab1"), 1, -1, 0);
    TabCtrl->AddTab(STR_Quest, ItemWnd[1], 94, 23, TEXT("L2UI_CH3.InventoryWnd.Inventory_tab2"),
                    TEXT("L2UI_CH3.InventoryWnd.Inventory_tab1"), 1, -1, 0);
    return 1;
}
NW_HOOK_METHOD(0x1008f870, NCGMInventoryWnd, OnCreate)

// 0x1008d950 (ret 4) - an item that is worn (NItemInfo +0x19A0) goes to the paperdoll, any other into the page of its kind
// (the quest items, Param 3, in the second)
UBOOL NCGMInventoryWnd::AddItem(NItemInfo* Info)
{
    INT Page = 0;
    if (InfoParam(Info) == 3)
        Page = 1;
    if (InfoWorn(Info) != 0)
    {
        EquipWnd->AddItem(Info);
        return 1;
    }
    if (ItemWnd[Page])
        ItemWnd[Page]->AddItem(Info);
    return 1;
}
NW_HOOK_METHOD(0x1008d950, NCGMInventoryWnd, AddItem)

// 0x1008a5a0 (vslot 62) - the background under the title bar, the adena (right-aligned at x 199) and the two icons, and
// the count of the items held, "(held/most)", right-aligned 5 pixels from the window's edge (C4 only; the most is the
// window's word at +0xF4, which the console sets)
INT NCGMInventoryWnd::OnPaint(NCGDevice* Device)
{
    guard(NCGMInventoryWnd::OnPaint);
    TCHAR Text[128];
    TCHAR Formatted[128];
    TCHAR Count[68];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0x14, 0x100, 0x17D, 0, 0, 0x100, 0x17D, BackTex, 0xFF, 1);
    appSprintf(Text, TEXT("%d"), ItemWnd[0]->GetAdena());
    FormatNumber(Formatted, Text);
    appSprintf(Text, TEXT("%s"), Formatted);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText(0xC7 - Size.cx, 0x164, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (AdenaTex)
        Device->DrawTexture(0x62, 0x163, 0x10, 0xC, 0, 0, 0x10, 0xC, AdenaTex, 0xFF, 1);
    if (WeightTex)
        Device->DrawTexture(0x62, 0x174, 0x10, 0xC, 0, 0, 0x10, 0xC, WeightTex, 0xFF, 1);
    appSprintf(Count, TEXT("(%d/%d)"), EquipWnd->Items.Num() + ItemWnd[1]->Items.Num() + ItemWnd[0]->Items.Num(),
               ::nw::Field<INT>(this, 0xF4));
    NCGDevice::GetTextExtent(&Size, Count, 0, 0, 0, 0, 0);
    Device->DrawText((INT)(Width - Size.cx - 5.0f), 0xA2, 0xFFB09B79, Count, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008a5a0, NCGMInventoryWnd, OnPaint)

// 0x10089120 (name ours) - the console's slot 143 (GMAddHennaInfo): the paperdoll's tattoo (a tail jump to NCEquipItemWnd::AddHenna)
void NCGMInventoryWnd::AddHenna(INT HennaID, INT bUsable)
{
    EquipWnd->AddHenna(HennaID, bUsable);
}
NW_HOOK_METHOD(0x10089120, NCGMInventoryWnd, AddHenna)

// 0x10089130 (name ours) - the console's slot 144 (GMReceiveHennaInfo): the paperdoll's tattoos gone (a tail jump to NCEquipItemWnd::ClearHennas)
void NCGMInventoryWnd::ClearHennas()
{
    EquipWnd->ClearHennas();
}
NW_HOOK_METHOD(0x10089130, NCGMInventoryWnd, ClearHennas)

NW_CHECK_VTABLE(NCGMInventoryWnd, 0x101b4ba8, 74)

/*-----------------------------------------------------------------------------
    NCGMQuestWnd (C3: NCQuestGaraTreeWnd's copy, with a list of item counts at +0x180: the constructor and the
    destructor are the base's with that list; ShowWindow, the tool tip, the mouse move and the two buttons are the base's)
-----------------------------------------------------------------------------*/

enum
{
    STR_QuestTitle = 324,           // the title line (GL2GameData + 0x669BC)
    STR_QuestInfo1 = 861,           // .. 865: the five labels of the quest's info lines (+ 0x682E8 .. 0x68318)
};

// 0x10030670 (a method of NToolTipInfo, ret 0xC; the quest tree windows' OnCreate is its other caller) -
// the tip made of lines "label" with an icon each: emptied, set to the Style given (0x1000: the title box only, with
// the lines under it), a line per text (the icon of the same number in front) and measured (Layout). Declared on a view
// of NToolTipInfo (its header is not this lane's; NToolTipInfoLinesView is declared once, in NWinBStock.h)
void NToolTipInfoLinesView::SetIconLines(TArray<FString*>* Texts, TArray<UTexture*>* Icons, DWORD InStyle)
{
    Clear();
    Style = InStyle;
    for (INT i = 0; i < Texts->Num(); i++)
    {
        NToolTipLine Line;
        appStrcpy(Line.Label, **(*Texts)(i));
        Line.Icon = (*Icons)(i);
        INT Index = Lines.Add(1);
        Lines(Index) = Line;
    }
    Layout();
}
NW_HOOK_METHOD(0x10030670, NToolTipInfoLinesView, SetIconLines)

// 0x10090300 - the quest tree window's constructor, the vtable and the three words of Counts cleared
NCGMQuestWnd::NCGMQuestWnd()
{
    ::nw::Field<INT>(this, 0x1A4) = 0;
    ::nw::Field<INT>(this, 0x1A8) = 0;
    ::nw::Field<INT>(this, 0x1AC) = 0;
}
NW_HOOK_CTOR(0x10090300, NCGMQuestWnd)

// 0x10090360 - the vtable of this class, ConditionalDestroy, Counts (the stock calls the shared TArray<8-byte element> destructor
// 0x1008E930: Remove(0, Num, 8) and ~FArray) and the base's destructor (NCQuestGaraTreeWnd's, 0x1008F590)
NW_HOOK_DTOR(0x10090360, NCGMQuestWnd)


// 0x100891f0 (vslot 25) - NCQuestGaraTreeWnd's (0x100da270) answer as a plain 0 / 1
INT NCGMQuestWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCQuestGaraTreeWnd::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x100891f0, NCGMQuestWnd, WindowProc)

// 0x100eb490 (vslot 38) - NCSSQMainEventWnd's body, shared (hooked there): the control at +0xCC goes, then NCWnd's. The
// GM window made no button or check box of the base's
INT NCGMQuestWnd::OnDestroy()
{
    DestroyChild(this, TreeCtrl);
    NCWnd::OnDestroy();
    return 1;
}

// 0x1008e9c0 (vslot 37) - NCWnd's, the title bar (256 x 20), the background and the tool tip button's textures, the
// tree (242 x 274 at (7, 47)), and the tip of the five info lines of a quest (the labels are system strings 861 ..
// 865, the icons "QuestWndInfoIcon_1" .. "_5"); the lists made for it are emptied again and the counts are cleared
INT NCGMQuestWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMQUEST_FRAME;
    FrameCtrl->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndBack"), 1);
    ToolTipBtnTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndToolTipBtn"), 1);
    TreeCtrl = new NCQuestGaraTreeCtrl();
    TreeCtrl->SetPosition(7, 0x2F, 0xF2, 0x112);
    CreateChild(TreeCtrl, NWS_VISIBLE);

    TArray<FString*> Texts;
    TArray<UTexture*> Icons;
    for (INT i = 0; i < 5; i++)
    {
        FString* Text = new FString(*GL2GameData.SysString[STR_QuestInfo1 + i]);
        Texts(Texts.Add(1)) = Text;
    }
    static const TCHAR* const IconNames[5] =
    {
        TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_1"), TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_2"),
        TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_3"), TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_4"),
        TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_5"),
    };
    for (INT i = 0; i < 5; i++)
    {
        UTexture* Icon = FindTexture(IconNames[i], 1);
        Icons(Icons.Add(1)) = Icon;
    }
    ((NToolTipInfoLinesView*)&ToolTip)->SetIconLines(&Texts, &Icons, 0x1000);
    for (INT i = 0; i < Texts.Num(); i++)
    {
        Texts(i)->Empty();
        delete Texts(i);
    }
    Texts.Empty();
    Icons.Empty();
    Counts.Empty();
    return 1;
}
NW_HOOK_METHOD(0x1008e9c0, NCGMQuestWnd, OnCreate)

// 0x1008c990 (vslot 62, guard: NCGMQuestWnd::OnPaint) - the background (256 x 335 at y 20), the title (system string 324)
// and "(N/15)", the tool tip button; the marked quest's required items: the amount held of each (+0x1E1C) is the sum
// of the entries of Counts with its item id (+0x1D54). The tree's quest records are C4's (everything +0x20 from C3)
INT NCGMQuestWnd::OnPaint(NCGDevice* Device)
{
    guard(NCGMQuestWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, 0x100, 0x14F, 0, 0, 0x100, 0x14F, BackTex, 0xFF, 1);
    Device->DrawText(0xB, 0x20, 0xFFDCDCDC, *GL2GameData.SysString[STR_QuestTitle], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    TCHAR Buffer[32];
    appSprintf(Buffer, TEXT("(%d/%d)"), NumQuests, 15);
    Device->DrawText(0xBE, 0x20, 0xFFB09B79, Buffer, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawTexture(0xE8, 0x1E, 0xF, 0xF, 0, 0, 0xF, 0xF, ToolTipBtnTex, 0xFF, 1);

    BYTE* Category = ::nw::Field<BYTE*>(::nw::Field<BYTE*>(TreeCtrl, 0x134), 0x224);
    if (Category)
    {
        INT Num = ::nw::Field<INT>(Category, 0x20AC);
        if (Num > 0)
        {
            BYTE* Quest = ::nw::Field<BYTE**>(Category, 0x20A8)[Num - 1];
            if (Quest)
            {
                for (INT i = 0; i < ::nw::Field<INT>(Quest, 0x1A2C); i++)
                {
                    INT* Held = &::nw::Field<INT>(Quest, 0x1E1C + i * 4);
                    *Held = 0;
                    for (INT j = 0; j < Counts.Num(); j++)
                    {
                        if (::nw::Field<INT>(Quest, 0x1D54 + i * 4) == Counts(j).ItemID)
                            *Held += Counts(j).Count;
                    }
                }
            }
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008c990, NCGMQuestWnd, OnPaint)

// 0x1008da80 (vslot 68, no argument) - the tree control's slot 72 (its list emptied), the counts and the number of
// quests cleared
void NCGMQuestWnd::ClearList()
{
    TreeCtrl->ResetScroll();
    Counts.Empty();
    NumQuests = 0;
}
NW_HOOK_METHOD(0x1008da80, NCGMQuestWnd, ClearList)

NW_CHECK_VTABLE(NCGMQuestWnd, 0x101b53b0, 77)

/*-----------------------------------------------------------------------------
    NCGMPledgeWnd
-----------------------------------------------------------------------------*/

// 0x10090460 - NCPledgeWnd's constructor (0x100508D0, defined in NCPledgeWnd.cpp: called, not inlined), then this class' vtable
NCGMPledgeWnd::NCGMPledgeWnd()
{
}
NW_HOOK_CTOR(0x10090460, NCGMPledgeWnd)

// 0x100904b0
NW_HOOK_DTOR(0x100904b0, NCGMPledgeWnd)

// 0x10089280 (vslot 25, ret 0xC) - the clan page's answer as a plain 0 / 1
INT NCGMPledgeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCPledgeWnd::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10089280, NCGMPledgeWnd, WindowProc)

// 0x100892a0 (vslot 38) - the clan page's OnDestroy, guarded
INT NCGMPledgeWnd::OnDestroy()
{
    guard(NCGMPledgeWnd::OnDestroy);
    NCPledgeWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100892a0, NCGMPledgeWnd, OnDestroy)

// 0x1008cc50 (vslot 62) - the player's page as the C2 client drew it: no crest, the clan's name (when it has one), the
// leader, the status text after the leader's name, the level, the castle and the hideout as the two texts of the stock
// sysstring (no ids: the GM's record has none). The record is the C4 page's: the name at +0xF0, the leader's at +0x2F8, the
// level at +0x4F8, the castle and the hideout flags at +0x4FC and +0x500, the status text at +0x50C (the header of the
// clan page is not this lane's and has the C3 ones)
INT NCGMPledgeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCGMPledgeWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 256, 335, 0, 0, 256, 335, BackTex, 0xFF, 1);

    SIZE Size;
    const TCHAR* ClanName = (const TCHAR*)((BYTE*)this + 0xF0);
    if (ClanName && *ClanName)
    {
        NCGDevice::GetTextExtent(&Size, ClanName, 0, 0, 0, 0, 0);  // not used
        Device->DrawText(19, 10, CLAN_TEXT_COLOR, ClanName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }

    if (*GL2GameData.SysString[STR_Leader])
        Device->DrawText(19, 26, CLAN_LABEL_COLOR, *GL2GameData.SysString[STR_Leader], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    const TCHAR* Leader = (const TCHAR*)((BYTE*)this + 0x2F8);
    if (Leader && *Leader)
        Device->DrawText(61, 26, CLAN_TEXT_COLOR, Leader, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    const TCHAR* StatusText = (const TCHAR*)((BYTE*)this + 0x50C);
    if (StatusText && *StatusText)
    {
        NCGDevice::GetTextExtent(&Size, Leader, 0, 0, 0, 0, 0);
        Device->DrawText(Size.cx + 49, 26, CLAN_TEXT_COLOR, StatusText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    if (*GL2GameData.SysString[STR_Lv])
        Device->DrawText(192, 26, CLAN_LABEL_COLOR, *GL2GameData.SysString[STR_Lv], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0);
    TCHAR Number[10];
    appSprintf(Number, TEXT("%d"), ::nw::Field<INT>(this, 0x4F8));
    Device->DrawText(208, 26, CLAN_TEXT_COLOR, Number, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);

    if (*GL2GameData.SysString[STR_Base])
        Device->DrawText(19, 42, CLAN_LABEL_COLOR, *GL2GameData.SysString[STR_Base], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0);
    if (::nw::Field<INT>(this, 0x4FC))
    {
        Device->DrawText(61, 42, CLAN_TEXT_COLOR, *GL2GameData.SysString[STR_CastleExists], 0, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0);
        if (::nw::Field<INT>(this, 0x500))
        {
            NCGDevice::GetTextExtent(&Size, *GL2GameData.SysString[STR_CastleExists], 0, 0, 0, 0, 0);
            Device->DrawText(Size.cx + 63, 42, CLAN_TEXT_COLOR, *GL2GameData.SysString[STR_HideoutExists], 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
    }
    else if (::nw::Field<INT>(this, 0x500))
        Device->DrawText(61, 42, CLAN_TEXT_COLOR, *GL2GameData.SysString[STR_HideoutExists], 0, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008cc50, NCGMPledgeWnd, OnPaint)

NW_CHECK_VTABLE(NCGMPledgeWnd, 0x101b5628, 90)

/*-----------------------------------------------------------------------------
    NCGMActionWnd
-----------------------------------------------------------------------------*/

// a button of the clan page at its C4 offset (NCPledgeWnd.h is not this lane's and has the C3 ones)
static FORCEINLINE NCButton* PledgeButton(NCPledgeWnd* Pledge, DWORD Offset)
{
    return ::nw::Field<NCButton*>(Pledge, Offset);
}

// the clan page's buttons the GM window disables (repeated in OnCreate and ShowWindow), in the order of the stock code:
// the title, the crest, the war, the truce, the surrender, the expel, the yield and the leave buttons (C4 has no more
// of the C3 ones the page lost; ShowWindow disables two more, the invite and the first new one)
static FORCEINLINE void DisablePledgeButtons(NCPledgeWnd* Pledge)
{
    PledgeButton(Pledge, 0x730)->EnableWindow(0);
    PledgeButton(Pledge, 0x740)->EnableWindow(0);
    PledgeButton(Pledge, 0x738)->EnableWindow(0);
    PledgeButton(Pledge, 0x73C)->EnableWindow(0);
    PledgeButton(Pledge, 0x744)->EnableWindow(0);
    PledgeButton(Pledge, 0x748)->EnableWindow(0);
    PledgeButton(Pledge, 0x72C)->EnableWindow(0);
    PledgeButton(Pledge, 0x750)->EnableWindow(0);
}

// 0x10090500 - NCWnd's constructor and the vtable
NCGMActionWnd::NCGMActionWnd()
{
}
NW_HOOK_CTOR(0x10090500, NCGMActionWnd)

// 0x10090550
NW_HOOK_DTOR(0x10090550, NCGMActionWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCGMActionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100905a0 (vslot 37) - the title bar (20 high) and the clan page under it, hidden; its buttons disabled but the last
// new one (the privileges' viewer), which is enabled with the text 883
INT NCGMActionWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMACTION_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    PledgeWnd = new NCGMPledgeWnd;
    PledgeWnd->SetPosition(0, 20, 256, 335);
    CreateChild(PledgeWnd, NWS_VISIBLE);
    PledgeWnd->HideWindow();
    DisablePledgeButtons(PledgeWnd);
    NCButton* Viewer = PledgeButton(PledgeWnd, 0x754);
    Viewer->EnableWindow(1);
    Viewer->SetTextID(883);
    return 1;
}
NW_HOOK_METHOD(0x100905a0, NCGMActionWnd, OnCreate)

// 0x1008d010 (vslot 38)
INT NCGMActionWnd::OnDestroy()
{
    guard(NCGMActionWnd::OnDestroy);
    DestroyChild(this, PledgeWnd);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008d010, NCGMActionWnd, OnDestroy)

// 0x10089320 (vslot 48) - shown, the clan page's buttons are disabled again (the page enables the ones its privileges
// allow; C4: the eight of OnCreate and two more)
void NCGMActionWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    DisablePledgeButtons(PledgeWnd);
    PledgeButton(PledgeWnd, 0x734)->EnableWindow(0);
    PledgeButton(PledgeWnd, 0x74C)->EnableWindow(0);
}
NW_HOOK_METHOD(0x10089320, NCGMActionWnd, ShowWindow)

// 0x100ae1c0 (vslot 62) - draws nothing but the clip and the origin; also NCSkillWnd's and NCGMSkillWnd's
INT NCGMActionWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100ae1c0, NCGMActionWnd, OnPaint)

NW_CHECK_VTABLE(NCGMActionWnd, 0x101b5790, 74)
