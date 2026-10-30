# NWindow do C3 — infraestrutura e o que falta

Este repositório é o fork do projeto do C2 (`<C2 project>`, só
leitura, commit `9e5d91c`, que por sua vez é fork do C1) apontado para o `NWindow.dll` do cliente do C3
(`orig/NWindow.dll`, md5 `f0362880cdec6eedc6b8f860a0bc9945`, TimeDateStamp `0x4329071F` = 15/09/2005).
O estudo que levou a isto é o [C3_NWindow_Viabilidade.md](notas/C3_NWindow_Viabilidade.md); o método é o do
[PLANO.md](PLANO.md); o que o porte do C2 ensinou está em [C2_Porte.md](notas/C2_Porte.md) e
[C2_Agentes.md](notas/C2_Agentes.md). Este documento diz o que **já está retargetado**, as decisões, e o que falta.

## Decisões (10/10/2026)

| Tema | Decisão |
|---|---|
| Binário alvo | o `system\NWindow.dll` do cliente "atualizado" de out/2005 (`Lineage II - Chronicle 3 - Rise of Darkness`), escolhido pelo dono. Arquivo de 23/11/2013 de um pacote de comunidade: não há cópia íntegra nem `.BAK`, então remendos (no C2 eram 8) não são conhecidos; ver `orig/README.md` |
| Base da source | **a nossa source do C2, já portada e verificada** (3.444 ganchos, 92 % do código), não um fork limpo do C1 |
| Rota | C2 → C3 → C4: o C4 sai depois, sobre a source do C3 (`C4_NWindow_Viabilidade.md`) |
| Escopo inicial | modo gancho primeiro, como no C2. Messenger (`Msn*`, ~45 KB de virtuais + ~320 auxiliares), replay/observador/benchmark/transmissão de GM ficam como o original (stubs que saltam para o código stock) até a base de jogo passar na conferência; só depois entram, ou ficam de fora do standalone (como o nProtect). Reavaliar ao fim da onda 2 |
| Servidor de teste | o `PORTE-C3.md` do Lucera diz que o servidor loga, cria personagem e entra no mundo com o cliente C3 |

## A imagem do C3 (o que o C2 tinha fixo no código)

| O quê | C1 | C2 | C3 | Onde |
|---|---|---|---|---|
| md5 / TimeDateStamp | `2dacb6eb…` / `0x40DBD452` | `0111f657…` / `0x4227239B` | `f0362880…` / `0x4329071F` | `tools/nwd/image.py`, `src/re/NwMain.cpp`, `tools/smoke/nwd_smoke_standalone.cpp` |
| Tamanho / SizeOfCode / entry point | | 1.835.028 / `0x106000` / `0xE90B1` | 2.732.052 / `0x174000` / `0x14E13A` | `src/re/NwMain.cpp` |
| `.text` | 0xD59D0 bytes | `0x10001000`–`0x10106020` | `0x10001000`–`0x101748E0` (0x1738E0) | |
| stubs `jmp [IAT]` | `0x100BD360–0x100BD4D3` | `0x100DFDD0–0x100DFED7` (44), `0x100E7180–0x100E7239` (30) | `0x10144A60–0x10144B67` (44), `0x1014C0C0–0x1014C18B` (34: o `WININET` entra) | `tools/build_db.py` (`THUNK_BLOCKS`) |
| objeto do nProtect (GameGuard) | | `0x100DFEE0–0x100E717F`, 83 funções, 28.658 bytes | `0x10144B70–0x1014C0BF`, 84 funções, 29.318 bytes | `tools/verify_standalone.py` (`NPROTECT_LIB`) |
| primeiro objeto da libcmt (`CRT_LO`) | `0x100BD4D4` | `0x100E723A` | `0x1014C18C` (as mesmas cinco primeiras funções: 28, 11, 63, 24 e 63 bytes) | `tools/build_db.py` |
| funclets `Unwind@` / `__ehhandler` | | `0x100F7130–0x100FD265` (956) | `0x1015C240–0x10168595` (2.214; os `Catch@`, 1.579, ficam dentro do código do jogo) | derivado |
| inicializadores estáticos | | `0x100FD280–0x10106010` | `0x101685B0–0x10173480` (309 classes × 3 + globais); o `.text` termina em `0x101748DB` | derivado |
| `_purecall` | `0x100BE4FC` | `0x100E84EA` | `0x1014D4CA` (14 referências de vtable) | `db/names.csv`, `tools/verify_hooks.py`, `verify_standalone.py` |
| `appMemcpy` (SSE, fora de linha) | `0x1000E320` | `0x10001790` | `0x10001790` (o mesmo endereço; o sled de `movsb` em `0x100017E0` é fragmento) | `db/names.csv`, `db/not_functions.csv` |
| `wcscpy` | `0x100BD5BD` | `0x100E78E8` | `0x1014C9BB` | `db/names.csv` |
| slots do `UObject` | 24 | 25 | 25 (nenhum virtual novo no Core) | `tools/build_db.py` |
| classes / exports / imports | 178 / 180 / 335 | 225 / 227 / 452 | **309 / 311 / 530** | |
| imports por DLL | | Engine 98, Core 161, KERNEL32 109, ADVAPI32 29, IMM32 16, USER32 15, WS2_32 12, `npkcrypt` 8, VERSION 3, WINMM 1 | Engine 115, Core 198, KERNEL32 113, ADVAPI32 30, USER32 17, IMM32 16, WS2_32 13, WININET 8, `npkcrypt` 8, VERSION 6, SHLWAPI 4, GDI32 1, WINMM 1 | |
| compilador | VC 7.0 `13.00.9466` | o mesmo | o mesmo (Rich header: build 9466, linker 7.0) | |

Vtables do Engine do C3 (de `analysis/engine_vtables.txt`, lidas do `Engine.dll`/`Core.dll` do cliente): `UL2ConsoleWnd`
390 slots (C2: 320), `UL2NetHandler` 276 (257), `FL2NetNotify` 403 (352), `UCanvas` 77 (69), `UL2Font` 34 (34), `UObject` 25 (25).
No lado do `NWindow`: `NCWnd` 0xC8 bytes e 68 slots (C2: 0x98 / 67), `NConsoleWnd` 405 slots (335).

## Estado (10/10/2026)

A cadeia do C2 (`python tools/cycle.py`) roda no C3 **com a source de jogo vazia**: o alvo `NWindowDecomp` compila só o
`src/re`, o instalador injeta o import `NWindowDecomp.dll` numa cópia do `NWindow.dll` do C3, a cópia carrega fora do jogo
(o `DllMain` da nossa DLL roda, confere a imagem e registra `hooks: 0 installed, 0 registered, 0 errors`), e
`verify_hooks.py` (`0 ok, 0 explained, 0 unexplained`) e `regcheck.py` (`0 problem(s)`) passam com zero hooks.

