# A fundação do Engine do C4 (`include/ue2`) — etapa 2 do `PLANO_C4.md`

Feita em 18/10/2026 sobre o `Core.dll` (978.964 bytes, md5 `34b0218e…`) e o `Engine.dll` (7.786.516 bytes, md5 `db71fb8d…`), os dois de 27/05/2006, que ficam
na pasta do cliente ao lado do `NWindow.dll` de 13/01/2006. Ponto de partida: o `include/ue2` do C3 já provado (tag `c3-base`). O método é o do
[PLANO_C3.md](../PLANO_C3.md) (“Etapa 2”), com três ferramentas novas (abaixo).

**Resultado:** `python tools/ue2_check/run_check.py` dá **ALL OK**: compila nos dois modos (`NW_HOOK_MODE=1` e `0`) com 0 diagnósticos, liga as **644
referências** (funções, variáveis exportadas e o `FMemMark` inline) contra as bibliotecas de importação geradas dos `.def` do C4 (`third_party/ue_import`:
Core 2.670 exports, Engine 9.954; 0 não resolvidas) e confere **1.620 slots virtuais** (882 iguais à DLL em slot e assinatura, 738 puros, dos quais 338
iguais ao slot da classe que os implementa; 0 errados). O `ue2_check_headers.py` prova **63 de 178** `NW_ASSERT_OFFSET` pelo script do `Engine.u` do C4 (nenhum
difere; 115 são membros nativos); o `ue2_check_fields_by_code.py` (novo) confirma **86** pelo código das DLLs (0 divergentes sem explicação). Todos os 126 imports do
`Engine.dll` do `NWindow.dll` do C4 estão declarados; dos 208 do `Core.dll` ficam de fora os mesmos 4 do C3 (`FExec::vftable`, `UObject::operator delete`, o
construtor e o destrutor de `UClass`: o IMPLEMENT_CLASS do próprio NWindow).

## O achado que move tudo: `UObject` tem 0x34 bytes

`UObject::UObject(ENativeConstructor…)` e a variante `EStaticConstructor` do `Core.dll` gravam `-1` em `+0x28` e `0` em `+0x2C`; `~UObject` indexa uma segunda tabela
global de objetos com `+0x30` (como indexa `GObjObjects` com `Index`, `+0x04`); `UObject::IsA` / `UStruct::IsChildOf` leem `SuperField` em `[classe + 0x34]`.
Todo `IMPLEMENT_CLASS` do `Engine.dll` cresce 0xC (`USubsystem` 0x2C → 0x38, `UAnimation` 0x28 → 0x34…) e, com `UOBJECT_SIZE = 0x34` em `tools/ue2_layouts.py`, os layouts
dos scripts (`Engine.u` do C4) fecham **exatamente** no tamanho de cada classe (`native_extra` = 0 em `AActor`, `APawn`, `AController`, `UCanvas`, `UEngine`, `UGameEngine`):
não há membro nativo escondido depois das variáveis do script. (É o `NCObject` de 0x34 do estudo de viabilidade: o `NCObject` deriva de `UObject`.)

## Quadro (no estilo da etapa 2 do C3)

