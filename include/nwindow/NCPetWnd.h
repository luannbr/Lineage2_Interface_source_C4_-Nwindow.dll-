#pragma once
// NCPetWnd - the pet window (UClass "CPetWnd", 0x184; opened from the pet's status bar or the
// action window): name and level, HP / MP / food / exp / weight bars, the Name button (a pet
// can be named once) and three pages under tabs: Information (NCPetDetailStatusWnd, the combat
// values), Action (NCPetActionWnd, the pet's actions) and Items (NCPetInventoryWnd).
// Everything is read again from the network handler at each paint.
//
// Desktop (NCConsole) +0x104 is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCActionItemWnd;
class NCButton;
class NCPetInventoryWnd;
class NCTabControl;
struct NItemInfo;

// NWM_COMMAND ids (names ours; the summon window uses the same tab id)
enum
{
    NID_PET_TABS = 0xFE001B,
    NID_PET_FRAME = 0xFE001C,
    NID_PET_NAME = 0xFE001D,          // the Name button
};

// The Information page (UClass "CPetDetailStatusWnd", 0x14C): P.Atk. ... Casting Spd.
class NCPetDetailStatusWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetDetailStatusWnd, NCWnd, 0, 0x102F3F58)

    UTexture* BackTex;               // +0xE8 "L2UI.PetWnd.Pet02"
    PetInfo Pet;                     // +0xEC (0xAC in C2) the handler's record, read again by each paint   // C2: offset derived, no evidence

    NCPetDetailStatusWnd();                                          // 0x100deb90
    // ~NCPetDetailStatusWnd                                         // 0x100b2d70 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100db070
    virtual INT OnDestroy();                                         // 38 0x100db0a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // C2: slot gone (was 50)
};
NW_ASSERT_OFFSET(NCPetDetailStatusWnd, Pet, 0xEC);
NW_ASSERT_SIZE(NCPetDetailStatusWnd, 0x1A0);

// The Action page (UClass "CPetActionWnd", 0x9C): the pet actions of actionname-*.dat
// (category 4) in an icon grid.
class NCPetActionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetActionWnd, NCWnd, 0, 0x102F4488)

    UTexture* BackTex;               // +0xE8 "L2UI.PetWnd.pet03"   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NCActionItemWnd* Items;          // +0xEC

    NCPetActionWnd();                                                // 0x100dec50
    // ~NCPetActionWnd                                               // 0x100b2e10 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100de3c0
    virtual INT OnDestroy();                                         // 38 0x100dbce0
    virtual INT OnPaint(NCGDevice* Device);                          // C2: slot gone (was 50)

    void LoadActions();                                              // 0x100dbd90 (name ours; an anonymous sub_ in the db)
};
NW_ASSERT_OFFSET(NCPetActionWnd, Items, 0xEC);
NW_ASSERT_SIZE(NCPetActionWnd, 0xF0);

class NCPetWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetWnd, NCWnd, 0, 0x102F34F0)

    UTexture* BackTex;               // +0xE8 (C3) "L2UI_CH3.PetWnd.Petinterface_back1"
    UTexture* BackTex2;              // +0xEC (C3) "L2UI_CH3.PetWnd.Petinterface_back2"
    UTexture* HPTex;                 // +0xF0 "L2UI.StatusWnd.statusHP1"
    UTexture* MPTex;                 // +0xF4 "L2UI.StatusWnd.statusMP1"
    UTexture* EXPTex;                // +0xF8 "L2UI.StatusWnd.statusEXP1"
    UTexture* WeightTex[4];          // +0xFC "L2UI.StatusWnd.statusWt1" .. 4 (as the status
                                     //   window: up to 50%, 66.66%, 80%, above)
    UTexture* FedTex;                // +0x10C "L2UI.PetWnd.statusEat1"
    PetInfo Pet;                     // +0x110 (0xAC in C2) the handler's record, read again by each paint   // C2: offset derived, no evidence
    NCPetDetailStatusWnd* DetailWnd; // +0x1C4
    NCPetActionWnd* ActionWnd;       // +0x1C8
    NCPetInventoryWnd* InventoryWnd; // +0x1CC
    NCTabControl* TabCtrl;           // +0x1D0
    NCButton* NameButton;            // +0x1D4
    UBOOL bCanName;                  // +0x1D8 (1) the console clears it (SetCanName)

    NCPetWnd();                                                      // 0x100def60
    // ~NCPetWnd                                                     // 0x100b3870 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100db170
    virtual INT OnCreate();                                          // 37 0x100df040
    virtual INT OnDestroy();                                         // 38 0x100dc620
    virtual void ShowWindow();                                       // 48 0x100dd030
    virtual void Minimize();                                         // 54 0x100db1e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100dc770
    virtual INT OnName();                                            // 74 0x100dd0f0 (name ours)

    // names ours (the console)
    void SetCanName(UBOOL bCan);                                     // 0x100db240
    void ClearInventory();                                           // 0x100db260
    UBOOL AddInventoryItem(NItemInfo* Info);                         // 0x100de9a0 (C1 0x100940b0)
    UBOOL UpdateInventoryItem(INT Op, NItemInfo* Info);              // 0x100deb70 (C1 0x100940d0)
    virtual void SetFrameTitleID(INT ID);                            // 56 0x100db1d0 (ret 4) FrameCtrl (+0x7C) takes the SysString title, no null test (NCWnd's slot)
};
NW_ASSERT_OFFSET(NCPetWnd, WeightTex, 0xFC);
NW_ASSERT_OFFSET(NCPetWnd, Pet, 0x110);
NW_ASSERT_OFFSET(NCPetWnd, DetailWnd, 0x1C4);
NW_ASSERT_OFFSET(NCPetWnd, bCanName, 0x1D8);
NW_ASSERT_SIZE(NCPetWnd, 0x1DC);

#pragma pack(pop)