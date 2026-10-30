# NWindow do C4 — infraestrutura e o que falta

Este repositório é o fork do projeto do C3 (`<C3 repo>`, só leitura; tag `c3-base` = commit
`db0325f`, a source do C3 com 4.963 ganchos / 92,98 % do código, **confirmada em jogo pelo dono em 18/10/2026**) apontado para o
`NWindow.dll` do cliente do C4 (`orig/NWindow.dll`, md5 `afab3ae3fa4f85f7f4c323715e5155e3`, TimeDateStamp `0x43C73652` = 13/01/2006).
O estudo que levou a isto é o [C4_NWindow_Viabilidade.md](notas/C4_NWindow_Viabilidade.md); o método é o do [PLANO.md](PLANO.md); o que o
porte do C3 ensinou está em [PLANO_C3.md](PLANO_C3.md) e [notas/C3_Agentes.md](notas/C3_Agentes.md). Este documento diz o que **já está
retargetado**, as decisões, e o que falta.

> **Estado final (26/10/2026, tag `c4-final`):** modo gancho com 5.456 ganchos (93,19 % do código; `CHECKS: ALL OK`, `verify_data` 0
> diferenças); o standalone linka, passa no `verify_standalone` e no `smoke_standalone` (9 etapas, saída do original), na suíte do
> emulador (127 entradas, 35.204 casos, 0 diferenças, [notas/C4_EmuSA.md](notas/C4_EmuSA.md)) e, sozinho no cliente, loga, lista os
> servidores e entra no mundo (o login pedia a resposta à consulta do GameGuard: `NConsoleWnd::GameGuardQueryX`). As etapas abaixo
> ficam como registro de como se chegou lá.

## Decisões (18/10/2026)

| Tema | Decisão |
|---|---|
| Binário alvo | o `NWindow.dll` de `<C4 client>\system` (idêntico ao de `<C4 folder>\system`), o mesmo do estudo de viabilidade. Pedido do dono em 18/10/2026 ("pode começar o C4"), pasta de trabalho `<C4 repo>` |
| Base da source | **a nossa source do C3, já portada e confirmada em jogo** (não a do C2: 647 KB pedem mão partindo do C2 contra 171 KB partindo do C3) |
| Rota | C2 → C3 → C4 (feita até o C3): o passo C3 → C4 é o menor (12 % do código de jogo pede mão) |
| Escopo inicial | modo gancho primeiro, como no C2 e no C3. Messenger e replay continuam no C4 (as 93 classes do C3 estão lá). **GameGuard (`NPG*`) fora de escopo** por decisão do dono (fica o código original) |
| Pasta de teste | `C4_Client\system_nwtest` (cópia da `system`); a `system` nunca é tocada |
| Servidor de teste | `<C4 server>` (o Lucera adaptado ao C4, `PORTE-C4.md`); no C4 o formato dos pacotes é o do Interlude com ajustes (cuidado: o `writeD(nameColor)` do C3 em `UserInfo`/`CharInfo` tem de sair, o C4 lê o crest grande ali) |

## A imagem do C4 (o que o C3 tinha fixo no código)

| O quê | C2 | C3 | C4 | Onde |
|---|---|---|---|---|
| md5 / TimeDateStamp | `0111f657…` / `0x4227239B` | `f0362880…` / `0x4329071F` | `afab3ae3…` / `0x43C73652` | `tools/nwd/image.py`, `src/re/NwMain.cpp`, `tools/smoke/nwd_smoke_standalone.cpp` |
| Tamanho / SizeOfCode / entry point | 1.835.028 / `0x106000` / `0xE90B1` | 2.732.052 / `0x174000` / `0x14E13A` | 3.112.980 / `0x19C000` / `0x172B4A` | `src/re/NwMain.cpp` |
| `.text` | `0x10001000`–`0x10106020` | `0x10001000`–`0x101748E0` | `0x10001000`–`0x1019C810` (0x19B810) | |
| stubs `jmp [IAT]` | 44 + 30 | `0x10144A60–0x10144B67` (44), `0x1014C0C0–0x1014C18B` (34) | `0x10169420–0x10169527` (44), `0x10170A90–0x10170B49` (31) | `tools/build_db.py` (`THUNK_BLOCKS`) |
| objeto do nProtect (GameGuard) | `0x100DFEE0–0x100E717F` | `0x10144B70–0x1014C0BF` | `0x10169530–0x10170A8F` (provisório: confirmar com o censo) | `tools/verify_standalone.py` (`NPROTECT_LIB`) |
| primeiro objeto da libcmt (`CRT_LO`) | `0x100E723A` | `0x1014C18C` | `0x10170B4A` (mesma assinatura de 28 bytes) | `tools/build_db.py` |
| `_purecall` | `0x100E84EA` | `0x1014D4CA` (14 refs) | `0x10171E13` (18 refs de vtable) | `db/names.csv`, `tools/verify_hooks.py`, `verify_standalone.py` |
| `appMemcpy` (SSE, fora de linha) | `0x10001790` | `0x10001790` | `0x10001CB0` (o sled de `movsb` em `0x10001D00` é fragmento) | `db/names.csv`, `db/not_functions.csv` |
| `wcscpy` | `0x100E78E8` | `0x1014C9BB` | `0x1017137D` | `db/names.csv` |
| funclets `Unwind@` / `__ehhandler`, inicializadores estáticos | | `0x1015C240–0x10168595` / `0x101685B0–0x10173480` | (pelo censo) | derivado |
| classes / exports / imports | 225 / 227 / 452 | 309 / 311 / 530 | 341 / 343 / 548 (estudo; conferir) | |
| compilador | VC 7.0 `13.00.9466` | o mesmo | o mesmo (Rich header) | |

