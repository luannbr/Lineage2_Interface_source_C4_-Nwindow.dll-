# C3 pacotes, pista T2: tudo o que o cliente C3 manda ao servidor (c2s) (17/10/2026)

> Nota da publicação: esta nota fazia parte de uma auditoria de pacotes cujo lado do servidor (código Java, as notas das
> outras pistas e as ferramentas delas) não é publicado; as referências a elas ficam só como histórico.

Pista T2 da auditoria de pacotes (briefing em `C3_Agentes_Pacotes.md`). Fonte da verdade: o `Engine.dll` do cliente C3
(md5 `9ad6c902...`, o mesmo de `orig/Engine.dll`; só leitura) e o `NWindow.dll` stock (md5 `f0362880...`, export Ghidra em
`analysis/ghidra`). Nada foi executado; "certeza alta" = formato lido do disassembly (`pefile` + `capstone`) e opcode
conferido com a especificação (`C3 Rise Of Darkness.html`, 185 nomes de pacotes do cliente) e com o código Java.

Entregáveis:

* `tools/c3/c2s_formats.py` (re-executável; `--help`; tudo sai do `Engine.dll` + do export Ghidra + do Java): `python tools/c3/c2s_formats.py --json analysis/c3_c2s_formats.json --md <saida.md>`.
* `analysis/c3_c2s_formats.json`: 201 chaves (212 entradas contando as variantes; 207 do game server + 5 do login server).
  Chave = opcode em hex (`"45"`), `"CF:0003"` para o prefixo estendido do C3 (0xCF + word), `"39:02"` (0x39 + byte), `"LOGIN:00"` para o
  protocolo do login server. Campos: `name`, `fmt`, `netHandler_vslot`/`netHandler_vofs`, `engine_rva`, `send_site`, `params`, `fields`,
  `field_names`, `item_fmt` (formato de cada item das listas), `nwindow_stub`, `nwindow_callers`, `nwindow_detail` (cada chamador
  com o que empilha), `certainty`, `spec_name`, `server_class`, `server_opcode_matches` (`true` / `false` / `null` = sem handler),
  `server_reads`, `layout_check`, `notes`, e `variants` (outro envio com o mesmo opcode).
* Esta nota.