Na imagem do C3 (sem nenhuma source nossa):

* `tools/verify_standalone.py --stock`: PE 12 ok, imports 14 ok (inclui `gdi32`, `shlwapi` e `wininet` na linha de link),
  resources 7 ok, exports 315 ok, classes 310 ok (as 309 `IMPLEMENT_CLASS`), vtables 320 ok. As falhas que sobram são da
  **source**, que ainda é a do C2: 626 (`no NW_DECLARE_CLASS` / `no NW_CHECK_VTABLE`) e 9 vtables de classes que só o C2
  tinha (`MSNSocket`, `NCMSNWnd`, `NCMatchOptionWnd`, `NCNoticeWnd`, `NCRecipeTree*`, `NCTVItem`, `NCTestUIWnd`).
* `tools/smoke_standalone.py --control-only`: `ALL STAGES OK` — o `NWindow.dll` **original** do C3 pelo caminho do engine
  (Core do C3, `StaticLoadClass` de `NWindow.ConsoleWnd`/`NWindow.L2Font`): as 309 classes registradas batem com
  `analysis/classes.json`, `ConsoleWnd` (0x39E8 bytes) e `L2Font` (0x48) construídos.
* Dois ajustes de ferramenta que o C3 exigiu: (1) os inicializadores estáticos do C3 constroem `FString` globais
  (cópia de 52 bytes de um literal), o que aloca pelo `GMalloc` do Core: o host de teste tem de dar o heap **antes** de
  mapear o `NWindow.dll` (`HookModeStandIns` em `tools/smoke/nwd_smoke_standalone.cpp`, chamada pelo `nwd_smoke` e pela
  etapa `bind`); no C2 isto não era preciso. (2) o `pefile` nomeia só os 8.192 primeiros exports e o `Engine.dll` do C3
  tem 9.670: `verify_standalone.py` lê a tabela à mão (`engine_vtables.read_exports`). O `nwd_smoke` ganhou um
  tratador de exceções de primeira chance (código, módulo+RVA, registradores e retornos da pilha) para diagnosticar
  falhas na carga.

### O banco (`db/functions.csv`): 10.992 funções, 3.701 com nome

Classificação (`kind`): 5.607 `game`, 939 `init`, 3.843 `eh`, 518 `crt`, 84 `thunk`, 1 `fragment`; trabalho (game + init) =
**6.546 funções, 1.312.496 bytes** (C2: 4.661 funções, 917.473 bytes). Prólogos para o instalador: 6.546, 0 pequenos
demais para `jmp`, 8 sem trampolim. Origem dos nomes: `class` 1.447, `vslot` 920, `guard` 718, `dtor` 309, `ghidra` 303
(thunks e CRT), `manual` 3, `export` 1. Sem nome: 3.843 funclets de EH (não precisam), 297 do CRT, 3.138 `game`
(`sub_xxxxxxxx` ou `Classe::vfnNNN`) e 12 `init`: o `fmap` (etapa 3) dá nome à maior parte dos 3.138 por herança do C2.

## Etapas

| # | Etapa | Estado |
|---|---|---|
| 0 | fork + git + binários de referência (`orig/`), dados do C2 postos de lado (`db/c2`, `analysis/c2`) | feito |
| 1 | **infraestrutura**: constantes da imagem, análise (Ghidra, classes, EH), banco de funções, prólogos, build do hook sem source de jogo, carga fora do jogo, verificação | feito (10/10/2026), ver "Estado" |
| 2 | **fundação do Engine** (`include/ue2`): declarações dos imports novos, vtables do C3, tabela de slots do console, layouts das estruturas compartilhadas | feito (10/10/2026): `ue2_check` ALL OK; ver abaixo |
| 3 | **pipeline de porte C2 → C3** (`tools/port`: `match`, `maps`, `layout`, `vslots`, `eligible`, `gen`) sobre a source do C2 | feito (10/10/2026): primeira geração, 763 ganchos, todos os juízes passam; tag `generated-v1`; ver abaixo |
| 4 | **ondas de porte à mão** (o que o pipeline não copia: categoria D, funções novas, 93 classes novas) com o ciclo como juiz | a fazer |
| 5 | **standalone** (`src/standalone`) e conferência de remendos | em andamento (29/10/2026, preparação `agent/sa-prep`, modelo = a etapa 5 do C4): tudo compila com `NW_HOOK_MODE=0` e o link do `NWindowStandalone` só falha nos pontos (383 símbolos); lista de trabalho `python tools/port/sa_sites.py` (1.122 pontos em 180 arquivos), 7 lanes `sa-*` em `db/lanes_s5.json`, guia [notas/C3_Standalone.md](notas/C3_Standalone.md). GameGuard continua fora (no-op, `NGameGuard.h`), mas o standalone responde à consulta do Engine |

### O que os mapas mediram (10/10/2026, base = a source do C2)

| Passo | Resultado |
|---|---|
| `match.py` → `db/fmap.csv` | 2.973 das 3.967 funções de jogo do C2 têm par no C3: **81,0 % dos bytes** (716.904 de 884.559). A 705 funções / 69 KB (igual), B 1.147 / 133 KB (igual sem os deslocamentos), C 315 / 87 KB (mesma forma, constantes diferentes), D 806 / 428 KB (forma diferente). 5.768 das 10.992 funções do C3 passam a ter nome (3.701 antes) |
| `maps.py` → `db/amap.csv`, `analysis/port/` | 17.628 pares de endereço (111 ambíguos), votos de campo para 284 classes, 2.767 imediatos que mudaram |
| `layout.py` → `db/layout.json` | 216 classes em comum: 1.119 campos mapeados, 518 derivados, 21 sem mapa; 2 campos cujo trecho do C2 invade o seguinte no C3 (para olhar à mão) |
| `vslots.py` → `db/vslots.json` | `NCObject` 25 → 25, `NCCmdTarget` 38 → 39, `NCWnd` 67 → 68, `NConsoleWnd` 335 → 405 slots; 1.687 slots sem par no C2 e 1.354 slots do C2 que sumiram (a maioria é deslocamento de numeração) |
| `eligible.py` → `db/eligible.csv` | 2.167 pares, **1.727 elegíveis (226.776 de 289.187 bytes)**: corpos do C2 que compilam contra os cabeçalhos do C3 e dão a mesma função; 417 com deslocamentos de outros objetos por conferir na compilação |


