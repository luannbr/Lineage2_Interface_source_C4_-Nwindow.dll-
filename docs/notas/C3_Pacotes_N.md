# C3 pacotes, pista N: a fronteira NWindow -> Engine (NetHandler) do codigo portado

> Nota da publicação: esta nota fazia parte de uma auditoria de pacotes cujo lado do servidor (código Java, as notas das
> outras pistas e as ferramentas delas) não é publicado; as referências a elas ficam só como histórico.

Gerado por `python tools/c3/net_boundary.py --md docs/notas/C3_Pacotes_N.md` (bruto em `analysis/c3_net_boundary.json`).
A ferramenta executa simbolicamente o stock e a nossa DLL e compara, por funcao hookeada: o `vcall` no `Engine->NetHandler`,
a sequencia ORDENADA de valores de cada `L2ParamStack` (`PushBack`), o argumento do construtor e os argumentos das chamadas.

## 1. Veredito sobre sentar/levantar e cancelar alvo (lado NWindow)

O lado NWindow e IDENTICO ao stock nesses caminhos (certeza alta, lido do disassembly e do dataflow simbolico):

* `NConsoleWnd::UseAction` (0x10074ab0), ramo `default`: stock e nosso fazem `IsCtrlDown()` (virtual 297, +0x4a4),
  `UInput::KeyDown(0x10)`, `L2ParamStack Params(10)`, `PushBack(actionId)`, `PushBack(ctrl ? IsCtrlDown() : bForce)`,
  `PushBack(KeyDown(0x10) ? KeyDown(0x10) : bShift)`, depois `NConsoleWnd::RequestActionUse` (0x1005d3a0), que salta para
  `NetHandler->vtable+0x298` (slot 166, `RequestActionUse` no Engine C3). Ordem, contagem (3) e argumento do construtor (10) batem.
* `NConsoleWnd::RequestActionUse` (0x1005d3a0) e `RequestTargetCancel` (0x1005d390, +0x260 = slot 152, `RequestTargetCancel` no Engine C3):
  stock e nosso saltam para o mesmo offset. Ha uma unica sobrecarga de `L2ParamStack::PushBack` no Engine C3, `PushBack(void*)`
  (nao existe tag de tipo: o Engine le a pilha por posicao), entao "INT contra ponteiro" nao muda nada.
* Cancelar alvo: `NCTargetStatusWnd::OnClose` (0x100fa9f0, botao X), ESC (`NConsoleWnd::OnKeyEvent`) e `ShowRestartMenu` constroem uma
  `L2ParamStack Params(10)` VAZIA e chamam `RequestTargetCancel`, exatamente como o stock. O que sai no fio depende do Engine
  (formato do pacote no `Engine.dll` C3) e do servidor, nao do NWindow.
* Conclusao: se sentar/levantar e cancelar alvo nao funcionam, a causa NAO esta no que o NWindow empurra nem no slot do NetHandler.

## 2. Defeitos reais encontrados e corrigidos (todos do mesmo tipo: wrapper errado, "nomes deslocados")

O `NConsoleWnd` tem dezenas de wrappers minusculos (`0x1005d020..0x1005dxxx`: `mov eax,[ecx+0x48]; mov ecx,[eax+0x58]; mov edx,[ecx];
jmp [edx+X]`). Os nomes `Request*` vieram do mapa do C2; no C3 a ORDEM dos wrappers mudou em varias familias, entao o wrapper
que o NOSSO codigo chama pelo nome nao e o que o stock chama: o corpo do hook esta certo (usa o offset do stock), mas o ponto de
chamada manda outro pacote. A tabela da secao 4 lista os wrappers cujo nome difere do metodo que o Engine C3 da ao slot.

| funcao (VA) | antes (ponto de chamada) | agora | pacote |
|---|---|---|---|
| `NConsoleWnd::ReceiveAskJoinPledge` 0x10070570 (recusa automatica do convite de clan) | `RequestAnswerJoinPledge` = 0x1005d4d0 (+0x1c8) | wrapper 0x1005d4c0 (+0x1c4) | saia `RequestOustPledgeMember`, passa a `RequestAnswerJoinPledge` |
| `NConsoleWnd::ReceiveStartPledgeWar` 0x10070680 (recusa da guerra) | `RequestReplyStartPledgeWar` = 0x1005d510 (+0x1e0) | 0x1005d500 (+0x1dc) | saia `RequestStopPledgeWar`, passa a `ReplyStartPledgeWar` |
| `NConsoleWnd::ReceiveStopPledgeWar` 0x100707a0 (recusa do fim da guerra) | `RequestReplyStopPledgeWar` = 0x1005d530 (+0x1e8) | 0x1005d520 (+0x1e4) | saia `RequestSurrenderPledgeWar`, passa a `ReplyStopPledgeWar` |
| `NCPledgeWnd::OnInvite` 0x10047350 (botao Convidar do clan) | `RequestJoinPledge` = 0x1005d570 (+0x1d4) | 0x1005d560 (+0x1d0) | saia `RequestWithDrawalPledge`, passa a `RequestJoinPledge` |
| `NCMacroArrayWnd::OnDialogResult` 0x1009d090 (apagar macro) | `RequestDeleteMacro` = 0x1005d020 (+0x25c) | 0x1005d030 (+0x258) | saia `RequestOpenMinimap`, passa a `RequestDeleteMacro` |

Os valores empurrados (e a ordem) desses pontos de chamada ja eram iguais aos do stock; so o wrapper estava trocado. Os pontos de
chamada corrigidos usam `nw::StockCallAt<void>(VA, ...)` (o mesmo padrao dos macros `*_AT` de `NCGaraDialogBox.cpp`, que o porte
anterior ja tinha descoberto para os dialogos de clan).

## 3. Resultado da comparacao automatica