| Parte | Como foi feita |
|---|---|
| `UObject` 0x28 → 0x34, `UField::SuperField` 0x34, `FExec` do `USubsystem` em 0x34 | `UE2Core.h`: três dwords `_L2Field28/_L2Field2C/_L2Index30` (nomes nossos, evidência acima) |
| Classes com script: `AActor` 0x398 → 0x3A8, `APawn` 0x16CC → 0x1770, `AController` 0x538 → 0x548, `UCanvas` 0x1A4 → 0x1D0 (+0x2C: dez variáveis novas depois de `m_L2Font`), `UEngine` 0x184 → 0x190, `UGameEngine` 0x36C → 0x374, `UTexture`, `UPlayer`, `USkillVisualEffect` 0x60 → 0x6C… | `tools/ue2_shift_layout.py --apply --from analysis/c3/engine/layouts.json --old-label C3` (as variáveis do `Engine.u` são as âncoras; `NMagicInfo` 168 → 180 e `NPrimeAction` entram nas âncoras); `ue2_check_headers.py`: 63 provados, 0 diferem. `UGameEngine::LandMark` (`+0x2F0`, `int` do script) é novo na fundação (`FLandMark::DeleteLandMark`) |
| Classes nativas sem script: `USkeletalMesh` (`DefaultAnim` 0x23C → 0x248), `UViewport` (`Canvas/Input/RenDev` +0xC; classe +0x10), `UClient` (+0xC), `UL2Font` 0x2C → 0x38 | `tools/ue2_disp_diff.py` (métodos exportados das duas DLLs pareados; 29, 4, 9, 8, 10 votos); `UViewport` cresceu 4 além do `UObject`, depois de `+0x98` (último campo conferido: `UViewport::_Screen`, 0x98 → 0xA4) |
| **Três offsets do cabeçalho do C3 estavam errados** (velhos desde o C2, nunca conferidos pelo código): `UMeshAnimation::AnimSeqs` (C3: dados em `+0x60`, passo 0x44; C4: `+0x6C`, passo **0x74**), `USkeletalMeshInstance::Blends` (C3: `+0x178`; C4: `+0x184`, contagem em `+0x188`) e `FL2TextAlignParser` (0x78 nos dois, o cabeçalho dizia 0x6C) | `UMeshAnimation::GetAnimSeq`, `GetAnimChannelCount`/`SetAnimRate`/`GetActiveAnimRate` (48 + 45 métodos pareiam `0x178 → 0x184`), construtor e `Clear` do `FL2TextAlignParser` (último campo em `+0x74`); o `NWindow` do C3 e o do C4 alocam 0x78 |
| `UL2ConsoleWnd` 0x392C → **0x3BA8** | `State` 0x38, `SavedState` 0x3C, `Engine` 0x54, `DefaultCharacterInfo[18]` 0x58, `SelectCharacterInfo[8]` **0x2968**, `User` 0x248: `imul 0x248` em `SelectedCharacterNum`, `rep movsd` de 0x92 dwords em `ResetSelectCharacterInfo`, o `Pawn` dos registros em `+0x2AC0 = 0x2968 + 0x158`, `0x2968 + 8 × 0x248 = 0x3BA8` |
| `User` 0x230 → **0x248** | `ret 0x248` de `FL2NetNotify::AddCharacterInfo`; getters de uma instrução e o construtor (`memset 0x248`; registros de 12 bytes em `0x128/0x134/0x140/0x14C`; bytes `0x228..0x22B`; `-1` em `0x244`); o `NCDetailStatusWnd::OnPaint` do C3 e do C4 pareado pelo nome do `guard()` (`Load 0x158 → 0x164 … CastSpeed 0x180 → 0x18C`, `PKCount 0x200 → 0x210`, `PvPCount 0x204 → 0x214`). Nada se moveu até `+0xCC`; `+4` em `HaveAccessary`; `+8` do `GetNickColor` ao `Pawn` (0x158); `+0xC` do bloco de estatísticas ao `GetNickName` (0x1A4) e a `PledgeID` (0x1D4); `+0x10` de `GetPrivateStoreState` (0x200); `+0x18` na cauda |
| `FL2GameData` 0x7AF90 → **0xADC24** | `ue2_gamedata_regions.py` (construtor de cópia) + os votos de `ue2_disp_diff.py FL2GameData` e a leitura do `NWindow` (`guardpairs`): base 0x34 (+0xC), um dword novo antes do `BlowFish` (0xC0), **a tabela de quests passa a 2.000 × 0xA0** (C3: 1.000 × 0x70, `+0x32C80`), e tudo depois dela é `+0x32C90` (`SystemMsg` 0x722C8, `SysString` 0x9871C, `CastleNames` 0xAA9EC), o último bloco `+0x32C94` (0xAD80C) |
| `FL2SystemMsgData` (0x30) | o `SystemMsgFormatClear` do C4 destrói **três** `FString` por registro (`+4`, `+0x18`, `+0x24`): os 12 bytes “planos” do C3 viram `Message3` |
| `FL2MagicSkillData` 0x4C → **0x78**; `FL2ActionEntry` (0x44); `FL2QuestData` (0xA0); `FServerStaticObject` 0x10 → **0x18** (`Actor` em `+0x14`); `PetInfo` 0xAC → **0xB4** | os carregadores do próprio Engine (`MSItemDataLoad` guarda cada coluna do `.dat` no registro, `MSItemNameDataLoad`, `ActionDataLoad`, `QuestDataLoad`), `FServerStaticObject::operator=` (6 dwords) + `OnMyTargetSelected`, `GetPetInfo` (`mov ecx, 0x2D`; um retorno por valor pelo ponteiro escondido não aparece no `ret N`) e as 7 cópias no `NWindow` do C4. O cabeçalho do C3 tinha o `FL2MagicSkillData` do C2 (o C3 usava uma visão própria em `NConsoleWndData.cpp`) e `FL2ActionEntry` com `Name`/`IconName` trocados |
| Vtables regeneradas (`tools/gen_engine_decls.py`) | `FL2NetNotify` 403 → 430, `UL2NetHandler` 276 → 296, `UL2ConsoleWnd` 390 → 425, `AActor` (sai `GetExtraBoneName`/`GetExtraMesh`/`ExtraMeshGetInstance(…,INT)`, entram `Get/SetLowbodyBoneName`), `UViewport` 46 → 48 (`PlayForceFeedBack(INT, FLOAT)`, `TickForceFeedBack`, `IsDefaultConsolePos`), `URenderDevice` 53 → 54 (`SupportsTextureFormat`), `UMesh` (`ExtraMeshGetInstance(AActor*)`) |
| `UCanvas` 77 → 82 slots (escrita à mão) | `DrawSpecialDigit` ganha dois argumentos, **`DrawCameraSceneNode()` / `DrawCameraSceneNode(INT×5)` / `GetViewportSceneTexture(INT)` entram em 59–61** (tudo o que vinha depois sobe 3), `SetFontScale(FLOAT)` e `DrawRecordMark()` no fim (80, 81); `DrawMinimapLoc(INT, INT, UMaterial*, BYTE)` e `MakeBMPToDXT1(…, INT)` com assinatura nova, `SetTextAnim(bool, FLOAT)`/`StartTextAnim`/`EndTextAnim` novos; `DrawMinimapTex` não é mais exportada |
| Imports novos do Engine (+14 / −3 do estudo) | `FL2GameData::Clear`, `L2ParamStack::Clear`, `FL2ReplayManager::OnPublicLoad/OnSaveStart/OnSaveStartStop/OnSaveStop`, `APawn::GetCurAnimName`, `ALineagePlayerController::InvalidateZoneName`, `FLandMark::DeleteLandMark`, os três de texto animado, os dois de `UCanvas` com assinatura nova; saem `DrawMinimapTex` e as assinaturas velhas |
| Imports novos do Core (+12 / −2) | `GL2SystemMsgWnd`, `GL2SystemMsgWndDamage`, `GL2SystemMsgWndExpendableItem`, `GL2ShowZoneTitle`, `GL2PostProcessType`, `GL2FFEffectUse`, `GL2DisableGCLobby`, `GIsTrueFontUse` (todos `int`), `FMemMark::Pop`, `FMemStack::GetThreadSafeMemStack`. **A pilha de memória é por thread agora**: `FMemStackFrame` (Top, End, DefaultChunkSize, TopChunk; `FreeChunks`), o `FMemMark` do NWindow é inline com 0xC bytes (`Top`, `Frame`, `SavedChunk`) e `Pop` é importada; `FMemStack::FreeChunks` passa a `FMemStackFrame::FreeChunks`; `GL2GlowEffect` deixa de ser importada (continua exportada) |