### Etapa 2: a fundação do Engine (feita em 10/10/2026)

`python tools/ue2_check/run_check.py` dá **ALL OK** sobre `include/ue2` e as DLLs do cliente do C3: compila nos dois
modos (hook e standalone) sem diagnóstico, liga as 614 referências de API contra bibliotecas de importação geradas dos
`.def` do C3 (`third_party/ue_import`: Core 2.586 exports, Engine 9.670) e confere **1.530 slots virtuais** (842 iguais à DLL em
slot e assinatura, 688 puros, 0 errados). O `ue2_check_headers.py` prova 61 offsets contra o script do `Engine.u` do C3
(nenhum difere; 102 são membros nativos, sem prova pelo script).

| Parte | Como foi feita |
|---|---|
| Classes com script (`AActor` +4, `APawn` 0x14F4 → 0x16CC, `AController` +4, `UCanvas` 0x13C → 0x1A4, `UEngine` +0x10, `UGameEngine` +0x3C) | `tools/ue2_shift_layout.py`: as variáveis do `Engine.u` (inclusive os membros das structs: `NMagicInfo` 124 → 168, `NPrimeActionParam` 36 → 40) são as âncoras; os fillers são refeitos; as 38 traduções que o `ue2_check_headers.py` prova coincidem |
| `UL2ConsoleWnd` 0x3518 → 0x392C (sem script) | lido do código do `Engine.dll` do C3: `ResetSelectCharacterInfo`, `SpawnDefaultCharacter` (18 registros, tabela de 12 bytes por entrada), `SelectedCharacterNum`: `Engine` em `+0x48`, `DefaultCharacterInfo` em `+0x4C`, `SelectCharacterInfo` em `+0x27AC`, tamanho do `User` 0x230; os 0x120 bytes que o C2 guardava entre as duas listas sumiram |
| `User` 0x1F8 → 0x230 | getters de uma instrução (`GetNickColor`, `GetPledgeID`, `GetPrivateStoreState`…), construtor (`memset 0x230`; 4 registros de 12 bytes antes de `Pawn`) e os votos fortes das funções pareadas |
| `FL2GameData` 0x605FC → 0x7AF90, `FL2SystemMsgData` 0x24 → 0x30 | `ue2_gamedata_regions.py` (cópia do construtor) + os métodos exportados do próprio Engine (`Clear`, `Load`, `Get*`: o mesmo nome nas duas DLLs pareia cada deslocamento do C2 com o do C3) |
| Nomes dos slots do console | `tools/ue2_remap_console_slots.py`: 270 das 275 linhas da tabela do C2 passam ao slot do C3 (o mapa de slots do `NConsoleWnd`, validado contra as vtables: puro ↔ puro); 5 ficam de fora (3 de sala de grupo, `ShowMapItem`, `IsCtrlDown`); os **75 callbacks novos** seguem como `PureSlotNNN` (a nota de cada um tem o `ret N` e o nome do banco) |
| Declarações novas | os 47 imports do NWindow do C3 que nenhum cabeçalho tinha (efeitos de apêndice do `APawn`, `FL2ReplayManager`, `USkillVisualEffect`, `FNMagicInfo`, virtuais de fonte TrueType/chat/observador do `UCanvas`, `FL2GameData::GetZoneID/GetQuestData`, `FBlowFish`, `FString::Locs/Substitute/!=`, `GMsn*`/`GIs*`/`GL2*`, `FArchive::Attach/DetachRoughLoader`…) e as cinco assinaturas que o Engine do C3 mudou (`SetMagicInfo`, `MakeBMPToDXT1`, `SetParserParam`, `GetTextInformation`, `Load`); `UCanvas::DrawRadar` sumiu |

O que **não** está provado e a conferência do ciclo (`tools/port/verify_data.py`: deslocamentos do stock contra os nossos) tem de
fechar: as faixas do `User` entre os getters (`0xE8..0x110` e `0x154..0x194` foram movidas com os vizinhos), os bytes novos do
`User` (`+0xD0..0xDC`, `+0x1F2..0x200`, `+0x208..0x230`), o bloco novo de `0x88` bytes em `FL2GameData+0x28` e as regiões novas
`0x7A790..0x7AB78`, o `+0x44` novo do `UL2ConsoleWnd`, `ERunningOS` (só o nome do tipo), e as estruturas de registro que o
NWindow lê por ponteiro e que não são classes (`NItemEntry`, `NViewItemData`, `NViewNpcData`, `NDialogInfo`, `FL2MagicSkillData`
e os demais `FL2*Data`), que ficam para o gerador (etapa 3) e para o `verify_data`. `PetInfo` (0xAC) e `ItemInfo` (0x30) não
mudaram de tamanho (o `GetPetInfo` do Engine ainda copia 0x2B dwords).

### Etapa 3: a primeira geração da source do C3 (feita em 10/10/2026)

`python tools/port/gen.py` sobre a nossa source do C2 (a árvore-base) e os mapas do C3 gera `include/nwindow` e `src/nwindow`;
o laço **gerar → compilar → juízes → demotar** (db/demote.csv) fechou assim:

| Passo | Resultado |
|---|---|
| geração | 1.195 unidades portadas por cópia, ~2.000 stubs (`NW_NAKED … NW_STOCK(va C3)`: saltam para a função original), 229 sem par (classe ou método que o C3 não tem) |
| compilação contra `include/ue2` do C3 | 0 erros nos cabeçalhos; 20 unidades demotadas (membros e classes que não existem mais: `NCWnd::LocalX/Y/W`, `NCLoginWnd::NoticeWnd`, `NCTVItem`…) |
| `verify_hooks` (pops de `ret`, slots virtuais, imports contra a função do C3) | 224 demotadas |
| `regcheck` (chamador do original guarda um registrador através de um gancho) | 13 demotadas |
| `verify_data` (constantes, deslocamentos, strings) | 195 demotadas, 70 explicadas (o quadro `guard()` que o C3 não tem naquela função) |
| `check_breakers` | 1 demotada |
| **final** | smoke `hooks: 763 installed, 763 registered, 0 errors`; verify 763 ok, 0 não explicados; regcheck 0 problemas; verify_data 693 ok + 70 explicados, 0 diferenças; nenhum gancho alcança um `NW_UNPORTED()`; instalado em `system_nwtest` |

**763 funções reimplementadas = 68.578 bytes, 5,2 % do código** (4.809 `todo` com 1,2 MB; 940 só da fase 5; 34 por convenção). O estado
é o de "primeira build verificada" do C2 (1.240 ganchos); o resto vem das ondas manuais. Nada disso foi executado dentro do jogo ainda:
os juízes são estáticos (forma, slots, imports, conjuntos de dados) e o carregamento é fora do jogo.

