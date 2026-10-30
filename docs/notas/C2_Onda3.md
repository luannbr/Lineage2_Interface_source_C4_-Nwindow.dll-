# C2: onda 3 (o que sobrou depois da onda 2)

Estado de partida (commit 1afe4a8): 3.294 ganchos, 87,45 % dos bytes. `db/functions.csv` tem 183 funções de jogo `todo`
(42,7 KB) fora do nProtect: 80 com nome de classe e 102 anônimas (`sub_XXXXXXXX`, 16,7 KB). Fora do plano, como na onda 2:
o nProtect GameGuard ligado estaticamente (NPGameLib, NPGSys, NPGAuth, NPGComm, NPGRc4, NPGLog, NPGStr, NPGIni,
NPGTempDll, NPGCrc32, NPGameLibApi, 0x100DFEE0..0x100E7050) e o NKeyCrypt (npkcrypt por ordinal): ficam no original. Uma
função anônima que se mostrar pertencer a esse bloco (chama NPGameLib, ou é chamada só por ele) também fica: anote em
`db/names.csv` com a evidência e diga no relatório.

O guia de trabalho é `docs/notas/C2_Agentes.md` (com a seção 9) e o de classes novas está em `C2_Onda2.md`. O resultado da
onda 2 (o que foi feito e as armadilhas da fusão) está no fim de `C2_Onda2.md`; o que vale repetir:
* dois ganchos no mesmo endereço = corpo que o compilador original dobrou (ICF) entre duas classes: o smoke acusa
  "duplicate hook". Fica UM gancho (da classe que o dono do endereço já usa); a outra só define o corpo.
* nada de `swprintf("%c")` com TCHAR (o UCRT guarda um byte só).
* o `cycle.py` para antes de regcheck/verify_data se o verify tiver uma diferença sem nota: resolva ou anote primeiro.
* o scratchpad é um só para todos os agentes: use uma subpasta com o nome do seu grupo.
* funções com caminho para um stub `NW_UNPORTED()` (check_breakers) não podem receber gancho.
* corpos grandes só do C2: use o teste diferencial com emulação (tools/port/emu/, exemplos t_vip.py, t_macro.py,
  t_equippaint.py); a emulação achou erros que o ciclo não vê.

## Grupos (um agente por grupo, um worktree `wt/<nome>` por agente)

Cada grupo tira a lista de `db/functions.csv` (status `todo`). Função `todo` que não esteja em nenhum grupo abaixo é do
`w3misc`. Cada função deve acabar portada com gancho, ou com uma linha dizendo por que não (nProtect, conversão ICF que não
precisa de gancho, corpo de 1 a 3 bytes dobrado com 87 chamadores, etc.).

### w3shortcut (a barra de atalhos, 10 KB)
NCShortCutWnd: OnPaint 100ce740, OnMouseMove 100ce820, OnLButtonUp 100ce9d0 e as anônimas da classe: o pintor da barra
100cd790 (2,5 KB), o pintor do pad 100ce160 (1,5 KB), OnShortCutKeyDown 100cf080, 100cc8d0, 100cd250 (ReloadNames),
100cfe90, 100cce40, 100cd610, 100ccef0 (o console chama por StockCall: dê o nome e porte). Mais: NCSKillItemWnd::OnMouseMove
10090ee0, NActionItem::Select 1003f3b0 / GetActionID 10040190, NCActionWnd::OnDestroy 10042df0, NAmountColor 1002bb70
(NCEditBox::OnPaint e outros chamam por StockCall: troque pelo corpo nosso depois de porta-lo). NShortCutItem (0x1A7C) e a
cadeia de chamadas nos console (UseShortCut etc.) já estão portadas; o NCShortCutWnd.cpp tem código `#if 0` do C1 que
pode ser apagado quando as funções forem trocadas. Arquivos: NCShortCutWnd, NCSkillWnd/NCActionWnd e cabeçalhos iguais.

