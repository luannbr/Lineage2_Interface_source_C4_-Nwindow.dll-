> **Nota do C2 (06/10/2026):** este documento é do projeto do C1 e descreve o `NWindow.dll` do C1 (178 classes, endereços `0x100…` do C1). Os fatos do C2 e o que já foi retargetado estão em [../PLANO_C2.md](../PLANO_C2.md); os endereços daqui não valem para a imagem do C2 (o `db/fmap.csv` traduz C1 → C2).

# Restante — o que ainda é `todo` no banco (rodada 9, 26/09/2026)

Auditoria de completude do modo gancho: todas as funções `game` e `init` que o `db/functions.csv`
ainda marca como `todo`, agrupadas pelo motivo, com os bytes de cada grupo e o que zera cada um.
Levantada sobre `agent/status` `ccc2123` (master `4029469` + os ganchos desta rodada): 2.743
ganchos, 95,60 % do código.

Fontes:
- `db/functions.csv` e `analysis/classes.json`;
- o original (`orig/NWindow.dll`, lido com `tools/nwd`): todas as `call`/`jmp` rel32 do `.text` e
  todas as referências por relocação;
- as faixas das funções do Ghidra (`analysis/ghidra/functions.tsv`).

O cruzamento com o `docs/notas/Standalone.md` está na seção 11.

## Resumo

- **810 funções `todo`, 33.190 bytes**: 268 `game` (7.960 B) e 542 `init` (25.230 B).
- **Nenhuma pede decompilação à mão.** O último corpo do jogo que o nosso código ainda chamava
  no original foi feito nesta rodada: o `TMap::FindRef` da tabela de itens (`0x1004dda0`), que
  era chamado via `nw::StockCall`.