### Nomes dos slots do `UL2ConsoleWnd` (`tools/ue2_data/l2consolewnd_slots_c4.csv`)

`tools/ue2_console_slots_c4.py` leva a tabela do C3 aos slots do C4 (mesmo método do `ue2_console_slots.py` do C2: âncoras `layout` / `handler` / `guard`, preenchimento
`gap` e alinhamento `align` das funções do `NConsoleWnd` das duas imagens; a imagem do C3 vem do repositório do C3, `--c3`). As vtables do `UL2ConsoleWnd` só diferem em
duas corridas de slots puros: **272 → 294 (27..320) e 60 → 73 (352..424)**; tudo o mais mantém tamanho e ordem (+22 depois da primeira).

* 425 slots: 25 do `UObject`/`USubsystem`, `Tick`, e **399 declarados**: 19 do próprio Engine (nomes exportados), **380 puros**, dos quais **247 com nome** (172 `gap`, 32 `handler`, 19 `guard`, 13 `align`, 11 `layout`) e **133 `PureSlotNNN`**.
* Os 133: **35 slots novos do C4** (22 + 13: Manor `OnSendManorList`…`OnShowSellCropList`, Olimpíada, Heróis, correio, canal de comando `OnExOpenMPCC`/`CloseMPCC`/`AskJoinMPCC`/
  `OnExMultiPartyCommandChannelInfo`, `OnExPCCafePointInfo`…; a nota de cada um traz o `UGameEngine::OnXxx` que o chama, o nome do `guard()` e o `ret N` da função), **75** que já eram
  `PureSlotNNN` no C3 e **23** linhas nomeadas no C3 que perderam o nome: 12 sem par no C4 (`ReceivePledgeExtendedInfo`, `BuilderCommand`, `SelectItemToEnchant`,
  `ReceiveEnchantResult`, `HideTrainWnd`, `TeleportToLocation`, `SetVIPShopID`, `AddVIPItemList`, `RequestBuyVIPItem`, `EnablePartyChat/PledgeChat/AllyChat`: o enchant, o VIP shop e o
  chat foram refeitos) e 11 cujo `ret N` mudou (a assinatura mudou: `AddCharacterInfo`, `StartDepositList`, `StartWithdrawList`, `ReceiveRecipeBookItemList`,
  `ReceiveRecipeItemMakeInfo`, `ReceiveRecipeShopManageList`, `EndSystemMessageParam`, `ShowBoard`, `GMViewReceiveItemList`, `PrivateStoreManageList`, `ShowMessageWnd`; a linha do C3 fica na nota).
