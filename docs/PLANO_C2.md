# NWindow do C2 — infraestrutura e o que falta

Este repositório é o fork do projeto do C1 (`NWindow_Decomp`, só leitura) apontado para o `NWindow.dll`
do cliente do C2 (`orig/NWindow.dll`, md5 `0111f6578f6482f73980f6268221c03e`, TimeDateStamp `0x4227239B`
= 03/03/2005). O estudo que levou a isto é o [C2_NWindow_Viabilidade.md](notas/C2_NWindow_Viabilidade.md);
o método é o do [PLANO.md](PLANO.md). Este documento diz o que **já está retargetado** (a infraestrutura),
como rodar cada ferramenta e o que falta para a fase 5 (standalone).

## Estado (06/10/2026)

A cadeia do C1 (`python tools/cycle.py`) roda no C2 **com a source de jogo vazia**: o alvo
`NWindowDecomp` compila só o `src/re`, o instalador injeta o import `NWindowDecomp.dll` numa cópia do
`NWindow.dll` do C2, a cópia carrega fora do jogo (`tools/smoke`: o `DllMain` da nossa DLL roda, confere a
imagem e registra `hooks: 0 installed, 0 registered, 0 errors`), e o `verify_hooks.py` e o `regcheck.py`
passam com zero hooks. Uma prova com **um** hook de verdade (`--hook-selftest`: `NCWnd::IsEnabled`,
`0x10001B70`) também passa: instalado (`ok`) e igual ao original no `verify_hooks.py`.

Além disso, na imagem do C2 (sem nenhuma source nossa):
- `tools/verify_standalone.py --stock` passa tudo: PE, 7 cursores, 227 exports com os ordinais, 225 classes
  (os 225 `IMPLEMENT_CLASS`, na ordem dos inicializadores) e as 227 vtables slot a slot;
- `tools/smoke_standalone.py --control-only` carrega o `NWindow.dll` **original** pelo caminho do engine
  (Core do C2, `StaticLoadClass` de `NWindow.ConsoleWnd`/`NWindow.L2Font`, construção dos dois objetos):
  `ALL STAGES OK`, as 225 classes registradas batem com `analysis/classes.json`. Os stand-ins de
  `tools/smoke/nwd_smoke_standalone.cpp` (vtables de `FMalloc`, `FOutputDevice`, `FConfigCache`; offsets do
  `UObject`/`UClass`), escritos para o C1, servem no Core do C2.

### Constantes da imagem do C2 (as que o C1 tinha fixas no código)

| O quê | C1 | C2 | Onde |
|---|---|---|---|
| md5 / TimeDateStamp | `2dacb6eb…` / `0x40DBD452` | `0111f657…` / `0x4227239B` | `tools/nwd/image.py`, `src/re/NwMain.cpp`, `tools/smoke/nwd_smoke_standalone.cpp` |
| SizeOfCode / entry point | `0xD6000` / `0xBF1A3` | `0x106000` / `0xE90B1` | `src/re/NwMain.cpp` |
| `.text` | 0xD59D0 bytes | 0x105020 bytes (0x10001000–0x10106020) | |
| stubs `jmp [IAT]` | um bloco, `0x100BD360–0x100BD4D3` (62) | dois blocos: `0x100DFDD0–0x100DFED7` (Core/Engine, 44) e `0x100E7180–0x100E7239` (IMM32/VERSION/WS2_32, 30); mais 5 soltos (2 do Engine no meio do código do jogo, 2 da `npkcrypt.dll`, `RtlUnwind`) | `tools/build_db.py` (`THUNK_BLOCKS`; o resto vem dos bytes `FF 25`) |
| primeiro objeto da libcmt (`CRT_LO`) | `0x100BD4D4` | `0x100E723A` (as mesmas cinco primeiras funções: 28, 11, 63, 24 e 63 bytes) | `tools/build_db.py` |
| funclets `Unwind@` / stubs `__ehhandler` | depois do CRT | `0x100F7130–0x100FD265` (956 funclets; os `Catch@`, 893, ficam dentro do código do jogo) | derivado, `build_db.py` |
| inicializadores estáticos | `0x100d…` | `0x100FD280–0x10106010` (225 classes × 3 + globais) | derivado |
| 83 funções entre os dois blocos de stubs | não havia | `0x100DFEE0–0x100E717F`, 28.658 bytes, sem par no C1 (de um objeto linkado no meio; ficam como `game`) | |
| `_purecall` | `0x100BE4FC` | `0x100E84EA` (`push 19h; call __amsg_exit`; 8 slots de vtable) | `db/names.csv`, `tools/verify_hooks.py`, `tools/verify_standalone.py` |
| `appMemcpy` (SSE, fora de linha) | `0x1000E320` | `0x10001790` (a "cauda" `movsb` em `0x100017E0` é fragmento: `db/not_functions.csv`) | `db/names.csv` |
| `wcscpy` | `0x100BD5BD` | `0x100E78E8` | `db/names.csv` |
| `FMalloc::Free` dobrado com o `operator delete` (`NConsoleWnd::Free`) | `0x10046F40`, slot 258 | `0x10026550`, slot 302 | `db/names.csv` |
| slots do `UObject` | 24 | 25 (`ClearL2Game` no 22) | `tools/build_db.py`, `tools/vtable.py` |
| classes / exports / imports | 178 / 180 / 335 | 225 / 227 / 452 | |
| compilador | VC 7.0 `13.00.9466` | o mesmo (Rich header: build 9466; linker 7.0) | |

