# NWindow.dll do Lineage II C4 — source reconstruída em C++

**Português** · [English](README.en.md) · [Русский](README.ru.md)

Reconstrução, função por função, do `NWindow.dll` do cliente Lineage II **Chronicle 4 (Scions of Destiny)**:
a DLL nativa da interface (janelas, controles, HTML das NPCs, IME, chat, console, messenger, replay). O alvo é o
build original de 13/01/2006 (TimeDateStamp `0x43C73652`, Visual C++ .NET 2002 `13.00.9466`, md5
`afab3ae3fa4f85f7f4c323715e5155e3`; 341 classes, 343 exports). A mesma source compila de dois jeitos:

| Alvo CMake | Arquivo gerado | O que é |
|---|---|---|
| `NWindowDecomp` | `build/bin/Release/NWindowDecomp.dll` | **modo hook**: roda ao lado do `NWindow.dll` original e troca cada função já reimplementada por um `jmp` para a nossa |
| `NWindowStandalone` | `build/standalone/Release/NWindow.dll` | **standalone**: o `NWindow.dll` inteiro, só com o nosso código, no lugar do original |

Este é o quarto projeto da série (C1 → C2 → C3 → C4): a source do C3, já confirmada em jogo, foi portada para o C4
por um pipeline próprio (`tools/port`) e completada à mão. As notas do C2 e do C3 continuam em `docs/` porque o
método, as ferramentas e boa parte dos dados vêm delas.

## Como funciona

**Modo hook.** O instalador (`tools/install_test.py`) acrescenta ao `NWindow.dll` original um import da
`NWindowDecomp.dll`. Ela carrega antes de qualquer código do original rodar e põe um `jmp` na entrada de cada
função que já reimplementamos (um `NW_HOOK_*` ao lado de cada definição, `src/re/NwHook.h`). O resto continua
sendo o código original. Qualquer função pode ser desligada no `NWindowDecomp.ini` (`[Hooks] Disable=`) para
isolar um bug.

**Standalone (etapa 5).** Com `NW_HOOK_MODE=0` os macros de hook ficam vazios e a source gera o `NWindow.dll`
sozinha: as 341 `UClass` (`src/standalone/Classes.gen.cpp`), as globais (`NwGlobals.cpp`), os exports com os
ordinais do original (`src/standalone/NWindow.def`) e os cursores coloridos (extraídos de `orig/NWindow.dll` no
build). A engine (`Core.dll`/`Engine.dll`) carrega o nosso DLL como carregava o original.

**GameGuard.** A biblioteca estática do nProtect GameGuard (`NPG*`, código de terceiros) fica **fora** do
standalone: os pontos de entrada são no-ops documentados (`include/nwindow/NGameGuard.h`,
`src/standalone/NwGameGuard.cpp`). Sem ela o login do C4 parava depois do `Init` (o login server espera o
`AuthGameGuard`); o standalone responde à consulta sozinho, com zeros (`NConsoleWnd::GameGuardQueryX`,
`src/nwindow/NConsoleWndC2.cpp`). O NKeyCrypt (`npkcrypt.dll`) é carregado com `LoadLibrary`/`GetProcAddress`.

## Estado

- **Modo hook:** **5.456 de 7.202** funções a reimplementar estão com hook (**93,19 %** dos bytes de código).
  Das que faltam, 587 são de convenção (*scalar deleting destructors*, `InternalConstructor`) e 1.038 só existem no
  standalone (inicialização estática, `DllMain`); 121 (34 KB) ficam sem hook, quase todas a biblioteca do GameGuard,
  fora de escopo. Contagem viva: [docs/PROGRESSO.md](docs/PROGRESSO.md) (gerado por `tools/build_db.py`).
  Juízes verdes: `hooks: 5456 installed, 0 errors`, `verify_hooks` 0 sem explicação, `regcheck` 0 problemas,
  `tools/port/checks.py` `CHECKS: ALL OK`, `verify_data` 0 diferenças. Builds de hook de até 5.387 funções
  entraram no mundo no jogo sem exceção.
- **Standalone:** linka (0 símbolos sem resolução), passa no `verify_standalone.py` (PE, imports, recursos, exports,
  classes e vtables iguais aos do original) e no `smoke_standalone.py` (as 9 etapas, saída idêntica à do original),
  e a suíte diferencial no emulador (`tools/port/emu`, Unicorn) roda contra ele: 127 entradas, 35.204 casos, 0
  diferenças. **No jogo, o `NWindow.dll` standalone sozinho loga, lista os servidores e entra no mundo.**
  As janelas abertas depois disso estão provadas no emulador; o teste janela por janela no cliente ainda é parcial
  (`Verificadas no jogo: 0` no banco).
