# C2: onda 2 (o que sobrou: funções grandes, classes novas, o resto do console)

Estado de partida (commit 78dfab0): 2.359 ganchos, 49,9 % dos bytes; `db/functions.csv` tem 1.317 funções de jogo `todo`
(420 KB). Fora do plano: o bloco `NPGameLib`/`NPGSys`/`NPGAuth`/`NPGComm`/... (0x100DFEE0..0x100E7050, 83 funções)
é a biblioteca do nProtect GameGuard ligada estaticamente, não é UI nem código nosso: fica no original.

Cada grupo abaixo é uma frente. O agente tira a lista de trabalho de `db/functions.csv` (status `todo`, coluna
`class`/`name`), das funções anônimas (`sub_XXXXXXXX`) que as classes dele chamam (`python tools/xrefs.py <va>`
mostra os chamadores; dê nome em `db/names.csv` com uma linha de evidência) e dos `NOT PORTED` dos arquivos dele.
Função anônima que dois grupos chamam: porta quem a achar primeiro; o ciclo avisa de gancho duplicado.

## Como criar uma classe nova do C2 (as 47 que o C1 não tem)
1. Dados: `analysis/classes.json` (nome C++, UClass `static_class`, `size`, `super`, `class_flags`, `vtable`,
   `vtable_slots` com o endereço de cada slot), `analysis/classes.md`, os nomes no `db/functions.csv`.
2. Arquivos: `include/nwindow/<Classe>.h` e `src/nwindow/<Classe>.cpp` (o CMake pega o `.cpp` sozinho; rode
   `cmake -S . -B build -DNWD_GAME_SOURCES=ON` de novo para ele ver o arquivo novo). Modele-os em uma classe
   parecida que já exista (por exemplo `NCCheckBox` para um controle, `NCPawnSelectWnd` para uma janela).
3. Cabeçalho: `NW_DECLARE_CLASS(Classe, Pai, flags, 0x<static_class>)`, campos em ordem de offset (preenchimentos
   `BYTE _UnknownXXXX[n]` onde não souber), virtuais na ordem exata da vtable do C2 (slots herdados não se
   redeclaram; `NewVirtualNN` do pai podem ser renomeados quando você souber o que são), `NW_ASSERT_OFFSET` e
   `NW_ASSERT_SIZE(Classe, <size de classes.json>)`.
4. Fonte: construtor(es), destrutor, e cada virtual com `NW_HOOK_*` no endereço do C2; no fim,
   `NW_CHECK_VTABLE(Classe, 0x<vtable>, <nº de slots>)` (em modo gancho é só informativo).
5. Uma classe cujo construtor não é portado não pode ser construída por código nosso (`new Classe`): use
   `nw::StockCall` para o construtor do original enquanto isso. Um construtor portado exige os dos ancestrais
   nossos portados (o `cycle` avisa).
6. Verificação: igual às outras (guia §5). Funções só do C2 não têm corpo do C1 para comparar: leia o asm e a
   decompilação; para as maiores existe o teste diferencial com emulação (`tools/port/emu/`, ver docs/notas/C2_Porte.md).

## Grupos (um agente por grupo, um worktree `wt/<nome>` por agente)

### w2console (console, parte 1: ciclo de vida e janelas)
`NCConsole` (OnCreate 11,7 KB, OnDestroy 2,9 KB, ShowWindow), `NConsoleWnd`: Initialize (3,8 KB), InitializePawnViewer,
Save/Load/ResetWindowPositions (4–5,5 KB cada, janelas novas), Tick, UpdateCursor, DispatchWndMsgX, UnlockCursor,
GetIMEType, StartNotice, AuthLoginFail, AcceptGameStart, BlockedAccount(WithMsg), ShowDialog, MakeSystemMessage/
EndSystemMessageParam, RegisterPledgeCrest/AllyCrest, HasBadWord, AcquireSkillInfo, SetViewRect.
Arquivos: src/nwindow/{NCConsole,NConsoleWnd,NConsoleWndWindows,NConsoleWndLogin,NConsoleWndInput,NConsoleWndText}.cpp +
NCConsole.h/NConsoleWnd.h.

### w2consolenet (console, parte 2: rede e slots só do C2)
`NConsoleWnd::vfn314` (7,2 KB, slot 314 = 2.210 instruções), os ~46 virtuais só do C2 (`PureSlotNNN`: manor, janelas de
receitas, henna, macros, cabeçalho de MSN...), RequestCharacterCreate (CharacterInfo é 0x60 no C2), RequestAssist,
AddAbnormalStatus (FAbnormalStatus), SkillCoolTime/UseShortCut/UseAction (NShortCutItem 0x1A7C), AddVIPNeedItemList,
ReceiveVIP*, os ProcessChatLine/NCChatWnd (6,9 KB). Arquivos: src/nwindow/{NConsoleWndNet,NConsoleWndEvents,
NConsoleWndItems,NConsoleWndData,NConsoleWndActions,NCChatWnd}.cpp, NShortCut* e FAbnormalStatus nos cabeçalhos que
definem esses tipos (NCShortCutWnd.h, NCAbnormalStatusWnd.h).

