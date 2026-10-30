#pragma once
// The page model of NCHtmlViewer (all names ours: no class of this module has a UClass or a
// guard string). NHtmlParser turns the text into a list of tags (NHtmlTag); the viewer builds
// a tree from them: NHtmlBlock stacks its children one under the other (the page, a table
// cell), NHtmlTable lays cells out in rows and columns, NHtmlItem is a leaf (a run of text, an
// image, a line break, or a control the viewer creates: link, button, edit, combo box). Each
// object sizes itself (Layout), places its children (Arrange) and draws them (Draw) relative to
// its parent.
//
// The parser and the item's strings use the VC 7.1 STL of the stock build (std::vector,
// std::wstring), mirrored without the STL in NStl.h.

#include "nwindow/NWindow.h"
#include "nwindow/NStl.h"

class NCHtmlViewer;
class NCGDevice;

// tag ids of NHtmlTag::Tag (the parser maps its alphabetical tag table to these)
enum
{
    HTMLTAG_Text = 1,          // text between tags (NHtmlTag::Text)
    HTMLTAG_Unknown = 2,
    HTMLTAG_Comment = 3,
    HTMLTAG_Extend = 4,
    HTMLTAG_Img = 5,
    HTMLTAG_Input = 6,
    HTMLTAG_Select = 7,
    HTMLTAG_Option = 8,
    HTMLTAG_TextArea = 9,
    HTMLTAG_Spin = 10,
    HTMLTAG_Bar = 11,
    HTMLTAG_Volumn = 12,
    HTMLTAG_Var = 13,
    HTMLTAG_A = 14,
    HTMLTAG_Br = 15,
    HTMLTAG_Left = 16,
    HTMLTAG_Right = 17,
    HTMLTAG_Center = 18,
    HTMLTAG_Font = 19,
    HTMLTAG_B = 20,
    HTMLTAG_I = 21,
    HTMLTAG_U = 22,
    HTMLTAG_TT = 23,
    HTMLTAG_Strike = 24,
    HTMLTAG_Sub = 25,
    HTMLTAG_Sup = 26,
    HTMLTAG_P = 27,
    HTMLTAG_H1 = 28,           // .. 34 H7
    HTMLTAG_Pre = 35,
    HTMLTAG_Address = 36,
    HTMLTAG_UL = 37,
    HTMLTAG_OL = 38,
    HTMLTAG_LI = 39,
    HTMLTAG_Table = 40,
    HTMLTAG_TR = 41,
    HTMLTAG_TD = 42,
    HTMLTAG_Html = 43,
    HTMLTAG_Body = 44,
    HTMLTAG_Head = 45,
    HTMLTAG_Title = 46,
    HTMLTAG_Button = 49,
    HTMLTAG_MultiEdit = 50,
    HTMLTAG_Edit = 51,
    HTMLTAG_ComboBox = 52,
    HTMLTAG_Br1 = 53,          // C2 (name ours): the new "BR1" tag
};

// attribute ids: 1-based index in the tag's sorted attribute list (the C4 table GHtmlTagInfo at 0x10234020, 12-byte records; FindTag
// 0x1003AAA0 bisects 0x33 = 51 of them; the C2 table was at 0x1015f3a8)
enum { ATTR_A_Action = 1, ATTR_A_Href = 2, ATTR_A_Unknown3 = 3, ATTR_A_Link = 4, ATTR_A_Msg = 5 };   // C3: a fifth attribute (3)
enum { ATTR_Html_ImgSrc = 2 };
enum { ATTR_Table_BgColor = 3, ATTR_Table_Border = 4, ATTR_Table_CellPadding = 8,
       ATTR_Table_CellSpacing = 9, ATTR_Table_Height = 10, ATTR_Table_Width = 11 };
enum { ATTR_TD_Align = 1, ATTR_TD_FixWidth = 5, ATTR_TD_Height = 6, ATTR_TD_VAlign = 7, ATTR_TD_Width = 8 };
enum { ATTR_Img_Height = 1, ATTR_Img_Src = 2, ATTR_Img_Width = 4 };
enum { ATTR_Button_Action = 1, ATTR_Button_Back = 2, ATTR_Button_Fore = 3, ATTR_Button_Height = 4,
       ATTR_Button_Value = 5, ATTR_Button_Width = 6 };