- As notas técnicas (`docs/`) estão em português (as do C4 que servem de guia para agentes, em inglês); os
  comentários de código, em inglês.

## Requisitos

- Windows, **Visual Studio 2022** com C++ para x86 (MSVC v143), **CMake ≥ 3.20**;
- **Python 3** com `pefile` e `capstone` (`pip install pefile capstone`): usado pelo build standalone (extração
  dos cursores) e por todas as ferramentas; `unicorn` (`pip install unicorn`) para a suíte do emulador;
- **o seu próprio cliente Lineage II C4**. Nenhum arquivo do jogo vem no repositório: copie para `orig/` os
  arquivos listados em [orig/README.md](orig/README.md) (o `NWindow.dll` tem de ter o md5 de lá);
- para os juízes (`verify_hooks.py`, `regcheck.py`, `verify_standalone.py`, `build_db.py`, `tools/port/*`): o
  export do Ghidra em `analysis/ghidra/`, recriado por `tools/ghidra/run_full.sh` (Ghidra 11.3.2 e JDK 21;
  `NWD_GHIDRA_HOME` e `GHIDRA_JAVA_HOME`). Leva um bom tempo; rode em segundo plano.

## Compilar

Num *Developer Command Prompt* x86 do VS 2022 (ou depois de `vcvars32.bat`), na raiz do repositório:

```bat
cmake -S . -B build -A Win32 -DNWD_GAME_SOURCES=ON
cmake --build build --config Release
cmake --build build --config Release --target NWindowStandalone
```

`-DNWD_GAME_SOURCES=ON` liga a source de jogo (`src/nwindow`); sem ele (o padrão, herdado da primeira etapa do
porte) o `NWindowDecomp` sai só com o runtime de hook e o `NWindowStandalone` não linka. O primeiro `--build` gera
o DLL do modo hook e as ferramentas de teste (`nwd_smoke`, `nwd_difftest`) e não precisa de `orig/`; o
`NWindowStandalone` precisa de `orig/NWindow.dll` e escreve também `NWindow.map` (para `tools/port/map_addr.py`).
Avisos C4244/C4838/C4005 são conhecidos (conversões que reproduzem o original).

## Instalar e testar

Use uma **cópia** da pasta `system` do cliente (por exemplo `system_nwtest`), nunca a que você usa para jogar
(o instalador recusa uma pasta chamada `system`). Os comandos abaixo leem a pasta de `--system` ou de
`NWD_CLIENT_SYSTEM`:

```bat
xcopy /E /I /Y "C:\...\Lineage II\system" "C:\...\Lineage II\system_nwtest"
set NWD_CLIENT_SYSTEM=C:\...\Lineage II\system_nwtest
```

**Modo hook:**

```bat
python tools\install_test.py
python tools\install_test.py --restore
```

O primeiro grava um `NWindow.dll` refeito a partir de `orig/NWindow.dll` com o import a mais, a
`NWindowDecomp.dll` e um `NWindowDecomp.ini` (o `NWindow.dll` que estava lá fica como
`NWindow.dll.pre-decomp-bak`); o segundo devolve o que estava lá. O `NWindowDecomp.log` dessa pasta lista cada
hook instalado. No `.ini`: `[Hooks] Enabled=0` deixa todo o código original; `Disable=` aceita um endereço
(`1000cc40`), um método (`NCGDevice::DrawText`) ou uma classe (`NCGDevice::*`).

**Standalone:** feche o jogo, guarde o `NWindow.dll` da pasta de teste e copie por cima dele o
`build\standalone\Release\NWindow.dll`. Só esse arquivo (o `npkcrypt.dll` do cliente continua lá). Se o processo
cair, o standalone escreve `NWindow_crash.log` ao lado do `l2.exe`: `python tools\port\map_addr.py --log
NWindow_crash.log` dá o nome das funções. Para voltar, ponha o original de volta.

**Ciclo** (compila, smoke, juízes, instala, atualiza o banco; para no primeiro passo que falhar):

```bat
python tools\cycle.py --game-source --no-install
python tools\cycle.py --game-source --no-install --standalone
tools\port\cycle.bat
```

