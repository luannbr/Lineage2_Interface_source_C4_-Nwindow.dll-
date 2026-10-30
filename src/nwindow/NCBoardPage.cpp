/*=============================================================================
    NCBoardPage.cpp - the community board's page as the console hands it over: pieces
    "key\bvalue" gathered in NCBBSHtmlViewer and assembled into the page it shows. Stock:
    0x100264c0 .. 0x1002650d, 0x100269a0 .. 0x10026b20, 0x1002be70 .. 0x1002c6d9 (the rest of
    both classes is in NCBoardWnd.cpp).

    A post comes as "1" (the page, with <!--FIELD--> marks), "2" (the fields, separated by
    '\b', in GBoardMarks order), "3" (the comments) and "4" (the thread); a write form as
    "1001" and "1002" (its title and content go into the page's edit boxes); a long page as
    "101".."103". Anything starting with '<' is shown as it is.
=============================================================================*/
#include "nwindow/NCBoardWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCHtmlEdit.h"
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NControl.h"
#include "nwindow/NStl.h"

#include <string.h>
#include <wchar.h>

// the stock code calls the CRT's string functions; the intrinsics would expand inline
#pragma function(wcscpy, wcscat, wcscmp, wcslen)

// the marks of the post fields, in the order of the "2" list, then <!--COMMENT--> and
// <!--THREAD--> (stock table, name ours). C3 has one more field than C2 (<!--WRITER-->, between the account and the
// title); a write form takes the first 17 marks (up to <!--STATUS-->), a post 18 (up to <!--IP-->)
#if NW_HOOK_MODE
#define GBoardMarks (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10235340)))
#else
extern const TCHAR* const GBoardMarks[];
#endif
enum
{
    BOARDMARK_NumPostFields = 18,      // <!--ROOT_BID--> .. <!--IP-->
    BOARDMARK_NumFormFields = 17,      // <!--ROOT_BID--> .. <!--STATUS-->
    BOARDMARK_Comment = 18,
    BOARDMARK_Thread = 19,
};

// where Mark is in the page (negative when it is not: the pointer difference of the stock code)
static FORCEINLINE INT FindMark(NStlWString& Page, const TCHAR* Mark)
{
    return wcsstr(Page.CStr(), Mark) - Page.CStr();
}

// 0x1003b450 - the next field of a list separated by '\b': skips the separators, sets *OutField
// to the field (left as it was when there is none), ends the field and returns where the next
// one is looked for
static TCHAR* NextBoardField(TCHAR* Str, TCHAR** OutField)
{
    TCHAR* P = Str;
    while (*P == 8)
        P++;
    if (*P)
    {
        *OutField = P;
        if (*P)
        {
            while (*P != 8)
            {
                P++;
                if (!*P)
                    break;
            }
            if (*P)
            {
                *P = 0;
                P++;
            }
        }
    }
    return P;
}
NW_HOOK_FUNC(0x1003b450, NextBoardField)

/*-----------------------------------------------------------------------------
    NCBBSHtmlViewer.
-----------------------------------------------------------------------------*/

// 0x10041b20 - the commands are unlocked by a new page
INT NCBBSHtmlViewer::ShowPage(TCHAR* Text)
{
    bLocked = 0;
    return LoadHtml(Text);
}
NW_HOOK_METHOD(0x10041b20, NCBBSHtmlViewer, ShowPage)

