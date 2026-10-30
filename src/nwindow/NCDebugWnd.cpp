/*=============================================================================
    NCDebugWnd.cpp - debug log list. Stock: 0x100576d0 .. 0x10057b0d.
=============================================================================*/
#include "nwindow/NCDebugWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"

// 0x10084af0
NCDebugWnd::NCDebugWnd()
    : NCListBox(50, 10, 16)
{
}
NW_HOOK_CTOR(0x10084af0, NCDebugWnd)

// 0x10084b90
NCDebugWnd::NCDebugWnd(INT InMaxLines, INT InVisibleLines)
    : NCListBox(InMaxLines, InVisibleLines, 16)
{
}
NW_HOOK_CTOR_ARGS(0x10084b90, NCDebugWnd, (INT A, INT B), (A, B))

// 0x10084b40
NW_HOOK_DTOR(0x10084b40, NCDebugWnd)

// vslot 25 - the stock body (0x100dc140) is shared with NCChatListBox's (folded, hooked there): a plain
// forward to the list box (no enabled check, unlike the other WindowProcs)
INT NCDebugWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10084760 (vslot 37) - NCListBox's OnCreate and 1 (one body for several classes)
INT NCDebugWnd::OnCreate()
{
    NCListBox::OnCreate();
    return 1;
}
NW_HOOK_METHOD(0x10084760, NCDebugWnd, OnCreate)

// 0x100849f0 (vslot 38) - removes the scroll controls itself, then NCListBox does it again
// (finding them gone)
INT NCDebugWnd::OnDestroy()
{
    guard(NCDebugWnd::OnDestroy);
    if (ScrollUpButton)
    {
        RemoveChildWindow(ScrollUpButton->Entry);
        delete ScrollUpButton;
        ScrollUpButton = NULL;
    }
    if (ScrollDownButton)
    {
        RemoveChildWindow(ScrollDownButton->Entry);
        delete ScrollDownButton;
        ScrollDownButton = NULL;
    }
    if (ScrollBar)
    {
        RemoveChildWindow(ScrollBar->Entry);
        delete ScrollBar;
        ScrollBar = NULL;
    }
    NCListBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100849f0, NCDebugWnd, OnDestroy)

// 0x10084770 (vslot 62)
INT NCDebugWnd::OnPaint(NCGDevice* Device)
{
    NCListBox::OnPaint(Device);
    return 1;
}
NW_HOOK_METHOD(0x10084770, NCDebugWnd, OnPaint)

// 0x10084790 - C4: FindTexture takes a second argument (1)
void NCDebugWnd::SetBackTexture(const TCHAR* Name)
{
    if (Name)
        BackTex = FindTexture(Name, 1);
    else
        BackTex = FindTexture(TEXT("NWindow.BlackTexture"), 1);
}
NW_HOOK_METHOD(0x10084790, NCDebugWnd, SetBackTexture)

// 0x100847d0 - "[category]text" in the category color, when the category is enabled
void NCDebugWnd::AddLog(INT Category, const TCHAR* Text, INT Unused)
{
    TCHAR Temp[0x1F74 / sizeof(TCHAR)];
    if (Category < 0 || (GNumDebugCategories > 0 && Category > GNumDebugCategories))
        return;
    NDebugCategory& Cat = GDebugCategories[Category];
    if (Cat.bEnabled)
    {
        appSprintf(Temp, TEXT("[%s]%s"), Cat.Name, Text);
        AddString(Temp, Cat.Color, -1);
    }
}
NW_HOOK_METHOD(0x100847d0, NCDebugWnd, AddLog)

// 0x10084850 - does not use this. The name is copied with the ini value as the format string
// and the color's alpha byte is left unset, as in the stock code.
void NCDebugWnd::LoadCategories()
{
    TCHAR Ini[] = TEXT(".\\L2Debug.ini");
    if (!GConfig->GetInt(TEXT("DebugInfo"), TEXT("count"), GNumDebugCategories, Ini))
        GNumDebugCategories = 0;
    for (INT i = 0; i < GNumDebugCategories; i++)
    {
        TCHAR Section[64], Key[64], Value[1024];
        NDebugCategory& Cat = GDebugCategories[i];
        appSprintf(Section, TEXT("Library%d"), i);
        appSprintf(Key, TEXT("name%d"), i);
        appSprintf(Cat.Name, GConfig->GetStr(Section, Key, Ini));
        appSprintf(Key, TEXT("color%d"), i);
        if (GConfig->GetString(Section, Key, Value, 1024, Ini))
        {
            FColor Color;
            if (!Parse(Value, TEXT("R="), Color.R))
                Color.R = 0xFF;
            if (!Parse(Value, TEXT("G="), Color.G))
                Color.G = 0xFF;
            if (!Parse(Value, TEXT("B="), Color.B))
                Color.B = 0xFF;
            Cat.Color = Color.DWColor();
        }
        appSprintf(Key, TEXT("IsEnable%d"), i);
        if (!GConfig->GetBool(Section, Key, Cat.bEnabled, Ini))
            Cat.bEnabled = 0;
    }
}
NW_HOOK_METHOD(0x10084850, NCDebugWnd, LoadCategories)

NW_CHECK_VTABLE(NCDebugWnd, 0x101b2ed0, 82)
