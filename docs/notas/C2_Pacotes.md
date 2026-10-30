# O que o cliente C2 espera de cada pacote do servidor

O `NWindow.dll` não lê pacotes: quem decodifica o fluxo do servidor é o `Engine.dll` (`UNetworkHandler::Tick`), com um
interpretador de formato (rva 0xE0240 do Engine do C2) que lê o pacote pela string de formato (`c` byte, `h` word, `d` dword,
`f` double, `S` string larga) e em seguida chama um virtual de `FL2NetNotify` (`On<Pacote>(...)`), que chega ao `NConsoleWnd`.
Um pacote MAIS CURTO do que o formato manda o interpretador ler depois do fim do buffer: GPF em `Engine.dll+E028F`
(`mov dx,[ebx]`, o caso `h`). `NWindow_crash.log` (o registrador de falhas do standalone) mostra isso.

Ferramenta: `python tools/c2/packet_formats.py [--name Trecho]` lê o `Engine.dll` do cliente e imprime cada string de formato
com o virtual de `FL2NetNotify` que o mesmo trecho de código chama em seguida. É heurística: o nome de uma linha só vale quando o
formato aparece junto da chamada certa (as linhas do `PrivateStore*` abaixo foram conferidas no disassembly). O opcode de
cada pacote não sai daqui (o despacho por opcode não é uma tabela simples).

## Lojas privadas (conferido no disassembly do handler 0x103f1cd0 do Engine do C2)
`PrivateStoreManageList` (a loja de VENDA do próprio jogador; no servidor C4/C5: `PrivateStoreManageListSell`):

    c opcode
    ddd            três dwords: o dono, o adena e a QUANTIDADE da 1a lista (na ordem em que o código usa cada um)
    N1 x "ddddhhhdd"   itens do inventário (30 bytes cada)
    d              quantidade da 2a lista
    N2 x "ddddhhhddd"  itens à venda (34 bytes cada)

O C5 (Interlude) manda `sellerId, packageSale, adena, count`: QUATRO inteiros antes da lista. O C2 lê três: o `packageSale` do
C5 vira o "count" do C2 (ou o adena vira o count), a lista tem milhões de itens, o interpretador passa do fim do pacote e dá GPF.
É o que o log do servidor mostra (o cliente pediu `RequestPrivateStoreSell`, o servidor mandou `PrivateStoreManageListSell`
size=1493, o cliente caiu na hora). Para o cliente C2 o servidor tem de mandar o formato do C2 (sem `packageSale`).

`PrivateStoreManageList` de compra (`PrivateStoreBuyManageList`): `ddd`, depois "dhddhdh" (inventário) e "dhddhdhdd" (itens da
compra). `PrivateStoreList` (a loja de outro jogador): `ddd` + "ddddhhhddd". `PrivateStoreBuyList`: `ddd` + "ddhddhdhdd".