O que o gerador teve de aprender (todas as mudanças estão em `tools/port` e valem para o C4): o tamanho novo do `User` nos layouts de
classe (`SIZE_CHANGED`); o tamanho do pai de uma classe do Engine para o primeiro filler (`NConsoleWnd : UL2ConsoleWnd`);
classes que o cliente novo não tem (arquivos, blocos de classe, includes, unidades, nomes adiantados em `NWindow.h`); virtuais
que sumiram deixam de ser `virtual`; virtuais novos declarados **na ordem do slot** (`db/vslots_manual.csv` para os movimentos
que o alinhamento monótono não diz: `NCWnd::IsShowWindow` 40 → 39, `PtInRect` 41 → 40, `SetSize` 42 → 43, herdados por todos os
descendentes); classes abstratas sem virtuais passam seus slots à de baixo (`NCCmdTarget`); um gancho por endereço do original
(o compilador do cliente dobrou corpos idênticos); stubs de construtor mantêm os inicializadores de base; ajudantes `template`/`struct`
locais escondidos (`#if 0`); `db/signature_changed.csv` (`PtInRect`/`SetSize` passam a `FLOAT` no C3: quem os chama por cópia vira stub).
As ferramentas de juízes ganharam `--demote` (regcheck, check_breakers), `demote_data.py`, leitura dos logs do MSBuild no `demote.py`
e o `verify_hooks.py` agora segue o stub `NW_STOCK` para dentro da imagem original.

**O que as ondas manuais (etapa 4) têm pela frente**, em ordem de rendimento:

1. **`NCWnd`, `NControl`, `NCGDevice`, `NCObject`** (a "foundation lane" do C2): `NCWnd` ganhou 0x30 bytes e perdeu `LocalX/Y/W` (o retângulo
   local passou a `FLOAT`, `PtInRect`/`SetSize` mudaram); tudo herda disso, e 20 das demoções de compilação nascem aí.
2. **As 224 demoções do `verify_hooks`** pelas causas mais comuns (assinatura com um argumento a mais: 76 `ret` diferentes; o quadro
   `guard()` do C2 que o C3 não tem: funções que são só um repasse para a base).
3. **As 195 do `verify_data`** (deslocamentos de campos que o layout derivou sem evidência, constantes de cor) e os `User`/`FL2GameData`
   com faixas sem prova (etapa 2).
4. **As 93 classes novas** (messenger fora do escopo inicial), e os 4.809 `todo` pelo método do C2: `tools/port/packet.py <função>` +
   decompilação, o ciclo como juiz.

### Etapa 4: as pistas manuais (em andamento, 10/10/2026)

**Achados que mudaram os juízes** (tudo em `tools/port`):

* Os cabeçalhos da base carregavam os **endereços da base** nas macros `NW_GLOBAL` / `nw::Addr` / `StockCall` / `NW_STOCK` (e o
  `NClasses.gen.h` era o do C2): `GConsole`, `GFocusEntry`, `GShiftDown`… apontavam para outros dados deste cliente, e nenhum juiz
  olhava globais. O gerador passa esses endereços pelo mapa (`db/amap.csv`, `db/fmap.csv`; 0xBADxxxxx quando não há par), o
  `verify_data.py` compara agora as **globais gravávei­s do stock** que cada função toca (`g_nwBase + off` do nosso lado) e o
  `NClasses.gen.h` foi regerado da tabela de classes do C3 (`tools/gen_class_ids.py`).
* O mapa de slots do `NCWnd` perdia cinco movimentos (`OnTimer` 35→36, `OnPaint/OnDrawToolTip/OnChar` 55–57→56–58, `OnMouseWheel`
  66→67; lidos do `WindowProc`/`DefWindowProc` do C3 em `db/vslots_manual.csv`) e uma assinatura mudada vale para **toda
  sobrescrita** (`db/decl_manual.csv`, classe `*`): sobrescrever `PtInRect`/`SetSize` com os tipos antigos criava slots novos.
* Entradas manuais do gerador: `db/layout_manual.csv` (a posição que o código do C3 dá a um campo, `gone`, retipagem),
  `db/decl_manual.csv`, `db/renames.csv` (`SetWindowRect`→`SetPosition`, `SetSize`→`SetWindowSize`: os nomes dos `guard()`),
  `db/fmap_manual.csv` (`tools/port/suggest_fmap.py`: pares que os chamadores de mesma forma nomeiam), `db/hand_owned.txt`
  (arquivos escritos à mão: o gerador não os escreve e sabe o que ganchearam). `eligible.py` usa os votos para os campos de uma
  classe-base do Engine (`NConsoleWnd` +0x44 → +0x48 destravou 173 funções). `gen.py --cat-c --derived`: copia também a
  categoria C (mesma forma, constantes diferentes) e os campos sem evidência de voto; **o juiz decide** (verify_data compara os
  conjuntos de constantes e deslocamentos).

**`NCWnd` à mão** (`src/nwindow/NCWnd.cpp`, `include/nwindow/NCWnd.h`): layout C3 de 0xC8 bytes (retângulo `FLOAT` em +0x38 que o
`PtInRect` recebe por valor, quatro listas de entradas coladas em +0x84, âncora em +0xB4); construtor, destrutor, `SetFocus`,
`ShowWindow`/`HideWindow`/`Restore`/`Minimize`, `SetPosition`/`SetWindowSize`/`CenterInParent`, arrastar (`OnMouseMove`,
`OnLButtonDown/Up`) e o sistema novo de janelas coladas e âncoras (`StickWnd`, `UnstickWnd`, `IsStuck`, `GetStuckWndList`,
`GetPositioningRect`, `RearrangePosition`, `MoveChildWindow` com as nove âncoras, `ClipToParent`). Faltam `OnCreate`/`OnDestroy`
(precisam dos construtores do `NCFrameCtrl` de três argumentos, `NCIcon` e `NToolTipSkin`, que ainda são `int3`) e o `Snap`
(0x100321d0, 2.247 bytes).

