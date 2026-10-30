# C3 pacotes servidor -> cliente: opcode -> formato (pista T1)

> Nota da publicação: esta nota fazia parte de uma auditoria de pacotes cujo lado do servidor (código Java, as notas das
> outras pistas e as ferramentas delas) não é publicado; as referências a elas ficam só como histórico.

Fonte da verdade: o `Engine.dll` do cliente C3 (`Lineage II - Chronicle 3 - Rise of Darkness\system\Engine.dll`, imagem em
`0x10300000`), lido por disassembly (capstone) e por emulação instrumentada (unicorn). Nada foi executado no jogo.
Ferramenta: `tools/c3/s2c_formats.py` (reexecutável, `--help`, `--json`, `--opcode`). Dados: `analysis/c3_s2c_formats.json`.

Resultado: 255 opcodes primários (0x00..0xFE) + 19 estendidos (`FE:0001..FE:0013`) + 13 handlers do estado de login. Dos
pacotes de jogo, certeza alta em 270, média em 3, baixa em 1 (ver seção 10).

## 1. Achados que importam (leia isto primeiro)

1. **A tabela de despacho do Engine traz o NOME de cada pacote e o opcode.** O `History: ... Function Name=PrivateStoreMan` do
   `NWindowDecomp.log` é o nome dessa tabela truncado em 15 caracteres: o pacote que derrubou o cliente é o **0x9A
   `PrivateStoreManageList`** (único nome com esse prefixo). O formato dele é `ddd` + N x `ddddhhhdd` + `d` + M x `ddddhhhddd`
   (3 dwords: dono, adena e N; o servidor Interlude manda `dono, packageSale, adena, N`: o cliente toma o adena como N e lê
   além do fim: GPF no caso `d` do interpretador, `Engine.dll+0EC053`, exatamente o endereço do log).
2. **O opcode (e o sub-opcode) NÃO fazem parte do formato.** O `Tick` consome o cabeçalho e passa ao handler só o payload. Todos os
   formatos abaixo começam depois do opcode. No fio: `[len:2][opcode:1]` e, se o opcode é `0xFE`, `[subopcode:2]` antes do payload.
3. **O interpretador de formato não tem arrays.** Listas são laços DO HANDLER que chamam o interpretador de novo. Por isso o
   `fmt` do JSON usa uma notação própria com contagens (seção 4); `min_bytes` é o menor payload que NÃO faz o cliente ler além do fim.
4. **Pacote mais curto que o formato = GPF; mais longo = tolerado** (o handler não conhece o tamanho do pacote: só o interpretador
   anda no buffer). Contagem de lista maior que os itens enviados = leitura além do fim = GPF.
5. **A especificação (HTML do netpro) NÃO traz tipos de campo**: só `opcode nome` (links para páginas que não estão no arquivo). O
   briefing dizia o contrário. Os formatos vêm só do Engine.
6. **Há 19 pacotes estendidos** (`0xFE` + word `0x0001..0x0013`, nomes `ExRegenMax`, `ExEventMatch*`, `ExPartyRoomMember`,
   `ExAutoSoulShot`...), a spec só lista `FE:0001`. O `0xFE` da tabela primária é `DummyPacket` (não faz nada).
7. **Caudas que dependem da versão de protocolo.** `CharInfo`, `UserInfo` e `NpcInfo` leem campos extras no fim SE a versão de
   protocolo que o cliente mandou no 1o pacote (`cdb`, global `0x108E6290`) for `> 0x1A4` (420). No C3 a versão é 530 (a spec
   mostra `[530]`), então as caudas SÃO lidas e já estão nos formatos publicados (a ferramenta emula 530; `--protocol`).
8. **Handlers que não fazem nada** (`xor al,al; ret`, 3 bytes): `14` LoginFail, `1F` TradeStartOk, `57` MagicList, `6B` SetPledgeCrest, `75` TradePressOwnOk, `78` FriendAdd, `79` FriendRemove, `7A` FriendList, `7B` FriendStatus, `7C` TradePressOtherOk, `9E` ReviveRequest, `9F` AbnormalVisualEffect, `AD` SetAllianceCrest, `AF` ServerCloseSocket, `B4` AllianceInfo, `BD` ReplyStartAllianceWar, `BF` ReplyStopAllianceWar, `C0` SurrenderAllianceWar, `C6` BlockList, `FE:0002` ExEventMatchList, `FE:0011` ExEventMatchLockResult. O cliente C3 ignora esses pacotes (inclui `ReviveRequest`
   0x9E e `AbnormalVisualEffect` 0x9F, que a spec lista). Mandá-los é inofensivo e inútil.
9. **`PartyMemberPosition` (0xA7) é perigoso**: por entrada o cliente lê o id do objeto e SÓ lê `x,y,z` se o objeto existe no mapa
   dele; entradas de objetos desconhecidos não consomem os 12 bytes e o resto desalinha. Não mandar.
10. **O `FL2NetNotify.vt.inc` está certo para o C3** (403 slots; o C2 tinha 352). Conferi o vtable `??_7FL2NetNotify@@6B@`
    (RVA `0x467308`) slot a slot com a tabela de exportação do `Engine.dll`: coincide em 402 dos 403 (o slot 297,
    `UpdatePetInventoryItem`, não tem export; o nome vem do `.vt.inc`). O erro do `tools/c2/packet_formats.py` era só a
    heurística de ligar formato a chamada (pegava a primeira chamada virtual cujo offset coincidia com um slot).
11. **Quem implementa o FL2NetNotify é a `UGameEngine`** (o vtable dela tem os mesmos 403 slots; `[[G+0x48]]` com `G=*(0x1097B408)`).
    Os métodos do próprio `FL2NetNotify` no Engine são stubs (`xor eax,eax; ret N`). `UGameEngine::OnX` repassa ao console na
    primeira chamada virtual; o offset dela, procurado em `db/functions.csv` (`vslot N (+0xOFF)`), dá o `NConsoleWnd::...`.
    Foi assim que preenchi a coluna `NC::` (evidência, não palpite; vazio quando o override trabalha no mundo e não no console).

## 2. Método

1. **Dispatch.** `UNetworkHandler::Tick` (VA `0x10405B10`) pega um pacote (byte do opcode em `+0`, word do sub-opcode em `+2`, ponteiro
   do payload em `+8`) e, no estado de jogo (`engine+0x44 == 2`), chama `[0x108E6318 + opcode*0x104]` (tabela primária) ou, para
   `0xFE`, `[0x108F6618 + subopcode*0x104]` (tabela estendida). No estado de login (`engine+0x44 == 1`) usa uma tabela de 13
   ponteiros em `0x108E62C4`. Cada entrada de 0x104 bytes é `{dword handler; wchar nome[]}`; as tabelas são preenchidas na
   inicialização por código `mov [tabela+n*0x104], thunk` (e a cópia do nome logo antes), então a ferramenta recupera os dois
   varrendo o código atrás desses `mov` (a entrada 0 é estática no `.data`). O handler é chamado como `handler(this, payload)`.
2. **Formatos.** O handler chama o interpretador (`0x103EBFF0`, via thunk `0x1030537B`) com `reader(unused, cursor, formato, out...)`,
   que devolve o cursor novo. A string de formato fica em `.rdata`; a ferramenta lê o formato exato de cada chamada.
3. **Estrutura (laços, condições).** Não dá para ler só pelo disassembly (270 handlers). Emulo cada handler (unicorn): o código real
   roda com um engine falso, o interpretador é reimplementado em Python, e todas as chamadas fora do handler são substituídas
   (stubs; a convenção de limpeza da pilha vem do `ret N` do chamado, da tabela de importações e de regras para `GMalloc`,
   `Logf` etc.). Roda-se uma linha de base (todo campo numérico vale 2) e uma execução por campo perturbado (valores 0, 1, 3, as
   constantes de `cmp` do próprio handler e 0..12 para campos que parecem tipo). Um campo cuja mudança altera quantas vezes um
   ponto de leitura executa é contagem de laço (`[#n:...]`) ou condição (`{#n=...:...}`).
4. **Conferência cruzada.** Para cada handler comparei os pontos de leitura que a emulação alcançou com TODOS os pontos de leitura
   achados estaticamente (273 handlers de jogo). Sobraram sem alcançar só `A7` e `C9` (condições de dois campos / consulta ao mapa),
   que corrigi à mão lendo o código; qualquer leitura condicional escondida em outro handler apareceria nessa conferência.
5. **Notify.** Slot -> nome: vtable real do `Engine.dll` + exportações (seção 1, item 10); slot -> `NConsoleWnd`: seção 1, item 11.
6. **Spec.** `C3 Rise Of Darkness.html`: lista `opcode nome` (255 itens), comparada com os nomes do Engine (seção 8).

## 3. O alfabeto do formato (decodificado do interpretador, `Engine.dll+0EC000`)

