# C4 — levantamento da source (21/10/2026, fim da onda C do estágio 4)

Fonte: `docs/PROGRESSO.md` (gerado pelo ciclo) e `db/functions.csv` do commit `s4-wave-c-merged`.
"Feito" = função com gancho ativo na DLL (`NW_HOOK_*` no começo da linha).

## Número principal

| | funções | bytes | % do código |
|---|---:|---:|---:|
| código a decompilar (game + init) | 7.202 | 1.448.754 | 100 % |
| **reimplementado, com gancho ativo** | **5.456** | **1.350.076** | **93,19 %** |
| sem gancho, por convenção (destrutores deleting, InternalConstructor) | 587 | 15.842 | 1,09 % |
| só na fase 5 (inicialização estática, DllMain) | 1.038 | 48.609 | 3,36 % |
| **falta (todo)** | **121** | **34.227** | **2,36 %** |

Marcos: fim do estágio 3 = 2.846 ganchos; fim da onda A = 4.224 / 62,53 %; fim da onda B = 5.387 / 92,19 %; **fim da onda C = 5.456 / 93,19 %**
(+69 ganchos, +1,0 ponto). O C3 terminou em 4.970 ganchos / 93,03 %.

**Do "falta" de 34.227 bytes, 28.815 (82 funções) são o GameGuard (`NPG*`), fora de escopo por decisão do usuário.** O que sobra de
verdade são 39 funções / 5.412 bytes (0,37 %):

| classe | funções | bytes | por que ficou |
|---|---:|---:|---|
| Md5 (CopyWords / DecodeWords / Transform) | 3 | 2.334 | mortas no modo gancho; shims com argumento em registrador falham no regcheck |
| anônimas (`sub_`) | 10 | 1.008 | pedaços sem classe |
| NConsoleWnd | 5 | 420 | getters pequenos e pedaços |
| NCBroadcastObserverWnd, NCVirtualWndMain | 5 | 686 | |
| demais (NCEditBox, receitas, NStlWString, NStdString, TaskBox...) | 16 | ≈ 960 | cada uma abaixo de 135 bytes |

A onda B deixou ≈ 20 KB de resto; a C fechou o que era ponderável (NCInvenItemWnd::OnLButtonUp, NCTaiwanIME, NToolTip Draw/DrawFull,
ConstructConsole/DestroyConsole). O que sobrou ficou stock de propósito: um gancho errado custa um crash no jogo, um stub não custa nada.

## Onda C por lane (ordem de integração)

| lane | ganchos | total depois do merge | o que fez |
|---|---:|---:|---|
| debt1 | +6 | 5.393 | construtor e destrutor do NConsoleWnd, slot 56 (`SetFrameTitleID`), `SetRes` do NCVideoOptionWnd, stand-ins removidos |
| inven | +15 | 5.408 | `NCInvenItemWnd::OnLButtonUp`, ctors/dtors dos slots de item, `UseItem` |
| ime | +12 | 5.420 | `NCTaiwanIME` (9), `OnInputLangChange`, `GetLanguageID`, `NCIMEManager::WindowProc` |
| misc | +36 | 5.456 | tooltip (Draw/DrawFull), `SendMessageBody`, `MsnFit*`, `MakeTimeStr`, `appMemcpy`, ctors pequenos |
| debt2 | 0 | 5.456 | dívidas de comentário/cabeçalho com evidência C4; 13 notas de `verify_data` |
| audit | 0 | 5.456 | `check_literals` triado (527 alertas → 19 revisados), novos julgadores, 7 bugs |
| vtslots | 0 | 5.456 | `vt_slots.py` (25.984 slots das nossas vtables contra o stock), 5 correções |
| proof2 | 0 | 5.456 | messenger / rest1 / VIP provados no emulador (635 testes), 2 bugs |
| proof1 | 0 | 5.456 | janelas de sistema provadas no emulador (179 ganchos, 204 entradas), 5 bugs |

Todos os ciclos fecharam com `0 errors`, `verify 0 sem explicação` (5.134 ok / 322 explicados) e `CHECKS: ALL OK`.

## Bugs que a onda C achou (nenhum era visível aos julgadores da onda B)

- **Vtable própria com corpo C3 de função dobrada** (classe nova de bug): com o ctor hookado o objeto carrega a NOSSA vtable, e o
  slot 25 de 28 classes (`WindowProc`) rodava o `IsEnabled` do C3, que o stock C4 (`0x10148F10`, `0x10021E60`) não tem. Mais 5 slots
  (NCBBSHtmlViewer 85, NCNpcView_InvenItemWnd 67, NCShopWnd/NCDeliverWnd 25, grids de item 25, NCPetInventoryWnd 62).