enum { ATTR_MultiEdit_Height = 1, ATTR_MultiEdit_Var = 2, ATTR_MultiEdit_Width = 3 };
// C2 inserted LENGTH (the edit's character limit) before TYPE
enum { ATTR_Edit_Action = 1, ATTR_Edit_Height = 2, ATTR_Edit_Length = 3, ATTR_Edit_Type = 4,
       ATTR_Edit_Var = 5, ATTR_Edit_Width = 6 };
enum { ATTR_Combo_List = 1, ATTR_Combo_Sel = 2, ATTR_Combo_Var = 3, ATTR_Combo_Width = 4 };

// NHtmlObject::HAlign / VAlign
enum { HTMLALIGN_None = 0, HTMLALIGN_Center = 1, HTMLALIGN_Left = 2, HTMLALIGN_Right = 3 };
enum { HTMLVALIGN_None = 0, HTMLVALIGN_Center = 1, HTMLVALIGN_Top = 2, HTMLVALIGN_Bottom = 3 };

// NHtmlItem::Mode (NHtmlBlock::Mode holds it for the next item)
enum
{
    HTMLITEM_Text = 0,
    HTMLITEM_FileLink = 1,     // <a link=...>: loads another page (NCFileLinker)
    HTMLITEM_ActionLink = 2,   // <a action=... msg=...> (NCActionLinker)
    HTMLITEM_Button = 3,
    HTMLITEM_MultiEdit = 4,
    HTMLITEM_Edit = 5,
    HTMLITEM_RedText = 6,      // drawn red; nothing in this module sets it
    HTMLITEM_Image = 7,
    HTMLITEM_ComboBox = 8,
    HTMLITEM_FileLink2 = 9,    // C3 (name ours): laid out, drawn and created like HTMLITEM_FileLink; nothing seen sets it
};

// Parser state shared with the tree while it is built (stock globals, names ours): the
// horizontal alignment stack (<center>, <td align>) and the target / message of the <a> being
// read (256 characters each; the viewer's command handlers reuse the first as their buffer).
#if NW_HOOK_MODE
#define GHtmlAlignStack  NW_GLOBAL(TArray<INT>, 0x102C943C)
#define GHtmlVAlignStack NW_GLOBAL(TArray<INT>, 0x102C89C4)   // only ever holds its 0
#define GHtmlLinkTarget  (reinterpret_cast<TCHAR*>(::nw::Addr(0x102C8098)))
#define GHtmlLinkMsg     (reinterpret_cast<TCHAR*>(::nw::Addr(0x102C8298)))
#else
extern TArray<INT> GHtmlAlignStack;
// references into one object that keeps the stock order (src/standalone/NwGlobals.cpp): an
// overlong link target runs into the message and the message into GHtmlVAlignStack (the wcscpy
// of BuildTree and of the viewers' handlers), as in the stock image
extern TCHAR (&GHtmlLinkTarget)[256];
extern TCHAR (&GHtmlLinkMsg)[256];
extern TArray<INT>& GHtmlVAlignStack;
#endif

/*-----------------------------------------------------------------------------
    The parser (src/nwindow/NHtmlParser.cpp).
-----------------------------------------------------------------------------*/

// One tag or text run from NHtmlParser (0x60 bytes). The attributes are two parallel vectors:
// ids (1-based index in the tag's attribute list) and values.
struct NHtmlTag
{
    INT Tag;                   // +0x00 HTMLTAG_* (not set by the constructor)
    BYTE bOpen;                // +0x04 1 = <tag>, 0 = </tag> (not set by the constructor)
    NStlWString Text;          // +0x08 HTMLTAG_Text: the text; comments, unknown tags and
                               //   <extend>: their source
    NStlWordVector AttrIds;    // +0x24
    NStlWStringVector AttrValues; // +0x34
    NStlWString Colors;        // +0x44 HTMLTAG_Text: the <font color> runs, "color\x1Eword\x1E...";
                               //   other tags: the last attribute not in their list, "name=value"