### w3gm (janelas de GM, 12 KB)
NCGMWnd: OnCreate 100770a0 (2,6 KB), vfn077/078/079 (10078bf0, 10078d60, 10078ec0: OnNewButton4-6, consultam tabelas do
GL2GameData em +0x27E8/+0x27E4/+0x4CB8: mapear), NCGMEquipItemWnd OnPaint 10073cd0 + vfn067 10072e80, NCGMInventoryWnd
OnCreate 10078570 + OnPaint 10074250, NCGMPledgeWnd::OnPaint 10076800, NCGMActionWnd::OnCreate 10076c70,
NCGMSkillWnd::OnDestroy 100745e0 (NCSkillWnd.cpp), NCPledgeWnd vfn076/077 (10042920, 100429c0: abrem outras janelas) e as
anônimas 10073020, 10073030, 10073ac0 se forem dessas janelas. Arquivos: NCGMWnd, NCGMPledgeWnd, NCGMActionWnd,
NCGMQuestWnd, NCGMInventoryWnd, NCGMEquipItemWnd, NCGMSkillWnd.cpp (se existir; senão em NCSkillWnd.cpp), NCPledgeWnd.

### w3ui (janelas de lista, cerco, quadro, vídeo e o conversor Thai, 15 KB)
NCSiegePartInfoWnd: OnCreate 100d25d0 + AddClan 100d1e80, NCSiegeDateInfoWnd: WindowProc 100d01b0 + OnDestroy 100d1080 (e o
resto da NCSiegeInfoMainWnd que sobrar), NCBoardWnd::OnCreate 10033920 (o ctor de NCBBSHtmlViewer 10033590 já está
portado: o gancho agora é possível; botões em x 10,310,70,130,190,250 y 24 50x16, textos 377,403,379..382), NCVideoOptionWnd::Apply
100a1b90 (3,5 KB) e as anônimas ao redor (100a13c0, 100a3a10, 100a0310: veja quem as chama), NCListCtrl::OnPaint 100d0480,
NCHeaderCtrl::OnDestroy 1001c5d0, NCScrollWnd::ShowBars 100254d0, NCLoginWnd::HideWindow 10082be0, NCommandParser::ExpandToken
1003e900 (+ a anônima 1003e5b0 se for dela), L2WordWrapperWnd (as 7 funções 1003f030 .. 1003fff0: ferramenta dos
desenvolvedores que converte HTML tailandês por um servidor: sockets, arquivos ..\INPUT_THAI_HTML; mesmo assim é código
do jogo: porte, com chamadas de ws2_32 pelos thunks do original). Arquivos: NCSiegePartInfoWnd, NCSiegeInfoMainWnd,
NCBoardWnd, NCVideoOptionWnd, NCListCtrl, NCHeaderCtrl, NCScrollWnd, NCLoginWnd, NCommandParser, L2WordWrapperWnd.

### w3pet (visualizadores, pet e janelas pequenas, 6 KB)
NCNpcView_CharacterSelectWnd::vfn067 = OnSpawnTest 1009e230 (botão "TEST": 30 arqueiros esqueleto; sequência longa do
engine), FillItemInfo do visualizador de pawn 100aafe0 e do de NPC 1009c530 (o registro do item vira NItemInfo; chama dois
virtuais do registro, slots 0 e 2, que não foram identificados: ache o tipo), as anônimas da janela do pet 100b6080,
100b6170, 100b6190, 100b6440, 100b63c0, 100b7260, 100b74f0, 100b7550 (NCPetInventoryWnd: AddInventoryItem,
UpdateInventoryItem etc.; as de endereço 100b6190/100b6440 têm stub), 100b2b90, 100b13f0, e os OnDestroy: NCPetDetailStatusWnd
100b2ae0, NCSummonedDetailStatusWnd 100b2c60, NCSummonedActionWnd 100b4fa0, NCPawnInfoWnd 100a6720, NCMagicSkillWnd 10091790,
NCMessageWnd 10098710, NCAutoLogWnd 10043ff0, NCTrainListWnd 100dcbc0 + as anônimas 10098f00, 10098790, 1009a540 se forem
dessas janelas. Arquivos: NCNpcViewrWnd, NCPawnViewrWnd, NCPetInventoryWnd, NCPetWnd, NCSummonedWnd, NCStatusWnd, NCPawnInfoWnd,
NCMagicSkillWnd, NCMessageWnd, NCAutoLogWnd, NCTrainWnd.