274 funcoes tocam a fronteira (L2ParamStack ou NetHandler); 26 ainda aparecem com diferenca simbolica (a secao 5 diz o que foi lido a mao).

| VA | funcao | tipo | stock x nosso |
|---|---|---|---|
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x294 (AnswerTradeRequest): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x2e4 (ReplyStartAllianceWar): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x2ec (ReplyStopAllianceWar): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x300 (RequestJoinCastleSiege): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x304 (RequestConfirmCastleSiegeWaitingList): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | VSLOT | NetHandler +0x30c (RequestMultiSellChoose): stock yes, ours NO |
| 10007d80 | NCGaraDialogBox::OnOK | PUSH | stock Params(0xa) pushes [[this+0x258], phi(0/1), phi(0/1)] -> NConsoleWnd::RequestJoinSiege / ours Params(0xa) pushes [[this+0x258], L-1688, 0] -> NConsoleWnd::RequestGetItemFromPet |
| 10007d80 | NCGaraDialogBox::OnOK | PUSH | stock Params(0xa) pushes [[this+0x258], [this+0x25c], 0] -> NConsoleWnd::RequestConfirmSiegeWaitingList / ours Params(0xa) pushes [[this+0x258], L-1688] -> NConsoleWnd::RequestGiveItemToPet |
| 10007d80 | NCGaraDialogBox::OnOK | PUSH | stock Params(0xa) pushes [[this+0x258], 1] -> NConsoleWnd::SendConfirmDlg / ours Params(0xa) pushes [[this+0x258], [this+0x25c]] -> (none) |
| 10007d80 | NCGaraDialogBox::OnOK | PUSH | stock Params(0xa) pushes [] -> (no consumer seen) / ours Params(0xa) pushes [ret(??DFString@@QBEPBGXZ)] -> (none) |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [(ret(1006a2b0)+0x1c)] -> NConsoleForwardFns::RequestHandOverPartyMaster / ours Params(0xa) pushes [SP:-4568] -> NConsoleWnd::BuilderCommand |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [(ret(1006a2b0)+0x1c), [[G@1026e44c]+0x39c8]] -> NConsoleWnd::RequestPartyInvite / ours Params(0xa) pushes [0, ret(?GetName@User@@QAEPAGXZ)] -> (none) |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [[ret(?GetMSCommandData@FL2Gam)], 1, 0] -> NConsoleWnd::RequestSkillList / ours Params(0xa) pushes [0, SP:-220] -> NConsoleWnd::RequestBlock |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [[ret(?GetMSCommandData@FL2Gam)], 0, 1] -> NConsoleWnd::RequestSkillList / ours Params(0xa) pushes [1, SP:-220] -> NConsoleWnd::RequestBlock |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [0, SP:-4544] -> NConsoleWnd::RequestBlock / ours Params(0xa) pushes [L-4576] -> NConsoleWnd::BypassUserCmd |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [0, ret(?GetName@User@@QAEPAGXZ)] -> (no consumer seen) / ours Params(0xa) pushes [] -> (none) |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [1, SP:-4544] -> (no consumer seen) / ours Params(0xa) pushes [] -> (none) |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [3] -> (no consumer seen) / ours Params(0xa) pushes [] -> (none) |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [4] -> (no consumer seen) / ours Params(0xa) pushes [] -> NConsoleWnd::RequestPartyLeave |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [ret(10046480)] -> NConsoleWnd::BypassUserCmd / ours NO MATCHING RECORD |
| 10050360 | NCChatWnd::ProcessChatLine | PUSH | stock Params(0xa) pushes [SP:-2036] -> NConsoleWnd::BuilderCommand / ours NO MATCHING RECORD |
| 10054540 | NCConsole::OnHtmlCommand | PUSH | stock Params(0xa) pushes [SP:-3604, SP:-1556, SP:-1044, ret(vt120), phi(SP:-2580/ret(vt120)), phi(SP:-2068/ret(vt120))] -> NConsoleWnd::RequestBBSWrite / ours Params(0xa) pushes [SP:-532, SP:-2580, SP:-3092, ret(vt120)] -> NConsoleWnd::RequestBBSWrite |
| 10074ab0 | NConsoleWnd::UseAction | VSLOT | NetHandler +0xdc (Attack): stock yes, ours NO |
| 10076020 | NConsoleWnd::UseShortCut | PUSH | stock Params(0xa) pushes [None, phi(a3/ret(vt4a4)), phi(a4/ret(?KeyDown@UInput@@QAEEH@Z))] -> NConsoleWnd::RequestSkillList / ours Params(0xa) pushes [None, phi(a1/ret(vt4a4)), L-152] -> NConsoleWnd::RequestSkillList |
| 10078ea0 | NConsoleWnd::FormatDateTime | PUSH | stock Params(0xb) pushes [0x2c9, 5, 1, a0, 0, SP:-60, 0, SP:-52, 0, SP:-44, 0, SP:-36] -> NConsoleWnd::MakeSystemMessage / ours Params(0xb) pushes [0x2c9, 5, 1, L-72, 0, SP:-28, 0, SP:-36, 0, SP:-44, 0, SP:-52] -> NConsoleWnd::MakeSystemMessage |
| 1008a9d0 | NCEquipItemWnd::OnLButtonUp | PUSH | stock Params(0xa) pushes [None, 0x10, 0x20, 2, 4] -> NConsoleWnd::RequestUnEquipItem / ours Params(0xa) pushes [2, 4, None] -> NConsoleWnd::RequestUnEquipItem |
| 1008b480 | NCEquipItemWnd::OnRButtonDown | PUSH | stock Params(0xa) pushes [None] -> NConsoleWnd::RequestUnEquipItem / ours Params(0xa) pushes [2, 4, 0x10, 0x20, None] -> NConsoleWnd::RequestUnEquipItem |
| 1008b480 | NCEquipItemWnd::OnRButtonDown | PUSH | stock Params(0xa) pushes [None, 0x10, 0x20, 2, 4] -> NConsoleWnd::RequestUnEquipItem,NConsoleWnd::RequestUnEquipItem,NConsoleWnd::RequestUnEquipItem / ours NO MATCHING RECORD |
| 1008b900 | NCEquipItemWnd::OnLButtonDblClick | PUSH | stock Params(0xa) pushes [None] -> NConsoleWnd::RequestUnEquipItem / ours Params(0xa) pushes [2, 4, 0x10, 0x20, [a1+0x1928]] -> NConsoleWnd::RequestUnEquipItem |
| 1008b900 | NCEquipItemWnd::OnLButtonDblClick | PUSH | stock Params(0xa) pushes [None, 0x10, 2, phi(0x20/4)] -> NConsoleWnd::RequestUnEquipItem / ours NO MATCHING RECORD |
| 1009d090 | NCMacroArrayWnd::OnDialogResult | PUSH | stock Params(0xa) pushes [] -> NConsoleWnd::RequestMagicSkillUse / ours Params(0xa) pushes [[[[[[[this+0x54]+0xc]+4]+0x10c]+[this+0x140]*4]+0xa38]] -> NConsoleWnd::RequestMagicSkillUse |
| 1009d1b0 | NCMacroArrayWnd::OnLButtonUp | PUSH | stock Params(0xa) pushes [4, ret(100f3aa0), [[[L-6356+0x10c]+[this+0x140]*4]+0xa38], 1] -> NConsoleWnd::RequestShortCutReg / ours Params(0xa) pushes [4, L-6176, None, 1] -> NConsoleWnd::RequestShortCutReg |
| 100a16c0 | NCSkillItemWnd::OnLButtonUp | PUSH | stock Params(0xa) pushes [[[[L-24+0x10c]+[L-24+0x140]*4]+0x1918], ret(vt4a4), ret(?KeyDown@UInput@@QAEEH@Z)] -> NConsoleWnd::RequestSkillList / ours Params(0xa) pushes [[[[this+0x10c]+[this+0x140]*4]+0x1918], ret(vt4a4), ret(?KeyDown@UInput@@QAEEH@Z)] -> NConsoleWnd::RequestSkillList |
| 100d1b40 | NCPetitionFeedBackWnd::OnSend | PUSH | stock Params(0xa) pushes [None, ret(100251b0)] -> NConsoleWndFeedBackView::RequestPetitionFeedBack / ours Params(0xa) pushes [None, ret(100415a0)] -> NConsoleWndFeedBackView::RequestPetitionFeedBack |
| 100ee3a0 | NCServerSelectWnd::OnOK | PUSH | stock Params(None) pushes [[ret(10020530)+0x804], [ret(10020530)+0x808], L-28] -> (no consumer seen) / ours Params(0xa) pushes [[L-28+0x804], [L-28+0x808], ret(?appAtoi@@YAHPBG@Z)] -> NConsoleWnd::RequestServerLogin |
| 100f0c00 | NCShopWnd::OnOK | PUSH | stock Params(None) pushes [[this+0xf4], None] -> NConsoleWnd::RequestBuyItem / ours Params(None) pushes [[this+0xf4], None, None, None] -> NConsoleWnd::RequestBuyItem |

