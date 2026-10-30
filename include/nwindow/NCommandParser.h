#pragma once
// NCommandParser - the FParser that recognizes chat commands (name ours; 0x418 bytes, plain
// C++): NCChatWnd keeps one for its input line, NCGMPetitionWnd builds one on the stack.
// GetCommand gives the commandname-*.dat code of a "/command" line, the tokens after it come
// from GetNextToken.

#include "nwindow/NWindow.h"

// separators given to every parser (name ours); FParser::SetString puts its own back
#if NW_HOOK_MODE
#define GCommandSeparators (reinterpret_cast<TCHAR*>(::nw::Addr(0x10239610)))   // L" ,\t\n"
#else
extern TCHAR GCommandSeparators[];
#endif

// the character that starts a command, ASCII and full width (name ours; C2 reads two pointers, 0x10161e84 -> L"/"
// and 0x10161e88 -> L"\xFF0F", where C1 compared with the literals)
#if NW_HOOK_MODE
#define GCommandPrefix (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x1023892C)))
#else
extern const TCHAR* const GCommandPrefix[];   // 2 (the stock table goes on with the "//" and "///" pairs)
#endif

// GetCommand results: the action codes of commandname-*.dat (named after its actions) and
// the special ones
enum
{
    NCMD_Loc, NCMD_Petition, NCMD_PetitionCancel, NCMD_GMList, NCMD_Sit, NCMD_Stand, NCMD_Walk,
    NCMD_Run, NCMD_Attack, NCMD_AttackForce, NCMD_AttackStand, NCMD_Trade, NCMD_Target,
    NCMD_TargetNext, NCMD_PickUp, NCMD_Assist, NCMD_PartyInvite, NCMD_PartyLeave,
    NCMD_PartyDismiss, NCMD_Vendor, NCMD_PartyMatching, NCMD_SocialHello, NCMD_SocialVictory,
    NCMD_SocialCharge, NCMD_UseSkill, NCMD_UseSkillForce, NCMD_UseSkillStand, NCMD_UseShortcut,
    NCMD_UseShortcutForce, NCMD_UseShortcutStand, NCMD_AllyInvite, NCMD_AllyLeave, NCMD_AllyOust,
    NCMD_AllyDismiss, NCMD_AllyCrest, NCMD_AllyInfo, NCMD_FriendInvite, NCMD_FriendList,
    NCMD_FriendDel, NCMD_Buy, NCMD_AllyWarStart, NCMD_AllyWarStop, NCMD_AllyWarSurrender,
    NCMD_BlockList, NCMD_Block, NCMD_Unblock, NCMD_SocialNo, NCMD_SocialYes, NCMD_SocialBow,
    NCMD_SocialUnaware, NCMD_SocialWaiting, NCMD_SocialLaugh, NCMD_Escape,
    NCMD_DwarvenManufacture,                    // 0x35

    NCMD_None = -1,                             // no line, or an unknown /command
    NCMD_Builder = 0x10000000,                  // "//...": a builder command for the server
    NCMD_Exec = 0x10000001,                     // "///...": a console command run here
    NCMD_Chat = 0x20000000,                     // not a command: plain chat
};

// the words of a command that stand for a name (name ours; C2 only, the macros and the chat use them): a table
// of eleven pointers at 0x10161e58 to %target, %self, %pet and %party1 .. %party8
#define NCMD_TOKEN_COUNT 11
#if NW_HOOK_MODE
#define GCommandTokenNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10238900)))
#else
extern const TCHAR* const GCommandTokenNames[];   // NCMD_TOKEN_COUNT
#endif

class NCommandParser : public FParser
{
public:
    NCommandParser();                         // 0x100338e0
    // ~NCommandParser: FParser's (0x10033930 is the import thunk, hooked)

    INT GetCommand(TCHAR* Line);              // 0x10033940 (name ours)
    INT ExpandToken(TCHAR* Token);            // 0x1003e900 (name ours; C2 only; the parser itself is not read)
};
NW_ASSERT_SIZE(NCommandParser, 0x418);

// A "msgid;argument" code of an action link (C2 only; name ours) turned in place into the text of the system
// message msgid with its "$s1" replaced by the argument
void ExpandSystemMsgCode(TCHAR* Message);     // 0x10046190 (cdecl)
