/*=============================================================================
    NCHtmlEdit.cpp - NCHtmlEdit and NCJHtmlEdit, the edit boxes of the HTML pages.
    Stock: 0x10030160 .. 0x100301e7, 0x100310a0 .. 0x100310b7, 0x100326e0 .. 0x10032f9f
    (WindowProc, OnKeyDown and GetValue are one body for both classes).
=============================================================================*/
#include "nwindow/NCHtmlEdit.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "re/NwDebug.h"

// the code a click sends the viewer with the box's y: MAKELONG(100, LocalY)
enum { HTML_SCROLLTO = 100 };

/*-----------------------------------------------------------------------------
    NCHtmlEdit.
-----------------------------------------------------------------------------*/

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1004aee0 - NCEditBox(0, NULL, 1, 0, 0), then the box's Type, no Enter command, the back texture when the base
// loaded none, and the extra width when there is one
NCHtmlEdit::NCHtmlEdit(INT InExtraWidth, INT InType) : NCEditBox(0, NULL, 1, 0, 0)
{
    Type = InType;
    EnterCommand = NULL;
    if (!BackTex)
        BackTex = FindTexture(TEXT("L2UI.EtcWnd.Edit_Back"));
    if (InExtraWidth > 0)
        ExtraWidth = InExtraWidth;
}
NW_HOOK_CTOR_ARGS(0x1004aee0, NCHtmlEdit, (INT A, INT B), (A, B))

// 0x1004afa0 - the implicit destructor
NW_HOOK_DTOR(0x1004afa0, NCHtmlEdit)


// 0x1011ea70 (vslot 25) - NCEditBox's answer as a plain 0 / 1. One stock body for NCHtmlEdit, NCJHtmlEdit and NCCalcEditCtrl
// (the folded copies); hooked here
INT NCHtmlEdit::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCEditBox::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x1011ea70, NCHtmlEdit, WindowProc)