Função `0x103EBFF0`; despacho por `char - 'S'` numa tabela de bytes (`0x103EC168`) + tabela de saltos (`0x103EC148`).

| letra | handler (VA / RVA) | bytes lidos do pacote | detalhe |
|---|---|---|---|
| `c` | `0x103EC02F` / `+EC02F` | 1 | byte |
| `h` | `0x103EC03F` / `+EC03F` | 2 | word little-endian (`mov dx,[ebx]`) |
| `d` | `0x103EC053` / `+EC053` | 4 | dword (`mov ecx,[ebx]`; é o endereço do crash do log) |
| `f` | `0x103EC065` / `+EC065` | 8 | 8 bytes copiados (um `double`) |
| `s` | `0x103EC07D` / `+EC07D` | len+1 | string de 8 bits terminada em NUL; argumentos (tamanho, destino); `strncpy` e NUL no destino; avança até o NUL, ou `tamanho` bytes se a string for mais longa |
| `S` | `0x103EC0AE` / `+EC0AE` | 2 x (len+1) | string larga (UTF-16LE) terminada em `0x0000`; argumentos (tamanho do destino em bytes, destino); copia no máximo `tamanho/2-1` caracteres e termina o destino; o cursor avança a string inteira (len medido com `wcslen`) |
| `b` | `0x103EC0F4` / `+EC0F4` | N | bloco cru; **N não está no formato nem no pacote**: o handler passa N (um campo lido antes, ou constante). Notação: `b<#n>` (N = n-ésima letra) ou `b<16>` (fixo) |
| outra | `0x103EC116` | 0 | grava um erro no log (`0x10313C82`) e não lê nada |

Observações para contar bytes: não existe alinhamento, nem prefixo de tamanho em `S`/`s` (o terminador é o fim), nem `[`/`(`/dígitos
no formato (os `[#n:...]` do JSON são nossa notação para os laços do handler). `S` vazia ocupa 2 bytes, `s` vazia 1. O interpretador
nunca sabe o tamanho do pacote: não há checagem de fim de buffer.

## 4. Notação do campo `fmt` (JSON e tabelas)

* letras = campos na ordem do fio (`c h d f S s b`), sem o opcode.
* `[#n:corpo]` = o corpo se repete N vezes, N = valor da n-ésima letra do MESMO nível de aninhamento (1-based, não conta o
  conteúdo dos colchetes). `#^n` = n-ésima letra de um nível acima.
* `{#n=a,b:corpo}` / `{#n!=a:corpo}` / `{#n=a..b:corpo}` = o corpo só existe se a letra n tem (ou não tem) esses valores
  (testados 0..12 e as constantes do handler; fora desse conjunto o comportamento não foi sondado).
* `{#2=1&#9=0:...}` e `{objectId known to the client:...}` aparecem só nas entradas manuais (nota no JSON).
* `b<#n>` / `b<N>`: bloco cru de N bytes.
* `min_bytes` = bytes lidos com todo laço em 0 e toda condição falsa (o piso: payload menor que isso garante leitura além do fim).
* Convenção do JSON: `fmt` NÃO inclui o opcode (`fmt_has_opcode: false`); `wire_fmt` = `prefix` + `fmt` (`c` ou `ch`), isto é, o que o fio carrega depois do tamanho de 2 bytes.
* Campos extras do JSON: `engine_name` (nome da tabela do Engine), `prefix` (`c` ou `ch` no fio), `handler_va/rva/size`, `notify`
  (1a chamada executada) e `notifies` (todas, com slot, offset no vtable, assinatura, `console_vslot_off`, `nwindow`), `loops`,
  `min_bytes`, `emulated`, `state` (`game` ou `login`) e, no `0E`, `switch_notify`.
* `certainty`: **alta** = emulação limpa (terminou, pilha correta) e todos os pontos de leitura estáticos alcançados; **média** =
  correção manual lida no código ou emulação incompleta sem leitura faltando; **baixa** = leitura faltando ou não auditado.

## 5. Pacotes prioritários (o que o cliente C3 lê, byte a byte)

Tamanhos: `c`=1 `h`=2 `d`=4 `f`=8, `S`>=2. "item" = bytes por elemento de lista.

| opcode | pacote (Engine) | formato do payload | mínimo | item(ns) | notify -> NConsoleWnd |
|---|---|---|---|---|---|
| 9A | PrivateStoreManageList (venda, dono) | `ddd[#3:ddddhhhdd]d[#4:ddddhhhddd]` | 16 | 30 e 34 | StartPrivateStoreManageList, PrivateStoreManageList |
| 9B | PrivateStoreList (venda, comprador vê) | `ddd[#3:ddddhhhddd]` | 12 | 34 | StartPrivateStoreList, PrivateStoreList |
| 9C | PrivateStoreMsg (venda) | `dS` (destino de 0x400 bytes) | 6 | - | (grava no objeto do jogador, sem notify) |
| B7 | PrivateStoreBuyManageList | `ddd[#3:dhddhdh]d[#4:dhddhdhdd]` | 16 | 22 e 30 | StartPrivateStoreBuyManageList, PrivateStoreBuyManageList |
| B8 | PrivateStoreBuyList | `ddd[#3:ddhddhdhdd]` | 12 | 34 | StartPrivateStoreBuyList, PrivateStoreBuyList |
| B9 | PrivateStoreBuyMsg | `dS` | 6 | - | (sem notify) |
| 2F | ChangeWaitType | `ddddd` | 20 | - | OnChangeWaitType (mundo, não o console) |
| 2E | ChangeMoveType | `ddd` | 12 | - | OnChangeMoveType |
| 29 | TargetSelected | `ddddd` | 20 | - | OnTargetSelected |
| 2A | TargetUnselected | `dddd` | 16 | - | OnTargetUnselected |
| A6 | MyTargetSelected | `dh` | 6 | - | OnMyTargetSelected -> NC::MyTargetSelected |
| 25 | ActionFail | (nada) | 0 | - | OnActionFail |
| 0E | StatusUpdate | `dd[#2:dd]` | 8 | 8 | um OnXUpdate por atributo (tabela abaixo) |
| 04 | UserInfo | ver JSON (`dddddS...`, cauda de versão incluída) | 427 | lista `h` | OnUserInfo (+OnEquipItem, Enable*Chat) |
| 03 | CharInfo | ver JSON | 235 | lista `h` | OnCharInfo (+OnEquipItem) |
| 16 | NpcInfo | `d x18 ffff ddd ccccc SS d x8 cc` | 159 | - | OnNpcInfo |
| 1B | ItemList | `hh[#2:hdddhhhdhh]` | 4 | 28 | OnReceiveItemList -> AddInventoryItem |
| 27 | InventoryUpdate | `h[#1:hhdddhhhdhh]` | 2 | 30 | OnReceiveUpdateItemList |
| 64 | SystemMessage | `dd[#2:d{#1=4:dd}{#1=0:S}{#1=1,2,3,5:d}]` | 8 | 4/8/2+ | OnAddSystemMessageParam -> NC::AddSystemMessageParam |
| 4A | Say2 | `dd{#2=11:dd}{#2!=11:SS}` | 8 | - | OnSay2 -> NC::Say2 |
| 58 | SkillList | `d[#1:ddd]` | 4 | 12 | OnShowSkillWnd, OnSkillListPacket |
| 48 | MagicSkillUse | `dddddddddh[#10:h]` | 38 | 2 | OnReceiveMagicSkillUse |
| 05 | Attack | `dddcdddh[#8:ddc]` | 27 | 9 | OnAttack |
| 1E | TradeStart | `dh[#2:hdddhhdhhh]` | 6 | 28 | OnReceiveTradeStart |
| 4E | PartySmallWindowAll | `ddd[#3:dSdddddddd]` | 12 | 38+ | OnPartyAddMemberPacket |
| 4F | PartySmallWindowAdd | `dddSdddddddd` | 46 | - | OnPartyAddMemberPacket |
| 50 | PartySmallWindowDeleteAll | (nada) | 0 | - | OnPartyDeleteAllPacket |
| 51 | PartySmallWindowDelete | `dS` | 6 | - | OnPartyDeleteMemberPacket |
| 52 | PartySmallWindowUpdate | `dSdddddd` | 30 | - | OnPartyUpdateMemberPacket |
| 53 | PledgeShowMemberListAll | `dSS{#1!=0:dddddddddSddd[#13:Sddddd]}` | 8 | 22+ | OnReceivePledgeInfo, OnAddPledgeMember |
| 54/55 | PledgeShowMemberListUpdate/Add | `Sddddd` | 22 | - | OnUpdatePledgeMember / OnAddPledgeMember |
| 56 | PledgeShowMemberListDelete | `S` | 2 | - | OnDeletePledgeMember |
| 45 | ShortCutInit | `d[#1:dd{#1=4:d}{#1=3:d}{#1=1:d}{#1=2:dd}d]` | 4 | 12..16 | OnReceiveShortCutList, AddShortCutItem |

