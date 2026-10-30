#pragma once
// TaskWnd - the messenger's task list (UClass "TaskWnd", 0xEC; C3 only): a narrow window (a vertical title bar and a
// TaskBox) listing the open conversations. It keeps a hash map from each conversation window (ChatWnd) to its Task
// window (Task), and draws a three-piece back (msn6_back1 .. 3).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class TaskBox;

// One pair of TaskWnd's map (UE2's TMapBase::TPair, 12 bytes): the conversation window and its Task. The hash is the key
// object's index (GetTypeHash(UObject*)).
struct NTaskPair
{
    INT HashNext;                    // +0x00
    NCWnd* Key;                      // +0x04 the conversation window (ChatWnd / L2ChatWnd)
    NCWnd* Value;                    // +0x08 its Task / L2Task window
};
NW_ASSERT_SIZE(NTaskPair, 0xC);

typedef TArray<NTaskPair> NTaskPairArray;      // destructor 0x1011C480 (and L2TaskWnd's copy 0x10125030)

// TMap<NCWnd*, NCWnd*> as UE2 lays it out: the pairs, the hash table, its size (a power of 2, 8 at least) (name ours)
struct NTaskMap
{
    NTaskPairArray Pairs;            // +0x00
    INT* Hash;                       // +0x0C
    INT HashCount;                   // +0x10

    NTaskMap() : Hash(NULL), HashCount(8) { Rehash(); }              // the stock constructors' inline code (0x1011FB60)
    ~NTaskMap()                                                      // 0x1011D490 / 0x10125760: the table freed
    {
        if (Hash)
            appFree(Hash);
        Hash = NULL;
        HashCount = 0;
    }
    __declspec(noinline) void Rehash();                              // 0x1013FE50 TMapBase::Rehash (shared with L2TaskWnd's; out of line in the stock: its callers call it)
    // 0x1011C290 (ret 4; the key by reference): every pair of that window goes (through the 12-byte arrays' out-of-line
    // Remove, 0x1012CA10); returns how many
    INT RemoveKey(NCWnd* const& InKey);
    void Relax()
    {
        while (Pairs.Num() * 2 + 8 < HashCount)
            HashCount = HashCount / 2;
        Rehash();
    }
};
NW_ASSERT_SIZE(NTaskMap, 0x14);

// the task map's iterator: after a removal the map is relaxed (0x1011C300)
struct NTaskIter
{
    NTaskMap* Map;                   // +0x00
    NTaskMap* Pairs;                 // +0x04
    INT Index;                       // +0x08
    INT Removed;                     // +0x0C

    NTaskIter(NTaskMap& InMap) : Map(&InMap), Pairs(&InMap), Index(0), Removed(0) {}
    __declspec(noinline) ~NTaskIter();                               // 0x1011C300
};
NW_ASSERT_SIZE(NTaskIter, 0x10);

class TaskWnd : public NCWnd
{
    NW_DECLARE_CLASS(TaskWnd, NCWnd, 0, 0x1031AA78)

    NTaskMap Tasks;                  // +0xE8 (name ours) conversation window -> its Task window
    TaskBox* List;                   // +0xFC 140 x 126 at (14, 3), 6 lines shown
    UTexture* BackTex1;              // +0x100 "L2UI_CH3.msn6_back1": the top of the back (180 x 8 at (12, 0))   // C2: offset derived, no evidence
    UTexture* BackTex2;              // +0x104 "L2UI_CH3.msn6_back2": the middle, stretched (180 x 116 at (12, 8))   // C2: offset derived, no evidence
    UTexture* BackTex3;              // +0x108 "L2UI_CH3.msn6_back3": the bottom (180 x 8 at (12, 124))   // C2: offset derived, no evidence

    TaskWnd();                                                       // 0x101446f0
    // ~TaskWnd                                                      // 0x10144790

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x10143430
    virtual INT OnDestroy();                                         // 38 0x10141b50
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013fab0 (shared with L2TaskWnd)
};
NW_ASSERT_OFFSET(TaskWnd, Tasks, 0xE8);
NW_ASSERT_OFFSET(TaskWnd, List, 0xFC);
NW_ASSERT_OFFSET(TaskWnd, BackTex1, 0x100);
NW_ASSERT_SIZE(TaskWnd, 0x10C);

#pragma pack(pop)