    NHtmlTag() {}                                                 // 0x10046600
    ~NHtmlTag() {}                                                // 0x10046510
    const TCHAR* GetAttr(INT Attr, UBOOL bUnused);                // 0x1003ABB0 NULL = absent
    bool HasAttr(INT Attr);                                       // 0x1003AC10 (al only)
};
NW_ASSERT_OFFSET(NHtmlTag, Text, 0x08);
NW_ASSERT_OFFSET(NHtmlTag, AttrIds, 0x24);
NW_ASSERT_OFFSET(NHtmlTag, AttrValues, 0x34);
NW_ASSERT_OFFSET(NHtmlTag, Colors, 0x44);
NW_ASSERT_SIZE(NHtmlTag, 0x60);

// std::vector<NHtmlTag*> made by NHtmlParser::Parse
struct NHtmlTagList : NStlVector<NHtmlTag*>
{
};
NW_ASSERT_SIZE(NHtmlTagList, 0x10);

// The tag names the parser knows, in alphabetical order, each with its sorted attribute names
// (stock table, 51 searched in C2), and the HTMLTAG_* id of each (names ours).
struct NHtmlTagInfo
{
    const TCHAR* Name;
    const TCHAR* const* Attrs;
    INT NumAttrs;
};
#if NW_HOOK_MODE
#define GHtmlTagInfo (reinterpret_cast<const NHtmlTagInfo*>(::nw::Addr(0x10234020)))
#define GHtmlTagIds  (reinterpret_cast<const INT*>(::nw::Addr(0x10236918)))
#else
extern const NHtmlTagInfo GHtmlTagInfo[];
extern const INT GHtmlTagIds[];
#endif

// Name lookups in GHtmlTagInfo (4 bytes: a vtable only)
class NHtmlTagTable
{
public:
    NHtmlTagTable() {}                                            // 0x1003AA80
    virtual ~NHtmlTagTable() {}                                   // 0x1003AA90
    // names ours
    INT FindTag(const TCHAR* Name);                               // 0x1003AAA0 index, -1
    INT FindAttr(INT Tag, const TCHAR* Name);                     // 0x1003AB00 1-based, -1
};
NW_ASSERT_SIZE(NHtmlTagTable, 4);

// base of the parser (its destructor is inlined; out-of-line copy 0x10043B90: the vtable store 0x101A99F0 and ret)
class NHtmlParserBase
{
public:
    virtual ~NHtmlParserBase() {}
};

// The tokenizer (0x84 bytes): turns the page into a list of tags and text runs.
class NHtmlParser : public NHtmlParserBase
{
public:
    NHtmlTagTable Table;       // +0x04
    INT Unknown08;             // +0x08 never set
    UBOOL bOutOfMemory;        // +0x0C a tag could not be allocated (never read)
    NStlWString Name;          // +0x10 name of the tag or attribute being read, upper case
    NStlWString Value;         // +0x2C value of the attribute being read
    NStlWString Extra;         // +0x48 an attribute the tag does not have, "name=value"
    NStlWString Unknown64;     // +0x64 emptied by ReadText, never read
    TCHAR* File;               // +0x80 the text LoadFile read

    NHtmlParser();                                                // 0x10043E90
    virtual ~NHtmlParser();                                       // 0x10044390
    TCHAR* LoadFile(TCHAR* FileName);                             // 0x10043CD0 its text, NULL
                                                                  //   when it cannot be read
    NHtmlTagList* Parse(TCHAR* Text);                             // 0x10046F80
    void FreeTags(NHtmlTagList* Tags);                            // 0x10046B10
    void FreeFile();                                              // 0x10043CA0
    // names ours
    bool IsAlnum(TCHAR C);                                        // 0x10043B10
    void ResetError();                                            // 0x10043B40 (`mov [ecx + 0xC], 0`; hooked in NHtmlParser.cpp as ResetErrorStock)
    TCHAR* SkipSpaces(TCHAR* P);                                  // 0x10043BD0
    TCHAR* SkipWord(TCHAR* P);                                    // 0x10043C00 to a space, < or >
    TCHAR* SkipName(TCHAR* P);                                    // 0x10043C50 ... or =
    TCHAR* ReadTagName(TCHAR* P, NHtmlTag* Tag, INT* OutTag);     // 0x100446D0
    TCHAR* ReadSource(TCHAR* P, NHtmlTag* Tag);                   // 0x10044910
    void ReplaceEntities(NStlWString* Str);                       // 0x100449E0
    TCHAR* ReadText(TCHAR* P, NHtmlTag* Tag);                     // 0x10045550
    TCHAR* ReadExtend(TCHAR* P, NHtmlTag* Tag, INT TagIndex);     // 0x10046660
    NHtmlTag* NewTag();                                           // 0x100468F0
    void DeleteTag(NHtmlTag* Tag);                                // 0x100469B0
    TCHAR* ReadAttrs(TCHAR* P, NHtmlTag* Tag, INT TagIndex);      // 0x10046B80
};
NW_ASSERT_OFFSET(NHtmlParser, Table, 0x04);
NW_ASSERT_OFFSET(NHtmlParser, Name, 0x10);
NW_ASSERT_OFFSET(NHtmlParser, Extra, 0x48);
NW_ASSERT_OFFSET(NHtmlParser, File, 0x80);
NW_ASSERT_SIZE(NHtmlParser, 0x84);

