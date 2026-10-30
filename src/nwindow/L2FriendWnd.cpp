/*=============================================================================
    L2FriendWnd.cpp - the Lineage II friend list page of the messenger (C3 only; C3 addresses). Stock: 0x10133D90 (OnPaint),
    0x10133E30 (OnDestroy), 0x10136000 (OnCreate), 0x101360C0 (the task window), 0x10137370 (ShowWindow), 0x101373F0 (the free chat
    window), 0x10138CB0 (open a chat), 0x10139510 (double click), 0x1013AC10 (WindowProc), 0x1013B6D0 / 0x1013B7E0 (constructor,
    destructor).
    Left stock on purpose: the chat windows' own functions (the L2ChatWnd constructor 0x10125B20 and its name setter 0x10131650,
    the L2TaskWnd's list 0x10126240 / 0x101255D0 / 0x101263F0) belong to the chat lane.
=============================================================================*/
#include "nwindow/L2FriendWnd.h"
#include "nwindow/L2ChatWnd.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/L2Task.h"
#include "nwindow/L2FriendTree.h"
#include "nwindow/L2FriendTreeCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/MsnUiStockP.h"

enum
{
    SYSSTR_L2FriendChatTitle = 997,
    L2FRIEND_OnlineType = 3,        // NTVItemData::Unknown2848 of a friend entry
};

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// (struct NL2Book, the 0x20-byte object the constructor makes (0x101525f0): an array, empty, and a table by name, empty (names ours): MsnUiStock.h)

// 0x101525f0 - the array empty, the table empty (the pairs none, 8 heads: the member's constructor, which builds the heads with the stock
// Rehash 0x1013fc50)
NW_HOOK_CTOR(0x101525f0, NL2Book)

// 0x10151670 (ret 0xC) - the record of the friend of that name (the table by name's value), NULL for none; the name is freed here. The
// table's search is the stock 0x101514f0 (the case-folded hash of the name, then the chain)
void* NL2Book::Find(NMsnStringArg Name)
{
    void* Found = Names.FindWnd(&Name.Str());                    // NMsnWndStrMap::FindWnd 0x101514F0
    Name.Str().~FString();
    return Found;
}
NW_HOOK_VARARGS(0x10151670, NL2Book, Find, void* (NL2Book::*)(NMsnStringArg))

// 0x101600c0 - the two tables are built by their members' constructors; the 0x20-byte object is made and the pointers cleared
L2FriendWnd::L2FriendWnd()
{
    void* Mem = appMalloc(0x20, TEXT("new"));
    Book = Mem ? new (Mem) NL2Book : NULL;                       // NL2Book's constructor 0x101525f0
    TreeCtrl = NULL;
    TaskWnd = NULL;
    BackTex = NULL;
}
NW_HOOK_CTOR(0x101600c0, L2FriendWnd)

// 0x101601d0 - the number table and the name table are freed by the members' destructors (in reverse order)
NW_HOOK_DTOR(0x101601d0, L2FriendWnd)

// 0x1015f5f0 (vslot 25) - a double click (0x203) on a friend
INT L2FriendWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0x203)
        return OnDoubleClick((INT)wParam, (INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1015f5f0, L2FriendWnd, WindowProc)

// 0x1015ab80 (vslot 37) - NCWnd's, the picture and the friends' tree (240 x 270 at (8, 45))
INT L2FriendWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.msn_back0"));
    TreeCtrl = new L2FriendTreeCtrl();
    TreeCtrl->SetPosition(8, 0x2D, 0xF0, 0x10E);
    CreateChild(TreeCtrl, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1015ab80, L2FriendWnd, OnCreate)

// 0x10158ad0 (vslot 38) - the tree goes, then NCWnd's
INT L2FriendWnd::OnDestroy()
{
    DestroyChild(this, TreeCtrl);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10158ad0, L2FriendWnd, OnDestroy)

// 0x1015bd60 (vslot 44) - NCWnd's, then the windows of the number table hidden
void L2FriendWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    for (INT i = 0; i < FreeWnds.Num; i++)
    {
        if (FreeWnds.Data[i].Wnd)
            FreeWnds.Data[i].Wnd->HideWindow();
    }
}
NW_HOOK_METHOD(0x1015bd60, L2FriendWnd, ShowWindow)