// 0x10041b30 - each mark found past the start of the page is replaced by its field; once the list runs out the last
// field is used again (sic). (A copy of the C2 body: only the field count and the offsets moved.)
void NCBBSHtmlViewer::BuildPostPage()
{
    NStlWString Page;
    Page.Assign(PostTemplate, wcslen(PostTemplate));
    TCHAR* Field = NULL;
    TCHAR* Rest = NextBoardField(PostFields, &Field);
    if (Field)
    {
        for (const TCHAR* const* Mark = GBoardMarks; Mark < GBoardMarks + BOARDMARK_NumPostFields;)
        {
            INT Pos = FindMark(Page, *Mark);
            if (Pos > 0)
                Page.Replace(Pos, wcslen(*Mark), Field, wcslen(Field));
            Rest = NextBoardField(Rest, &Field);
            Mark++;
            if (!Field)
                break;
        }
    }
    INT Pos = FindMark(Page, GBoardMarks[BOARDMARK_Comment]);
    if (Pos > 0)
        Page.Replace(Pos, wcslen(GBoardMarks[BOARDMARK_Comment]), PostComments, wcslen(PostComments));
    Pos = FindMark(Page, GBoardMarks[BOARDMARK_Thread]);
    if (Pos > 0)
        Page.Replace(Pos, wcslen(GBoardMarks[BOARDMARK_Thread]), PostThread, wcslen(PostThread));
    ShowPage(Page.CStr());
    TCHAR* It;
    Page.Erase(&It, Page.CStr(), Page.CStr() + Page.Size);
    NumPostPieces = 0;
    PostTemplate[0] = 0;
    PostFields[0] = 0;
    PostComments[0] = 0;
    PostThread[0] = 0;
}
NW_HOOK_METHOD(0x10041b30, NCBBSHtmlViewer, BuildPostPage)

// 0x10041dc0 - the writer, title and content fields (" " = empty) go into the page's one-line boxes (added to their text:
// the writer to the first, the title to the ones after it) and multi-line boxes, the other fields replace their marks.
// The stock code sets the three buffers only when their mark comes up: with no field at all ("1002" alone) it tests stack
// garbage; empty here. (With fewer fields than marks the last field is reused, so all are always set.)
void NCBBSHtmlViewer::BuildFormPage()
{
    TCHAR Writer[0x2000];
    TCHAR Title[0x2000];
    TCHAR Content[0x2000];
    Writer[0] = 0;
    Title[0] = 0;
    Content[0] = 0;
    TCHAR* Template = FormTemplate;
    TCHAR* Fields = FormFields;
    NStlWString Page;
    Page.Assign(Template, wcslen(Template));
    TCHAR* Field = NULL;
    TCHAR* Rest = NextBoardField(Fields, &Field);
    if (Field)
    {
        for (const TCHAR* const* Mark = GBoardMarks; Mark < GBoardMarks + BOARDMARK_NumFormFields;)
        {
            TCHAR* Found = wcsstr(Page.CStr(), *Mark);
            if (!wcscmp(*Mark, TEXT("<!--TITLE-->")))
            {
                if (Field && wcscmp(Field, TEXT(" ")))
                    wcscpy(Title, Field);
                else
                    Title[0] = 0;
            }
            else if (!wcscmp(*Mark, TEXT("<!--CONTENT-->")))
            {
                if (Field && wcscmp(Field, TEXT(" ")))
                    wcscpy(Content, Field);
                else
                    Content[0] = 0;
            }
            else if (!wcscmp(*Mark, TEXT("<!--WRITER-->")))
            {
                if (Field && wcscmp(Field, TEXT(" ")))
                    wcscpy(Writer, Field);
                else
                    Writer[0] = 0;
            }
            else
            {
                INT Pos = Found - Page.CStr();
                if (Pos > 0)
                    Page.Replace(Pos, wcslen(*Mark), Field, wcslen(Field));
            }
            Rest = NextBoardField(Rest, &Field);
            Mark++;
            if (!Field)
                break;
        }
    }
    ShowPage(Page.CStr());
    INT i = 0;
    if (Writer[0] && Controls.Num() > 0)
    {
        for (; i < Controls.Num(); i++)
        {
            if (Controls(i)->IsA(NCHtmlEdit::StaticClass()))
            {
                ((NCHtmlEdit*)Controls(i))->Left += Writer;
                i++;
                break;
            }
        }
    }
    if (Title[0] && i < Controls.Num())
    {
        do
        {
            NControl* Control = Controls(i);
            if (Control->IsA(NCHtmlEdit::StaticClass()))
                ((NCHtmlEdit*)Control)->Left += Title;
            i++;
        } while (i < Controls.Num());
    }
    if (Content[0])
    {
        for (i = 0; i < Controls.Num(); i++)
        {
            NControl* Control = Controls(i);
            if (Control->IsA(NCHtmlMultiEdit::StaticClass()))
                ((NCHtmlMultiEdit*)Control)->SetText(Content);
        }
    }
    NumFormPieces = 0;
    *Template = 0;
    *Fields = 0;
}
NW_HOOK_METHOD(0x10041dc0, NCBBSHtmlViewer, BuildFormPage)