**Mais achados da pista (mesmo dia)**: o tamanho dos campos de `FL2GameData` que o cabeçalho-base escrevia como número
(`0x4B4E8` → `GL2GameData.SysString`, `db/text_patches.csv`); a posição das janelas filhas do `NCConsole` lida do
`OnCreate` do C3 (`StaticAllocateObject` + `*(this+off) = ...`: `tools/port/layout_from_create.py` faz isso para todas as janelas,
mas só onde os votos não colocaram o campo: onde os dois discordaram, os votos estavam certos); `layout.py` trata as
posições manuais como âncoras (campo que a âncora empurra para além do objeto não é colocado); `vslots.py`: um slot herdado
mantém a identidade quando a classe o sobrescreve e os virtuais próprios que ninguém pareou vão por posição (89 → 72 slots
"gone"); `verify_data` compara constantes de ponto flutuante pelo valor (o compilador desta build guarda operandos x87 como
`double`) e ignora os números de linha dos `assert` do `TArray`; `verify_hooks` ignora o slot 0 (o `new` do stock é inline).

**Resultado**: `hooks: 1163 installed, 1163 registered, 0 errors` = 102.836 bytes, 7,8 % do código (4.384 `todo`, 1,16 MB);
verify_hooks 0 não explicados, regcheck 0, verify_data 1.050 ok + 113 explicados, breakers 0. Instalado em `system_nwtest`
(`NWindowDecomp.ini`: `Enabled=0` desliga tudo, `Disable=NCWnd::*` / endereço / método para bisecção). O laço é
`bash tools/port/judge_loop.sh` (gera com `--cat-c --derived`, compila, roda os juízes e demota o que falha).

**O que sobra**, em ordem de rendimento: (1) as 350 demoções do `verify_data` (a diferença exata de cada uma está em
`db/demote.csv`; 160 diferenças de constante distintas, 120 de deslocamento: cada uma é uma linha de `db/layout_manual.csv` ou de
`db/text_patches.csv`; `tools/port/disp_votes.py` diz que campo foi para onde); (2) as 200 do `verify_hooks` (uma fração
vem de `NCWnd::OnCreate/OnDestroy`, que esperam os construtores do `NCFrameCtrl` de três argumentos, do `NCIcon` e do
`NToolTipSkin`, ainda `int3`); (3) `NCWnd::Snap` (0x100321d0, 2.247 bytes); (4) as ~4.000 funções que só o C3 tem, pelo método
do C2, sem par: decompilação (`analysis/ghidra/decomp`) + `tools/disasm.py` + o ciclo como juiz.

### O primeiro teste no jogo (10/10/2026): o que os juízes não viam

A primeira execução (`system_nwtest/L2.exe`, 1.123 ganchos, `generated-v6`) caiu em `NConsoleWnd::Initialize` (General protection
fault) antes do login. O `NWindowDecomp.log` traz agora as linhas `[exception]` (um *vectored exception handler* em
`src/re/NwMain.cpp`: código, módulo+RVA, registradores e os endereços de código da pilha) e `tools/crash_lookup.py` converte o
módulo+RVA em função (mapa do `NWindowDecomp.dll` ou `db/functions.csv`). A causa foi `NCWnd::GetLocalPos`/`GetLocalRect`:
o stock devolve `Out` em `eax` e `NCHtmlViewer::ShowWindow` lê o ponteiro; a nossa versão era `void`. Nenhum juiz olha o valor de
retorno, então há **duas verificações novas** no fim do `judge_loop.sh`:

* `tools/port/check_stubs.py`: os bytes que o stub declara contra o `ret N` do stock (26 funções mudaram de assinatura no C3);
* `tools/port/check_retval.py`: gancho `void`, ou que devolve `bool`/`BYTE`/`WORD`, ou não declarado `FLOAT`/`DOUBLE`, cujo chamador
  do stock lê `eax` (ou `st0`) logo depois da chamada (chamadas diretas: as virtuais não aparecem). Resultado atual: 0.

A segunda execução caiu em `int3` (código 80000003) dentro de `NToolTipInfo::NToolTipInfo`, chamado por `NItemEntry::NItemEntry`,
chamado pelo construtor (nosso) de `NCHennaInfoWnd`, dentro do `NCConsole::OnCreate` do stock. O `check_breakers` dizia "0
funções alcançam um `int3`" porque só reconhecia como `int3` a função que **começa** com ele; um construtor/destrutor
`NW_UNPORTED()` tem o `int3` atrás do prólogo e da construção dos membros. Correções:

* `check_breakers.py` acha também o `int3` no meio do corpo (desassembla; ignora as funções de CRT), segue as chamadas até 10 níveis e
  olha as vtables: um *breaker* num slot de classe cujo construtor é gancho conta como alcançado (`--verbose` lista os demais);
* `gen.py`: um construtor não portado com endereço conhecido chama o construtor do stock (`nw::StockCallAt<void>(va, this, args)`; o
  nosso constrói os membros vazios e o do stock refaz o objeto). Um destrutor não pode ser repassado (o `naked` é recusado com
  membros que pedem desenrolamento, C3068, e uma chamada destruiria os membros duas vezes): quem o alcança volta ao stock
  (`check_breakers --demote`); foram 13 ganchos (1.123 → 1.110);
* `check_stubs.py` olha também o DLL: nenhuma chamada compilada alcança um stub de assinatura mudada (hoje 21 stubs, 0 chamadas).

A terceira execução entrou no mundo e caiu: `c0000409` (*fast-fail*) em `___report_gsfailure` de `NWindowDecomp.dll`. Nenhuma
exceção chega ao *handler* vetorial nesse caso (o `__fastfail` não despacha), então o log ficou sem `[exception]`; o dado veio do
Visualizador de Eventos (Application Error 1000 / WER 1001: código `0xc0000409`, dados `00000002` = cookie do `/GS`, deslocamento
`0x38828`). Agora `NwMain.cpp` troca o início do `__report_gsfailure` do CRT por um salto para uma rotina nossa que escreve
`[gsfailure]` (cookie, registradores, endereços de código da pilha) antes de falhar do mesmo jeito. A causa: `NItemInfo` cresceu
4 bytes no C3 (o construtor do stock grava `[+0x17A8]`; o quadro do `ReceiveSkillList` do stock é exatamente `0x17AC`) e o nosso
local tinha `0x17A8`: os 4 bytes caem no cookie. Nenhum juiz via, porque o código compara constantes e deslocamentos, não o
tamanho dos objetos. O que mudou:

* `db/struct_sizes.csv` (lido pelo `gen.py`): os structs que o C3 aumentou (`NItemInfo` +4, `NItemEntry` +0xC, `NToolTipInfo` +4,
  `NToolTipLine` +4, `NListCell`, `NListRow`, `NCCoolTimeIcon` +0x10, `NDialogInfo` 0x78→0xA0, `NMacroEntry` +0x5C,
  `NShortCutItem` +0x38, `NPledgeMember`, `NVIP*`…). A evidência é o construtor do C3 (`tools/port/size_audit.py`), as constantes
  que as funções pareadas mudam (`analysis/port/constants.csv`: o mesmo `push`/`imul`/`sub esp` com o tamanho velho e o novo) e
  o quadro de pilha das funções que os têm como local. O gerador troca o `NW_ASSERT_SIZE`, fecha o struct com `BYTE _PadC3[n]`
  (os deslocamentos que os juízes verificaram não se mexem), desloca os `NW_ASSERT_OFFSET` dos derivados e faz a janela que o
  embute usar o tamanho novo (`SIZE_CHANGED`: os preenchimentos `_UnknownXXXX[a - b]` encolhem);
* `tools/port/check_frames.py`: o quadro do gancho contra o do stock (o nosso leva +4 do cookie com a mesma pilha); hoje só
  sobram 4 funções de teclado/IME de buffer de texto, que escrevem textos curtos;
* `tools/port/size_audit.py`: o construtor do stock grava além do tamanho afirmado no cabeçalho? (hoje: 0).

Hooks: 1.107 (de 1.110), todos os juízes passam.

A quarta execução passou pelo `ReceiveSkillList` e caiu em `UNetworkHandler::Tick` (`General protection fault`, `c0000005` com
EIP 0): o `Tick` do Engine despacha um pacote `Ex` pela tabela `0x108F6618` (entradas de `0x104` bytes: ponteiro + nome; o índice
0 é `ExDummyPacket`, sem função) e chamou o ponteiro nulo. O Engine não é nosso; a pergunta é o que levou um pacote a esse índice
(memória do pacote corrompida por um gancho nosso, ou o servidor). Para responder sem bisecção às cegas, cada gancho agora
entra por um pequeno trampolim que anota a entrada num anel de 256 posições (`[Debug] Trace=1`, padrão) e os dois *loggers* de
falha escrevem as últimas 40 (`trace-NN`, a mais recente primeiro). A bisecção por classe continua em `tools/hook_ini.py`.

**Resultado do teste (10/10/2026, 18:18)**: com `generated-v8` + o *trace* (1.107 ganchos) o cliente passou do login, da escolha
do personagem e entrou no mundo; o `NWindowDecomp.log` ficou sem `[exception]` nem `[gsfailure]`. A queda anterior em
`UNetworkHandler::Tick` era do personagem (segundo o usuário, um item de outra crônica no char: o servidor mandou um pacote `Ex`
de índice 0), não de um gancho. Falta o teste funcional (janelas, inventário, loja, mapa, janelas coladas).

### Onda 1 (11/10/2026): oito frentes em paralelo

Oito agentes, cada um em um worktree (`..\wt\<frente>`, ramo `agent/<frente>`), com arquivos de classes marcados como feitos à mão
em `db/hand_owned.txt` e o protocolo de `docs/notas/C3_Agentes.md`. O limite semanal de uso da conta interrompeu todos no meio;
foram retomados com a ordem de fechar o estado atual e relatar. Resultado fundido na `main` (tag `wave1-merged`):

| frente | ganchos do ramo | o que entrou |
|---|---|---|
| edit | 1.255 | NCMultiEdit/NCEditBox inteiros, as IMEs, NCCandidateBox, NCHtmlEdit |
| console-core | 1.238 | NCConsole::OnCreate/OnDestroy, janelas, login, entrada, texto do console |
| console-events | 1.279 | ~150 tratadores de pacote e pedidos (itens, clã, party, quests, atalhos, ações) |
| chatopt | 1.232 | chat, opções (vídeo/áudio/jogo), diálogos, menus, atalhos parcial |
| pawn | 1.239 | visores de personagem, criação/seleção, lobby, login, lista de servidores |
| status | 1.258 | party, status do jogador/alvo/pet/invocação, quest, petição, cerco, quadro |
| items | 1.270 | NItemEntry/NCItemWnd reconstruídos para o C3, lojas privadas de compra e venda |
| tooltip-html | 1.287 | construtores de NToolTip*/NCIcon/NCFrameCtrl, `NCWnd::OnCreate/OnDestroy/Snap`, HTML, janela virtual |

O conjunto fundido exigiu quatro acertos entre frentes: a assinatura de `NToolTipSkin::Draw` (a frente do tooltip passou X/Y
por valor, a dos itens ainda passava ponteiros: o código do C3 empilha valores); o *slot* 71 de `NCItemWnd` (um virtual novo
deslocava `Clear` e os de todas as derivadas); o gancho duplicado de `0x100aa860` (a mesma função serve `NCSystemMenuWnd` e
`NCBoardWnd`); e `gen.py` não re-emite um *placeholder* `NewVirtualN` que um arquivo à mão já define. `check_breakers.py` passou a
descer o código por controle de fluxo (nada de tabelas de salto nem de padding) e a ignorar o `int3` depois de chamada a função
da CRT que não retorna: de 13 achados falsos para 0. Resultado: **2.225 ganchos instalados**, `verify_hooks` 2.141 ok + 84 explicados,
`verify_data` 1.591 ok + 634 explicados, `checks.py` ALL OK; **490.025 bytes = 37,3 % do código** (eram 7,8 %).

## Ferramentas

Tudo a partir da raiz do repositório. Python 3 com `pefile` e `capstone`; VS 2022 para compilar. As do C2 valem; o
que muda é o que lêem.

**Análise (uma vez, ou quando o binário mudar)**

| Comando | Faz |
|---|---|
| `bash tools/ghidra/run_full.sh [rodadas]` | Ghidra: importa, analisa, faz o censo com rodadas de sementes (`tools/c2/ghidra_census.sh`) e exporta tudo para `analysis/ghidra/{functions.tsv,asm/,decomp/}`. Projeto em `ghidra/census/project` (`NWindow_C3`). Roda em segundo plano (censo ~10 min; export com decompilação ~1 h) |
| `python tools/extract_classes.py` | as classes dos `IMPLEMENT_CLASS` → `analysis/classes.json`, `classes.md`, `vtable_starts.txt` (309 classes) |
| `python tools/eh_scan.py` | as tabelas de EH → `analysis/eh.json` |
| `python tools/engine_vtables.py --all` | todas as vtables exportadas do Core/Engine → `analysis/engine_vtables.txt` |
| `python tools/build_db.py [--game-src]` | `db/functions.csv` + `docs/PROGRESSO.md` |
| `python tools/gen_prologues.py` | `src/re/Prologues.gen.cpp`: a tabela do instalador |
| `python tools/ue2_check_imports.py` | os imports do Core/Engine do `NWindow.dll` que nenhum cabeçalho de `include/ue2` declara |

