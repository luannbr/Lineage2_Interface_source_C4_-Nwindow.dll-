/*=============================================================================
    NCTestHtmlViewer.cpp - the HTML test page. Stock: 0x10026640 .. 0x100266aa,
    0x100277e0 .. 0x1002787f, 0x100298d0 .. 0x10029a3c, 0x1002c660 .. 0x1002c6b4; OnDestroy,
    OnPaint and OnClose are NCNPCHtmlViewer's.
=============================================================================*/
#include "nwindow/NCTestHtmlViewer.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"

enum { NID_TESTHTML_LOAD = (INT)0xF0F01413 };  // the button's NWM_COMMAND id (name ours)

// 0x1003f6e0 - NCNPCHtmlViewer's, nothing more
NCTestHtmlViewer::NCTestHtmlViewer()
{
}
NW_HOOK_CTOR(0x1003f6e0, NCTestHtmlViewer)

// 0x1003f730
NW_HOOK_DTOR(0x1003f730, NCTestHtmlViewer)


// 0x1003b610 (vslot 25)
INT NCTestHtmlViewer::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCNPCHtmlViewer::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == (WPARAM)NID_TESTHTML_LOAD)
        return OnLoad() != 0;
    return 0;
}
NW_HOOK_METHOD(0x1003b610, NCTestHtmlViewer, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1003f780 (vslot 37) - the dialog, then an edit line (192 x 16, hidden) 20 pixels above the bottom and a 16 x 16
// white button (id 0xF0F01413) at its right. The edit line is hidden between the registration and the entry's
// assignment, as in the stock code
INT NCTestHtmlViewer::OnCreate()
{
    NCNPCHtmlViewer::OnCreate();
    FileEdit = new NCEditBox(0, TEXT("NWindow.BlackTexture"), 1, 0, 0);
    FileEdit->SetPosition(0, (INT)(Height - 20.0f), 192, 16);
    NWndEntry* E = AddChildWindow(FileEdit, 0x22);
    FileEdit->HideWindow();
    FileEdit->Entry = E;
    if (E->Parent)
    {
        NCWnd* ParentWnd = E->Parent->Wnd;
        FileEdit->ScreenX += ParentWnd->ScreenX;
        FileEdit->ScreenY += ParentWnd->ScreenY;
    }
    FileEdit->WindowProc(WM_CREATE, 0, 0);
    LoadButton = new NCButton(TEXT("NWindow.WhiteTexture"), TEXT("NWindow.WhiteTexture"), 0);
    LoadButton->ID = NID_TESTHTML_LOAD;
    LoadButton->SetPosition(192, (INT)(Height - 20.0f), 16, 16);
    CreateChild(LoadButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1003f780, NCTestHtmlViewer, OnCreate)

// 0x100423d0 (vslot 75) - the button: ..\test_html\<the name typed>
INT NCTestHtmlViewer::OnLoad()
{
    TCHAR Path[256];
    const TCHAR* Name = FileEdit->CommitText();
    if (Name && *Name)
    {
        Path[0] = 0;
        appSprintf(Path, TEXT("..\\test_html\\%s"), Name);
        LoadFile(Path);
    }
    return 1;
}
NW_HOOK_METHOD(0x100423d0, NCTestHtmlViewer, OnLoad)

NW_CHECK_VTABLE(NCTestHtmlViewer, 0x101a9080, 76)