## Saída bruta da ferramenta (para conferir com o servidor)
### PrivateStore
103f1cf4  ddd                                OnStartPrivateStoreManageList()
103f1df0  ddddhhhdd                          OnPrivateStoreManageList(ItemInfo&, ItemInfo&, INT, INT)
103f1eb5  ddddhhhddd                         OnPrivateStoreManageList(ItemInfo&, ItemInfo&, INT, INT)
103f2000  ddd                                OnStartPrivateStoreList()
103f20a7  ddddhhhddd                         OnPrivateStoreList(ItemInfo&, INT, INT)
103f2194  ddd                                OnStartPrivateStoreBuyManageList()
103f228c  dhddhdh                            OnPrivateStoreBuyManageList(ItemInfo&, ItemInfo&, INT, INT)
103f2350  dhddhdhdd                          OnPrivateStoreBuyManageList(ItemInfo&, ItemInfo&, INT, INT)
103f24a0  ddd                                OnStartPrivateStoreBuyList()
103f2547  ddhddhdhdd                         OnPrivateStoreBuyList(ItemInfo&, INT, INT)
### PledgeInfo
103ec805  SdSS                               OnGMViewReceivePledgeInfo(PledgeInfo&)
103ec883  dddddddddSd                        OnGMViewReceivePledgeInfo(PledgeInfo&)
103f0bed  SddSddddddddSd                     OnReceivePledgeExtendedInfo(PledgeInfo&)
1040a2cb  dSS                                OnReceivePledgeInfo(PledgeInfo&)
1040a34c  dddddddddSd                        OnReceivePledgeInfo(PledgeInfo&)
1040a5d6  ddddddddd                          OnReceivePledgeInfoUpdate(PledgeInfo&)
### PledgeMember
103ec907  Sddddd                             OnGMViewAddPledgeMember(L2ParamStack&)
103f0853  Sddddd                             OnUpdatePledgeMember(L2ParamStack&)
103f0983  Sddddd                             OnAddPledgeMember(L2ParamStack&)
### Ally
103f2656  dSSS                               OnReceiveAskJoinAlly(L2ParamStack&)
### SellList
103eac91  ddh                                OnStartSellList(INT, INT)
103ead4b  dddhh                              OnAddSellList(ItemInfo&)
103ead82  dhhh                               OnAddSellList(ItemInfo&)
103eb2f1  ddh                                OnStartManorSellList(INT, INT)
103eb3ab  dddhh                              OnAddManorSellList(ItemInfo&)
103eb3e2  dhhh                               OnAddManorSellList(ItemInfo&)
### BuyList
103eaeb1  ddh                                OnStartBuyList(INT, INT)
103eaf6b  dddhh                              OnAddBuyList(ItemInfo&)
103eafa2  dhhh                               OnAddBuyList(ItemInfo&)
103eb0d1  ddh                                OnStartManorBuyList(INT, INT)
103eb18b  dddhh                              OnAddManorBuyList(ItemInfo&)
103eb1c2  dhhh                               OnAddManorBuyList(ItemInfo&)
103f24a0  ddd                                OnStartPrivateStoreBuyList()
103f2547  ddhddhdhdd                         OnPrivateStoreBuyList(ItemInfo&, INT, INT)

## Clan: o que foi confirmado numa sessão real (07/10/2026)
Servidor Lucera (core Interlude, `Lucera-Interlude-FULL-main`) com o cliente C2 standalone:
* **Lojas de venda** (`PrivateStoreManageListSell` 0x9A e `PrivateStoreListSell` 0x9B): o servidor escrevia o `packageSale` entre o dono
  e o adena; o C2 não tem esse campo. Removido nos dois: a loja abre.
* **Lista do clan** (`PledgeShowMemberListAll` 0x53): o handler do cliente C2 (Engine 0x1040a280) lê, nesta ordem, `d clanId, S nome,
  S líder`, `9 x d, S aliado, d crestAliado`, depois `d quantidade` e a quantidade de `S nome + 5 x d`. Se `clanId == 0` ou o nome vem
  vazio ele descarta o pacote inteiro e NÃO chama `ReceivePledgeInfo` (é o que mantinha a aba Clan apagada: o cabeçalho do C4/C5
  `d d d S S` virava `clanId = 0`). Com o cabeçalho C2 a aba liga e a janela mostra nome e nível.
* **Ainda aberto (a lista de membros não aparece):** depois do `crestAliado` o cliente C2 lê a quantidade de membros, mas o
  servidor ainda escreve o `atWar` ali (campo que a referência C2_L2J tem e este cliente de 03/03/2005 não lê): o cliente toma o
  `atWar` (0) como a quantidade. A correção é tirar o `writeD(atWar)` do `PledgeShowMemberListAll` (e conferir com o dump hexadecimal
  `PACKET_HEX` que o servidor agora imprime para os pacotes `Pledge*`).
* A referência `C2_L2J_decompiled` é de uma revisão anterior do C2: quando ela e o cliente discordam, vale o cliente
  (`tools/c2/packet_formats.py` e o disassembly do handler).
* Ferramentas desta sessão: `NWindow_crash.log` (registrador de falhas do standalone), `NWindow_trace.log` (`NW_TRACE`, só no
  standalone), `tools/port/map_addr.py`, `tools/c2/packet_formats.py`.