### O banco (`db/functions.csv`): 7.117 funções, 4.293 com nome

Classificação (`kind`): 3.976 `game`, 685 `init`, 1.861 `eh`, 515 `crt`, 79 `thunk`, 1 `fragment`;
trabalho (game + init) = **4.661 funções, 917.473 bytes**. Origem dos nomes (coluna `name_src`; a ordem é a
prioridade, a primeira que sabe o nome ganha; o `fmap` é o último):

| Origem | Funções | O que é |
|---|---:|---|
| `manual` | 4 | `db/names.csv` (`_purecall`, `wcscpy`, `appMemcpy`, `NConsoleWnd::Free`) |
| `guard` | 375 | a string do `guard()` do UE2 (`tools/eh_scan.py`) |
| `export` | 1 | `DllMain` |
| `class` | 1.083 | o `IMPLEMENT_CLASS`: inicializador, `autoclass`, atexit, `InternalConstructor`, construtor |
| `dtor` | 225 | o destrutor real, chamado pelo `scalar deleting destructor` |
| `vslot` | 688 | slot de vtable com nome conhecido no C2: os 25 do `UObject`, os do Engine lidos da vtable do `UL2ConsoleWnd`/`UL2Font` no `Engine.dll` do C2, os que um `guard()` de uma filha dá ao slot de todas |
| `ghidra` | 292 | thunks (nome do import, 79) e nomes de biblioteca que o Ghidra casou no CRT (213) |
| `fmap:A/B/C/D` | 314 / 684 / 109 / 433 | herdado da função do C1 que o `tools/port/match.py` casou (A = igual, B = igual sem os offsets, C = mesma forma, D = forma diferente: confiança menor) |
| `fmap-slot` | 85 | slot nomeado pelo que os nomes do C1 das suas sobrescritas concordam |
| sem nome | 2.824 | 1.861 funclets de EH (não precisam), 294 do CRT (bibliotecas), 666 `game` (169 delas `Classe::vfnNNN`: slot que ninguém nomeou; 497 `sub_xxxxxxxx`), 2 `init`, 1 fragmento |

(os números mudam quando o `fmap` ou o `names.csv` mudam; `python tools/build_db.py` imprime a tabela.)

## Ferramentas

Tudo a partir da raiz do repositório. Python 3 com `pefile` e `capstone`; VS 2022 para compilar.

**Análise (uma vez, ou quando o binário mudar)**

