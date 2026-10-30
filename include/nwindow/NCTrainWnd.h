#pragma once
// NCTrainWnd - the skill trainer window (UClass "CTrainWnd", 0x9C; desktop +0xE8): a title
// bar ("Skill Trainer", sysstring 477) over two pages in the same place, the skill list
// (NCTrainListWnd) and the details of one skill (NCTrainInfoWnd). The console drives it: a new
// list empties both pages and shows the list (vslot 151), each row comes with 153; a row
// picked asks the server for the skill, whose answer (152) fills and shows the info page and
// its required items (154). "Learn" asks the server to learn it, "List" goes back.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCTrainListBox;
struct NItemEntry;
struct NItemInfo;
struct NTrainEntry;

// NWM_COMMAND ids of the pages' controls
enum
{
    NID_TRAIN_FRAME = 4,
    NID_TRAIN_LEARN = 2,
    NID_TRAIN_LIST = 3,
    NID_TRAIN_LISTBOX = 0x1A04,
};

// A skill as the info page shows it (names ours; 0x4C0 bytes in C2, 0x2C0 in C1: Name and IconName are
// 256 characters). The console fills one from the
// server's answer and hands it to SetInfo; the page keeps its own (NCTrainInfoWnd::Info), whose
// Items are its required item entries.
struct NTrainSkillInfo
{
    TCHAR Name[256];           // +0x000
    TCHAR IconName[256];       // +0x200
    FString Desc;              // +0x400
    TCHAR Unknown20C[64];      // +0x40C drawn after the level
    UTexture* Icon;            // +0x48C
    NItemEntry* Items[4];      // +0x490 required items: the page's are made by its OnCreate
                               //   (NItemEntry::UnknownC34 1: shown) and freed here
    INT SkillID;               // +0x4A0 sent with Level by "Learn"
    INT Level;                 // +0x4A4
    INT Unknown2A8;            // +0x4A8
    INT SP;                    // +0x4AC required SP
    INT MpCost;                // +0x4B0
    INT Range;                 // +0x4B4 not shown when negative
    INT Unknown2B8;            // +0x4B8
    INT Unknown2BC;            // +0x4BC
    INT bEnchanted;            // +0x4C0 (0) new in C4 (name ours): the skill is an enchanted route: "Learn" asks for it with
                               //   another request, and the page draws the enchant view
    INT Unknown4C4;            // +0x4C4 new in C4 (left as it was by the constructor; the page shows it as a percentage)
    INT Unknown4C8;            // +0x4C8 new in C4 (left as it was; sent with the skill and level by "Learn")
    INT Unknown4CC;            // +0x4CC (-1) new in C4

    NTrainSkillInfo();                                               // 0x1006E920
    ~NTrainSkillInfo();                                              // 0x10082320 frees Items
};
NW_ASSERT_OFFSET(NTrainSkillInfo, Desc, 0x400);
NW_ASSERT_OFFSET(NTrainSkillInfo, Icon, 0x48C);
NW_ASSERT_OFFSET(NTrainSkillInfo, SkillID, 0x4A0);
NW_ASSERT_OFFSET(NTrainSkillInfo, Unknown2BC, 0x4BC);
NW_ASSERT_OFFSET(NTrainSkillInfo, bEnchanted, 0x4C0);
NW_ASSERT_OFFSET(NTrainSkillInfo, Unknown4CC, 0x4CC);
NW_ASSERT_SIZE(NTrainSkillInfo, 0x4D0);

// NCTrainListWnd - the list page (UClass "CTrainListWnd", 0x9C): "Skill list" (sysstring 364)
// over the NCTrainListBox, the player's SP at the bottom right.
class NCTrainListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCTrainListWnd, NCWnd, 0, 0x1030DF38)

    UTexture* BackTex;                     // +0xE8 "L2UI.SkillTrain.TrainerBack1"   // C2: offset derived, no evidence
    NCTrainListBox* ListBox;               // +0xEC
    BYTE bEnchantList;                     // +0xF0 (name ours) new in C4: the rows are enchanted routes (set by AddEntry from the
                                           //   row): the player's SP is not drawn
    BYTE _PadF1[3];

    NCTrainListWnd();                                                // 0x1011e410
    // ~NCTrainListWnd                                               // 0x100dcb70 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011b8b0
    virtual INT OnCreate();                                          // 37 0x1011e4b0
    virtual INT OnDestroy();                                         // 38 0x101566e0 (NCPetActionWnd's)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011cd20
    // name ours
    virtual INT OnListCommand(LPARAM lParam);                        // 74 0x1011cf10 a row picked

    // names ours
    void Clear();                                                    // 0x100b2b90 (C1 only: no C2 match) (C1 only: no C2 match) (the list box's Clear, vslot 70)
    __declspec(noinline) void AddEntry(const NTrainEntry* Entry);                         // 0x10108f10 (C1 only: no C2 match) (the stock NCTrainWnd::AddEntry jumps to it)
};
NW_ASSERT_OFFSET(NCTrainListWnd, ListBox, 0xEC);
NW_ASSERT_SIZE(NCTrainListWnd, 0xF4);