Notas:
* `PledgeShowMemberListAll`: se o 1o `d` (clanId) é 0 o cliente ignora o resto (mesma regra do C2); depois dos 9 `d` do cabeçalho vêm
  `S` (aliança), `d`, `d` e **`d` = quantidade** (a letra #13), e então a lista de `S + 5 x d`. São DOIS `d` entre o nome da aliança e a
  quantidade (o `atWar` do L2J está entre eles), não um.
* `PledgeShowMemberListAll` (53) e `GMViewPledgeInfo` (90) terminam em listas de `Sddddd`; os `dddddddddSdd`(+`d`) são o cabeçalho de clã.
* `Say2`: tipo 11 (barco/sistema) lê `dd` (dois índices de tabela do cliente); qualquer outro tipo lê `SS` (nome, texto).
* `SystemMessage`: por parâmetro, `d` tipo; tipo 0 = `S`, 1/2/3/5 = `d`, 4 = `dd`; tipos 6..12 não lêem nada.
* `ShortCutRegister` (44) é o mesmo item de 45: `dd{#1=4:d}{#1=3:d}{#1=1:d}{#1=2:dd}d` (tipo, slot, dados do tipo, `d` final).
* `StatusUpdate` (0E): id do atributo -> notify do cliente C3 (conferido por emulação): 0 Class, 1 Level, 2 Exp, 3 STR, 4 DEX, 5 CON,
  6 INT, 7 WIT, 8 MEN, 9 HP, 10 MaxHP, 11 MP, 12 MaxMP, 13 SP, 14 CarryWeight(atual), 15 CarryWeight(max), 16 AttackRange,
  17 PAtk, 18 PAtkSpd, 19 PDef, 20 Evasão, 21 Precisão, 22 Crítico, 23 MAtk, 24 MCastSpd, 25 MDef, 26 Guilty(PvP flag), 27 CriminalRate
  (karma), 33 CP, 34 MaxCP. Outros ids não fazem nada.
* `ItemList`/`InventoryUpdate`/`TradeStart`...: o primeiro `h` do item é o tipo/flag; ver o JSON para o formato exato. As listas de
  itens de lojas (`9A`, `9B`, `B7`, `B8`) NÃO têm o campo `packageSale` do Interlude.

## 6. Tabela completa: opcodes primários

Colunas: opcode (hex) | nome na spec | nome no Engine (sem o sufixo `Packet`) | formato | RVA do handler | primeira chamada FL2NetNotify
(`NC::` = `NConsoleWnd::`) | certeza. Opcodes que o Engine não tem na tabela: `0xFF` (sem handler: chamada nula se chegar) e os `Dummy` abaixo.

| opcode | spec (netpro) | nome no Engine | formato (payload, sem o opcode) | handler (RVA) | notify -> NConsoleWnd | certeza |
|---|---|---|---|---|---|---|
| 00 | VersionCheck | - | `cddd` | 0xF63E0 | OnReceiveVersionCheck -> NC::ReceiveVersionCheck | média |
| 01 | MoveToLocation | MoveToLocation | `ddddddd` | 0x108500 | OnMoveToLocation | alta |
| 02 | NpcSay | NpcSay | `dddS` | 0xF65F0 | OnNpcSay -> NC::NpcSay | alta |
| 03 | CharInfo | CharInfo | `dddddSddddddddddddddddddddddddddddffffdddSdddddccccccch[#55:h]cdchdddccd` | 0x1162A0 | OnEquipItem | alta |
| 04 | UserInfo | UserInfo | `dddddSdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddffffddddSdddddcccddh[#97:h]cdchhdhddddccd` | 0x1176E0 | OnUserInfo | alta |
| 05 | Attack | Attack | `dddcdddh[#8:ddc]` | 0x108E70 | OnAttack | alta |
| 06 | Die | Die | `ddddddd` | 0x1097E0 | OnDie | alta |
| 07 | Revive | Revive | `d` | 0x109B00 | OnRevive | alta |
| 08 | AttackOutofRange | AttackOutofRange | `(nada)` | 0x109BF0 | OnAttackOutofRange | alta |
| 09 | AttackinCoolTime | AttackinCoolTime | `(nada)` | 0x109CC0 | OnAttackinCoolTime | alta |
| 0A | AttackDeadTarget | AttackDeadTarget | `(nada)` | 0x109D90 | OnAttackDeadTarget | alta |
| 0B | SpawnItem | SpawnItem | `dddddddd` | 0x110AC0 | OnSpawnItem | alta |
| 0C | DropItem | DropItem | `ddddddddd` | 0x110D80 | OnDropItem | alta |
| 0D | GetItem | GetItem | `ddddd` | 0x109E60 | OnGetItem | alta |
| 0E | StatusUpdate | StatusUpdate | `dd[#2:dd]` | 0x10A350 | OnExpUpdate | alta |
| 0F | NpcHtmlMessage | NpcHtmlMessage | `dSd` | 0x10A980 | OnNpcHtmlMessage | alta |
| 10 | SellList | SellList | `ddh[#3:hdddhhdhhhd]` | 0xF7440 | OnStartSellList -> NC::StartSellList | alta |
| 11 | BuyList | BuyList | `ddh[#3:hdddhhdhhhd]` | 0xF7610 | OnStartBuyList | alta |
| 12 | DeleteObject | DeleteObject | `dd` | 0x1193F0 | OnDeleteUser | alta |
| 13 | CharacterSelectionInfo | CharacterSelectionInfo | `d[#1:SdSddddddddddffddddddddddddddddddddddddddddddddddddddddddddddddffdddc]` | 0xF7C20 | OnReceiveCharacterInfo | alta |
| 14 | LoginFail | LoginFail | `(nada)` | 0xEA870 | - | alta |
| 15 | CharacterSelected | CharacterSelected | `SdSddddddddddffdddddddddddddddddddddddddddddddddddddddddddddb<#60>ddddddddd` | 0xF80D0 | OnReceiveCharacterSelectedPacket | alta |
| 16 | NpcInfo | NpcInfo | `ddddddddddddddddddffffdddcccccSSddddddddcc` | 0x119DF0 | OnNpcInfo | alta |
| 17 | NewCharacterSuccess | NewCharacterSuccess | `d[#1:dddddddddddddddddddd]` | 0xF97D0 | OnAcceptNewCharacter | alta |
| 18 | NewCharacterFail | NewCharacterFail | `(nada)` | 0xEA8E0 | OnAcceptNewCharacter | alta |
| 19 | CharacterCreateSuccess | CharacterCreateSuccess | `(nada)` | 0xEA910 | OnAcceptCharacterCreate | alta |
| 1A | CharacterCreateFail | CharacterCreateFail | `d` | 0xF9940 | OnCharacterCreateFail | alta |
| 1B | ItemList | ItemList | `hh[#2:hdddhhhdhh]` | 0xF99D0 | OnReceiveItemList | alta |
| 1C | SunRise | SunRise | `(nada)` | 0xEA940 | OnReceiveServerTime | alta |
| 1D | SunSet | SunSet | `(nada)` | 0xEA970 | OnReceiveServerTime | alta |
| 1E | TradeStart | TradeStart | `dh[#2:hdddhhdhhh]` | 0xF9C00 | OnReceiveTradeStart | alta |
| 1F | TradeStartOk | TradeStartOk | `(nada)` | 0xEAA60 | - | alta |
| 20 | TradeOwnAdd | TradeOwnAdd | `h[#1:hdddhhdhhh]` | 0xF9DD0 | OnReceiveAddTradeOwnItem | alta |
| 21 | TradeOtherAdd | TradeOtherAdd | `h[#1:hdddhhdhhh]` | 0xF9F60 | OnReceiveAddTradeOtherItem | alta |
| 22 | TradeDone | TradeDone | `d` | 0xFA6F0 | OnReceiveTradeDoneMsg | alta |
| 23 | CharacterDeleteSuccess | CharacterDeleteSuccess | `(nada)` | 0xEAA80 | OnAcceptCharacterDelete | alta |
| 24 | CharacterDeleteFail | CharacterDeleteFail | `d` | 0xFA780 | OnAcceptCharacterDelete | alta |
| 25 | ActionFail | ActionFail | `(nada)` | 0xEAAD0 | OnActionFail | alta |
| 26 | ServerClose | SeverClose | `(nada)` | 0xEAB20 | OnServerClose | alta |
| 27 | InventoryUpdate | InventoryUpdate | `h[#1:hhdddhhhdhh]` | 0xFA810 | OnReceiveUpdateItemList | alta |
| 28 | TeleportToLocation | TeleportToLocation | `ddddd` | 0x10B310 | OnTeleportToLocation | alta |
| 29 | TargetSelected | TargetSelected | `ddddd` | 0x10B4A0 | OnTargetSelected | alta |
| 2A | TargetUnselected | TargetUnselected | `dddd` | 0x10B920 | OnTargetUnselected | alta |
| 2B | AutoAttackStart | AutoAttackStart | `d` | 0x10BA80 | OnAutoAttackStart | alta |
| 2C | AutoAttackStop | AutoAttackStop | `d` | 0x10BB70 | OnAutoAttackStop | alta |
| 2D | SocialAction | SocialAction | `dd` | 0x10BC60 | OnSocialAction | alta |
| 2E | ChangeMoveType | ChangeMoveType | `ddd` | 0x10BD80 | OnChangeMoveType | alta |
| 2F | ChangeWaitType | ChangeWaitType | `ddddd` | 0x10BEB0 | OnChangeWaitType | alta |
| 30 | ManagePledgePower | ManagePledgePower | `dd` | 0x101C30 | OnPledgePower -> NC::PledgePower | alta |
| 31 | CreatePledge | CreatePledge | `d` | 0xFAA20 | OnReceiveCreatePledge -> NC::ReceiveCreatePledge | alta |
| 32 | AskJoinPledge | AskJoinPledge | `dS` | 0xFAB10 | OnReceiveAskJoinPledge -> NC::ReceiveAskJoinPledge | alta |
| 33 | JoinPledge | JoinPledge | `d` | 0xFAC30 | OnReceiveJoinPledge -> NC::ReceiveJoinPledge | alta |
| 34 | WithdrawalPledge | WithdrawalPledge | `d` | 0xFAD20 | OnReceiveWidthdrawalPledge -> NC::ReceiveWithdrawalPledge | alta |
| 35 | OustPledgeMember | OustPledgeMember | `d` | 0xFAE10 | OnReceiveOustPledgeMember -> NC::ReceiveOustPledgeMember | alta |
| 36 | SetOustPledgeMember | SetOustPledgeMember | `(nada)` | 0xEAB90 | OnReceiveOustPledgeMessage -> NC::ReceiveOustPledgeMessage | alta |
| 37 | DismissPledge | DismissPledge | `d` | 0xFAF00 | OnReceiveDismissPledge -> NC::ReceiveDismissPledge | alta |
| 38 | SetDismissPledge | SetDismissPledge | `(nada)` | 0xEAC50 | OnReceiveOustPledgeMessage -> NC::ReceiveOustPledgeMessage | alta |
| 39 | AskJoinParty | AskJoinParty | `Sd` | 0xFB840 | OnReceiveAskJoinParty -> NC::ReceiveAskJoinParty | alta |
| 3A | JoinParty | JoinParty | `d` | 0xFB970 | OnReceiveJoinParty -> NC::ReceiveJoinParty | alta |
| 3B | WithdrawalParty | WithdrawalParty | `d` | 0xFBA60 | OnReceiveWithdrawalParty -> NC::ReceiveWithdrawalParty | alta |
| 3C | OustPartyMember | OustPartyMember | `d` | 0xFBB50 | OnReceiveOustPartyMember -> NC::ReceiveOustPartyMember | alta |
| 3D | SetOustPartyMember | SetOustPartyMember | `(nada)` | 0xEAD30 | OnReceiveOustPartyMessage -> NC::ReceiveOustPartyMessage | alta |
| 3E | DismissParty | DismissParty | `d` | 0xFBC40 | OnReceiveDismissParty -> NC::ReceiveDismissParty | alta |
| 3F | SetDismissParty | SetDismissParty | `(nada)` | 0xEADF0 | OnRecevieDismissPartyMessage -> NC::ReceiveDismissPartyMessage | alta |
| 40 | MagicAndSkillList | MagicAndSkillList | `ddd` | 0xFBD30 | OnReceiveMagicSkillListPacket -> NC::ReceiveMagicSkillList | alta |
| 41 | WareHouseDepositList | WareHouseDepositList | `dh[#2:hdddhhdhhhd]` | 0xFBE60 | OnStartDepositList -> NC::StartDepositList | alta |
| 42 | WareHouseWithdrawList | WareHouseWithdrawList | `dh[#2:hdddhhdhhhd]` | 0xFC010 | OnStartWithdrawList -> NC::StartWithdrawList | alta |
| 43 | WareHouseDone | WareHouseDone | `d` | 0xFC1C0 | OnReceiveWareHouseDonePacket -> NC::ReceiveWareHouseDone | alta |
| 44 | ShortCutRegister | ShortCutRegister | `dd{#1=4:d}{#1=3:d}{#1=1:d}{#1=2:dd}d` | 0xFC2B0 | AddShortCutItem -> NC::AddShortCutItem | alta |
| 45 | ShortCutInit | ShortCutInit | `d[#1:dd{#1=4:d}{#1=3:d}{#1=1:d}{#1=2:dd}d]` | 0xFC550 | OnReceiveShortCutList -> NC::ReceiveShortCutList | alta |
| 46 | ShortCutDelete | ShortCutDelete | `d` | 0xFC870 | OnReceiveShortCutDelete -> NC::ReceiveShortCutDelete | alta |
| 47 | StopMove | StopMove | `ddddd` | 0x10C020 | OnStopMove | alta |
| 48 | MagicSkillUse | MagicSkillUse | `dddddddddh[#10:h]` | 0x10CB30 | OnReceiveMagicSkillUse | alta |
| 49 | MagicSkillCanceled | MagicSkillCanceled | `d` | 0x10D070 | OnReceiveMagicSkillCanceled | alta |
| 4A | Say2 | SayPacket2 | `dd{#2=11:dd}{#2!=11:SS}` | 0xFC9C0 | OnSay2 -> NC::Say2 | alta |
| 4B | EquipUpdate | EquipUpdate | `ddd` | 0xFCC10 | OnUpdateEquipItem -> NC::UpdateEquipItem | alta |
| 4C | DoorInfo | DoorInfo | `dd` | 0x112C90 | OnDoorInfo | alta |
| 4D | DoorStatusUpdate | DoorStatusUpdate | `ddddd` | 0x10C330 | OnDoorStatusUpdate | alta |
| 4E | PartySmallWindowAll | PartySmallWindowAll | `ddd[#3:dSdddddddd]` | 0x11B2D0 | OnPartyAddMemberPacket -> NC::PartyAddMember | alta |
| 4F | PartySmallWindowAdd | PartySmallWindowAdd | `dddSdddddddd` | 0x11B630 | OnPartyAddMemberPacket -> NC::PartyAddMember | alta |
| 50 | PartySmallWindowDeleteAll | PartySmallWindowDeleteAll | `(nada)` | 0x11B860 | OnPartyDeleteAllPacket -> NC::PartyDeleteAll | alta |
| 51 | PartySmallWindowDelete | PartySmallWindowDelete | `dS` | 0x11B930 | OnPartyDeleteMemberPacket -> NC::PartyDeleteMember | alta |
| 52 | PartySmallWindowUpdate | PartySmallWindowUpdate | `dSdddddd` | 0xFD180 | OnPartyUpdateMemberPacket -> NC::PartyUpdateMember | alta |
| 53 | PledgeShowMemberListAll | PledgeShowMemberListAll | `dSS{#1!=0:dddddddddSddd[#13:Sddddd]}` | 0x11AE70 | OnReceivePledgeInfo -> NC::ReceivePledgeInfo | alta |
| 54 | PledgeShowMemberListUpdate | PledgeShowMemberListUpdate | `Sddddd` | 0xFCD40 | OnUpdatePledgeMember -> NC::UpdatePledgeMember | alta |
| 55 | PledgeShowMemberListAdd | PledgeShowMemberListAdd | `Sddddd` | 0xFCE70 | OnAddPledgeMember -> NC::AddPledgeMember | alta |
| 56 | PledgeShowMemberListDelete | PledgeShowMemberListDelete | `S` | 0xFCFA0 | OnDeletePledgeMember -> NC::DeletePledgeMember | alta |
| 57 | MagicList | MagicList | `(nada)` | 0xEAEE0 | - | alta |
| 58 | SkillList | SkillList | `d[#1:ddd]` | 0xFD330 | OnShowSkillWnd -> NC::ShowSkillWnd | alta |
| 59 | VehicleInfo | VehicleInfo | `ddddd` | 0x112F40 | OnVehicleInfo | alta |
| 5A | VehicleDeparture | VehicleDeparture | `dddddd` | 0x10C510 | OnVehicleDeparture | alta |
| 5B | VehicleCheckLocation | VehicleCheckLocation | `dddd` | 0xFC900 | - | alta |
| 5C | GetOnVehicle | GetOnVehicle | `ddddd` | 0x10C730 | OnGetOnVehicle | alta |
| 5D | GetOffVehicle | GetOffVehicle | `ddddd` | 0x10C960 | OnGetOffVehicle | alta |
| 5E | TradeRequest | TradeRequest | `d` | 0xFD4F0 | OnReceiveTradeDemand -> NC::ReceiveTradeDemand | alta |
| 5F | RestartResponse | RestartResponse | `dS` | 0x11BAA0 | OnRestartResponse | alta |
| 60 | MoveToPawn | MoveToPawn | `dddddd` | 0x108840 | OnMoveToPawn | alta |
| 61 | ValidateLocation | ValidateLocation | `ddddd` | 0x108B00 | OnValidateLocation | alta |
| 62 | StartRotating | StartRotating | `ddd` | 0x108C20 | OnStartRotating | alta |
| 63 | FinishRotating | FinishRotating | `dd` | 0x108D50 | OnFinishRotating | alta |
| 64 | SystemMessage | SystemMessage | `dd[#2:d{#1=4:dd}{#1=0:S}{#1=1,2,3,5:d}]` | 0xFD600 | OnAddSystemMessageParam -> NC::AddSystemMessageParam | alta |
| 65 | StartPledgeWar | StartPledgeWar | `SS` | 0xFAFF0 | OnReceiveStartPledgeWar -> NC::ReceiveStartPledgeWar | alta |
| 66 | ReplyStartPledgeWar | ReplyStartPledgeWar | `d` | 0xFB110 | OnReceiveReplyStartPledgeWar -> NC::ReceiveReplyStartPledgeWar | alta |
| 67 | StopPledgeWar | StopPledgeWar | `SS` | 0xFB200 | OnReceiveStopPledgeWar -> NC::ReceiveStopPledgeWar | alta |
| 68 | ReplyStopPledgeWar | ReplyStopPledgeWar | `d` | 0xFB320 | OnReceiveReplyStopPledgeWar -> NC::ReceiveReplyStopPledgeWar | alta |
| 69 | SurrenderPledgeWar | SurrenderPledgeWar | `d` | 0xFB410 | OnReceiveSurrenderPledgeWar -> NC::ReceiveSurrenderPledgeWar | alta |
| 6A | ReplySurrenderPledgeWar | ReplySurrenderPledgeWar | `d` | 0xFB500 | OnReceiveReplySurrenderPledgeWar | alta |
| 6B | SetPledgeCrest | SetPledgeCrest | `(nada)` | 0xEAD10 | - | alta |
| 6C | PledgeCrest | PledgeCrest | `dd` | 0xFB6E0 | OnMakeDDSCrestFile | alta |
| 6D | SetupGauge | SetupGauge | `ddd` | 0xFD8B0 | OnSetupGaugePacket | alta |
| 6E | ShowBoard | ShowBoard | `SSSSSSSSS` | 0xFD9E0 | OnShowboardPacket -> NC::ShowBoard | alta |
| 6F | ChooseInventoryItem | ChooseInventoryItem | `(nada)` | 0xEAF00 | OnSelectItemToEnchant -> NC::SelectItemToEnchant | alta |
| 70 | Dummy_70 | Dummy | `(nada)` | 0xEAF50 | - | alta |
| 71 | MoveToLocationInVehicle | MoveToLocationInVehicle | `dddddddd` | 0x10D160 | OnMoveToLocationInVehicle | alta |
| 72 | StopMoveInVehicle | StopMoveInVehicle | `dddddd` | 0x10D360 | OnStopMoveInVehicle | alta |
| 73 | ValidateLocationInVehicle | ValidateLocationInVehicle | `dddddd` | 0x10D520 | OnValidateLocationInVehicle | alta |
| 74 | TradeUpdate | TradeUpdate | `h[#1:hhdddhhdhhh]` | 0xFA0F0 | OnReceiveTradeUpdate | alta |
| 75 | TradePressOwnOk | TradePressOwnOk | `(nada)` | 0xEAA20 | - | alta |
| 76 | MagicSkillLaunched | MagicSkillLaunched | `dddd[#4:d]` | 0x10D730 | OnReceiveMagicSkillLaunched | alta |
| 77 | FriendAddRequestResult | FriendAddRequestResult | `d` | 0xFF210 | OnReceiveFriendAddRequestResult -> NC::ReceiveFriendAddRequestResult | alta |
| 78 | FriendAdd | FriendAdd | `(nada)` | 0xEB010 | - | alta |
| 79 | FriendRemove | FriendRemove | `(nada)` | 0xEB030 | - | alta |
| 7A | FriendList | FriendList | `(nada)` | 0xEB070 | - | alta |
| 7B | FriendStatus | FriendStatus | `(nada)` | 0xEB050 | - | alta |
| 7C | TradePressOtherOk | TradePressOtherOk | `(nada)` | 0xEAA40 | - | alta |
| 7D | FriendAddRequest | FriendAddRequest | `S` | 0xFF300 | OnReceiveFriendAddRequest -> NC::ReceiveFriendAddRequest | alta |
| 7E | LogOutOk | LogOutOk | `(nada)` | 0x10B250 | OnAcceptLogOut | alta |
| 7F | AbnormalStatusUpdate | AbnormalStatusUpdate | `h[#1:dhd]` | 0xFA2A0 | OnReceiveAbnormalStatus -> NC::ReceiveAbnormalStatus | alta |
| 80 | QuestList | QuestList | `h[#1:dd]h[#2:dddd]` | 0xFA450 | OnReceiveQuestList -> NC::ReceiveQuestList | alta |
| 81 | EnchantResult | EnchantResult | `d` | 0xFA650 | OnReceiveEnchantResult -> NC::ReceiveEnchantResult | alta |
| 82 | PledgeShowMemberListDeleteAll | PledgeShowMemberListDeleteAll | `(nada)` | 0xEAEB0 | OnDeleteAllPledgeMember -> NC::DeleteAllPledgeMember | alta |
| 83 | PledgeInfo | PledgeInfo | `dSS` | 0xFCFF0 | - | alta |
| 84 | PledgeExtendedInfo | PledgeExtendedInfo | `SddSddddddddSd` | 0xFD090 | OnReceivePledgeExtendedInfo | alta |
| 85 | SurrenderPersonally | SurrenderPersonally | `d` | 0xFB5F0 | OnReceiveSurrenderPersonally -> NC::ReceiveSurrenderPersonally | alta |
| 86 | Ride | Ride | `dddd` | 0x1099C0 | OnRide | alta |
| 87 | GiveNickNameDone | Dummy | `(nada)` | 0xEAF50 | - | alta |
| 88 | PledgeShowInfoUpdate | PledgeShowInfoUpdate | `ddddddddddSdd` | 0x11B190 | OnReceivePledgeInfoUpdate -> NC::ReceivePledgeInfoUpdate | alta |
| 89 | ClientAction | ClientAction | `d` | 0xFDC30 | OnReceiveAction -> NC::ReceiveAction | alta |
| 8A | AcquireSkillList | AcquireSkillList | `d[#1:ddddd]` | 0xFDCC0 | OnShowTrainListWnd -> NC::ShowTrainListWnd | alta |
| 8B | AcquireSkillInfo | AcquireSkillInfo | `dddd[#4:dddd]` | 0xFDEC0 | OnShowTrainInfoWnd -> NC::ShowTrainInfoWnd | alta |
| 8C | ServerObjectInfo | ServerObjectInfo | `ddSdddddffffdddd` | 0x112670 | OnServerObjectInfo | alta |
| 8D | GMHide | GMHide | `d` | 0xF85F0 | OnGMHide | alta |
| 8E | AcquireSkillDone | AcquireSkillDone | `(nada)` | 0xEA890 | OnHideTrainWnd -> NC::HideTrainWnd | alta |
| 8F | GMViewCharacterInfo | GMViewCharacterInfo | `dddddSdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddffffddddSdddcccddhhddd` | 0x10ABE0 | OnGMViewCharacterInfo -> NC::GMViewCharacterInfo | alta |
| 90 | GMViewPledgeInfo | GMViewPledgeInfo | `SdSS{#2!=0:dddddddddSdd}d[#5:Sddddd]` | 0xF8C30 | OnGMViewReceivePledgeInfo -> NC::GMViewReceivePledgeInfo | alta |
| 91 | GMViewSkillInfo | GMViewSkillInfo | `Sd[#2:ddd]` | 0xF8F00 | OnGMViewShowSkillWnd -> NC::GMViewShowSkillWnd | alta |
| 92 | GMViewMagicInfo | GMViewMagicInfo | `Sd[#2:ddd]` | 0xF9090 | OnGMViewShowMagicWnd | alta |
| 93 | GMViewQuestInfo | GMViewQuestInfo | `Sh[#2:dd]h[#3:dddd]` | 0xF9210 | OnGMViewReceiveQuestList -> NC::GMViewReceiveQuestList | alta |
| 94 | GMViewItemList | GMViewItemList | `Shh[#3:hdddhhhdhh]` | 0xF9410 | OnGMViewReceiveItemList -> NC::GMViewReceiveItemList | alta |
| 95 | GMViewWarehouseWithdrawList | GMViewWarehouseWithdrawList | `Sdh[#3:hdddhh{#1=0..3:dhhh}d]` | 0xF95D0 | OnGMViewStartWithdrawList -> NC::GMViewStartWithdrawList | alta |
| 96 | PartyMatchList | ListPartyWating | `dd[#2:dSddddd]` | 0xF8680 | OnStartPartyMatch | alta |
| 97 | PartyMatchDetail | PartyRoomInfo | `ddddddS` | 0xF88B0 | OnPartyRoomInfo | alta |
| 98 | PlaySound | PlaySound | `dSdddddd` | 0xF8A70 | OnPlaySound | alta |
| 99 | StaticObject | StaticObject | `dd` | 0x1132B0 | OnStaticObjectInfo | alta |
| 9A | PrivateStoreSellManageList | PrivateStoreManageList | `ddd[#3:ddddhhhdd]d[#4:ddddhhhddd]` | 0xFE250 | OnStartPrivateStoreManageList -> NC::StartPrivateStoreManageList | alta |
| 9B | PrivateStoreSellList | PrivateStoreList | `ddd[#3:ddddhhhddd]` | 0xFE560 | OnStartPrivateStoreList -> NC::StartPrivateStoreManageList | alta |
| 9C | PrivateStoreSellMsg | PrivateStoreMsg | `dS` | 0x10DA40 | - | alta |
| 9D | ShowMinimap | ShowMinimap | `dc` | 0xFE130 | OnShowMinimap -> NC::C3Virtual270 | alta |
| 9E | ReviveRequest | ReviveRequest | `(nada)` | 0xEA7A0 | - | alta |
| 9F | AbnormalVisualEffect | AbnormalVisualEffect | `(nada)` | 0x10B2F0 | - | alta |
| A0 | TutorialShowHtml | TutorialShowHtml | `S` | 0xF6860 | OnTutorialShowHtml -> NC::TutorialShowHtml | alta |
| A1 | ShowTutorialMark | TutorialShowQuestionMark | `d` | 0xF6900 | OnTutorialShowQuestionMark -> NC::TutorialShowQuestionMark | alta |
| A2 | TutorialEnableClientEvent | TutorialEnableClientEvent | `d` | 0xF6990 | OnTutorialEnableClientEvent -> NC::TutorialEnableClientEvent | alta |
| A3 | TutorialCloseHtml | TutorialCloseHtml | `(nada)` | 0xEA820 | OnTutorialCloseHtml -> NC::TutorialCloseHtml | alta |
| A4 | ShowRadar | ShowRadar | `ddddd` | 0xF6AE0 | OnShowRadar -> NC::ShowRadar | alta |
| A5 | DeleteRadar | DeleteRadar | `ddd` | 0xF6BA0 | OnDeleteRadar -> NC::DeleteRadar | alta |
| A6 | MyTargetSelected | MyTargetSelected | `dh` | 0x10B750 | OnMyTargetSelected | alta |
| A7 | PartyMemberPosition | PartyMemberPosition | `d[#1:d{objectId known to the client:ddd}]` | 0x10AAC0 | - | média |
| A8 | AskJoinAlliance | AskJoinAlliance | `dSSS` | 0xFEB90 | OnReceiveAskJoinAlly -> NC::ReceiveAskJoinAlly | alta |
| A9 | JoinAlliance | JoinAlliance | `d` | 0xFECF0 | OnReceiveJoinAlly -> NC::ReceiveJoinAlly | alta |
| AA | WithdrawAlliance | WithdrawAlliance | `d` | 0xFEDE0 | OnReceiveWithdrawAlly -> NC::ReceiveWithdrawAlly | alta |
| AB | OustAllianceMemberPledge | OustAllianceMemberPledge | `d` | 0xFEED0 | OnReceiveOustAlly -> NC::ReceiveOustAlly | alta |
| AC | DismissAlliance | DismissAlliance | `d` | 0xFEFC0 | OnReceiveDismissAlly -> NC::ReceiveDismissAlly | alta |
| AD | SetAllianceCrest | SetAllianceCrest | `(nada)` | 0xEAFD0 | - | alta |
| AE | AllianceCrest | AllianceCrest | `dd` | 0xFF0B0 | OnMakeDDSCrestFile | alta |
| AF | ServerCloseSocket | ServerCloseSocket | `(nada)` | 0xEA850 | - | alta |
| B0 | PetStatusShow | PetStatusShow | `d` | 0xF6C40 | OnShowPetStatusWnd -> NC::ShowPetStatusWnd | alta |
| B1 | PetInfo | PetInfo | `dddddddddddddddddddffffdddcccccSSddddddddddddddddddddddddddddcc` | 0x1110C0 | OnShowPetNameBtn -> NC::ShowPetNameBtn | alta |
| B2 | PetItemList | PetItemList | `h[#1:hdddhhhdhh]` | 0xF6C90 | OnReceivePetItemList -> NC::ReceivePetItemList | alta |
| B3 | PetInventoryUpdate | PetInventoryUpdate | `h[#1:hhdddhhhdhh]` | 0xF6E40 | UpdatePetInventoryItem -> NC::UpdatePetInventoryItem | alta |
| B4 | AllianceInfo | AllianceInfo | `(nada)` | 0xEAFF0 | - | alta |
| B5 | PetStatusUpdate | PetStatusUpdate | `dddddSdddddddddd` | 0xF7050 | OnReceivePetStatusInfo -> NC::ReceivePetStatusInfo | alta |
| B6 | PetDelete | PetDelete | `dd` | 0xF73B0 | OnHidePetWnd -> NC::HidePetWnd | alta |
| B7 | PrivateStoreBuyManageList | PrivateStoreBuyManageList | `ddd[#3:dhddhdh]d[#4:dhddhdhdd]` | 0xFE6F0 | OnStartPrivateStoreBuyManageList -> NC::StartPrivateStoreBuyManageList | alta |
| B8 | PrivateStoreBuyList | PrivateStoreBuyList | `ddd[#3:ddhddhdhdd]` | 0xFEA00 | OnStartPrivateStoreBuyList -> NC::StartPrivateStoreBuyManageList | alta |
| B9 | PrivateStoreBuyMsg | PrivateStoreBuyMsg | `dS` | 0x10DBD0 | - | alta |
| BA | VehicleStart | VehicleStart | `dd` | 0xFF3F0 | OnVehicleStartPacket | alta |
| BB | RequestTimeCheck | RequestTimeCheck | `d` | 0xFF4D0 | OnRequestTimeCheckPacket | alta |
| BC | StartAllianceWar | StartAllianceWar | `SS` | 0xFF730 | OnStartAllianceWar | alta |
| BD | ReplyStartAllianceWar | ReplyStartAllianceWar | `(nada)` | 0xEB090 | - | alta |
| BE | StopAllianceWar | StopAllianceWar | `SS` | 0xFF810 | OnStopAllianceWar | alta |
| BF | ReplyStopAllianceWar | ReplyStopAllianceWar | `(nada)` | 0xEB0B0 | - | alta |
| C0 | SurrenderAllianceWar | SurrenderAllianceWar | `(nada)` | 0xEB0D0 | - | alta |
| C1 | SkillCoolTime | SkillCoolTime | `d[#1:dddd]` | 0xFF5B0 | OnSkillCoolTimePacket | alta |
| C2 | PackageToList | PackageToList | `d[#1:dS]` | 0xFF8F0 | AddPackageToList -> NC::AddPackageToList | alta |
| C3 | PackageSendableList | PackageSendableList | `ddd[#3:hdddhhdhhhd]` | 0xFFA50 | AddSendableItemList -> NC::AddSendableItemList | alta |
| C4 | EarthQuake | EarthQuake | `dddddd` | 0xFFBE0 | OnEarthQuake | alta |
| C5 | FlyToLocation | FlyToLoaction | `ddddddd` | 0x10DD60 | OnFlyToLocation | alta |
| C6 | BlockList | BlockList | `(nada)` | 0xEB0F0 | - | alta |
| C7 | SpecialCamera | SpecialCamera | `dddddd` | 0x10E180 | OnSpecialCamera | alta |
| C8 | NormalCamera | NormalCamera | `(nada)` | 0xEB110 | OnNormalCamera | alta |
| C9 | CastleSiegeInfo | CastleSiegeInfo | `dddSSdSdd{#2=1&#9=0:d[#1:d]}` | 0xFFD10 | OnReceiveSiegeInfo -> NC::ReceiveSiegeInfo | média |
| CA | CastleSiegeAttackerList | CastleSiegeAttackerList | `dddddd[#6:dSSdddSSd]` | 0x11BC80 | OnReceiveSiegeAttackerList -> NC::ReceiveSiegeAttackerList | alta |
| CB | CastleSiegeDefenderList | CastleSiegeDefenderList | `dddddd[#6:dSSddddSSd]` | 0x11BF20 | OnReceiveSiegeDefenderList -> NC::ReceiveSiegeDefenderList | alta |
| CC | NickNameChanged | NickNameChanged | `dS` | 0x10DF40 | - | alta |
| CD | PledgeStatusChanged | PledgeStatusChanged | `ddddd` | 0x11C1D0 | - | alta |
| CE | RelationChanged | RelationChanged | `ddddd` | 0x10E010 | EnablePartyChat -> NC::EnablePartyChat | alta |
| CF | EventTrigger | EventTrigger | `dc` | 0xFFF80 | OnEventTrigger | alta |
| D0 | MultiSellList | MultiSellList | `dd[#2:dhdhdhh[#6:hhdh]]` | 0x100040 | OnSetVIPShopID -> NC::SetVIPShopID | alta |
| D1 | SetSummonRemainTime | SetSummonRemainTime | `dd` | 0x100210 | OnSetSummonRemainTime -> NC::SetSummonRemainTime | alta |
| D2 | SkillRemainSec | SkillRemainSec | `ddddddd` | 0x10E360 | OnSkillRemainSec | alta |
| D3 | NetPing | NetPing | `d` | 0x1002A0 | - | alta |
| D4 | Dice | Dice | `dddddd` | 0x10E700 | OnDice | alta |
| D5 | Snoop | Snoop | `dSddSS` | 0x100330 | OnSnoop -> NC::Snoop | alta |
| D6 | RecipeBookItemList | RecipeBookItemList | `dd[#2:dd]` | 0x1004C0 | OnReceiveRecipeBookItemList -> NC::ReceiveRecipeBookItemList | alta |
| D7 | RecipeItemMakeInfo | RecipeItemMakeInfo | `dddd` | 0x1005D0 | ReceiveRecipeItemMakeInfo -> NC::ReceiveRecipeItemMakeInfo | alta |
| D8 | RecipeShopManageList | RecipeShopManageList | `ddd[#3:dd]d[#4:ddd]` | 0x100690 | ReceiveRecipeShopManageList -> NC::ReceiveRecipeShopManageList | alta |
| D9 | RecipeShopSellList | RecipeShopSellList | `ddddd[#5:ddd]` | 0x100840 | ReceiveRecipeShopSellList -> NC::ReceiveRecipeShopSellList | alta |
| DA | RecipeShopItemInfo | RecipeShopItemInfo | `dddddd` | 0x100970 | ReceiveRecipeShopItemInfo -> NC::ReceiveRecipeShopItemInfoX | alta |
| DB | RecipeShopMsg | RecipeShopMsg | `dS` | 0x10E570 | - | alta |
| DC | ShowCalc | ShowCalc | `d` | 0xFE1C0 | OnShowCalc -> NC::ShowCalc | alta |
| DD | MonRaceInfo | MonRaceInfo | `ddd[#3:ddddddddffd[20:c]d]` | 0x113550 | OnNpcInfo | baixa |
| DE | ShowTownMap | ShowTownMap | `Sdd` | 0x100A40 | OnShowTownMapPacket -> NC::ShowTownMap | alta |
| DF | ObserverStart | ObserverStart | `ddddd` | 0x100BA0 | OnObserverStartPacket -> NC::ObserverStart | alta |
| E0 | ObserverEnd | ObserverEnd | `ddd` | 0x100D00 | OnObserverEndPacket -> NC::ObserverEnd | alta |
| E1 | ChairSit | ChairSit | `dd` | 0x100E30 | OnChairSitPacket | alta |
| E2 | HennaEquipList | HennaEquipList | `ddd[#3:ddddd]` | 0x100F40 | ReceiveHennaEquipList -> NC::ReceiveHennaEquipList | alta |
| E3 | HennaItemInfo | HennaItemInfo | `dddddddcdcdcdcdcdc` | 0x1010F0 | ReceiveHennaItemInfo -> NC::ReceiveHennaItemInfo | alta |
| E4 | HennaInfo | HennaInfo | `ccccccdd[#8:dd]` | 0x101270 | ReceiveHennaInfo -> NC::ReceiveHennaInfo | alta |
| E5 | HennaUnequipList | HennaUnequipList | `ddd[#3:ddddd]` | 0x101630 | ReceiveHennaUnequipList -> NC::ReceiveHennaUnequipList | alta |
| E6 | HennaUnequipInfo | HennaUnequipInfo | `dddddddcdcdcdcdcdc` | 0x101AB0 | ReceiveHennaUnequipItemInfo -> NC::ReceiveHennaUnequipItemInfo | alta |
| E7 | MacroList | MacroList | `cdcc[#4:{#^3!=0:dSSScc[#6:ccdcS]}]` | 0x1017E0 | ReceiveMacroListInfo | alta |
| E8 | BuyListSeed | BuyListSeed | `ddh[#3:hdddhh{#1=0..3:dhhh}d]` | 0xF77E0 | OnStartManorBuyList -> NC::StartManorBuyList | alta |
| E9 | SellListProcure | SellListProcure | `ddh[#3:hdddhh{#1=0..3:dhhh}d]` | 0xF7A00 | OnStartManorSellList -> NC::StartManorSellList | alta |
| EA | GMHennaInfo | GMHennaInfo | `ccccccdd[#8:dd]` | 0x101450 | GMReceiveHennaInfo -> NC::GMReceiveHennaInfo | alta |
| EB | RadarControl | RadarControl | `ddddd` | 0xF6A20 | OnShowRadar -> NC::ShowRadar | alta |
| EC | ClientSetTime | ClientSetTime | `dd` | 0x101CF0 | OnClientSetTime | alta |
| ED | ConfirmDlg | ConfirmDlg | `d` | 0x101E10 | OnConfirmDlg -> NC::ShowMessageDialog46 | alta |
| EE | PartySpelled | PartySpelled | `ddd[#3:d]` | 0x101F00 | OnPartySpelledPacket -> NC::ReceiveBuffList | alta |
| EF | ShopPreviewList | ShopPreviewList | `dddh[#4:dhhd]` | 0x102040 | OnStartPreviewList | alta |
| F0 | ShopPreviewInfo | ShopPreviewInfo | `dddddddddddddddd` | 0x1021D0 | OnPreviewInfo | alta |
| F1 | CameraMode | CameraMode | `d` | 0x102290 | OnCameraMode | alta |
| F2 | ShowXMasSeal | ShowXMasSeal | `d` | 0x1022E0 | OnShowXMasSeal -> NC::ShowXMasSeal | alta |
| F3 | EtcStatusUpdate | EtcStatusUpdate | `ddddd` | 0x102370 | OnReceiveEtcStatus -> NC::C3Virtual168 | alta |
| F4 | ShortBuffStatusUpdate | ShortBuffStatusUpdate | `ddd` | 0x102670 | OnReceiveShortStatus -> NC::C3Virtual166 | alta |
| F5 | SSQStatus | SSQStatus | `cc{#1=4:ccccd}{#1=3:ccccccc}{#1=1:dddccdddddcdddc}{#1=2:c[#1:cc[#2:cddc[#4:S]dc[#6:S]]]}` | 0x1027B0 | OnStartSSQMainEvent -> NC::ResetSSQEvents | alta |
| F6 | PetitionVote | PetitionVote | `(nada)` | 0xEB160 | OnPetitionVote -> NC::C3Virtual349 | alta |
| F7 | AgitDecoInfo | AgitDecoInfo | `d` | 0x102F60 | OnAgitInfo | alta |
| F8 | SSQInfo | SSQInfo | `cc` | 0x102FF0 | OnSSQInfo | alta |
| F9 | GameGuardQuery | GameGuardQuery | `dddd` | 0x103080 | OnGameGuardQuery | alta |
| FA | L2FriendList | L2FriendList | `d[#1:dSdd]` | 0x103140 | OnReceiveL2FriendList -> NC::C3Virtual291 | alta |
| FB | L2Friend | L2Friend | `ddSdd` | 0x103260 | OnUpdateL2Friend -> NC::OnMessengerFriendEvent | alta |
| FC | L2FriendStatus | L2FriendStatus | `dSd` | 0x103320 | OnUpdateL2FriendStatus -> NC::OnMessengerFriendStatus | alta |
| FD | L2FriendSay | L2FriendSay | `dSSS` | 0x1033C0 | OnReceiveL2FriendSay -> NC::OnMessengerChat | alta |
| FE | - | Dummy | `(nada)` | 0xEAF50 | - | alta |

## 7. Pacotes estendidos (`0xFE` + word)

| opcode | spec (netpro) | nome no Engine | formato (payload, sem o opcode) | handler (RVA) | notify -> NConsoleWnd | certeza |
|---|---|---|---|---|---|---|
| FE:0001 | ExRegenMax | ExRegenMax | `dddf` | 0x103490 | OnRegenStatus -> NC::StartHPBarAnim | alta |
| FE:0002 | - | ExEventMatchList | `(nada)` | 0xEB1B0 | - | alta |
| FE:0003 | - | ExEventMatchObserver | `dccSSddd` | 0x103550 | OnEventMatchObserver | alta |
| FE:0004 | - | ExEventMatchMessage | `cS` | 0x1036C0 | OnEventMatchMessage | alta |
| FE:0005 | - | ExEventMatchScore | `ddd` | 0x1037B0 | OnEventMatchScore | alta |
| FE:0006 | - | ExEventMatchTeamInfo | `dcd[#3:dSdddddddddd]` | 0x103890 | OnEventMatchTeamInfoStart | alta |
| FE:0007 | - | ExEventMatchCreate | `d` | 0x103AA0 | OnEventMatchCreate | alta |
| FE:0008 | - | ExEventMatchUserInfo | `dSdddddddd` | 0x103BB0 | OnEventMatchUserInfo | alta |
| FE:0009 | - | ExColosseumFenceInfo | `ddddddd` | 0x113F20 | OnColosseumFence | alta |
| FE:000A | - | ExEventMatchSpelledInfo | `ddd[#3:dhd]` | 0x103CF0 | OnEventMatchSpellInfoStart | alta |
| FE:000B | - | ExEventMatchFirecracker | `d` | 0x10E860 | OnFirecracker | alta |
| FE:000C | - | ExEventMatchTeamUnlocked | `dc` | 0x103E80 | OnEventMatchTeamUnlocked | alta |
| FE:000D | - | ExEventMatchGMTest | `(nada)` | 0xEB1D0 | OnEventMatchGMTest | alta |
| FE:000E | - | ExPartyRoomMember | `dd[#2:dSdddd]` | 0x103F60 | OnExPartyRoomMember | alta |
| FE:000F | - | ExClosePartyRoom | `(nada)` | 0xEB270 | OnExClosePartyRoom | alta |
| FE:0010 | - | ExManagePartyRoomMember | `ddSdddd` | 0x104170 | OnExManagePartyRoomMember | alta |
| FE:0011 | - | ExEventMatchLockResult | `(nada)` | 0xEB380 | - | alta |
| FE:0012 | - | ExAutoSoulShot | `dd` | 0x104330 | OnExAutoSoulShot | alta |
| FE:0013 | - | ExEventMatchManage | `dccScScd[#8:ccdSdd]` | 0x104490 | OnExEventMatchManage | alta |

## 8. Spec x Engine

* A spec tem 255 entradas (`00..FD` e `FE:01`). **Spec tem e o Engine NÃO decodifica** (handler vazio, o pacote é descartado):
  `14` LoginFail, `1F` TradeStartOk, `57` MagicList, `6B` SetPledgeCrest, `75` TradePressOwnOk, `78` FriendAdd, `79` FriendRemove, `7A` FriendList, `7B` FriendStatus, `7C` TradePressOtherOk, `9E` ReviveRequest, `9F` AbnormalVisualEffect, `AD` SetAllianceCrest, `AF` ServerCloseSocket, `B4` AllianceInfo, `BD` ReplyStartAllianceWar, `BF` ReplyStopAllianceWar, `C0` SurrenderAllianceWar, `C6` BlockList, `FE:0002` ExEventMatchList, `FE:0011` ExEventMatchLockResult; e os que caem no `DummyPacket` (loga e descarta): `70` Dummy_70, `87` GiveNickNameDone. Os que mais pesam no jogo:
  `9E ReviveRequest`, `9F AbnormalVisualEffect`, `7A FriendList`/`78 FriendAdd` (a lista de amigos antiga), `B4 AllianceInfo`.
* O Engine tem 18 pacotes estendidos que a spec não lista (`FE:0002..FE:0013`: `ExEventMatch*`, `ExColosseumFenceInfo`,
  `ExPartyRoomMember`, `ExClosePartyRoom`, `ExManagePartyRoomMember`, `ExAutoSoulShot`). `FE:0002` e `FE:0011` são stubs.
* Nomes que divergem (mesmo opcode, só o nome muda; o opcode e a ordem batem em todos os outros):

| opcode | spec | Engine |
|---|---|---|
| 26 | ServerClose | SeverClosePacket |
| 4A | Say2 | SayPacket2 |
| 70 | Dummy_70 | DummyPacket |
| 87 | GiveNickNameDone | DummyPacket |
| 96 | PartyMatchList | ListPartyWatingPacket |
| 97 | PartyMatchDetail | PartyRoomInfoPacket |
| 9A | PrivateStoreSellManageList | PrivateStoreManageList |
| 9B | PrivateStoreSellList | PrivateStoreList |
| 9C | PrivateStoreSellMsg | PrivateStoreMsg |
| A1 | ShowTutorialMark | TutorialShowQuestionMarkPacket |
| C5 | FlyToLocation | FlyToLoactionPacket |

* Nenhum opcode da spec está ausente da tabela do Engine (o `00 VersionCheck` é a entrada estática: formato `cddd`: resultado `c`,
  chave `dd`, `d`).
* O código tem mais 8 funções que leem pacote e que NINGUÉM registra (legado, inalcançáveis): `0x10408660` (`ddddddd`, notify
  MoveBackwardToLocation), `0x10409570` / `0x104096f0` (`dd`/`d`, OnAttacked), `0x1040A030` / `0x1040A1C0` (`ddd`), `0x1040C1B0`
  (`dddd`), `0x10419120` (`d`), `0x103F6770` (`d`). Se um servidor mandasse esses opcodes, o cliente cairia no `DummyPacket`.

## 9. Estado de login (13 handlers, tabela `0x108E62C4`)

Só para completar; o servidor Lucera fala o protocolo de login à parte e esta pista não os auditou a fundo.

| estado de login | handler (RVA) | formato | notify | certeza |
|---|---|---|---|---|
| L:00 | 0xf5020 | `(?)` | OnGameGuardQuery | baixa |
| L:01 | 0xf5770 | `c` | OnAuthLoginFail | alta |
| L:02 | 0xf58e0 | `d` | OnBlockedAccount | alta |
| L:03 | 0xf5a00 | `ddddddddb<16>` | CheckLoginType, OnStartNotice | alta |
| L:04 | 0xf5c40 | `cc[#1:cddcchhcdc]` | OnStartServerList, OnAddServerList, OnEndServerList | alta |
| L:05 | 0xf5f00 | `c` | OnAuthLoginFail | alta |
| L:06 | 0xf6020 | `c` | OnAuthLoginFail | alta |
| L:07 | 0x108410 | `dd` | - | média |
| L:08 | 0xf6140 | `c` | OnAuthLoginFail | alta |
| L:09 | 0xf6260 | `c[#1:hS]` | OnBlockedAccountWithMsg | alta |
| L:0A | 0xf5bd0 | `dc` | OnSCCheckStart | alta |
| L:0B | 0xf5690 | `ddddd` | OnGameGuardQuery | alta |
| L:0C | 0xeaf50 | `(?)` | - | alta |

## 10. Não resolvido / limites

* `L:00` (primeiro pacote do estado de login) e `L:07` não têm formato confiável (o 0C é o `DummyPacket`); fora do escopo do jogo.
* `DD` (MonRaceInfo, corrida de monstros, nunca usada) tem a contagem de runners em um dos dois primeiros `d`, sem fixar qual.
* Valores de campo de tipo/condição só foram sondados em 0..12 mais as constantes de `cmp` do handler: um `switch` por valores
  maiores, sem `cmp` correspondente no handler, passaria despercebido (a conferência de pontos de leitura estáticos x
  alcançados cobre isso para as leituras, não para as ramificações sem leitura).
* A parte de escrita (c2s, `Request*`) é da pista T2.
* As caudas de versão de `CharInfo`/`UserInfo`/`NpcInfo` assumem protocolo 530; um cliente com `Version` menor não as lê.
* `nwindow` vazio não quer dizer que o pacote não chega ao NWindow: quer dizer que `UGameEngine::OnX` trabalha no mundo (pawn/animação)
  antes de qualquer chamada ao console.

## 11. Como reproduzir

    python tools/c3/s2c_formats.py --opcode 9A            # uma linha
    python tools/c3/s2c_formats.py --opcode 9A --full     # a entrada inteira (JSON)
    python tools/c3/s2c_formats.py --opcode FE:0003
    python tools/c3/s2c_formats.py --json analysis/c3_s2c_formats.json   # tudo (~5 min com emulação)
    python tools/c3/s2c_formats.py --no-emu                              # só estático (rápido, laços sem contagem)

Requer `pefile`, `capstone` e `unicorn` (a ausência de `unicorn` cai no passe estático).