**Porte (etapa 3)** — `tools/port/*`: o lado "c1" de cada ferramenta é a **árvore-base** (aqui o projeto do C2,
`NW_C1_ROOT`, padrão em `tools/port/common.py`) e o lado "c2" é a imagem do C3 em `orig/`. Os nomes (`C1_ROOT`, `--c1`,
colunas `c1_va`/`c2_va`) são os do porte C1 → C2 e ficam assim até a geração começar; ver `docs/notas/C2_Porte.md`.

**Build, instalação e teste (o ciclo)**: igual ao do C2 (`python tools/cycle.py [--game-source] [--hook-selftest]
[--no-install] [--difftest] [--standalone]`), com a pasta de teste
`Lineage II - Chronicle 3 - Rise of Darkness\system_nwtest` (cópia da `system`; nunca se mexe na `system`).

## Armadilhas já conhecidas (do C2)

* O `NWindow.dll` do cliente pode ter remendos que não estão no original de set/2005; o projeto decompila o que roda.
* `Engine.dll` tem mais de 8.192 exports: ler a tabela à mão (`tools/engine_vtables.py`, `tools/gen_import_defs.py`).
* `npkcrypt.dll` é importada por ordinal (8 funções) e carrega `npkpdb.dll`: as duas têm de estar na pasta de teste.
* O `Core.dll` do cliente tem data de 2025 (remendado por quem empacotou); o do `system_original` é de abr/2005.

## Onda 2 (11/10/2026)

Seis agentes (console2/3, windows2/3, pawn2, status2/3), todos fundidos: **2.840 ganchos, 57,3 % do código**
(2.595 funções restantes), `checks.py` em ALL OK, instalado em `system_nwtest` (tag `wave2-merged`).

- Defeitos de jogo achados no meio: `fread` do CRT da nossa DLL em `FILE*` do CRT do original (`StockFread`,
  `check_crt.py`); moldura do alvo com `NCFrameCtrl(1,0,0)` em vez de `(0,0,0)` (X extra); diálogo Restart/Exit com
  87 em vez de 108 de altura.
- Ferramentas novas: `tools/port/verify_args.py` (constantes dos argumentos, por registrador e x87, com a pilha pelo
  fluxo de controle), `tools/port/check_crt.py`, registro de chamadas `[Debug] LogCalls=` / `nw::TraceCall`
  (`tools/hook_ini.py log ...`), `check_breakers` conhece os reportadores sem retorno do CRT.
- Regra de mesclagem: acrescentar os arquivos dos agentes a `db/hand_owned.txt` ANTES de qualquer `gen.py`.
- Resta: família GM (NCGMWnd...), NCDeliverWnd, ~842 funções anônimas (167 KB), construtores que dependem de
  `NControl::NControl`, classes sem base (Msn*, broadcast/observer, NPG* ficam originais), vários NewVirtualN.

## Onda 3 (15/10/2026)

Sete pistas em paralelo, sem arquivos em comum (`tools/port/lanes_wave3.py`, `db/lanes_wave3.json`, `db/anon_lane.csv`;
dono de cada função anônima por `tools/port/anon_owner.py`): nconsole, console, gm, trees, recipe, shophtml, sys,
mais duas pistas de resíduos (shophtml2, left). Todas fundidas: **3.940 ganchos, 75,3 % do código** (1.232 funções
restantes, quase todas das famílias que ficam originais por regra), `checks.py` em ALL OK, instalado em `system_nwtest`
(tag `wave3-merged`).

- Dentro: NConsoleWnd (pacotes, mensagens de partida, Seven Signs, buffs, auto-uso), interface principal
  (NCGaraDialogBox, Restart/Exit, zona, GDevice Init/Destroy), GM e petições, árvores (quest, Seven Signs, cerco,
  mapas), receitas / macros / calculadora, loja VIP / frete / família HTML / barra de HP-MP-CP, login / servidor /
  opções / créditos / EULA / IME, NControl::NControl, NL2Font::Load, ~80 destrutores.
- Fora de propósito (ficam originais): messenger (Msn*, Switchboard*, ChatWnd, L2*, Friend*...), GameGuard NPG*,
  NCBroadcast* / NCReplay* / observer, o ctor/dtor do NConsoleWnd. O que resta "sem classe" são ~300 funções anônimas
  dessas mesmas famílias.
- Defeitos achados e corrigidos no caminho: NDialogInfo (`bProgress` em +0x1C, não +0x18: a barra de contagem nunca
  rodava); NCItemWnd::OnDrawToolTip passava a largura como inteiro convertido; NCDeliverWnd::WindowProc chamava
  OnUp/OnDown int3; IsQuickUseItem sem os ids 0x1793/0x1794; `NW_HOOK_CTOR` zerava o objeto antes do ctor (agora
  inicializa por padrão); NCEditBox slot 35 e NCManorShopWnd slots 70-72 (vtable nossa apontava para outra função).
- `verify_hooks.py`: a diferença de vtable agora FALHA para classe com ctor gancheado (o objeto carrega a nossa vtable);
  só nomes diferentes e retornos indecidíveis continuam informativos.
- Nenhuma função da onda foi vista rodando no jogo; só os juízes (verify_hooks, verify_data, verify_args, regcheck,
  check_breakers/retval/stubs/frames, size_audit, check_crt) as aprovam.

## Onda 4 (17/10/2026)

Sete pistas em paralelo sobre as famílias só do C3, que não têm fonte C2 (cada classe ganhou `.h` / `.cpp` novos a partir
do binário e do RTTI; `tools/port/lanes_wave4.py`, `db/lanes_wave4.json`, `db/anon_lane4.csv`): bgm, bobs, dlg, chat,
msn1, msn2, core. Todas fundidas: **4.858 ganchos**, 4.857 funções feitas de 6.546 (91,2 % dos bytes), `checks.py` em ALL
OK, instalado em `system_nwtest` (tags `wave4-partial1..3`, `wave4-final`).

- Dentro: transmissão GM (`NCBroadcastGM*`), observador / replay / benchmark (`NCBroadcastObserver*`, `NCReplay*`,
  `NCObserverWnd`, `NCBenchMarkResultWnd`), os nove diálogos do messenger, as janelas de conversa e tarefas (`ChatWnd`,
  `L2Chat*`, `Task*`, `L2WordWrapperWnd`), o protocolo (`Msn`, `MSNSocket`, `Switchboard`, `DispatchProcess`,
  `FriendList`), as janelas (`MsnWnd`, `MessengerWnd`, `LoginWnd`, `OptionWnd`, `StatusWnd`, `AlarmWnd`, `LoadingWnd`,
  `VersionWnd`, `L2Friend*`, árvores) e o resto do `NConsoleWnd` (modo replay, amigos).