As posições dos stubs, do `CRT_LO`, do `_purecall`, do `wcscpy` e do `appMemcpy` foram medidas no binário (corridas de `jmp [IAT]`; assinatura de
bytes com os endereços mascarados, validada contra o C3; contagem de referências em `.rdata`).

**Atenção:** `Engine.dll` e `Core.dll` do cliente são de 27/05/2006, o `NWindow.dll` é de 13/01/2006 (ver `orig/README.md`). Se o Engine mudou
layouts ou vtables depois do `NWindow.dll`, o cliente "atualizado" já roda assim e é esse conjunto que o projeto precisa honrar.

## Etapas

| # | Etapa | Estado |
|---|---|---|
| 0 | fork + git + binários de referência (`orig/`), dados do C3 postos de lado (`db/c3`, `analysis/c3`) | feito (18/10/2026) |
| 1 | **infraestrutura**: constantes da imagem, análise (Ghidra, classes, EH), banco de funções, prólogos, build do hook sem source de jogo, carga fora do jogo, verificação | feito (18/10/2026), ver Estado |
| 2 | **fundação do Engine** (`include/ue2`) para o Engine/Core do C4: declarações dos imports novos, vtables (425 slots no `UL2ConsoleWnd`; 448 no `NConsoleWnd`), layouts | feito (18/10/2026): `ue2_check` ALL OK (1.620 slots, 644 referências ligadas); ver [notas/C4_Fundacao_Engine.md](notas/C4_Fundacao_Engine.md) |
| 3 | **pipeline de porte C3 → C4** (`tools/port`: `match`, `maps`, `layout`, `vslots`, `eligible`, `gen`) sobre a source do C3 | feito (18/10/2026, agente `gen4`): `judge_loop.sh` termina em ALL JUDGES PASS e `checks.py` em CHECKS: ALL OK com **2.846 ganchos (23,18 % do código)**; ver "Etapa 3" abaixo |
| 4 | **ondas de porte à mão** (o que o pipeline não copia: categoria D, 36 classes novas, o `NConsoleWnd` com 448 slots) com o ciclo como juiz | em andamento (18/10/2026, ok do dono: "continua a source"): 16 lanes em `db/lanes_s4.json` (`tools/port/lanes_s4.py`; 2.639 funções, 1,02 MB; guia `notas/C4_Agentes.md`; base = tag `s4-base`). **Onda A concluída e fundida (19/10/2026, tag `s4-wave-a-merged`): 8 lanes (nconsole, input1, input2, base, pawn, status1, status2, items1), 4.224 ganchos = 62,53 % do código** (levantamento em `notas/C4_Levantamento.md`). Onda B (a lançar, depende do aval do dono): items2, items3, msn1, msn2, bgm, sys1, sys2, rest1 (431 KB de todo) |
| 5 | **standalone** (`src/standalone`) e conferência de remendos | em andamento (24/10/2026, pedido do dono: "abandonar completamente o NWindow original, assim como foi feito nos outros"): onda de 11 lanes `sa-*` em `db/lanes_s5.json`, guia `notas/C4_Standalone.md`, lista de trabalho `python tools/port/sa_sites.py` (≈ 1.720 pontos em 229 arquivos que ainda tocam a imagem original), base = tag `s5-prep`. GameGuard continua fora (no-op, `NGameGuard.h`) |

