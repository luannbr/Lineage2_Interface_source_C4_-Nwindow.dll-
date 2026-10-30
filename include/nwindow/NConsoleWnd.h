#pragma once
// NConsoleWnd - the game console (UClass "ConsoleWnd", CLASS_Config, sizeof 0x35B8): implements
// UL2ConsoleWnd for the engine, owns the window manager and every window, runs the timers.
// The whole vtable is declared in the order of the C2 one (335 slots, 0x10115438): the 275 slots the
// engine leaves pure (all named: the 39 that only C2 has from the UGameEngine handlers that call them and
// from what they do, NConsoleWndC2.cpp), the engine's own, and 15 of the console's (slots 320..334); docs/notas/NConsoleWnd.md
// maps the C1 ones. Every virtual below sits at its C2 slot (the number in its comment).

#include "nwindow/NCObject.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCWnd;
class NCConsole;
class NCVirtualWndMain;
class NCDebugWnd;
struct NDialogInfo;
struct CharacterInfo;
class NCHtmlViewer;
class NCRestartMenuWnd;
class NCOptionWnd;
class NCLoginWnd;
class NCLobbyWnd;
class NCPawnCreateWnd;
class NCLoadingWnd;
class NCMessageWnd;
class NCDialogBox;
class NKeyCrypt;
struct NItemRequest;
struct NItemEntry;
struct NMacroEntry;

// A list of windows that can shrink to its count (UE2's TArray::Shrink, which the stock build expands inline): the
// windows of one UI mode (name ours)
class NWndPtrArray : public TArray<NCWnd*>
{
public:
    void Shrink()
    {
        if (ArrayMax != ArrayNum)
        {
            ArrayMax = ArrayNum;
            Realloc(sizeof(NCWnd*));
        }
    }
};
NW_ASSERT_SIZE(NWndPtrArray, 0xC);

// one SetTimer record (NConsoleWnd::NSetTimer)
struct NTimer
{
    UINT ID;              // +0x00
    UINT Elapse;          // +0x04 milliseconds
    UBOOL bKilled;        // +0x08 set by NKillTimer, dropped by the next Tick
    FLOAT Elapsed;        // +0x0C seconds since the last WM_TIMER
    NCObject* Owner;      // +0x10 gets WindowProc(WM_TIMER, ID, 0)
};
NW_ASSERT_SIZE(NTimer, 0x14);

// a parameter of the system message being received (names ours; AddSystemMessageParam): its
// text, made from a string or a number
struct NSysMsgParam
{
    TCHAR* Text;          // +0x00 GMalloc'd copy
    INT Len;              // +0x04 appStrlen(Text)

    void Set(const TCHAR* InText);    // 0x10047380 a copy of InText; nothing for NULL
    void SetNumber(INT Value);        // 0x100473d0 "%d"
};
NW_ASSERT_SIZE(NSysMsgParam, 0x8);

// a manor of the list the server sends (OnSendManorList; names ours): its id and its name
struct NManorEntry
{
    INT ID;               // +0x00
    FString Name;         // +0x04
};
NW_ASSERT_SIZE(NManorEntry, 0x10);

// The Korean particles of the system messages ("%" and a particle after a "$sN"): whether
// a Hangul syllable ends without a final consonant (any other character counts so; stock
// 0x100427d0, which takes the character in AX: not hooked, its own calling convention),
// and the particle's form after it: i / ga, eul / reul, eun / neun, gwa / wa (0 for
// another character: nothing is put). MakeSystemMessage and EndSystemMessageParam.
FORCEINLINE UBOOL HasNoFinalConsonant(TCHAR C)
{
    if (C < 0xAC00 || C > 0xD7A3)
        return 1;
    return (C - 0xAC00) % (21 * 28) % 28 == 0;
}
FORCEINLINE TCHAR ParticleForm(TCHAR Particle, UBOOL bNoFinal)
{
    // C3 (FormatSystemMessage 0x1005f130, MakeSystemMessage 0x10077330): three pairs, as single-byte codes
    switch (Particle)
    {
    case 0xC0: return bNoFinal ? 0xCC : 0xC0;
    case 0xB0: return bNoFinal ? 0xA1 : 0xB0;
    case 0xB8: return bNoFinal ? 0xB8 : 0xA6;
    }
    return 0;
}

// One line of questname-*.dat in the C2 game data (name ours): 1000 of them at FL2GameData +0x4CF4
// (0x24 bytes each: C1's FL2QuestNameData had 0x14, the two texts are FStrings now). The Engine
// header does not place the table yet; NConsoleWnd reads it through GQuestNames.
struct NQuestNameLine
{
    INT bValid;               // +0x00 0: no quest here
    INT QuestID;              // +0x04
    INT Step;                 // +0x08
    FString Name;             // +0x0C the quest's (AddQuestID)
    FString Text;             // +0x18 the step's (GetQuestText)
};
NW_ASSERT_SIZE(NQuestNameLine, 0x24);
#define GQuestNames ((NQuestNameLine*)((BYTE*)&GL2GameData + 0x4CF4))

// A dye of the henna lists (names ours; 0x44 bytes in C2: UGameEngine::ReceiveHennaInfo and its kind pass it by value
// to the console, NConsoleWndC2.cpp reads these fields: the henna's id for the henna data, the dye's item id for the
// item data)
struct HennaInfo
{
    INT HennaID;               // +0x00 FL2GameData::GetHennaData's key
    INT ItemID;                // +0x04 the dye's item (FL2GameData::GetItemData)
    INT Param08;               // +0x08
    INT Param0C;               // +0x0C
    INT Param10;               // +0x10
    BYTE _Unknown14[0x44 - 0x14]; // +0x14 not read by NWindow
};
NW_ASSERT_SIZE(HennaInfo, 0x44);

// A macro of the list the server sends (UGameEngine::ReceiveMacroListInfo; 0x3FC bytes in C2: the console copies it
// into the macro window's records and its macro object; the fields are the macro window's business)
struct MacroInfo
{
    BYTE Data[0x3FC];
};
NW_ASSERT_SIZE(MacroInfo, 0x3FC);

// a window's place as NConsoleWnd::ScaleRect takes it (names ours): the screen place and size
struct NResizeRect
{
    INT X;
    INT Y;
    INT W;
    INT H;
};
NW_ASSERT_SIZE(NResizeRect, 0x10);

class NConsoleWnd : public UL2ConsoleWnd
{
    NW_DECLARE_CLASS(NConsoleWnd, UL2ConsoleWnd, CLASS_Config, 0x102D26E0)

    UBOOL bInitialized;               // +0x3BA8 (name ours) set at the end of Initialize (C4 evidence: the constructor 0x100824f0 clears it,
                                      //   Initialize returns 1 at once when it is set, ConsolePostRender tests it; +0x3BB0 was wrong)
    UBOOL bL2MarkSaved;               // +0x3BAC (new in C4, name ours) GIsL2Mark was kept for the observer mode (SaveL2Mark / RestoreL2Mark)
    INT SavedL2Mark;                  // +0x3BB0 (new in C4, name ours) the value GIsL2Mark had
    INT UIMode;                       // +0x3BB4 (new in C3, name ours) 0 the game's windows, 1 the observer's (SwitchUIMode 0x10074890 keeps the
                                      //   visible windows of each mode in the two arrays below); the shortcut keys and the target window read it
    NWndPtrArray ModeWnds[3];         // +0x3BB8 (name ours) the windows of each UI mode: the ones of the desktop that were shown when the mode was left
                                      //   (SwitchUIMode); C4 has a third mode, 2, the Olympiad's, at +0x3BD0
    NCVirtualWndMain* WndMain;        // +0x3BDC (NConsoleWnd::SetCursor)
    // the windows of the login and character stages (Initialize; names ours)
    NCWnd* GodWnd;                    // +0x3BE0 (C3: NCGodWnd, 0xDC bytes; name ours, was RestartMenuWnd) the screen-sized top window made last by
                                      //   Initialize (style 0xA, ctor 0x100861C0): it owns the restart menu (+0xCC) and the options window (+0xD4)
                                      //   and answers ShowMessage / ShowOptionWnd / ReceiveServerList / RestartBegin; hidden for the test consoles
    // C2: not placed (no evidence for where it went): NCOptionWnd* OptionWnd;           // +0x3524
    // C2: not placed (no evidence for where it went): NCLoginWnd* LoginWnd;             // +0x3954
    NCWnd* CreditWnd;                 // +0x3BE4 (name ours; new in C2) NCCreditWnd, a top window made by Initialize   // C2: offset derived, no evidence
    NCLobbyWnd* LobbyWnd;             // +0x3BE8 the character selection
    NCPawnCreateWnd* PawnCreateWnd;   // +0x3BEC the character creation
    NCLoadingWnd* LoadingWnd;         // +0x3BF0
    // C2: not placed (no evidence for where it went): NCMessageWnd* MessageWnd;         // +0x353C the line under the login screens
    NCWnd* Desktop;                   // +0x3BF4 (name ours) the screen-sized NCConsole window,
                                      //   root of the in-game UI (Initialize)
    NCDebugWnd* DebugWnd;             // +0x3BF8 (NCObject::Debugf)
    NCWnd* UnusedWnd;                 // +0x3BFC (name ours) a top window never created   // C2: offset derived, no evidence   // C2: offset derived, no evidence
                                      //   (Finalize destroys it)
    NCWnd* PetitionWnd;               // +0x3C00 (name ours) the player's petition window (NCPetitionWnd, class 0x102F5E88; title: system string 470);
                                      //   the C3 header had the login stages' dialog box here (it is a child of the god window now)
    NCWnd* PetitionFeedBackWnd;       // +0x3C04 (name ours) the petition feedback window (NCPetitionFeedBackWnd, class 0x102F4EF0; title 848) at the screen's
                                      //   lower right (the C3 header called it PetitionWnd)
    NCWnd* Wnd3C08;                   // +0x3C08 (name ours) a window new in C4 (class 0x103103A0, ctor 0x10120D10, style 0x400A, title 1112): the tick
                                      //   feeds it while it is shown (0x10120210); Finalize destroys it
    NCWnd* Wnd3C0C;                   // +0x3C0C (new in C2, name ours) a window the console shows and hides (C4Virtual317 / 318; Finalize destroys it)
    TArray<NSysMsgParam> SysMsgParams;// +0x3C10 (name ours) of the system message being received (C4: the constructor clears
                                      //   +0x3C10, +0x3C14, +0x3C18; Add/EndSystemMessageParam, MakeSystemMessage; C3 had it at +0x3980)
    NCWnd* ThaiWordWrapperWnd;        // +0x3C1C (name ours) the Thai word wrapper window (L2WordWrapperWnd, made by Initialize when GL2ThaiHtmlMode is set)
    TArray<NManorEntry> ManorList;    // +0x3C20 (new in C4, name ours) the manors (the id, then the name) that OnSendManorList fills and
                                      //   the manor windows read (ValidateManorComboBox, OnShowSeedInfo)
    NCWnd* WordWrapperWnd;            // +0x3C2C (name ours; new in C2) L2WordWrapperWnd, a top window made by Initialize   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    TArray<NTimer> Timers;            // +0x3C30
    INT MouseWheelDelta;              // +0x3C3C (name ours) of the last wheel event
    FLOAT DeltaTime;                  // +0x3C40 seconds of the last Tick   // C2: offset derived, no evidence
    INT Unknown357C;                  // +0x3C44 (new in C2; the constructor sets 1)   // C2: offset derived, no evidence
    INT Unknown3580;                  // +0x3C48 (new in C2; the constructor clears it)   // C2: offset derived, no evidence
    UBOOL bCtrlDown;                  // +0x3C4C (name ours) PressCtrl / ReleaseCtrl   // C2: offset derived, no evidence
    // C3: bCtrlDownAlt is gone (PressCtrl / ReleaseCtrl are two plain stores, IsCtrlDown is no function). From here on
    // every field is 4 bytes below the derived offsets this header had (evidence: the constructor's stores, PressCtrl,
    // ConsolePostRender 0x10075c80 (bSuspended), MasterConsoleEventProcess 0x10075d50 (bWheelHandled), SendTutorialEvent
    // 0x1005df40 (TutorialEvents), UseAction 0x10074ab0 (the party item rule), Initialize 0x10077f20 (KeyCrypt); the C4 offsets are the ones
    // the C4 constructor 0x100824F0 and the stock readers use, listed per field below)
    UBOOL bSuspended;                 // +0x3C50 (name ours) no painting, no input; never set
    UBOOL bWheelHandled;              // +0x3C54 (name ours) the last wheel step went to a window:
                                      //   its release is eaten (MasterConsoleEventProcess)
    INT Unknown3720;                  // +0x3C58 (AcceptGameStart passes it to SetRes as the fullscreen flag)
    INT TutorialEvents;               // +0x3C5C (name ours) the pending tutorial event bits
    NKeyCrypt* KeyCrypt;              // +0x3C60 (new in C2; the constructor clears it, Initialize sets it) the keyboard protection helper
                                      //   (NKeyCrypt.h), made by Initialize when GL2UseKeyCrypt is set
    INT LoadingScreenID;              // +0x3C64 (name ours) shown when entering the game (10001)
    INT SpecialServer;                // +0x3C68 (name ours; stage 4 wave C: this was named PartyItemDistribution, which is the field below) the
                                      //   "special server" flag of the server the player logged in to: 0 or 8, the row's flag 8 that
                                      //   NCServerSelectWnd::OnOK (0x10100BF0, `mov [ecx + 0x3C68], eax` after `and eax, 8`) stores. The only
                                      //   reader is the getter at 0x100826A0 (GetSpecialServer, vslot 416), which Engine.dll calls
    INT PartyItemRule;                // +0x3C6C (name ours) how the party's items are shared (the loot combo of NCGameOptionWnd: Apply 0x100C8530
                                      //   and InitItems 0x100C97D0 write it); sent with the party invitations (NConsoleWnd::UseAction 0x1007E8D0,
                                      //   NCChatWnd::ProcessChatLine 0x100582D0) and with the party room request (NCPartyRoomMakingWnd::OnOkBtnClick
                                      //   0x100B2F30). Cleared by no constructor (the allocator's zero)
    UBOOL bNoChatOnEnter;             // +0x3C70 (name ours) Enter leaves the chat line alone (the constructor clears it,
                                      //   HookShortCutKey reads it); never set
    INT Unknown39D0;                  // +0x3C74 (new in C3; ReceiveVersionCheck stores it, 0x1008cb80 reads it)   // C2: offset derived, no evidence
    NCWnd* PawnViewerWnd[3];          // +0x3C78 (name ours) the pawn viewer's windows
    NCWnd* ReplayWnd;                 // +0x3C84 (new in C3, name ours) the replay mode's screen-sized top window (NCReplayWnd, made by
                                      //   InitializeReplayMode 0x100744a0; C3Virtual385 / 386 hide and show it)
    INT Unknown39E4;                  // +0x3C88 (new in C3, up to sizeof; the constructor clears it)
    INT GMValue;                      // +0x3C8C (new in C4, name ours) a number the GM's server change keeps (C4Virtual410 sets it, 445 changes it)
    INT GMStep;                       // +0x3C90 (new in C4, name ours) the step of the GM's server change (SetGMChangeServerState advances it,
                                      //   C4Virtual446 / 447 count it down / up, 444 clears it)
    UBOOL bGMChange;                  // +0x3C94 (new in C4, name ours) the GM's server change is on: StartNotice and RequestGameStart read it
    FLOAT GMDest[3];                  // +0x3C98 (new in C4, name ours) the place the GM goes to (C4Virtual442 gives it as integers)

