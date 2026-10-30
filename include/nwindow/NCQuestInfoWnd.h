#pragma once
// NCQuestInfoWnd - the quest window's "Description" page (UClass "CQuestInfoWnd", 0xEC): the
// text of the selected quest at one of its steps (quest data, NConsoleWnd::AddQuestID), shown
// at the step the player is at; the arrows (NCQuestWnd's) page back to the earlier steps and
// forward again.

#include "nwindow/NCScrollWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// a quest of the list (name ours; 0x18 bytes in C2, allocated one by one: the window keeps pointers). The server
// sends the steps of a quest as a bit set (bit N: step N + 1; a count when bit 31 is clear), the window
// keeps the numbers (C1: 12 bytes, a quest had one step)
struct NQuestEntry
{
    INT QuestID;               // +0x0
    INT Step;                  // +0x4 index in Steps of the step the player is at (the last one)
    INT ShownStep;             // +0x8 index in Steps of the step shown (0 .. Step)
    TArray<INT> Steps;         // +0xC the step numbers (1 .. 30), in ascending order

    NQuestEntry() : Step(0), ShownStep(0) { Steps.Empty(); }      // 0x100bfed0 (the quest id is set by AddQuest); ~NQuestEntry 0x100c0090 (the steps' array)
};
NW_ASSERT_OFFSET(NQuestEntry, Steps, 0xC);
NW_ASSERT_SIZE(NQuestEntry, 0x18);

class NCQuestInfoWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCQuestInfoWnd, NCScrollWnd, 0, 0x102FA758)

    TArray<NQuestEntry*> Quests;     // +0x120 in the order of NCQuestWnd's list (C1: TArray<NQuestEntry>)
    FString Text;                    // +0x12C the text shown (C1: TCHAR Text[1024] at +0xD8)
    INT Current;                     // +0x138 index in Quests, -1 = none (C1 +0x8D8); the arrows (C1: two more
                                     //   fields) are NCQuestWnd's now

    NCQuestInfoWnd();                                                // 0x100eecf0
    // ~NCQuestInfoWnd                                               // 0x100eed90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90
    virtual INT OnCreate();                                          // 37 0x100edae0
    virtual INT OnDestroy();                                         // 38 0x100ef700
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100edd60
    virtual INT ScrollUp();                                          // 75 0x100edef0
    virtual INT ScrollDown();                                        // 76 0x100edf80
    virtual INT OnScrollBar(INT Value);                              // 77 0x100ee020 (C3: not rounded to a line step)

    // names ours (C2: plain functions; C1 had NextStep / PrevStep as virtuals)
    void ShowQuest(INT Index);                                       // 0x100ee080
    void NextStep();                                                 // 0x100ee110
    void PrevStep();                                                 // 0x100ee1b0
    void ClearQuests();                                              // 0x100eee10
};
NW_ASSERT_OFFSET(NCQuestInfoWnd, Quests, 0x120);
NW_ASSERT_OFFSET(NCQuestInfoWnd, Text, 0x12C);
NW_ASSERT_OFFSET(NCQuestInfoWnd, Current, 0x138);
NW_ASSERT_SIZE(NCQuestInfoWnd, 0x13C);

#pragma pack(pop)