// 0x10158a30 (vslot 62) - the picture (256 x 353)
INT L2FriendWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 0x100, 0x161, 0, 0, 0x100, 0x161, BackTex, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10158a30, L2FriendWnd, OnPaint)

// 0x1015ac50 (name ours) - the task window (172 x 132, in the bottom right corner of the screen, a child of the desktop) made
// the first time, and shown
void L2FriendWnd::ShowTaskWnd()
{
    if (!TaskWnd)
    {
        TaskWnd = new ::L2TaskWnd;                                                      // L2TaskWnd's constructor 0x1014AEC0
        TaskWnd->SetPosition(GSEKScreenX - 0xAC, GSEKScreenY - 0x19E, 0xAC, 0x84);
        GConsole->Desktop->CreateChild(TaskWnd, 0x4002);
    }
    TaskWnd->ShowWindow();
}
NW_HOOK_METHOD(0x1015ac50, L2FriendWnd, ShowTaskWnd)

// 0x1015bde0 (name ours) - the first chat window of the number table that is not in use (its +0xD8 flag is clear), else nothing
NCWnd* L2FriendWnd::FindFreeChatWnd()
{
    for (INT i = 0; i < FreeWnds.Num; i++)
    {
        NCWnd* Wnd = FreeWnds.Data[i].Wnd;
        if (Wnd && static_cast<L2ChatWnd*>(Wnd)->Unknown_D8 == 0)
            return Wnd;
    }
    return NULL;
}
NW_HOOK_METHOD(0x1015bde0, L2FriendWnd, FindFreeChatWnd)

// 0x1015de80 (name ours; ret 8) - a double click at (X, Y): on a friend that is on line (the entry's type 3 with its word set)
// the chat with him opens
INT L2FriendWnd::OnDoubleClick(INT X, INT Y)
{
    L2FriendTree* Tree = TreeCtrl->FriendTree;
    NCTVItem* Hit = Tree->FindItemAt(X, Y, Tree->Root);
    if (Hit)
    {
        NTVItemData Item = Hit->Data;
        if (Item.Unknown2848 == L2FRIEND_OnlineType && Item.Unknown08)
            OpenChat(MsnStringArgOf(Item.Text));
    }
    return 1;
}
NW_HOOK_METHOD(0x1015de80, L2FriendWnd, OnDoubleClick)