| Comando | Faz |
|---|---|
| `bash tools/ghidra/run_full.sh [rodadas]` | Ghidra: importa, analisa, faz o censo com rodadas de sementes (`tools/c2/ghidra_census.sh`) e exporta tudo para `analysis/ghidra/{functions.tsv,asm/,decomp/}`; termina com `export done: 7117 functions`. Roda em segundo plano, minutos. Projeto em `ghidra/census/project` (`NWindow_C2`); não rodar duas vezes ao mesmo tempo. |
| `python tools/extract_classes.py` | as classes dos `IMPLEMENT_CLASS` → `analysis/classes.json`, `classes.md`, `vtable_starts.txt` (225 classes). Dois acertos em relação ao `tools/c2/extract_classes.py` do estudo: `L2WordWrapperWnd` e `MSNSocket` com o nome certo (o literal `TEXT("…")+1`; antes `N2WordWrapperWnd`/`NSNSocket`), e o `NCEditBox`, cujo `InternalConstructor` empilha 5 argumentos antes do `call`, agora tem construtor, vtable (78 slots) e slots. |
| `python tools/eh_scan.py` | as tabelas de EH → `analysis/eh.json`: o pai de cada funclet e o nome do `guard()`. 838 stubs, 733 pais, 375 com `guard`; 105 stubs são compartilhados por várias funções (sem pai único: fica sem nome de `guard`). |
| `python tools/build_db.py [--game-src]` | `db/functions.csv` + `docs/PROGRESSO.md`; imprime as origens dos nomes. `--game-src` lê os `NW_HOOK_*`/`NW_CHECK_VTABLE`/globais de `src/nwindow` e `include/` (só quando já forem os do C2). |
| `python tools/c2/remap_vslots.py` | `db/vslots_c1.csv` + `db/vslots.json` → `db/vslots.csv` com os slots do C2 (o do C1 só tinha o cabeçalho: nada a remapear hoje) |
| `python tools/gen_prologues.py` | `src/re/Prologues.gen.cpp`: a tabela do instalador (4.661 entradas: as funções `game`/`init` do banco; 1 pequena demais para `jmp`, 7 sem trampolim) |
| `python tools/disasm.py <va\|Classe::Metodo\|--class C>`, `tools/xrefs.py <va>…`, `tools/vtable.py <Classe>` | consulta |
| `python tools/c2/compare_nwindow.py [--c1 DIR]` | o estudo C1 × C2 (lê o C1 do repositório do C1, só leitura) |

**Build, instalação e teste (o ciclo)**

| Comando | Faz |
|---|---|
| `cmake -S . -B build -A Win32` | configura (`-DNWD_GAME_SOURCES=ON` liga o `src/nwindow`; `-DNWD_HOOK_SELFTEST=ON` liga o hook de prova) |
| `cmake --build build --config Release --target NWindowDecomp` | `build\bin\Release\NWindowDecomp.dll` (a mensagem `'pwsh.exe' não é reconhecido` no fim de cada build vem do ambiente do Visual Studio, não do projeto; o build não falha) |
| `python tools/install_test.py [--system DIR] [--restore]` | `NWindow.dll` do `orig/` + import `NWindowDecomp.dll!NWindowDecomp_Attach` (`tools/nwd/peimport.py`, que copia os 10 descritores de import do C2, `npkcrypt.dll` por ordinal incluído, acrescenta o nosso por último e confere o resultado) em `C2_Client\system_nwtest`. Recusa instalar na `C2_Client\system`. |
| `build\smoke\Release\nwd_smoke.exe <System>` | carrega o `NWindow.dll` da pasta fora do jogo (a de teste, ou `build\smoke_system`), imprime o `NWindowDecomp.log` |
| `python tools/verify_hooks.py [--log F] [--dll F]` | cada hook contra a função original (`ret`, slots de vtable, imports); com zero hooks passa (`0 ok, 0 explained, 0 unexplained`) |
| `python tools/regcheck.py [--game-src]` | registradores que o original assume entre chamadas e que um hook quebra |
| `python tools/cycle.py [--game-source] [--hook-selftest] [--no-install] [--difftest] [--standalone]` | configura, compila, instala na `build\smoke_system` (cópia mínima das DLLs do engine: Core, Engine, IFC23, MSVCR70, ogg, vorbis, vorbisfile, **npkcrypt, npkpdb**), carrega, verifica, instala em `system_nwtest` e regenera o banco. O padrão é a source de jogo vazia. |

A pasta de teste: `xcopy /E /I /Y <C2 client>\system <C2 client>\system_nwtest`
(feita; nunca se mexe na `system`).

**Fase 5 (standalone) — o que roda hoje e o que ainda não**

| Comando | Estado no C2 |
|---|---|
| `python tools/verify_standalone.py --stock` | passa na imagem original. Com uma DLL standalone: `--dll X [--map M]`. O check `source` (e as vtables extras do `NW_CHECK_VTABLE`) só lê a source com `--game-src`. |
| `python tools/smoke_standalone.py --control-only` | passa (o original pelo caminho do engine). Sem `--control-only`: precisa do `NWindowStandalone` e compara a saída com a do original. |
| `python tools/gen_implement_class.py [--check]` | `src/standalone/NWindow.def` (227 exports) já foi gerado; `Classes.gen.cpp` só sai quando os 225 `NW_DECLARE_CLASS` existirem em `include/nwindow` (faltam as 47 classes novas). |
| `python tools/extract_cursors.py DIR` | os 7 cursores (iguais aos do C1: grupos 101, 102, 106, 107, 127, 130, 131, coreano) |
| `python tools/standalone_report.py` | compila o `NWindowStandalone` e lista o que falta no link; hoje o alvo não compila (`include/nwindow` ainda é o do C1) |
| `python tools/gen_globals.py`, `python tools/difftest.py` | **recusam rodar** na imagem do C2 (saída 2, com a explicação; `--force` força): a tabela `SPEC` das globais e os casos do difftest têm endereços do C1 |