- Tabela de quests do C3 em vez da do C4 (2000 × 0xA0 em `GL2GameData+0x3D28`): `GetQuestText`, `AddQuestID`, `GMViewAddQuestID`.
- Apagar macro chamava `RequestMakeMacro` em vez de `RequestMagicSkillUse`.
- Setters da loja privada (comprar) escreviam em Mode/TotalPrice (endereços C3 dobrados); `NCDeliverWnd::SetTargetID` era stub.
- `UseItem`: `IsDwarf` errado (inven e audit acharam o mesmo).
- `NCVideoOptionWnd::Apply`: `SetRes` empilhava 6 valores, o stock empilha 7 (o 7º vinha do lixo da pilha). Achado por debt1 e por proof1.
- `NCServerSelectWnd::SetPing` gravava o ping em `id - 1`; `OnServerListEnd` não zerava `LastServerRow`.
- `NCLoginFunctionWnd::OnPaint` desenhava o fundo com 120×138 fixo; `NCQuestListInfoWnd::FillList` reiniciava o ícone a cada linha.
- `FriendBox::AddUser`: o ícone `msnicon4` era para quem NÃO é `FLN` (o stock: só para `FLN`); `NMsnGroup::SetName` trocava os argumentos
  de `MsnStrReplace` (o nome decodificado ficava com `%20`).
- `OnInputLangChange`; nomes trocados `SpecialServer` (+0x3C68) / `PartyItemRule` (+0x3C6C); `KNOWN_FRAMES 10094ed0`.
- Juiz errado: `check_frames.py` lia a constante mágica `0x88888889` (divisão por 60) como tamanho de `__chkstk`.

Ferramentas novas em `tools/port/`: `check_literals.py` (reescrito, `db/literals_ok.txt`), `check_tiny.py`, `check_vslots.py`,
`vt_slots.py` (`db/vtslots_ok.txt`), todas juízes do `checks.py`; e o harness de janelas no unicorn (`emu/win/pw.py`, `tr.py`,
`t_video`, `t_srvsel`, `t_gara`, `t_sys`, `t_msn1..16`, `t_rest1`, `t_vip5`, `run_proof2.py`).

## O que a prova no emulador NÃO cobre

- `NotificationProcess`, `DispatchProcess`, `SwitchboardSession`, `NMsnPassport` (wininet): sem modelo no harness; `ChatWnd`, `NCChatWnd`,
  `InviteWnd` não foram alcançadas; o multisell não foi reconferido; `NCLoginWnd::WindowProc` e `SetTabDisabled` seguem stock.
- Cada teste roda stock contra nosso no mesmo estado de janela; ele prova que NÃO divergimos do stock, não que o estado montado no
  harness é o que o jogo monta. Código do Taiwan IME só roda com `GLanguageType 3`.

## Dívidas conhecidas (para o próximo passo)

- **Varredura no nível das chamadas** (classe de bug que nenhum julgador cobre): um corpo hookado que chama a cópia compilada de uma
  função "gêmea" dobrada pelo linker em vez do endereço hookado (achado em `NCManorShopWnd::WindowProc`). Comparar cada chamada direta
  de um corpo hookado com os alvos de chamada do stock.
- ≈ 40 structs `Ext` de apoio em NConsoleWndAnon/Misc/C2/C4; ≈ 279 + 183 + 6 notas herdadas "C3:" nas notas de verificação.
- Comentários velhos: NCManorShopWnd.cpp, NCRecipeGaraTree.cpp, Msn.h, NotificationProcess.h, SwitchboardSession.h; nomes de campos de
  NItemEntry com números do C3.
- `GetSpecialServer` segue não virtual atrás da NewVirtual416.
- Incompatibilidade dos ids de NPC do servidor C4 (ver PORTE-C4.md §5.1).

## Verificação em jogo

Só o build de **5.387 ganchos** entrou no mundo (teste do usuário em 21/10, 0 exceções). O build de **5.456 ganchos** está instalado em
`system_nwtest` e ainda não foi jogado. `docs/PROGRESSO.md` segue com "Verificadas no jogo: 0": só a entrada no mundo foi vista no jogo; as
janelas abertas depois dela estão provadas no emulador, não no cliente.

Roteiro de teste (abrir cada janela uma vez; mandar `NWindowDecomp.log` depois): seleção de servidor (ping e último servidor lembrados),
opções de vídeo (aplicar uma resolução) / jogo / áudio, inventário (usar item, arrastar para o chão, para a loja, para o atalho), loja
privada (comprar e vender), manor, receitas, macro (criar, apagar), janela de quests, messenger (lista de amigos com status, grupo com
espaço no nome), loja VIP, tooltips, chat e console, visualizador de BBS (som), inventário do pet, NPC viewer. Olimpíada e hero tower
dependem do servidor.
