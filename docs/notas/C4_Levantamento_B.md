# C4 — levantamento da source (20/10/2026, fim da onda B do estágio 4)

Fonte: `docs/PROGRESSO.md` (gerado pelo ciclo) e `db/functions.csv` do commit `s4-wave-b-merged`.
"Feito" = função com gancho ativo na DLL (`NW_HOOK_*` no começo da linha). Nenhum gancho desligado resta
(os 13 da onda A foram religados; os 42 que esperavam o outro lane do messenger viraram linhas `NW_HOOK_*` normais).

## Número principal

| | funções | bytes | % do código |
|---|---:|---:|---:|
| código a decompilar (game + init) | 7.202 | 1.448.754 | 100 % |
| **reimplementado, com gancho ativo** | **5.387** | **1.335.576** | **92,19 %** |
| sem gancho, por convenção (destrutores deleting, InternalConstructor) | 577 | 15.636 | 1,08 % |
| só na fase 5 (inicialização estática, DllMain) | 1.038 | 48.609 | 3,36 % |
| **falta (todo)** | **200** | **48.933** | **3,38 %** |

Marcos: fim do estágio 3 = 2.846 ganchos / 23,18 %; fim da onda A = 4.224 / 62,53 %; fim da onda B = 5.387 / 92,19 %
(+1.163 ganchos, +29,66 pontos). O C3 terminou em 4.970 ganchos / 93,03 %.

**Do "falta" de 48.933 bytes, 28.1 KB são o GameGuard (`NPG*`, 19 + 15 + 5 + 10 + 2 + 9 + 2 + 4 + 3 + 6 + 6 funções), fora de escopo
por decisão do usuário.** O que sobra de verdade é ≈ 20,8 KB (1,4 %):

| classe | funções | bytes | por que ficou |
|---|---:|---:|---|
| NCTaiwanIME | 9 | 5.225 | específica de locale; as IMEs irmãs foram provadas só no harness do input1 |
| (anônimas) | 39 | 4.199 | pedaços sem classe; triagem pendente |
| NCInvenItemWnd::OnLButtonUp | 1 | 2.701 | arrasta-e-solta sobre ~8 classes de janela, sem harness de prova |
| Md5 (CopyWords/DecodeWords/Transform) | 3 | 2.334 | mortas no modo gancho; shims com argumento em registrador falham no regcheck |
| NToolTipSkin (Draw, DrawFull) | 2 | 1.546 | forwarders por valor, adiados por instrução; o corpo é o do C3 |
| NConsoleWnd | 4 | 567 | getters pequenos |
| SwitchboardSession | 1 | 466 | |
| demais (várias classes, cada uma abaixo de 460 bytes) | — | ≈ 3.800 | triagem pendente |

## Onda B por lane (ordem de integração)

| lane | ganchos | hooks totais depois do merge |
|---|---:|---:|
| bgm (GM broadcast, observer, replay) | +128 | 4.352 |
| sys2 (vídeo, servidor, Seven Signs, cerco) | +82 | 4.434 |
| msn2 (notificação, switchboard, diálogos) | +93 | 4.527 |
| items2 (loja privada, manor, receitas) | +188 | 4.715 |
| sys1 (login, opções, quests, mapa, créditos) | +98 | 4.813 |
| items3 (tooltip, VIP, loja, troca, macro, HTML) | +144 | 4.957 |
| msn1 (janelas e protocolo do messenger) | +220 (178 + 42 que esperavam) | 5.177 |
| rest1 (sobras, olympiad, hero tower, inven, IME...) | +210 | 5.387 |

Todos os ciclos fecharam com `0 errors`, `verify 0 sem explicação`, `CHECKS: ALL OK`.

## Bugs que os julgadores não viam e a onda B achou

- Winsock: os thunks `jmp [IAT]` do C3 foram remapeados para vizinhos. C4: WSAGetLastError `10170B02`, connect `B08`, htons `B0E`,
  gethostbyname `B14`, inet_addr `B1A`, socket `B20`, WSAStartup `B26`, recv `B2C`, send `B32`, WSAAsyncSelect `B38`, htonl `B3E`,
  closesocket `B44`. Corrigido em todos os arquivos.
- Getters minúsculos dobrados pelo linker (listas do messenger FL/AL/BL/RL/Groups): remapeados para vizinhos.
- `MSN_SYSMSG` usava a base de mensagens de sistema do C3 (`0x3F638`); a do C4 é `GL2GameData+0x722C8`.
- `NCGDevice::DrawPledgeCrest/DrawAllyCrest` chamavam os slots 43/44 do handler de rede; o stock chama os 45/46 (`0x1006A900/910`).
- `NMsnHeaderMap::HasHeader/GetHeader`: o `this` do stock é a mensagem, não o mapa.
- `verify_data.py` lia a tabela de importação como UTF-16 (175 a 513 diferenças falsas por lane): corrigido.
- `check_breakers.py` tinha RVAs do C3 em `STOCK_NORETURN_RVA`: corrigido para os do C4.

Ferramentas novas: `tools/port/check_literals.py` (cada endereço literal de um corpo hookado tem de aparecer no stock da função;
rodada nos lanes, achou os bugs de Winsock/getters; os alertas que restam são quase todos vtable em construtor, triagem pendente) e as
provas no emulador unicorn (`tools/port/emu/win`, `t_entry.py`, `t_md5*.py`, `t_vipwnd*.py`, `t_tip*.py`).

## Dívidas conhecidas (para o próximo passo)

- 13 diferenças de `verify_data` sem nota, de nomes de guard dos lanes sys1 (Login/Option/GameOption/NoticeInfo/MapCover/QuestListInfo).
- `NConsoleWnd.h`: `PartyItemDistribution` e `GetPartyItemDistribution` estão com nome trocado (`+0x3C68` é a flag de servidor especial,
  `+0x3C6C` a regra de divisão de itens).
- `NItemEntry::~NItemEntry` (`0x10050030`) real e os construtores stand-in `NCShopWnd()` / `NCGaraTree()`: liberam 3 destrutores e 2 construtores.
- `NCRecipeShopItemWnd.cpp` ainda com `Field(&Info,0x18)` do C3; `NCTVItem.h` e `NNewVirtuals.gen.cpp` (slot 56 de L2ChatWnd etc.).
- `include/ue2 URenderDevice`: slot 27 `SetRes` recebe viewport + 5 inteiros, o header declara 6.
- `db`: nomes errados (`0x100f87d0`/`0x100f95f0` são `NCReplayLogoWnd/NCReplayPlayWnd::OnPaint`); 15 conv das classes novas ainda `todo` no db.
- Classes de nenhum lane (46,8 KB na partição da onda B) e as classes novas fora das tabelas standalone (`classes.json` / `Classes.gen.cpp`).
- Servidor: incompatibilidade de id de NPC do C4 (PORTE-C4.md §5.1).

## Verificação em jogo

**Nada além de 3.617 ganchos entrou no mundo.** O build de 5.387 ganchos foi fechado só pelos julgadores e pelas provas de emulador;
está instalado em `system_nwtest` (nunca em `system`). Riscos mais altos: janela de opções de vídeo (maior reescrita),
seleção de servidor (chave por id), `NCGaraDialogBox::OnOK`, janelas do messenger (rodam na primeira entrada no mundo) e a VIP/multisell.
