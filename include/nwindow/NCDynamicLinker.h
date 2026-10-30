#pragma once
// NCDynamicLinker - a link of an HTML page that the layout broke across lines (UClass
// "CDynamicLinker", 0x224 in C3; NCHtmlViewer::AddFileLink and AddActionLink create it with the item's
// geometry): OnCreate makes one, two or three child links (NCFileLinker when bFile, else NCActionLinker) over the
// pieces: the first line's part, the lines between, the last line's part, as many as the link's height asks for;
// a click on any of them is passed on to the parent with the child's code. The geometry fields are the
// constructor's arguments (the HTML layout's measures of the link).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCActionLinker;
class NCFileLinker;

class NCDynamicLinker : public NControl
{
    NW_DECLARE_CLASS(NCDynamicLinker, NControl, 0, 0x102CB370)

    // C3 layout (the constructor 0x10045070, OnCreate 0x10045170, OnDestroy 0x10041c60; names ours)
    INT AbsX;                        // +0x1F4 the link's left (the first argument)
    INT AbsY;                        // +0x1F8
    INT X;                           // +0x1FC added to the width of the first piece
    INT Y;                           // +0x200
    INT ParentAbsX;                  // +0x204 the pieces after the first start at ParentAbsX - AbsX
    INT ParentAbsY;                  // +0x208
    INT W;                           // +0x20C the right edge of a one-line link
    INT H;                           // +0x210 the link's height (it decides the number of pieces)
    INT LayoutWidth;                 // +0x214 the right edge of the first line when there are several pieces, and the width of
                                     //   the middle piece of three
    INT TailWidth;                   // +0x218 the width of the last piece (and of the second of two)
    TCHAR* Target;                   // +0x21C the string every piece hands on (the file or the action code); NULL: no pieces
    TCHAR* Message;                  // +0x220 copied into each piece's Message (the chat line / the question), or NULL
    INT bFile;                       // +0x224 1: NCFileLinker pieces (AddFileLink), 0: NCActionLinker pieces
    NCActionLinker* ActionLinks[3];  // +0x228 the pieces of an action link
    NCFileLinker* FileLinks[3];      // +0x234 the pieces of a file link
    INT LinkCount;                   // +0x240 pieces made

    NCDynamicLinker();                                               // 0x1004a5e0 (the class constructor InternalConstructor calls)
    // the one the HTML viewer calls (13 arguments, ret 0x34)
    NCDynamicLinker(INT InAbsX, INT InAbsY, INT InX, INT InY, INT InW, INT InH, INT InLayoutWidth, INT InTailWidth,
                    INT InParentAbsX, INT InParentAbsY, TCHAR* InTarget, TCHAR* InMessage, INT InbFile); // 0x10045070
    // ~NCDynamicLinker                                              // 0x1004a6b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10047390
    virtual INT OnCreate();                                          // 37 0x1004a800
    virtual INT OnDestroy();                                         // 38 0x10047c00
    virtual INT OnLink(INT Code);                                    // 75 0x100473f0 (name ours) a piece was clicked: the parent is told
};
NW_ASSERT_OFFSET(NCDynamicLinker, AbsX, 0x1F4);
NW_ASSERT_OFFSET(NCDynamicLinker, H, 0x210);
NW_ASSERT_OFFSET(NCDynamicLinker, Target, 0x21C);
NW_ASSERT_OFFSET(NCDynamicLinker, bFile, 0x224);
NW_ASSERT_OFFSET(NCDynamicLinker, ActionLinks, 0x228);
NW_ASSERT_OFFSET(NCDynamicLinker, FileLinks, 0x234);
NW_ASSERT_OFFSET(NCDynamicLinker, LinkCount, 0x240);
NW_ASSERT_SIZE(NCDynamicLinker, 0x244);

#pragma pack(pop)