## 4. Wrappers `NConsoleWnd` cujo nome difere do metodo que o Engine C3 da ao slot

Cada linha: VA do wrapper, nosso nome (do mapa do C2), offset do NetHandler que o stock chama, nome do slot no `UNetworkHandler` do Engine.dll C3.
O corpo do hook usa o offset do stock (correto). O RISCO e um ponto de chamada que escolhe o wrapper pelo NOME (a secao 2 lista os 5 que erravam).

| VA | nome nosso | offset | slot no Engine C3 |
|---|---|---|---|
| 1005aa10 | NConsoleWnd::RequestAuthCardKeyLogin | +0x128 | RequestSCCheck |
| 1005aaa0 | NConsoleWnd::RequestLogout | +0x270 | SendLogOutPacket |
| 1005c810 | NConsoleWnd::RequestWarehouseDeposit | +0x240 | SendWareHouseDepositList |
| 1005c820 | NConsoleWnd::RequestWarehouseWithdraw | +0x244 | SendWareHouseWithdrawList |
| 1005d020 | NConsoleWnd::RequestDeleteMacro | +0x25c | RequestOpenMinimap |
| 1005d030 | NConsoleWnd::RequestMagicSkillUse | +0x258 | RequestDeleteMacro |
| 1005d040 | NConsoleWnd::RequestSkillList | +0x268 | RequestMagicSkillUse |
| 1005d050 | NConsoleWndSkillListView::RequestSkillListC3 | +0x28c | RequestSkillItem |
| 1005d060 | NConsoleWnd::ReceiveMagicSkillList | +0x264 | RequestMagicSkillList |
| 1005d3b0 | NConsoleWnd::RequestAttackForce | +0xdc | Attack |
| 1005d490 | NConsoleLeftFns::RemovePledgeCrest | +0x1b0 | RequestSetPledgeCrest |
| 1005d4b0 | NConsoleWnd::RequestPledgeMemberList | +0x1c0 | RequestCreatePledge |
| 1005d4c0 | NConsoleWnd::RequestNet103 | +0x1c4 | RequestAnswerJoinPledge |
| 1005d4d0 | NConsoleWnd::RequestAnswerJoinPledge | +0x1c8 | RequestOustPledgeMember |
| 1005d4e0 | NConsoleWnd::RequestOustPledgeMember | +0x1cc | RequestDismissPledge |
| 1005d4f0 | NConsoleWnd::RequestNet106 | +0x1d8 | RequestStartPledgeWar |
| 1005d500 | NConsoleWnd::RequestStartPledgeWar | +0x1dc | RequestReplyStartPledgeWar |
| 1005d510 | NConsoleWnd::RequestReplyStartPledgeWar | +0x1e0 | RequestStopPledgeWar |
| 1005d520 | NConsoleWnd::RequestStopPledgeWar | +0x1e4 | RequestReplyStopPledgeWar |
| 1005d530 | NConsoleWnd::RequestReplyStopPledgeWar | +0x1e8 | RequestSurrenderPledgeWar |
| 1005d540 | NConsoleWnd::RequestSurrenderPledgeWar | +0x1f0 | RequestSurrenderPersonally |
| 1005d550 | NConsoleWnd::RequestSurrenderPersonally | +0x1f4 | RequestGiveNickName |
| 1005d560 | NConsoleWnd::RequestGiveNickName | +0x1d0 | RequestJoinPledge |
| 1005d570 | NConsoleWnd::RequestJoinPledge | +0x1d4 | RequestWithDrawalPledge |
| 1005d750 | NConsoleWnd::RequestAllyInvite | +0x200 | RequestJoinAlly |
| 1005d970 | NConsoleWnd::RequestAnswerFriendInvite | +0x224 | RequestFriendAddReply |
| 1005d9a0 | NConsoleWnd::vfn184 | +0x2f4 | RequestCastleSiegeInfo |
| 1005d9b0 | NConsoleWnd::RequestSiegeAttackerList | +0x2f8 | RequestCastleSiegeAttackerList |
| 1005d9c0 | NConsoleWnd::RequestSiegeDefenderList | +0x2fc | RequestCastleSiegeDefenderList |
| 1005d9d0 | NConsoleWnd::RequestJoinSiege | +0x300 | RequestJoinCastleSiege |
| 1005d9e0 | NConsoleWnd::RequestConfirmSiegeWaitingList | +0x304 | RequestConfirmCastleSiegeWaitingList |
| 1005d9f0 | NConsoleWnd::RequestSetSiegeTime | +0x308 | RequestSetCastleSiegeTime |
| 1005da40 | NConsoleWnd::RequestPartyInvite | +0x22c | RequestJoinParty |
| 1005da60 | NConsoleWnd::RequestPartyLeave | +0x234 | RequestWithDrawalParty |
| 1005da70 | NConsoleWnd::RequestPartyDismiss | +0x238 | RequestOustPartyMember |
| 1005df30 | NConsoleWnd::RequestTutorialQuestionMark | +0x1a4 | RequestTutorialQuestionMarkPressed |
| 1005df40 | NConsoleWnd::SendTutorialEvent | +0x1a8 | RequestTutorialClientEvent |
| 1005e160 | NConsoleWnd::RequestGMInventory | +0x15c | RequestGMCommand |
| 1005e180 | NConsoleWnd::RequestGMSkills | +0x15c | RequestGMCommand |
| 1005e1a0 | NConsoleWnd::RequestGMStatus | +0x15c | RequestGMCommand |
| 1005e1c0 | NConsoleWnd::RequestGMQuests | +0x15c | RequestGMCommand |
| 1005e1e0 | NConsoleWnd::RequestGMWarehouse | +0x15c | RequestGMCommand |
| 1005e200 | NConsoleWnd::RequestGMPledge | +0x15c | RequestGMCommand |
| 1005e880 | NConsoleWnd::RequestVendor | +0x378 | RequestPrivateStoreManage |
| 1005e890 | NConsoleWnd::RequestSetPrivateStoreListSell | +0x37c | SetPrivateStoreList |
| 1005e8b0 | NConsoleWnd::RequestPrivateStoreQuitSell | +0x384 | RequestPrivateStoreQuit |
| 1005e8d0 | NConsoleWnd::RequestPrivateStoreBuy | +0x390 | SendPrivateStoreBuyList |
| 1005e910 | NConsoleWnd::RequestBuy | +0x394 | RequestPrivateStoreBuyManage |
| 1005e920 | NConsoleWnd::RequestSetPrivateStoreListBuy | +0x398 | SetPrivateStoreBuyList |
| 1005e940 | NConsoleWnd::RequestPrivateStoreQuitBuy | +0x3a0 | RequestPrivateStoreBuyQuit |
| 1005e960 | NConsoleWnd::RequestPrivateStoreSell | +0x3ac | SendPrivateStoreBuyBuyList |
| 1005ea00 | NConsoleWnd::RequestBuyVIPItem | +0x30c | RequestMultiSellChoose |
| 1005ee90 | NConsoleWnd::RequestDwarvenManufacture | +0x3f0 | RequestRemainTime |
| 1005eea0 | NConsoleWnd::BuilderCommand | +0x2b4 | SendBypassBuildCmd |
| 1005f470 | NConsoleWnd::FindItemByID | +0x9c | GetItem |
| 1005f4a0 | NConsoleWnd::FindUserByName | +0x8c | GetUser |
| 1005f4b0 | NConsoleWnd::FindUserByID | +0x90 | GetUser |
| 1005f4c0 | NConsoleWnd::FindStaticObjectInfo | +0x98 | GetStaticObject |
| 1005f4d0 | NConsoleWnd::GetTargetedActor | +0x90 | GetUser |
| 1005f610 | NConsoleWnd::GetPledgeCrestTexture | +0xac | GetPledgeCrestTex |
| 1005f620 | NConsoleWnd::GetAllyCrestTexture | +0xb0 | GetAllyCrestTex |
| 10060140 | NConsoleWnd::vfn176 | +0x2dc | RequestSkillCoolTime |
| 10060b40 | NConsoleWnd::SendConfirmDlg | +0x410 | ConfirmDlg |
| 100611a0 | NConsoleWndFeedBackView::RequestPetitionFeedBack | +0x418 | PetitionVote |
| 10062360 | NConsoleWnd::RequestAutoShot | +0x44c | RequestAutoSoulShot |
| 10064e70 | NConsoleWnd::TargetMyself | +0xe0 | Action |
| 10064f00 | NConsoleWnd::RequestTarget | +0xe0 | Action |

