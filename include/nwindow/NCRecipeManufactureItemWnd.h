#pragma once
// NCRecipeManufactureItemWnd - the ingredient grid of the recipe windows (UClass "CRecipeManufactureItemWnd", 0x110): an
// NCItemWnd (6 columns x 3 rows of 32-pixel cells, 160 entries at most) that NCRecipeManufactureWnd and
// NCRecipeBuyManufactureWnd keep at +0x98. It adds nothing to the base class' layout (sizeof is NCItemWnd's). The
// entries are the ingredients of the recipe being shown: Count is what the player has, UnknownC34 (+0x1938) what the
// recipe needs; an ingredient the player lacks is dimmed with a "NWindow.ChatBack" square over its icon.
//
// What differs from the item window it derives from (C2 vtable 0x101246A0):
//   OnPaint (55)    icons only: no selection frame, the dimming square for a missing ingredient
//   vslot 56        the tool tip painter ("OnPostPaint" in the stock names): the full box of the hovered entry
//   OnMouseMove (60) only tracks the hovered entry
//   slots 25 / 36 / 37 are bodies shared with NCSkillItemWnd / NCQuestItemWnd / NCShopItemWnd (see the .cpp)

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRecipeManufactureItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCRecipeManufactureItemWnd, NCItemWnd, 0, 0x102FDB40)

    NCRecipeManufactureItemWnd();                                    // 0x100f6360
    NCRecipeManufactureItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x100f6400
    // ~NCRecipeManufactureItemWnd                                   // 0x100f63b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (NCSkillItemWnd's body, shared; a stub here)
    virtual INT OnCreate();                                          // 37 0x100edb50 (NCQuestItemWnd's body, shared; a stub here)
    virtual INT OnDestroy();                                         // 38 0x100efac0 (NCShopItemWnd's body, shared; a stub here)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f41b0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100aeb30 ("OnPostPaint")
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100bed40 (shared with other grids)

    // names ours
    // 0x100c7cf0: a new entry that is a copy of Src (the ingredient the console builds), its tool tip made as a
    // recipe's (NTT_Recipe); 0 when the grid is full
    UBOOL AddIngredient(NItemEntry* Src);
};
NW_ASSERT_SIZE(NCRecipeManufactureItemWnd, 0x168);

// 0x100c79f0 (thiscall, ret 4; name ours): NItemEntry's full assignment, field by field (the texts, every number, then
// the tool tip object through NToolTipInfo::operator= 0x1001aca0). NCItemWnd.h's NItemEntry::Copy (0x1001af90) is the
// merging one the item windows use; this is the plain one. Returns the destination. NCItemWnd.h is not this group's, so the
// method sits on a struct derived from NItemEntry (the hook needs a class; `this` is the destination entry).
struct NItemEntryOps : public NItemEntry
{
    NItemEntry* Assign(NItemEntry* Src);
};
inline NItemEntry* NItemEntryAssign(NItemEntry* Dst, NItemEntry* Src)
{
    return static_cast<NItemEntryOps*>(Dst)->Assign(Src);
}

#pragma pack(pop)
