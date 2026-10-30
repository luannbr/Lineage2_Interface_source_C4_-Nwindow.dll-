/*=============================================================================
    NCDynamicLinker.cpp - the link broken across lines (C2 only). Stock: 0x1003a440 .. 0x1003a4b1,
    0x1003ad20 .. 0x1003ae3f (constructor, destructors) and 0x1003af30 .. 0x1003b1d1 (OnCreate,
    OnDestroy).
=============================================================================*/
#include "nwindow/NCDynamicLinker.h"
#include "nwindow/NCActionLinker.h"
#include "nwindow/NCFileLinker.h"
#include "nwindow/NCGDevice.h"

enum { NID_DYNAMICLINK_PIECE = (INT)0xFF00FF04 };   // the pieces' id (name ours)

// 0x1004a5e0 - everything cleared but the kind and the file pieces (left as allocated)
NCDynamicLinker::NCDynamicLinker()
{
    AbsX = 0;
    AbsY = 0;
    X = 0;
    Y = 0;
    ParentAbsX = 0;
    ParentAbsY = 0;
    W = 0;
    H = 0;
    LayoutWidth = 0;
    TailWidth = 0;
    Target = NULL;
    Message = NULL;
    ActionLinks[0] = NULL;
    ActionLinks[1] = NULL;
    ActionLinks[2] = NULL;
    LinkCount = 0;
}
NW_HOOK_CTOR(0x1004a5e0, NCDynamicLinker)

// 0x1004a6b0
NW_HOOK_DTOR(0x1004a6b0, NCDynamicLinker)

// 0x1004a700 (anonymous in the stock image) - the geometry of the item, the string every piece hands on (file or code), the
// message and the kind
NCDynamicLinker::NCDynamicLinker(INT InAbsX, INT InAbsY, INT InX, INT InY, INT InW, INT InH, INT InLayoutWidth,
                                 INT InTailWidth, INT InParentAbsX, INT InParentAbsY, TCHAR* InTarget,
                                 TCHAR* InMessage, INT InbFile)
{
    AbsX = 0;
    AbsY = 0;
    X = 0;
    Y = 0;
    ParentAbsX = 0;
    ParentAbsY = 0;
    AbsX = InAbsX;
    AbsY = InAbsY;
    X = InX;
    Y = InY;
    ParentAbsX = InParentAbsX;
    ParentAbsY = InParentAbsY;
    W = InW;
    H = InH;
    ActionLinks[0] = NULL;
    ActionLinks[1] = NULL;
    ActionLinks[2] = NULL;
    LinkCount = 0;
    LayoutWidth = InLayoutWidth;
    TailWidth = InTailWidth;
    Target = InTarget;
    Message = InMessage;
    bFile = InbFile;
}
NW_HOOK_CTOR_ARGS(0x1004a700, NCDynamicLinker,
                  (INT A, INT B, INT C, INT D, INT E, INT F, INT G, INT H, INT I, INT J, TCHAR* K, TCHAR* L, INT M),
                  (A, B, C, D, E, F, G, H, I, J, K, L, M))

// 0x10047390 (vslot 25)
INT NCDynamicLinker::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && (INT)wParam == NID_DYNAMICLINK_PIECE)
        return OnLink(lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10047390, NCDynamicLinker, WindowProc)

// 0x100473f0 (vslot 75) - a piece's code goes to the parent
INT NCDynamicLinker::OnLink(INT Code)
{
    NotifyParent(Code);
    return 1;
}
NW_HOOK_METHOD(0x100473f0, NCDynamicLinker, OnLink)