* Prova cruzada: em 45 dos 247 slots nomeados a função do `NConsoleWnd` do C4 tem um `guard()`; **35 coincidem com o nome** e os outros 10 são o mesmo assunto com outro nome (as variantes
  `GMView*` copiam o `guard()` da versão normal: `GMViewAddQuestID` ↔ `AddQuestID`, `GMViewReceivePledgeMemberList`, `GMViewAddPledgeMember`, `GMViewReceiveQuestList`, `GMViewAddInventoryItem`;
  `ObserverStart/End` ↔ `ReceiveObserverModeOn/OffPacket`; `ResizeWindows` ↔ `RearrangePosition`; `RestartResetWindows` ↔ `RestartBegin`; `ShowWarningDialog` ↔ `ShowWarningMessage`): nenhum nome caiu no slot errado.
* Dos 172 `gap`, 11 têm o corpo do `NConsoleWnd` bem diferente nas duas imagens (semelhança de mnemônicos < 0,7; funções refeitas, não prova de erro de posição): `RequestLogout`,
  `StartPrivateStoreManageList`, `StartPrivateStoreList`, `RestartResetWindows`, `StartNotice`, `PartyAddMember`, `PartyUpdateMember`, `ShowQuitDialog`, `UseShortCutSlot`, `CheckGeo`, `ObserverStart`.

## O que NÃO está provado (o `verify_data` do ciclo fecha)

* `User`: onde cai o dword novo entre o `Pawn` (0x158) e `Load` (0x164) — `DeleteDays` está em 0x15C por vizinhança; `bCanCrystallize` (+0xC ou +0x10: 0x1F4 aqui) e a cauda
  (`+0x218..0x248`); as palavras de `+0xD0` a `+0xE4` (um dword novo em 0xD0, o acessório em 0xD4) estão num `filler`.
* `FL2GameData`: as regiões entre `CastleNames` e o último bloco (`+0xABB1C..+0xAD80C`, 0x1CF0 bytes) e o bloco novo de 4 bytes antes dele; os registros que o NWindow não lê
  (`FL2ObsceneData`, `FL2AbnormalData`, `FL2QuestNameData`, `FL2ActionData` são do C1/C2 e ficam como estavam); `FL2QuestData` só tem nomeados `Name`, `StepText`, `Details`
  (as listas de itens, níveis e posição no mapa do C3 em `+0x30..+0x6C` andaram 0 ou 4 bytes).
* `PledgeInfo`, `ItemInfo`, `Item`, `CharacterInfo`: sem mudança de tamanho (nenhum `ret N` por valor mudou) e leituras do NWindow nos mesmos offsets, mas nenhum método
  exportado do Engine os lê: ficam como no C3. `PetInfo`: dois dwords novos em `+0xAC` e `+0xB0` (o resto confere com as leituras de `NCPetStatusWnd`).
* `UViewport::SizeX` (`+0x94`, por vizinhança), `UClient::WindowedViewportX/FullscreenViewportY` (`+0x6C/+0x78`, pelos irmãos), `FL2ReplayManager` (opaco),
  `FMeshAnimSeq` (só o nome em `+0`), os fillers do `FL2MagicSkillData` (`+0x38`).