    NConsoleWnd();                                                   // 0x100824f0
    // The stock constructor's and destructor's bodies as plain methods (names ours; hooked at 0x100824F0 / 0x10082900). The objects
    // are made by the stock InternalConstructor and must keep the STOCK vtables (0x101B2428 and, at +0x34, 0x101B2424): our compiled
    // vtable still has placeholders (NConsoleWnd.cpp, VT DIFF), so a C++ constructor / destructor hook, which stores ours, is no
    // option (SwitchboardSession and MSNSocket write the stock vtable by hand the same way)
    NConsoleWnd* ConstructConsole();                                 // 0x100824f0 (returns this)
    void DestroyConsole();                                           // 0x10082900
    NCConsole* GetDesktop() const;                                   // Desktop as what it is
                                                                     //   (defined in NCConsole.h)

    // Decompiled in NConsoleWndNet.cpp / NConsoleWndData.cpp, not in the groups of the windows
    // below (names ours: the requests after the protocol's packets)
    void RequestBypassToServer(L2ParamStack& Params);                // 0x10068d40 HTML "bypass"
    void RequestTutorialPassCmdToServer(L2ParamStack& Params);       // 0x10068ed0 the tutorial's
    UTexture* GetAllyCrestTexture(INT AllyID);                       // 0x1006a8f0 8x12 crest
    void RequestAddTradeItem(NItemRequest Request);                  // 0x10066800 (guard name)
    void RequestDropItem(NItemRequest Request, FVector Location);    // 0x1006ff50 (guard name)
    const TCHAR* GetSkillIconName(INT SkillID, INT Level);           // 0x1006a920
    const TCHAR* GetActionIconName(INT ActionID);                    // 0x10073fc0 (C2: heap entries, FString at +0x28)
    const TCHAR* GetCastleName(INT CastleID);                        // 0x1006b040 castlename-*.dat
    const TCHAR* GetCastleLocation(INT CastleID);                    // 0x1006b090 its 2nd text
    const TCHAR* GetCastleDescription(INT CastleID);                 // 0x1006b0e0 its 3rd text
    INT GetWeaponEnchantSteps(INT Enchant);                          // 0x1006b690 past +3: x2
    INT GetArmorEnchantSteps(INT Enchant);                           // 0x1006b6a0 past +3: x3
    const TCHAR* GetIconName(INT ItemID);                            // 0x1007d5d0 (guard name) the item's icon name
    UBOOL HasSelectedTarget();                                       // 0x1006a3a0 (C1 only: no C2 match) (shortcut keys)
    void FormatDateTime(INT Time, TCHAR* Out);                       // 0x10083a90 "yyyy/mm/dd hh:mm"
                                                                     //   (the siege windows)
    void MakeSystemMessage(L2ParamStack& Params, TCHAR* Out);        // 0x10081d20 (NConsoleWndText.cpp)
                                                                     //   systemmsg id, count, then
                                                                     //   (type, value) each