- Fora de propósito: GameGuard `NPG*` (biblioteca do fornecedor), destrutores com deleção escalar, blocos de desfazimento
  de exceção, o ctor / dtor do `NConsoleWnd` (10077ab0 / 10077e80, dependem das vtable slots 374-404), ~220 funções
  (57 KB) de resíduo: `NPG*` 82, o resto são funções minúsculas, pedaços de outras e as listadas por cada pista.
- Ferramentas: `check_breakers.py` entende os reportadores sem retorno do CRT e as chamadas a funções nossas sem retorno.
- Conflitos entre pistas que se coordenaram por endereço e não por tipo: `NMsnMessage` definido duas vezes (ficou a do
  `chat`, em `NChatMsg.h`), `NChatUser` == `NMsnFriend` e o `Rehash` do mapa de textos com gancho nos dois lados (ficou o
  do `msn1`). O log do smoke diz `ERROR duplicate hook <va>: A and B`.
- Defeito achado na fusão: `NotificationProcess::OnMessageImpl` consultava os cabeçalhos com a mensagem como `this`,
  quando o mapa está em +0x28 da mensagem.
- Nada das ondas 2-4 foi visto rodando no jogo; o último log do usuário é de 11/10 05:12. Os juízes não conferem a ordem
  dos argumentos nem a polaridade de desvios: o `msn2` achou um erro de ordem de argumentos só na revisão.

## Auditoria de pacotes (17/10/2026, noite)

Depois do teste da onda 4 (entra no mundo, sem exceção do hook), sentar/levantar, X/ESC do alvo e loja privada falhavam. Eram
defeitos do servidor de teste (um core Interlude adaptado ao C3), não do NWindow. Uma rodada de agentes leu o `Engine.dll` do C3
(interpretador de formato de pacotes, tabela de despacho, escritores c2s) e comparou com o servidor. **O lado do servidor dessa
auditoria (código Java, notas por pista, ferramentas de ids e de dados do cliente) não faz parte deste repositório.** Ficaram os
fatos do cliente: `docs/notas/C3_Pacotes_T1.md` (s2c: opcode -> formato, `tools/c3/s2c_formats.py`), `C3_Pacotes_T2.md` (c2s,
`tools/c3/c2s_formats.py`) e `C3_Pacotes_N.md` (a fronteira NWindow -> Engine, `tools/c3/net_boundary.py`).

* Lado NWindow (C++): 5 pontos de chamada enviavam o pacote vizinho (agent/net); 6 funções do messenger/MD5/socket (agent/left2).
  Build: 4.864 ganchos, `CHECKS: ALL OK` (tag `packet-audit-2`).

## Onda 5 (18/10/2026)

Resíduo da onda 4 (122 funções, ~26 KB, quase todo do messenger) em três pistas sem arquivos em comum (`tools/port/lanes_wave5.py`,
`db/lanes_wave5.json`, `db/anon_lane5.csv`, guia seção 6e): `msn5` (protocolo: `SwitchboardSession`, `MSNSocket`, `DispatchProcess`, 47 anônimas),
`ui5` (`ChatWnd`, `L2ChatWnd`, `MsnWnd` e auxiliares), `core5` (`NotificationProcess`, login do `NMsnPassport`, árvore de quests, `NHtmlParser`,
`NStdString::Copy`, funções dobradas). Todas fundidas: **4.963 ganchos, 92,98 % do código**, `checks.py` em ALL OK (tag `wave5-final`).

- Defeitos achados nas revisões e corrigidos: o `OnReply` original recebe o TIPO da requisição primeiro e a linha depois (o código da onda 4 usava a
  ordem contrária: os juízes não enxergam ordem de argumentos); `SwitchboardSession::SendMSG` não gravava `Message.Kind = 3` como o original.
- Ficam originais: `NPG*` (GameGuard, ~27 KB), `Md5::Transform` e seus dois auxiliares (argumentos em registrador: o `regcheck` recusa gancho),
  `appStrihash`, o ctor/dtor do `NConsoleWnd`, `appMemcpy` (rampa de `movsb`), `HasNoFinalConsonant`, e as ~100 funções (33 KB) do que sobra.
- Pedaços de funções já portadas (laços, caudas) foram registrados em `db/not_functions.csv`.
- Nada foi visto rodando no jogo: o messenger não tem servidor, então a prova é só estrutural (juízes + leitura do disassembly).

**Decisão do usuário (18/10/2026): GameGuard (`NPG*`) fora de escopo.** Não será portado nem listado como pendência
(biblioteca de terceiros): fica o código original. A fonte do C3 está completa, exceto `NPG*` e o resíduo (aliases, `Md5::Transform`, `appStrihash`).

**Teste do usuário (18/10/2026, build da onda 5 em `system_nwtest`, 4.963 ganchos): "tudo funcionando".** Primeiro teste em jogo confirmado das ondas 2-5
juntas (as funções do messenger seguem sem teste: não há servidor de messenger).

**Resíduo fechado (18/10/2026, tag `wave5-resid`):** `Md5::Transform`/`CopyWords`/`DecodeWords` e `appStrihash` viraram `morto` (todos os chamadores são
nossos; o MD5 em C++ foi conferido contra o CryptoAPI em 6.000 casos), `HasNoFinalConsonant` também `morto`; `EmptyTwoArgs`, `NCSliderBar::SetScrollRange/SetScrollPos`,
`NPointDeque::Tidy` e `appMemcpy` ganharam gancho; o ctor/dtor do `NConsoleWnd` (10077ab0/10077e80) ganhou gancho como métodos comuns que gravam as vtables
originais à mão (teste diferencial com o unicorn, `tools/port/emu/t_console.py`). **4.970 ganchos, 93,03 %**, `todo` = 87 (84 do `NPG*` + 3 anônimas do GameGuard).
Build instalado em `system_nwtest` a partir de 18/10 15:18; o DLL confirmado em jogo (onda 5) ficou em `NWindowDecomp.dll.wave5-confirmed` na mesma pasta.
O item de maior risco é o ctor/dtor do `NConsoleWnd` (testar abrir/fechar o jogo): `git revert fbfac67` o desfaz sozinho.
**Confirmado em jogo pelo dono em 18/10/2026** ("testei o console do C3, tudo ok"): o build de 4.970 ganchos, com o ctor/dtor do `NConsoleWnd`, está bom. DLL guardado como `NWindowDecomp.dll.resid-confirmed`; tag `c3-final` = `8a383f4`.
