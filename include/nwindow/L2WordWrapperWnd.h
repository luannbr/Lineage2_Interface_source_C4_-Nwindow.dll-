#pragma once
// L2WordWrapperWnd - the developers' word wrap converter (UClass "L2WordWrapperWnd", 0xF8; NConsoleWnd::Initialize makes
// it, at NConsoleWnd +0x398C): a combo box (Npc / Petition / Help / ServerInfo HTML), a "Generate!" button and a count
// of the files converted. It talks to a "WordWrap Server" on localhost:7778 (the Thai text is wrapped by the server) and
// tells the message window whether the connection worked. The Generate button converts the Thai HTML pages of
// ..\INPUT_THAI_HTML into ..\OUTPUT_THAI_HTML (the line breaks of the server's wrapping marked "<br1>", font and link
// tags closed and opened again around them); in Thai mode the HTML layout (NHtmlBlock) asks WrapText for the same breaks
// of every line of a page.
//
// C3 layout from the stock constructor 0x1003CC80, destructor 0x1003CD10 and OnCreate 0x1003C9D0.

#include "nwindow/NCWnd.h"
#include "nwindow/NStl.h"
#pragma pack(push, 4)

class NCButton;
class NCComboBox;

class L2WordWrapperWnd : public NCWnd
{
    NW_DECLARE_CLASS(L2WordWrapperWnd, NCWnd, 0, 0x102CA3E0)

    NCComboBox* PageCombo;           // +0xE8 (name ours; the constructor leaves it) id 0xFF330002, 125 x 17 at (0, 24): which kind of page
    NCButton* GenerateButton;        // +0xEC id 0xFF330001, text "Generate!", 67 x 19 at (0, 0)
    INT ConvertedCount;              // +0xF0 the files OnGenerate converted (name ours)
    UINT_PTR Socket;                 // +0xF4 the connection to the server (name ours; 0 = none, -1 after a failed socket())
    NStlWString Document;            // +0xF8 a std::wstring: the page being converted (name ours)
    UINT WrapCursor;                 // +0x114 where WrapText goes on in Document (name ours; 0 at the start of a page)

    L2WordWrapperWnd();                                              // 0x10042940
    // ~L2WordWrapperWnd                                             // 0x100429d0 (the string freed in line)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10043ab0
    virtual INT OnCreate();                                          // 37 0x10042680
    virtual INT OnDestroy();                                         // 38 0x100425c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10042600

    // names ours
    BYTE ConnectServer(const char* Host, INT Port);                  // 0x10042450 (anonymous in the db) (ret 8) 1 = connected
    BYTE SendRequest(INT Len, const void* Request, void* Reply);     // 0x10042520 (ret 0xC) 1 = the answer is in (a 4 KB buffer)
    void ClearWrapCursor();                                          // 0x10042670 (anonymous in the db) WrapCursor = 0
    void WriteOutputFile(const TCHAR* Name);                         // 0x10042890 (anonymous in the db) (ret 4) Document into ..\OUTPUT_THAI_HTML\Name
    void WrapText(TCHAR* Text, INT Width);                           // 0x10042df0 (ret 8) the server's line breaks of Text into Document
    void ConvertDocument();                                          // 0x10043200 (anonymous in the db) close and open the tags around the breaks
    INT LoadInputFile(const TCHAR* Path);                            // 0x100436c0 (anonymous in the db) (ret 4) 1 = Document is the file's text
    INT OnGenerate();                                                // 0x10043840 the button: converts the files
};
NW_ASSERT_OFFSET(L2WordWrapperWnd, PageCombo, 0xE8);
NW_ASSERT_OFFSET(L2WordWrapperWnd, GenerateButton, 0xEC);
NW_ASSERT_OFFSET(L2WordWrapperWnd, ConvertedCount, 0xF0);
NW_ASSERT_OFFSET(L2WordWrapperWnd, Socket, 0xF4);
NW_ASSERT_OFFSET(L2WordWrapperWnd, Document, 0xF8);
NW_ASSERT_OFFSET(L2WordWrapperWnd, WrapCursor, 0x114);
NW_ASSERT_SIZE(L2WordWrapperWnd, 0x118);

#pragma pack(pop)
