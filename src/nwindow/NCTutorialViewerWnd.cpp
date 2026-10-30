/*=============================================================================
    NCTutorialViewerWnd.cpp - the tutorial window and its question mark. Stock: 0x100bcdb0 ..
    0x100bd0dc, 0x100bd120 .. 0x100bd339 (WindowProc, OnClose and OnDestroy are bodies the
    linker folded: NCNPCHtmlViewer hooks the first two).
=============================================================================*/
#include "nwindow/NCTutorialViewerWnd.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// title bar id, blink timer (names ours)
enum
{
    NID_TUTORIAL_FRAME = (INT)0xFFF00001,
    TIMER_TutorialBlink = 0xFA0D06,
};

/*-----------------------------------------------------------------------------
    NCTutorialViewerWnd.
-----------------------------------------------------------------------------*/

// 0x1011f980 - NCWnd's, nothing more (the pointers stay as allocated)
NCTutorialViewerWnd::NCTutorialViewerWnd()
{
}
NW_HOOK_CTOR(0x1011f980, NCTutorialViewerWnd)

// 0x1011f9d0
NW_HOOK_DTOR(0x1011f9d0, NCTutorialViewerWnd)


// 0x1002fd40 (vslot 25) - one body with NCNPCHtmlViewer's (hooked there); C2: no enabled test
INT NCTutorialViewerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == (UINT)NWM_HTML_CLOSE)
        return OnClose(wParam, lParam) != 0;
    return 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1011f430 (vslot 37) - the title bar (20 high, the window's width; frame id 0xFFF00001), the three background
// textures and the page viewer 8 in, 32 under the top: 15 narrower and 41 shorter than the window (the local
// rectangle's width, the window's height)
INT NCTutorialViewerWnd::OnCreate()
{
    NCWnd::OnCreate();
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    GetLocalRect(&R);

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_TUTORIAL_FRAME;
    FrameCtrl->ID = NID_TUTORIAL_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    TopTex = FindTexture(TEXT("l2ui_ch3.tutorialwnd.tutorial_back1"));
    MidTex = FindTexture(TEXT("l2ui_ch3.tutorialwnd.tutorial_back2"));
    BottomTex = FindTexture(TEXT("l2ui_ch3.tutorialwnd.tutorial_back3"));

    HtmlViewer = new NCTutorialHtmlViewer;
    HtmlViewer->SetPosition(8, 32, (INT)(R.W - 8.0f - 7.0f), (INT)(Height - 12.0f - 9.0f - 20.0f));
    CreateChild(HtmlViewer, NWS_VISIBLE);
    HtmlViewer->SetOwner(Entry);
    return 1;
}
NW_HOOK_METHOD(0x1011f430, NCTutorialViewerWnd, OnCreate)

// 0x1011ecc0 (vslot 38) - guarded in C2
INT NCTutorialViewerWnd::OnDestroy()
{
    guard(NCTutorialViewerWnd::OnDestroy);
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
NW_HOOK_METHOD(0x1011ecc0, NCTutorialViewerWnd, OnDestroy)

// 0x1011ed70 (vslot 62) - the top strip (12 high, 20 below the top), the middle one repeated 12 pixels at a time down
// to the bottom strip (and the rest of it), the bottom strip (9 high)
INT NCTutorialViewerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTutorialViewerWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (TopTex)
        Device->DrawTexture(0, 20, (INT)Width, 12, 0, 0, (INT)Width, 12, TopTex, 0xFF, 1);
    INT Count = 0;
    while (Count < (INT)(Height - 20.0f - 12.0f - 9.0f) / 12)
    {
        if (MidTex)
            Device->DrawTexture(0, Count * 12 + 32, (INT)Width, 12, 0, 0, (INT)Width, 12, MidTex, 0xFF, 1);
        Count++;
    }
    if (MidTex)
    {
        INT Rest = (INT)(Height - 20.0f - 12.0f - 9.0f - (FLOAT)(Count * 12));
        Device->DrawTexture(0, Count * 12 + 32, (INT)Width, Rest, 0, 0, (INT)Width, Rest, MidTex, 0xFF, 1);
    }
    if (BottomTex)
        Device->DrawTexture(0, (INT)(Height - 9.0f), (INT)Width, 9, 0, 0, (INT)Width, 9, BottomTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011ed70, NCTutorialViewerWnd, OnPaint)

// 0x1002fda0 (vslot 67) - the stock body is NCNPCHtmlViewer's (hooked there): a page command closed the dialog
INT NCTutorialViewerWnd::OnClose(WPARAM wParam, LPARAM lParam)
{
    HideWindow();
    return 1;
}

// 0x1011e9c0 - an empty text leaves the page as it is
void NCTutorialViewerWnd::ShowPage(TCHAR* Text)
{
    if (Text && *Text)
        HtmlViewer->LoadHtml(Text);
}
NW_HOOK_METHOD(0x1011e9c0, NCTutorialViewerWnd, ShowPage)

// 0x1011e9e0 (name ours) - the window fits the page: 310 wide, as high as the page (256 .. 672) and 8 more, the viewer
// inside as large as the window less the frame (called by NCHtmlViewer::LoadHtml of the viewer it holds)
void NCTutorialViewerWnd::FitToPage(INT ContentHeight)
{
    INT Fit = ContentHeight;
    if (Fit < 0x100)
        Fit = 0x100;
    else if (Fit > 0x2A0)
        Fit = 0x2A0;
    SetWindowSize(310.0f, (FLOAT)(Fit + 8));
    HtmlViewer->SetSize((DWORD)(INT)(Width - 8.0f - 7.0f), (DWORD)(INT)(Height - 12.0f - 9.0f - 20.0f));
}
NW_HOOK_METHOD(0x1011e9e0, NCTutorialViewerWnd, FitToPage)

NW_CHECK_VTABLE(NCTutorialViewerWnd, 0x101d0660, 75)

/*-----------------------------------------------------------------------------
    NCTutorialQuestionWnd.
-----------------------------------------------------------------------------*/




// stock body shared with NControl's (0x10004e90)
#if 0  // C4: a method of NCTutorialQuestionWnd, a class this client does not have
INT NCTutorialQuestionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg != WM_PAINT && !IsEnabled())
        return 0;
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif






// C2: no vtable for NW_CHECK_VTABLE(NCTutorialQuestionWnd, 0x101a20e8, 68)