## Estado (18/10/2026)

**Etapa 1 fechada.** A cadeia do C3 (`python tools/cycle.py`) roda no C4 **com a source de jogo vazia**: a DLL `NWindowDecomp` carrega no `NWindow.dll`
do C4 (`TimeDateStamp 43c73652`, `SizeOfCode 0019c000`, entry `00172b4a` conferidos), `hooks: 0 installed, 0 registered, 0 errors`, `verify_hooks`
(0 ok, 0 explained, 0 unexplained) e `regcheck` (0 problem(s)) limpos, instalado em `C4_Client\system_nwtest`.

Banco (`db/functions.csv`): **12.470 funções** (o censo do Ghidra convergiu em 3 rodadas: 12.316 → 12.411 → 12.470), classificação (`kind`): 6.165 `game`,
1.037 `init`, 4.669 `eh`, 517 `crt`, 81 `thunk`, 1 `fragment`; trabalho (game + init) = **7.202 funções, 1.448.754 bytes** (C3: 6.546 / 1.312.496).
Prólogos: 7.202, 0 pequenos demais para `jmp`, 8 sem trampolim. O objeto do nProtect (`0x10169530–0x10170A8F`) tem 86 funções / 28.322 bytes (C3: 84 / 29.318).
341 classes (todas com ctor e vtable). `analysis/eh.json`: 1.814 stubs de EH, 1.620 pais, 977 funções com `guard()`.

### O que os mapas mediram (base = a source do C3)

| Passo | Resultado |
|---|---|
| `match.py` → `db/fmap.csv` | 5.395 das 5.595 funções de jogo do C3 têm par no C4: **96,6 % dos bytes** (1.223.838 de 1.267.128). A 1.530 funções / 170 KB (igual), B 2.278 / 318 KB (igual sem os deslocamentos), C 679 / 164 KB (mesma forma, constantes diferentes), D 908 / 572 KB (forma diferente). 1.793 funções do C4 (164 KB) sem par no C3 |
| `maps.py` → `db/amap.csv`, `analysis/port/` | 31.206 pares de endereço (114 ambíguos), votos de campo para 514 classes, 4.266 imediatos que mudaram |
| `layout.py` → `db/layout.json` | 305 classes em comum: 2.118 campos mapeados, 275 derivados, 12 sem mapa; 5 campos cujo trecho do C3 invade o seguinte no C4 (olhar à mão) |
| `vslots.py` → `db/vslots.json` | `NCObject` 25 → 25, `NCCmdTarget` 39 → 39, `NCWnd` 68 → 74, `NControl` 69 → 75, `NConsoleWnd` 405 → 448 slots; 2.245 slots sem par no C3 e 353 slots do C3 que sumiram |
| `eligible.py` → `db/eligible.csv` | 4.487 pares, **4.093 elegíveis (567.932 de 651.769 bytes)**: corpos do C3 que compilam contra os cabeçalhos do C4 e dão a mesma função; 1.182 com deslocamentos de outros objetos por conferir na compilação |

A etapa 3 (gerar a source) depende da etapa 2 (o `include/ue2` do C4, agente `found`): o gerador compila o que gera contra esses cabeçalhos.

## Etapa 3 — a source do C3 portada para o C4 (branch `agent/gen4`)

**Resultado** (`bash tools/port/judge_loop.sh`, de um `db/demote.csv` só com o cabeçalho, 14 rodadas): `hooks: 2846 installed, 2846 registered, 0 errors`,
`2375 ok, 471 explained, 0 unexplained` (verify_hooks), regcheck 0 problemas, verify_data `0 differ`, check_breakers rc 0, `verify_args` 0 diferenças e
`python tools/port/checks.py` termina em `CHECKS: ALL OK` (inclui o novo `vt_audit`). `docs/PROGRESSO.md`: **2.846 funções / 335.792 bytes = 23,18 % do código**
(7.202 funções, 1.448.754 bytes). O gerador copia 4.305 pares elegíveis (611.255 de 651.769 bytes da base) e o ciclo derruba o que não é a mesma função.

**O que o C3 assumia e o C4 não mantém** (consertado no gerador / nas tabelas, não nos arquivos gerados):