// NCTrainInfoWnd - the skill page (UClass "CTrainInfoWnd", 0x360): "Skill explanation" (366)
// with the icon, name, level, MP cost, range and description; "Learning condition" (367) with
// the required SP and up to four required items; the player's SP; "Learn" and "List".
class NCTrainInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCTrainInfoWnd, NCWnd, 0, 0x1030E9A0)

    UTexture* BackTex;                     // +0xE8 "L2UI.SkillTrain.TrainerBack3"
    UTexture* EnchantBackTex;              // +0xEC (new in C4, name ours) "L2UI_CH3.SkillTrainWnd.skillenchant_back": the background
                                           //   of the enchant view
    NTrainSkillInfo Info;                  // +0xF0 the skill shown (0x4C0 bytes in C2; C1 +0x9C, 0x2C0)
    NCButton* LearnButton;                 // +0x5C0 "Learn" (sysstring 368)
    NCButton* ListButton;                  // +0x5C4 "List" (sysstring 369)
    UTexture* OutLineDownTex;              // +0x5C8 (C3, name ours) "l2ui_ch3.InventoryWnd.Inventory_OutLine_down" the icon's
                                           //   shadow (34 x 34)
    UTexture* OutLineTex;                  // +0x5CC "l2ui_ch3.InventoryWnd.Inventory_OutLine" its frame (35 x 35)

    NCTrainInfoWnd();                                                // 0x1011e590
    // ~NCTrainInfoWnd                                               // 0x1011e620

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011b920
    virtual INT OnCreate();                                          // 37 0x1011e000
    virtual INT OnDestroy();                                         // 38 0x1011d040
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011d0b0
    // names ours
    virtual INT OnLearn();                                           // 74 0x1011c3c0
    virtual INT OnList();                                            // 75 0x1011c4b0

    // names ours
    void SetInfo(NTrainSkillInfo* Src);                              // 0x1011c0f0
    // the page of an enchanted route (OnPaint hands the device over once the clip is set; an anonymous function, name ours; ret 4)
    __declspec(noinline) void PaintEnchant(NCGDevice* Device);       // 0x1011b990
    void Reset();                                                    // 0x1011c340
    void AddRequiredItem(NItemInfo* Item);                           // 0x10108ba0 (C1 only: no C2 match) into a free row
};
NW_ASSERT_OFFSET(NCTrainInfoWnd, Info, 0xF0);
NW_ASSERT_OFFSET(NCTrainInfoWnd, LearnButton, 0x5C0);
NW_ASSERT_OFFSET(NCTrainInfoWnd, OutLineTex, 0x5CC);
NW_ASSERT_SIZE(NCTrainInfoWnd, 0x5D0);

class NCTrainWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCTrainWnd, NCWnd, 0, 0x1030E470)

    NCTrainListWnd* ListWnd;               // +0xE8
    NCTrainInfoWnd* InfoWnd;               // +0xEC

    NCTrainWnd();                                                    // 0x1011e680
    // ~NCTrainWnd                                                   // 0x100eec50 (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x1011e720
    virtual INT OnDestroy();                                         // 38 0x1011dc90
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011dd00

    // the console's (names ours)
    void Reset();                                                    // 0x1011c560 both pages
    void ShowList();                                                 // 0x1011c500
    void ShowInfo();                                                 // 0x1011c520
    void SetSkillInfo(NTrainSkillInfo* Info);                        // 0x1011c540
    void AddRequiredItem(NItemInfo* Item);                           // 0x10108d50 (C1 only: no C2 match)
    void AddEntry(const NTrainEntry* Entry);                         // 0x100bca90 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
};
NW_ASSERT_OFFSET(NCTrainWnd, ListWnd, 0xE8);
NW_ASSERT_SIZE(NCTrainWnd, 0xF0);

#pragma pack(pop)