## 4b. Slots do NetHandler chamados pelo stock (offset -> metodo do UNetworkHandler no Engine.dll C3)

| offset | slot | metodo no Engine C3 | funcoes NWindow que chamam |
|---|---|---|---|
| +0x88 | 34 | GetPetInfo | NConsoleWnd::GetPetInfo |
| +0x8c | 35 | GetUser | NConsoleWnd::FindUserByName |
| +0x90 | 36 | GetUser | NCBroadcastObserverWnd::HideWindow, NCBroadcastObserverWnd::NotifyTeamInfoEnd, NCBroadcastObserverWnd::NotifyTeamUnlocked, NCBroadcastObserverWnd::SetSelecteUser, NCBroadcastObserverWnd::ShowWindow, NConsoleWnd::FindUserByID ... |
| +0x94 | 37 | SetMatchedStore | NCChatWnd::ProcessChatLine |
| +0x98 | 38 | GetStaticObject | NConsoleWnd::AssistMember, NConsoleWnd::FindStaticObjectInfo, NConsoleWnd::RequestAssist, NMacroEntry::Tick |
| +0x9c | 39 | GetItem | NConsoleWnd::FindItemByID |
| +0xa8 | 42 | GetPledgeInfo | NConsoleWnd::GetPledgeInfo |
| +0xac | 43 | GetPledgeCrestTex | NConsoleWnd::GetPledgeCrestTexture |
| +0xb0 | 44 | GetAllyCrestTex | NConsoleWnd::GetAllyCrestTexture |
| +0xbc | 47 | IsEnemyPledge | NConsoleWnd::UpdateCursor |
| +0xc0 | 48 | IsSurrender | NConsoleWnd::UpdateCursor |
| +0xcc | 51 | RefreshWSAAsyncSelect | MSNSocket::OnSocketMessage |
| +0xdc | 55 | Attack | NConsoleWnd::RequestAttack, NConsoleWnd::RequestAttackForce |
| +0xe0 | 56 | Action | NConsoleWnd::AssistMember, NConsoleWnd::RequestAction, NConsoleWnd::RequestAssist, NConsoleWnd::RequestAttack, NConsoleWnd::RequestPickUp, NConsoleWnd::RequestTarget ... |
| +0xf0 | 60 | SocialAction | NConsoleWnd::RequestSocialAction |
| +0xf4 | 61 | ChangeMoveType | NConsoleWnd::RequestChangeMoveType |
| +0xf8 | 62 | ChangeWaitType | NConsoleWnd::RequestChangeWaitType |
| +0x124 | 73 | RequestAuthLogin | NConsoleWnd::RequestAuthLogin |
| +0x128 | 74 | RequestSCCheck | NConsoleWnd::RequestAuthCardKeyLogin |
| +0x12c | 75 | RequestServerList | NConsoleWnd::RequestServerList |
| +0x130 | 76 | RequestServerLogin | NConsoleWnd::RequestServerLogin |
| +0x134 | 77 | RequestGameStart | NConsoleWnd::RequestGameStart |
| +0x138 | 78 | RequestCharacterDelete | NConsoleWnd::RequestCharacterDelete |
| +0x13c | 79 | RequestCharacterRestore | NConsoleWnd::RequestCharacterRestore |
| +0x140 | 80 | RequestNewCharacter | NConsoleWnd::RequestNewCharacter |
| +0x148 | 82 | ReviveReply | NConsoleWnd::AnswerReviveRequest |
| +0x14c | 83 | RequestItemList | NConsoleWnd::RequestItemList |
| +0x150 | 84 | RequestPetition | NConsoleWnd::RequestPetition |
| +0x154 | 85 | RequestPetitionCancel | NConsoleWnd::RequestPetitionCancel |
| +0x158 | 86 | RequestGMList | NConsoleWnd::RequestGMList |
| +0x15c | 87 | RequestGMCommand | NConsoleWnd::RequestGMInventory, NConsoleWnd::RequestGMPledge, NConsoleWnd::RequestGMQuests, NConsoleWnd::RequestGMSkills, NConsoleWnd::RequestGMStatus, NConsoleWnd::RequestGMWarehouse |
| +0x160 | 88 | RequestTrade | NConsoleWnd::RequestTrade |
| +0x168 | 90 | RequestTradeDone | NConsoleWnd::RequestTradeDone |
| +0x170 | 92 | RequestUseItem | NConsoleWnd::RequestUseItem |
| +0x178 | 94 | RequestSellItem | NConsoleWnd::RequestSellItem |
| +0x17c | 95 | RequestBuyItem | NConsoleWnd::RequestBuyItem |
| +0x180 | 96 | RequestPreviewItem | NConsoleWnd::RequestPreviewItem |
| +0x184 | 97 | RequestBuySeed | NConsoleWnd::RequestBuySeed |
| +0x188 | 98 | RequestProcureCrop | NConsoleWnd::RequestProcureCrop |
| +0x18c | 99 | RequestTeleport | NConsoleWnd::MoveToLocation |
| +0x190 | 100 | RequestLinkHtml | NConsoleWnd::RequestLinkHtml |
| +0x194 | 101 | RequestBBSWrite | NConsoleWnd::RequestBBSWrite |
| +0x198 | 102 | RequestBypassToServer | NConsoleWnd::RequestBypassToServer |
| +0x19c | 103 | RequestTutorialLinkHtml | NConsoleWnd::RequestTutorialLinkHtml |
| +0x1a0 | 104 | RequestTutorialPassCmdToServer | NConsoleWnd::RequestTutorialPassCmdToServer |
| +0x1a4 | 105 | RequestTutorialQuestionMarkPressed | NConsoleWnd::RequestTutorialQuestionMark |
| +0x1a8 | 106 | RequestTutorialClientEvent | NConsoleWnd::SendTutorialEvent |
| +0x1b0 | 108 | RequestSetPledgeCrest | NConsoleLeftFns::RemovePledgeCrest, NConsoleWnd::RegisterPledgeCrest |
| +0x1c0 | 112 | RequestCreatePledge | NConsoleWnd::RequestPledgeMemberList |
| +0x1c4 | 113 | RequestAnswerJoinPledge | NConsoleWnd::RequestNet103 |
| +0x1c8 | 114 | RequestOustPledgeMember | NConsoleWnd::RequestAnswerJoinPledge |
| +0x1cc | 115 | RequestDismissPledge | NConsoleWnd::RequestOustPledgeMember |
| +0x1d0 | 116 | RequestJoinPledge | NConsoleWnd::RequestGiveNickName |
| +0x1d4 | 117 | RequestWithDrawalPledge | NConsoleWnd::RequestJoinPledge |
| +0x1d8 | 118 | RequestStartPledgeWar | NConsoleWnd::RequestNet106 |
| +0x1dc | 119 | RequestReplyStartPledgeWar | NConsoleWnd::RequestStartPledgeWar |
| +0x1e0 | 120 | RequestStopPledgeWar | NConsoleWnd::RequestReplyStartPledgeWar |
| +0x1e4 | 121 | RequestReplyStopPledgeWar | NConsoleWnd::RequestStopPledgeWar |
| +0x1e8 | 122 | RequestSurrenderPledgeWar | NConsoleWnd::RequestReplyStopPledgeWar |
| +0x1f0 | 124 | RequestSurrenderPersonally | NConsoleWnd::RequestSurrenderPledgeWar |
| +0x1f4 | 125 | RequestGiveNickName | NConsoleWnd::RequestSurrenderPersonally |
| +0x1f8 | 126 | RequestSetAllyCrest | NConsoleWnd::RegisterAllyCrest |
| +0x200 | 128 | RequestJoinAlly | NConsoleWnd::RequestAllyInvite |
| +0x204 | 129 | RequestAnswerJoinAlly | NConsoleWnd::RequestAnswerJoinAlly |
| +0x208 | 130 | RequestOustAlly | NConsoleWnd::RequestOustAlly |
| +0x20c | 131 | RequestWithdrawAlly | NConsoleWnd::RequestWithdrawAlly |
| +0x210 | 132 | RequestDismissAlly | NConsoleWnd::RequestDismissAlly |
| +0x214 | 133 | RequestAllyInfo | NConsoleWnd::RequestAllyInfo |
| +0x218 | 134 | RequestFriendInvite | NConsoleWnd::RequestFriendInvite |
| +0x21c | 135 | RequestFriendDel | NConsoleWnd::RequestFriendDel |
| +0x220 | 136 | RequestFriendList | NConsoleWnd::RequestFriendList |
| +0x224 | 137 | RequestFriendAddReply | NConsoleWnd::RequestAnswerFriendInvite |
| +0x228 | 138 | RequestBlock | NConsoleWnd::RequestBlock |
| +0x22c | 139 | RequestJoinParty | NConsoleWnd::RequestPartyInvite |
| +0x230 | 140 | RequestAnswerJoinParty | NConsoleWnd::RequestAnswerJoinParty |
| +0x234 | 141 | RequestWithDrawalParty | NConsoleWnd::RequestPartyLeave |
| +0x238 | 142 | RequestOustPartyMember | NConsoleWnd::RequestPartyDismiss |
| +0x240 | 144 | SendWareHouseDepositList | NConsoleWnd::RequestWarehouseDeposit |
| +0x244 | 145 | SendWareHouseWithdrawList | NConsoleWnd::RequestWarehouseWithdraw |
| +0x248 | 146 | RequestShortCutReg | NConsoleWnd::RequestShortCutReg |
| +0x250 | 148 | RequestShortCutDel | NConsoleWnd::RequestShortCutDel |
| +0x254 | 149 | RequestMakeMacro | NConsoleWnd::RequestMakeMacro |
| +0x258 | 150 | RequestDeleteMacro | NConsoleWnd::RequestMagicSkillUse |
| +0x25c | 151 | RequestOpenMinimap | NConsoleWnd::RequestDeleteMacro |
| +0x260 | 152 | RequestTargetCancel | NConsoleWnd::RequestTargetCancel |
| +0x264 | 153 | RequestMagicSkillList | NConsoleWnd::ReceiveMagicSkillList |
| +0x268 | 154 | RequestMagicSkillUse | NConsoleWnd::RequestSkillList |
| +0x26c | 155 | Say2 | NConsoleWnd::SendChatMsgToServer |
| +0x270 | 156 | SendLogOutPacket | NConsoleWnd::ReceiveVersionCheck, NConsoleWnd::RequestLogout, NConsoleWnd::ReturnToLogin |
| +0x288 | 162 | RequestUnEquipItem | NConsoleWnd::RequestUnEquipItem |
| +0x28c | 163 | RequestSkillItem | NConsoleWndSkillListView::RequestSkillListC3 |
| +0x294 | 165 | AnswerTradeRequest | NConsoleWnd::AnswerTradeRequest |
| +0x298 | 166 | RequestActionUse | NConsoleWnd::RequestActionUse |
| +0x2a0 | 168 | RequestRestart | NConsoleWnd::RequestRestart |
| +0x2a4 | 169 | RequestRestartPoint | NConsoleWnd::RequestRestartPoint |
| +0x2b0 | 172 | RequestShowboard | NConsoleWnd::RequestShowBoard |
| +0x2b4 | 173 | SendBypassBuildCmd | NConsoleWnd::BuilderCommand |
| +0x2bc | 175 | RequestCrystallizeItem | NConsoleWnd::RequestCrystallizeItem |
| +0x2c4 | 177 | AuthReconnect | NCEulaWnd::OnDisagree, NCServerSelectWnd::OnCancel, NConsoleWnd::ReturnToLogin |
| +0x2c8 | 178 | RequestQuestList | NConsoleWnd::RequestQuestList |
| +0x2cc | 179 | RequestDestroyQuest | NConsoleWnd::RequestDestroyQuest |
| +0x2d0 | 180 | RequestEnchantItem | NConsoleWnd::RequestEnchantItem |
| +0x2d4 | 181 | RequestAcquireSkillInfo | NConsoleWnd::RequestAcquireSkillInfo |
| +0x2d8 | 182 | RequestAcquireSkill | NConsoleWnd::RequestAcquireSkill |
| +0x2dc | 183 | RequestSkillCoolTime | NConsoleWnd::vfn176 |
| +0x2e0 | 184 | RequestStartAllianceWar | NConsoleWnd::RequestStartAllianceWar |
| +0x2e4 | 185 | ReplyStartAllianceWar | NConsoleWnd::RequestReplyStartAllianceWar |
| +0x2e8 | 186 | RequestStopAllianceWar | NConsoleWnd::RequestStopAllianceWar |
| +0x2ec | 187 | ReplyStopAllianceWar | NConsoleWnd::RequestReplyStopAllianceWar |
| +0x2f0 | 188 | RequestSurrenderAllianceWar | NConsoleWnd::RequestSurrenderAllianceWar |
| +0x2f4 | 189 | RequestCastleSiegeInfo | NConsoleWnd::vfn184 |
| +0x2f8 | 190 | RequestCastleSiegeAttackerList | NConsoleWnd::RequestSiegeAttackerList |
| +0x2fc | 191 | RequestCastleSiegeDefenderList | NConsoleWnd::RequestSiegeDefenderList |
| +0x300 | 192 | RequestJoinCastleSiege | NConsoleWnd::RequestJoinSiege |
| +0x304 | 193 | RequestConfirmCastleSiegeWaitingList | NConsoleWnd::RequestConfirmSiegeWaitingList |
| +0x308 | 194 | RequestSetCastleSiegeTime | NConsoleWnd::RequestSetSiegeTime |
| +0x30c | 195 | RequestMultiSellChoose | NConsoleWnd::RequestBuyVIPItem |
| +0x310 | 196 | RequestPackageSend | NConsoleWnd::RequestPackageSend |
| +0x314 | 197 | RequestPackageSendableItemList | NConsoleWnd::RequestPackageSendableItemList |
| +0x318 | 198 | RequestPetGetItem | NConsoleWnd::PetPickUp |
| +0x31c | 199 | RequestPetUseItem | NConsoleWnd::RequestPetUseItem |
| +0x320 | 200 | RequestGiveItemToPet | NConsoleWnd::RequestGiveItemToPet |
| +0x324 | 201 | RequestGetItemFromPet | NConsoleWnd::RequestGetItemFromPet |
| +0x32c | 203 | RequestChangePetName | NConsoleWnd::RequestChangePetName |
| +0x330 | 204 | RequestRecipeBookOpen | NConsoleWnd::RequestRecipeBookOpen |
| +0x334 | 205 | RequestRecipeItemDelete | NConsoleWnd::RequestRecipeItemDelete |
| +0x338 | 206 | RequestRecipeItemMakeInfo | NConsoleWnd::RequestRecipeItemMakeInfo |
| +0x33c | 207 | RequestRecipeItemMakeSelf | NConsoleWnd::RequestRecipeItemMakeSelf |
| +0x340 | 208 | RequestRecipeShopManageList | NConsoleWnd::RequestRecipeShopManageList |
| +0x344 | 209 | RequestRecipeShopMessageSet | NConsoleWnd::RequestRecipeShopMessageSet |
| +0x348 | 210 | RequestRecipeShopListSet | NConsoleWnd::RequestRecipeShopListSet |
| +0x34c | 211 | RequestRecipeShopManageQuit | NConsoleWnd::RequestRecipeShopManageQuit |
| +0x350 | 212 | RequestRecipeShopManageCancel | NConsoleWnd::RequestRecipeShopManageCancel |
| +0x354 | 213 | RequestRecipeShopSellList | NConsoleWnd::RequestRecipeShopSellList |
| +0x358 | 214 | RequestRecipeShopMakeInfo | NConsoleWnd::RequestRecipeShopMakeInfo |
| +0x35c | 215 | RequestRecipeShopMakeDo | NConsoleWnd::RequestRecipeShopMakeDo |
| +0x360 | 216 | RequestHennaItemInfo | NConsoleWnd::RequestHennaItemInfo |
| +0x364 | 217 | RequestHennaEquip | NConsoleWnd::RequestHennaEquip |
| +0x368 | 218 | RequestHennaUnequipInfo | NConsoleWnd::RequestHennaUnequipInfo |
| +0x36c | 219 | RequestHennaUnequip | NConsoleWnd::RequestHennaUnequip |
| +0x370 | 220 | RequestHennaItemList | NConsoleWnd::RequestHennaItemList |
| +0x374 | 221 | RequestHennaUnequipList | NConsoleWnd::RequestHennaUnequipList |
| +0x378 | 222 | RequestPrivateStoreManage | NConsoleWnd::RequestVendor |
| +0x37c | 223 | SetPrivateStoreList | NConsoleWnd::RequestSetPrivateStoreListSell |
| +0x380 | 224 | RequestPrivateStoreManageCancel | NConsoleWnd::RequestPrivateStoreManageCancel |
| +0x384 | 225 | RequestPrivateStoreQuit | NConsoleWnd::RequestPrivateStoreQuitSell |
| +0x388 | 226 | SetPrivateStoreMsg | NConsoleWnd::SetPrivateStoreMsg |
| +0x390 | 228 | SendPrivateStoreBuyList | NConsoleWnd::RequestPrivateStoreBuy |
| +0x394 | 229 | RequestPrivateStoreBuyManage | NConsoleWnd::RequestBuy |
| +0x398 | 230 | SetPrivateStoreBuyList | NConsoleWnd::RequestSetPrivateStoreListBuy |
| +0x39c | 231 | RequestPrivateStoreBuyManageCancel | NConsoleWnd::RequestPrivateStoreBuyManageCancel |
| +0x3a0 | 232 | RequestPrivateStoreBuyQuit | NConsoleWnd::RequestPrivateStoreQuitBuy |
| +0x3a4 | 233 | SetPrivateStoreBuyMsg | NConsoleWnd::SetPrivateStoreBuyMsg |
| +0x3ac | 235 | SendPrivateStoreBuyBuyList | NConsoleWnd::RequestPrivateStoreSell |
| +0x3b4 | 237 | GetPetNearestItem | NConsoleWnd::PetPickUp |
| +0x3b8 | 238 | GetNearestItem | NConsoleWnd::RequestPickUp |
| +0x3c4 | 241 | GetNextEnemy | NConsoleWnd::RequestTargetNext, NMacroEntry::Tick |
| +0x3f0 | 252 | RequestRemainTime | NConsoleWnd::RequestDwarvenManufacture |
| +0x3f4 | 253 | BypassUserCmd | NConsoleWnd::BypassUserCmd |
| +0x3f8 | 254 | AddServerPingList | NCServerSelectWnd::AddServer |
| +0x3fc | 255 | EndServerPingList | NCServerSelectWnd::OnServerListEnd |
| +0x400 | 256 | GMSnoopEnd | NConsoleWnd::RequestGMSnoopEnd |
| +0x404 | 257 | RequestObserverEndPacket | NConsoleWnd::RequestObserverEndPacket |
| +0x408 | 258 | RequestPledgePower | NConsoleWnd::RequestPledgePower |
| +0x40c | 259 | VoteSociality | NConsoleWnd::UseAction |
| +0x410 | 260 | ConfirmDlg | NConsoleWnd::SendConfirmDlg |
| +0x414 | 261 | RequestSSQStatus | NConsoleSSQ::RequestSSQStatus |
| +0x418 | 262 | PetitionVote | NConsoleWndFeedBackView::RequestPetitionFeedBack |
| +0x430 | 268 | RequestListPartyWating | NConsoleForwardFns::RequestListPartyWating |
| +0x434 | 269 | RequestManagePartyRoom | NConsoleForwardFns::RequestManagePartyRoom |
| +0x438 | 270 | RequestJoinPartyRoom | NConsoleForwardFns::RequestJoinPartyRoom |
| +0x43c | 271 | RequestOustFromPartyRoom | NConsoleForwardFns::RequestOustFromPartyRoom |
| +0x440 | 272 | RequestDismissPartyRoom | NConsoleForwardFns::RequestDismissPartyRoom |
| +0x444 | 273 | RequestWithdrawPartyRoom | NConsoleForwardFns::RequestWithdrawPartyRoom |
| +0x448 | 274 | RequestHandOverPartyMaster | NConsoleForwardFns::RequestHandOverPartyMaster |
| +0x44c | 275 | RequestAutoSoulShot | NConsoleWnd::RequestAutoShot |