O segundo acrescenta os passos do standalone (declarações, build, verify, smoke). O `tools\port\cycle.bat` chama
o `vcvars32.bat` sozinho e roda o ciclo do modo hook seguido dos juízes do porte (`verify_data.py`,
`check_breakers.py`, `checks.py`). Sem `--no-install` o ciclo também instala na pasta de teste (o jogo tem de
estar fechado). O ciclo copia dessa pasta as DLLs que o carregador precisa (`Core.dll`, `Engine.dll`,
`npkcrypt.dll` e as dependências) para `build/smoke_system`.

## Caminhos da máquina

Nenhuma ferramenta tem um caminho da máquina do autor: uma pasta fora do repositório vem da opção de linha de
comando ou de uma destas variáveis (a opção vence; a ferramenta que precisa de uma e não tem nenhuma para e diz
qual):

| Variável | Aponta para | Usada por |
|---|---|---|
| `NWD_CLIENT_SYSTEM` | a pasta `system` do cliente de teste (uma cópia: a instalação escreve nela) | `--system` padrão de `install_test.py`, `cycle.py`, `smoke_standalone.py`, `difftest.py`; `--log` de `verify_hooks.py` e `tools/port/verify_args.py`; os `.u` de `ue2_layouts.py` |
| `NWD_GHIDRA_HOME` | a instalação do Ghidra (com `support/analyzeHeadless.bat`; `GHIDRA` ainda tem precedência) | `tools/ghidra/*.sh`, `tools/c2/ghidra_census.sh` |
| `NWD_C3_ROOT` | um checkout do projeto do C3 (com o seu `orig/` e `analysis/ghidra/`): a base do porte | `tools/port/*` (`match`, `maps`, `layout`, `vslots`, `eligible`, `gen`...), `ue2_check_fields_by_code.py`, `ue2_disp_diff.py`, `ue2_guard_pairs.py`, `ue2_console_slots_c4.py` |
| `NWD_C1_ROOT` | um checkout do projeto do C1 | `ue2_vtable_diff.py`, `ue2_ret_diff.py`, `ue2_gamedata_regions.py`, `ue2_console_slots.py`, `tools/c2/compare_nwindow.py` |
| `NWD_C4_SERVER` | a pasta de deploy do servidor (páginas em `gameserver/data/html-en`) | `difftest.py`, conjunto de páginas `server` (pulado sem ela) |
| `NWD_C2_SYSTEM`, `NWD_C3_SYSTEM` | a pasta `system` dos clientes C2 e C3 | `tools/c2/packet_formats.py`; `tools/c3/*_formats.py`, `net_boundary.py`, `gen_recipe_ids.py` |
| `NWD_C3_SERVER_SRC`, `NWD_C3_SPEC`, `NWD_DAT_CRYPT`, `NWD_JAVAC` | a source do servidor de teste do C3, a especificação `C3 Rise Of Darkness.html`, o `oldclient_crypt.py` de um editor de `.dat`, o `javac` | as ferramentas da auditoria de pacotes do C3 (`tools/c3/`) |

## Os juízes

"Verde" é cada um terminar com código 0; o ciclo roda os quatro primeiros (e os do standalone com `--standalone`),
o `tools\port\cycle.bat` acrescenta os do porte.

| Juiz | Comando | O que prova |
|---|---|---|
| smoke | `build\smoke\Release\nwd_smoke.exe build\smoke_system` | o `NWindow.dll` remendado carrega fora do jogo e todo hook dá `ok` (`NWindowDecomp.log`: `hooks: 5456 installed, 5456 registered, 0 errors`) |
| verify | `python tools/verify_hooks.py --log build/smoke_system/NWindowDecomp.log` | cada função nossa contra a original no assembly: bytes desempilhados no `ret`, slots de vtable e imports chamados; diferença aceita só com explicação em `db/verify_notes.csv` |
| regcheck | `python tools/regcheck.py --game-src` | nenhum chamador original depende de um registrador que a nossa função pode mudar (o original foi compilado com `/GL`) |
| db | `python tools/build_db.py --game-src` | regenera `db/functions.csv` e `docs/PROGRESSO.md` |
| verify_data | `python tools/port/verify_data.py` | as constantes e os deslocamentos de dados de cada função batem com os do original (`db/verify_data_notes.csv` explica os que não batem) |
| check_breakers | `python tools/port/check_breakers.py` | nenhuma função com hook chega a um `NW_UNPORTED()` (`int3`) |
| checks | `python tools/port/checks.py` | as 15 verificações estáticas do cabeçalho do arquivo (retorno lido em `eax`/`st0`, `ret N` dos stubs, quadros de pilha, tamanhos, argumentos constantes, CRT, endereços, deslocamentos crus, literais, slots virtuais, `vt_audit`, `vt_slots`...): `CHECKS: ALL OK` |
| standalone: declarações | `python tools/verify_standalone.py --checks source` | os nossos `NW_DECLARE_CLASS` / `NW_CHECK_VTABLE` contra a tabela de classes original |
| standalone: verify | `python tools/verify_standalone.py --dll build/standalone/Release/NWindow.dll` | PE, imports, recursos, exports, classes e vtables iguais aos do original, sem rodar |
| standalone: smoke | `python tools/smoke_standalone.py --dll build/standalone/Release/NWindow.dll` | carregado como a engine carrega: 341 classes registradas, `L2Console`/`L2Font` construídos, saída igual à do original |
| emulador | `python tools/port/emu/run_suite.py --mode hook` / `--mode sa --cases 30 --seed 1` | pedaços do DLL rodam no Unicorn com o código original e com o nosso (hook ou standalone) e escrevem o mesmo ([docs/notas/C4_EmuSA.md](docs/notas/C4_EmuSA.md)) |