/*-----------------------------------------------------------------------------
    The tree (C2 layouts: NHtmlObject 0x4C, NHtmlBlock 0x870, NHtmlTable 0x8BC, NHtmlItem 0xFC). C4 CHECK (wave C, debt2): the
    same sizes and offsets hold in the C4 image: the allocations `push 0x8BC` (NewTable 0x1003ED70), `push 0x870` (NewCell
    0x1003E070), the constructors 0x1003DC20 (the object's stores at +0x04 .. +0x48), 0x1003DCA0 / 0x1003DCF0 (the block's: +0x4C ..
    +0x70), 0x1003DFB0 (the table's: +0x870 .. +0x8B8) and 0x10040840 (the item's: +0x50 .. +0xF0), and the parser globals
    0x102C943C / 0x102C89C4 / 0x102C8098 / 0x102C8298. The addresses in the member comments are the C4 ones (they were the C3
    build's until wave C).
-----------------------------------------------------------------------------*/

// Base of the tree (0x4C bytes, abstract: every slot is _purecall in the stock vtable). Objects
// are always deleted through this class, whose destructor does nothing: the derived parts are
// only freed by Release. Every field below +0x1C is new or moved in C2 (C1: 0x2C bytes).
class NHtmlObject
{
public:
    INT Type;                  // +0x04 HTMLTAG_* of the tag that made it
    INT X;                     // +0x08 in the parent (names ours: C1 had X, Y at +0x10, +0x14)
    INT Y;                     // +0x0C
    INT AlignedX;              // +0x10 AlignX stores X here too; the viewer's links take it
    INT Unknown14;             // +0x14 (0; the links take it)
    NHtmlObject* Parent;       // +0x18
    NCHtmlViewer* Viewer;      // +0x1C
    INT TailWidth;             // +0x20 a link item: how far its last line reaches (Layout)
    INT Width;                 // +0x24
    INT Height;                // +0x28
    INT MinWidth;              // +0x2C narrowest width it can be laid out at
    INT LayoutWidth;           // +0x30 an item: the width it was laid out and is drawn at
    INT HAlign;                // +0x34 HTMLALIGN_*, top of GHtmlAlignStack when parsed
    INT VAlign;                // +0x38 HTMLVALIGN_*
    INT CellPadding;           // +0x3C <table cellpadding> (1); a cell takes its table's
    INT CellSpacing;           // +0x40 <table cellspacing> (2)
    INT Border;                // +0x44 <table border>, px; a cell takes its table's
    INT OffsetY;               // +0x48 the vertical alignment inside the row (set by the table)

    // not hooked: the class is abstract and only our constructors / deletes use them
    NHtmlObject();                                                // 0x1003DC20 (hooked in NHtmlLayout.cpp as NHtmlObjectStandIn)
    ~NHtmlObject() {}                                             // 0x1003AC40

    virtual void Layout(INT MaxWidth, INT MaxHeight) = 0;         // 0 sizes itself
    virtual void Arrange(INT AreaWidth, INT AreaHeight) = 0;      // 1 aligns and places
    virtual void Draw(NCGDevice* Device) = 0;                     // 2
    virtual UBOOL IsPercentWidth() = 0;                           // 3
    virtual INT GetPercentWidth(INT Total) = 0;                   // 4 WIDTH=n% of Total, else 0
    virtual void Release() = 0;                                   // 5 frees what it owns