## O que falta para a fase 5

1. **A source de jogo do C2** (`tools/port`, outra frente): quando `src/nwindow` e `include/nwindow` forem os
   do C2, ligar `-DNWD_GAME_SOURCES=ON` (ou mudar o default no `CMakeLists.txt`) e passar `--game-src`
   (`cycle.py --game-source` faz isso, para o `build_db.py`, o `regcheck.py` e a compilação). Trocar os
   padrões em `tools/cycle.py`, `tools/build_db.py`, `tools/regcheck.py` e `tools/verify_standalone.py`.
2. **`src/standalone`**: `Classes.gen.cpp` (225 classes) com `gen_implement_class.py`; `NwGlobals.cpp` com as globais
   do C2 (refazer a `SPEC` do `gen_globals.py`: as do C1 têm outros endereços e há globais novas);
   `NwStandalone.h`: o `sizeof(UClass)` (0x520 no C1: o passo entre os `PrivateStaticClass` no `.data`) tem
   de ser conferido no C2 (`analysis/classes.json`: diferença entre `static_class` de classes consecutivas).
3. **Imports que o C1 não tinha**: `npkcrypt.dll` por ordinal (8 funções: 1 `NPKOpenDriver`, 2 `NPKCloseDriver`,
   16 `NPKRegCryptMsg`, 25 `NPKSetDrvPath`, 26 `NPKLoadAtStartup`, 36 `NPKGetAppCompatFlag`,
   37 `NPKSetAppCompatFlag`, 41 `NPKGetLoadStartup`; não há `.lib`: um `.def` com os ordinais ou `LoadLibrary`),
   `WS2_32` (MSN), `ADVAPI32` (registro, CryptoAPI), `WINMM`. As três últimas já estão na linha de link do
   `NWindowStandalone` (CMake).
4. **Difftest**: os casos (`tools/difftest/case_*.cpp`) chamam funções do original por endereço do C1 e o
   `tests/golden/` é a saída do C1. Refazer com os endereços do C2 (o `db/fmap.csv` dá o par de cada função)
   e regenerar o golden com o `NWindow.dll` do C2 (`difftest.py --update-golden`). O alvo `nwd_difftest` compila.
5. **Os stand-ins do `nwd_smoke --standalone`** já foram conferidos contra o Core do C2 (controle passa);
   falta rodá-lo com a nossa DLL.
6. **Funções que o Ghidra pode ter perdido**: 75 alvos de `call` não são início de função na lista (65 caem no
   meio de outra função, 10 em código sem dono); no C1 isso virou `db/extra_functions.csv`. O C2 começa com a
   lista vazia (só o fragmento do `appMemcpy` em `db/not_functions.csv`): olhar quando um hook falhar com
   `HOOK_ERR_UNKNOWN`/`BYTES`.
7. **As 83 funções de `0x100DFEE0`**: sem par no C1; ver de que objeto são (não são CRT).
8. Fora do escopo por ora: a fase 6 (`NW_FEATURES`, `src/features`, `tools/craft`/`chat`) e a camada
   de testes do Engine (as `nwd_smoke_*.inc` do C1 testavam a source do `Engine.dll` do C1) foram retiradas
   do CMake e de `tools/smoke`; `tests/standalone/l2_extras.csv` e o `--l2` do `verify_standalone.py` são
   resto da fase 6 do C1.

## Observações

- O `NWindow.dll` de `orig/` (o que roda no cliente, arquivo de 27/12/2022) difere do `nwindow.dll.BAK` da
  mesma pasta em 24 bytes (8 trechos em `.text`): ver `orig/README.md`. O projeto decompila o que roda.
- O `l2.exe` do cliente é de 02/01/2018 (remendado em 2018); o PE continua com a data de 2005.
- O cliente do C2 não tem `Network.dll`; o código de rede está no `Engine.dll`/`IpDrv.dll`.
- Para o carregamento fora do jogo basta a pasta de teste: `Core.dll` importa `vorbisfile.dll`, o `Engine.dll`
  importa `IFC23.dll`/`MSVCR70.dll`, o `NWindow.dll` importa `npkcrypt.dll`, que importa `npkpdb.dll`.