// 0x10042140 - a long page's three parts (the empty ones left out) shown as one
void NCBBSHtmlViewer::ShowPageParts()
{
    TCHAR Buf[0x6000];
    wcscpy(Buf, PageParts[0]);
    if (PageParts[1][0])
        wcscat(Buf, PageParts[1]);
    if (PageParts[2][0])
        wcscat(Buf, PageParts[2]);
    ShowPage(Buf);
    PageParts[2][0] = 0;
    NumPageParts = 0;
    PageParts[0][0] = 0;
    PageParts[1][0] = 0;
}
NW_HOOK_METHOD(0x10042140, NCBBSHtmlViewer, ShowPageParts)

// 0x100421d0 - a piece "key\bvalue" of the page being received; a page in three parts, a post (four pieces) or a write
// form (two) is shown as soon as all of them are in. C3 ignores an empty or missing text
void NCBBSHtmlViewer::AddPiece(TCHAR* Text)
{
    if (!Text || !Text[0])
        return;
    if (Text[0] == '<')
    {
        ShowPage(Text);
        PageKind = 0;
        return;
    }
    TCHAR* Key = Text;
    TCHAR* Value = NextBoardField(Text, &Key);
    if (!wcscmp(Key, TEXT("1")))
    {
        wcscpy(PostTemplate, Value);
        NumPostPieces++;
    }
    else if (!wcscmp(Key, TEXT("2")))
    {
        wcscpy(PostFields, Value);
        NumPostPieces++;
    }
    else if (!wcscmp(Key, TEXT("3")))
    {
        wcscpy(PostComments, Value);
        NumPostPieces++;
    }
    else if (!wcscmp(Key, TEXT("4")))
    {
        wcscpy(PostThread, Value);
        NumPostPieces++;
    }
    else if (!wcscmp(Key, TEXT("1001")))
    {
        wcscpy(FormTemplate, Value);
        NumFormPieces++;
    }
    else if (!wcscmp(Key, TEXT("1002")))
    {
        wcscpy(FormFields, Value);
        NumFormPieces++;
    }
    else if (!wcscmp(Key, TEXT("101")))
    {
        wcscpy(PageParts[0], Value);
        NumPageParts++;
    }
    else if (!wcscmp(Key, TEXT("102")))
    {
        wcscpy(PageParts[1], Value);
        NumPageParts++;
    }
    else if (!wcscmp(Key, TEXT("103")))
    {
        wcscpy(PageParts[2], Value);
        NumPageParts++;
    }
    if (NumPageParts == 3)
    {
        ShowPageParts();
        PageKind = 1;
    }
    if (NumPostPieces == 4)
    {
        BuildPostPage();
        PageKind = 2;
    }
    if (NumFormPieces == 2)
    {
        BuildFormPage();
        PageKind = 3;
    }
}
NW_HOOK_METHOD(0x100421d0, NCBBSHtmlViewer, AddPiece)

/*-----------------------------------------------------------------------------
    NCBoardWnd.
-----------------------------------------------------------------------------*/

// 0x1003b800 - C3: eight commands (the seven tabs' and the button's); the board is unlocked
void NCBoardWnd::SetCommands(TCHAR* Command1, TCHAR* Command2, TCHAR* Command3, TCHAR* Command4, TCHAR* Command5,
                             TCHAR* Command6, TCHAR* Command7, TCHAR* Command8)
{
    for (INT i = 0; i < 8; i++)
        Commands[i][0] = 0;
    appStrcpy(Commands[0], Command1);
    appStrcpy(Commands[1], Command2);
    appStrcpy(Commands[2], Command3);
    appStrcpy(Commands[3], Command4);
    appStrcpy(Commands[4], Command5);
    appStrcpy(Commands[5], Command6);
    appStrcpy(Commands[6], Command7);
    appStrcpy(Commands[7], Command8);
    bLocked = 0;
}
NW_HOOK_METHOD(0x1003b800, NCBoardWnd, SetCommands)

// 0x10042430 - a piece of the page into the viewer
void NCBoardWnd::LoadPage(TCHAR* Html)
{
    if (Html)
        HtmlViewer->AddPiece(Html);
}
NW_HOOK_METHOD(0x10042430, NCBoardWnd, LoadPage)
