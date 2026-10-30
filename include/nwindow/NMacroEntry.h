#pragma once
// NMacroEntry - one macro of the macro window (a plain C++ object, name ours; 0xA54 bytes, allocated with
// GMalloc->Malloc by NConsoleWnd::vfn132 (0x10066d70, the macro list from the server) and freed by
// NCMacroListWnd::OnDestroy). NCMacroListWnd keeps them in a TArray; NCMacroArrayWnd draws their icons and
// short names; NCMacroEditWnd edits them; the console's tick runs the one whose Running flag is set, a line
// at a time (0x1008ebe0 -> 0x1008bc60).
//
// A macro starts with its NToolTipInfo (NToolTipInfo::FillMacro reads the name at +0x9EC and the
// description at +0xA06 through `this`); then twelve lines, an icon texture, the command parser the lines
// are recognized with (NCommandParser, +0x5BC) and the numbers and texts the server sent.
//
// Layout from the constructor 0x1008F840 (the vector constructor of the lines 0x1008B2D0, the parser
// constructor 0x1003E840, the memsets of the three texts), the destructor 0x1008F910, Set 0x1008B380 and the
// executor 0x1008BC60.

#include "nwindow/NCWnd.h"
#include "nwindow/NCommandParser.h"
#include "nwindow/NCCoolTimeIcon.h"
#pragma pack(push, 4)

enum
{
    NMACRO_MaxLines = 12,      // lines of a macro
    NMACRO_MaxMacros = 24,     // macros the window shows (6 x 4 slots of the list; 0x18)
};

// What a line of a macro does (NMacroLine::Type; names ours, from the executor 0x1008BC60 and Set 0x1008B380)
enum
{
    NMLT_None = 0,
    NMLT_Skill = 1,            // Param08: the skill id, Param10 its level; used with NConsoleWnd::RequestMagicSkillUse
    NMLT_Action = 2,           // Param08: the action's number (the social actions set Duration)
    NMLT_Command = 3,          // Text is a "/command" or a chat line (NCommandParser::GetCommand)
    NMLT_ShortCut = 4,         // Param08 + Param0C * 10: a shortcut slot (Param08 the slot, Param0C the page)
    NMLT_Item = 5,             // Param04 / Param08: an item (NConsoleWnd::RequestUseItem(Param04)), not set by Set
    NMLT_Delay = 6,            // Param0C seconds: Duration
};

// One line of a macro (0x70 bytes, name ours; the entry's lines start at +0x78). The constructor 0x1008B2D0
// clears the four state words and the text; 0x1008B300 (the vector destructor's callback) clears the words.
struct NMacroLine
{
    INT Type;                  // +0x00 NMLT_*
    INT Param04;               // +0x04 NMLT_Item: the item's object id (RequestUseItem)
    INT Param08;               // +0x08 (the executor: line +0x80) the skill / action / shortcut slot / item id
    INT Param0C;               // +0x0C the shortcut page, the delay's seconds
    INT Param10;               // +0x10 the skill's level
    BYTE _Unknown14[4];        // +0x14 (never touched by the code seen)
    FLOAT Duration;            // +0x18 seconds the line waits (the executor sets it from the animation's length or the
                               //   delay, the console's events from a cool time); the line ends when Clock - StartTime passes it
    FLOAT StartTime;           // +0x1C the clock's value when the line started (Tick: the first tick's delta); 0 = not started
    FLOAT Clock;               // +0x20 the line's clock, running from the first tick's delta (Tick adds each delta)
    INT Unknown24;             // +0x24 same
    TCHAR Text[33];            // +0x28 the command (NMLT_Command)
    BYTE _Unknown6A[6];        // +0x6A

    NMacroLine();              // 0x1008B2D0
    ~NMacroLine();             // 0x1008B300 (clears the four state words)
};
NW_ASSERT_OFFSET(NMacroLine, Duration, 0x18);
NW_ASSERT_OFFSET(NMacroLine, Text, 0x28);
NW_ASSERT_SIZE(NMacroLine, 0x70);

// A line as the server sends it (0x4C bytes, name ours): Set copies it into the entry's line
// number LineNo - 1 (a LineNo of 0 or 1 is the first line)
struct NMacroInfoLine
{
    BYTE LineNo;               // +0x00 1-based
    BYTE Type;                 // +0x01 NMLT_*
    BYTE _Unknown02[2];        // +0x02
    INT A;                     // +0x04 skill id / action number / shortcut page / delay
    BYTE B;                    // +0x08 the skill's level / the shortcut slot
    BYTE _Unknown09;           // +0x09
    TCHAR Text[33];            // +0x0A the command
};
NW_ASSERT_OFFSET(NMacroInfoLine, A, 0x04);
NW_ASSERT_OFFSET(NMacroInfoLine, Text, 0x0A);
NW_ASSERT_SIZE(NMacroInfoLine, 0x4C);