// 0x1015d640 (name ours; ret 0xC) - the chat window for this friend: the one open already is shown and focused; else a window of
// the number table that is not in use, or a new one (L2ChatWnd, 256 x 401 at a random place of the desktop), is entered by
// the name, titled, given the name, shown and focused; the task window learns of it (or, for an open one, that it is in use)
void L2FriendWnd::OpenChat(NMsnStringArg Name)
{
    ShowTaskWnd();
    NCWnd* Chat;
    {
        FString Key(*Name.Str());
        Chat = ChatWnds.FindWnd(&Key);                                                      // the table of names, 0x101514F0
    }
    if (!Chat)
    {
        Chat = FindFreeChatWnd();
        if (!Chat)
        {
            Chat = new L2ChatWnd(this, Name.Str());                                         // L2ChatWnd's constructor 0x1014A5D0
            INT X = appRand() % 200 + 200;
            INT Y = appRand() % 150 + 150;
            Chat->SetPosition(X, Y, 0x100, 0x191);
            GConsole->Desktop->CreateChild(Chat, 0x2002);
        }
        ChatWnds.SetWnd(*Name.Str(), &Chat);                                                // entered by the name, 0x1015AD40
        ::nw::VirtualCall<void>(Chat, 56, (INT)SYSSTR_L2FriendChatTitle);   // SetTitleID: the C4 stock calls the slot 56 with the SysString id
        static_cast<L2ChatWnd*>(Chat)->SetFriendName(Name.Str());                           // the chat window keeps the name, 0x101562A0
        static_cast<L2ChatWnd*>(Chat)->Unknown_D8 = 1;                                   // in use (L2ChatWnd +0xF8)
        Chat->ShowWindow();
        Chat->SetFocus();
        if (TaskWnd)
            ((NChatL2TaskWnd*)TaskWnd)->AddTask(Chat, Name.Str(), 0);                       // the task window lists it, 0x1014AD00
    }
    else
    {
        ::nw::VirtualCall<void>(Chat, 56, (INT)SYSSTR_L2FriendChatTitle);   // SetTitleID: the C4 stock calls the slot 56 with the SysString id
        static_cast<L2ChatWnd*>(Chat)->SetFriendName(Name.Str());                           // 0x101562A0
        static_cast<L2ChatWnd*>(Chat)->Unknown_D8 = 1;                                   // in use (L2ChatWnd +0xF8)
        Chat->ShowWindow();
        Chat->SetFocus();
        if (TaskWnd)
        {
            NCWnd* Listed = static_cast< ::L2TaskWnd*>(TaskWnd)->FindTask(Name.Str());      // its entry, 0x1014A080
            if (Listed)
                static_cast<L2Task*>(Listed)->bAlert = 0;                               // (L2Task +0xF8 in C4)
        }
    }
    Name.Str().~FString();
}
NW_HOOK_VARARGS(0x1015d640, L2FriendWnd, OpenChat, void (L2FriendWnd::*)(NMsnStringArg))

// 0x101560c0 (ret 0xC) - the record of the friend of that name in the book (0x1012CBD0)
void* L2FriendWnd::FindFriend(NMsnStringArg Name)
{
    void* Found = ((NL2Book*)Book)->Find(MsnStringCopy(Name.Str()));    // NL2Book::Find 0x10151670
    Name.Str().~FString();
    return Found;
}
NW_HOOK_VARARGS(0x101560c0, L2FriendWnd, FindFriend, void* (L2FriendWnd::*)(NMsnStringArg))

// 0x10156020 (ret 0x18) - the console is asked (0x10061180: the network handler's virtual 263) to send the text to the friend
void L2FriendWnd::SendWhisper(NMsnStringArg Name, NMsnStringArg Text)
{
    GConsole->RequestSendL2FriendSay((TCHAR*)*Name.Str(), (TCHAR*)*Text.Str());   // NConsoleWnd::RequestSendL2FriendSay 0x1006ca80
    Name.Str().~FString();
    Text.Str().~FString();
}
NW_HOOK_VARARGS(0x10156020, L2FriendWnd, SendWhisper, void (L2FriendWnd::*)(NMsnStringArg, NMsnStringArg))

// 0x1015de00 (ret 4) - a chat window closed: its name leaves the name table (0x101374E0), the task window forgets it (0x10125580) and
// hides when it has nothing left, the chat window is cleared, and it is entered in the number table
void L2FriendWnd::OnChatClosed(NCWnd* Chat)
{
    ((NMsnStrMapOps*)&ChatWnds)->RemoveKey(*static_cast<L2ChatWnd*>(Chat)->FriendName);          // 0x10151E60
    if (TaskWnd)
        static_cast< ::L2TaskWnd*>(TaskWnd)->RemoveTask(Chat);                                       // 0x1014A030
    if (TaskWnd && static_cast< ::L2TaskWnd*>(TaskWnd)->Tasks.Pairs.Num() == 0)
        TaskWnd->HideWindow();
    static_cast<L2ChatWnd*>(Chat)->ClearAll();                                                       // 0x10148DF0
    INT Key = (INT)Chat;
    FreeWnds.SetWnd(&Key, &Chat);
}
NW_HOOK_METHOD(0x1015de00, L2FriendWnd, OnChatClosed)

NW_CHECK_VTABLE(L2FriendWnd, 0x101dd400, 74)