### w2edit (entrada de texto e tailandês)
NCMultiEdit (17 funções, 20 KB: OnPaint, OnKeyDown, OnCreate, CountRows, GetRow*, InsertString/InsertChar, SetCaretX,
PaintComposition JP/TW/CN), NCEditBox (OnChar/OnKeyDown/OnPaint/InsertText/DeleteChar: ramos tailandeses
GLanguageType 5, helpers 0x1000a1b0 / 0x1000a310 / ThaiCharacter::GetWidth 0x1000b190), NCHtmlMultiEdit, NCHtmlEdit,
NCThaiIME e o resto das IME, NCCandidateBox, NCCountSpin. Arquivos: NCMultiEdit, NCEditBox, NCHtmlEdit, NCIME*,
NCKorean/Japanese/Taiwanese/Chinese/ThaiIME, NCCandidateBox, NCCountSpin (.cpp/.h).

### w2html (layout HTML e visualizadores)
NHtmlBlock/NHtmlTable/NHtmlItem/NHtmlObject (C2: NHtmlObject 0x4C, NHtmlBlock 0x870, tabela em 0x870.., ramos
tailandeses, laço de borda grossa), NCHtmlViewer::BuildDocument/AddEdit/AddMultiEdit/AddFileLink/AddActionLink/
SetOwner (cuidado: o regcheck bloqueia SetOwner), NCBBSHtmlViewer, NCNPCHtmlViewer, NCTutorialHtmlViewer,
NCFileLinker/NCActionLinker (construtores com std::wstring por valor), NCHtmlButton. Arquivos: NHtml*.cpp/.h, NCHtml*.cpp,
NCBBSHtmlViewer, NCNPCHtmlViewer, NCFileLinker, NCActionLinker.

### w2pawn (visualizadores de personagem e login)
NCPawnViewrWnd (OnCreate 4,3 KB, OnPaint, OnSearch, GetItemCategory, AddAllItem), NCPawnViewr_HairChangeWnd (classe
redesenhada), NCMSViewrWnd + NCMSView_* , NCNpcViewrWnd + NCNpcView_CharacterSelectWnd, NCPawnView_CharacterSelectWnd
(OnNameComboBox), NCServerSelectWnd + NCServerSelectListCtrl (OnCreate, OnOK, SetPing, AddServer, SortByQuality; o
NListRow/NListCell já têm layout C2), NCPetitionWnd. Arquivos: NCPawn*, NCMSViewrWnd, NCNpcViewrWnd, NCServerSelectWnd,
NCPetitionWnd (.cpp/.h).

### w2status (status, GM, listas, quest, match)
NCDetailStatusWnd::OnPaint (6,8 KB) e NCGMDetailStatusWnd::OnPaint (6,1 KB), NCGMWnd (OnCreate e subjanelas),
NCSiegePartInfoWnd, NCMatchWnd/NCMatchOptionWnd/NCMatchListCtrl/NCSiegeInfoListCtrl, NCBoardWnd/NCBoardPage,
NCQuestWnd/NCQuestInfoWnd/NCQuestItemWnd, NCAbnormalStatusWnd, NCShortCutWnd (pintura, mouse, UpdateItem; NShortCutItem
0x1A7C), os quatro pintores de tool tip que faltam (NCShortCutWnd 0x100cfc80, NCAbnormalStatusWnd 0x100d8800,
NCMacroArrayWnd 0x10090990 → fica com o grupo de macros, NCRecipeManufactureItemWnd → receitas),
NCMatchWnd::UpdatePlayerInfo. Arquivos: NCDetailStatusWnd, NCGMWnd, NCSiegeInfoMainWnd, NCMatch*, NCBoard*, NCQuest*,
NCAbnormalStatusWnd, NCShortCutWnd.

### w2shop (loja VIP, equipamento/henna, arrastar e soltar)
NCVIPShopWnd (9 + 12), NCVIPShopItemInfoWnd (DrawItemInfo 6,3 KB), NCVIPShopItemListBox, NCEquipItemWnd (OnPaint, henna,
OnLButtonUp), os OnLButtonUp/OnMouseMove de NCInvenItemWnd/NCPetInventoryWnd/NCSkillItemWnd/NCActionItemWnd (fluxos novos
de soltar ícone em macro/chat), NCInventoryWnd::IsInTrash, as classes novas de henna (NCHennaInfoWnd, NCHennaListWnd,
NCHennaListBox) e de manor (NCManorShopWnd, NCManorShopItemWnd).