O `tools/difftest.py` (casos fixos contra `tests/golden/`) ainda tem os endereços do C1 e se recusa a rodar na
imagem do C4; no C4 o teste diferencial é a suíte do emulador. O ciclo **não** desenha a interface: mudanças
visuais só aparecem num teste no jogo.

## Pastas

```
orig/          binários do seu cliente (fora do git; md5 em orig/README.md)
analysis/      fatos extraídos do original (classes, EH, vtables e layouts do Core/Engine); analysis/c2/, analysis/c3/ =
               os mesmos dados do C2 e do C3; analysis/ghidra/ = export do Ghidra (gerado, fora do git)
db/            banco de funções (functions.csv gerado) + nomes, notas, verificações e mapas do porte dados à mão;
               db/c1/, db/c2/, db/c3/ = as listas dos projetos anteriores
include/ue2/   declarações do Core.dll/Engine.dll do C4, conferidas contra os exports (gen/*.vt.inc gerados)
include/nwindow/, src/nwindow/    a source decompilada, uma classe por arquivo
src/re/        runtime do modo hook (instalação dos hooks, log, ini, compatibilidade do CRT)
src/standalone/                   IMPLEMENT_CLASS / IMPLEMENT_PACKAGE, globais, NWindow.def, GameGuard no-op, log de crash
tests/         saídas de referência do original (golden) e listas do standalone
third_party/ue_import/            .def dos exports do Core/Engine (viram .lib no build)
tools/         análise, geração, instalação, juízes, ciclo; tools/port/ = pipeline C3 → C4, juízes do porte e o
               emulador (tools/port/emu); tools/c2/, tools/c3/ = ferramentas herdadas (censo, auditoria de pacotes)
docs/          plano, convenções, progresso e notas técnicas
```

## Guias

- [docs/PLANO_C4.md](docs/PLANO_C4.md) — o que é do C4 (constantes da imagem, decisões, etapas);
  [docs/PLANO.md](docs/PLANO.md) — o método; [docs/CONVENCOES.md](docs/CONVENCOES.md) — regras de código e armadilhas
- [docs/notas/C4_Standalone.md](docs/notas/C4_Standalone.md) — a etapa 5 (standalone);
  [docs/notas/C4_EmuSA.md](docs/notas/C4_EmuSA.md) — a suíte do emulador contra o standalone;
  [docs/notas/C4_Agentes.md](docs/notas/C4_Agentes.md) — o guia das ondas de porte à mão
- [docs/MODIFICAR_INTERFACE.md](docs/MODIFICAR_INTERFACE.md) — como modificar a interface (barra de atalhos, botão, janela, cor de chat, linha de tooltip)
  · [English](docs/MODIFY_INTERFACE.md) · [Русский](docs/MODIFY_INTERFACE.ru.md)
- [docs/LLM.md](docs/LLM.md) — guia para agentes de IA que forem trabalhar neste código (em inglês)
- O projeto do C1, de onde vêm o método e as ferramentas, foi publicado à parte (com a sua fase 6 de recursos).

## Nota legal

Projeto independente de engenharia reversa, para compatibilidade e preservação. Não é afiliado, autorizado nem
endossado pela NCSoft. *Lineage II* é marca registrada da NCSoft Corporation. **Nenhum arquivo original do
jogo está incluído** (nem DLLs, nem executáveis, nem pacotes de dados): cada usuário fornece o próprio cliente.
A biblioteca do nProtect GameGuard não é reproduzida.

## Licença

[GNU General Public License v3.0](LICENSE).