    // Declared by the windows that use them. Decompiled: the requests to the server
    // (NConsoleWndNet.cpp) and the data getters (NConsoleWndData.cpp); the rest in the other
    // NConsoleWnd*.cpp files. Names ours unless a guard string gave them; the
    // Request* ones pass the call on to the engine's network handler (UL2NetHandler at Engine
    // +0x5C) and are named after the chat command that uses them.
    INT GetIMEType();                 // 0x10064b10 kind of the keyboard layout's IME: 1 closed
                                      //   or none, 2..12 by layout (9, 10: Chinese), -1 other
    UBOOL SendChatMsgToServer(INT Type, TCHAR* Text, TCHAR* Target); // 0x10064ee0 (guard name) 0
    void RequestTrade(INT CreatureID);                               // 0x10066780 (guard name)
    User* GetSelectedActor();                                        // 0x10073ed0 (guard name) target
    User* GetMyUser();                                               // 0x10073e80 the player
    User* FindUserByName(TCHAR* Name);                               // 0x1006a770
    FVector GetMyLocation();                                         // 0x1006eb10 (C2: anonymous, the C1 twin was 0x10038540) where the player's pawn stands
    // the player's pawn and the macros (C2 only; names ours): the macro list is the macro window's (NCConsole +0xF8)
    APawn* GetMyPawn();                                              // 0x10073eb0 the first viewport's controller's pawn (no checks)
    __declspec(noinline) INT GetMacroCount();                                             // 0x1006abc0 0 without the macro window
    __declspec(noinline) NMacroEntry* GetMacro(INT Index);                                // 0x10074060 NULL out of 0..GetMacroCount
    UBOOL IsAnyMacroRunning();                                       // 0x10074090 a macro with bRunning set
    TCHAR* GetMacroName(INT MacroID);                                // 0x100740f0 the name of the macro with that id, NULL when none
    INT GetMacroIconType(INT MacroID);                               // 0x10074150 its icon number, -1 when none
    void StartMacro(INT MacroID);                                    // 0x10074e00 sets bRunning of the macro with that id
    UBOOL AddMacro(NMacroEntry* Macro);                              // 0x1006ab80 into the macro window's list; 0 when there is none
    INT GetHennaSlotCount();                                         // 0x10074450 1..3 by the profession tier of the class (-1: no player)
    UBOOL HasItemAdditionalName(INT ItemID);                         // 0x1006c040 the item's additional name is not "NAME_None"
    const TCHAR* GetItemAdditionalName(INT ItemID);                  // 0x1006c2e0
    TCHAR* GetPartyMemberName(INT Index);                            // 0x10065af0 the party window's member, NULL out of range
    UBOOL GetMousePos(INT* X, INT* Y);                               // 0x1006a2e0 the window manager's (tail call)
    void RequestGMSnoopEnd(INT SnoopID);                             // 0x10069be0 NetHandler slot 251 (a tail call)
    void StartGameGuard(INT hWnd);                                   // 0x10060620 (C1 only: no C2 match) (name ours) the game's side of the nProtect GameGuard start-up
    void ScaleRect(INT W0, INT H0, INT W1, INT H1, NResizeRect* Rect); // (C2 only: no C3 function, nobody calls it) the place a window keeps in proportion when the desktop goes from W0 x H0 to W1 x H1
    void ShowChatText(const TCHAR* Text, DWORD Color);               // 0x1006a2a0 the desktop's AddSystemText with sub type 5
    void GetNumberMessage(INT MsgID, INT Number, TCHAR* Out);        // 0x10069df0 the text of a system message with one number
    __declspec(noinline) void ShowSystemMessage(INT ID, DWORD Color); // 0x1007d380 systemmsg line (Color is not read in C2)
    __declspec(noinline) void PlaySoundName(const TCHAR* Name);                           // 0x1006c310 (name ours) the interface sound of a name
    void PlayInterfaceSound(INT SoundID);                            // 0x10074e60 (name ours) the sound of a number: the table
                                                                     //   of names at 0x1016a0a0 (0 click_1 ... 10 petition)
    void ShowDialog(NDialogInfo* Info);                              // 0x10072e20
    void ExecCommand(TCHAR* Command);                                // 0x10072df0 viewport Exec
    void RequestPetition(TCHAR* Text, INT Type);                     // 0x10069140 (C2: Type is new, passed on: name ours)
    void RequestPetitionCancel();                                    // 0x10069150
    void RequestGMList();                                            // 0x10069160
    void RequestSocialAction(INT Action);                            // 0x100683f0
    void RequestChangeMoveType(UBOOL bRun);                          // 0x10068410
    void RequestChangeWaitType(UBOOL bStand);                        // 0x10068400
    void RequestAttack(INT ObjectID, FVector Origin, INT bForceAttack = 0, INT bShift = 0); // 0x10071c00 (C2: two more words, ret 0x18)
    void RequestAttackForce(INT ObjectID, FVector Origin);           // 0x100683b0
    void RequestAttackStand(INT ObjectID, FVector Origin);           // 0x10054710 (C1 only: no C2 match) (C1 only: no C2 match)
    void RequestTarget(INT ObjectID);                                // 0x100718b0
    void RequestTargetNext(INT Range, INT CurrentID);                // 0x100719c0
    void RequestPickUp(INT Range, INT ObjectID);                     // 0x10071930
    void RequestAssist();                                            // 0x1007a210
    void RequestPartyInvite(L2ParamStack& Params);                   // 0x10068a80
    void RequestPartyLeave(L2ParamStack& Params);                    // 0x10068aa0
    void RequestPartyDismiss(L2ParamStack& Params);                  // 0x10068ab0
    void RequestVendor();                                            // 0x100698a0
    void RequestMagicSkillUse(L2ParamStack& Params);                 // 0x10068040 (the macro list; the engine header: DeleteMacro)
    void RequestSkillList(L2ParamStack& Params);                     // 0x10068050 (the chat commands; the engine header: MagicSkillUse)
    void RequestMakeMacro(L2ParamStack& Params);                     // 0x10068020 NetHandler +0x264
    void RequestDeleteMacro(L2ParamStack& Params);                   // 0x10068030 NetHandler +0x26C (C3 name; the engine header: OpenMinimap)
    void BypassUserCmd(L2ParamStack& Params);                        // 0x10069ff0 (C2 only; name ours) NetHandler slot 248
    void RequestRecipeBookOpen(L2ParamStack& Params);                 // 0x100680a0 NetHandler +0x354
    void RequestRecipeItemDelete(INT P1);                             // 0x100680b0 (C2 only; name ours) NetHandler slot 200
    void RequestRecipeItemMakeInfo(INT P1);                           // 0x100680c0 (C2 only; name ours) NetHandler slot 201
    void RequestRecipeItemMakeSelf(INT P1);                           // 0x100680d0 (C2 only; name ours) NetHandler slot 202
    void RequestRecipeShopMessageSet(TCHAR* P1);                      // 0x100680e0 NetHandler +0x368
    void RequestRecipeShopListSet(L2ParamStack& P1);                  // 0x100680f0 NetHandler +0x36C
    void RequestRecipeShopManageQuit();                               // 0x10068100 NetHandler +0x370
    void RequestRecipeShopManageCancel();                             // 0x10068110 NetHandler +0x374
    void RequestRecipeShopSellList(INT P1);                           // 0x10068120 NetHandler +0x378
    void RequestRecipeShopMakeInfo(INT P1, INT P2);                   // 0x10068130 NetHandler +0x37C
    void RequestRecipeShopMakeDo(INT P1, INT P2, INT P3);             // 0x10068140 NetHandler +0x380 (three values, ret 0xC)
    void RequestHennaItemInfo(INT P1);                                // 0x10068190 (C2 only; name ours) NetHandler slot 211
    void RequestHennaEquip(INT P1);                                   // 0x100681a0 (C2 only; name ours) NetHandler slot 212
    void RequestHennaUnequipInfo(INT P1);                             // 0x100681b0 (C2 only; name ours) NetHandler slot 213
    void RequestHennaUnequip(INT P1);                                 // 0x100681c0 (C2 only; name ours) NetHandler slot 214
    void UseShortcut(L2ParamStack& Params);                          // 0x10044860 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    void UseShortCutEquip(INT Index);                                // 0x1007a130 (C2 only; name ours) the item of a shortcut slot, a weapon or armor not worn yet: put on
    void RequestAllyInvite(L2ParamStack& Params);                    // 0x10068790
    void RequestAllyInfo();                                          // 0x100687c0
    void RequestFriendInvite(L2ParamStack& Params);                  // 0x10068990
    void RequestFriendDel(L2ParamStack& Params);                     // 0x100689a0
    void RequestFriendList();                                        // 0x100689c0
    void RequestBlock(L2ParamStack& Params);                         // 0x100689d0
    void RequestBuy();                                               // 0x10069a80
    void RequestDwarvenManufacture();                                // 0x1006a000
    // used by the dialog box (NCDialogBox::OnOK / OnCancel)
    void ConvertNewLines(TCHAR* Text, FString& Result);              // 0x1006b130 escaped newlines
    UBOOL HasInvalidChars(TCHAR* Text);                              // 0x1006a570 empty, or a
                                                                     //   character the font lacks
    UBOOL HasBadWord(TCHAR* Text);                                   // 0x1004a190 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) word filter
    UBOOL IsValidName(TCHAR* Text);                                  // 0x10081c50
    void RegisterPledgeCrest(L2ParamStack& Params);                  // 0x10080bf0 bmp file path
    void RegisterAllyCrest(L2ParamStack& Params);                    // 0x10080ec0
    void SetPrivateStoreMsg(TCHAR* Text);                            // 0x100698e0
    void SetPrivateStoreBuyMsg(TCHAR* Text);                         // 0x10069ac0
    void RequestChangePetName(TCHAR* Name);                          // 0x10069020
    void RequestAnswerJoinParty(L2ParamStack& Params);               // 0x10068a90
    void RequestAnswerJoinPledge(L2ParamStack& Params);              // 0x10068500 NetHandler +0x1D4
    void RequestAnswerJoinAlly(L2ParamStack& Params);                // 0x100687a0
    void RequestAnswerFriendInvite(L2ParamStack& Params);            // 0x100689b0
    void RequestGiveNickName(L2ParamStack& Params);                  // 0x10068590 NetHandler +0x204 member title
    void RequestOustPledgeMember(L2ParamStack& Params);              // 0x10068510 NetHandler +0x1D8
    void RequestDismissPledge();                                     // 0x10068520 NetHandler +0x1DC
    void RequestWithdrawalPledge();                                  // 0x100685b0 NetHandler +0x1E4
    void RequestStartPledgeWar(L2ParamStack& Params);                // 0x10068530 NetHandler +0x1E8
    void RequestReplyStartPledgeWar(L2ParamStack& Params);           // 0x10068540 NetHandler +0x1EC
    void RequestStopPledgeWar(L2ParamStack& Params);                 // 0x10068550 NetHandler +0x1F0
    void RequestReplyStopPledgeWar(L2ParamStack& Params);            // 0x10068560 NetHandler +0x1F4
    void RequestSurrenderPledgeWar(L2ParamStack& Params);            // 0x10068570 NetHandler +0x1F8
    void RequestSurrenderPersonally(L2ParamStack& Params);           // 0x10068580 NetHandler +0x200
    void RequestOustAlly(L2ParamStack& Params);                      // 0x100687b0
    void RequestWithdrawAlly();                                      // 0x100687d0
    void RequestDismissAlly();                                       // 0x100687e0
    void RequestActionUse(L2ParamStack& Params);                     // 0x100683a0
    // used by the login screens (NCAuthWnd, NCLoginWnd, NCServerSelectWnd)
    UBOOL RequestAuthLogin(const TCHAR* Account, const TCHAR* Password); // 0x10064fd0 (guard name)
    UBOOL RequestAuthCardKeyLogin(INT CardID, INT Key);              // 0x10065060 (guard name; C2 only) NetHandler slot 68 (RequestSCCheck)
    void RequestServerLogin(L2ParamStack& Params);                   // 0x100656c0 (guard name)
    // ShowMessageWnd (the message window) is a virtual in C2: slot 317
    // used by the clan and party matching windows
    UTexture* GetPledgeCrestTexture(INT PledgeID);                   // 0x1006a8e0 16x12 crest
    UTexture* GetPledgeCrestTex(INT PledgeID);                       // 0x1006a900 (C4) NetHandler slot 45, by the clan's id (names ours)
    UTexture* GetAllyCrestTex(INT AllyID);                           // 0x1006a910 (C4) NetHandler slot 46, by the alliance's id
    void RequestJoinPledge(L2ParamStack& Params);                    // 0x100685a0 NetHandler +0x1E0 invitation
    void RequestPartyMatchConfig(L2ParamStack& Params);              // 0x10054db0 (C1 only: no C2 match) (C1 only: no C2 match)
    void RequestPartyMatchDetail(INT ObjectID);                      // 0x10054dd0 (C1 only: no C2 match) (C1 only: no C2 match)
    const TCHAR* GetClassNameString(INT ClassID);                    // 0x1006b250 SysString[247 + ID]
    // used by the menus and the option windows
    void RequestRestartPoint(L2ParamStack& Params);                  // 0x1005ae60 (C1 only: no C2 match) restart menu
                                                                     //   choice (0 village .. 4 fixed)
    void RequestRestart(L2ParamStack& Params);                       // 0x10065530 back to the
                                                                     //   character selection
    // RequestLogout (quit the game) is a virtual in C2: slot 315
    // C1 virtuals (slots 269 and 270) with no function in C2 (the pad of the shortcut bar is gone)
    void SetPadShift1(BYTE bShift);                                  // 0x10047b50 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) ShortCutWnd->bPadShift1
    void SetPadShift2(BYTE bShift);                                  // 0x10021510 (C1 only: no C2 match) ShortCutWnd->bPadShift2
    void ResetWindowPositions();                                     // 0x100694D0 (C1 only: no C2 match) every window back
                                                                     //   to its default place
    void ReloadGameTexts();                                          // 0x1006a6b0 after the item /
                                                                     //   skill texts were reloaded
    // used by the status windows (player, target, party, buffs)
    void TargetMyself();                                             // 0x10071820 click on the
                                                                     //   player window
    AActor* GetSelectedStaticActor();                                // 0x10073f80 door or other
                                                                     //   static target
    FServerStaticObject* FindStaticObjectInfo(INT CreatureID);       // 0x1006a790
    PetInfo GetPetInfo();                                            // 0x1006a750
    BYTE IsSiegeNpc(INT NpcID);                                      // 0x10057320 (C1 only: no C2 match) (C1 only: no C2 match) castle guards,
                                                                     //   teleporters, mercenaries
    PledgeInfo* GetPledgeInfo(INT PledgeID);                         // 0x1006a8d0
    void RequestTargetCancel(L2ParamStack& Params);                  // 0x10068390
    void AssistMember(INT ObjectID, FVector Origin);                 // 0x10071a60 targets what
                                                                     //   the member targets
    INT GetSkillRange(INT SkillID, INT Level);                       // 0x1006ab50 skill data +0x2C
    TCHAR* GetSkillName(INT SkillID, INT Level);                     // 0x1006a950
    TCHAR* GetSkillDesc(INT SkillID, INT Level);                     // 0x1006a980
    Item* FindItemByID(INT ObjectID);                                // 0x1006a740 (C3; the C2 name) NetHandler +0x9C: the item on the ground
                                                                     //   30 (GM status: crest names)
    // used by the pet, summon and minimap windows
    void RequestAction(INT ObjectID, FVector Origin);                // 0x10071ba0 net handler
                                                                     //   slot 53 (click on it)
    void LockCursor();                                               // 0x1006a2f0
    void UnlockCursor();                                             // 0x1006a300
    // used by the action window
    void RequestCreatePledge(L2ParamStack& Params);                  // 0x100684f0 NetHandler +0x1D0 (the clan creation dialog)
    void RequestShortCutReg(L2ParamStack& Params);                   // 0x10044830 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) (type, slot,
                                                                     //   id...) + the drop sound
    void PetPickUp(INT Range, INT Param);                            // 0x10068fb0 nearest item
                                                                     //   (slot 210) to slot 189
    // used by the character selection and creation (NCLobbyWnd, NCPawnSelectWnd,
    // NCPawnCreateWnd) and the announcements (NCNoticeWnd)
    UBOOL RequestGameStart(INT Index);                               // 0x10065120 (guard name)
    UBOOL RequestCharacterDelete(INT Index);                         // 0x10065260 (guard name)
    UBOOL RequestCharacterRestore(INT Index);                        // 0x10065310 (guard name)
    void RequestNewCharacter();                                      // 0x100653c0 (guard name)
    void ReturnToLogin();                                            // 0x1006a1a0 "Previous" on
                                                                     //   the character selection
    void ReturnToCharacterSelect();                                  // 0x1006a250 creation cancelled
    void ShowWarningWnd();                                           // 0x10065540 after the
                                                                     //   announcements, when any
    void FormatSystemMessage(TCHAR* Out, const TCHAR* Format, const TCHAR* Param1,
                             const TCHAR* Param2);                   // 0x10046120 $s1 / $s2
    UBOOL RequestCharacterCreate(CharacterInfo Info);                // 0x10065440 (guard name)
    // used by the quest and item windows
    const TCHAR* GetQuestText(INT QuestID, INT Step);                // 0x100675d0 (name ours; it
                                                                     //   uses AddQuestID's guard)
    void GetItemSlotText(INT Type, INT BodyPart, FString& Result, INT SubType); // 0x1006acd0 the
                                                                     //   slot line of a tool tip
    // used by the shortcut bar and the skill windows
    INT RequestUseItem(INT ObjectID);                                // 0x10066320 (guard name; C3 returns the network handler's answer)
    void RequestPetUseItem(INT ObjectID);                            // 0x10068ff0 NetHandler slot 190
    void RequestShortCutDel(L2ParamStack& Params);                   // 0x10068280 (slot)
    const TCHAR* GetItemName(INT ItemID);                            // 0x1007d680 (guard name) the item's name
    const TCHAR* GetActionName(INT ActionID);                        // 0x10074010 (C2: FString at +0x1C) actionname-*.dat
    UBOOL IsToggleSkill(INT SkillID, INT Level);                     // 0x1006a9e0 skill data +0x14 == 3
    UBOOL HasAbnormal(INT SkillID);                                  // 0x100747d0 the skill's effect
                                                                     //   is on the player (buffs)
    UBOOL IsShotItem(INT ItemID);                                    // 0x1006e500 soul / spirit shots (the ones whose auto use can be turned on)
    INT GetSkillMpCost(INT SkillID, INT Level);                      // 0x1006ab20 skill data +0x1C
    const TCHAR* GetSkillTypeText(INT SkillID, INT Level);           // 0x1006aa10 "Active Skill",
                                                                     //   "Passive Skill" or "Magic"
    // used by the HTML viewer
    UBOOL HasNoAmpersand(TCHAR* Text);                               // 0x1006a5d0 0 when Text
                                                                     //   holds a '&'
    void RequestTutorialQuestionMark(INT ID);                        // 0x10068ee0 the tutorial
                                                                     //   question mark clicked
    // used by the desktop (NCConsole)
    UBOOL MoveToLocation(FVector Loc, UBOOL bNoZ);                   // 0x10074740 NetHandler slot 90
    User* FindUserByID(INT ObjectID);                                // 0x1006a780 NetHandler slot 33
    void RequestItemList();                                          // 0x100662a0 (guard name)
    void RequestPartyMatchList(INT Status);                          // 0x10054dc0 (C1 only: no C2 match) (C1 only: no C2 match) 3: leaves the room
    void RequestLinkHtml(L2ParamStack& Params);                      // 0x10068d30 HTML "Link"
    void RequestBBSWrite(L2ParamStack& Params);                      // 0x10068d50 HTML "Write"
    void RequestShowBoard(L2ParamStack& Params);                     // 0x10068d60 community board
    void RequestTutorialLinkHtml(L2ParamStack& Params);              // 0x10068ec0
    void RequestPrivateStoreQuitSell();                              // 0x100698d0
    void RequestPrivateStoreQuitBuy();                               // 0x10069ab0
    void RequestGMInventory(TCHAR* Name);                            // 0x10069170 GM command 5
    void RequestGMSkills(TCHAR* Name);                               // 0x10069190 GM command 3
    void RequestGMStatus(TCHAR* Name);                               // 0x100691b0 GM command 1
    void RequestGMQuests(TCHAR* Name);                               // 0x100691d0 GM command 4
    void RequestGMWarehouse(TCHAR* Name);                            // 0x100691f0 GM command 6
    void RequestGMPledge(TCHAR* Name);                               // 0x10069210 GM command 2
    // used by the trade window and the pet's inventory
    void RequestTradeDone(INT bConfirm);                             // 0x10066890 (guard name) OK 1,
                                                                     //   cancel 0
    void RequestGetItemFromPet(L2ParamStack& Params);                // 0x10069010 NetHandler slot
                                                                     //   192 (object id, amount, 0)
    // used by the inventory (NCInventoryWnd, NCInvenItemWnd, NCEquipItemWnd)
    void RequestUnEquipItem(L2ParamStack& Params);                   // 0x100663e0 body part bits
    void RequestGiveItemToPet(L2ParamStack& Params);                 // 0x10069000 object id, amount
    AActor* GetTargetedActor(UBOOL bCheck);                          // 0x1006a7a0 (guard name) the
                                                                     //   pawn under the mouse
    INT GetEquipSlot(INT BodyPart, INT ObjectID);                    // 0x100744e0 paperdoll slot,
                                                                     //   14 / 15: two-slot items
    const TCHAR* GetGradeText(INT Grade);                            // 0x1006b440 crystal grade
    UBOOL IsPetCollar(INT ItemID);                                   // 0x10046880 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) 2375, 3500..3502
    UBOOL IsMagicArmor(INT ItemID);                                  // 0x1007d730 armor type 3 (MP)
    const TCHAR* GetWeaponTypeName(INT Type);                        // 0x1006abe0 sysstring 43..504
    const TCHAR* GetAttackSpeedText(INT Speed);                      // 0x1006afc0 sysstring 306..310
    INT GetEnchantedDefense(INT Grade, INT Enchant, INT Base);       // 0x10060e60 (C1 only: no C2 match) (C1 only: no C2 match) base + bonus
    INT GetEnchantedAttack(INT Grade, INT Enchant, INT Base);        // 0x100742f0 (weapons)
    // used by the shop and warehouse windows (NCShopWnd, NCStoreWnd): (id, count) lists
    void RequestBuyItem(L2ParamStack& Params);                       // 0x10066c10 (guard name)
    void RequestBuySeed(L2ParamStack& Params);                       // 0x100669c0 (guard name; C2 only) NetHandler slot 90
    void RequestProcureCrop(L2ParamStack& Params);                   // 0x10066a40 (guard name; C2 only) NetHandler slot 91
    void RequestSellItem(L2ParamStack& Params);                      // 0x10066c90 (guard name)
    void RequestWarehouseDeposit(L2ParamStack& Params);              // 0x10066fd0 NetHandler slot 138
    void RequestWarehouseWithdraw(L2ParamStack& Params);             // 0x10066fe0 NetHandler slot 139
    // used by the private stores (names ours, after the packets; the quit requests, NetHandler
    // slots 198 and 205, are with the desktop's above)
    void RequestSetPrivateStoreListSell(L2ParamStack& Params);       // 0x100698b0 NetHandler slot
                                                                     //   196: count, then object id,
                                                                     //   count and price of each
    void RequestPrivateStoreManageCancel();                          // 0x100698c0 NetHandler slot 197
    void RequestPrivateStoreBuy(L2ParamStack& Params);               // 0x100698f0 NetHandler slot
                                                                     //   201: the owner's object id,
                                                                     //   then as above
    void RequestSetPrivateStoreListBuy(L2ParamStack& Params);        // 0x10069a90 NetHandler slot
                                                                     //   203: count, then item id,
                                                                     //   enchant, count and price
    void RequestPrivateStoreBuyManageCancel();                       // 0x10069aa0 NetHandler slot 204
    void RequestPrivateStoreSell(L2ParamStack& Params);              // 0x10069ad0 NetHandler slot
                                                                     //   208: the owner's object id,
                                                                     //   count, then object id and
                                                                     //   the above for each
    // the console's own window management (names ours unless a guard string gave them)
    UBOOL Initialize(HWND hWnd);                                     // 0x10061e10 (C1 only: no C2 match) (C1 only: no C2 match) every window
    void LoadWindowPositions();                                      // 0x10067a70 (C1 only: no C2 match) WindowsInfo.ini
    void SaveWindowPositions();                                      // 0x10066940 (C1 only: no C2 match) WindowsInfo.ini
    void Finalize();                                                 // 0x10075b40
    UBOOL InitializePawnViewer(HWND hWnd);                           // 0x100613a0 (C1 only: no C2 match) (C1 only: no C2 match)
    // the replay mode (C3 only; names from the stock guard strings). InitReplayMode is the C3 virtual 325, not declared
    // virtual (the placeholder NewVirtual325 below jumps to the stock address, where the hook is)
    UBOOL InitializeReplayMode(HWND hWnd);                           // 0x1007e1d0 (ret 4) Initialize for the replay
    UBOOL InitReplayMode(UEngine* InEngine);                         // 0x10082260 (ret 4) the same for the replay: the engine kept, the state InGame
    void SwitchUIMode(INT NewMode);                                  // 0x1007e5e0 (ret 4) 0 the game's windows, 1 the observer's, 2 the Olympiad's
    // C3 virtual 359 (not declared virtual: FL2NetNotify's OnEventMatchObserver, ret 0x30): mode 0 / 1 of a match observed
    void OnEventMatchObserver(INT A, ANSICHAR Mode, ANSICHAR C, FString D, FString E, INT F, INT G, INT H); // 0x1007ef20
    INT NSetTimer(UINT ID, UINT Elapse, NCObject* Owner);            // 0x10075f20 (NCObject::SetTimer)
    UBOOL NKillTimer(UINT ID, NCObject* Owner);                      // 0x1006edb0
    void SetCursor(INT Type, UTexture* Texture);                     // 0x1006a2c0 unless locked
    void UpdateCursor();                                             // 0x1007d880 every frame
    void UpdateTargetWindow();                                       // 0x1006a9d0 (C1 only: no C2 match) (C1 only: no C2 match) every tick
    INT DispatchWndMsgX(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam); // 0x1007f5d0
    UBOOL HookShortCutKey(UINT Msg, WPARAM wParam, LPARAM lParam);   // 0x1004f050 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    // the console's own text services and actions (names ours)
    void RemoveInvalidChars(TCHAR* Text);                            // 0x1006a610 the characters
                                                                     //   the font lacks
    AActor* GetPickupUnderMouse();                                   // 0x1006a890 an AL2Pickup
    void UseAction(INT ActionID, INT bForce = 0, INT bShift = 0);    // 0x1007e8d0 an action slot (C2: ret 0xC: Ctrl and Shift)
    void UseShortCut(INT Slot, INT bForce = 0, INT bShift = 0);      // 0x100802f0 (C2: ret 0xC) a shortcut slot
                                                                     //   (the keys, slot 263)

