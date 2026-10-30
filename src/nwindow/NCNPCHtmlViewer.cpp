/*=============================================================================
    NCNPCHtmlViewer.cpp - the NPC dialog window. Stock: 0x10026b30 .. 0x10026ba1,
    0x100276e0 .. 0x100277a8, 0x10029770 .. 0x100298cc; WindowProc and OnClose sit with
    NCTutorialViewerWnd (0x100bcd30, 0x100bcda0: one body for both classes), OnDestroy is a
    body shared with other windows, LoadFile sits with NCTestHtmlViewer's (0x1002c640).
=============================================================================*/
#include "nwindow/NCNPCHtmlViewer.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"

// title bar id (name ours)
enum { NID_NPCHTML_FRAME = (INT)0xF0F01412 };

// 0x1003f4f0 - NCWnd's constructor, the vtable, no background yet
NCNPCHtmlViewer::NCNPCHtmlViewer()
{
    BackTex = NULL;
}
NW_HOOK_CTOR(0x1003f4f0, NCNPCHtmlViewer)

// 0x1003ecf0
NW_HOOK_DTOR(0x1003ecf0, NCNPCHtmlViewer)

// 0x1011e960 (vslot 25) - also NCTutorialViewerWnd's; C2: no enabled test
INT NCNPCHtmlViewer::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == (UINT)NWM_HTML_CLOSE)
        return OnClose(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1011e960, NCNPCHtmlViewer, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1003f510 (vslot 37) - the title bar (20 px high, the window's width) and the viewer 7 px in, 30 under the top,
// 14 narrower and 37 shorter than the window. C3: the size comes from GetLocalRect
INT NCNPCHtmlViewer::OnCreate()
{
    NCWnd::OnCreate();
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    GetLocalRect(&R);

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_NPCHTML_FRAME;
    FrameCtrl->ID = NID_NPCHTML_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)R.W, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    BackTex = FindTexture(TEXT("L2UI.etcwnd.NPCHtml_BACK"));

    HtmlViewer = new NCHtmlViewer;
    HtmlViewer->SetPosition(7, 30, (INT)(Width - 14.0f), (INT)(Height - 37.0f));
    CreateChild(HtmlViewer, NWS_VISIBLE);
    HtmlViewer->SetOwner(Entry);
    return 1;
}
NW_HOOK_METHOD(0x1003f510, NCNPCHtmlViewer, OnCreate)

// 0x1003c790 (vslot 38) - guarded in C2
INT NCNPCHtmlViewer::OnDestroy()
{
    guard(NCNPCHtmlViewer::OnDestroy);
    if (HtmlViewer)
    {
        RemoveChildWindow(HtmlViewer->Entry);
        delete HtmlViewer;
        HtmlViewer = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1003c790, NCNPCHtmlViewer, OnDestroy)

// 0x1003c840 (vslot 62) - the background under the title bar (20 px), the texture's own size; the clip goes on
// after the origin
INT NCNPCHtmlViewer::OnPaint(NCGDevice* Device)
{
    guard(NCNPCHtmlViewer::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f),
                            BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1003c840, NCNPCHtmlViewer, OnPaint)

// 0x1003b600 (vslot 74) - a page command closed the dialog; also NCTutorialViewerWnd's
INT NCNPCHtmlViewer::OnClose(WPARAM wParam, LPARAM lParam)
{
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x1003b600, NCNPCHtmlViewer, OnClose)

// 0x100423b0 - an empty name leaves the page as it is
void NCNPCHtmlViewer::LoadFile(const TCHAR* FileName)
{
    if (FileName && *FileName)
        HtmlViewer->LoadHtmlFile((TCHAR*)FileName);
}
NW_HOOK_METHOD(0x100423b0, NCNPCHtmlViewer, LoadFile)

// the stock body is NCTutorialViewerWnd's (0x100de3d0): an empty text leaves the page as it is
void NCNPCHtmlViewer::ShowPage(TCHAR* Text)
{
    if (Text && *Text)
        HtmlViewer->LoadHtml(Text);
}

NW_CHECK_VTABLE(NCNPCHtmlViewer, 0x101a8b70, 75)