### w3misc (console, auxiliares, destrutores e todo o resto, 12 KB)
* Console: NConsoleWnd::GetMyLocation (no C2 é 0x10057dd0, anônima: gancho nele; o stub C1 em NConsoleWndNet.cpp morre),
  GetMyPawn 10060af0, GetMacroCount 10056a20, GetMacro 10060ce0, IsAnyMacroRunning 10060d10, RequestAuthCardKeyLogin 10052110,
  Free 10026550 (dobrada com o operator delete), NCConsole::ToggleMacroEdit 1004c160, NCAuthWnd::ResetInput 10057cd0, e as
  anônimas do console (100569e0, 10056500, 10060d70, 10060dd0, 10060e90, 10061cc0, 10066fd0, 10057730, 100578a0, 10057940,
  10057c80, 10057e20, 100574e0 [o ramo GIsL2NetLogin que o remendo binário do Initialize pula: porte e diga se é nProtect],
  10055e20, 10055615, 10055c10, 100560f0, 10056130, 10054195, 10054266, 10052c50, 10051b50/70 HasNoFinalConsonant,
  1004c100, 1004c440, 1004c5c0 CastToLineagePlayerController, 100450b0 e as do limitador de chat 100479f0, 10048c30,
  10048c90, 10048ba0, 10040930).
* Auxiliares do núcleo: FL2SystemMsgData cópia/destrutor 10055ee0/10055f50, TIntArray 1001fb70 e as outras instâncias de
  destrutor de TArray (1000d540, 1008f7b0, 100c37c0, 1007af00, 100dde60, 100dbb90, 100dc0c0), NHtmlObject 100326e0 /
  NHtmlBlock 10032760 (ctors só chamados por bases), NCEditBox::OnReading 1000af1e (cauda), NCHtmlViewer::SetOwner 1002f6a0
  (regcheck: o sub_10093290 mantém ecx: ligue junto com os chamadores), NL2Font::GetTexture 10072490, NCCmdTarget::SetStyle
  10001ad0, Shared::GetFirstDword_ICF 100e5330, appMemcpy 10001790, as anônimas pequenas do núcleo (10019a10, 10019e00,
  10019560, 10019570, 10001990, 1000fdef, 1000fea0, 10010c10, 10010c40, 100128d0, 100375e0, 10009510, 10025240, 1002525a,
  1002cec0, 1002e460, 1002e682, 1002ec97, 1002ece6, 100324e7, 100067d0 [~NDialogInfo], 1006d940, 100c0d30, 100c0090,
  100c3510, 100bfed0, 100dfb40, 100d8540, 100cce40 se o w3shortcut não pegar).
* Destrutores e `scalar deleting destructor` de NCChatWnd (1004b6d0/1004b6a0), NCDynamicLinker (1003adf0/1003adc0: veja o que o
  w2html e o w2newui deixaram: o gancho de 1003adf0 foi movido para o NCHtmlButton, 1003deb0), NCShopWnd (92370/923c0),
  NCTradeWnd (db290/db260), NCTrainInfoWnd (de170/de140), NCTrainListBox (ddf50/ddf20). Convenção do projeto: o `scalar
  deleting destructor` não leva gancho (mas confira o `~X` por trás dele).
* Dê nome (db/names.csv, uma linha de evidência) a toda função anônima que portar.

## Resultado (07/10/2026)
Os cinco grupos foram fundidos (commit 70e6f8e): **3.444 ganchos, 91,94 % dos bytes do trabalho (842.625 de 916.452)**,
verificação 3.111 ok + 333 explicadas + 0 sem explicação, regcheck limpo, 0 ganchos alcançam um stub que quebra. Nada foi
rodado no jogo. Emulação nova: t_gm, t_shortcut, t_viewitem, t_spawn, t_snoop, t_w3ui, t_misc.

Sobra `todo`: 106 funções / 31 KB. 92 são nProtect/NKeyCrypt (ficam no original) e 14 (306 bytes) estão com a razão em
`db/names.csv` (corpos de 1 a 4 bytes que o linker dobrou com dezenas de chamadores, ctors de classe abstrata, thunks de
unwind, convenção própria, destrutores `scalar deleting`). Não sobrou código de jogo sem destino. O que resta de dívida são
as 108 diferenças de `verify_data` sem nota: docs/notas/C2_Onda4.md.

Erros achados e consertados nesta onda (a emulação achou três que o ciclo não vê): `NConsoleWnd::Snoop` chamava um
auxiliar com 4 argumentos quando ele leva 5 (pilha corrompida), `_ftol` do VC 7.0 guarda o dword baixo (o nosso estourava:
ScaleRect, AddLine passam por __int64), ordem de chamada em UpdateMacro/ReloadNames. Erro meu na fusão: um commit saiu com
marcadores de conflito em NGameData.h (script que não tratou CRLF); corrigido em seguida (51623a0), sem marcadores no repo.
Regra: depois de resolver conflito, rodar `git grep -n '^<<<<<<< '` antes de commitar.