O servidor Java está sendo editado em paralelo por outras pistas (T3 e as de correção). Os campos `server_*` do JSON são o estado do
`java\` no momento em que a ferramenta rodou; a seção 5 mostra o estado do início da auditoria (o `GamePacketHandler.java` de
`java_backup_pre_audit`) e o de agora. Re-execute a ferramenta para atualizar.

## 1. Resultado em poucas linhas

* **Os formatos e opcodes de todos os pedidos do cliente estão extraídos e batem com a especificação** (os nomes do vtable do
  `UNetworkHandler` do C3 também). Os pedidos que importam para jogar (sentar/levantar, alvo, clique, ataque, andar, chat, itens,
  skills, lojas privadas) estão na seção 4, com o que o NWindow empilha e o que o servidor lê.
* **O lado do cliente (NWindow e Engine) está correto para sentar/levantar e cancelar alvo**: `RequestActionUse` sai como
  `45 | d actionId | d ctrl | c shift`, `RequestTargetCancel` como `37` sem campo nenhum. Os defeitos eram do servidor (tabela de ações
  vazia e `readH` em `RequestTargetCanceld`).
* **Discordâncias de OPCODE (o servidor executaria o pacote errado ou nenhum)**, estado no início da auditoria: `0xB4`
  (`RequestRecipeShopManageCancel` do cliente x `SnoopQuit` do servidor), `0xC3` (`RequestProcureCrop` x `RequestHennaItemInfo`) e todo o
  prefixo estendido `0xCF + word` (party room, auto soulshot) que o servidor só conhecia como `0xD0 + word` (e o `0xCF` dele era
  `RequestReload`). Já corrigidas por outra pista no `GamePacketHandler.java` atual (seção 5).
* **Pedidos que o cliente manda e o servidor não tem handler** (cada um conta para `UnknownPacketAmountLimit` (padrão 10) e depois a conexão é
  fechada, `GameClient.onUnknownPacket`): no início `0x23` criar clã, `0x28` dissolver clã, `0x4E 0x50 0x51 0x69` (resposta/rendição de
  guerra de clã), `0x75 0x92` (cancelar a janela de loja), `0x7A` (resposta de ressurreição), `0xA9`, `0xB0`, `0xCE`. Hoje restam
  sem handler os de guerra de clã (`0x4E 0x50 0x51 0x69`) e `0x28`.
* **O servidor lia mais do que o cliente manda** (BufferUnderflowException, pacote perdido, e `FailPacketAmountLimit` (padrão 10) fecha a conexão):
  `RequestJoinPledge` (0x24: o C3 manda 1 dword, o Interlude 2), `RequestAquireSkill` / `RequestAquireSkillInfo` (0x6C/0x6B: 2
  dwords, sem o tipo), `ConfirmDlg` (0xC5: 2 dwords), `RequestPartyMatchDetail` (0x71: 1 dword), `RequestMultiSellChoose` (0xA7: sem
  quantidade), `RequestProcureCrop` (0xC3: itens de 12 bytes), `RequestTargetCanceld` (0x37), `SetPrivateStoreSellList` / `SetPrivateStoreBuyList` /
  `RequestPrivateStoreBuySellList` (0x74/0x91/0x96, layouts de item do C3). Os arquivos já mudaram no `java\` (ver `java_backup_pre_audit`).
* **Esquisitices do cliente** que o servidor tem de tolerar (seção 7): `RequestMagicSkillList` manda o opcode como dword (formato `ddd`),
  `RequestPetitionCancel` / `RequestHennaItemList` / `FinishRotating` mandam lixo de pilha nos campos que o formato promete e a
  chamada não empilha.
* **Achado extra para o porte**: em 47 dos 126 stubs `NConsoleWnd::Request*` do NWindow stock o nome de `db/functions.csv` (herdado do
  C2) não é o do slot que o stub chama no C3 (seção 8). O porte já chama esses por offset (`VirtualCall`); só os nomes enganam.

## 2. Método

1. **O vtable.** O `Engine.dll` do C3 exporta `??_7UNetworkHandler@@6BUObject@@@` (282 slots) e cada método com o nome mangled; o
   `UL2NetHandler` (abstrato, 276 slots, o que o NWindow enxerga) tem os mesmos offsets até o slot 275. Os slots 52..275 são os pedidos
   (`MoveToLocation` ... `RequestAutoSoulShot`). `include/ue2/gen/UL2NetHandler.vt.inc` e `analysis/engine_vtables.txt` JÁ são do C3
   (conferido: `RequestActionUse` = slot 166 = `+0x298`); a ordem NÃO é a do C2.
2. **Conferência do offset.** `NConsoleWnd::RequestActionUse` (stock 0x1005d3a0) é `mov eax,[ecx+0x48]; mov ecx,[eax+0x58]; mov edx,[ecx];
   jmp [edx+0x298]`; `+0x298` é o slot 166 do vtable do C3 = `UNetworkHandler::RequestActionUse` (Engine 0x103f1110).
3. **O caminho do pacote.** `UNetworkHandler::Request*(L2ParamStack&)` desempilha os valores com `L2ParamStack::Top` (Engine 0x1045c080:
   o primeiro empilhado sai primeiro; o NWindow chama `PushBack` com qualquer valor de 32 bits, ponteiro de string inclusive) e chama
   `Socket->Send(Socket, "fmt", opcode, valores...)`: o slot `+0x64` do objeto em `UNetworkHandler+0x3C` (game server; `+0x40` é o
   do login server), varargs cdecl. O `Send` (corpo em 0x103ec200: entra na seção crítica do socket, formata em 0x1FFE bytes, põe o comprimento de
   16 bits na frente, cifra e chama `send`) usa o escritor `Format` (Engine 0x103ebb20). Os pedidos com lista montam os itens num buffer
   local com `FormatTo` (0x103ebf50, o mesmo escritor, sem cabeçalho) e passam o buffer como bloco `b` (comprimento, ponteiro).
4. **A ferramenta** segue cada função de `Request*` em ordem de endereço, com registradores, slots de pilha e a FPU como expressões
   (`Pn` = n-ésimo `Top()`; `argN` = n-ésimo argumento do virtual; `ftol(...)` = float convertido para int antes do push), e anota cada
   `Send` (formato, opcode, valores) e cada `FormatTo` (formato dos itens). Achou também 4 `Send` que não são slots: o
   `ProtocolVersion` (0x00 `cdb`) e o `AuthLogin` (0x08 `cSddddd`) do game server e dois do login server (`cxdhc...`).
5. **O NWindow.** Acha os stubs de 14 bytes (`mov eax,[ecx+0x48]; mov ecx,[eax+0x58]; mov edx,[ecx]; jmp [edx+OFS]`) e as chamadas
   inline `NetHandler->virtual` no export Ghidra (`analysis/ghidra/asm`), e para cada chamada lista os `PushBack` (em ordem de
   código) do `L2ParamStack` construído por último, ou os `push` imediatos quando o virtual não usa `L2ParamStack`.
6. **O servidor.** Lê `GamePacketHandler.java` (estados CONNECTED / AUTHED / IN_GAME, switch estendido) e os `read*()` do `readImpl()`
   de cada classe; compara nome (normalizado, com uma tabela de apelidos revisada) e layout. "Condicional" = o `readImpl` tem
   `if` / `for` / `?`: nesses a comparação é manual.

## 3. O alfabeto do escritor (Engine 0x103ebb20, tabela de saltos em 0x103ebcec, índices em 0x103ebd0c, indexada por letra - 0x53)

| letra | bytes | argumento |
|---|---|---|
| `c` | 1 | inteiro de 32 bits (vai o byte baixo) |
| `h` | 2 | inteiro (vai a word baixa) |
| `d` | 4 | inteiro |
| `f` | 8 | double (dois slots da pilha) |
| `S` | 2n+2 | `TCHAR*` UTF-16 terminado em NUL (ponteiro nulo vira `00 00`) |
| `s` | n+1 | `char*` terminado em NUL (nulo vira `00`) |
| `b`, `x` | n | dois argumentos: comprimento (int) e ponteiro; copia `n` bytes |
| outra | 0 | pulada, não consome argumento |

A PRIMEIRA letra do formato de um `Send` é sempre `c` e é o OPCODE (o primeiro valor depois da string de formato). Os pacotes
estendidos são `cd...` / `ch...` (opcode e depois a sub-opcode como campo). Vetores (`FVector`) viram dwords com `_ftol` antes do push.
É a mesma família de letras do interpretador do s2c (`c d h f S b`), em sentido contrário.

## 4. Os pedidos que importam (conferidos um a um)

Campos depois do opcode. `P0, P1...` = ordem de `PushBack` do NWindow; `argN` = argumento direto do virtual (o virtual não usa `L2ParamStack`).

| pedido (slot / offset) | opcode | formato | campos | NWindow stock (quem chama, o que empilha) | servidor (classe lê) | certeza |
|---|---|---|---|---|---|---|
| `RequestActionUse` (166 / +0x298) | 0x45 | `cddc` | `d` P0 = id da ação, `d` P1 = ctrl, `c` P2 = shift | `NConsoleWnd::UseAction` 0x10074ab0, ramo `default` (ids 0, 1, 15..17, 19, 21..23...): `L2ParamStack(10)`, 3 `PushBack` = `[id, ctrl, shift]` (ctrl = virtual 297 `IsCtrlDown`; shift = `KeyDown(0x10)`); `NCGaraDialogBox::OnOK` (const 0x14, ...) | `RequestActionUse` lê `D, D==1, C==1`: igual | alta |
| `ChangeWaitType(INT)` (62 / +0xF8) | 0x1D | `cd` | `d` 0 = sentar, 1 = levantar (arg1) | `NCChatWnd::ProcessChatLine`: `/sit` -> 0, `/stand` -> 1 | `RequestChangeWaitType` lê `D` | alta |
| `ChangeMoveType(INT)` (61 / +0xF4) | 0x1C | `cd` | `d` 0 = andar, 1 = correr (arg1) | `ProcessChatLine`: `/walk` -> 0, `/run` -> 1 | `RequestChangeMoveType` lê `D` | alta |
| `RequestTargetCancel(L2ParamStack&)` (152 / +0x260) | 0x37 | `c` | nenhum; a pilha é ignorada | `NConsoleWnd::HookShortCutKey` (ESC), `NCTargetStatusWnd::OnClose` (X), `NConsoleWnd::ShowRestartMenu`: pilha VAZIA | `RequestTargetCanceld` lia `H`; hoje lê `H` só se sobrar 2 bytes | alta |
| `Action(INT, FVector, INT)` (56 / +0xE0) | 0x04 | `cddddc` | `d` objectId (arg1), `d,d,d` x y z (arg2, floats viram int), `c` 0 clique / 1 shift-clique (arg3) | `NConsoleWnd::RequestAction` 0x100651f0 (shift = `KeyDown`), `NConsoleWnd::RequestAttack` 0x10065250 (se não for ataque forçado) | `Action` lê `D D D D C`: igual | alta |
| `Attack(INT, FVector, INT)` (55 / +0xDC) | 0x0A | `cddddc` | idem | `NConsoleWnd::RequestAttackForce` 0x1005d3b0 (shift 0), `RequestAttack` (ctrl pressionado) | `AttackRequest` lê `D D D D C`: igual | alta |
| `MoveToLocation(AActor*, FVector, FVector, AActor*, INT)` (52 / +0xD0) | 0x01 / 0x5C | `cddddddd` | alvo x y z, origem x y z, `d` moveMovement (1 quando arg5 == 0); sobre veículo manda 0x5C (id do veículo, ...) | sem chamador no NWindow (o controlador do Engine) | `MoveBackwardToLocation` lê 6 `D` e um 7º se sobrar byte: igual | alta (0x01), média (0x5C) |
| `MoveBackwardToLocation(FVector, FVector)` (53 / +0xD4) | 0x01 | `cdddddd` | alvo x y z, origem x y z (6 dwords, sem moveMovement) | sem chamador no NWindow | idem | alta |
| `ValidatePosition(FVector, INT, INT)` (68 / +0x110) | 0x48 | `cddddd` | x y z, heading (arg2), id do veículo (arg3) | sem chamador no NWindow | `ValidatePosition` lê 5 `D`: igual | alta |
| `Say2(L2ParamStack&)` (155 / +0x26C) | 0x38 | `cSd` / `cSdS` | `S` texto (P1), `d` tipo (P0) e, no tipo 2 (sussurro), `S` alvo (P2); texto vazio não manda nada | `NConsoleWnd::SendChatMsgToServer` 0x1005a890: `[tipo, texto, alvo]` | `Say2C` lê `S, D, (S se TELL)`: igual | alta |
| `RequestItemList` (83 / +0x14C) | 0x0F | `c` | nenhum | `NConsoleWnd::RequestItemList` (inline) | `RequestItemList`: igual | alta |
| `RequestUseItem(L2ParamStack&)` (92 / +0x170) | 0x14 | `cdd` | `d` objectId (P0), `d` ctrl (P1) | `NConsoleWnd::RequestUseItem` | `UseItem` lê `D D`: igual | alta |
| `RequestSkillItem` (163 / +0x28C), a `RequestSkillList` da spec | 0x3F | `c` | nenhum | `NCMagicSkillWnd::ShowWindow` (stub `RequestSkillListC3`) | `RequestSkillList`: igual | alta |
| `RequestMagicSkillUse` (154 / +0x268) | 0x2F | `cddc` | `d` skillId (P0), `d` ctrl (P1), `c` shift (P2) | `NConsoleWnd::UseShortCut`, `NMacroEntry::ExecuteLine`, `NCSkillItemWnd::OnLButtonUp`, `ProcessChatLine` | `RequestMagicSkillUse` lê `D D C`: igual | alta |
| `RequestPrivateStoreManage` (222 / +0x378) | 0x73 | `c` | nenhum | `NConsoleWnd::UseAction` (ação 10), `ProcessChatLine` | `RequestPrivateStoreSell` (abre `PrivateStoreManageListSell`): igual | alta |
| `SetPrivateStoreList` (223 / +0x37C) | 0x74 | `cdb` | `d` quantidade (P0) + bloco de N x `ddd` (objectId, quantidade, preço) | `NCPrivateShopWnd::OnStart` | `SetPrivateStoreSellList` lê `D` + N x `DDD`: igual (o `packageSale` do Interlude NÃO existe no C3) | alta |
| `RequestPrivateStoreManageCancel` (224 / +0x380) | 0x75 | `c` | nenhum | `NCPrivateShopWnd::OnClose` | sem handler no início; hoje `RequestPrivateStoreManageCancel` | alta |
| `RequestPrivateStoreQuit` (225 / +0x384) | 0x76 | `c` | nenhum | `NCConsole::ToggleInterface`, `NCPrivateShopWnd::OnAbort`, `NCPrivateShopFrameCtrl::OnClose` | `RequestPrivateStoreQuitSell` | alta |
| `SetPrivateStoreMsg` (226 / +0x388) | 0x77 | `cS` | `S` mensagem | `NCGaraDialogBox::OnOK` | `SetPrivateStoreMsgSell` lê `S` | alta |
| `SendPrivateStoreBuyList` (228 / +0x390) | 0x79 | `cddb` | `d` dono da loja (P0), `d` quantidade (P1) + N x `ddd` (objectId, quantidade, preço) | `NCPrivateShopWnd::OnStart`, `::OnBuyConfirmed` | classe `RequestPrivateStoreManageBuy` (nome trocado no fonte Lucera; `runImpl` é a compra) lê `D D` + N x `DDD`: igual | alta |
| `RequestPrivateStoreBuyManage` (229 / +0x394) | 0x90 | `c` | nenhum | `UseAction` (ação 28), `ProcessChatLine` | classe `RequestPrivateStoreBuy` (nome trocado; `runImpl` abre `PrivateStoreManageListBuy`) | alta |
| `SetPrivateStoreBuyList` (230 / +0x398) | 0x91 | `cdb` | `d` quantidade + N x `dhdd` (itemId, enchant `h`, quantidade, preço) = 14 bytes | `NCPrivateBuyWnd::OnStart` | `SetPrivateStoreBuyList` (já ajustado para 14 bytes) | alta |
| `RequestPrivateStoreBuyManageCancel` / `Quit` / `SetPrivateStoreBuyMsg` (231..233) | 0x92 / 0x93 / 0x94 | `c` / `c` / `cS` | nenhum / nenhum / mensagem | `NCPrivateBuyWnd::OnClose` / `OnAbort`... / `NCGaraDialogBox::OnOK` | 0x92 sem handler no início; 0x93, 0x94 ok | alta |
| `SendPrivateStoreBuyBuyList` (235 / +0x3AC) | 0x96 | `cddb` | `d` dono (P0), `d` quantidade (P1) + N x `ddhdd` (objectId, itemId, enchant `h`, quantidade, preço) = 18 bytes | `NCPrivateBuyWnd::OnSellConfirmed`, `::OnStart` | classe `RequestPrivateStoreBuySellList` (já ajustada para 18 bytes) | alta |
| `RequestPrivateStoreList` / `RequestPrivateStoreBuyList` (227 / 234) | 0x78 / 0x95 | `cd` | `d` dono da loja | sem chamador no NWindow | sem handler (não usado) | alta |

As lojas: a janela de venda não manda `packageSale`; o `SetPrivateStoreList` do C3 é `count + N x (objectId, count, price)`. O que derruba o cliente
na loja (`PrivateStoreManageList` s2c) é a outra direção (pistas P/T1); o c2s acima já bate com o servidor.

## 5. Discordâncias de OPCODE cliente x servidor (fatais: o servidor executa o pacote errado, ou nenhum)

Estado no INÍCIO da auditoria (`GamePacketHandler.java` de `java_backup_pre_audit`, conferido com a ferramenta `--handler`) e hoje:

| opcode | cliente C3 (Engine, spec) | servidor no início | servidor hoje | efeito |
|---|---|---|---|---|
| `0xB4` | `RequestRecipeShopManageCancel`, `c` sem campos (`NCRecipeShopWnd::OnCancel`) | `SnoopQuit` (180; `readD`) | `RequestRecipeShopManageCancel` | `SnoopQuit` lia um dword que não vinha: BufferUnderflow, conta para `FailPacketAmountLimit` |
| `0xC3` | `RequestProcureCrop`, `cddb` (manor) | `RequestHennaItemInfo` (195) | `RequestProcureCrop` | rodava o pedido de henna no lugar da colheita |
| `0xCF` + word 1..5 | `RequestOustFromPartyRoom`, `RequestDismissPartyRoom`, `RequestWithdrawPartyRoom`, `RequestHandOverPartyMaster`, `RequestAutoSoulShot` (formatos `chd`, `chdd`, `chdd`, `chS`, `chdd`) | `0xCF` = `RequestReload` (`sendUserInfo` + `showObjectsToPlayer` a cada pedido); o Interlude usa `0xD0 + word` para estes | `case 207` com sub-switch `(0xCF, word)` | auto soulshot (botão direito no atalho) e os comandos da sala de grupo não funcionavam |
| `0xCA` / `0xCB` no estado CONNECTED | `ReplyGameGuardQuery` é `0xCA` (`cdddd`) | CONNECTED só conhecia `203` (0xCB) | `case 202, 203` | resposta do GameGuard antes do login |

Fora essa tabela os nomes batem (as trocas de nome `Sell` / `Buy` do `0x79` / `0x90` nas lojas são só de nome: a `runImpl` de cada
classe faz o que o opcode do C3 significa; conferido no código das classes `RequestPrivateStoreSell`, `RequestPrivateStoreBuy`,
`RequestPrivateStoreManageBuy`). Hoje a ferramenta não lista nenhuma discordância de opcode.

### 5.1 Pedidos do cliente SEM handler no servidor

Os que o NWindow chama de verdade (cada pacote desconhecido incrementa o contador de 10; `UnknownPacketAmountLimit`):

| opcode | pedido | chamado por | situação |
|---|---|---|---|
| `0x23` | `RequestCreatePledge` (`cS`, nome do clã) | `NCGaraDialogBox::OnOK` | sem handler no início (a classe `RequestCreatePledge` existe, mas nenhum `case` a instanciava); hoje mapeado |
| `0x28` | `RequestDismissPledge` (`c`) | `NCGaraDialogBox::OnOK` | **ainda sem handler** (não existe classe `RequestDismissPledge` no `c2s`) |
| `0x4E`, `0x50` | `RequestReplyStartPledgeWar`, `RequestReplyStopPledgeWar` (`cSd`) | `NCGaraDialogBox::OnOK/OnCancel`, `NConsoleWnd::ReceiveStartPledgeWar` / `ReceiveStopPledgeWar` | **ainda sem handler** |
| `0x51`, `0x69` | `RequestSurrenderPledgeWar`, `RequestSurrenderPersonally` (`cS`) | `NCGaraDialogBox::OnOK` | **ainda sem handler** |
| `0x75`, `0x92` | `RequestPrivateStoreManageCancel`, `RequestPrivateStoreBuyManageCancel` (`c`) | `NCPrivateShopWnd::OnClose`, `NCPrivateBuyWnd::OnClose` | sem handler no início (fechar a janela de gerenciar sem abrir a loja deixava o estado de loja); hoje mapeados |
| `0x7A` | `ReviveReply` (`cd`) | `NConsoleWnd::AnswerReviveRequest` | sem handler no início; hoje mapeado |
| `0xA9`, `0xB0`, `0xCE` | `RequestRemainTime` (`c`), `RequestRecipeShopManageList` (`c`), `RequestSendMsnChatLog` (`cSSd`) | `ProcessChatLine`, `UseAction` (ação 37), `ChatWnd::*` | sem handler no início; hoje mapeados |

Sem chamador no NWindow (código morto ou do Engine): `0x02 Say`, `0x10 RequestEquipItem`, `0x1A RequestTeleport`, `0x2D RequestDismissParty`,
`0x2E RequestMagicSkillList`, `0x34 RequestShortCutUse`, `0x39:xx` (`SendCharacterInfo`, `SendSummonCmd`, `SendServerStatus`,
`SendL2ParamSetting`), `0x3E RequestMagicItem`, `0x49 RequestSEKCustom`, `0x52`, `0x78`, `0x95`, `0x98..0x9C` (guerra de aliança), `0xA1`.

### 5.2 O servidor lia mais do que o cliente manda (BufferUnderflowException => pacote perdido)

Início da auditoria (cada linha agora tem o ajuste feito por outra pista; confira `java_backup_pre_audit`):

| opcode | pedido | cliente manda | o servidor lia |
|---|---|---|---|
| `0x24` | `RequestJoinPledge` | `d` (objectId do alvo) | `D D` (com o tipo de pledge do Interlude): o convite de clã falhava |
| `0x6B`, `0x6C` | `RequestAcquireSkillInfo`, `RequestAcquireSkill` | `d skillId, d level` | `D D D` (com o tipo): aprender skill no mestre falhava |
| `0xC5` | `ConfirmDlg` | `d, d` | `D D D` |
| `0x71` | `RequestJoinPartyRoom` | `d` (id da sala) | `D D D` |
| `0xA7` | `RequestMultiSellChoose` | `d listId, d entryId` (C3 não tem quantidade) | `D D D` |
| `0xC3` | `RequestProcureCrop` | `cddb`, itens `ddd` (12 bytes) | itens de 16 bytes (`var_91 * 16`): a colheita do manor nunca passava no teste de tamanho |
| `0x37` | `RequestTargetCancel` | `c` | `H` |
| `0x74`, `0x91`, `0x96` | listas das lojas privadas | `cdb` (sem `packageSale`), itens de 14 / 18 bytes | com `packageSale` e itens de 16 / 20 bytes |
| `0x04` | `RequestAttack` (slot 167) | `cdddd` (sem o byte final) | `Action` lê `D D D D C` | sem chamador no NWindow: só vale se o Engine usar |

Tamanhos de item conferidos (cliente x teste `n * tam <= remaining()` do servidor, hoje): `RequestSellItem` 12 (`ddd`), `RequestBuyItem` 8 (`dd`), `RequestBuySeed` 8,
`RequestPackageSend` 8, `RequestRecipeShopListSet` 8, `SendWareHouse*List` 8, `RequestProcureCrop` 12, `RequestPreviewItem` 4, lojas 12 / 14 / 18: todos iguais.

Confirmado a favor do servidor (leituras condicionais que toleram o C3): `MoveBackwardToLocation` (6 `D`, o 7º só se sobrar),
`Say2C` (alvo só no sussurro), `RequestBlock` (nome só nos tipos 0 e 1), `RequestPledgePower` (3º `D` só na ação 2; o C3 manda um bloco de 0x20 bytes
em vez de um dword, caso de clã raro).

## 6. Todos os pedidos: o que o `SendX` do Engine escreve

Cada linha: opcode | nome do vtable do C3 | formato (e formato de cada item das listas) | chamadores no NWindow stock (com o que empilham, entre
colchetes; operandos são os do disassembly) | classe do servidor e o que `readImpl` lê | certeza (alta = campos todos rastreados até argumento / constante /
`Top()`; média = há campo com lixo ou rastreio incompleto). Gerada por `python tools/c3/c2s_formats.py --json ... --md ...`.

| opcode | nome | formato | NWindow | servidor | certeza |
|---|---|---|---|---|---|
| 00 | (connect) ProtocolVersion | `cdb` | sem chamador no NWindow | ProtocolVersion lê `D` | high |
| 01 | MoveToLocation | `cddddddd` | sem chamador no NWindow | MoveBackwardToLocation lê `DDDDDDD` (condicional) | high |
| 01 | MoveBackwardToLocation | `cdddddd` | sem chamador no NWindow | MoveBackwardToLocation lê `DDDDDDD` (condicional) | high |
| 02 | Say | `cS` | sem chamador no NWindow | **sem handler** | high |
| 03 | EnterWorld | `cS` | sem chamador no NWindow | EnterWorld lê `` | high |
| 03 | RequestEnterWorldPacket | `cddddd` | sem chamador no NWindow | EnterWorld lê `` | high |
| 04 | Action | `cddddc` | NConsoleWnd::RequestAction [EDX <- dword ptr [ESP + 0x1c], EDX <- AL]; NConsoleWnd::RequestAttack [EAX <- dword ptr [ESP + 0x1c], EAX <- AL] | Action lê `DDDDC` | high |
| 04 | RequestAttack | `cdddd` | sem chamador no NWindow | Action lê `DDDDC` | high |
| 08 | (connect) AuthLogin | `cSddddd` | sem chamador no NWindow | AuthLogin lê `SDD` | high |
| 09 | SendLogOutPacket | `c` | NConsoleWnd::ReceiveVersionCheck []; NConsoleWnd::ReturnToLogin [] | Logout/Logout lê `` | high |
| 0A | Attack | `cddddc` | NConsoleWnd::RequestAttackForce [EAX <- dword ptr [ESP + 0x18], 0x0, ESI]; NConsoleWnd::RequestAttack [EAX <- dword ptr [ESP + 0x1c], EAX <- dword ptr [ESP + 0x20]] | AttackRequest lê `DDDDC` | high |
| 0B | RequestCharacterCreate | `cSdddddddddddd` | NConsoleWnd::RequestCharacterCreate [EDI] | CharacterCreate lê `SDDDDDDDDDDDD` | high |
| 0C | RequestCharacterDelete | `cd` | NConsoleWnd::RequestCharacterDelete [EAX <- dword ptr [EBP + 0x8]] | CharacterDelete lê `D` | high |
| 0D | RequestGameStart | `cd` | NConsoleWnd::RequestGameStart [EAX <- dword ptr [EBP + 0x8]] | CharacterSelected lê `D` | high |
| 0E | RequestNewCharacter | `c` | NConsoleWnd::RequestNewCharacter [] | NewCharacter lê `` | high |
| 0F | RequestItemList | `c` | NConsoleWnd::RequestItemList [] | RequestItemList lê `` | high |
| 10 | RequestEquipItem | `cdd` | sem chamador no NWindow | **sem handler** | high |
| 11 | RequestUnEquipItem | `cd` | NCEquipItemWnd::OnLButtonUp [EAX <- dword ptr [EAX + 0x1928], 0x10, 0x20, 0x2, 0x4]; NCEquipItemWnd::OnRButtonDown [EAX <- dword ptr [ESI + 0x1928]]; NCEquipItemWnd::OnRButtonDown [EDX <- dword ptr [ESI + 0x1928]] (+6) | RequestUnEquipItem lê `D` | high |
| 12 | RequestDropItem | `cddddd` | NConsoleWnd::RequestDropItem [] | RequestDropItem lê `DDDDD` | high |
| 12 | RequestDropItemFromPet | `cddddd` | sem chamador no NWindow | RequestDropItem lê `DDDDD` | high |
| 14 | RequestUseItem | `cdd` | NConsoleWnd::RequestUseItem [EAX <- dword ptr [EBP + 0x8], EAX] | UseItem lê `DD` | high |
| 15 | RequestTrade | `cd` | NConsoleWnd::RequestTrade [EAX <- dword ptr [EBP + 0x8]] | TradeRequest lê `D` | high |
| 16 | RequestAddTradeItem | `cddd` | NConsoleWnd::RequestAddTradeItem [EDI, ESI, EBX] | AddTradeItem lê `DDD` | high |
| 17 | RequestTradeDone | `cd` | NConsoleWnd::RequestTradeDone [EAX <- dword ptr [EBP + 0x8]] | TradeDone lê `D` | high |
| 1A | RequestTeleport | `cdddd` | sem chamador no NWindow | **sem handler** | high |
| 1A | RequestTeleport | `cddddd` | sem chamador no NWindow | **sem handler** | high |
| 1B | SocialAction | `cd` | NCChatWnd::ProcessChatLine [0x2]; NCChatWnd::ProcessChatLine [0x3]; NCChatWnd::ProcessChatLine [0x4] (+10) | RequestSocialAction lê `D` | high |
| 1C | ChangeMoveType | `cd` | NCChatWnd::ProcessChatLine [0x0]; NCChatWnd::ProcessChatLine [0x1] | RequestChangeMoveType lê `D` | high |
| 1D | ChangeWaitType | `cd` | NCChatWnd::ProcessChatLine [0x0]; NCChatWnd::ProcessChatLine [0x1] | RequestChangeWaitType lê `D` | high |
| 1E | RequestSellItem | `cddb  itens: ddd` | NConsoleWnd::RequestSellItem | RequestSellItem lê `DDDDD` (condicional) | high |
| 1F | RequestBuyItem | `cddb  itens: dd` | NConsoleWnd::RequestBuyItem | RequestBuyItem lê `DDDD` (condicional) | high |
| 20 | RequestLinkHtml | `cS` | NCConsole::OnHtmlCommand [ECX] | RequestLinkHtml lê `S` | high |
| 21 | RequestBypassToServer | `cS` | NConsoleWnd::RunHtmlCommand [EDX] | RequestBypassToServerInLobby/RequestBypassToServer lê `S` | high |
| 22 | RequestBBSWrite | `cSSSSSS` | NCConsole::OnHtmlCommand [EDX, EDX, EDX, EAX, EAX, EAX] | RequestBBSwrite lê `SSSSSS` | high |
| 23 | RequestCreatePledge | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestCreatePledge lê `S` | high |
| 24 | RequestJoinPledge | `cd` | NCPledgeWnd::OnInvite [ECX <- dword ptr [EAX + 0x54]] | RequestJoinPledge lê `DD` (condicional) | high |
| 25 | RequestAnswerJoinPledge | `cd` | NCGaraDialogBox::OnCancel [0x0]; NCGaraDialogBox::OnOK [0x1]; NConsoleWnd::ReceiveAskJoinPledge [EBP] | RequestAnswerJoinPledge lê `D` (condicional) | high |
| 26 | RequestWithDrawalPledge | `c` | NCGaraDialogBox::OnOK [] | RequestWithdrawalPledge lê `` | high |
| 27 | RequestOustPledgeMember | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestOustPledgeMember lê `S` | high |
| 28 | RequestDismissPledge | `c` | NCGaraDialogBox::OnOK [] | **sem handler** | high |
| 29 | RequestJoinParty | `cSd` | NCChatWnd::ProcessChatLine [ESI, EAX <- dword ptr [EDX + 0x39c8]]; NCChatWnd::ProcessChatLine [ESI, EAX <- dword ptr [ECX + 0x39c8]]; NConsoleWnd::UseAction [EDI, EAX <- dword ptr [ESI + 0x39c8]] (+1) | RequestJoinParty lê `SD` | high |
| 2A | RequestAnswerJoinParty | `cd` | NCGaraDialogBox::OnCancel [0x0]; NCGaraDialogBox::OnOK [0x1]; NConsoleWnd::ReceiveAskJoinParty [ESI] | RequestAnswerJoinParty lê `D` (condicional) | high |
| 2B | RequestWithDrawalParty | `c` | NCChatWnd::ProcessChatLine []; NConsoleWnd::UseAction [] | RequestWithDrawalParty lê `` | high |
| 2C | RequestOustPartyMember | `cS` | NCChatWnd::ProcessChatLine [ECX]; NConsoleWnd::UseAction [EAX] | RequestOustPartyMember lê `S` | high |
| 2D | RequestDismissParty | `c` | sem chamador no NWindow | **sem handler** | high |
| 2E | RequestMagicSkillList | `ddd` | sem chamador no NWindow | **sem handler** | high |
| 2F | RequestMagicSkillUse | `cddc` | NCChatWnd::ProcessChatLine [ECX <- dword ptr [ESI], 0x0, 0x0]; NCChatWnd::ProcessChatLine [EAX <- dword ptr [ESI], 0x1, 0x0]; NCChatWnd::ProcessChatLine [EDX <- dword ptr [ESI], 0x0, 0x1] (+3) | RequestMagicSkillUse lê `DDC` | high |
| 30 | SendAppearingPacket | `c` | sem chamador no NWindow | Appearing lê `` | high |
| 31 | SendWareHouseDepositList | `cdb  itens: dd` | NCStoreWnd::OnOK [EDI, ECX <- dword ptr [EAX + 0x1938], EBX] | SendWareHouseDepositList lê `DDD` (condicional) | high |
| 32 | SendWareHouseWithdrawList | `cdb  itens: dd` | NCStoreWnd::OnOK [EDI, EAX <- dword ptr [EAX + 0x1938], ECX <- dword ptr [EAX + 0x194c]] | SendWareHouseWithDrawList lê `DDD` (condicional) | high |
| 33 | RequestShortCutReg | `cdddd` | NConsoleWnd::RequestShortCutReg | RequestShortCutReg lê `DDDD` | high |
| 34 | RequestShortCutUse | `cddc` | sem chamador no NWindow | **sem handler** | high |
| 35 | RequestShortCutDel | `cd` | NCShortCutWnd::UpdateMacro; NCShortCutWnd::OnLButtonUp [EAX <- dword ptr [ESI + 0x254]] | RequestShortCutDel lê `D` | high |
| 36 | CanNotMoveAnymore | `cdddd` | sem chamador no NWindow | CannotMoveAnymore lê `DDDD` | high |
| 37 | RequestTargetCancel | `c` | NConsoleWnd::ShowRestartMenu []; NConsoleWnd::HookShortCutKey; NCTargetStatusWnd::OnClose [] | RequestTargetCanceld lê `H` (condicional) | high |
| 38 | Say2 | `cSd` | NConsoleWnd::SendChatMsgToServer [EDX <- dword ptr [EBP + 0x8], EAX <- dword ptr [EBP + 0xc], ECX <- dword ptr [EBP + 0x10]] | Say2C lê `SDS` (condicional) | high |
| 38 | Say2 | `cSdS` | NConsoleWnd::SendChatMsgToServer [EDX <- dword ptr [EBP + 0x8], EAX <- dword ptr [EBP + 0xc], ECX <- dword ptr [EBP + 0x10]] | Say2C lê `SDS` (condicional) | medium |
| 39:00 | SendCharacterInfo | `ccS` | sem chamador no NWindow | **sem handler** | high |
| 39:01 | SendSummonCmd | `ccS` | sem chamador no NWindow | **sem handler** | high |
| 39:02 | SendServerStatus | `cc` | sem chamador no NWindow | **sem handler** | high |
| 39:03 | SendL2ParamSetting | `ccdd` | sem chamador no NWindow | **sem handler** | high |
| 3C | RequestPledgeMemberList | `c` | sem chamador no NWindow | RequestPledgeMemberList lê `` | high |
| 3E | RequestMagicItem | `c` | sem chamador no NWindow | **sem handler** | high |
| 3F | RequestSkillItem | `c` | NCMagicSkillWnd::ShowWindow [] | RequestSkillList lê `` | high |
| 41 | MoveWithDelta | `cddd` | sem chamador no NWindow | MoveWithDelta lê `DDD` | high |
| 42 | GetOnVehicle | `cdddd` | sem chamador no NWindow | RequestGetOnVehicle lê `DDDD` | high |
| 43 | GetOffVehicle | `cdddd` | sem chamador no NWindow | RequestGetOffVehicle lê `DDDD` | high |
| 44 | AnswerTradeRequest | `cd` | sem chamador no NWindow | AnswerTradeRequest lê `D` | high |
| 45 | RequestActionUse | `cddc` | NCGaraDialogBox::OnOK [0x14, EAX, EDX <- AL]; NConsoleWnd::UseAction [EDI, EBP, EBX] | RequestActionUse lê `DDC` | high |
| 46 | RequestRestart | `c` | NCSystemMenuWnd::OnDialogResult [] | RequestRestart lê `` | high |
| 47 | RequestSiegeInfo | `c` | sem chamador no NWindow | RequestSiegeInfo lê `` | high |
| 48 | ValidatePosition | `cddddd` | sem chamador no NWindow | ValidatePosition lê `DDDDD` | high |
| 49 | RequestSEKCustom | `cdd` | sem chamador no NWindow | **sem handler** | high |
| 4A | StartRotating | `cdd` | sem chamador no NWindow | StartRotatingC lê `DD` | high |
| 4B | FinishRotating | `cdd` | sem chamador no NWindow | FinishRotatingC lê `DD` | medium |
| 4D | RequestStartPledgeWar | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestStartPledgeWar lê `S` | high |
| 4E | RequestReplyStartPledgeWar | `cSd` | NCGaraDialogBox::OnCancel [EAX, 0x0]; NCGaraDialogBox::OnOK [EAX, 0x1]; NConsoleWnd::ReceiveStartPledgeWar [ESI, EBP] | **sem handler** | high |
| 4F | RequestStopPledgeWar | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestStopPledgeWar lê `S` | high |
| 50 | RequestReplyStopPledgeWar | `cSd` | NCGaraDialogBox::OnCancel [0x0]; NCGaraDialogBox::OnOK [EAX, 0x1]; NConsoleWnd::ReceiveStopPledgeWar [ESI, EBP] | **sem handler** | high |
| 51 | RequestSurrenderPledgeWar | `cS` | NCGaraDialogBox::OnOK [EAX] | **sem handler** | high |
| 52 | RequestReplySurrenderPledgeWar | `cSd` | sem chamador no NWindow | **sem handler** | high |
| 53 | RequestSetPledgeCrest | `cd` | NConsoleLeftFns::RemovePledgeCrest [0x0, 0x0]; NConsoleWnd::RegisterPledgeCrest [EDX <- dword ptr [ESP + 0x10], EBX] | RequestSetPledgeCrest lê `DB` (condicional) | high |
| 53 | RequestSetPledgeCrest | `cdb` | NConsoleLeftFns::RemovePledgeCrest [0x0, 0x0]; NConsoleWnd::RegisterPledgeCrest [EDX <- dword ptr [ESP + 0x10], EBX] | RequestSetPledgeCrest lê `DB` (condicional) | high |
| 55 | RequestGiveNickName | `cSS` | NCGaraDialogBox::OnOK [EAX, EAX]; NCGaraDialogBox::OnOK [EAX, ECX] | RequestGiveNickName lê `SS` | high |
| 57 | RequestShowboard | `cd` | NCConsole::ToggleBoard [] | RequestShowBoard lê `D` | high |
| 58 | RequestEnchantItem | `cd` | NConsoleWnd::RequestEnchantItem [EAX <- dword ptr [EBP + 0x8]] | RequestEnchantItem lê `D` | high |
| 59 | RequestDestroyItem | `cdd` | NConsoleWnd::RequestDestroyItem [EDI, ESI, EBX] | RequestDestroyItem lê `DD` | high |
| 59 | SendDestroyItem | `cdd` | sem chamador no NWindow | RequestDestroyItem lê `DD` | high |
| 5B | SendBypassBuildCmd | `cS` | sem chamador no NWindow | SendBypassBuildCmd lê `S` (condicional) | high |
| 5C | MoveToLocation | `cddddddd` | sem chamador no NWindow | RequestMoveToLocationInVehicle lê `DDDDDDD` | medium |
| 5D | CanNotMoveAnymore | `cddddd` | sem chamador no NWindow | CannotMoveAnymoreInVehicle lê `DDDDD` | medium |
| 5E | RequestFriendInvite | `cS` | NCChatWnd::ProcessChatLine [EDX] | RequestFriendInvite lê `S` | high |
| 5F | RequestFriendAddReply | `cd` | NCGaraDialogBox::OnCancel [0x0]; NCGaraDialogBox::OnOK [0x1] | RequestFriendAddReply lê `D` (condicional) | high |
| 60 | RequestFriendList | `c` | NCChatWnd::ProcessChatLine [] | RequestFriendList lê `` | high |
| 61 | RequestFriendDel | `cS` | NCChatWnd::ProcessChatLine [EAX] | RequestFriendDel lê `S` | high |
| 62 | RequestCharacterRestore | `cd` | NConsoleWnd::RequestCharacterRestore [EAX <- dword ptr [EBP + 0x8]] | CharacterRestore lê `D` | high |
| 63 | RequestQuestList | `c` | NConsoleWnd::RequestQuestList [] | RequestQuestList lê `` | high |
| 64 | RequestDestroyQuest | `cd` | NConsoleWnd::RequestDestroyQuest [EAX <- dword ptr [EBP + 0x8]] | RequestQuestAbort lê `D` | high |
| 66 | RequestPledgeInfo | `cd` | sem chamador no NWindow | RequestPledgeInfo lê `D` | high |
| 67 | RequestPledgeExtendedInfo | `cS` | sem chamador no NWindow | RequestPledgeExtendedInfo lê `S` | high |
| 68 | RequestPledgeCrest | `cd` | sem chamador no NWindow | RequestPledgeCrest lê `D` | high |
| 69 | RequestSurrenderPersonally | `cS` | NCGaraDialogBox::OnOK [EAX] | **sem handler** | high |
| 6B | RequestAcquireSkillInfo | `cdd` | sem chamador no NWindow | RequestAquireSkillInfo lê `DDD` (condicional) | high |
| 6C | RequestAcquireSkill | `cdd` | sem chamador no NWindow | RequestAquireSkill lê `DDD` (condicional) | high |
| 6D | RequestRestartPoint | `cd` | NCRestartMenuWnd::OnVillage [0x0]; NCRestartMenuWnd::OnHideaway [0x1]; NCRestartMenuWnd::OnCastle [0x2] (+2) | RequestRestartPoint lê `D` | high |
| 6E | RequestGMCommand | `cSdd` | NConsoleWnd::RequestGMInventory [EAX <- dword ptr [ESP + 0x4], 0x5]; NConsoleWnd::RequestGMSkills [EAX <- dword ptr [ESP + 0x4], 0x3]; NConsoleWnd::RequestGMStatus [EAX <- dword ptr [ESP + 0x4], 0x1] (+3) | RequestGMCommand lê `SD` | high |
| 6F | RequestListPartyWating | `cddd` | NCChatWnd::ProcessChatLine [0x1, -0x1, 0x1]; NConsoleWnd::UseAction [0x1, -0x1, 0x1]; NCMatchWnd::OnPrevPage [EDI, EAX, EAX <- dword ptr [EAX + 0x220]] (+2) | RequestPartyMatchConfig lê `DDD` | high |
| 70 | RequestManagePartyRoom | `cdddddS` | NCPartyRoomMakingWnd::OnOkBtnClick [EAX <- dword ptr [EBX + 0xe4], EAX, ESI, EDI, EAX <- dword ptr [ECX + 0x39c8], EAX] | RequestPartyMatchList lê `DDDDDS` | high |
| 71 | RequestJoinPartyRoom | `cd` | NCMatchWnd::OnJoin [0x0]; NCMatchListCtrl::OnLButtonDblClick [EAX] | RequestPartyMatchDetail lê `DDD` (condicional) | high |
| 72 | RequestCrystallizeItem | `cdd` | sem chamador no NWindow | RequestCrystallizeItem lê `DD` | high |
| 73 | RequestPrivateStoreManage | `c` | NCChatWnd::ProcessChatLine []; NConsoleWnd::UseAction [] | RequestPrivateStoreSell lê `` | high |
| 74 | SetPrivateStoreList | `cdb  itens: ddd` | NCPrivateShopWnd::OnStart [EDI, EDX <- dword ptr [EAX + 0x1914], EAX <- dword ptr [EDX + 0x194c], ECX <- dword ptr [EAX + 0x1924]] | SetPrivateStoreSellList lê `DDDD` (condicional) | high |
| 75 | RequestPrivateStoreManageCancel | `c` | NCPrivateShopWnd::OnClose [] | RequestPrivateStoreManageCancel lê `` | high |
| 76 | RequestPrivateStoreQuit | `c` | NCConsole::ToggleInterface []; NCPrivateShopWnd::OnAbort []; NCPrivateShopFrameCtrl::OnClose [] | RequestPrivateStoreQuitSell lê `` | high |
| 77 | SetPrivateStoreMsg | `cS` | NCGaraDialogBox::OnOK [EAX] | SetPrivateStoreMsgSell lê `S` | high |
| 78 | RequestPrivateStoreList | `cd` | sem chamador no NWindow | **sem handler** | high |
| 79 | SendPrivateStoreBuyList | `cddb  itens: ddd` | NCPrivateShopWnd::OnStart [EDX <- dword ptr [ESI + 0xf8], EDI, ECX <- dword ptr [ECX + 0x1914], EDX <- dword ptr [ECX + 0x194c], EAX <- dword ptr [EDX + 0x1924]]; NCPrivateShopWnd::OnBuyConfirmed [ECX <- dword ptr [EDI + 0xf8], ESI, EAX <- dword ptr [EAX + 0x1914], ECX <- dword ptr [EAX + 0x194c], EDX <- dword ptr [ECX + 0x1924]] | RequestPrivateStoreManageBuy lê `DDDDD` (condicional) | high |
| 7A | ReviveReply | `cd` | NConsoleWnd::AnswerReviveRequest [ESI, EAX <- dword ptr [ESP + 0x2c], ECX <- dword ptr [ESP + 0x30]] | RequestReviveReply lê `D` | high |
| 7B | RequestTutorialLinkHtml | `cS` | NCConsole::OnHtmlCommand [ECX] | RequestTutorialLinkHtml lê `S` | high |
| 7C | RequestTutorialPassCmdToServer | `cS` | NConsoleWnd::RunHtmlCommand [EDX] | RequestTutorialPassCmdToServer lê `S` | high |
| 7D | RequestTutorialQuestionMarkPressed | `cd` | NCTutorialQuestionWnd::OnLButtonUp [ECX <- dword ptr [ESI + 0xd0]] | RequestTutorialQuestionMark lê `D` | high |
| 7E | RequestTutorialClientEvent | `cd` | NConsoleWnd::SendTutorialEvent [EDI <- dword ptr [ESP + 0xc]] | RequestTutorialClientEvent lê `D` | high |
| 7F | RequestPetition | `cSd` | NCPetitionBeginWnd::OnSend [EDI, EBX] | RequestPetition lê `SD` | high |
| 80 | RequestPetitionCancel | `cS` | NCChatWnd::ProcessChatLine []; NCPetitionWnd::OnCancel []; NCPetitionBeginWnd::OnCancel [] | RequestPetitionCancel lê `` | medium |
| 81 | RequestGMList | `c` | NCChatWnd::ProcessChatLine [] | RequestGmList lê `` | high |
| 82 | RequestJoinAlly | `cd` | NCChatWnd::ProcessChatLine [ECX <- dword ptr [EAX + 0x54]] | RequestJoinAlly lê `D` | high |
| 83 | RequestAnswerJoinAlly | `cd` | NCGaraDialogBox::OnCancel [0x0]; NCGaraDialogBox::OnOK [0x1]; NConsoleWnd::ReceiveAskJoinAlly [EDI] | RequestAnswerJoinAlly lê `D` (condicional) | high |
| 84 | RequestWithdrawAlly | `c` | NCGaraDialogBox::OnOK [] | RequestWithdrawAlly lê `` | high |
| 85 | RequestOustAlly | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestOustAlly lê `S` | high |
| 86 | RequestDismissAlly | `c` | NCGaraDialogBox::OnOK [] | RequestDismissAlly lê `` | high |
| 87 | RequestSetAllyCrest | `cdb` | NConsoleWnd::RegisterAllyCrest [EDX <- dword ptr [ESP + 0x10], EBX] | RequestSetAllyCrest lê `DB` (condicional) | high |
| 88 | RequestAllyCrest | `cd` | sem chamador no NWindow | RequestAllyCrest lê `D` | high |
| 89 | RequestChangePetName | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestChangePetName lê `S` | high |
| 8A | RequestPetUseItem | `cd` | NConsoleWnd::UseShortCut [ECX <- dword ptr [ESI + 0x1914]]; NCPetInvenItemWnd::OnLButtonDblClick [EAX <- dword ptr [EDX + 0x1914]]; NCPetInvenItemWnd::OnRButtonDown [EAX <- dword ptr [EDX + 0x1914]] | RequestPetUseItem lê `D` | high |
| 8B | RequestGiveItemToPet | `cdd` | NCGaraDialogBox::OnOK [EAX <- dword ptr [EBP + 0x258], ESI]; NCEquipItemWnd::OnLButtonUp [EAX <- dword ptr [EDX + 0x1914], EBP]; NCInvenItemWnd::OnLButtonUp [ECX <- dword ptr [EAX + 0x1914], EBX] | RequestGiveItemToPet lê `DD` | high |
| 8C | RequestGetItemFromPet | `cddd` | NCGaraDialogBox::OnOK [EDX <- dword ptr [EBP + 0x258], ESI, EBX]; NCPetInvenItemWnd::OnLButtonUp [EDX <- dword ptr [ECX + 0x1914], EBP, 0x0] | RequestGetItemFromPet lê `DDD` | high |
| 8E | RequestAllyInfo | `c` | NCChatWnd::ProcessChatLine [] | RequestAllyInfo lê `` | high |
| 8F | RequestPetGetItem | `cd` | NConsoleWnd::PetPickUp [EAX <- dword ptr [EAX]] | RequestPetGetItem lê `D` | high |
| 90 | RequestPrivateStoreBuyManage | `c` | NCChatWnd::ProcessChatLine []; NConsoleWnd::UseAction [] | RequestPrivateStoreBuy lê `` | high |
| 91 | SetPrivateStoreBuyList | `cdb  itens: dhdd` | NCPrivateBuyWnd::OnStart [EBX, EDX <- dword ptr [EAX + 0x1918], EAX <- dword ptr [EDX + 0x193c], ECX <- dword ptr [EAX + 0x194c], EDX <- dword ptr [ECX + 0x1924]] | SetPrivateStoreBuyList lê `DDHDD` (condicional) | high |
| 92 | RequestPrivateStoreBuyManageCancel | `c` | NCPrivateBuyWnd::OnClose [] | RequestPrivateStoreBuyManageCancel lê `` | high |
| 93 | RequestPrivateStoreBuyQuit | `c` | NCConsole::ToggleInterface []; NCPrivateBuyWnd::OnAbort []; NCPrivateBuyFrameCtrl::OnClose [] | RequestPrivateStoreQuitBuy lê `` | high |
| 94 | SetPrivateStoreBuyMsg | `cS` | NCGaraDialogBox::OnOK [EAX] | SetPrivateStoreMsgBuy lê `S` | high |
| 95 | RequestPrivateStoreBuyList | `cd` | sem chamador no NWindow | **sem handler** | high |
| 96 | SendPrivateStoreBuyBuyList | `cddb  itens: ddhdd` | NCPrivateBuyWnd::OnStart [ECX <- dword ptr [ESI + 0xf8], EDI, ECX <- dword ptr [ECX + 0x1914], EDX <- dword ptr [ECX + 0x1918], EAX <- dword ptr [EDX + 0x193c], ECX <- dword ptr [EAX + 0x194c], EDX <- dword ptr [ECX + 0x1924]]; NCPrivateBuyWnd::OnSellConfirmed [ECX <- dword ptr [EDI + 0xf8], ESI, EAX <- dword ptr [EAX + 0x1914], ECX <- dword ptr [EAX + 0x1918], EDX <- dword ptr [ECX + 0x193c], EAX <- dword ptr [EDX + 0x194c], ECX <- dword ptr [EAX + 0x1924]] | RequestPrivateStoreBuySellList lê `DDDDHDD` (condicional) | high |
| 97 | SendTimeCheckPacket | `cdd` | sem chamador no NWindow | RequestTimeCheck lê `DD` | high |
| 98 | RequestStartAllianceWar | `cS` | sem chamador no NWindow | **sem handler** | high |
| 99 | ReplyStartAllianceWar | `cSd` | sem chamador no NWindow | **sem handler** | high |
| 9A | RequestStopAllianceWar | `cS` | sem chamador no NWindow | **sem handler** | high |
| 9B | ReplyStopAllianceWar | `cSd` | sem chamador no NWindow | **sem handler** | high |
| 9C | RequestSurrenderAllianceWar | `cS` | sem chamador no NWindow | **sem handler** | high |
| 9D | RequestSkillCoolTime | `c` | sem chamador no NWindow | RequestSkillCoolTime lê `` | high |
| 9E | RequestPackageSendableItemList | `cd` | sem chamador no NWindow | RequestPackageSendableItemList lê `D` | high |
| 9F | RequestPackageSend | `cddb  itens: dd` | sem chamador no NWindow | RequestPackageSend lê `DDDD` (condicional) | high |
| A0 | RequestBlock | `cdS` | NCChatWnd::ProcessChatLine [0x0, ECX] | RequestBlock lê `DS` (condicional) | high |
| A0 | RequestBlock | `cdS` | NCChatWnd::ProcessChatLine [0x0, ECX] | RequestBlock lê `DS` (condicional) | high |
| A0 | RequestBlock | `cd` | NCChatWnd::ProcessChatLine [0x0, ECX] | RequestBlock lê `DS` (condicional) | medium |
| A1 | RequestCastleSiegeInfo | `cd` | sem chamador no NWindow | **sem handler** | high |
| A2 | RequestCastleSiegeAttackerList | `cd` | sem chamador no NWindow | RequestCastleSiegeAttackerList lê `D` | high |
| A3 | RequestCastleSiegeDefenderList | `cd` | sem chamador no NWindow | RequestCastleSiegeDefenderList lê `D` | high |
| A4 | RequestJoinCastleSiege | `cddd` | sem chamador no NWindow | RequestJoinCastleSiege lê `DDD` | high |
| A5 | RequestConfirmCastleSiegeWaitingList | `cddd` | sem chamador no NWindow | RequestConfirmCastleSiegeWaitingList lê `DDD` | high |
| A6 | RequestSetCastleSiegeTime | `cdd` | sem chamador no NWindow | RequestSetCastleSiegeTime lê `DD` | high |
| A7 | RequestMultiSellChoose | `cdd` | sem chamador no NWindow | RequestMultiSellChoose lê `DDD` (condicional) | high |
| A8 | NetPing | `cddd` | sem chamador no NWindow | NetPing/NetPing/NetPing lê `DDD` | high |
| A9 | RequestRemainTime | `c` | NCChatWnd::ProcessChatLine [] | RequestRemainTime lê `` | high |
| AA | BypassUserCmd | `cd` | NCChatWnd::ProcessChatLine [EDI] | BypassUserCmd lê `D` | high |
| AB | GMSnoopEnd | `cd` | NCGMSnoopWnd::OnClose [ECX <- dword ptr [ESI + 0xf0]] | SnoopQuit lê `D` | high |
| AC | RequestRecipeBookOpen | `c` | NCInventoryWnd::OnManufacture []; NCRecipeManufactureWnd::OnPrev [] | RequestRecipeBookOpen lê `D` (condicional) | high |
| AD | RequestRecipeItemDelete | `cd` | NCGaraDialogBox::OnOK [EDX <- dword ptr [EBP + 0x258]] | RequestRecipeItemDelete lê `D` | high |
| AE | RequestRecipeItemMakeInfo | `cd` | NCRecipeBookItemWnd::OnLButtonDblClick [EDX <- dword ptr [EAX + 0x1914]] | RequestRecipeItemMakeInfo lê `D` | high |
| AF | RequestRecipeItemMakeSelf | `cd` | NCRecipeManufactureWnd::OnMake [EAX <- dword ptr [ECX + 0x1b14]] | RequestRecipeItemMakeSelf lê `D` | high |
| B0 | RequestRecipeShopManageList | `c` | NCChatWnd::ProcessChatLine []; NConsoleWnd::UseAction [] | RequestRecipeShopManageList lê `` | high |
| B1 | RequestRecipeShopMessageSet | `cS` | NCGaraDialogBox::OnOK [EAX] | RequestRecipeShopMessageSet lê `S` | high |
| B2 | RequestRecipeShopListSet | `cdb  itens: dd` | NCRecipeShopWnd::OnStart [ESI, ECX <- dword ptr [EAX + 0x1914], EDX <- dword ptr [ECX + 0x1924]] | RequestRecipeShopListSet lê `DDD` (condicional) | high |
| B3 | RequestRecipeShopManageQuit | `c` | NCConsole::ToggleInterface []; NCRecipeShopFrameCtrl::OnClose []; NCRecipeShopWnd::OnQuit [] | RequestRecipeShopManageQuit lê `` | high |
| B4 | RequestRecipeShopManageCancel | `c` | NCRecipeShopWnd::OnCancel [] | RequestRecipeShopManageCancel lê `` | high |
| B5 | RequestRecipeShopMakeInfo | `cdd` | NCRecipeBuyListWnd::OnSelectRecipe [EDX <- dword ptr [ESI + 0xdc], ECX <- dword ptr [ESP + 0xa08]] | RequestRecipeShopMakeInfo lê `DD` | high |
| B6 | RequestRecipeShopMakeDo | `cddd` | NCRecipeBuyManufactureWnd::OnMake [EAX <- dword ptr [ECX + 0x1b30], EDX <- dword ptr [ECX + 0x1b14], EAX <- dword ptr [ECX + 0x1b34]] | RequestRecipeShopMakeDo lê `DDD` | high |
| B7 | RequestRecipeShopSellList | `cd` | NCRecipeBuyManufactureWnd::OnPrev [ECX <- dword ptr [ESI + 0x1b30]] | RequestRecipeShopSellList lê `D` | high |
| B8 | RequestObserverEndPacket | `c` | sem chamador no NWindow | RequestObserverEnd lê `` | high |
| B9 | VoteSociality | `cd` | NConsoleWnd::UseAction [EAX <- dword ptr [EAX + 0x54]] | RequestVoteNew lê `D` | high |
| BA | RequestHennaItemList | `cd` | sem chamador no NWindow | RequestHennaList lê `` | medium |
| BB | RequestHennaItemInfo | `cd` | NCHennaListWnd::OnSelect [ECX <- dword ptr [ESP + 0x140c]] | RequestHennaItemInfo lê `D` | high |
| BC | RequestHennaEquip | `cd` | NCHennaInfoWnd::OnOK [EAX <- dword ptr [ESI + 0x3418]] | RequestHennaEquip lê `D` | high |
| BD | RequestHennaUnequipList | `cd` | sem chamador no NWindow | RequestHennaUnequipList lê `D` | medium |
| BE | RequestHennaUnequipInfo | `cd` | NCHennaListWnd::OnSelect [EDX <- dword ptr [ESP + 0x140c]] | RequestHennaUnequipInfo lê `D` | high |
| BF | RequestHennaUnequip | `cd` | NCHennaInfoWnd::OnOK [ECX <- dword ptr [ESI + 0x3418]] | RequestHennaUnequip lê `D` | high |
| C0 | RequestPledgePower | `cddb` | NConsoleWnd::RequestPledgePower [] | RequestPledgePower lê `DDD` (condicional) | high |
| C0 | RequestPledgePower | `cdd` | NConsoleWnd::RequestPledgePower [] | RequestPledgePower lê `DDD` (condicional) | medium |
| C1 | RequestMakeMacro | `cdSSSccb  itens: ccdcS` | NCMacroEditWnd::OnSave [0x0, 0x0, 0x0, 0x0, ECX <- dword ptr [EDI + 0xf0]] | RequestMakeMacro lê `DSSSCCCCDCS` (condicional) | medium |
| C2 | RequestDeleteMacro | `cd` | NCMacroArrayWnd::OnDialogResult [ECX <- dword ptr [EAX + 0xa38]] | RequestDeleteMacro lê `D` | high |
| C3 | RequestProcureCrop | `cddb  itens: ddd` | NConsoleWnd::RequestProcureCrop | RequestProcureCrop lê `DDDDD` (condicional) | high |
| C4 | RequestBuySeed | `cddb  itens: dd` | NConsoleWnd::RequestBuySeed | RequestBuySeed lê `DDDD` (condicional) | high |
| C5 | ConfirmDlg | `cdd` | NConsoleWnd::SendConfirmDlg | ConfirmDlg lê `DDD` (condicional) | high |
| C6 | RequestPreviewItem | `cdddb  itens: d` | NConsoleWnd::RequestPreviewItem | RequestPreviewItem lê `DDDD` (condicional) | high |
| C7 | RequestSSQStatus | `cc` | NCSSQStatusWnd::OnTogglePage [0x4]; NCSSQStatusWnd::OnTogglePage [0x1]; NCSSQStatusWnd::OnRefresh [0x4] (+5) | RequestSSQStatus lê `C` | high |
| C8 | PetitionVote | `cddS` | NCPetitionFeedBackWnd::OnSend [0, ESI] | PetitionVote lê `DDS` | high |
| CA | ReplyGameGuardQuery | `cdddd` | sub_100623c0 [EDX <- dword ptr [ESP + 0x14], EDX <- dword ptr [ESP + 0x14], EDX <- dword ptr [ESP + 0x14], EDX <- dword ptr [ESP + 0x14]] | ReplyGameGuardQuery/ReplyGameGuardQuery lê `` | high |
| CC | RequestSendL2FriendSay | `cSS` | L2FriendWnd::SendWhisper [EAX] | RequestSendL2FriendSay lê `SS` | high |
| CD | RequestOpenMinimap | `c` | NConsoleWnd::HookShortCutKey; NConsoleWnd::HookShortCutKey; NCMenuWnd::OnCommunity [] | RequestShowMiniMap lê `` | high |
| CE | RequestSendMsnChatLog | `cSSd` | sub_1011ce80 [EAX]; ChatWnd::SendWarning [EAX]; ChatWnd::ReceiveInstantMessage [EAX, ESI, 0x1] (+1) | RequestSendMsnChatLog lê `SSD` | high |
| CF:0001 | RequestOustFromPartyRoom | `chd` | NCPartyRoomWnd::OnBanBtnClick [ECX <- dword ptr [EAX + 0x1808]] | RequestOustFromPartyRoom lê `D` | high |
| CF:0002 | RequestDismissPartyRoom | `chdd` | NCPartyRoomWnd::RequestExit [EAX <- dword ptr [ESI + 0xe8]] | RequestDismissPartyRoom lê `D` | medium |
| CF:0003 | RequestWithdrawPartyRoom | `chdd` | NCPartyRoomWnd::RequestExit [EDX <- dword ptr [ESI + 0xe8]] | RequestWithdrawPartyRoom lê `D` | medium |
| CF:0004 | RequestHandOverPartyMaster | `chS` | NCChatWnd::ProcessChatLine [ESI]; NCChatWnd::ProcessChatLine [ESI]; NConsoleWnd::UseAction [EDI] | RequestHandOverPartyMaster lê `S` | high |
| CF:0005 | RequestAutoSoulShot | `chdd` | NCShortCutWnd::OnRButtonDown [EDX <- dword ptr [ECX + ESI*0x1 + 0x1918], 0] | RequestAutoSoulShot lê `DD` | high |
| LOGIN:00 | RequestLogin | `cxddddddhc` | sub_100623c0 [] | (login server) | medium |
| LOGIN:02 | RequestServerLogin | `cddc` | NConsoleWnd::RequestServerLogin | (login server) | medium |
| LOGIN:05 | RequestServerList | `cddc` | NConsoleWnd::RequestServerList [] | (login server) | medium |
| LOGIN:06 | RequestSCCheck | `cdx` | NConsoleWnd::RequestAuthCardKeyLogin [EAX <- dword ptr [EBP + 0x8], EAX <- dword ptr [EBP + 0xc]] | (login server) | high |
| LOGIN:07 | ResponseAuthGameGuard | `cddddd` | sub_100623c0 [] | (login server) | medium |

## 7. Esquisitices do cliente (o servidor precisa tolerar)

* `RequestMagicSkillList` (slot 153, 0x2E) usa o formato `ddd` no lugar de `cdd`: o OPCODE vai como DWORD (`2E 00 00 00`) seguido de 2 dwords. Sem chamador no NWindow
  e sem handler no servidor (a spec chama 0x2E de `RequestMagicSkillList`; o Interlude não tem).
* `RequestPetitionCancel` (0x80) manda o formato `cS` sem empilhar a string: o escritor lê um slot depois dos argumentos e sai lixo (uma string larga) depois do opcode.
  `RequestHennaItemList` (0xBA) e `RequestHennaUnequipList` (0xBD) mandam `cd` sem valor (dword de lixo). `FinishRotating` (0x4B) manda `cdd` com um só valor
  (o 2º é o `ESI` salvo do chamador). `RequestDismissPartyRoom` / `RequestWithdrawPartyRoom` mandam o último dword com o `ESI` salvo. O servidor só lê o que precisa, então passa.
* `RequestSkillItem` (0x3F) é o `RequestSkillList` da spec e `RequestMagicItem` (0x3E) o `RequestMagicList`: os nomes do vtable do C3 divergem da spec nesses dois.
* `EnterWorld(INT)` (slot 59, 0x03, `cS`) manda uma string larga fixa (`narcasse`) depois do opcode; o `EnterWorld` do servidor não lê nada.
* Há dois envios com o mesmo opcode: `0x01` (`MoveToLocation` com 7 dwords e `MoveBackwardToLocation` com 6), `0x04` (`Action` e `RequestAttack` slot 167),
  `0x12` (`RequestDropItem` e `RequestDropItemFromPet`), `0x36` / `0x5D` (`CanNotMoveAnymore`, sem e com veículo), `0x53` (`RequestSetPledgeCrest`, com e sem bloco), `0xA0` (`RequestBlock`, tipos 0..4),
  `0xC0` (`RequestPledgePower`): no JSON o segundo vai em `variants`.
* Pacotes do login server saem pela mesma classe (`UNetworkHandler+0x40`): `RequestLogin` (opcode 0, `cxdhc...`, o RSA de 128 bytes), `RequestServerLogin` (2), `RequestServerList` (5),
  `RequestSCCheck` (6), `ResponseAuthGameGuard` (7). Estão no JSON como `LOGIN:xx` e fora do servidor de jogo.

## 8. Nomes dos stubs do NWindow que enganam (achado extra)

`db/functions.csv` dá aos stubs `NConsoleWnd::Request*` os nomes que eles tinham no C2. Em 47 dos 126 stubs o slot que o stock chama no C3 é OUTRO (o offset está certo, o porte
chama por offset, mas o nome engana quem for ler o código ou o log do hook). Tabela (VA do stub, offset, nome em `db/functions.csv`, nome REAL do slot no C3):

| VA | offset | nome no db | pedido real (C3) |
|---|---|---|---|
| 0x1005aaa0 | +0x270 | RequestLogout | SendLogOutPacket |
| 0x1005c810 | +0x240 | RequestWarehouseDeposit | SendWareHouseDepositList |
| 0x1005c820 | +0x244 | RequestWarehouseWithdraw | SendWareHouseWithdrawList |
| 0x1005d020 | +0x25c | RequestDeleteMacro | RequestOpenMinimap |
| 0x1005d030 | +0x258 | RequestMagicSkillUse | RequestDeleteMacro |
| 0x1005d040 | +0x268 | RequestSkillList | RequestMagicSkillUse |
| 0x1005d050 | +0x28c | RequestSkillListC3 | RequestSkillItem |
| 0x1005d060 | +0x264 | ReceiveMagicSkillList | RequestMagicSkillList |
| 0x1005d4b0 | +0x1c0 | RequestPledgeMemberList | RequestCreatePledge |
| 0x1005d4c0 | +0x1c4 | RequestNet103 | RequestAnswerJoinPledge |
| 0x1005d4d0 | +0x1c8 | RequestAnswerJoinPledge | RequestOustPledgeMember |
| 0x1005d4e0 | +0x1cc | RequestOustPledgeMember | RequestDismissPledge |
| 0x1005d4f0 | +0x1d8 | RequestNet106 | RequestStartPledgeWar |
| 0x1005d510 | +0x1e0 | RequestReplyStartPledgeWar | RequestStopPledgeWar |
| 0x1005d530 | +0x1e8 | RequestReplyStopPledgeWar | RequestSurrenderPledgeWar |
| 0x1005d540 | +0x1f0 | RequestSurrenderPledgeWar | RequestSurrenderPersonally |
| 0x1005d550 | +0x1f4 | RequestSurrenderPersonally | RequestGiveNickName |
| 0x1005d560 | +0x1d0 | RequestGiveNickName | RequestJoinPledge |
| 0x1005d570 | +0x1d4 | RequestJoinPledge | RequestWithDrawalPledge |
| 0x1005d750 | +0x200 | RequestAllyInvite | RequestJoinAlly |
| 0x1005d970 | +0x224 | RequestAnswerFriendInvite | RequestFriendAddReply |
| 0x1005d9a0 | +0x2f4 | vfn184 | RequestCastleSiegeInfo |
| 0x1005d9b0 | +0x2f8 | RequestSiegeAttackerList | RequestCastleSiegeAttackerList |
| 0x1005d9c0 | +0x2fc | RequestSiegeDefenderList | RequestCastleSiegeDefenderList |
| 0x1005d9d0 | +0x300 | RequestJoinSiege | RequestJoinCastleSiege |
| 0x1005d9e0 | +0x304 | RequestConfirmSiegeWaitingList | RequestConfirmCastleSiegeWaitingList |
| 0x1005d9f0 | +0x308 | RequestSetSiegeTime | RequestSetCastleSiegeTime |
| 0x1005da40 | +0x22c | RequestPartyInvite | RequestJoinParty |
| 0x1005da60 | +0x234 | RequestPartyLeave | RequestWithDrawalParty |
| 0x1005da70 | +0x238 | RequestPartyDismiss | RequestOustPartyMember |
| 0x1005df30 | +0x1a4 | RequestTutorialQuestionMark | RequestTutorialQuestionMarkPressed |
| 0x1005e880 | +0x378 | RequestVendor | RequestPrivateStoreManage |
| 0x1005e890 | +0x37c | RequestSetPrivateStoreListSell | SetPrivateStoreList |
| 0x1005e8b0 | +0x384 | RequestPrivateStoreQuitSell | RequestPrivateStoreQuit |
| 0x1005e8d0 | +0x390 | RequestPrivateStoreBuy | SendPrivateStoreBuyList |
| 0x1005e910 | +0x394 | RequestBuy | RequestPrivateStoreBuyManage |
| 0x1005e920 | +0x398 | RequestSetPrivateStoreListBuy | SetPrivateStoreBuyList |
| 0x1005e940 | +0x3a0 | RequestPrivateStoreQuitBuy | RequestPrivateStoreBuyQuit |
| 0x1005e960 | +0x3ac | RequestPrivateStoreSell | SendPrivateStoreBuyBuyList |
| 0x1005ea00 | +0x30c | RequestBuyVIPItem | RequestMultiSellChoose |
| 0x1005ee90 | +0x3f0 | RequestDwarvenManufacture | RequestRemainTime |
| 0x1005eea0 | +0x2b4 | BuilderCommand | SendBypassBuildCmd |
| 0x10060140 | +0x2dc | vfn176 | RequestSkillCoolTime |
| 0x10061180 | +0x41c | sub_10061180 | RequestSendL2FriendSay |
| 0x10061190 | +0x420 | sub_10061190 | RequestSendMsnChatLog |
| 0x100611a0 | +0x418 | RequestPetitionFeedBack | PetitionVote |
| 0x10062360 | +0x44c | RequestAutoShot | RequestAutoSoulShot |

Exemplos que importam: o stub `0x1005d030`, "RequestMagicSkillUse" no db, é `RequestDeleteMacro` (0xC2); o `0x1005d040`, "RequestSkillList", é `RequestMagicSkillUse` (0x2F, o uso de
skill); a família do clã (`0x1005d4b0..0x1005d570`) está deslocada de um slot; o `0x1005ee90`, "RequestDwarvenManufacture", é `RequestRemainTime`. O JSON traz o nome real em `name` e o nome do db em `nwindow_stub`.

## 9. Não resolvido / limites

* **Pacotes sem chamador no NWindow** (ex.: `MoveToLocation`, `ValidatePosition`, `RequestAttack` slot 167): quem os chama é o Engine (controlador); a ferramenta não percorre o
  Engine para saber quando. Os campos `fields` deles vêm do próprio `Request*`.
* **Certeza média**: `0x5C` (movimento sobre veículo), `0x5D`, `Say2` variante de sussurro (o valor do alvo é `Top()` indireto), `RequestMakeMacro` (ícone e contagem em locais),
  os pacotes do login e os de lixo da seção 7. O formato e o opcode destes ainda estão certos; só o rastreio de um campo é incompleto.
* O `ArrayUtils`, `Config` e a lógica de `runImpl` do servidor não foram avaliados: esta nota compara opcodes e layouts de leitura. As comparações marcadas "condicional" (listas) foram revisadas à mão
  nas classes de loja, `SellItem`, `RequestBlock`, `RequestPledgePower`, `RequestMakeMacro`, `Say2C` e `MoveBackwardToLocation`; as demais batem em contagem de campos.
* O servidor muda sob os pés (outras pistas): re-execute `python tools/c3/c2s_formats.py --json analysis/c3_c2s_formats.json` para o estado de agora.

## 10. Como reproduzir

```
python tools/c3/c2s_formats.py                       # tabela resumida no terminal
python tools/c3/c2s_formats.py --name RequestActionUse --trace   # um pedido com os valores rastreados
python tools/c3/c2s_formats.py --list                # o vtable do UNetworkHandler
python tools/c3/c2s_formats.py --json analysis/c3_c2s_formats.json --md rows.md
python tools/c3/c2s_formats.py --json antes.json --handler <GamePacketHandler.java antigo>   # o estado do servidor num backup
```