## 5. Nao provado / limites da ferramenta

* `NCChatWnd::ProcessChatLine` (0x10050360) e `NCConsole::OnHtmlCommand` (0x10054540): muitos `Params` num corpo so; a ferramenta junta
  os blocos por construtor e nao e sensivel ao caminho, entao o relatorio e ruidoso. O conjunto de slots/wrappers chamados bate
  (nenhum VSLOT), mas os pushes individuais nao foram provados um a um. Revisar a mao se um `/comando` de chat se comportar mal.
* `NCEquipItemWnd::OnLButtonUp/OnRButtonDown/OnLButtonDblClick`, `NCShopWnd::OnOK`, `NCTrainListWnd::OnListCommand`, `FormatDateTime`:
  aparecem como PUSH mas foram lidos a mao (decompile do stock x fonte) e sao iguais (varios ramos reusando o mesmo construtor,
  lacos, e campos de um struct preenchido por chamada). `NCPetitionFeedBackWnd::OnSend`: o stock chama `NCMultiEdit::GetText`
  (0x100251b0) e nos `NCHtmlMultiEdit::GetValue` (0x100415a0), que e um `jmp 0x100251b0`: igual.
* `NMacroEntry::Tick` (0x1009e8f0) e o ramo Attack de `NConsoleWnd::UseAction`: o nosso chama `nw::VirtualCall<>` / `RequestAttack`
  por funcoes auxiliares fora de linha que a ferramenta nao atravessa com valores; os offsets (38 -> +0x98, 241 -> +0x3c4,
  +0xdc/+0xe0) foram conferidos na fonte contra o stock.
* Chamadas `nw::VirtualCall<>(NetHandler, N, ...)` com N fixo (varias em `NConsoleWndNet.cpp`) so sao conferidas pelo offset final
  (verify_hooks) e pelo conjunto de slots desta ferramenta; o FORMATO que o Engine le em cada slot nao e verificavel daqui.
* O que o `Engine.dll` C3 faz com a `L2ParamStack` de cada slot (formato do pacote c2s) nao foi lido nesta pista.