- **Quase nada disso roda no modo gancho.** Conferi os chamadores de cada uma (ver "Como foi
  conferido"). As únicas que ainda rodam:
  - os `InternalConstructor`, que o Core chama pela `UClass` e que só chamam os nossos
    construtores;
  - os `scalar deleting destructor` de objetos com vtable do original. Na prática são só as
    exceções `NStd*`, que guardam essa vtable de propósito (`NStdLib.h`), e eles chamam os
    nossos destrutores;
  - o `DllMain` e os inicializadores estáticos.
- **Para o `todo` do modo gancho chegar a zero, basta uma regra no `build_db.py`** (seção 10). Uma
  linha no `NTemplates.cpp` tira mais um (grupo 7).

| # | Grupo | Funções | Bytes | Modo gancho | Fase 5 |
|---|---|---:|---:|---|---|
| 1 | `scalar deleting destructor` | 183 | 6.564 | convenção: nada a escrever | o compilador gera |
| 2 | `InternalConstructor` | 76 (+100 fora do banco) | 1.107 (+1.444) | convenção: nada a escrever | o `NW_DECLARE_CLASS` gera; 17 classes sem os argumentos padrão |
| 3 | inicialização estática e `DllMain` | 543 | 25.247 | status "fase 5" | `IMPLEMENT_CLASS`, `IMPLEMENT_PACKAGE`, 4 globais |
| 4 | argumento em `AX` (`0x100427d0`) | 1 | 54 | morto | nada |
| 5 | corpos vazios (`ret N`) | 3 | 7 | mortos | nada |
| 6 | cópias fora de linha de código de biblioteca | 2 | 91 | mortas | nada |
| 7 | destrutor implícito do `FL2SystemMsgData` | 1 | 9 | uma linha, ou morto | nada |
| 8 | construtor da base abstrata `NHtmlObject` | 1 | 111 | morto | nada |
| 9 | trabalho de verdade | 0 | 0 | — | — |
| | **total** | **810** | **33.190** | | |

"Morto" quer dizer duas coisas:
- todo chamador é função nossa, ou funclet de EH de função nossa;
- toda vtable que cita a função é do original, e nenhum objeto usa mais essas vtables.

## 1. `scalar deleting destructor` (183 funções, 6.564 B)

**O que são.** O slot 3 de cada vtable: chama o destrutor e, com o bit 1, o `operator delete`.
- 176 têm nome no banco: os das 178 classes, menos `NCCmdTarget` e `NCAbstractMain`.
- 7 estão como `sub_`:

  | VA | Classe | Vtable |
  |---|---|---|
  | `10001720` | NCCmdTarget | `0x100d7d18` (slot 3) |
  | `100246e0` | NCAbstractMain | `0x100dd878` (slot 3) |
  | `1000d5b0` | NStdLogicError | `0x100f0dc0` (slot 0) |
  | `1000da10` | NStdLengthError | `0x100f0dcc` (slot 0) |
  | `10025d40` | NHtmlTagTable | `0x100de04c` (slot 0) |
  | `1002c760` | NHtmlParserBase | `0x100ded74` (slot 0) |
  | `1002d000` | NHtmlParser | `0x100ded78` (slot 0) |

- **Por que `NCCmdTarget` e `NCAbstractMain` ficaram sem nome.**
  - Os construtores de `NCObject`, `NCCmdTarget` e `NCAbstractMain` são um só (ICF), e ele
    instala a vtable do `NCObject` (`0x100d7cb8`).
  - As vtables verdadeiras das duas só são instaladas pelos destrutores.
  - Por isso o `classes.json` dá às duas a vtable do `NCObject`.
  - A do `NCCmdTarget` só difere da do `NCObject` no slot 3, então o
    `NW_CHECK_VTABLE(NCCmdTarget, 0x100d7cb8, 24)` do `NCWnd.cpp` continua valendo.

**Por que não têm gancho.** É a convenção (`CONVENCOES.md`):
- o destrutor que cada um chama já é nosso (conferido nos 183);
- os nossos objetos saem dos nossos construtores com as nossas vtables, cujo slot 3 é o do nosso
  compilador;
- os do original só são alcançados por vtable do original, e as únicas em uso são as das
  exceções `NStd*`.

**Para zerar:** nada a escrever. Basta a regra no banco (seção 10).

**Fase 5:** o compilador gera todos a partir do `virtual ~X()`.
- As classes `UObject` apagam pelo `UObject::operator delete` do Core.
  - O Standalone.md (§2) lista esse import como "só por código original".
  - No standalone ele volta a ser chamado, por esses corpos gerados.
- As `NStd*` e `NHtml*` apagam pelo `operator delete` do módulo (`GMalloc`, `NwMemory.cpp`).

Lista completa: apêndice A.

## 2. `InternalConstructor` (76 no banco + 100 fora, 2.551 B)

**O que são.** O ponteiro que o `IMPLEMENT_CLASS` passa à `UClass` (o `ClassConstructor`); o
corpo é `if (X) Classe::Classe(argumentos padrão)`.
- São 176 corpos para 178 classes: `NCObject`, `NCCmdTarget` e `NCAbstractMain` dividem um.
- Quem os chama é o Core, pela `UClass` (`StaticConstructObject`).
- Conferido: os 176 chamam um construtor que já tem gancho, então rodam o nosso código.

**100 estão fora do banco** (1.444 B).
- O Ghidra não fez função deles, porque só o `push` do inicializador os cita.
- A lista está no apêndice C.
- Para o total do banco ficar honesto, eles entram no `db/extra_functions.csv`. O `build_db.py` já
  os nomeia pelo `classes.json`.

**Para zerar:** nada a escrever. Basta a regra no banco (seção 10).

**Fase 5:** o `NW_DECLARE_CLASS` do standalone gera `new ((EInternal*)X) TClass;` (Standalone.md
§3).
- 26 classes passam argumentos ao construtor.
- 9 já têm esses valores como argumentos padrão no header: `NCCandidateBox`, `NCEditBox`, `NCIME`,
  `NCIMEManager`, os quatro IMEs e `NCResizeFrame`.
- Nas outras 17 o construtor não tem padrão, e o `new` sem argumentos não compila:

  | Classe | Argumentos do original |
  |---|---|
  | NCAbnormalStatusWnd | `24` |
  | NCActionItemWnd | `5, 2, 15, 32` |
  | NCActionLinker | `0` |
  | NCCheckBox | `TEXT("default"), 0, 0` (o construtor `0x10003df0`) |
  | NCCountSpin | `0, 100, NULL` |
  | NCEquipItemWnd, NCGMEquipItemWnd | `32` |
  | NCFileLinker | `NULL` |
  | NCHtmlEdit | `0, 0` |
  | NCJHtmlEdit | `0, 1, 1` |
  | NCShortCutWnd | `10, 10, 32` |
  | NCStatusWnd, NCPlayerStatusWnd, NCTargetStatusWnd, NCPartyStatusWnd, NCPetStatusWnd, NCSummonedStatusWnd | `100` |

- Há duas saídas:
  - acrescentar esses padrões nos headers;
  - fazer o gerador do `IMPLEMENT_CLASS` emitir `TClass(argumentos)`.

## 3. Inicialização estática e `DllMain` (543 funções, 25.247 B)

| Quantas | O que | Bytes | Como está no banco |
|---:|---|---:|---|
| 178 | `Classe::PrivateStaticClass`: construtor nativo da `UClass` + `atexit` | 21.206 | `…dynamic initializer'` |
| 178 | `autoclassClasse = &Classe::PrivateStaticClass` | 1.958 | 176 como `…dynamic atexit destructor'` (nome errado), 2 como `sub_` |
| 178 | `~UClass` registrado no `atexit` (`0x100d5e70..0x100d69c0`) | 1.958 | `sub_` |
| 2 | `GCoolTimeTextures`/`GCoolTimeEndTextures`: construtor do `TArray` + `atexit` (`0x100d07e0`, `0x100d0800`) | 44 | `sub_` |
| 2 | os destrutores deles (`0x100d6000`, `0x100d6010`) | 20 | `sub_` |
| 2 | `GHtmlAlignStack`/`GHtmlVAlignStack`: só o `atexit` (`0x100d0ee0`, `0x100d0ef0`) | 24 | `sub_` |
| 2 | os destrutores deles (`0x100d6140`, `0x100d6150`) | 20 | `sub_` |
| 1 | `DllMain` (`0x10024080`): `GNWindowInstance = hInstance; return 1` | 17 | `sub_` |

- **O mecanismo e os nomes errados** estão no Standalone.md §4; o lobby vai corrigir o banco. O
  apêndice D dá os três VAs de cada classe.
- **Dois `autoclass` sem nome:** o de `NConsoleWnd` (`sub_100d1da0`) e o de `NL2Font`
  (`sub_100d2080`).
  - O `build_db.py` procura o `autoclass` em `inicializador + 0x80`.
  - Mas o inicializador da `UClass` dessas duas é maior (134 e 131 bytes, porque o pai vem de
    import), e o `autoclass` fica em `+0x90`.
- **No modo gancho dá para fazer gancho, mas não vale.**
  - Os ganchos entram antes: o `NWindow.dll` remendado importa a `NWindowDecomp.dll`, cujo
    `DllMain` roda primeiro (`NwMain.cpp`).
  - Só que um gancho refaria o `IMPLEMENT_CLASS` com os endereços do original, e a fase 5 jogaria
    isso fora.
  - O `DllMain` ainda tem o problema do `regcheck` (26/09).
  - Proposta: status "fase 5" no banco (seção 10).
- **Fase 5:**
  - o `IMPLEMENT_CLASS` gerado do `classes.json`, com as três divergências do Standalone.md §3;
  - o `IMPLEMENT_PACKAGE` (`DllMain`, `GPackage`);
  - os quatro `TArray` definidos como globais.
- **Conferido nos nossos headers:**
  - as 178 classes têm `NW_DECLARE_CLASS` e `NW_ASSERT_SIZE`, e os 178 tamanhos batem com o
    `sizeof` do original;
  - as únicas divergências são as três do Standalone.md: o pai do `NCTestHtmlViewer` e do
    `NCTradeItemWnd`, e o `CLASS_Config` do `NL2Font`.

## 4. Argumento em `AX`: `0x100427d0` (54 B)

- **O que faz:** devolve 1 quando o caractere não tem consoante final:
  - ou não é sílaba hangul (fora de `U+AC00..U+D7A3`);
  - ou `((c - 0xAC00) % 588) % 28 == 0`.

  Com isso escolhe a partícula coreana do `$s1`/`$s2`.
- **Por que não tem gancho:** recebe o caractere em `AX`, uma convenção própria da otimização
  de programa inteiro.
- **Morto:** os três chamadores (`FormatSystemMessage`, `EndSystemMessageParam`,
  `MakeSystemMessage`) são nossos. No nosso código a conta é o `HasNoFinalConsonant`
  (`NConsoleWnd.h`).
- **Para zerar:** a regra no banco. O mapa do console manda deixar sem gancho.
  - A alternativa seria um adaptador `NW_NAKED`
    (`movzx eax, ax; push eax; call HasNoFinalConsonant; add esp, 4; ret`).
  - Ele pediria uma cópia não inline do `HasNoFinalConsonant`, sem mudar nada.

## 5. Corpos vazios (3 funções, 7 B)

| VA | Corpo | Onde está nas vtables | Chamadores diretos |
|---|---|---|---|
| `10001700` | `ret 4` | slots 74, 110–114, 140 e 271 do console e slots de outras classes (ICF, 9 referências) | `NCMultiEdit::OnCreate`, `NCHtmlMultiEdit::OnCreate`, `PartyAddMember`, `PartyDeleteAll`, `EnablePartyChat` (jmp), `Initialize` |
| `10045870` | `ret` | slots 27, 137, 208, 42 e 64 (ICF, 7 referências) | `EndRecommandedData`, `NCSkillItemWnd::OnDestroy/Clear`, `NCShortCutWnd::SetShortCut`, `~NShortCutItem` (jmp), 3 funclets de EH de funções nossas |
| `10045c50` | `ret 8` | só o slot 222 do console (`PrivateStoreMsg`), sem ICF | nenhum |

- **Mortos:** os chamadores diretos são todos nossos, e as nossas vtables têm os nossos vazios.
- **Para zerar:** a regra no banco.
  - `0x10001700` e `0x10045870` ficam sem gancho, como decidido na rodada 8: são de muitas classes.
  - O `0x10045c50` aceitaria gancho, porque o slot já está declarado vazio no `NConsoleWnd.h`.
    Mas o gancho também não muda nada.

## 6. Cópias fora de linha de código de biblioteca (2 funções, 91 B)

**`appMemcpy` (`0x1000e320`, 79 B no banco).**
- É o `appMemcpy` inline (asm SSE/MMX) dos headers do UE2 com que o original foi compilado,
  emitido fora de linha.
- O corpo passa dos 79 B:
  - o trecho `0x1000e370` já está no `not_functions.csv`;
  - `0x1000e4e2` e `0x1000e504` ficam fora das faixas do Ghidra.
- Lê o `GIsSSE` do Core, o outro import "só por código original" do Standalone.md §2.
- Os 43 pontos de chamada estão em funções nossas: os 4 IMEs, `NCMultiEdit::GetText`,
  `NString::Insert`, `NStringArray::InsertAt`, `CreateColorCursor` e os dois brasões.
- No nosso `UE2Core.h` a função é o `memcpy` inline.

**`NConsoleWnd::Free` (`0x10046f40`, 12 B).**
- É o slot 258, `GMalloc->Free`. O slot está declarado inline no `NConsoleWnd.h`.
- O linker o dobrou com o `operator delete` global do módulo.
- Quem chama esse `operator delete` é o `NString::Insert`/`Empty`, e os dois são nossos.

**Para zerar:** a regra no banco. Um gancho no `0x10046f40` trocaria o `operator delete` do
original inteiro, sem ganho.

## 7. Destrutor implícito do `FL2SystemMsgData`: `0x10045e60` (9 B)

- **O que é:** `add ecx, 4; jmp ~FStringNoInit`, o destrutor que o compilador gerou para a linha de
  `systemmsg`, com o `Message` em +4.
- **Morto:** só o desenrolar de `EndSystemMessageParam` e `MakeSystemMessage` o chama
  (`sub_100cda40`, `sub_100cda80`), e as duas funções são nossas.
- **Para zerar:** a regra no banco, ou uma linha no `NTemplates.cpp`:
  `NW_HOOK_DTOR(0x10045e60, FL2SystemMsgData)`. Não fiz porque está fora da lista desta rodada.

## 8. Construtor da base abstrata: `NHtmlObject::NHtmlObject` (`0x10028aa0`, 111 B)

- **Morto:** só os construtores de `NHtmlBlock` (2) e de `NHtmlItem` o chamam. Os três são nossos
  e rodam o nosso `NHtmlObject::NHtmlObject` (`NHtmlLayout.cpp`).
- **Por que não tem gancho:** o `NW_HOOK_CTOR` instancia a classe, e o `NHtmlObject` é abstrato
  (comentário do clan no `NHtmlLayout.cpp`).
- **Para zerar:** a regra no banco. A alternativa é uma variante do macro para classe abstrata.

## 9. Trabalho de verdade: nada

O clan (rodada 6) terminou o que ainda era trabalho de alguém, e tudo entrou no master em
`0e9567f`:
- o tokenizador HTML (`NHtmlParser`);
- a montagem das páginas do quadro de avisos (`NCBoardWnd::LoadPage`, `SetCommands`,
  `NCBoardPage.cpp`);
- o `std::wstring` e o `std::vector` sem STL;
- o cursor colorido.

Na faixa `0x10022000..0x10030000` sobram 22 `todo`, todos dos grupos 1, 2, 3 (o `DllMain`) e 8.

## 10. O banco (zerar o `todo` do modo gancho)

**Feito na rodada 10 (27/09/2026).** O `build_db.py` dá os três status abaixo, com o motivo na
coluna `notes` de cada linha, e o `PROGRESSO.md` os conta à parte. Resultado sobre o master
`eb11e6a`: `todo` 0; `conv` 359 (183 deleting destructors + 176 `InternalConstructor`);
`fase5` 543 (542 `init` + `DllMain`); `morto` 7 (`db/morto.csv`: os grupos 4, 5, 6 e 8; o grupo
7 ganhou gancho em `0167153`). A regra do `morto` confere os chamadores no original: se aparecer
um que não é nosso, o `build_db.py` avisa e a função volta a `todo`.

A proposta, como foi escrita na rodada 9. Hoje `work` = `game` + `init`. Três status novos no
`build_db.py` tirariam os 810 da conta sem esconder nada:
- **`conv`:**
  - `scalar deleting destructor`: o slot 3 das vtables e os 7 da seção 1, reconhecíveis pelo
    formato `call dtor; test [esp+8], 1; … delete; ret 4`, com o destrutor feito;
  - `InternalConstructor`: pelo `classes.json`, com o construtor feito;
  - e os 100 `InternalConstructor` que faltam, no `extra_functions.csv`.
- **`fase5`:** o kind `init` e o `DllMain`.
- **`morto`:** os 7 dos grupos 4 a 8, numa lista curta à mão. Outra forma é a regra "todo
  chamador feito e nenhuma referência por dado fora de vtable"; o script desta auditoria faz essa
  conta.

O `PROGRESSO.md` passaria a mostrar `todo` = 0 e as três contagens à parte.

## 11. Cruzamento com o Standalone.md

**O mesmo diagnóstico.** O Standalone.md (lobby, sobre `0e9567f`) conta 273 funções `game`
restantes. Aqui são 268: saíram os 4 templates do console e o `FindRef`.

**Grupos 1, 2 e 3 = §3 e §4 dele.** Esta auditoria acrescenta:
- os 7 deleting destructors sem nome;
- as 17 classes cujo construtor não tem os argumentos padrão que o `InternalConstructor` usa;
- os VAs dos 100 `InternalConstructor` fora do banco;
- os três VAs por classe (apêndice D);
- o `autoclass` de `NConsoleWnd`/`NL2Font` em `+0x90`. Por isso as linhas com o nome errado
  `dynamic atexit destructor` são 176, e não 178; os outros dois são os `sub_` que o §4 cita.

**§2 (os imports "só por código original").** São exatamente os grupos 1
(`UObject::operator delete`) e 6 (`GIsSSE`).

**§6 (os andaimes que bloqueiam o link).**
- O `StockCall` do `NCInventoryWnd.h` (`0x1004dda0`) saiu nesta rodada.
- No `agent/status` ficam:
  - 6 `NW_STOCK`: o `0x10057ba0` do `NCPrivateShopWnd.cpp`, e `_CxxThrowException`, `_Xlen` e
    `_Xran` no `NStdLib.cpp`/`NStl.cpp`;
  - 6 `StockCall`: `NCNoticeWnd` ×2, o `NewStockWnd` do `NCConsole.cpp` e o `std::exception` ×3.
- A `agent/menus` já tira o `0x10057ba0` e os dois do `NCNoticeWnd`.

**Fora do banco (conferido).** Na parte do jogo (abaixo do CRT), as faixas do Ghidra cobrem o
`.text` inteiro, menos:
- os 100 `InternalConstructor`;
- 52 tabelas de salto e de índice de `switch` (3.757 B de dados no `.text`);
- enchimento de alinhamento (`lea esp, [esp]`, `mov edi, edi`…; 762 B);
- dois pontos de retorno de `catch` em funções nossas (`0x1000d356` do `NStdString::Copy`,
  `0x100288a1` do `NStlWString::Copy`);
- a cauda do `appMemcpy`;
- um enchimento inalcançável dentro de `NCPawnViewr_Simulator::OnStart` (`0x1008dfb4`).

Nenhuma outra função escapou do banco.

## 12. Texto dos `check()` (cosmético)

Só muda a mensagem do erro fatal; a lógica já era a do original.

**Feito nesta rodada (`1c9af73`): os `check()` que o original expandiu do `UnTemplate.h`.**
- São os do `TArray::Remove` (linhas 515–517) e do `TArray::Pop` (398).
- Agora dão o texto, o caminho (`e:\work\2004_06_07\warfare2110\core\inc\UnTemplate.h`) e a linha
  do original, pelo `NW_CHECK_AT` do `UE2Core.h`.
- O `TArray::Pop` entrou no `UE2Core.h` no lugar do ajudante `TIntStack`: o `0x10029fc0` agora é
  `TIntArray::Pop`, e o `PopAlign` do `NHtmlLayout.cpp` o usa.

**Ainda com o nosso texto (3 lugares, em arquivos de outros trabalhadores):**

| Original | Nosso |
|---|---|
| `Src\NConsoleWindow.cpp` 1601 `m_pLobbyWnd` | `check(LobbyWnd)` no `AddCharacterInfo` (`NConsoleWndLogin.cpp`) |
| `Src\NConsoleWindow.cpp` 1612 `m_pConsole` | `check(Desktop)` no `AcceptGameStart` (`NConsoleWndLogin.cpp`) |
| `Src\NPawnViewerWnd.cpp` 1053 `pController` | `check(pController)` no `OnNameComboBox` (`NCPawnView_CharacterSelectWnd.cpp`) |

- As cópias do MSView e do NpcView já chamam o `appFailAssert` com o texto do original (clan).
- Para os três: o `appFailAssert` explícito, como no MSView. No `pController`, onde o nome bate,
  também serve o `NW_CHECK_AT`.

## Como foi conferido

- **Chamadores:** toda `call`/`jmp` rel32 do `.text` e toda relocação que aponta para a função.
  - Cada uma foi atribuída à função do banco que a contém. Nas funções de várias faixas valem
    as faixas do Ghidra; os funclets de EH contam pelo pai.
  - "Morto" = só chamadores `done`/`verified`.
  - Os `InternalConstructor` e os deleting destructors só têm referências por dado (o
    inicializador e a vtable).
- **Alvos:** tem gancho o destrutor chamado por cada deleting destructor e o construtor chamado por
  cada `InternalConstructor` (os 176, inclusive os fora do banco).
- **Números:** o `db/functions.csv` depois do `cycle.py` de `ccc2123` tem 5.368 funções; work
  3.553 (754.891 B); feitas 2.743 (95,60 %).

## Apêndices

### A. `scalar deleting destructor` (183, 6.564 B)

| VA | B | classe | VA | B | classe | VA | B | classe |
|---|---|---|---|---|---|---|---|---|
| `10001660` | 33 | NCObject | `10001720` | 33 | NCCmdTarget | `100017e0` | 36 | NCWnd |
| `100018a0` | 36 | NControl | `10001920` | 36 | NCButton | `10002100` | 36 | NCExButton |
| `100021e0` | 36 | NCPushButton | `10002300` | 36 | NCRadioButton | `100033f0` | 36 | NCCompositeBox |
| `10003cc0` | 36 | NCCandidateBox | `10003e80` | 36 | NCCheckBox | `10004df0` | 36 | NCComboBox |
| `10005380` | 36 | NCCountSpin | `100057f0` | 36 | NCProgressCtrl | `10006310` | 36 | NCDialogBox |
| `10009bb0` | 36 | NCEditBox | `1000be80` | 36 | NCFrameCtrl | `1000c530` | 36 | NCResizeFrame |
| `1000d5b0` | 34 | NStdLogicError | `1000d720` | 33 | NCGDevice | `1000da10` | 34 | NStdLengthError |
| `1000f300` | 36 | NCIME | `1000f460` | 36 | NCIMEManager | `10014680` | 36 | NCKoreanIME |
| `10014750` | 36 | NCJapaneseIME | `100148a0` | 36 | NCTaiwanIME | `10014be0` | 36 | NCChineseIME |
| `100157c0` | 36 | NCIcon | `10016930` | 36 | NCScrollWnd | `10017750` | 36 | NCItemWnd |
| `100185a0` | 36 | NCListBox | `100190d0` | 36 | NCHeaderCtrl | `1001a180` | 36 | NCListCtrl |
| `10020a60` | 36 | NCMultiEdit | `10021a40` | 36 | NCSliderBar | `10022a80` | 36 | NCTabControl |
| `100241c0` | 36 | NHtmlAnalyzer | `100246e0` | 33 | NCAbstractMain | `10025b80` | 36 | NCVirtualWndMain |
| `10025d40` | 34 | NHtmlTagTable | `10026b80` | 36 | NCNPCHtmlViewer | `10027800` | 36 | NCTestHtmlViewer |
| `100278a0` | 36 | NCBoardWnd | `10028980` | 36 | NCHtmlComboBox | `10028a60` | 36 | NCHtmlViewer |
| `10029630` | 36 | NCTutorialHtmlViewer | `100296f0` | 36 | NCBBSHtmlViewer | `1002c760` | 35 | NHtmlParserBase |
| `1002d000` | 34 | NHtmlParser | `10030660` | 36 | NCFileLinker | `10030720` | 36 | NCActionLinker |
| `10030860` | 36 | NCHtmlButton | `10030c40` | 36 | NCHtmlPushButton | `10031020` | 36 | NCHtmlMultiEdit |
| `10032770` | 36 | NCHtmlEdit | `10032f40` | 36 | NCJHtmlEdit | `10032fe0` | 36 | NCHtmlSliderBar |
| `10034d30` | 36 | NCActionItemListWnd | `10034fd0` | 36 | NCPledgeWnd | `100364e0` | 36 | NCActionWnd |
| `10036fd0` | 36 | NCActionItemWnd | `10037470` | 36 | NCAutoLogWnd | `100390d0` | 36 | NCChatFilterWnd |
| `1003cb70` | 36 | NCChatListBox | `1003d6a0` | 36 | NCChatWnd | `1003e830` | 36 | NCGMPetitionWnd |
| `1003e8d0` | 36 | NCMainWnd | `10042630` | 36 | NCConsole | `10047300` | 36 | NCPetitionWnd |
| `10055890` | 36 | NConsoleWnd | `10057a50` | 36 | NCDebugWnd | `10059d90` | 36 | NCDeliverWnd |
| `1005a3d0` | 36 | NCSelectDeliverWnd | `1005ae70` | 36 | NCDeliverItemWnd | `1005b850` | 33 | NL2Font |
| `1005c600` | 36 | NCDetailStatusWnd | `1005c680` | 36 | NCStoreWnd | `1005c700` | 36 | NCQuestWnd |
| `1005c760` | 36 | NCGMInventoryWnd | `1005ca90` | 36 | NCGMSkillWnd | `1005cb30` | 36 | NCGMMagicSkillWnd |
| `1005ce00` | 36 | NCGMDetailStatusWnd | `1005e330` | 36 | NCGMQuestWnd | `1005e9d0` | 36 | NCGMStoreWnd |
| `1005eae0` | 36 | NCGMPledgeWnd | `1005ef60` | 36 | NCGMActionWnd | `1005f2a0` | 36 | NCGMWnd |
| `1005fdd0` | 36 | NCEquipItemWnd | `1005fe50` | 36 | NCInvenItemWnd | `1005fee0` | 36 | NCSkillItemWnd |
| `1005ff30` | 36 | NCGMEquipItemWnd | `1005ffd0` | 36 | NCGMInvenItemWnd | `10060420` | 36 | NCGMSkillItemWnd |
| `100659d0` | 36 | NCInventoryWnd | `10068a40` | 36 | NCLoadingWnd | `10069240` | 36 | NCPawnSelectWnd |
| `10069e60` | 36 | NCLobbyWnd | `1006aaa0` | 36 | NCAuthWnd | `1006b180` | 36 | NCLoginWnd |
| `1006d630` | 36 | NCVIPShopItemListWnd | `1006d9d0` | 36 | NCVIPShopItemInfoWnd | `1006e840` | 36 | NCVIPShopItemListBox |
| `1006ea50` | 36 | NCVIPShopNeededItemWnd | `1006eb10` | 36 | NCVIPShopWnd | `1006fd40` | 36 | NCMSViewrWnd |
| `10070150` | 36 | NCMSView_CharacterSelectWnd | `10070d10` | 36 | NCMSView_InvenItemWnd | `100731d0` | 36 | NCSkillWnd |
| `10073310` | 36 | NCMagicSkillWnd | `10074640` | 36 | NCMatchOptionWnd | `10075150` | 36 | NCMatchWnd |
| `100759f0` | 36 | NCTestUIWnd | `10075ec0` | 36 | NCMatchListCtrl | `10076f60` | 36 | NCRestartMenuWnd |
| `10077460` | 36 | NCSystemMenuWnd | `10077af0` | 36 | NCMenuWnd | `10078030` | 36 | NCMessageWnd |
| `100793e0` | 36 | NCMinimapWnd | `10079710` | 36 | NCNoticeInfoWnd | `100798e0` | 36 | NCNoticeWnd |
| `10079d90` | 36 | NCWarningWnd | `1007aa30` | 36 | NCNpcViewrWnd | `1007b260` | 36 | NCNpcView_CharacterSelectWnd |
| `1007beb0` | 36 | NCNpcView_InvenItemWnd | `1007c5f0` | 36 | NCNpcView_CharacterSelect_List | `1007df70` | 36 | NCVideoOptionWnd |
| `10080240` | 36 | NCAudioOptionWnd | `100809a0` | 36 | NCGameOptionWnd | `100817e0` | 36 | NCOptionWnd |
| `10084100` | 36 | NCPawnInfoWnd | `10084330` | 36 | NCPawnSetupWnd | `10085770` | 36 | NCPawnCreateWnd |
| `10086e80` | 36 | NCSelectCtrl | `10088850` | 36 | NCPawnView_InvenTextWnd | `10088af0` | 36 | NCPawnViewrWnd |
| `10089190` | 36 | NCPawnView_CharacterSelectWnd | `10089650` | 36 | NCPawnViewr_MixAnimWnd | `1008a0f0` | 36 | NCPawnViewr_HairChangeWnd |
| `1008c770` | 36 | NCPawnView_InvenItemWnd | `1008d090` | 36 | NCPawnView_CharacterSelect_List | `1008e160` | 36 | NCPawnViewr_Simulator |
| `10090f10` | 36 | NCPetDetailStatusWnd | `10091600` | 36 | NCPetActionWnd | `10091ef0` | 36 | NCPetInventoryWnd |
| `10092000` | 36 | NCPetWnd | `10092e30` | 36 | NCSummonedDetailStatusWnd | `10092ed0` | 36 | NCSummonedActionWnd |
| `100930c0` | 36 | NCSummonedWnd | `100941f0` | 36 | NCPetInvenItemWnd | `10094e60` | 36 | NCPetitionListBox |
| `10098930` | 36 | NCPrivateBuyWnd | `1009a100` | 36 | NCPrivateBuyItemWnd | `1009da40` | 36 | NCPrivateShopWnd |
| `1009eed0` | 36 | NCPrivateShopItemWnd | `100a1730` | 36 | NCQuestInfoWnd | `100a17e0` | 36 | NCQuestItemWnd |
| `100a1dc0` | 36 | NCRadarWnd | `100a23a0` | 36 | NCServerSelectWnd | `100a3340` | 36 | NCServerSelectListCtrl |
| `100a5e20` | 36 | NCShopWnd | `100a74a0` | 36 | NCShopItemWnd | `100ab2f0` | 36 | NCShortCutWnd |
| `100ac8c0` | 36 | NCSiegeInfoMainWnd | `100ad340` | 36 | NCSiegeDateInfoWnd | `100ad630` | 36 | NCSiegeInfoListCtrl |
| `100ad760` | 36 | NCSiegePartInfoWnd | `100aebc0` | 36 | NCStatusWnd | `100af400` | 36 | NCTargetStatusWnd |
| `100affc0` | 36 | NCPlayerStatusWnd | `100b0a80` | 36 | NCPetStatusWnd | `100b0ef0` | 36 | NCSummonedStatusWnd |
| `100b1410` | 36 | NCPartyStatusWnd | `100b1740` | 36 | NCPartyWnd | `100b37e0` | 36 | NCAbnormalStatusWnd |
| `100b69d0` | 36 | NCStoreItemWnd | `100b9bd0` | 36 | NCTradeWnd | `100ba430` | 36 | NCTradeItemWnd |
| `100bb4c0` | 36 | NCTrainListWnd | `100bc410` | 36 | NCTrainWnd | `100bc810` | 36 | NCTrainInfoWnd |
| `100bcb70` | 36 | NCTrainListBox | `100bcef0` | 36 | NCTutorialViewerWnd | `100bd230` | 36 | NCTutorialQuestionWnd |

### B. `InternalConstructor` no banco (76, 1.107 B)

| VA | B | classe | VA | B | classe | VA | B | classe |
|---|---|---|---|---|---|---|---|---|
| `10003a40` | 14 | NCCompositeBox | `10003d50` | 18 | NCCandidateBox | `10004250` | 23 | NCCheckBox |
| `100042f0` | 14 | NCObject | `10004e80` | 14 | NCComboBox | `10005090` | 14 | NControl |
| `10005a80` | 20 | NCCountSpin | `1000d960` | 14 | NCGDevice | `10010210` | 16 | NCIME |
| `10017880` | 14 | NCItemWnd | `100186e0` | 14 | NCListBox | `10019900` | 14 | NCHeaderCtrl |
| `1001a430` | 14 | NCListCtrl | `10020c90` | 14 | NCMultiEdit | `10021790` | 14 | NCScrollWnd |
| `10021ce0` | 14 | NCSliderBar | `10022b50` | 14 | NCTabControl | `10023d20` | 14 | NCWnd |
| `10024280` | 14 | NHtmlAnalyzer | `10025c30` | 14 | NCVirtualWndMain | `10027cb0` | 14 | NCBoardWnd |
| `1002a490` | 14 | NCHtmlViewer | `10033260` | 16 | NCFileLinker | `10036880` | 14 | NCActionItemListWnd |
| `10037310` | 22 | NCActionItemWnd | `10037620` | 14 | NCAutoLogWnd | `1003aa70` | 14 | NCChatFilterWnd |
| `1003d680` | 14 | NCChatListBox | `100417f0` | 14 | NCMainWnd | `100427c0` | 14 | NCConsole |
| `10057b00` | 14 | NCDebugWnd | `1005a810` | 14 | NCSelectDeliverWnd | `1005b760` | 14 | NCDeliverItemWnd |
| `1005b830` | 14 | NL2Font | `1005fc50` | 14 | NCGMInventoryWnd | `10060700` | 16 | NCGMEquipItemWnd |
| `10066190` | 14 | NCInventoryWnd | `100688f0` | 16 | NCEquipItemWnd | `10068b50` | 14 | NCLoadingWnd |
| `1006bab0` | 14 | NCAuthWnd | `1006dd80` | 14 | NCVIPShopItemListWnd | `1006f090` | 14 | NCVIPShopItemListBox |
| `10070b60` | 14 | NCMSView_CharacterSelectWnd | `10073750` | 14 | NCSkillWnd | `10073d20` | 14 | NCSkillItemWnd |
| `10075c50` | 14 | NCMatchOptionWnd | `100767b0` | 14 | NCMatchListCtrl | `10077f70` | 14 | NCRestartMenuWnd |
| `10079470` | 14 | NCMinimapWnd | `1007a170` | 14 | NCNoticeInfoWnd | `10082100` | 14 | NCVideoOptionWnd |
| `10086340` | 14 | NCPawnInfoWnd | `10087410` | 14 | NCSelectCtrl | `1008c280` | 14 | NCPawnView_CharacterSelectWnd |
| `1008e220` | 14 | NCPawnViewr_Simulator | `100938d0` | 14 | NCPetDetailStatusWnd | `10094370` | 14 | NCPetInvenItemWnd |
| `10094bc0` | 14 | NCPetitionWnd | `10095590` | 14 | NCPetitionListBox | `1009ad80` | 14 | NCPrivateBuyItemWnd |
| `1009fdd0` | 14 | NCPrivateShopItemWnd | `100a1c70` | 14 | NCQuestInfoWnd | `100a1e50` | 14 | NCRadarWnd |
| `100a69a0` | 14 | NCShopWnd | `100a7fe0` | 14 | NCShopItemWnd | `100ab380` | 20 | NCShortCutWnd |
| `100ad600` | 14 | NCSiegeDateInfoWnd | `100b3870` | 16 | NCAbnormalStatusWnd | `100b60d0` | 14 | NCStoreWnd |
| `100b72e0` | 14 | NCStoreItemWnd | `100ba1c0` | 14 | NCTradeWnd | `100ba990` | 14 | NCTradeItemWnd |
| `100bc550` | 14 | NCTrainListWnd | `100bcb30` | 14 | NCTrainInfoWnd | `100bcd20` | 14 | NCTrainListBox |
| `100bd340` | 14 | NCTutorialViewerWnd |  |  |  |  |  |  |

### C. `InternalConstructor` fora do banco (100, 1.444 B)

| VA | B | classe | VA | B | classe | VA | B | classe |
|---|---|---|---|---|---|---|---|---|
| `10002560` | 14 | NCButton | `10002570` | 14 | NCExButton | `10002580` | 14 | NCPushButton |
| `10002590` | 14 | NCRadioButton | `10005aa0` | 14 | NCProgressCtrl | `10007f60` | 14 | NCDialogBox |
| `1000b350` | 22 | NCEditBox | `1000c6e0` | 14 | NCFrameCtrl | `1000c6f0` | 16 | NCResizeFrame |
| `10010220` | 16 | NCIMEManager | `10015260` | 16 | NCKoreanIME | `10015270` | 16 | NCJapaneseIME |
| `10015280` | 16 | NCTaiwanIME | `10015290` | 16 | NCChineseIME | `10015b70` | 14 | NCIcon |
| `10027cc0` | 14 | NCNPCHtmlViewer | `10027cd0` | 14 | NCTestHtmlViewer | `1002a4a0` | 14 | NCTutorialHtmlViewer |
| `1002a4b0` | 14 | NCBBSHtmlViewer | `10033270` | 16 | NCActionLinker | `10033280` | 14 | NCHtmlButton |
| `10033290` | 14 | NCHtmlPushButton | `100332a0` | 14 | NCHtmlSliderBar | `100332b0` | 14 | NCHtmlMultiEdit |
| `100332c0` | 18 | NCHtmlEdit | `100332e0` | 20 | NCJHtmlEdit | `100338d0` | 14 | NCHtmlComboBox |
| `10036890` | 14 | NCPledgeWnd | `100368a0` | 14 | NCActionWnd | `1003d690` | 14 | NCChatWnd |
| `100574f0` | 14 | NConsoleWnd | `1005a820` | 14 | NCDeliverWnd | `1005fc60` | 14 | NCGMSkillWnd |
| `1005fc70` | 14 | NCGMMagicSkillWnd | `1005fc80` | 14 | NCGMDetailStatusWnd | `1005fc90` | 14 | NCGMQuestWnd |
| `1005fca0` | 14 | NCGMStoreWnd | `1005fcb0` | 14 | NCGMPledgeWnd | `1005fcc0` | 14 | NCGMActionWnd |
| `1005fcd0` | 14 | NCGMWnd | `10060710` | 14 | NCGMInvenItemWnd | `10060720` | 14 | NCGMSkillItemWnd |
| `10068900` | 14 | NCInvenItemWnd | `1006a710` | 14 | NCPawnSelectWnd | `1006a720` | 14 | NCLobbyWnd |
| `1006bac0` | 14 | NCLoginWnd | `1006dd90` | 14 | NCVIPShopItemInfoWnd | `1006f0a0` | 14 | NCVIPShopNeededItemWnd |
| `1006f0b0` | 14 | NCVIPShopWnd | `10070b70` | 14 | NCMSViewrWnd | `10071570` | 14 | NCMSView_InvenItemWnd |
| `10073760` | 14 | NCMagicSkillWnd | `10075c60` | 14 | NCMatchWnd | `10075c70` | 14 | NCTestUIWnd |
| `10077f80` | 14 | NCSystemMenuWnd | `10077f90` | 14 | NCMenuWnd | `100781d0` | 14 | NCMessageWnd |
| `1007a180` | 14 | NCNoticeWnd | `1007a190` | 14 | NCWarningWnd | `1007bd60` | 14 | NCNpcView_CharacterSelectWnd |
| `1007bd70` | 14 | NCNpcViewrWnd | `1007d240` | 14 | NCNpcView_InvenItemWnd | `1007d250` | 14 | NCNpcView_CharacterSelect_List |
| `10082110` | 14 | NCAudioOptionWnd | `10082120` | 14 | NCGameOptionWnd | `10082130` | 14 | NCOptionWnd |
| `10086350` | 14 | NCPawnSetupWnd | `10086360` | 14 | NCPawnCreateWnd | `1008c290` | 14 | NCPawnView_InvenTextWnd |
| `1008c2a0` | 14 | NCPawnViewrWnd | `1008c2b0` | 14 | NCPawnViewr_MixAnimWnd | `1008c2c0` | 14 | NCPawnViewr_HairChangeWnd |
| `1008d940` | 14 | NCPawnView_CharacterSelect_List | `1008d950` | 14 | NCPawnView_InvenItemWnd | `100938e0` | 14 | NCPetActionWnd |
| `100938f0` | 14 | NCPetInventoryWnd | `10093900` | 14 | NCPetWnd | `10093910` | 14 | NCSummonedDetailStatusWnd |
| `10093920` | 14 | NCSummonedActionWnd | `10093930` | 14 | NCSummonedWnd | `10094bd0` | 14 | NCGMPetitionWnd |
| `10099670` | 14 | NCPrivateBuyWnd | `1009e550` | 14 | NCPrivateShopWnd | `100a13c0` | 14 | NCQuestWnd |
| `100a1c80` | 14 | NCQuestItemWnd | `100a30c0` | 14 | NCServerSelectWnd | `100a39d0` | 14 | NCServerSelectListCtrl |
| `100ace90` | 14 | NCSiegeInfoMainWnd | `100adf20` | 14 | NCSiegeInfoListCtrl | `100adf30` | 14 | NCSiegePartInfoWnd |
| `100b3300` | 16 | NCStatusWnd | `100b3310` | 16 | NCTargetStatusWnd | `100b3320` | 16 | NCPlayerStatusWnd |
| `100b3330` | 16 | NCPetStatusWnd | `100b3340` | 16 | NCSummonedStatusWnd | `100b3350` | 16 | NCPartyStatusWnd |
| `100b3360` | 14 | NCPartyWnd | `100b3370` | 14 | NCDetailStatusWnd | `100bc560` | 14 | NCTrainWnd |
| `100bd350` | 14 | NCTutorialQuestionWnd |  |  |  |  |  |  |

### D. Inicialização estática, por classe (178 × 3)

Na ordem do original, que é a do link.
- `UClass`: o inicializador do `IMPLEMENT_CLASS` (construtor nativo + `atexit`).
- `autoclass`: o inicializador do export; no banco, com o nome errado `dynamic atexit destructor`
  ou como `sub_`.
- `~UClass`: o destrutor registrado no `atexit` (`sub_` no banco).

| classe | UClass | autoclass | ~UClass | classe | UClass | autoclass | ~UClass |
|---|---|---|---|---|---|---|---|
| NCButton | `100cf9d0` | `100cfa50` | `100d5e70` | NCExButton | `100cfa60` | `100cfae0` | `100d5e80` |
| NCPushButton | `100cfaf0` | `100cfb70` | `100d5e90` | NCRadioButton | `100cfb80` | `100cfc00` | `100d5ea0` |
| NCCandidateBox | `100cfc10` | `100cfc90` | `100d5eb0` | NCCompositeBox | `100cfca0` | `100cfd20` | `100d5ec0` |
| NCCheckBox | `100cfd30` | `100cfdb0` | `100d5ed0` | NCCmdTarget | `100cfdc0` | `100cfe40` | `100d5ee0` |
| NCComboBox | `100cfe50` | `100cfed0` | `100d5ef0` | NControl | `100cfee0` | `100cff60` | `100d5f00` |
| NCCountSpin | `100cff70` | `100cfff0` | `100d5f10` | NCProgressCtrl | `100d0000` | `100d0080` | `100d5f20` |
| NCDialogBox | `100d0090` | `100d0110` | `100d5f30` | NCEditBox | `100d0120` | `100d01a0` | `100d5f40` |
| NCFrameCtrl | `100d01b0` | `100d0230` | `100d5f50` | NCResizeFrame | `100d0240` | `100d02c0` | `100d5f60` |
| NCGDevice | `100d02d0` | `100d0350` | `100d5f70` | NCIMEManager | `100d0360` | `100d03e0` | `100d5f80` |
| NCIME | `100d03f0` | `100d0470` | `100d5f90` | NCKoreanIME | `100d0480` | `100d0500` | `100d5fa0` |
| NCJapaneseIME | `100d0510` | `100d0590` | `100d5fb0` | NCTaiwanIME | `100d05a0` | `100d0620` | `100d5fc0` |
| NCChineseIME | `100d0630` | `100d06b0` | `100d5fd0` | NCIcon | `100d06c0` | `100d0740` | `100d5fe0` |
| NCItemWnd | `100d0750` | `100d07d0` | `100d5ff0` | NCListBox | `100d0820` | `100d08a0` | `100d6020` |
| NCHeaderCtrl | `100d08b0` | `100d0930` | `100d6030` | NCListCtrl | `100d0940` | `100d09c0` | `100d6040` |
| NCMultiEdit | `100d09d0` | `100d0a50` | `100d6050` | NCObject | `100d0a60` | `100d0ae0` | `100d6060` |
| NCScrollWnd | `100d0af0` | `100d0b70` | `100d6070` | NCSliderBar | `100d0b80` | `100d0c00` | `100d6080` |
| NCTabControl | `100d0c10` | `100d0c90` | `100d6090` | NCWnd | `100d0ca0` | `100d0d20` | `100d60a0` |
| NHtmlAnalyzer | `100d0d30` | `100d0db0` | `100d60b0` | NCAbstractMain | `100d0dc0` | `100d0e40` | `100d60c0` |
| NCVirtualWndMain | `100d0e50` | `100d0ed0` | `100d60d0` | NCHtmlViewer | `100d0f00` | `100d0f80` | `100d60e0` |
| NCTutorialHtmlViewer | `100d0f90` | `100d1010` | `100d60f0` | NCBBSHtmlViewer | `100d1020` | `100d10a0` | `100d6100` |
| NCNPCHtmlViewer | `100d10b0` | `100d1130` | `100d6110` | NCTestHtmlViewer | `100d1140` | `100d11c0` | `100d6120` |
| NCBoardWnd | `100d11d0` | `100d1250` | `100d6130` | NCHtmlComboBox | `100d1260` | `100d12e0` | `100d6160` |
| NCFileLinker | `100d12f0` | `100d1370` | `100d6170` | NCActionLinker | `100d1380` | `100d1400` | `100d6180` |
| NCHtmlButton | `100d1410` | `100d1490` | `100d6190` | NCHtmlPushButton | `100d14a0` | `100d1520` | `100d61a0` |
| NCHtmlMultiEdit | `100d1530` | `100d15b0` | `100d61b0` | NCHtmlEdit | `100d15c0` | `100d1640` | `100d61c0` |
| NCJHtmlEdit | `100d1650` | `100d16d0` | `100d61d0` | NCHtmlSliderBar | `100d16e0` | `100d1760` | `100d61e0` |
| NCActionItemWnd | `100d1770` | `100d17f0` | `100d61f0` | NCActionItemListWnd | `100d1800` | `100d1880` | `100d6200` |
| NCPledgeWnd | `100d1890` | `100d1910` | `100d6210` | NCActionWnd | `100d1920` | `100d19a0` | `100d6220` |
| NCAutoLogWnd | `100d19b0` | `100d1a30` | `100d6230` | NCChatListBox | `100d1a40` | `100d1ac0` | `100d6240` |
| NCChatWnd | `100d1ad0` | `100d1b50` | `100d6250` | NCChatFilterWnd | `100d1b60` | `100d1be0` | `100d6260` |
| NCConsole | `100d1bf0` | `100d1c70` | `100d6270` | NCMainWnd | `100d1c80` | `100d1d00` | `100d6280` |
| NConsoleWnd | `100d1d10` | `100d1da0` | `100d6290` | NCDebugWnd | `100d1db0` | `100d1e30` | `100d62a0` |
| NCDeliverItemWnd | `100d1e40` | `100d1ec0` | `100d62b0` | NCDeliverWnd | `100d1ed0` | `100d1f50` | `100d62c0` |
| NCSelectDeliverWnd | `100d1f60` | `100d1fe0` | `100d62d0` | NL2Font | `100d1ff0` | `100d2080` | `100d62e0` |
| NCGMEquipItemWnd | `100d2090` | `100d2110` | `100d62f0` | NCGMInvenItemWnd | `100d2120` | `100d21a0` | `100d6300` |
| NCGMInventoryWnd | `100d21b0` | `100d2230` | `100d6310` | NCGMSkillItemWnd | `100d2240` | `100d22c0` | `100d6320` |
| NCGMSkillWnd | `100d22d0` | `100d2350` | `100d6330` | NCGMMagicSkillWnd | `100d2360` | `100d23e0` | `100d6340` |
| NCGMDetailStatusWnd | `100d23f0` | `100d2470` | `100d6350` | NCGMQuestWnd | `100d2480` | `100d2500` | `100d6360` |
| NCGMStoreWnd | `100d2510` | `100d2590` | `100d6370` | NCGMPledgeWnd | `100d25a0` | `100d2620` | `100d6380` |
| NCGMActionWnd | `100d2630` | `100d26b0` | `100d6390` | NCGMWnd | `100d26c0` | `100d2740` | `100d63a0` |
| NCEquipItemWnd | `100d2750` | `100d27d0` | `100d63b0` | NCInvenItemWnd | `100d27e0` | `100d2860` | `100d63c0` |
| NCInventoryWnd | `100d2870` | `100d28f0` | `100d63d0` | NCLoadingWnd | `100d2900` | `100d2980` | `100d63e0` |
| NCPawnSelectWnd | `100d2990` | `100d2a10` | `100d63f0` | NCLobbyWnd | `100d2a20` | `100d2aa0` | `100d6400` |
| NCAuthWnd | `100d2ab0` | `100d2b30` | `100d6410` | NCLoginWnd | `100d2b40` | `100d2bc0` | `100d6420` |
| NCVIPShopItemListBox | `100d2bd0` | `100d2c50` | `100d6430` | NCVIPShopItemListWnd | `100d2c60` | `100d2ce0` | `100d6440` |
| NCVIPShopNeededItemWnd | `100d2cf0` | `100d2d70` | `100d6450` | NCVIPShopItemInfoWnd | `100d2d80` | `100d2e00` | `100d6460` |
| NCVIPShopWnd | `100d2e10` | `100d2e90` | `100d6470` | NCMSView_InvenItemWnd | `100d2ea0` | `100d2f20` | `100d6480` |
| NCMSViewrWnd | `100d2f30` | `100d2fb0` | `100d6490` | NCMSView_CharacterSelectWnd | `100d2fc0` | `100d3040` | `100d64a0` |
| NCSkillItemWnd | `100d3050` | `100d30d0` | `100d64b0` | NCSkillWnd | `100d30e0` | `100d3160` | `100d64c0` |
| NCMagicSkillWnd | `100d3170` | `100d31f0` | `100d64d0` | NCMatchOptionWnd | `100d3200` | `100d3280` | `100d64e0` |
| NCMatchListCtrl | `100d3290` | `100d3310` | `100d64f0` | NCMatchWnd | `100d3320` | `100d33a0` | `100d6500` |
| NCTestUIWnd | `100d33b0` | `100d3430` | `100d6510` | NCRestartMenuWnd | `100d3440` | `100d34c0` | `100d6520` |
| NCSystemMenuWnd | `100d34d0` | `100d3550` | `100d6530` | NCMenuWnd | `100d3560` | `100d35e0` | `100d6540` |
| NCMessageWnd | `100d35f0` | `100d3670` | `100d6550` | NCMinimapWnd | `100d3680` | `100d3700` | `100d6560` |
| NCNoticeInfoWnd | `100d3710` | `100d3790` | `100d6570` | NCNoticeWnd | `100d37a0` | `100d3820` | `100d6580` |
| NCWarningWnd | `100d3830` | `100d38b0` | `100d6590` | NCNpcView_InvenItemWnd | `100d38c0` | `100d3940` | `100d65a0` |
| NCNpcViewrWnd | `100d3950` | `100d39d0` | `100d65b0` | NCNpcView_CharacterSelect_List | `100d39e0` | `100d3a60` | `100d65c0` |
| NCNpcView_CharacterSelectWnd | `100d3a70` | `100d3af0` | `100d65d0` | NCVideoOptionWnd | `100d3b00` | `100d3b80` | `100d65e0` |
| NCAudioOptionWnd | `100d3b90` | `100d3c10` | `100d65f0` | NCGameOptionWnd | `100d3c20` | `100d3ca0` | `100d6600` |
| NCOptionWnd | `100d3cb0` | `100d3d30` | `100d6610` | NCSelectCtrl | `100d3d40` | `100d3dc0` | `100d6620` |
| NCPawnInfoWnd | `100d3dd0` | `100d3e50` | `100d6630` | NCPawnSetupWnd | `100d3e60` | `100d3ee0` | `100d6640` |
| NCPawnCreateWnd | `100d3ef0` | `100d3f70` | `100d6650` | NCPawnView_InvenItemWnd | `100d3f80` | `100d4000` | `100d6660` |
| NCPawnViewrWnd | `100d4010` | `100d4090` | `100d6670` | NCPawnView_CharacterSelectWnd | `100d40a0` | `100d4120` | `100d6680` |
| NCPawnView_InvenTextWnd | `100d4130` | `100d41b0` | `100d6690` | NCPawnViewr_MixAnimWnd | `100d41c0` | `100d4240` | `100d66a0` |
| NCPawnView_CharacterSelect_List | `100d4250` | `100d42d0` | `100d66b0` | NCPawnViewr_HairChangeWnd | `100d42e0` | `100d4360` | `100d66c0` |
| NCPawnViewr_Simulator | `100d4370` | `100d43f0` | `100d66d0` | NCPetDetailStatusWnd | `100d4400` | `100d4480` | `100d66e0` |
| NCPetActionWnd | `100d4490` | `100d4510` | `100d66f0` | NCPetInvenItemWnd | `100d4520` | `100d45a0` | `100d6700` |
| NCPetInventoryWnd | `100d45b0` | `100d4630` | `100d6710` | NCPetWnd | `100d4640` | `100d46c0` | `100d6720` |
| NCSummonedDetailStatusWnd | `100d46d0` | `100d4750` | `100d6730` | NCSummonedActionWnd | `100d4760` | `100d47e0` | `100d6740` |
| NCSummonedWnd | `100d47f0` | `100d4870` | `100d6750` | NCPetitionListBox | `100d4880` | `100d4900` | `100d6760` |
| NCPetitionWnd | `100d4910` | `100d4990` | `100d6770` | NCGMPetitionWnd | `100d49a0` | `100d4a20` | `100d6780` |
| NCPrivateBuyItemWnd | `100d4a30` | `100d4ab0` | `100d6790` | NCPrivateBuyWnd | `100d4ac0` | `100d4b40` | `100d67a0` |
| NCPrivateShopItemWnd | `100d4b50` | `100d4bd0` | `100d67b0` | NCPrivateShopWnd | `100d4be0` | `100d4c60` | `100d67c0` |
| NCQuestInfoWnd | `100d4c70` | `100d4cf0` | `100d67d0` | NCQuestItemWnd | `100d4d00` | `100d4d80` | `100d67e0` |
| NCQuestWnd | `100d4d90` | `100d4e10` | `100d67f0` | NCRadarWnd | `100d4e20` | `100d4ea0` | `100d6800` |
| NCServerSelectListCtrl | `100d4eb0` | `100d4f30` | `100d6810` | NCServerSelectWnd | `100d4f40` | `100d4fc0` | `100d6820` |
| NCShopItemWnd | `100d4fd0` | `100d5050` | `100d6830` | NCShopWnd | `100d5060` | `100d50e0` | `100d6840` |
| NCShortCutWnd | `100d50f0` | `100d5170` | `100d6850` | NCSiegeInfoListCtrl | `100d5180` | `100d5200` | `100d6860` |
| NCSiegePartInfoWnd | `100d5210` | `100d5290` | `100d6870` | NCSiegeDateInfoWnd | `100d52a0` | `100d5320` | `100d6880` |
| NCSiegeInfoMainWnd | `100d5330` | `100d53b0` | `100d6890` | NCAbnormalStatusWnd | `100d53c0` | `100d5440` | `100d68a0` |
| NCStatusWnd | `100d5450` | `100d54d0` | `100d68b0` | NCTargetStatusWnd | `100d54e0` | `100d5560` | `100d68c0` |
| NCPlayerStatusWnd | `100d5570` | `100d55f0` | `100d68d0` | NCPetStatusWnd | `100d5600` | `100d5680` | `100d68e0` |
| NCSummonedStatusWnd | `100d5690` | `100d5710` | `100d68f0` | NCPartyStatusWnd | `100d5720` | `100d57a0` | `100d6900` |
| NCPartyWnd | `100d57b0` | `100d5830` | `100d6910` | NCDetailStatusWnd | `100d5840` | `100d58c0` | `100d6920` |
| NCStoreItemWnd | `100d58d0` | `100d5950` | `100d6930` | NCStoreWnd | `100d5960` | `100d59e0` | `100d6940` |
| NCTradeItemWnd | `100d59f0` | `100d5a70` | `100d6950` | NCTradeWnd | `100d5a80` | `100d5b00` | `100d6960` |
| NCTrainListBox | `100d5b10` | `100d5b90` | `100d6970` | NCTrainListWnd | `100d5ba0` | `100d5c20` | `100d6980` |
| NCTrainInfoWnd | `100d5c30` | `100d5cb0` | `100d6990` | NCTrainWnd | `100d5cc0` | `100d5d40` | `100d69a0` |
| NCTutorialViewerWnd | `100d5d50` | `100d5dd0` | `100d69b0` | NCTutorialQuestionWnd | `100d5de0` | `100d5e60` | `100d69c0` |

