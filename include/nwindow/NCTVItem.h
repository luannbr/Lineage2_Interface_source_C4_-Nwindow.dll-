#pragma once
// NCTVItem - one entry of a tree view (since C3 no UClass any more, a plain object of 0x2880 bytes that NCTree allocates
// with GMalloc; the C2 one was a child window, UClass "CTVItem"). The messenger's tree (MsnTree) is made of them.
// C4 CHECK (wave C, debt2): the layout of this header is the C3 one and it is also the C4 one, read against the C4 image: the
// two `push 0x2880` of NCTVItem's allocations (0x10033F85, 0x10033FC5, and the messenger's 0x10155313 / 0x101553D2), the
// constructors 0x10032BF0 (every store of NTVItemData, up to +0x2858 and +0x284C = 1) and 0x10033D60 (+0x285C .. +0x287C),
// the destructor 0x10033DB0 (Parent +0x285C, Children +0x2860 / count +0x2864) all use the same displacements as the C3 build,
// so the offsets below needed no change; only the addresses in the comments were C3's and are C4's now.
// What the tree is given for an entry (NTVItemData, 0x285C bytes in C3 and in C4; 0x1828 in C2) is the first part of it; then
// come the links: the parent, the entries under it, how deep it sits and whether it is open, visible and selected.
// NCTree::DrawItem puts it in place and draws it (up to two pictures and a text), a click on it toggles bExpanded.
// NCRecipeTree and NCRecipeTreeCtrl (C2 only) include this header for the pointer types.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// where a picture or the text was drawn at the last paint (X, Y: set by DrawItem) and how big it is (W, H: set by
// whoever fills the record) (name ours)
struct NTVBox
{
    INT X;
    INT Y;
    FLOAT W;
    FLOAT H;
};
NW_ASSERT_SIZE(NTVBox, 0x10);

// What a tree is given for an entry (name ours; from the stock constructor 0x10032BF0 and the copy 0x10032C90 of the C4 image;
// C3: 0x1002E2D0 and 0x1002E370)
struct NTVItemData
{
    INT Unknown00;                   // +0x000 (-1 in a new record)
    INT Unknown04;                   // +0x004 (0)
    INT Unknown08;                   // +0x008 (0)
    INT Unknown0C;                   // +0x00C (0)
    TCHAR Text[1024];                // +0x010 (empty) the messenger puts the name here
    TCHAR Text2[2048];               // +0x810 (empty)
    TCHAR Text3[1024];               // +0x1810 (empty)
    TCHAR Tag[1024];                 // +0x2010 (empty) the messenger compares it with "FLN" (name ours)
    NTVBox TextBox;                  // +0x2810 (0) right of the first picture; its size is the selection bar's
    NTVBox Icon2Box;                 // +0x2820 (0) the second picture
    NTVBox IconBox;                  // +0x2830 (0) the first picture (collapsed or expanded), at the entry's place
    INT TextOffsetX;                 // +0x2840 (0) how far right of the first picture the second picture / the bar go
    INT CollapsedOffsetY;            // +0x2844 (0) how far down the collapsed picture is drawn
    INT Unknown2848;                 // +0x2848 left as allocated by the constructor, copied
    UBOOL bCenterOnIcon2;            // +0x284C (1) the text is centered on the second picture's height (else the first)
    UTexture* Icon2Tex;              // +0x2850 (0) the second picture, right of the first
    UTexture* ExpandedTex;           // +0x2854 (0) the first picture of an open entry
    UTexture* CollapsedTex;          // +0x2858 (0) the first picture of a closed one

    __declspec(noinline) NTVItemData();                              // 0x10032BF0
    __declspec(noinline) NTVItemData& operator=(const NTVItemData& Other);   // 0x10032C90 everything
};
NW_ASSERT_OFFSET(NTVItemData, Text, 0x10);
NW_ASSERT_OFFSET(NTVItemData, Text2, 0x810);
NW_ASSERT_OFFSET(NTVItemData, Text3, 0x1810);
NW_ASSERT_OFFSET(NTVItemData, Tag, 0x2010);
NW_ASSERT_OFFSET(NTVItemData, TextBox, 0x2810);
NW_ASSERT_OFFSET(NTVItemData, Icon2Box, 0x2820);
NW_ASSERT_OFFSET(NTVItemData, IconBox, 0x2830);
NW_ASSERT_OFFSET(NTVItemData, TextOffsetX, 0x2840);
NW_ASSERT_OFFSET(NTVItemData, bCenterOnIcon2, 0x284C);
NW_ASSERT_OFFSET(NTVItemData, CollapsedTex, 0x2858);
NW_ASSERT_SIZE(NTVItemData, 0x285C);

class NCTVItem;

// the children's array: its destructor is a function of its own in the stock code (0x10033C90 in C4, C3: 0x1002F370; the
// elements' Remove with UnTemplate.h's checks, then FArray's destructor), shared with the messenger's lists (name ours)
class NTVItemArray : public TArray<NCTVItem*>
{
public:
    __declspec(noinline) ~NTVItemArray();                           // 0x10033c90 (C3 0x1002F370)
    __declspec(noinline) INT RemoveItem(NCTVItem* const& Item);     // 0x10033d20 (ret 4, name ours) every element equal to Item goes; how many
};
NW_ASSERT_SIZE(NTVItemArray, 0xC);

class NCTVItem
{
public:
    NTVItemData Data;                // +0x0000 the copy of the record
    NCTVItem* Parent;                // +0x285C the entry this one is under (name ours)
    NTVItemArray Children;           // +0x2860 the entries under this one
    INT Level;                       // +0x286C the constructor's argument (-1) (name ours)
    UBOOL bExpanded;                 // +0x2870 the children are shown (name ours)
    INT Unknown2874;                 // +0x2874 (0)
    UBOOL bVisible;                  // +0x2878 (1) hit tests skip an entry that is not (name ours)
    UBOOL bSelected;                 // +0x287C the selection bar is drawn (name ours)

    NCTVItem(INT InLevel = -1);                                      // 0x10033D60 (ret 4)
    ~NCTVItem();                                                     // 0x10033DB0 the children first (deleted)

    // name ours: Child goes under this entry, at the end (Index -1) or at Index, and is handed back (NULL: nothing)
    NCTVItem* AddChild(NCTVItem* Child, INT Index);                  // 0x10033800 (ret 8)
    void Expand();                                                   // 0x10032E20 (name ours) open, the children visible
    void Collapse();                                                 // 0x10032E70 (name ours) closed, the children not
    void DeleteChildItem(NCTVItem* Child);                           // 0x10033EB0 (ret 4) taken out and deleted
    void DeleteAllChildren();                                        // 0x10033E50 (name ours) every child deleted, the array emptied
};
NW_ASSERT_OFFSET(NCTVItem, Parent, 0x285C);
NW_ASSERT_OFFSET(NCTVItem, Children, 0x2860);
NW_ASSERT_OFFSET(NCTVItem, Level, 0x286C);
NW_ASSERT_OFFSET(NCTVItem, bExpanded, 0x2870);
NW_ASSERT_OFFSET(NCTVItem, bVisible, 0x2878);
NW_ASSERT_OFFSET(NCTVItem, bSelected, 0x287C);
NW_ASSERT_SIZE(NCTVItem, 0x2880);

#pragma pack(pop)