// A macro as the server sends it and as NCMacroListWnd keeps the slots (0x3F8 bytes; name ours). The slot is
// this behind a dword with the macro's id (NMacroSlot): the stock code passes the slot by value (0x3FC bytes)
struct NMacroInfo
{
    TCHAR Name[13];            // +0x00 the macro's name (NMacroEntry +0x9EC)
    TCHAR Description[33];     // +0x1A the description (+0xA06)
    TCHAR ShortName[5];        // +0x5C what the slot's icon shows (+0xA48)
    BYTE IconType;             // +0x66 0..6: the icon "L2UI.MacroWnd.Macro_Icon<n+1>"
    BYTE NumLines;             // +0x67
    NMacroInfoLine Lines[NMACRO_MaxLines];   // +0x68
};
NW_ASSERT_OFFSET(NMacroInfo, Description, 0x1A);
NW_ASSERT_OFFSET(NMacroInfo, ShortName, 0x5C);
NW_ASSERT_OFFSET(NMacroInfo, Lines, 0x68);
NW_ASSERT_SIZE(NMacroInfo, 0x3F8);

struct NMacroSlot
{
    INT ID;                    // +0x00
    NMacroInfo Info;           // +0x04
};
NW_ASSERT_SIZE(NMacroSlot, 0x3FC);

struct NMacroEntry
{
    NToolTipInfo ToolTip;      // +0x000 (the tool tip of the slot; FillMacro fills it)
    NCCoolTimeIcon CoolTime;   // +0x7C new in C3 (ctor 0x1009f980 builds it right after the tool tip)
    NMacroLine Lines[NMACRO_MaxLines]; // +0x078
    UTexture* IconTex;         // +0x5B8 "L2UI.MacroWnd.Macro_Icon<IconType + 1>" (NCMacroListWnd::SetIcon 0x1008C7A0)
    NCommandParser Parser;     // +0x5BC
    INT bRunning;              // +0x9D4 the macro is running: the console's tick executes its CurLine
    INT bSelected;             // +0x9D8 the slot is selected in NCMacroArrayWnd (the editor works on it)
    INT ID;                    // +0x9DC the server's id
    INT IconType;              // +0x9E0
    INT NumLines;              // +0x9E4
    INT CurLine;               // +0x9E8 the line being run
    TCHAR Name[13];            // +0x9EC
    TCHAR Description[33];     // +0xA06
    TCHAR ShortName[5];        // +0xA48
    BYTE _UnknownA52[2];       // +0xA52

    NMacroEntry();                                                   // 0x1008F840
    ~NMacroEntry();                                                  // 0x1008F910

    __declspec(noinline) INT IsSelected();                                                // 0x1009AE30 (anonymous in the db) bSelected
    __declspec(noinline) const TCHAR* GetName();                                          // 0x1009AE40 NULL when empty
    __declspec(noinline) const TCHAR* GetShortName();                                     // 0x1009AE60 (the db's GetDescription)
    __declspec(noinline) const TCHAR* GetDescription();                                   // 0x1009AE80 (anonymous in the db; the console lane's)
    __declspec(noinline) INT Deselect();                                                  // 0x1009AEA0 bSelected = 0 (naked: eax untouched)
    __declspec(noinline) void Set(INT InID, NMacroInfo Info);                             // 0x1009AEB0 (ret 0x3FC)
    __declspec(noinline) void Reset();                                                    // 0x1009AFF0 the lines' counters, the line, bRunning
    __declspec(noinline) void Select();                                                   // 0x1009B030 bSelected = 1
    __declspec(noinline) UTexture* GetIconTex();                                          // 0x1009B040
    __declspec(noinline) INT ExecuteLine(INT Index);                 // 0x1009B8B0 (ret 4; anonymous in the db) runs one line; -1 when it cannot
    __declspec(noinline) void Tick(FLOAT DeltaTime);                 // 0x1009E8F0 (ret 4) the console's tick: the running macro's timers
};
NW_ASSERT_OFFSET(NMacroEntry, Lines, 0xD8);
NW_ASSERT_OFFSET(NMacroEntry, IconTex, 0x618);
NW_ASSERT_OFFSET(NMacroEntry, Parser, 0x61C);
NW_ASSERT_OFFSET(NMacroEntry, bRunning, 0xA34);
NW_ASSERT_OFFSET(NMacroEntry, Name, 0xA4C);
NW_ASSERT_OFFSET(NMacroEntry, Description, 0xA66);
NW_ASSERT_OFFSET(NMacroEntry, ShortName, 0xAA8);
NW_ASSERT_SIZE(NMacroEntry, 0xAB4);

#pragma pack(pop)