    // UObject's and FExec's
    virtual void Destroy();                                             // 10 0x1007e5c0
    virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) { return 0; } // FExec 0x100016f0

    // UL2ConsoleWnd's, implemented here (called by the engine), in the order of the C2 vtable: each slot has the
    // name of include/ue2/gen/UL2ConsoleWnd.vt.inc (tools/ue2_data/l2consolewnd_slots_c2.csv says how it was
    // found). The UGameEngine::On* handler of FL2NetNotify that calls a slot names it (without "On") unless a
    // guard string does; names ours otherwise (vfnNNN: not known). The slots only C2 has are in NConsoleWndC2.cpp
    // (the manor's in NConsoleWndItems.cpp, 314 in NConsoleWndResize.cpp).
    virtual void Tick(FLOAT DeltaTime);                                 // 25 0x1007f9e0
    // WndMain->IsOverWindow
    virtual UBOOL IsOverWindow(INT X, INT Y);                           // 27 0x10064cb0
    virtual void ShowWarningDialog(INT P1, INT P2);                                         // 28 0x1006bfb0 (name ours)
    virtual void GameGuardQuery(INT P1, INT P2, INT P3, INT P4, INT P5); // 29 0x1006d870 (C4: five values, ret 0x14)
    void GameGuardQueryX(INT P1, INT P2, INT P3, INT P4, INT P5);       // 0x1006d870 the body (hook mode: hooked here, the slot jumps to it)
    virtual void AuthLoginSuccess() {}                                  // 30 0x10043b50
    virtual void SCCheckStart(INT P1, INT P2);                                         // 31 0x1006f5b0 (name ours)
    virtual void AuthLoginFail(INT Reason);                             // 32 0x1006f1d0
    // the announcements (NCNoticeWnd)
    virtual void StartNotice(L2ParamStack& Params);                     // 33 0x1006f5d0
    virtual void CharacterCreateFail(INT Reason);                       // 34 0x100658f0
    virtual void ReceiveCharacterInfo(INT bNoVoice);                    // 35 0x1006f8b0 (C4: ret 4)
    void ReceiveCharacterInfoX(INT bNoVoice);                           // 0x1006f8b0 the body (hook mode: hooked here, the slot jumps to it)
    virtual void NewVirtual36();  // 36 0x10065a00 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void AddCharacterInfo(User Info);                           // C4: slot 36 is the engine's NewVirtual36 here, no longer virtual
    virtual void AcceptGameStart(INT Result);                           // 37 0x1006fa20
    virtual void AcceptCharacterDelete(INT Result);                     // 38 0x10065b40
    virtual void AcceptNewCharacter(INT Result);                        // 39 0x1006fbd0
    virtual void AcceptCharacterCreate(INT Result);                     // 40 0x1006fd20
    // the same body as QuitGame (321)
    virtual void AcceptLogOut();                                        // 41 0x10076ad0
    virtual void ServerClose(INT Reason);                               // 42 0x10076ae0
    virtual void ReviveRequest(INT Param1, INT Param2);                 // 43 0x10004020
    // OnStartServerList
    virtual void ReceiveServerList(INT LastServerID);                   // 44 0x10065740
    virtual void AddServerList(L2ParamStack& Params);                   // 45 0x100657c0
    virtual void EndServerList();                                       // 46 0x10065850

    // the target, the inventory, the trade, the NPC shops and the warehouse
    virtual void MyTargetSelected(INT LevelDiff);                       // 47 0x10071cf0
    virtual void ReceiveItemList(INT bShow);                            // 48 0x10066410
    virtual void ReceiveUpdateItemList();                               // 49 0x100664c0 no handler
    virtual void ReceiveTradeStart(INT ObjectID);                       // 50 0x10077460
    // the engine's ItemInfo, by value
    virtual void ReceiveAddTradeInventoryItem(NItemRequest Item);       // 51 0x100775b0
    virtual void ReceiveAddTradeOwnItem(NItemRequest Item);             // 52 0x100778b0
    virtual void ReceiveAddTradeOtherItem(NItemRequest Item);           // 53 0x10077bb0
    virtual void ReceiveTradeUpdate(INT Change, ItemInfo& Item);        // 54 0x10077eb0
    virtual void ReceiveTradeDoneMsg(INT Result);                       // 55 0x10066920
    virtual void AddRecommandedData(RecommandedDataStr Data);           // 56 0x10066150
    virtual void EndRecommandedData();                                  // 57 0x100661e0
    virtual void AddInventoryItem(ItemInfo& Item);                      // 58 0x10076c70
    // OnReceiveUpdateItemList
    virtual void UpdateInventoryItem(INT Change, ItemInfo& Item);       // 59 0x10076fd0
    virtual void CheckGeo();                                            // 60 0x10066540 (no argument: the stock pops nothing)
    virtual void NewVirtual61();  // 61 0x10066e40 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual62();  // 62 0x10078aa0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void StartBuyList(INT Adena, INT ListID);                   // 63 0x10066d10 NPC shop
    virtual void AddBuyList(ItemInfo& Item);                            // 64 0x10078790
    virtual void StartSellList(INT Adena, INT ListID);                  // 65 0x10066f00
    virtual void AddSellList(ItemInfo& Item);                           // 66 0x10078d70
    virtual void StartManorBuyList(INT Adena, INT ListID);              // 67 0x10066ac0
    virtual void AddManorBuyList(ItemInfo& Item);                       // 68 0x100781b0
    virtual void StartManorSellList(INT Adena, INT ListID);             // 69 0x10066b50
    virtual void AddManorSellList(ItemInfo& Item);                      // 70 0x100784a0
    virtual void NewVirtual71();  // 71 0x10066ff0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void StartDepositList(INT Type, INT Adena);                 // 0x10066ff0 C4: slot 71 is the engine's NewVirtual71 here (the stock takes a kind of warehouse first, ret 8)
    virtual void AddDepositList(ItemInfo& Item);                        // 72 0x10079060
    virtual void NewVirtual73();  // 73 0x100670b0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void StartWithdrawList(INT Type, INT Adena);                // 0x100670b0 C4: slot 73 is the engine's NewVirtual73 here (ret 8)
    virtual void AddWithdrawList(ItemInfo& Item);                       // 74 0x10079350
    // OnReceiveWareHouseDonePacket
    virtual void ReceiveWareHouseDone(L2ParamStack& Params);            // 75 0x10080020
    virtual void UpdatePlayerInfo(L2ParamStack& Params);                // 76 0x10069f90

    // HTML pages, the tutorial, the radar
    virtual void NpcHtmlMessage(L2ParamStack& Params);                  // 77 0x10081520
    virtual void TutorialShowHtml(TCHAR* Html);                         // 78 0x10068d70
    virtual void TutorialShowQuestionMark(INT ID);                      // 79 0x10068f20
    virtual void TutorialEnableClientEvent(INT Events);                 // 80 0x10068f80
    virtual void TutorialCloseHtml();                                   // 81 0x10068f90
    // CheckTutorialClientEvent; once per pending bit (+0x3724)
    virtual void SendTutorialEvent(INT Event);                          // 82 0x10068ef0
    virtual void ShowRadar(INT Param1, INT Param2, INT X, INT Y, INT Z); // 83 0x100695c0
    virtual void DeleteRadar(INT X, INT Y, INT Z);                      // 84 0x10069790

    // the clan, the alliance, the friends, the party
    // empty, folded
    virtual void NewVirtual86();  // 86 0x100023c0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void ReceivePledgeExtendedInfo(PledgeInfo& Info);           // C4: slot 86 is the engine's NewVirtual86 here, no longer virtual (empty; its NW_TRACE runs only if NewVirtual86 jumps here: the standalone slot is the stock's `ret 4`)
    virtual void NewVirtual85();  // 85 0x10070000 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // standalone (stage 5): the engine's pure slots this console leaves to the stock; each is a placeholder (NNewVirtuals.gen.cpp): in the hook build it jumps to the stock
    // function, in the standalone it tail-jumps to the real non-virtual member of that address (no parameters here: the member has the real signature and `ret N`)
    virtual void NewVirtual167();  // 167 0x10068350 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual169();  // 169 0x10068370 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual192();  // 192 0x10068DC0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual203();  // 203 0x10066040 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual204();  // 204 0x10066070 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual207();  // 207 0x100660A0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual286();  // 286 0x10069480 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual290();  // 290 0x100695B0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual296();  // 296 0x1007BDB0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual311();  // 311 0x1006CA40 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual313();  // 313 0x1006CA60 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual319();  // 319 0x10072D90 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual364();  // 364 0x10074D60 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual366();  // 366 0x100023C0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual371();  // 371 0x100753B0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual372();  // 372 0x100663F0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual373();  // 373 0x100826B0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual374();  // 374 0x100826E0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual375();  // 375 0x10082700 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual376();  // 376 0x10082730 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual379();  // 379 0x10082880 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual397();  // 397 0x1006DA80 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual398();  // 398 0x1006DAA0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual399();  // 399 0x1006DAE0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual400();  // 400 0x1006DB00 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual402();  // 402 0x1006DB60 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual405();  // 405 0x1006E490 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual406();  // 406 0x1006E4D0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual411();  // 411 0x10066120 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual412();  // 412 0x10066140 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual415();  // 415 0x10082690 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    virtual void ReceivePledgeInfo(PledgeInfo& Info);                   // 87 0x10071db0
    virtual void ReceivePledgeInfoUpdate(PledgeInfo& Info);             // 88 0x10071f00
    virtual void ReceivePledgeMemberList();                             // 89 0x100685c0
    virtual void AddPledgeMember(L2ParamStack& Params);                 // 90 0x10072010
    virtual void UpdatePledgeMember(L2ParamStack& Params);              // 91 0x10072100
    virtual void DeletePledgeMember(TCHAR* Name);                       // 92 0x10068650
    virtual void DeleteAllPledgeMember();                               // 93 0x100686e0
    virtual void ReceiveCreatePledge(L2ParamStack& Params);             // 94 0x100811c0
    virtual void ReceiveAskJoinPledge(L2ParamStack& Params);            // 95 0x1007a360
    virtual void ReceiveOustPledgeMember(L2ParamStack& Params);         // 96 0x10081200
    virtual void ReceiveDismissPledge(L2ParamStack& Params);            // 97 0x10081240
    virtual void ReceiveJoinPledge(L2ParamStack& Params);               // 98 0x10081190
    // OnReceiveWidthdrawalPledge
    virtual void ReceiveWithdrawalPledge(L2ParamStack& Params);         // 99 0x10081280
    virtual void ReceiveOustPledgeMessage(L2ParamStack& Params);        // 100 0x100812c0
    virtual void ReceiveDismissPledgeMessage(L2ParamStack& Params);     // 101 0x100812e0
    virtual void ReceiveStartPledgeWar(L2ParamStack& Params);           // 102 0x1007a470
    virtual void ReceiveReplyStartPledgeWar(L2ParamStack& Params);      // 103 0x10081320
    virtual void ReceiveStopPledgeWar(L2ParamStack& Params);            // 104 0x1007a590
    virtual void ReceiveReplyStopPledgeWar(L2ParamStack& Params);       // 105 0x10081360
    virtual void ReceiveSurrenderPledgeWar(L2ParamStack& Params);       // 106 0x100813a0
    virtual void ReceiveSurrenderPersonally(L2ParamStack& Params);      // 107 0x100813d0
    virtual void ReceiveAskJoinParty(L2ParamStack& Params);             // 108 0x1007ae20
    virtual void ReceiveJoinParty(L2ParamStack& Params);                // 109 0x10081480
    virtual void ReceiveWithdrawalParty(L2ParamStack& Params);          // 110 0x10068ac0
    virtual void ReceiveOustPartyMember(L2ParamStack& Params);          // 111 0x100814b0
    virtual void ReceiveOustPartyMessage(L2ParamStack& Params);         // 112 0x100814e0
    virtual void ReceiveDismissParty(L2ParamStack& Params);             // 113 0x10068b00
    // OnRecevieDismissPartyMessage
    virtual void ReceiveDismissPartyMessage(L2ParamStack& Params);      // 114 0x10081500
    virtual void NewVirtual115();  // 115 0x10068b40 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void ReceiveAskJoinAlly(L2ParamStack& Params);              // 116 0x1007a6b0
    virtual void ReceiveDismissAlly(L2ParamStack& Params);              // 117 0x10068870
    virtual void ReceiveJoinAlly(L2ParamStack& Params);                 // 118 0x10081400
    virtual void ReceiveOustAlly(L2ParamStack& Params);                 // 119 0x10068830
    virtual void ReceiveWithdrawAlly(L2ParamStack& Params);             // 120 0x100687f0
    virtual void ReceiveFriendAddRequestResult(L2ParamStack& Params);   // 121 0x10081440
    virtual void ReceiveFriendAddRequest(L2ParamStack& Params);         // 122 0x1007ad60
    // empty, folded
    virtual void ReceiveFriendAdd(L2ParamStack& Params) {}              // 123 0x100023c0
    // empty, folded
    virtual void ReceiveFriendRemove(L2ParamStack& Params) {}           // 124 0x100023c0
    // empty, folded
    virtual void ReceiveFriendList(L2ParamStack& Params) {}             // 125 0x100023c0
    // empty, folded
    virtual void ReceiveFriendStatus(L2ParamStack& Params) {}           // 126 0x100023c0

    // new in C2 (recipes, hennas, the minimap, ...): nobody has named them yet, except the two UL2NetHandler forwarders
    virtual void NewVirtual127();  // 127 0x10070740 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void ReceiveRecipeBookItemList(INT P1, INT P2);                                 // 0x10070740 (ret 8; C4: slot 127 is the engine's NewVirtual127 here, no longer virtual) (name ours)
    virtual void AddRecipeBookItem(INT RecipeID);                                         // 128 0x100799d0 (name ours)
    virtual void NewVirtual129();  // 129 0x10070840 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void ReceiveRecipeItemMakeInfo(L2ParamStack& Params);                           // 0x10070840 (ret 4: five values; C4: slot 129 is the engine's NewVirtual129 here, no longer virtual) (name ours)
    virtual void NewVirtual130();  // 130 0x100709b0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void ReceiveRecipeShopManageList(INT P1, INT P2, INT P3);                       // 0x100709b0 (ret 0xC; C4: slot 130 is the engine's NewVirtual130 here, no longer virtual) (name ours)
    virtual void AddRecipeShopManageItem(INT RecipeID, INT P2);                                         // 131 0x10079b00 (name ours)
    virtual void AddRecipeShopRegisterdItem(INT RecipeID, INT P2, INT P3);                                         // 132 0x10079c40 (name ours)
    virtual void ReceiveRecipeShopSellList(INT P1, INT P2, INT P3, INT P4);                                         // 133 0x10070a30 (name ours)
    virtual void AddRecipeShopSellItem(INT RecipeID, INT P2, INT P3);                                         // 134 0x10079d90 (name ours)
    virtual void ReceiveRecipeShopItemInfo(INT P1, INT P2, INT P3, INT P4, INT P5, INT P6);                                         // 135 0x10070b50 (name ours; C4: ret 0x18)
    void ReceiveRecipeShopItemInfoX(INT P1, INT P2, INT P3, INT P4, INT P5, INT P6);   // 0x10070b50 the body (hook mode: hooked here, the slot jumps to it)
    virtual void ReceiveMacroListInfo(char P1, char P2, MacroInfo Macro, char P4, INT P5);                                         // 136 0x10079ed0 (name ours)
    virtual void ReceiveHennaEquipList(INT P1, INT P2);                                         // 137 0x10070cc0 (name ours)
    virtual void AddHennaEquipInfo(HennaInfo Info);                                         // 138 0x10070dd0 (name ours)
    virtual void ReceiveHennaItemInfo(HennaInfo Info, INT P2);                                         // 139 0x10070ef0 (name ours)
    virtual void ReceiveHennaInfo(HennaInfo Info, INT P2);                                         // 140 0x10071030 (name ours)
    virtual void AddHennaInfo(INT P1, INT P2);                                         // 141 0x10068150 (name ours)
    virtual void ReceiveHennaUnequipList(INT P1, INT P2);                                         // 142 0x10071070 (name ours)
    virtual void AddHennaUnequipInfo(HennaInfo Info);                                         // 143 0x10071180 (name ours)
    virtual void ReceiveHennaUnequipItemInfo(HennaInfo Info, INT P2);                                         // 144 0x100712e0 (name ours)
    virtual void RequestHennaItemList();                                // 145 0x100681d0
    virtual void RequestHennaUnequipList();                             // 146 0x100681e0
    virtual void GMAddHennaInfo(INT P1, INT P2);                                         // 147 0x10068170 (name ours)
    virtual void GMReceiveHennaInfo(HennaInfo Info, INT P2);                                         // 148 0x100712a0 (name ours)
    virtual void ReceiveBlockList(L2ParamStack& Params) {}              // 149 0x100023c0

    // the inventory and character toggles, the shortcut keys and the Ctrl cursor (C1 slots 261-270 moved here)
    virtual void ToggleInventory();                                     // 150 0x100681f0
    // no handler; Desktop->ToggleCharacter(0)
    virtual void ShowCharacterStatus();                                 // 151 0x10068230
    virtual void UseShortCutSlot(INT Index);                            // 152 0x10083a10
    virtual void ChangeShortCutPage(INT bNext);                         // 153 0x10068240
    // no handler; the pawn after (or before) CreatureID among the pawns in an area (ULevel slot 57 in C2)
    virtual User* FindNextPawnInArea(FLOAT Radius, INT CreatureID, INT bNext, INT P4, INT P5, INT P6, INT P7); // 154 0x10074ac0
    // the desktop's view rectangle (NCConsole +0x1C0..0x1CC in C2: left, top, width, height)
    virtual void SetViewRect(INT Left, INT Top, INT Right, INT Bottom); // 155 0x10074c20
    // bCtrlDown = 1
    virtual void PressCtrl();                                           // 156 0x1006a310
    // bCtrlDown = 0
    virtual void ReleaseCtrl();                                         // 157 0x1006a320

    // shortcuts, buffs, quests, chat, party, skills, system messages
    virtual void ReceiveShortCutList();                                 // 158 0x10068290
    virtual void AddShortCutItem(L2ParamStack& Params);                 // 159 0x100805e0
    virtual void UpdateShortCutItem(INT ObjectID);                      // 160 0x100682d0
    virtual void UpdatePetShortCutItem(INT ObjectID);                   // 161 0x100682f0
    virtual void ReceiveShortCutDelete(INT Slot);                       // 162 0x10068310
    virtual void UpdateMacroShortCut(INT P1, INT P2);                                         // 163 0x100682b0 (name ours)
    virtual void ReceiveMagicSkillList(L2ParamStack& Params);           // 164 0x10068070
    virtual void ReceiveAbnormalStatus();                               // 165 0x10068330
    virtual void AddAbnormalStatus(L2ParamStack& Params);               // 166 0x10071420
    virtual void NewVirtual168();  // 168 0x100716d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual170();  // 170 0x10071580 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void ReceiveQuestList();                                    // 171 0x10067270
    virtual void NewVirtual172();  // 172 0x10067300 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void AddQuestID(INT QuestID, INT Step);                     // 173 0x10067390
    virtual void AddQuestItem(ItemInfo& Item);                          // 174 0x100023c0
    // a chat line
    virtual UBOOL Say2(L2ParamStack& Params);                           // 175 0x1007d200
    // no handler; the debug window
    virtual void VARARGS DebugLogf(INT Category, const TCHAR* Fmt, ...); // 176 0x10069d90
    virtual void ReceiveVersionCheck(L2ParamStack& Params);             // 177 0x10065c90
    virtual void UpdateEquipItem(L2ParamStack& Params);                 // 178 0x100666c0
    // OnPartyAddMemberPacket
    virtual void PartyAddMember(L2ParamStack& Params);                  // 179 0x10068c60
    virtual void PartyUpdateMember(L2ParamStack& Params);               // 180 0x10068c90
    virtual void PartyDeleteMember(L2ParamStack& Params);               // 181 0x10068cc0
    virtual void PartyDeleteAll(L2ParamStack& Params);                  // 182 0x10068d00
    void ReceivePartyMatchList();                               // C3: slot gone (was 174), no longer virtual
    void AddPartyMatchMember(L2ParamStack& Params);             // C3: slot gone (was 175), no longer virtual
    void ReceivePartyMatchDetail(L2ParamStack& Params);         // C3: slot gone (was 176), no longer virtual
    // empty, folded
    virtual void ShowMagicWnd() {}                                      // 183 0x10043b50
    virtual void ShowSkillWnd();                                        // 184 0x10068080
    // OnSkillListPacket
    virtual void ReceiveSkillList(L2ParamStack& Params);                // 185 0x100705c0
    // OnMagicListPacket; empty, folded
    virtual void ReceiveMagicList(L2ParamStack& Params) {}              // 186 0x100023c0
    virtual void ReceiveTradeDemand(L2ParamStack& Params);              // 187 0x10077350
    // OnRestartResponse, first
    virtual void RestartResetWindows();                                 // 188 0x10065da0
    // OnRestartResponse, then
    virtual void RestartToLobby();                                      // 189 0x10065e80
    virtual void AddSystemMessageParam(L2ParamStack& Params);           // 190 0x10081610
    virtual void NewVirtual191();  // 191 0x10081700 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void EndSystemMessageParam(L2ParamStack& Params, INT P2, INT P3, INT P4); // 0x10081700 (ret 0x10; C4: slot 191 is the engine's NewVirtual191 here, no longer virtual)

    // board, enchant, skill trainer, restart, VIP shop, chat channels, pets
    // OnShowboardPacket
    void ShowBoard(INT bShow, L2ParamStack& Params);            // 0x10068dc0 (ret 8; C4: slot 192 is the engine's NewVirtual192 here, no longer virtual) the byte hides the board when 0
    // NetHandler 164
    virtual void NewVirtual193();  // 193 0x1006a020 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual194();  // 194 0x1006a010 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void BuilderCommand(L2ParamStack& Params);                  // C4: slot 194 is the engine's NewVirtual194 here, no longer virtual
    virtual void NewVirtual195();  // 195 0x10066560 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void SelectItemToEnchant(INT ItemID);                       // 0x10066560 (ret 4; C4: slot 195 is the engine's NewVirtual195 here, no longer virtual) the item to enchant (-1: none)
    virtual void NewVirtual196();  // 196 0x10066690 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void ReceiveEnchantResult(INT Result);                      // C4: slot 196 is the engine's NewVirtual196 here, no longer virtual
    virtual void ReceiveAction(INT Action);                             // 197 0x1007a2f0
    virtual void ShowTrainListWnd();                                    // 198 0x10067f70
    virtual void ShowTrainInfoWnd(L2ParamStack& Params);                // 199 0x10083740
    virtual void AddAcquireSkillInfo(L2ParamStack& Params);             // 200 0x100703f0
    virtual void AddAcquireSkillExtendedInfo(L2ParamStack& Params);     // 201 0x10079730
    // no handler; the restart points after dying
    virtual void ShowRestartMenu(L2ParamStack& Params);                 // 202 0x1006fe20
    virtual void NewVirtual205();  // 205 0x10068000 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual206();  // 206 0x10066090 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void TeleportToLocation();                                  // C4: slot 206 is the engine's NewVirtual206 here, no longer virtual
    void HideTrainWnd();                                        // C3: slot gone (was 197), no longer virtual
    virtual void NewVirtual208();  // 208 0x100660b0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual228();  // 228 0x10069b10 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void SetVIPShopID(INT ShopID);                              // C4: slot 228 is the engine's NewVirtual228 here, no longer virtual
    virtual void NewVirtual229();  // 229 0x1007d030 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void AddVIPItemList(INT Slot, INT List, INT ProductID, INT ItemID, INT BodyPart, INT Param, INT Count, INT Enchant); // 0x1007d030 (ret 0x20; C4: slot 229 is the engine's NewVirtual229 here, no longer virtual; C3: ret 0x18)
    virtual void NewVirtual209();  // 209 0x10067830 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual210();  // 210 0x10067870 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual211();  // 211 0x10080050 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // C4 only (the engine class declares slots 212 .. 227 unnamed; names after the stock guard strings): non-virtual, hooked at the slots' stock functions
    void OnShowSeedInfo(L2ParamStack& Params);                                      // 0x100678c0
    void OnShowCropInfo(L2ParamStack& Params);                                      // 0x10067950
    void OnShowManorDefaultInfo(L2ParamStack& Params);                              // 0x100679e0
    void OnShowSeedSetting(L2ParamStack& Params);                                   // 0x10067a70
    void OnShowCropSetting(L2ParamStack& Params);                                   // 0x10067b00
    void OnShowSellCropList(L2ParamStack& Params);                                  // 0x10067b90
    void OnShowProcureCropDetail(L2ParamStack& Params);                             // 0x10067c20
    void OnHeroList(L2ParamStack& Params);                                          // 0x10067cb0
    virtual void NewVirtual212();  // 212 0x100678c0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual213();  // 213 0x10067950 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual214();  // 214 0x100679e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual215();  // 215 0x10067a70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual216();  // 216 0x10067b00 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual217();  // 217 0x10067b90 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual218();  // 218 0x10067c20 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual219();  // 219 0x10067cb0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual220();  // 220 0x10067d40 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual221();  // 221 0x100700b0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual222();  // 222 0x10080180 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual223();  // 223 0x10067e30 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual224();  // 224 0x1006a0f0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual225();  // 225 0x100702a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual226();  // 226 0x10070340 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual227();  // 227 0x10079640 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void AddVIPNeedItemList(INT ProductID, INT ItemID, INT Count, INT Param4); // 230 0x1007d120 (C3: ret 0x10)
    virtual void VIPItemListEnd();                                      // 231 0x10069b30
    // NetHandler 186
    virtual void NewVirtual232();  // 232 0x10069b70 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void RequestBuyVIPItem(L2ParamStack& Params);               // C4: slot 232 is the engine's NewVirtual232 here, no longer virtual
    virtual void NewVirtual233();  // 233 0x10069b80 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void EnablePartyChat(bool bEnable);                         // C4: slot 233 is the engine's NewVirtual233 here, no longer virtual
    virtual void NewVirtual234();  // 234 0x10069ba0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void EnablePledgeChat(bool bEnable);                        // C4: slot 234 is the engine's NewVirtual234 here, no longer virtual
    virtual void NewVirtual235();  // 235 0x10069bc0 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void EnableAllyChat(bool bEnable);                          // C4: slot 235 is the engine's NewVirtual235 here, no longer virtual
    virtual void ShowPetStatusWnd(INT Type);                            // 236 0x1007afd0
    virtual void ShowPetPartyWnd(INT Type);                             // 237 0x10069030
    virtual void HideSummonWnd();                                       // 238 0x1007b040 no handler
    virtual void HidePetWnd();                                          // 239 0x1007b0d0
    virtual void ShowPetNameBtn(INT bShow);                             // 240 0x100690e0
    virtual void ReceivePetStatusInfo(INT Type, L2ParamStack& Params);  // 241 0x10069100
    virtual void ReceivePetItemList();                                  // 242 0x10069120
    virtual void AddPetInventoryItem(ItemInfo& Item);                   // 243 0x1007b160
    virtual void UpdatePetInventoryItem(INT Change, ItemInfo& Item);    // 244 0x1007b470
    // OnSkillCoolTimePacket
    virtual void SkillCoolTime(INT Param1, INT Param2, INT Param3, INT Param4); // 245 0x10074820

    // alliance wars and sieges (the Request* pass the call on to the network handler)
    // NetHandler 174; no caller in NWindow
    virtual void vfn176();                                              // 246 0x1006b6b0 (C4: no argument)
    virtual void MacroSkillUse(INT P1, INT P2);                                         // 247 0x10068420 (name ours)
    virtual void MacroTargetSelected();                                         // 248 0x10068460 (name ours)
    virtual void MacroItemPickedUp();                                         // 249 0x10068440 (name ours)
    virtual void RequestStartAllianceWar(L2ParamStack& Params);         // 250 0x100688b0
    // NetHandler 176
    virtual void RequestReplyStartAllianceWar(L2ParamStack& Params);    // 251 0x100688c0
    // NetHandler 177
    virtual void RequestStopAllianceWar(L2ParamStack& Params);          // 252 0x100688d0
    // NetHandler 178
    virtual void RequestReplyStopAllianceWar(L2ParamStack& Params);     // 253 0x100688e0
    // NetHandler 179
    virtual void RequestSurrenderAllianceWar(L2ParamStack& Params);     // 254 0x100688f0
    virtual void StartAllianceWar(TCHAR* AllianceName, TCHAR* Name);    // 255 0x1007a7d0
    virtual void StopAllianceWar(TCHAR* AllianceName, TCHAR* Name);     // 256 0x1007a8f0
    // NetHandler 180; no caller in NWindow
    virtual void vfn184(L2ParamStack& Params);                          // 257 0x100689e0
    // NetHandler 181
    virtual void RequestSiegeAttackerList(L2ParamStack& Params);        // 258 0x100689f0
    // NetHandler 182
    virtual void RequestSiegeDefenderList(L2ParamStack& Params);        // 259 0x10068a00
    // NetHandler 183
    virtual void RequestJoinSiege(L2ParamStack& Params);                // 260 0x10068a10
    // NetHandler 184
    virtual void RequestConfirmSiegeWaitingList(L2ParamStack& Params);  // 261 0x10068a20
    // NetHandler 185
    virtual void RequestSetSiegeTime(L2ParamStack& Params);             // 262 0x10068a30
    virtual void ReceiveSiegeInfo(L2ParamStack& Params);                // 263 0x100721f0
    virtual void ReceiveSiegeAttackerList(L2ParamStack& Params);        // 264 0x10072310
    virtual void ReceiveSiegeDefenderList(L2ParamStack& Params);        // 265 0x10072400
    virtual void AddSelectableTime(INT Time);                           // 266 0x10072500
    virtual void EndSiegeAttackerList();                                // 267 0x10072520
    virtual void EndSiegeDefenderList();                                // 268 0x10072540
    virtual void ClearAttackerList();                                   // 269 0x10068a40
    virtual void ClearDefenderList();                                   // 270 0x10068a60

    // packages, GM views, minimap, private stores
    // NetHandler 187
    virtual void RequestPackageSend(L2ParamStack& Params);              // 271 0x10068900
    // NetHandler 188
    virtual void RequestPackageSendableItemList(L2ParamStack& Params);  // 272 0x10068910
    virtual void AddPackageToList(L2ParamStack& Params);                // 273 0x10068920
    virtual void AddSendableItemList(INT Index, INT TargetID, INT Adena, ItemInfo& Item); // 274 0x1007aa10
    virtual void GMViewCharacterInfo(User& Info);                       // 275 0x10069230
    virtual void GMViewReceivePledgeInfo(PledgeInfo& Info);             // 276 0x100727d0
    // guard ReceivePledgeMemberList
    virtual void GMViewReceivePledgeMemberList();                       // 277 0x10069260
    // guard AddPledgeMember
    virtual void GMViewAddPledgeMember(L2ParamStack& Params);           // 278 0x10072910
    virtual void GMViewShowSkillWnd();                                  // 279 0x100692f0
    // OnGMViewSkillListPacket
    virtual void GMViewSkillList(L2ParamStack& Params);                 // 280 0x10072a00
    // empty, folded
    virtual void GMViewShowMagicWnd() {}                                // 281 0x10043b50
    // OnGMViewMagicListPacket
    virtual void GMViewMagicList(L2ParamStack& Params);                 // 282 0x10072b40
    // guard ReceiveQuestList
    virtual void GMViewReceiveQuestList();                              // 283 0x10069310
    // guard AddQuestID
    virtual void GMViewAddQuestID(INT QuestID, INT Step);               // 284 0x10069390
    // guard AddQuestItem
    virtual void GMViewAddQuestItem(ItemInfo& Item);                    // 285 0x10069450
    // guard ReceiveItemList
    void GMViewReceiveItemList(INT bShow, INT P2);              // 0x10069480 (ret 8; C4: slot 286 is the engine's NewVirtual286 here, no longer virtual)
    // guard AddInventoryItem
    virtual void GMViewAddInventoryItem(ItemInfo& Item);                // 287 0x1007b790
    virtual void GMViewStartWithdrawList(INT Adena);                    // 288 0x10069540
    virtual void GMViewAddWithdrawList(ItemInfo& Item);                 // 289 0x1007bae0
    // OnShowMinimap
    void ShowMapItem(INT ItemID);                               // C3: slot gone (was 260), no longer virtual
    virtual void NewVirtual291();  // 291 0x10072c70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual292();  // 292 0x100697e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void ShowCalc(INT Code);                                         // 293 0x10069840 (name ours)
    // the same body as StartPrivateStoreList (263)
    virtual void StartPrivateStoreManageList(L2ParamStack& Params);     // 294 0x10069900 (C4: ret 4)
    virtual void StartPrivateStoreList(L2ParamStack& Params);           // 295 0x100699c0 (C4: ret 4)
    // C4: the stock bodies of slots 294 / 295 take the packet's header (the engine class declares them without arguments)
    void StartPrivateStoreManageListC4(L2ParamStack& Params);           // 0x10069900
    void StartPrivateStoreListC4(L2ParamStack& Params);                 // 0x100699c0
    void PrivateStoreManageList(ItemInfo& InventoryItem, ItemInfo& StoreItem); // 0x1007bdb0 C4: slot 296 is the engine's NewVirtual296 here, no longer virtual (ret 8)
    virtual void PrivateStoreList(ItemInfo& Item, INT OwnerID, INT Adena); // 297 0x1007c390
    // also OnPrivateStoreBuyMsg; empty
    virtual void PrivateStoreMsg(TCHAR* Text, INT Param) {}             // 298 0x10004020
    // the same body as StartPrivateStoreBuyList (268)
    virtual void StartPrivateStoreBuyManageList();                      // 299 0x10069ae0
    virtual void StartPrivateStoreBuyList();                            // 300 0x10069ae0
    virtual void PrivateStoreBuyManageList(ItemInfo& InventoryItem, ItemInfo& StoreItem, INT OwnerID, INT Adena); // 301 0x1007c680
    virtual void PrivateStoreBuyList(ItemInfo& Item, INT OwnerID, INT Adena); // 302 0x1007cca0

    // the rest of the interface
    virtual void BlockedAccountWithMsg(TCHAR* Msg);                     // 303 0x1006ee00
    virtual void BlockedAccount(INT Reason);                            // 304 0x1006ef90
    virtual void SetSummonRemainTime(INT Remaining, INT Total);         // 305 0x100727a0
    // bCtrlDown || bCtrlDownAlt
    UBOOL IsCtrlDown();                                         // C3: slot gone (was 274), no longer virtual
    virtual void NewVirtual306();  // 306 0x1006c810 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual307();  // 307 0x1006c7d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual308();  // 308 0x1006c920 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual309();  // 309 0x10074e80 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual310();  // 310 0x10075040 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual312();  // 312 0x10075110 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual314();  // 314 0x100751c0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual315();  // 315 0x100752d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual316();  // 316 0x10075370 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // slot 297 0x10066410 (C3 only; name ours): not declared virtual (a virtual of ours the base lacks lands after the base's
    // slots, not at 297): the callers use nw::VirtualCall<UBOOL>(this, 319)
    UBOOL IsAttackKeyDown();
    // The C3-only virtuals ported below (names ours): not declared virtual (a virtual of ours that the base lacks lands after
    // the base's slots, not at its C3 number); hooked by their addresses, called by slot (nw::VirtualCall) when needed
    void C3Virtual166();   // 0x10068350 (C3-only virtual 166, not declared virtual: see above; name ours) the buff window's 0x10102BA0 (name unknown)
    void C3Virtual168();   // 0x10068370 (C3-only virtual 168, not declared virtual: see above; name ours) the buff window's 0x10102B30 (name unknown)
    void C3Virtual202();   // 0x10066040 (C3-only virtual 202, not declared virtual: see above; name ours) the NCGodWnd's restart menu hidden
    void C3Virtual203();   // 0x10066070 (C3-only virtual 203, not declared virtual: see above; name ours) the NCGodWnd's restart menu shown
    void C3Virtual206();   // 0x100660a0 (C3-only virtual 206, not declared virtual: see above; name ours) the loading screen shown
    void C3Virtual270(INT Mode);   // 0x100695b0 (C3-only virtual 270, not declared virtual: see above; name ours) the desktop's ToggleMinimap
    void C3Virtual291();   // 0x1006ca40 (C3-only virtual 291, not declared virtual: see above; name ours) the messenger window's 0x10139670 (name unknown)
    void C3Virtual293();   // 0x1006ca60 (C3-only virtual 293, not declared virtual: see above; name ours) the messenger window's 0x10131530 (name unknown)
    void C3Virtual346();   // 0x10060BC0 (C1 only: no C2 match) (C3-only virtual 346, not declared virtual: see above; name ours) the desktop hidden
    void C3Virtual347();   // 0x10060BE0 (C1 only: no C2 match) (C3-only virtual 347, not declared virtual: see above; name ours) the desktop shown
    void C3Virtual349();   // 0x100753b0 (C3-only virtual 349, not declared virtual: see above; name ours) the petition window's +0xDC child enabled
    void C3Virtual350(INT A, INT B, INT C);   // 0x100663f0 (C3-only virtual 350, not declared virtual: see above; name ours) the zone title window's 0x1010B320 with the three values
    UBOOL C3Virtual351();   // 0x100826b0 (C3-only virtual 351, not declared virtual: see above; name ours) a flag of the character window's quest page (+0xDC)
    UBOOL C3Virtual352();   // 0x100826e0 (C3-only virtual 352, not declared virtual: see above; name ours) the NCGodWnd's 0x10085C50 as a flag
    UBOOL C3Virtual353();   // 0x10082700 (C3-only virtual 353, not declared virtual: see above; name ours) a flag of the character window's quest page (+0xDD)
    void C3Virtual354(INT bValue);   // 0x10082730 (C3-only virtual 354, not declared virtual: see above; name ours) sets the quest page's flag at +0xDD
    void C3Virtual357(INT A, INT B, INT C);   // 0x10082880 (C3-only virtual 357, not declared virtual: see above; name ours) the quest page's 0x100DA3A0 with three values
    void C3Virtual374(L2ParamStack& Params);   // 0x1006da80 (C3-only virtual 374, not declared virtual: see above; name ours) the party matching window's 0x100A5B10
    void C3Virtual375();   // 0x1006daa0 (C3-only virtual 375, not declared virtual: see above; name ours) the party matching window shown, focused and refreshed (0x100A4DE0)
    void C3Virtual376(L2ParamStack& Params);   // 0x1006dae0 (C3-only virtual 376, not declared virtual: see above; name ours) the party room window's 0x100A5450
    void C3Virtual377(L2ParamStack& Params);   // 0x1006db00 (C3-only virtual 377, not declared virtual: see above; name ours) the party room window's 0x100A6EA0
    void C3Virtual379(L2ParamStack& Params);   // 0x1006db60 (C3-only virtual 379, not declared virtual: see above; name ours) the party room window's 0x100A75B0
    void C3Virtual382(INT A);   // 0x1006e490 (C3-only virtual 382, not declared virtual: see above; name ours) the broadcast GM window shown and given the value (0x1010DD50)
    void C3Virtual383(L2ParamStack& Params);   // 0x1006e4d0 (C3-only virtual 383, not declared virtual: see above; name ours) the broadcast GM window's 0x1010E110
    void C3Virtual385();   // 0x10066120 (C3-only virtual 385, not declared virtual: see above; name ours) the desktop and the window at +0x39E0 hidden
    void C3Virtual386();   // 0x10066140 (C3-only virtual 386, not declared virtual: see above; name ours) the window at +0x39E0 shown
    INT C3Virtual389();   // 0x10082690 (C3-only virtual 389, not declared virtual: see above; name ours) the loading picture's id
    virtual void NewVirtual317();  // 317 0x10066260 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual318();  // 318 0x10066280 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void VARARGS MessageBoxf(const TCHAR* Caption, const TCHAR* Fmt, ...); // 320 0x10069d40 no handler
    virtual UBOOL Init(UEngine* InEngine);                              // 325 0x10083600
    virtual INT ConsolePostRender(UCanvas* Canvas);                     // 326 0x1007fcb0
    virtual INT DispatchWndMsg(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam); // 327 0x100836b0
    virtual void NewVirtual328();  // 328 0x1006ed50 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual UBOOL NpcSay(L2ParamStack& Params);                         // 329 0x10064cd0
    virtual UBOOL MasterConsoleEventProcess(INT Key, INT Action, FLOAT Delta); // 330 0x1007fd80
    // "L2Debug", "L2DebugWindow"
    virtual UBOOL DebugCommand(const TCHAR* Cmd);                       // 331 0x1006b8d0
    virtual UBOOL InitForPawnViewer(UEngine* InEngine);                 // 343 0x100821b0
    virtual void PawnViewerFunction(INT Function);                      // 344 0x1006b5a0
    // GMalloc'd array of the item ids
    virtual void NewVirtual347();  // 347 0x10082260 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT* GetInventoryItemIDs(INT& Count);                       // 348 0x10074680

    // the last pure slots: C1's Free and SetServerPing, the rest C2 additions (RequestLogout and ShowMessageWnd, non-virtual in C1, are here)
    // folded with operator delete
    virtual void Free(void* Ptr);                                       // 349 0x1006b460
    virtual INT GetPartyMemberCount();                                         // 350 0x10065ac0 (name ours)
    virtual void SetServerPing(INT ServerID, DWORD Ping);               // 352 0x1006bcd0
    virtual void Snoop(L2ParamStack& Params);                                         // 353 0x10069bf0 (name ours)
    virtual void SetPadHoldL(BYTE bHold);                                         // 354 0x1006c080 (name ours)
    virtual void SetPadHoldR(BYTE bHold);                                         // 355 0x1006c0a0 (name ours)
    virtual void vfn271(INT Param) {}                                   // 356 0x100023c0
    virtual void ShowTownMap(L2ParamStack& Params);                                         // 357 0x1006c0c0 (name ours)
    virtual void ObserverStart();                                         // 358 0x1006c130 (name ours)
    virtual void ObserverEnd();                                         // 359 0x1006c1f0 (name ours)
    virtual void PledgePower(INT P1, INT P2, BYTE* P3);                                         // 360 0x1006c2c0 (name ours)
    virtual void ResizeWindows();                                         // 361 0x10073700 (name ours)
    void RequestLogout();                                       // C3: slot gone (was 340), no longer virtual
    void ShowQuitDialog();                                         // C4: slot 364 is the engine's NewVirtual364 here, no longer virtual (name ours)
    virtual void ShowMessageWnd(const TCHAR* Text);                     // 365 0x1006a330
    void vfn318(INT P1) {}                                         // C4: slot 366 is the engine's NewVirtual366 here, no longer virtual (name ours)
    virtual void NewVirtual367();  // 367 0x1006c390 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual368();  // 368 0x1006c590 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual369();  // 369 0x1006c490 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void ShowXMasSeal(INT P1);                                         // 370 0x1006c500 (name ours)

    // NConsoleWnd's own
    virtual void NewVirtual377();  // 377 0x10082760 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual378();  // 378 0x10082830 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual380();  // 380 0x1006cab0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual381();  // 381 0x1007ef20 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual382();  // 382 0x1006cae0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual383();  // 383 0x1006cbf0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual384();  // 384 0x1006cc90 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual385();  // 385 0x1006cd70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual386();  // 386 0x1006cee0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual387();  // 387 0x1006cfb0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual388();  // 388 0x1006d060 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual389();  // 389 0x1006d140 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual390();  // 390 0x1006d1e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual391();  // 391 0x1006d280 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual392();  // 392 0x1006d310 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual393();  // 393 0x1006d3a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual394();  // 394 0x1006d430 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual395();  // 395 0x1006d510 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual396();  // 396 0x1006d5e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual401();  // 401 0x1006db20 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual403();  // 403 0x1006e1e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual404();  // 404 0x1007f030 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual407();  // 407 0x10067ee0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual408();  // 408 0x10067f20 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual409();  // 409 0x1006d680 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual410();  // 410 0x1006d710 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual413();  // 413 0x10075700 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual414();  // 414 0x10075850 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual424();  // 424 0x10065640 (the engine class declares it unnamed; defined in NNewVirtuals.gen.cpp)
    void RequestServerList();                                   // C4: slot 424 is the engine's NewVirtual424 here, no longer virtual
    // the same body as AcceptLogOut (41)
    virtual void NewVirtual416();  // 416 0x100826a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual417();  // 417 0x1006d920 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual418();  // 418 0x1006d940 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual419();  // 419 0x1006d970 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual420();  // 420 0x1006d980 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual421();  // 421 0x1006d9a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual422();  // 422 0x1006d9c0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual423();  // 423 0x1006d9d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void QuitGame();                                            // 425 0x10076ad0
    virtual void ReceiveServerClose();                                  // 426 0x10065c00
    // NetHandler 76
    virtual void AnswerReviveRequest(UBOOL bYes, INT Param1, INT Param2); // 427 0x10066030
    // NetHandler 171
    virtual void RequestEnchantItem(INT ObjectID);                      // 428 0x10066610
    // NetHandler 165
    virtual void RequestDestroyItem(NItemRequest Request);              // 429 0x100666e0
    // NetHandler 166
    virtual void RequestCrystallizeItem(INT ObjectID, INT Count);       // 430 0x10066770
    // NetHandler 156
    virtual void AnswerTradeRequest(L2ParamStack& Params);              // 431 0x10066910
    // NetHandler 169; 0 without a server
    virtual UBOOL RequestQuestList();                                   // 432 0x10067150
    // NetHandler 170
    virtual void RequestDestroyQuest(INT QuestID);                      // 433 0x100671f0
    // NetHandler 172
    virtual void RequestAcquireSkillInfo(L2ParamStack& Params);         // 434 0x10067fc0
    // NetHandler 173
    virtual void RequestAcquireSkill(L2ParamStack& Params);             // 435 0x10067fd0
    // "bypass [-h] ..."; 2: close the page
    virtual void NewVirtual436();  // 436 0x10067fe0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual437();  // 437 0x10067ff0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT RunHtmlCommand(TCHAR* Command, NCHtmlViewer* Viewer);   // 438 0x10072560
    virtual void RequestObserverEndPacket(L2ParamStack& Params);        // 439 0x1006c290 (C2 only: a tail call of UL2NetHandler's slot of the same name)
    virtual void RequestPledgePower(INT P1, INT P2, BYTE* P3);          // 440 0x1006c2a0 (C2 only: a tail call of UL2NetHandler's slot of the same name)
    virtual void NewVirtual441();  // 441 0x1006d750 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual442();  // 442 0x1006d7f0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual443();  // 443 0x10075410 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual444();  // 444 0x1006d830 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual445();  // 445 0x1006d840 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual446();  // 446 0x1006d850 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual447();  // 447 0x1006d860 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // ---- console-core ----
    __declspec(noinline) void PlayVoiceName(const TCHAR* Name);         // 0x1006c350 (name ours; new in C3) the voice of a system message

    // ---- wave 3 (nconsole lane) ----
    // The C3-only virtuals ported here are NOT declared virtual (the NewVirtualN placeholders above stay for the vtable's
    // sake and jump to the stock address, which the hook replaces): hooked by address, called by slot when needed.
    // Names ours unless a guard string gave them.
    INT RequestPreviewItem(L2ParamStack& Params);                    // 0x10066dc0 (guard name) NetHandler slot 96: the try-on list
    void StartPreviewList(INT OwnerID, INT Adena, INT ListID);       // 0x10066e40 (C3 virtual 61) the try-on shop opened by an NPC
    void AddPreviewItem(ItemInfo& Item);                             // 0x10078aa0 (C3 virtual 62) a good of the try-on shop
    void EndQuestList();                                             // 0x10067300 (guard name; C3 virtual 171)
    TCHAR* GetQuestName(INT QuestID);                                // 0x10067510 (guard name) questname-*.dat
    TCHAR* GetCurrentZoneName();                                     // 0x100676a0 (guard name)
    INT GetCurrentZoneID();                                          // 0x10067720 (guard name)
    INT GetZoneID(FLOAT X, FLOAT Y, FLOAT Z);                        // 0x100677a0 (guard name)
    void ReceiveBuffList(L2ParamStack& Params);                      // 0x10068b40 (C3 virtual 114) the buffs of a party member, the pet or the summon
    void ShowMapPosition(INT X, INT Y, INT Z);                       // 0x100697e0 (C3 virtual 272) the minimap on that place
    void ShowMyMapPosition();                                        // 0x10072c70 (C3 virtual 271) the minimap on the player
    void SendBypassBuildCmd(FString Command);                        // 0x1006a020 (guard name; C3 virtual 192) a builder command as a parameter stack
    void GetSkillCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining); // 0x1006b6c0 (name ours) the skill window's cool time of a skill
    UBOOL IsQuickUseItem(INT ItemID);                                // 0x1006b700 (name ours) potions and shots: the key repeats
    void RequestAutoShot(L2ParamStack& Params);                      // 0x1006e4f0 (name ours) NetHandler slot 275, a tail call
    void SendConfirmDlg(L2ParamStack& Params);                       // 0x1006c410 (guard name) NetHandler slot 260
    void SetLoadingTextureName(FString Name);                        // 0x100660b0 (C3 virtual 207) the loading picture by its texture's name
    void MakeItemInfo(NItemEntry* Entry);                            // 0x1007d760 (guard name) the texts of an entry from the item table
    void OnEventMatchMessage(INT Kind, FString Message);              // 0x1006cae0 (guard name; C3 virtual 360) the match's text to the observer
    void OnEventMatchScore(INT A, INT B, INT C);                     // 0x1006cbf0 (guard name; C3 virtual 361)
    void OnEventMatchTeamInfoStart(INT A, INT B, INT C);             // 0x1006cc90 (C3 virtual 362; the stock's guard says "NConsoelWnd")
    void OnEventMatchTeamInfo(INT A, INT B, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7, INT P8, INT P9, INT P10); // 0x1006cd70 (C3 virtual 363)
    void OnEventMatchTeamInfoEnd();                                  // 0x1006cee0 (C3 virtual 364; "NConsoelWnd")
    void OnEventMatchCreate(INT GameID);                             // 0x1006cfb0 (guard name; C3 virtual 365)
    void OnEventMatchUserInfo(INT A, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7, INT P8); // 0x1006d060 (C3 virtual 366)
    void OnEventMatchSpellInfoStart(INT A, INT B, INT C);            // 0x1006d140 (C3 virtual 367; guard says OnEventMatchUserInfo)
    void OnEventMatchSpellInfo(INT A, INT B, INT C, INT D);          // 0x1006d1e0 (C3 virtual 368; the same guard)
    void OnEventMatchSpellInfoEnd();                                 // 0x1006d280 (C3 virtual 369; the same guard)
    void OnEventMatchTeamUnlocked(INT A, INT B);                     // 0x1006d310 (guard name; C3 virtual 370)
    void OnEventMatchGMTest();                                       // 0x1006d3a0 (guard name; C3 virtual 371)
    void OnReceiveMagicSkillUse(User* Caster, User* Target, INT Param); // 0x1006d430 (guard name; C3 virtual 372)
    void OnCharInfo(INT Info);                                       // 0x1006d5e0 (guard name; C3 virtual 373)
    void ExecuteCommand(TCHAR* Line);                                // 0x1006d680 (guard name; C3 virtual 384)
    INT AddAbnormalStatus2(L2ParamStack& Params);                    // 0x10071580 (C3 virtual 169) a buff of the buff window's second list
    INT SetExtraAbnormalStatus(L2ParamStack& Params);                // 0x100716d0 (C3 virtual 167) the buff window's extra icon
    void GetQuestTarget(FString* Name, FVector* Pos);                // 0x1006c590 (C3 virtual 345) the followed quest's target
    FString GetQuestTargetName();                                    // 0x10082760 (C3 virtual 355)
    FVector GetQuestTargetPos();                                     // 0x10082830 (C3 virtual 356)
    void StartHPBarAnim(INT Unused, INT Value, INT Max, DOUBLE Time); // 0x1006cab0 (C3 virtual 358) the player's HP bar animates
    void SetShotAutoUse(L2ParamStack& Params);                       // 0x1006e1e0 (C3 virtual 380) a shot's auto use on / off on the shortcut slots
    void UseAutoShots(INT bFlag);                                    // 0x1007f030 (C3 virtual 381) the auto list's shots used for the weapon's grade
    void ClosePartyRoom(L2ParamStack& Params);                       // 0x1006db20 (C3 virtual 378)
    void DispatchMessengerMsg(INT hWnd, UINT Msg, INT wParam, INT lParam); // 0x1006ed50 (C3 virtual 306) the messenger library's messages
    void ShowMessageDialog46(L2ParamStack& Params);                  // 0x1006c390 (C3 virtual 344)
    void ShowMatchEndDialog();                                       // 0x10075700 (C3 virtual 387)
    void ShowMatchTextDialog(const TCHAR* Text, INT bHide);          // 0x10075850 (C3 virtual 388)
    void ReceiveSSQStatus(L2ParamStack& Params);                     // 0x1006c810 (C3 virtual 286) the Seven Signs board's status
    void ReceiveSSQLists(L2ParamStack& Params);                      // 0x1006c7d0 (C3 virtual 287)
    void ResetSSQEvents();                                           // 0x1006c920 (C3 virtual 288)
    void ReceiveSSQEvents(L2ParamStack& Params);                     // 0x10074e80 (C3 virtual 289)
    void ReceiveSSQList(L2ParamStack& Params);                       // 0x10075040 (C3 virtual 290)
    void ToggleBuffIcons();                                          // 0x1006c530 (name ours; a shortcut key) the buff icons of the party, pet and summon windows on / off
    void OnMessengerFriendAdd(INT A, TCHAR* Name, INT B, INT C);     // 0x10075110 (C3 virtual 292) a friend of the messenger to its window
    void OnMessengerFriendEvent(INT Kind, INT A, TCHAR* Name, INT B, INT C); // 0x100751c0 (C3 virtual 294) the same by kind (1, 2, 3)
    void OnMessengerFriendStatus(INT B, TCHAR* Name, INT A);         // 0x100752d0 (C3 virtual 295)
    void OnMessengerChat(INT A, TCHAR* Name, TCHAR* Text);           // 0x10075370 (C3 virtual 296)

    // C4 only (NConsoleWndC4.cpp): the engine class declares these vtable slots as NewVirtualN(); the real signatures are here,
    // non-virtual, hooked at the slots' stock functions (names: C4Virtual<slot>, or the stock guard string's)
    void C4Virtual317();                                             // 0x10066260
    void C4Virtual318();                                             // 0x10066280
    void C4Virtual209(INT Unused);                                   // 0x10067830
    void C4Virtual210(L2ParamStack& Params);                         // 0x10067870
    void C4Virtual369();                                             // 0x1006c490
    void C4Virtual407(INT Value);                                    // 0x10067ee0 UGameEngine::OnExMultiPartyCommandChannelInfo
    void C4Virtual408(INT Value);                                    // 0x10067f20 ...InfoAddMember
    void C4Virtual085(L2ParamStack& Params);                         // 0x10070000
    void C4Virtual410(INT A, INT B, INT C, INT D);                   // 0x1006d710
    void C4Virtual417(INT Value);                                    // 0x1006d920
    void C4Virtual418(INT Value);                                    // 0x1006d940
    void C4Virtual419();                                             // 0x1006d970
    void C4Virtual420(INT A, INT B, INT C);                          // 0x1006d980
    void C4Virtual421(INT A, INT B, INT C, INT D, INT E, INT F);     // 0x1006d9a0
    void C4Virtual422();                                             // 0x1006d9c0
    void C4Virtual423(INT bShow);                                    // 0x1006d9d0
    void C4Virtual442(INT* A, INT* B, INT* C);                       // 0x1006d7f0
    void C4Virtual443();                                             // 0x10075410 (vslot 443) the GM's server change, one step per tick
    void C4Virtual444();                                             // 0x1006d830
    void C4Virtual445(INT Value);                                    // 0x1006d840
    void C4Virtual446();                                             // 0x1006d850
    void C4Virtual447();                                             // 0x1006d860
    void SetGMChangeServerState(INT State);                          // 0x1006d750 (guard name; vslot 441)
    void ShowOptionWnd();                                            // 0x1006a350 (guard name)
    INT GetMSHpConsume(INT SkillID, INT Level);                      // 0x1006aa80 (guard name) the skill's HP cost
    ALineagePlayerController* GetPlayerController();                 // 0x1005c490 (anonymous) the first viewport's controller
    void OnOlympiadUserInfo(L2ParamStack& Params);                   // 0x10067d40 (guard name; vslot 220)
    void OnOlympiadMatchEnd();                                       // 0x10067e30 (vslot 223)
    void OnExMailArrivedPacket();                                    // 0x1006a0f0 (vslot 224)
    void OnExOpenMPCC(INT Unused);                                   // 0x100702a0 (vslot 225)
    void OnExCloseMPCC(INT Unused);                                  // 0x10070340 (vslot 226)
    void OnExAskJoinMPCC(L2ParamStack& Params);                      // 0x10079640 (vslot 227)
    void C4Virtual246();                                             // 0x1006b6b0 (the slot 246, which the engine header gives a stack argument)
    void RequestManorList();                                         // 0x1006db90 (guard name) NetHandler slot 285
    void RequestProcureCropList(L2ParamStack& Params);               // 0x1006dc10 (guard name) 286
    void RequestSetSeed(L2ParamStack& Params);                       // 0x1006dc90 (guard name) 287
    void RequestSetCrop(L2ParamStack& Params);                       // 0x1006dd10 (guard name) 288
    void RequestWriteHeroWords(FString Words);                       // 0x1006dd90 (guard name) 289 (ret 0xC: the text by value)
    void RequestOlympiadObserverEnd();                               // 0x1006de40 (guard name) 290
    void RequestOlympiadMatchList();                                 // 0x1006dec0 (guard name) 291
    void SaveL2Mark(INT Value);                                      // 0x1006bd00 (name ours) the observer mode keeps GIsL2Mark and clears it
    void RestoreL2Mark();                                            // 0x1006bd30 (name ours) and gives it back
    void C4Virtual363();                                             // 0x100650f0 (vslot 363) the logout, then the replay recording stops
    void C4Virtual362();                                             // 0x10073ac0 (vslot 362) the windows back in place after the screen changed size
    void OnAttack(User* A, User* B, INT P3, INT P4, INT P5);         // 0x1006d510 (guard name; vslot 395, ret 0x14) the Olympiad windows are told
    void RequestRestartNow(L2ParamStack& Params);                    // 0x10065500 (name ours) NetHandler slot 172, then the replay recording stops
    void RequestExEnchantSkillInfo(L2ParamStack& Params);            // 0x10067fe0 (vslot 436) NetHandler slot 187
    void RequestExEnchantSkill(L2ParamStack& Params);                // 0x10067ff0 (vslot 437) 188
    void RequestExAskJoinMPCC(L2ParamStack& Params);                 // 0x10067f40 (name ours) 189
    void RequestExAcceptJoinMPCC(L2ParamStack& Params);              // 0x10067f50 (name ours) 190
    void RequestExOustFromMPCC(L2ParamStack& Params);                // 0x10067f60 (name ours) 191
    INT GetSpecialServer();                                          // 0x100826a0 (vslot 416; was named GetPartyItemDistribution) SpecialServer (+0x3C68)
    void RequestSendL2FriendSay(TCHAR* Name, TCHAR* Text);           // 0x1006ca80 (name ours; not a virtual; ret 8) NetHandler slot 272
    void RequestSendMsnChatLog(TCHAR* Account, TCHAR* Text, INT Flag);  // 0x1006ca90 (name ours; not a virtual; ret 0xC) NetHandler slot 273
    const TCHAR* GetManorName(INT Manor);                            // 0x1006b290 (guard name) the system string of manor 1 .. 7
    void C4Virtual211(L2ParamStack& Params);                         // 0x10080050 (guard name OnSendManorList; vslot 211) the manors; their window refreshed
    void OnOlympiadMode(INT Mode);                                   // 0x10080180 (guard name; vslot 222) 0 the Olympiad UI ends, 1 and 2 the target's side, 3 it starts
    void OnOlympiadSpelledInfo(L2ParamStack& Params);                // 0x100700b0 (guard name; vslot 221) the effects on an Olympiad player
};
NW_ASSERT_OFFSET(NConsoleWnd, WndMain, 0x3BDC);
NW_ASSERT_OFFSET(NConsoleWnd, Desktop, 0x3BF4);
NW_ASSERT_OFFSET(NConsoleWnd, DebugWnd, 0x3BF8);
NW_ASSERT_OFFSET(NConsoleWnd, PetitionWnd, 0x3C00);
NW_ASSERT_OFFSET(NConsoleWnd, PetitionFeedBackWnd, 0x3C04);
NW_ASSERT_OFFSET(NConsoleWnd, Timers, 0x3C30);
NW_ASSERT_OFFSET(NConsoleWnd, DeltaTime, 0x3C40);
NW_ASSERT_OFFSET(NConsoleWnd, SpecialServer, 0x3C68);
NW_ASSERT_OFFSET(NConsoleWnd, PartyItemRule, 0x3C6C);
NW_ASSERT_OFFSET(NConsoleWnd, GodWnd, 0x3BE0);
// NW_ASSERT_OFFSET(NConsoleWnd, MessageWnd, 0x353C);   // C2: field not mapped
NW_ASSERT_OFFSET(NConsoleWnd, LoadingScreenID, 0x3C64);
NW_ASSERT_OFFSET(NConsoleWnd, bInitialized, 0x3BA8);
NW_ASSERT_OFFSET(NConsoleWnd, CreditWnd, 0x3BE4);
NW_ASSERT_OFFSET(NConsoleWnd, Wnd3C08, 0x3C08);
NW_ASSERT_OFFSET(NConsoleWnd, SysMsgParams, 0x3C10);
NW_ASSERT_OFFSET(NConsoleWnd, ManorList, 0x3C20);
NW_ASSERT_OFFSET(NConsoleWnd, WordWrapperWnd, 0x3C2C);
NW_ASSERT_OFFSET(NConsoleWnd, TutorialEvents, 0x3C5C);
NW_ASSERT_OFFSET(NConsoleWnd, PawnViewerWnd, 0x3C78);
NW_ASSERT_OFFSET(NConsoleWnd, bGMChange, 0x3C94);
NW_ASSERT_OFFSET(NConsoleWnd, GMDest, 0x3C98);
NW_ASSERT_SIZE(NConsoleWnd, 0x3CA4);

// The children of the login screen in C2, by offset (the stock code reads them at these places; NCLoginWnd.h, the
// pawn group's, still has C1's order): +0x98 AuthWnd, +0x9C NoticeWnd, +0xA0 WarningWnd, +0xA4 and +0xA8 two
// windows C2 added (names ours: LOGIN_Wnd_A4, LOGIN_Wnd_A8), +0xAC the server list
class NCServerSelectWnd;
#define NLOGIN_SERVERLIST(Login) (nw::Field<NCServerSelectWnd*>((Login), 0xAC))
#define NLOGIN_WND_A4(Login) (nw::Field<NCWnd*>((Login), 0xA4))
#define NLOGIN_WND_A8(Login) (nw::Field<NCWnd*>((Login), 0xA8))

// Cast<APawn> out of line (0x10047230, name ours): NULL unless Object is a pawn
APawn* CastToPawn(UObject* Object);

// Cast<ALineagePlayerController> out of line (0x1004c5c0, name ours): NULL unless Object is the L2 player's controller
ALineagePlayerController* CastToLineagePlayerController(UObject* Object);

#pragma pack(pop)