    void AlignX(INT AreaWidth);                                   // 0x1003AC50
    INT GetAlignY(INT AreaHeight);                                // C1 only: C2 has no such function
    POINT* GetAbsPos(POINT* Out);                                 // 0x1003B930 in the page
};
NW_ASSERT_OFFSET(NHtmlObject, Type, 0x04);
NW_ASSERT_OFFSET(NHtmlObject, X, 0x08);
NW_ASSERT_OFFSET(NHtmlObject, Viewer, 0x1C);
NW_ASSERT_OFFSET(NHtmlObject, Width, 0x24);
NW_ASSERT_OFFSET(NHtmlObject, HAlign, 0x34);
NW_ASSERT_OFFSET(NHtmlObject, OffsetY, 0x48);
NW_ASSERT_SIZE(NHtmlObject, 0x4C);

// Children stacked top to bottom (0x870 bytes): the page (<body>) and every table cell.
class NHtmlBlock : public NHtmlObject
{
public:
    INT Font;                  // +0x4C UCanvas::L2Fonts index (constructor argument; every item
                               //   the block makes inherits it)
    INT FixWidth;              // +0x50 <td fixwidth>, -1
    INT WidthValue;            // +0x54 <td width> / <table width>
    UBOOL bPercent;            // +0x58 the width ends with '%'
    TArray<NHtmlObject*> Children; // +0x5C
    UBOOL bBorder;             // +0x68 a cell of a table with a border: draws its frame
    INT Mode;                  // +0x6C HTMLITEM_* of the next item (set by <a>, <button>...)
    TCHAR Buffer[0x400];       // +0x70 the text of the run of text items being laid out (the
                               //   Thai word wrapper's input, GL2ThaiHtmlMode)

    NHtmlBlock();                                                 // 0x1003DCA0
    NHtmlBlock(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InFont); // 0x1003DCF0 (ret 0xC)
    ~NHtmlBlock();                                                // 0x1003DD40

    virtual void Layout(INT MaxWidth, INT MaxHeight);             // 0 0x1003BAB0
    virtual void Arrange(INT AreaWidth, INT AreaHeight);          // 1 0x1003BCD0
    virtual void Draw(NCGDevice* Device);                         // 2 0x1003BD20
    virtual UBOOL IsPercentWidth();                               // 3 0x1003DCE0 (`mov eax, [ecx + 0x58]`, folded with NCVirtualWndMain::NumFocusWindows)
    virtual INT GetPercentWidth(INT Total);                       // 4 0x1003AD20
    virtual void Release();                                       // 5 0x1003CDA0

    void ParseCellAttrs(NHtmlTag* Tag);                           // 0x1003DDE0 <td ...>
    INT GetContentHeight();                                       // 0x1003C450
    NHtmlObject* NewItem(NHtmlTag*** It, INT InFont);             // 0x100413C0
    NHtmlObject* NewTable(NHtmlTag*** It, INT InFont);            // 0x1003ED70
    void Parse(NHtmlTag*** It, UBOOL bInBorder);                  // 0x10041450 to the closing tag
};
NW_ASSERT_OFFSET(NHtmlBlock, Font, 0x4C);
NW_ASSERT_OFFSET(NHtmlBlock, FixWidth, 0x50);
NW_ASSERT_OFFSET(NHtmlBlock, Children, 0x5C);
NW_ASSERT_OFFSET(NHtmlBlock, Mode, 0x6C);
NW_ASSERT_OFFSET(NHtmlBlock, Buffer, 0x70);
NW_ASSERT_SIZE(NHtmlBlock, 0x870);

// <table> (0x8BC bytes): rows of cells (NHtmlBlock), columns as wide as their widest cell.
class NHtmlTable : public NHtmlBlock
{
public:
    INT NumRows;               // +0x870 <tr> read
    INT CurCol;                // +0x874 column of the next cell of the row
    TArray<TArray<NHtmlBlock*>*> Rows; // +0x878
    TArray<INT> ColMinWidths;  // +0x884 widest MinWidth of each column
    TArray<INT> ColLayoutWidths; // +0x890 width of each column after Layout
    TArray<INT> ColWidths;     // +0x89C widest cell of each column
    TArray<INT> RowHeights;    // +0x8A8 after Layout
    DWORD BgColor;             // +0x8B4 <table bgcolor>
    UBOOL bBgColor;            // +0x8B8

    NHtmlTable(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InFont); // 0x1003DFB0
    // (deleted as NHtmlObject)