| Suposição do C3 | No C4 | Onde foi consertado |
|---|---|---|
| `NCWnd` = 25+ slots, `OnPaint`/`OnDrawToolTip`/`OnChar`/`OnMouseWheel` nos slots 56.. | 74 slots; os quatro são 62/63/64/73; novos 39/40/42/44/50/56; `SetPosition` virou virtual com 3 argumentos a mais (0, 0, 3) | `db/vslots_manual.csv`, `NCWnd.h/.cpp` à mão, `db/candidate_calls.csv` (chamadas que mudaram de forma) |
| os slots herdados de um pai mantêm o número do pai | o alinhamento monótono casava dois overrides pelo corpo compartilhado (`HideWindow` 45 → 50) | `vslots.py`: slot herdado = mapa do pai; virtuais próprios sem par entram em ordem (`NCTabControl` 69/70 → 75/76) |
| classe abstrata declara os slots puros (`NCAbstractMain`) | todos os 12 slots são `_purecall` na imagem: nada para parear | `hdr.py` lê `virtual ... = 0; // N`, linhas manuais `NCAbstractMain` (25..31, `NumFocusWindows` 32 → 34) |
| a vtable que o compilador monta é a do cabeçalho | um virtual com nome/assinatura diferente do declarado no Engine vira slot NOVO no fim (109 slots a mais no `NConsoleWnd`) | slots sem nome do `UL2ConsoleWnd` agora são `NewVirtual<N>() = 0` (`tools/gen_engine_decls.py`), `gen.py` declara a assinatura do Engine (`engine_slot`/`engine_override`), linhas da tabela `ue2_data/l2consolewnd_slots_c4.csv` corrigidas (230 `AddVIPNeedItemList` com 4 parâmetros, 365 `ShowMessageWnd`); `tools/port/vt_audit.py` confere as 307 classes com `/d1reportSingleClassLayout` |
| tamanhos de struct fixos | `UObject` 0x34, `NItemInfo`, `NItemEntry`, `NToolTipInfo` (+4 em 0x18), ... | `db/struct_sizes.csv` (coluna `insert`: 1 = bytes inseridos no struct, 2 = só a elegibilidade), `hdr.py` (arrays multidimensionais, `#if 0`) |
| `FindTexture(Name)` | segundo argumento (1) | `db/decl_manual.csv`, `db/candidate_calls.csv` |
| construtores/destrutores sempre portados | ctor sem par vira `StockCallAt` (encaminha); dtor não pode ser `naked` (C3068) | `gen.py` encaminha os ctors sem par por padrão (`--strict-ctors` desliga), chaves `naked:` e `helper:` do `demote.csv` |

**Juízes** (todos regeneráveis): `demote.py` reescrito (chaves por arquivo, `#define` com continuação, C3068), `demote_verify.py` (VT BAD → demove o ctor), `demote_data.py`
(candidatos estritos; rótulo de `guard()` copiado de um irmão também é explicado), `demote_args.py`, `carry_notes.py` (notas do C3 só valem se a diferença é a mesma),
`verify_data.py` (constante float lida do `.rdata` = imediato do mesmo valor; `add r,-N` = `sub r,N`), `check_breakers.py --demote`, `vt_audit.py`.

**Arquivos à mão** (`db/hand_owned.txt`): `include/nwindow/NCWnd.h`, `src/nwindow/NCWnd.cpp` (campos do C4, `SetPosition` virtual de 7 argumentos com o pin na tela, ctor, `OnCreate`, `OnDestroy`;
notas manuais em `verify_notes.csv`/`verify_data_notes.csv`).

**O que sobra sem gancho** (837.096 bytes da base em 2.084 unidades; `db/demote.csv` 1.265 linhas): categoria D / não elegível 345 KB (651 un.: código de forma
diferente, é a etapa 4), candidatos que não batem 132 KB (208), `verify_data` ~230 KB (778: campos de classes cujo layout é só derivado — `NCButton` e a família `NControl`,
`NConsoleWnd` —, constantes de layout de janela, `SetText(*SysString(n))` → `SetTextID(n)`), `verify_hooks` ~75 KB (127: chamadas virtuais a slots que o C4 renumerou, `FString::operator*`
que o C4 não chama), `check_breakers` 47 un. (destrutores sem gancho de `MsnLine`/`NMsnMessage`/`NCTVItem`), C3068 66 stubs por valor.

**Para repetir**: `printf 'c2_va,reason\n' > db/demote.csv; rm -f db/verify_data.csv db/candidates.csv; bash tools/port/judge_loop.sh 45 > build/loop.out` (em segundo plano; ~40 min), depois
`python tools/port/checks.py`. Depois de mexer em `vslots_manual.csv`/`struct_sizes.csv`: `python tools/port/vslots.py; python tools/port/eligible.py`. Depois de mexer na tabela do console:
`python tools/gen_engine_decls.py`.
