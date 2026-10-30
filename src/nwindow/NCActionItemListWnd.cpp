/*=============================================================================
    NCActionItemListWnd.cpp - the Basic page of the action window. Stock: 0x10034d10 ..
    0x10034f4e, 0x10036c70 .. 0x10036db3, 0x10037060 .. 0x1003724c.
=============================================================================*/
#include "nwindow/NCActionItemListWnd.h"
#include "nwindow/NCActionItemWnd.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NItemInfoC4.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleFns.h"   // NConsoleLeftFns::IsDwarf (0x100753d0, NLeftHelpers.cpp): whether the player's class is one of the dwarves

// 0x10050600 - the background texture cleared (the grids are made by OnCreate; C3 never loads the texture)
NCActionItemListWnd::NCActionItemListWnd()
{
    BackTex = NULL;
}
NW_HOOK_CTOR(0x10050600, NCActionItemListWnd)

// 0x10050650
NW_HOOK_DTOR(0x10050650, NCActionItemListWnd)

// stock body shared with NControl's (0x10004e90)
INT NCActionItemListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// one grid of the page (repeated inline in the stock OnCreate): 6 columns of 32 pixel icons, 15 at most, 18 pixels from
// the left, 219 wide
static FORCEINLINE NCActionItemWnd* CreateGrid(NCWnd* Parent, INT Rows, INT Y, INT H)
{
    NCActionItemWnd* Grid = new NCActionItemWnd(6, Rows, 15, 32);
    Grid->SetPosition(18, Y, 219, H);
    Parent->CreateChild(Grid, NWS_VISIBLE | 0x20);   // 0x20: meaning unknown
    return Grid;
}

// 0x100506a0 (vslot 37) - the three grids (basic 6 x 3, party 6 x 1, social 6 x 3); the actions are loaded when the
// action window shows. C4 does not load the page's background texture either
INT NCActionItemListWnd::OnCreate()
{
    NCWnd::OnCreate();
    BasicGrid = CreateGrid(this, 3, 25, 104);
    PartyGrid = CreateGrid(this, 1, 155, 34);
    SocialGrid = CreateGrid(this, 3, 215, 104);
    return 1;
}
NW_HOOK_METHOD(0x100506a0, NCActionItemListWnd, OnCreate)

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

// 0x1004d3a0 (vslot 38)
INT NCActionItemListWnd::OnDestroy()
{
    guard(NCActionItemListWnd::OnDestroy);
    DestroyChild(this, BasicGrid);
    DestroyChild(this, PartyGrid);
    DestroyChild(this, SocialGrid);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1004d3a0, NCActionItemListWnd, OnDestroy)

// 0x1004d2a0 (vslot 62) - the three labels (sysstrings 129, 130 and 204: Basic, Party, Social), the clip is the page
// from 17 pixels above its top to 17 below its bottom
INT NCActionItemListWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY - 17, (INT)Width, (INT)(Height + 17.0f));
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawText(22, 11, 0xFFDCDCDC, *GL2GameData.SysString[129], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(22, 141, 0xFFDCDCDC, *GL2GameData.SysString[130], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(22, 201, 0xFFDCDCDC, *GL2GameData.SysString[204], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x1004d2a0, NCActionItemListWnd, OnPaint)

// an action of actionname-*.dat the way LoadActions reads it (name ours; UE2Engine.h's FL2ActionEntry places only two
// strings, and by the walk below its Name and IconName are swapped: the name is at +1C, the icon's at +28)
struct NActionRow
{
    INT bValid;            // +0x00 0: no action here
    INT ActionID;          // +0x04
    INT Param;             // +0x08 the social action of the social ones
    INT Category;          // +0x0C 1 basic, 2 party, 3 social (4 pet, 5 summon: the pet windows)
    INT* Condition;        // +0x10 points at an int; -1: nothing to wait for (name ours, meaning not known)
    BYTE _Unknown14[8];    // +0x14
    FString Name;          // +0x1C
    FString IconName;      // +0x28
    FString Description;   // +0x34
};

// 0x10050100 (name ours) - every action of the table into the grid of its category (a missing icon is replaced by
// black); a full grid drops the rest. The action 25 (0x25) shows only for the classes 0x35..0x39 (0x100753D0: the
// dwarves, who crystallize), and an action with a condition (the pointer at +10 pointing at -1) shows only with a pet
// or a summon: the pawn's field at +660, or the pet info with its flag at +A8 and an object id. The record is the C4
// one (NItemInfoC4.h)
void NCActionItemListWnd::LoadActions()
{
    for (INT i = 0; i < GL2GameData.ActionTable.Num(); i++)
    {
        NActionRow* Row = (NActionRow*)GL2GameData.ActionTable(i);
        if (Row->bValid != 0 && Row->Category != 0
            && (Row->ActionID != 0x25 || ((NConsoleLeftFns*)GConsole)->IsDwarf()))
        {
            INT Category = Row->Category;
            NItemInfo Info;
            InfoID(&Info) = Row->ActionID;
            InfoParam(&Info) = Row->Param;
            appStrcpy(InfoName(&Info), *Row->Name);
            appStrcpy(InfoIconName(&Info), *Row->IconName);
            appStrcpy(InfoDescription(&Info), *Row->Description);
            InfoIcon(&Info) = FindTexture(InfoIconName(&Info), 1);
            if (!InfoIcon(&Info))
                InfoIcon(&Info) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
            PetInfo Pet = GConsole->GetPetInfo();
            if ((*Row->Condition != -1
                 || (GConsole->GetMyUser() && GConsole->GetMyUser()->Pawn
                     && (nw::Field<INT>(GConsole->GetMyUser()->Pawn, 0x660) != 0
                         || (Pet.UnknownA8 != 0 && Pet.ObjectID >= 0))))
                && (Category == 1 || Category == 2 || Category == 3))
            {
                switch (Category)
                {
                case 1:
                    BasicGrid->AddItem(&Info);
                    break;
                case 2:
                    PartyGrid->AddItem(&Info);
                    break;
                case 3:
                    SocialGrid->AddItem(&Info);
                    break;
                }
            }
        }
    }
}
NW_HOOK_METHOD(0x10050100, NCActionItemListWnd, LoadActions)

// 0x100508a0 (name ours) - the three grids' items are deleted (each: NCActionItemWnd::ClearItems, 0x100d8950)
void NCActionItemListWnd::ClearGrids()
{
    BasicGrid->ClearItems();
    PartyGrid->ClearItems();
    SocialGrid->ClearItems();
}
NW_HOOK_METHOD(0x100508a0, NCActionItemListWnd, ClearGrids)

NW_CHECK_VTABLE(NCActionItemListWnd, 0x101ab068, 74)