// 0x1004a800 (vslot 37) - one piece per line the item spans (a line is a character's height plus 6): one when it fits in
// one, two when it does not fit twice, else three (the first line from the item's start, the lines between at the
// parent's left, the last at the same place under them). The pieces are NCFileLinkers for a file link and
// NCActionLinkers for an action, each with id 0xFF00FF04, created with style 0x12 and given the viewer's message
INT NCDynamicLinker::OnCreate()
{
    if (!Target)
        return 1;
    SIZE CharSize;
    NCGDevice::GetCharSize(&CharSize, 0x41, 0);
    INT Row = CharSize.cy + 6;
    INT NumPieces;
    if (H > Row * 2)
        NumPieces = 3;
    else
        NumPieces = (H > Row) + 1;
    if (!bFile)
    {
        for (INT i = 0; i < NumPieces; i++)
        {
            NCActionLinker* Piece = new NCActionLinker((INT)Target);
            ActionLinks[i] = Piece;
            Piece->ID = NID_DYNAMICLINK_PIECE;
            if (i == 0)
            {
                if (NumPieces == 1)
                    Piece->SetPosition(0, 0, W - AbsX + ParentAbsX + X, CharSize.cy);
                else if (NumPieces > 1)
                    Piece->SetPosition(0, 0, LayoutWidth - AbsX + ParentAbsX + X, CharSize.cy);
            }
            else if (i == 1)
            {
                if (NumPieces == 2)
                    Piece->SetPosition(ParentAbsX - AbsX, CharSize.cy + 6, TailWidth, CharSize.cy);
                else if (NumPieces == 3)
                    Piece->SetPosition(ParentAbsX - AbsX, CharSize.cy + 6, LayoutWidth, H - (CharSize.cy + 6) * 2);
            }
            else if (i == 2)
                Piece->SetPosition(ParentAbsX - AbsX, H - CharSize.cy, TailWidth, CharSize.cy);
            CreateChild(Piece, 0x12);
            if (Message)
                appStrcpy(Piece->Message, Message);
            LinkCount++;
        }
    }
    else
    {
        for (INT i = 0; i < NumPieces; i++)
        {
            NCFileLinker* Piece = new NCFileLinker(Target);
            FileLinks[i] = Piece;
            Piece->ID = NID_DYNAMICLINK_PIECE;
            if (i == 0)
            {
                if (NumPieces == 1)
                    Piece->SetPosition(0, 0, W - AbsX + ParentAbsX + X, CharSize.cy);
                else if (NumPieces > 1)
                    Piece->SetPosition(0, 0, LayoutWidth - AbsX + ParentAbsX + X, CharSize.cy);
            }
            else if (i == 1)
            {
                if (NumPieces == 2)
                    Piece->SetPosition(ParentAbsX - AbsX, CharSize.cy + 6, TailWidth, CharSize.cy);
                else if (NumPieces == 3)
                    Piece->SetPosition(ParentAbsX - AbsX, CharSize.cy + 6, LayoutWidth, H - (CharSize.cy + 6) * 2);
            }
            else if (i == 2)
                Piece->SetPosition(ParentAbsX - AbsX, H - CharSize.cy, TailWidth, CharSize.cy);
            CreateChild(Piece, 0x12);
            if (Message)
                appStrcpy(Piece->Message, Message);
            LinkCount++;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1004a800, NCDynamicLinker, OnCreate)

// 0x10047c00 (vslot 38) - the pieces go, each from the list its kind keeps, then NControl's OnDestroy
INT NCDynamicLinker::OnDestroy()
{
    guard(NCDynamicLinker::OnDestroy);
    for (INT i = 0; i < LinkCount; i++)
    {
        if (!bFile)
        {
            if (ActionLinks[i])
            {
                RemoveChildWindow(ActionLinks[i]->Entry);
                delete ActionLinks[i];
                ActionLinks[i] = NULL;
            }
        }
        else if (FileLinks[i])
        {
            RemoveChildWindow(FileLinks[i]->Entry);
            delete FileLinks[i];
            FileLinks[i] = NULL;
        }
    }
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10047c00, NCDynamicLinker, OnDestroy)

NW_CHECK_VTABLE(NCDynamicLinker, 0x101a9eb8, 76)