// 0x100498a0 (vslot 56) - NCEditBox::OnPaint (the frame textures, the macro highlight; no number colour), clipped
// to the viewer's height (its own width); the origin goes on before the clip
INT NCHtmlEdit::OnPaint(NCGDevice* Device)
{
    TCHAR Stars[258];
    SIZE Size;
    INT TWClauseW = 0;
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Owner = FirstNonControlParent(this);
    if (Owner)
        Device->SetClip(ScreenX, Owner->ScreenY, (INT)Width, (INT)Owner->Height);
    if (IsFocused())
    {
        if (Edit1)
            Device->DrawStretchedTex(0, 0, 8, (INT)Height, 0, 0, 8, 17, Edit1);
        if (Edit2)
            Device->DrawStretchedTex(8, 0, (INT)(Width - 16.0f), (INT)Height, 0, 0, 8, 17, Edit2);
        if (Edit3)
            Device->DrawStretchedTex((INT)(Width - 8.0f), 0, 8, (INT)Height, 0, 0, 8, 17, Edit3);
    }
    else
    {
        if (EditDisable1)
            Device->DrawStretchedTex(0, 0, 8, (INT)Height, 0, 0, 8, 17, EditDisable1);
        if (EditDisable2)
            Device->DrawStretchedTex(8, 0, (INT)(Width - 16.0f), (INT)Height, 0, 0, 8, 17, EditDisable2);
        if (EditDisable3)
            Device->DrawStretchedTex((INT)(Width - 8.0f), 0, 8, (INT)Height, 0, 0, 8, 17, EditDisable3);
    }
    if (Unknown1D8 != 0 && HighlightTex != NULL)
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, 0, 0, 0xC1, 16, HighlightTex);

    if (ExtraWidth > 0)
        MaxWidth = (INT)(Width + ExtraWidth);
    else
        MaxWidth = (INT)Width;

    NCGDevice::GetTextExtent(&Size, *Left, 0, 0, 0, 0, 0, L2FONT_Normal);
    INT TextW = Size.cx;
    NCGDevice::GetTextExtent(&Size, *Comp, 0, 0, 0, 0, 0, L2FONT_Normal);
    TextW += Size.cx;
    NCGDevice::GetTextExtent(&Size, *Right, 0, 0, 0, 0, 0, L2FONT_Normal);
    TextW += Size.cx;
    if (MaxLen == 0)
        bCanInsert = TextW < MaxWidth - 12;

    // proof mode, not stock (re/NwDebug.h): a red caret
    const DWORD CaretColor = NW_DEBUG_MARKER() ? 0xFFFF0000 : 0xFFDCDCDC;

    if (Type == 2)
    {
        INT i;
        for (i = 0; i < Left.Len(); i++)
            Stars[i] = '*';
        Stars[i] = 0;
        TextW = Device->DrawText(2, 2, 0xFFDCDCDC, Stars, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    else
    {
        TextW = 0;
        UBOOL bFull = 0;
        if (ViewStart < Left.Len())
        {
            for (INT i = ViewStart; i < Left.Len(); i++)
            {
                NCGDevice::GetTextExtent(&Size, *Left.Mid(i, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                TextW += Size.cx;
                if (TextW >= (INT)Width)
                {
                    bFull = 1;
                    break;
                }
            }
        }
        if (!bFull)
        {
            NCGDevice::GetTextExtent(&Size, *Comp, 0, 0, 0, 0, 0, L2FONT_Normal);
            TextW += Size.cx;
            if (TextW < (INT)Width)
            {
                for (INT i = 0; i < Right.Len(); i++)
                {
                    NCGDevice::GetTextExtent(&Size, *Right.Mid(i, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                    TextW += Size.cx;
                    if (TextW >= (INT)Width)
                        break;
                }
            }
        }
        if (ViewStart < Left.Len())
        {
            TextW = Device->DrawText(2, 2, 0xFFDCDCDC, *Left + ViewStart, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            if (Left.Len() > 0)
                TextW = GetTextWidth(FString(*Left + ViewStart));
        }
        else
            TextW = 0;
    }

    NCGDevice::GetTextExtent(&Size, *Comp, 0, 0, 0, 0, 0, L2FONT_Normal);
    INT CompW = Size.cx;
    NCGDevice::GetTextExtent(&Size, *Reading, 0, 0, 0, 0, 0, L2FONT_Normal);
    DWORD ReadingW = Size.cx;
    if (ReadingW > 0 && GConsole->GetIMEType() == 6)
        ReadingW = 12;

    if (Type != 2)
    {
        if (GLanguageType == 2)
            PaintCompositionJP(2, 2, TextW, TWClauseW, Device);
        else if (GLanguageType == 0)
            PaintCompositionKR(2, 2, TextW, TWClauseW, Device);
        else if (GLanguageType == 3)
            PaintCompositionTW(2, 2, TextW, &TWClauseW, Device);
        else if (GLanguageType == 4)
            PaintCompositionCN(2, 2, TextW, Device);
    }
    if (Type == 2)
    {
        INT i;
        for (i = 0; i < Right.Len(); i++)
            Stars[i] = '*';
        Stars[i] = 0;
        NCGDevice::GetTextExtent(&Size, *Comp, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(TextW + ReadingW + Size.cx + 2, 2, 0xFFDCDCDC, Stars, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    }
    else
    {
        // Chinese layouts draw the composition in their own window
        if (GConsole->GetIMEType() == 10 || GConsole->GetIMEType() == 9)
            Size.cx = 0;
        else
            NCGDevice::GetTextExtent(&Size, *Comp, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(TextW + ReadingW + Size.cx + 2, 2, 0xFFDCDCDC, *Right, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    }

    if (IMEManager->IsNotComposing() == 0)
    {
        if (IsFocused() && GLanguageType == 0)
            PaintComposingKR(2, 2, TextW, CompW, Device);
        else if (IsFocused() && GLanguageType == 2)
            PaintComposingJP(2, 2, TextW, CompW, Device);
        else if (IsFocused() && GLanguageType == 3)
            PaintComposingTW(2, 2, TextW, CompW, TWClauseW, ReadingW, Device);
        else if (IsFocused() && GLanguageType == 4)
            PaintComposingCN(2, 2, TextW, CompW, Device);
        if (IsFocused() && CompW == 0 && Unknown1CC != 0 && bCaretOn)
            Device->DrawText(TextW - 1, 2, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if ((DWORD)(CompW + TextW) > Width)
            ViewStart++;
    }
    else
    {
        // the caret stays inside the box: past the right edge the view scrolls one character (cluster)
        CaretX = TextW + CompW;
        if (CaretX == Width)
            CaretX--;
        else if (CaretX > Width)
        {
            if (GLanguageType == 5)
            {
                INT Cluster = IMEManager->GetTextLength(Left.Mid(0, ViewStart));
                ViewStart += ThaiLeft(Cluster).CharCount();
            }
            else
                ViewStart++;
            ViewStart2 = ViewStart;
            CaretX = (INT)(Width - 1);
        }
        if (Unknown1CC != 0 && IsFocused() && bCaretOn)
            Device->DrawText(Type == 2 ? CaretX : CaretX - 1, 2, CaretColor, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0,
                             0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100498a0, NCHtmlEdit, OnPaint)


// 0x10047600 (vslot 65) - Enter with a command sends it; anything else is NCEditBox's
INT NCHtmlEdit::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN && EnterCommand)
    {
        NotifyParent((LPARAM)EnterCommand);
        return 1;
    }
    NCEditBox::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x10047600, NCHtmlEdit, OnKeyDown)

// 0x10047f00 (vslot 68) - the viewer scrolls the box into view (NCHtmlMultiEdit's is the same stock body)
INT NCHtmlEdit::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);
    NotifyParent(((DWORD)(_WORD)Local.Y << 16) | HTML_SCROLLTO);
    return 1;
}
NW_HOOK_METHOD(0x10047f00, NCHtmlEdit, OnLButtonDown)

// 0x100475f0 (vslot 74) - the stock body is a jump to the virtual at slot 75 (CommitText), shared with NCJHtmlEdit's and other classes'
const TCHAR* NCHtmlEdit::GetValue()
{
    return CommitText();
}

NW_CHECK_VTABLE(NCHtmlEdit, 0x101aa3c8, 87)

/*-----------------------------------------------------------------------------
    NCJHtmlEdit.
-----------------------------------------------------------------------------*/

// 0x1004aff0 - NCEditBox(0, NULL, 1, 0, 0) and no Enter command
NCJHtmlEdit::NCJHtmlEdit(INT Unused1, INT Unused2, INT Unused3) : NCEditBox(0, NULL, 1, 0, 0)
{
    EnterCommand = NULL;
}
NW_HOOK_CTOR_ARGS(0x1004aff0, NCJHtmlEdit, (INT A, INT B, INT C), (A, B, C))

// 0x1004b050 - the implicit destructor
NW_HOOK_DTOR(0x1004b050, NCJHtmlEdit)



// stock body shared with NCHtmlEdit's (0x10109a40)
INT NCJHtmlEdit::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCEditBox::WindowProc(Msg, wParam, lParam) != 0;
}

// stock body shared with NCHtmlEdit's (0x10041720)
INT NCJHtmlEdit::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN && EnterCommand)
    {
        NotifyParent((LPARAM)EnterCommand);
        return 1;
    }
    NCEditBox::OnKeyDown(wParam, lParam);
    return 1;
}

// stock body shared with NCHtmlEdit's (0x100faac0)
const TCHAR* NCJHtmlEdit::GetValue()
{
    return CommitText();
}

NW_CHECK_VTABLE(NCJHtmlEdit, 0x101aa528, 87)