### w2recipe (receitas e macros: classes novas)
NCRecipeWnd, NCRecipeBookWnd, NCRecipeBookItemWnd, NCRecipeBuyListWnd, NCRecipeBuyManufactureWnd, NCRecipeManufactureWnd,
NCRecipeManufactureItemWnd, NCRecipeShopWnd, NCRecipeShopItemWnd, NCRecipeListBox, NCRecipeTree, NCRecipeTreeCtrl,
NCRecipeTreeWnd e as de macro: NCMacroListWnd, NCMacroEditWnd, NCMacroEditList, NCMacroInfoWnd, NCMacroArrayWnd (e o pintor
de tool tip dela). O `docs/notas/F6_Craft.md` do projeto C1 descreve as janelas de receita que o C1 reconstruiu à mão
(são as mesmas, nativas, no C2: serve de especificação).

### w2newui (resto das classes novas)
NCGaraDialogBox (a caixa de mensagem do C2: 0x23C; OnCreate 0x10007110, OnPaint 0x10007560, vfn068 0x10007b20 de 4,9 KB;
as funções antigas do NCDialogBox que ficaram com stub de salto pertencem a ela), NCWarningDialogBox,
NCKeyCryptWarningDialogBox, NCAnnounceWnd, NCEulaWnd (0x1009bab0 é o OnPaint dela), NCCreditWnd, NCXMasSealWnd, NCEmpowerWnd
(14 funções), NCPetitionBeginWnd, NCCalcWnd/NCCalcEditCtrl, NCTree/NCTreeCtrl/NCTVItem/NCSliderCtrl/NCThumbButton,
NCDynamicLinker, L2WordWrapperWnd, NTownMapWnd, NCObserverWnd, NCGMSnoopWnd/NCGMSnoopManager, NCMSNWnd/MSNSocket (usa
ws2_32), NCThaiIME se o grupo w2edit não pegou.

## Resultado (07/10/2026)
Os nove grupos foram fundidos na main (commit 0721069): **3.294 ganchos, 87,45 % dos bytes do trabalho (802.349 de 917.473)**,
verificação 2.978 ok + 316 explicadas + 0 sem explicação, regcheck limpo, nenhuma função com gancho alcança um stub que
quebra. Nada foi rodado no jogo (sem servidor C2): "verificado" é estrutural + emulação (tools/port/emu: t_edit, t_multi,
t_html_*, t_macro, t_recipe*, t_vip*, t_equippaint, t_lbup).

Sobra `todo`: 272 funções / 72,5 KB. 89 delas (30 KB) são o nProtect GameGuard ligado estaticamente (NPGameLib, NPGSys,
NPGAuth, NPGComm, NPGRc4, NPGLog, NPGStr, NPGIni, NPGTempDll, NPGCrc32, NPGameLibApi) e o NKeyCrypt: ficam no original. O resto
(~43 KB) é: 102 funções anônimas (17 KB, ainda sem nome), NCGMWnd (OnCreate, OnNewButton4-6), NCVideoOptionWnd (3,5 KB),
L2WordWrapperWnd (conversor Thai-HTML dos desenvolvedores), NCShortCutWnd (OnPaint, os dois pintores 100cd790/100ce160,
OnMouseMove/OnLButtonUp), NCSiegePartInfoWnd, NCBoardWnd::OnCreate (precisa do ctor de NCBBSHtmlViewer, 10033590),
NCGMEquipItemWnd/NCGMInventoryWnd/NCGMPledgeWnd/NCGMActionWnd (OnPaint, OnCreate), NCNpcView_CharacterSelectWnd::OnSpawnTest,
NCListCtrl, NCommandParser, NCPledgeWnd OnNewButton1/2.

Para a próxima rodada: NCHtmlViewer::SetOwner (1002f6a0, regcheck: ecx), NConsoleWnd::GetMyLocation = 0x10057dd0 (C2),
NCActionItemWnd::ClearItems 100d8950 (dividida com NCAbnormalStatusWnd::ClearStatus), os ctors NCRecipeTree* agora ligados.
Armadilhas vistas na fusão: corpos que o compilador original fundiu (ICF) viram DOIS NW_HOOK no mesmo endereço (o smoke
acusa "duplicate hook"): fica o gancho de uma classe e a outra só define o corpo (NCHtmlEdit/NCCalcEditCtrl::WindowProc
1003a760, NCVIPShopItemListWnd::Clear/NCRecipeBuyListWnd::ClearList 100c11b0). O cycle.py para antes do regcheck e do
verify_data quando o verify tem uma diferença sem nota.