    virtual void Layout(INT MaxWidth, INT MaxHeight);             // 0 0x1003CE00
    virtual void Arrange(INT AreaWidth, INT AreaHeight);          // 1 0x1003BE40
    virtual void Draw(NCGDevice* Device);                         // 2 0x1003BF10
    virtual void Release();                                       // 5 0x1003E480

    NHtmlBlock* NewCell(NHtmlTag*** It, INT InFont);              // 0x1003E070
    void Parse(NHtmlTag*** It);                                   // 0x1003E110
};
NW_ASSERT_OFFSET(NHtmlTable, NumRows, 0x870);
NW_ASSERT_OFFSET(NHtmlTable, Rows, 0x878);
NW_ASSERT_OFFSET(NHtmlTable, RowHeights, 0x8A8);
NW_ASSERT_OFFSET(NHtmlTable, bBgColor, 0x8B8);
NW_ASSERT_SIZE(NHtmlTable, 0x8BC);

// A leaf (0xFC bytes): text, image, line break or a control. The strings hold the tag's
// attributes until Arrange hands them to the control it creates.
class NHtmlItem : public NHtmlObject
{
public:
    INT Font;                  // +0x4C UCanvas::L2Fonts index the text is measured and drawn in
    INT Mode;                  // +0x50 HTMLITEM_*
    NStlWString Action;        // +0x54 link target, button action, image, edit action,
                               //   combo box list
    NStlWString ForeTex;       // +0x70 <button fore>
    NStlWString BackTex;       // +0x8C <button back>
    NStlWString Var;           // +0xA8 control variable, button label, link message
    NStlWString Sel;           // +0xC4 <combobox sel>
    INT Length;                // +0xE0 <edit length> (0 = none; not set by the constructor)
    TCHAR* Colors;             // +0xE4 text: copy of NHtmlTag::Colors
    UTexture* Tex;             // +0xE8 image
    INT EditType;              // +0xEC <edit type>
    TCHAR* Text;               // +0xF0 text: copy of NHtmlTag::Text
    INT FirstLineOffset;       // +0xF4 text: where its first line starts (the previous text item's
                               //   TailEnd, so that text runs on)
    INT TailEnd;               // +0xF8 text: where its last line ends (what GetTextExtent returned)

    NHtmlItem(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InMode, INT InFont); // 0x10040840
    // (deleted as NHtmlObject)

    virtual void Layout(INT MaxWidth, INT MaxHeight);             // 0 0x1003BA10
    virtual void Arrange(INT AreaWidth, INT AreaHeight);          // 1 0x10040260
    virtual void Draw(NCGDevice* Device);                         // 2 0x10041210
    virtual UBOOL IsPercentWidth() { return 0; }                  // 3 0x100023A0 (`xor eax, eax; ret`, shared by many classes)
    virtual INT GetPercentWidth(INT Total) { return 0; }          // 4 0x100023D0 (`xor eax, eax; ret 4`, shared)
    virtual void Release();                                       // 5 0x10040670

    INT ParseEditType(const TCHAR* Str);                          // 0x1003AC90 (this unused)
    void SetText(NHtmlTag* Tag);                                  // 0x1003CC90
    void Parse(NHtmlTag*** It);                                   // 0x10040960
    void DrawColoredText(NCGDevice* Device, UBOOL bLink);         // 0x10040E10
};
NW_ASSERT_OFFSET(NHtmlItem, Mode, 0x50);
NW_ASSERT_OFFSET(NHtmlItem, Action, 0x54);
NW_ASSERT_OFFSET(NHtmlItem, Var, 0xA8);
NW_ASSERT_OFFSET(NHtmlItem, Sel, 0xC4);
NW_ASSERT_OFFSET(NHtmlItem, Length, 0xE0);
NW_ASSERT_OFFSET(NHtmlItem, Colors, 0xE4);
NW_ASSERT_OFFSET(NHtmlItem, Text, 0xF0);
NW_ASSERT_OFFSET(NHtmlItem, TailEnd, 0xF8);
NW_ASSERT_SIZE(NHtmlItem, 0xFC);

// "RRGGBB" as 0xFFRRGGBB; "LEVEL" is 0xFFFFCC00 (name ours)
DWORD HtmlParseColor(const TCHAR* Str);                           // 0x1003B980