* `ERunningOS` (só o nome do tipo) e os valores dos enums novos.
* Assinaturas com tipos desconhecidos: `DrawSpecialDigit` (qual dos inteiros é o novo), `OnPublicLoad(FString, INT, INT)`, `GetCurAnimName(INT)`, `SetTextAnim(bool, FLOAT)`.
  Nomes de argumento são nossos; o decorado confere pelo teste de ligação.
* As `FString(INT, INT)`, `FString(const char*)` e `FString::InStr(const FString&, INT)` que o NWindow importa (como no C3) não têm declaração com o nome decorado exato: o cabeçalho tem
  `FString(const TCHAR*)` e `InStr(const TCHAR*, UBOOL)`; ficam de fora para não criar ambiguidade nos fontes que serão regerados.

## Ferramentas

| Ferramenta | Papel |
|---|---|
| `tools/ue2_layouts.py` | `UOBJECT_SIZE = 0x34`, `USUBSYSTEM_SIZE = 0x38`; regera `analysis/engine/{classes,layouts,structs}.json` (`ue2_engine_classes.py` antes) |
| `tools/ue2_shift_layout.py` | ganhou `--old-label` (os comentários dos campos movidos leem `(C3 +0x..)`) |
| `tools/ue2_disp_diff.py` (novo) | pareia os métodos exportados de mesmo nome decorado em duas `Engine.dll` e lista os números que mudaram (deslocamentos, tamanhos de registro, passos); ignora slots de pilha |
| `tools/ue2_check_fields_by_code.py` (novo) | para cada `NW_ASSERT_OFFSET` do cabeçalho: o offset antigo (tag `c3-base`) e os votos dos métodos da classe e das bases; `tools/ue2_data/field_code_notes.csv` explica as 3 divergências (ruído de outro objeto; o `AnimSeqs` velho) |
| `tools/ue2_console_slots_c4.py` (novo) | a tabela de nomes dos slots do console, C3 → C4 |
| `tools/ue2_guard_pairs.py` (novo) | pareia as funções do `NWindow.dll` do C3 e do C4 pelo nome do `guard()` (636 pares) e lista os números que mudaram: os deslocamentos dos campos do Engine que o NWindow LÊ (`--votes`, `--same`) |
| `tools/gen_engine_decls.py` | lê `l2consolewnd_slots_c4.csv` |
| `tools/ue2_check/run_check.py`, `gen_checks.py`, `check_ue2.cpp` | rótulos “C4”; as variáveis exportadas agora são **guardadas** num vetor (antes `(void)&Var` era descartado pelo compilador e o nome decorado nunca era conferido); o `FMemMark` inline é instanciado; `check_ue2.cpp` repete os tamanhos de C4 (UObject, AActor, APawn, AController, UCanvas, UEngine, UGameEngine, UL2Font, User, UL2ConsoleWnd, FL2GameData, FServerStaticObject, PetInfo, FMemMark…) |

Receita para repetir: `python tools/ue2_engine_classes.py && python tools/ue2_layouts.py`; `python tools/ue2_shift_layout.py --apply --from analysis/c3/engine/layouts.json --old-label C3`;
`python tools/ue2_console_slots_c4.py && python tools/gen_engine_decls.py`; `python tools/ue2_check/run_check.py`; `python tools/ue2_check_headers.py`; `python tools/ue2_check_fields_by_code.py`.
(O repositório do C3 tem de estar em `<C3 repo>`, só leitura: as ferramentas de comparação leem as `Engine.dll`/`NWindow.dll` dele.)

## Evidência “do lado do NWindow” que serve ao ciclo seguinte

O pareamento por nome de `guard()` das funções do `NWindow.dll` do C3 e do C4 (`tools/ue2_guard_pairs.py`: `analysis/c3/eh.json` e `ue2_guard_names.py` sobre o censo do Ghidra) já dá, sem o `fmap`, os
deslocamentos de campo dos objetos do Engine que o NWindow lê (foi assim que se fechou o bloco de estatísticas do `User`, e confirmou-se o `FL2GameData`: `0x3F63C → 0x722CC`,
`0x65A8C → 0x9871C`, `0x77D5C → 0xAA9EC`…, todos `+0x32C90`). O `verify_data` do ciclo pode repetir isso para os 28 campos do `User` e os registros que ainda não têm prova.
