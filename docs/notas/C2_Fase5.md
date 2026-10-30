# C2: fase 5 (o NWindow.dll standalone: a nossa source gera a DLL sozinha)

Estado de partida (commit 446f3b3 + a regeneração do `Classes.gen.cpp`): modo gancho com 3.444 ganchos e 91,94 % dos bytes;
todo o código de jogo que não é nProtect/NKeyCrypt está portado. O que a fase 5 faz: compilar a mesma source com
`NW_HOOK_MODE=0` e linkar um `NWindow.dll` que NÃO precisa da imagem original na memória (nada de `NW_HOOK_*`, `NW_STOCK`,
`nw::StockCall`, `NW_GLOBAL`, `nw::Addr`). O guia do C1 (docs/notas/Standalone.md, Andaimes.md) descreve o método; os
endereços e os números de lá são do C1. Tudo o que o `tools/` tem para isso já está no repositório (`standalone_report.py`,
`verify_standalone.py`, `smoke_standalone.py`, `gen_implement_class.py`, `gen_globals.py`, `gen_class_ids.py`).

## Onde estamos (07/10/2026)
`python tools/standalone_report.py` (monta o alvo `NWindowStandalone` num worktree ou na árvore principal; saída em
`build/standalone_build.log`, os símbolos que faltam em `build/trial/sa_report.txt` se você redirecionar):
* **Compila inteiro** com `NW_HOOK_MODE=0` (225 classes no `Classes.gen.cpp`, regerado hoje com
  `python tools/gen_implement_class.py`: 225 classes, 227 exports, "stock check: ok"). Um erro que apareceu: o
  `InternalConstructor` do standalone é `new ((EInternal*)X) TClass` e exige construtor sem argumentos: o `NCTVItem(INT)`
  ganhou o padrão -1 que o `InternalConstructor` do original empilha (`push -1`, 0x1002b4f0).
* **O link falha com 65 símbolos sem definição**: 16 `NW_STOCK`, 39 assinaturas de `nw::StockCall` (151 sites, 99 alvos
  distintos), 9 `NW_GLOBAL`, 1 `nw::Addr` (11 sites).
  * 86 dos 99 alvos de `StockCall` JÁ têm gancho: a chamada vira uma chamada normal à nossa função (trabalho mecânico, mas é
    preciso conferir a assinatura e a convenção: o `StockCall` chama o original com `this` em ecx e `ret N`).
  * 13 não têm: 5 funções do NKeyCrypt (0x1003eb20..0x1003ed50), 3 do nProtect (0x100dff20, 0x100dff40, 0x100e0130) e 5
    endereços de dados do original (0x10114980, 0x1019c3d8, 0x101b1248, 0x101b21d8, 0x101cfb78, via `nw::Addr`).
  * dos 16 `NW_STOCK`, 15 têm gancho (os cinco do NCDialogBox que pertencem ao NCGaraDialogBox, corpos que o linker dobrou
    entre classes, `NCObject::ImeIsUIMessage` 0x10025240 que é uma thunk de 2 bytes para um `ret 0xC`) e 3 são dados.
* **O `src/standalone/NwGlobals.cpp` é do C1** (commit de baseline, gerado por `tools/gen_globals.py` do C1 sobre a imagem do
  C1): nomes iguais, VALORES INICIAIS E ENDEREÇOS DO C1. Os que o C2 mudou (tabelas de experiência, textos, listas)
  ficariam errados em silêncio. `tools/gen_globals.py` e `tools/difftest.py` recusam rodar na imagem do C2.

## Decisões (padrão = "reconstruir o que roda no cliente remendado"; o usuário pode mudar)
* **nProtect GameGuard**: o cliente de teste (`NWindow.dll` remendado) pula o início e o tick do GameGuard; o standalone não
  liga a biblioteca estática (83 funções de terceiros, 28 KB). `NConsoleWnd::GameGuardQuery` e `StartGameGuard` ficam como
  funções vazias documentadas (`src/standalone/NwGameGuard.cpp` ou dentro das próprias funções). Sem o GameGuard o servidor
  que exigir a resposta de autenticação não seria atendido: o cliente remendado também não responde.
* **NKeyCrypt (proteção de teclado, `npkcrypt.dll` por ordinal)**: portado com `LoadLibrary`/`GetProcAddress` por ordinal em
  tempo de execução (igual ao original quando a DLL existe, silencioso quando não existe), em vez de importar a DLL no PE.
  O `NWindow.dll` standalone então não importa `npkcrypt.dll`.

## Grupos (um agente por grupo, um worktree `wt/<nome>` por agente: `bash tools/port/new_worktree.sh <nome>`)
Regras gerais: o ciclo do modo gancho tem de continuar limpo depois de cada mudança (`cmd //c "tools\port\cycle.bat <seu
worktree>"`; ver docs/notas/C2_Agentes.md §5 e C2_Onda3.md, a lista de armadilhas), porque as duas builds compartilham a
source. Em cada arquivo, o ramo do modo gancho (`#if NW_HOOK_MODE`) pode continuar chamando o original por endereço
(`StockCall`) SÓ para funções que de fato continuam sendo do original (as do nProtect); o resto vira chamada normal nos dois
modos. Critério de pronto: os símbolos do seu grupo somem do `python tools/standalone_report.py` (rode-o no seu worktree) e o
ciclo do modo gancho fica limpo. Não commite `db/functions.csv`, `db/verify_data.csv`, `docs/PROGRESSO.md`.

* **p5glob**: as globais e tabelas. Retargetar `tools/gen_globals.py` para o C2 (tirar a recusa; refazer a `SPEC` com os
  endereços e os tipos do C2: cada `NW_GLOBAL(...)`/`nw::Addr(...)` que a source usa em `include/nwindow` e
  `src/nwindow`, 61 + 293 usos; conferir que cada global tem o ramo `#else extern ...` e uma definição com o valor
  inicial da imagem `orig/NWindow.dll`), regenerar o `NwGlobals.cpp` do C2 (apagar o do C1), os 9 `NW_GLOBAL` sem
  definição (0x10129364, 0x1016462c, 0x1017549c `GCommandSlash`, 0x101761cc, 0x101878ec `GShortCutCmdPrefix`,
  `GGameGuardConsole`, `GGameGuardTimer` (essas duas são do p5nprot), os dois sem nome `2` e `255` do NCCalcWnd/NCCreditWnd)
  e os dados que o `nw::Addr` alcança em `NConsoleWndWindows.cpp`/`NCLobbyWnd.cpp` (0x10114980, 0x1019c3d8, 0x101b1248,
  0x101b21d8, 0x101cfb78: ver o que são na imagem). `gen_globals.py --check DLL MAP` lê cada global do DLL standalone
  montado e compara com o objeto do original (quando houver link).
* **p5calls1**: os `nw::StockCall` dos arquivos do console: `NConsoleWndC2.cpp` (a maior parte: ~60 sites; hennas, manor,
  receitas, macros, cadastros), `NConsoleWndEvents/Input/Items/Login/Windows/Misc.cpp`, `NCConsole.cpp`, `NMacroRun.cpp`
  (menos os sites do NKeyCrypt/nProtect, que são do p5nprot). Cada site vira chamada tipada à função nossa
  (`GConsole->...`, o método da classe); 0x1006ed50 `NCDebugWnd::NCDebugWnd` e 0x10051a00 `NCConsole::NCConsole`: construtores
  (use `new`/a forma que o resto do código usa).
* **p5calls2**: os `nw::StockCall` dos outros arquivos (NCHtmlViewer, NCLoginWnd/NCLobbyWnd/NCPawn*Wnd, NCShortCutWnd,
  NCMacroListWnd/ArrayWnd, NCCreditWnd, NCChatWnd, NCCalcEditCtrl, NCEquip/Inven/PetInventory, NCFrameCtrl, NCAuthWnd,
  NCMSViewrWnd, NCommandParser, NCSystemMenuWnd, NCWnd, NHtmlLayout...: ver a lista de sites no
  `sa_report` do seu worktree) e os 16 `NW_STOCK`: as cinco funções do NCDialogBox (0x10006740, 0x10006c50, 0x10006cf0,
  0x10007860, 0x10007a00: no C2 são do NCGaraDialogBox; os chamadores que usam `NCDialogBox*` passam a usar a classe certa
  ou a ter o método real), `NCItemWnd::WindowProc` 0x10019810 / `DrawItemToolTip` 0x10019c40 / `NCNoticeInfoWnd::OnCreate`
  0x1002ce20 / `NCWnd::OnKeyDown` 0x1002b980 (corpos dobrados pelo linker entre classes: cada classe define o seu), 0x1002f6a0
  `SetOwner`, 0x100b2b90, 0x100c1110, 0x100c3310, 0x100d8950, 0x100d8aa0, `NCObject::ImeIsUIMessage` 0x10025240.
* **p5nprot**: nProtect e NKeyCrypt (ver "Decisões"): as 5 funções do NKeyCrypt (0x1003eb20, 0x1003eb30, 0x1003eb60,
  0x1003ed40, 0x1003ed50 e os sites em `NCLoginWnd.cpp`/`NConsoleWndWindows.cpp`), `InitNPGameMon` 0x100dff20,
  `SetHwndToGameMon` 0x100dff40, `SendAuthQuery` 0x100e0130 (em `NConsoleWndMisc.cpp`/`NConsoleWndC2.cpp`),
  `GGameGuardConsole`/`GGameGuardTimer`; os imports que a imagem original tem e a source standalone não (`WS2_32`,
  `ADVAPI32`, `WINMM` já estão na linha de link; confira os imports do `NWindow.dll` original contra o do standalone com
  `python tools/verify_standalone.py --dll X --map M`) e o que mais o `verify_standalone` apontar de PE/imports/recursos.
  Documente tudo no fim de `docs/notas/C2_Fase5.md` (uma seção "nProtect e NKeyCrypt no standalone").
* **p5ctor**: o `InternalConstructor` do standalone. Para as 225 classes, ler do asm do original (`analysis/ghidra/asm/<va>.asm`
  das 186 funções `InternalConstructor` do banco, `db/functions.csv`) os argumentos que ele empilha antes do `call` do
  construtor (ex.: `NCTVItem`: `push -1`; as caixas de texto, status etc. têm vários) e conferir/corrigir os valores padrão
  dos construtores declarados em `include/nwindow` (o standalone gera `new ((EInternal*)X) TClass` e usa os padrões).
  Fazer uma ferramenta (`tools/port/check_internal_ctors.py`) que lista classe, argumentos do original, declaração
  nossa, e diz o que difere. Verificar também as flags/pai/`Within` (o `gen_implement_class.py` já confere o registro).

## nProtect e NKeyCrypt no standalone (p5nprot, 07/10/2026)

Resumo: as duas decisões da seção "Decisões" foram executadas como estavam. O NKeyCrypt virou código real (7 ganchos novos,
verificados, mais um teste diferencial com emulação), os três pontos de entrada do GameGuard viraram chamadas por trás de
`include/nwindow/NGameGuard.h` (original no modo gancho, no-op documentado no standalone), as duas globais do GameGuard têm
definição, e o `tools/verify_standalone.py` ganhou a conferência de imports. Modo gancho depois de tudo: **3.451 ganchos**
(+7), verificação 3.112 ok + 339 explicadas + 0 sem explicação, `regcheck` 0 problemas, `verify_data` 0 diferenças, 0 ganchos
alcançam um stub que quebra. No `standalone_report` do worktree os símbolos do grupo sumiram (os 5 alvos de `StockCall` do
NKeyCrypt, os 3 `nw::Addr` do nProtect, `GGameGuardConsole`/`GGameGuardTimer`: `NW_GLOBAL` 9 para 7).

### nProtect GameGuard

* **O que é.** 83 funções de terceiros ligadas estaticamente, `0x100DFEE0..0x100E717F` (28.658 bytes, sem UI; strings
  ofuscadas). O jogo só chama as `NPGameLibApi::*`: `PreInitNPGameMonW` 0x100dff60 (do inicializador estático
  0x100FFB60, via `CreateGameMonObject` 0x10057e20 e `PreInitWrapper` 0x10051b50), `InitNPGameMon` 0x100dff20,
  `SetHwndToGameMon` 0x100dff40, `CheckNPGameMon` 0x100e0110 (todo quadro; o remendo do empacotador pula o desvio condicional
  do `Tick`, 0x1006BDD9), `SendAuthQuery` 0x100e0130, `CloseNPGameMon` 0x100e0150 (do `atexit` 0x10105680). O código do
  jogo que a biblioteca chama de volta é `GameMonMessageCallback` 0x10057940 (a única que lê `GGameGuardConsole`).
* **No standalone** a biblioteca não é ligada. Por isso também não existe o inicializador estático que cria o objeto
  (nada de `GameGuard\npgl.erl`, de `GameMon.exe` lançado, do `DisableAppCompat` no registro). `PreInitWrapper`,
  `CreateGameMonObject`, `GameMonMessageCallback` ficam sem chamador e sem porte (continuam `todo` no banco, junto das 83).
* **`include/nwindow/NGameGuard.h`** (namespace `NGameGuard`) tem as três chamadas que o nosso código faz, todas cdecl:

  | Função | Endereço | Modo gancho | Standalone |
  |---|---|---|---|
  | `Init()` | 0x100dff20 | chama o original (`nw::Addr`) | devolve 0x755 (sucesso da biblioteca) |
  | `SetHwnd(INT)` | 0x100dff40 | chama o original | não faz nada |
  | `SendAuthQuery(INT*)` | 0x100e0130 | chama o original | devolve 0 (a consulta do servidor fica sem resposta) |

  O 0x755 é para o `StartGameGuard` seguir para o `SetHwnd` sem mostrar erro (a biblioteca parada responderia 0, que o
  `StartGameGuard` mostraria como "(ErrorCode=0)"). `StartGameGuard` (0x100574e0) não é chamado em lugar nenhum nos dois
  modos (o ramo `GIsL2NetLogin` do `Initialize` original o chamava e o remendo do empacotador pula); `GameGuardQuery`
  (vslot 29, a consulta do servidor via `UGameEngine::OnGameGuardQuery`) é o único caminho vivo; na biblioteca, com o objeto
  não iniciado, `SendAuthQuery` devolve 0 sem enviar nada, que é o que o standalone faz. Usos: `NConsoleWndMisc.cpp` e
  `NConsoleWndC2.cpp`.
* **Globais** (`src/standalone/NwGameGuard.cpp`, só standalone; no modo gancho continuam `NW_GLOBAL`): `GGameGuardConsole`
  (0x101b1c9c, `NConsoleWnd*`, escrito no fim do `Initialize`) e `GGameGuardTimer` (0x101b1ca4, `FLOAT`, somado no `Tick`).
  Os dois são .bss (valor inicial 0). Nenhum dos dois é lido no standalone.
* **Pendente do p5glob** (não mexi para não colidir): `StartGameGuard` compara a resposta com `NW_GLOBAL(INT, 0x10129364)`;
  é um dado de .data com 0x755 que ninguém escreve (a única referência é essa comparação): pode virar uma constante.

### NKeyCrypt (proteção de teclado, `npkcrypt.dll`)

`include/nwindow/NKeyCrypt.h` e `src/nwindow/NKeyCrypt.cpp`. O objeto tem 4 bytes (o handle do driver), mora em
`NConsoleWnd` +0x359C (agora `NKeyCrypt* KeyCrypt`, antes `INT Unknown359C`) e é feito no `Initialize` com `GL2UseKeyCrypt`.

| Endereço | Função | Gancho | O que faz |
|---|---|---|---|
| 0x1003eb20 | `NKeyCrypt()` | `NW_HOOK_CTOR` | handle = 0 |
| 0x1003eb30 | `Close()` | método | com handle > 0 chama `NPKCloseDriver` e zera o handle se deu certo; 1 sem handle |
| 0x1003ed40 | `~NKeyCrypt()` | `NW_HOOK_DTOR` | o original é um `jmp` para `Close` |
| 0x1003eb60 | `RegisterMessage()` | método | `NPKRegCryptMsg(handle, GViewportHWnd, 0x508)` |
| 0x1003eb90 | `EnableProtectRegistry()` | estática | HKLM\Software\INCAInternet\npkcx, valor `protect` = 1 quando é 0 ou não existe |
| 0x1003ec20 | `IsAdministrator()` | estática | o token do processo tem o grupo Administrators (S-1-5-32-544), seja qual for o atributo; 0 sem `GUnicodeOS` |
| 0x1003ed50 | `Open()` | método | ver abaixo |
| 0x1003ed30 | `GetLoadStartup()` | sem gancho | o thunk do original é `jmp [NPKGetLoadStartup]` (ordinal 41) |
| 0x1003eb80 | `LoadAtStartup(INT)` | sem gancho | o thunk é `jmp [NPKLoadAtStartup]` (ordinal 26) |

* **`Open`**: log "OpenKeyCrypt start", `Close()`, caminho do jogo (`GFileManager`, slot 14 = `GetDefaultDirectory`, que no C2
  é o 13 do UE2 mais o `CreateMemReader` novo no 1) + `\L2.exe`; se o L2.exe tem modo de compatibilidade (`NPKGetAppCompatFlag`
  diferente de -1) o limpa (`NPKSetAppCompatFlag(-1)`), limpa também o do `LineageII.exe` da pasta acima e termina com 0
  ("Compatibility mode detected."); senão, se o `LineageII.exe` tem o modo, limpa e termina com 0; senão `EnableProtectRegistry`,
  `NPKSetDrvPath(GetCurrentDirectoryA)`, `NPKOpenDriver`: handle > 0 = 1 ("Open KeyCrypt Driver Succeed"), senão 0 com
  "Open KeyCrypt Driver Failed : <GetLastError>".
* **As 8 ordinais** (as que a imagem original importa de `npkcrypt.dll`, todas stdcall: os chamadores não limpam a pilha;
  assinaturas deduzidas do uso nos chamadores): 1 `NPKOpenDriver()`, 2 `NPKCloseDriver(handle)`, 16 `NPKRegCryptMsg(handle, hwnd, msg)`,
  25 `NPKSetDrvPath(char*)`, 26 `NPKLoadAtStartup(BOOL)`, 36 `NPKGetAppCompatFlag(WCHAR*)`, 37 `NPKSetAppCompatFlag(WCHAR*, int)`,
  41 `NPKGetLoadStartup()`. A DLL exporta também os nomes; usamos as ordinais como o original.
* **Carregamento** (os dois modos, mesmo código): `NpkLoad()` faz `LoadLibraryA("npkcrypt.dll")` e `GetProcAddress` por ordinal
  na primeira vez que alguma função precisa e guarda o resultado (nunca descarrega, como o original). Se a DLL não existe, ou
  falta uma das 8 ordinais, nenhuma função dela roda: `Open` termina com 0 e o log "Open KeyCrypt Driver Failed : <erro do
  LoadLibrary>" (a tela de login mostra o aviso do teclado, como quando o driver não abre), `RegisterMessage` e `GetLoadStartup`
  dão 0, `LoadAtStartup` não faz nada, `Close` dá 0 se havia handle. `EnableProtectRegistry` e `IsAdministrator` não dependem da DLL.
  No modo gancho o `LoadLibraryA` devolve o módulo que o original já importou: as mesmas funções.
* **Diferenças em relação ao original** (todas anotadas): o tipo do valor que `EnableProtectRegistry` passa ao `RegSetValueEx`
  quando a consulta falha é REG_DWORD (o original passa lixo de pilha); o caminho sem DLL acima (o original nem carregaria).
* **Chamadores convertidos** nos dois modos (sem `StockCall`): `NConsoleWndWindows.cpp` (`Initialize`: `GMalloc->Malloc(4)` e
  construtor; `Finalize`: `Close`, destrutor e `Free`), `NCLoginWnd.cpp` (`ShowWindow`: `Open` e `RegisterMessage`;
  `HideWindow`: `Close`), `NCGameOptionWnd.cpp` (os três auxiliares `KeyCryptGetLoadStartup/LoadAtStartup/IsAdmin`, que no
  standalone eram `NW_UNPORTED` = int3: `LoadSettings` e `Apply` quebrariam).
* **Verificação.** Ciclo do modo gancho como acima. Notas novas: `db/verify_notes.csv` 1003eb30, 1003eb60, 1003ed40, 1003ed50
  (os imports `#2`, `#16`, `#1/#25/#36/#37` do original viram ponteiros de `GetProcAddress` que o verificador não nomeia) e as
  reescritas de 100a3ad0 e 100a3ec0; `db/verify_data_notes.csv` 1003eb30, 1003ed40 (o `NpkLoad` que a conferência segue),
  1003ec20 (`SECURITY_NT_AUTHORITY` como a palavra 0x500), 100a3ad0 (o ponteiro do objeto que o original carrega em ecx para
  funções que o ignoram) e a reescrita de 100a3ec0. `db/names.csv`: a linha do 1003eb90 estava quebrada em duas (o `\n` de
  `\npkcx` virou quebra de linha), corrigida; 1003ed40 agora é o destrutor; os três pontos de entrada ganharam a nota do standalone.
* **Teste diferencial** `python tools/port/emu/t_nkeycrypt.py <casos> <semente>` (unicorn; precisa de
  `build/bin/Release/NWindowDecomp.dll` + `.map`): o original e o nosso rodam `Close`, `RegisterMessage`, `Open`, `Close`,
  `EnableProtectRegistry`, `IsAdministrator`, `GetLoadStartup`, `LoadAtStartup` sobre cenários aleatórios (com e sem `GLog`;
  modo de compatibilidade no L2.exe e/ou no launcher; `NPKOpenDriver` falhando ou não; registro sem a chave, sem o valor, com 0 e
  com 1; token com e sem o grupo; handle inicial 0, > 0 e -1) com os mesmos substitutos em Python; o registro das chamadas
  (argumentos incluídos), o retorno e o handle têm de ser iguais. **3 sementes × 1.500 cenários × 8 chamadas: 0 diferenças**
  (mascarado só o tipo acima). Roda também o nosso sem DLL (50 cenários: nada da DLL é alcançado, `Open` 0, o log certo).

### PE, imports e as conferências do standalone

`tools/verify_standalone.py` (PE, imports, recursos, exports, classes, vtables, source), `tools/smoke_standalone.py` e
`tools/install_test.py` já estavam no C2 (constantes, 227 exports, 225 classes). O que ainda era do C1 e mudou:
* `tests/standalone/l2_extras.csv` esvaziado: as 8 `NCRecipe*Wnd` que a lista tinha (a "craft" da fase 6 do C1) são classes
  da imagem do C2; sem isso a conferência `source` falhava 8 vezes. A conferência `source` agora lê `src/nwindow` e `include/`
  por padrão (`--no-game-src` desliga) e ignora linhas de comentário (os `// C2: no vtable for NW_CHECK_VTABLE(...)` contavam).
* **Conferência nova `imports`**: só DLLs que a imagem original importa e nenhuma é `npkcrypt.dll`; tudo o que o código de
  JOGO do original chama tem de estar importado (o que só a biblioteca nProtect ou o CRT estático usam não é exigido; quem usa
  o quê sai de um desassembly das funções de `db/functions.csv`, com os `jmp [IAT]` seguidos até os chamadores); e a linha de
  link do `CMakeLists.txt` do alvo standalone tem uma biblioteca para cada DLL exigida. Explicações em `tests/standalone/notes.csv`.
* `smoke_standalone.py`: o System do standalone NÃO recebe `npkcrypt.dll`/`npkpdb.dll` (só o do controle); `--with-npk` os copia.
* `cycle.py --standalone`: o `difftest --standalone` só roda com `--difftest` (os casos ainda são do C1 e o `difftest.py` recusa a imagem do C2).
* `sizeof(UClass)` do C2 = 0x520 (o registro de `UClass` no `Core.dll` empilha 0x520): o `NwStandalone.h` está certo.

**Os imports que o standalone deve ter** (medidos com um link `/OPT:NOREF` de todos os objetos, que não dá nenhum símbolo sem
definição fora das famílias `NW_*`/`nw::*` e dos dois globais sem nome do p5glob: a linha
`${UE_IMPORT_LIBS} user32 imm32 version winmm advapi32 ws2_32` mais o kernel32 padrão fornece tudo):

| DLL | original | usados pelo código de jogo | standalone | observação |
|---|---:|---:|---:|---|
| Core | 161 | 137 | 159 | faltam `GIsSSE` (o `appMemcpy` fora de linha do original) e `??_7FExec@@6B@` (só o construtor do `NL2Font` original); com notas |
| Engine | 98 | 79 | 98 | igual |
| USER32 | 15 | 14 | 14 | falta `wsprintfA`, só da biblioteca |
| IMM32 | 16 | 15 | 15 | falta `ImmIsUIMessageW`: o thunk não tem chamador (o C2 esvaziou o `ImeIsUIMessage`) |
| VERSION | 3 | 3 | 3 | `GetFileVersionInfoA/SizeA`, `VerQueryValueA` |
| WS2_32 | 12 | 12 | 12 | MSN e o `L2WordWrapperWnd`: `WSAStartup`, `socket`, `connect`, `send`, `recv`, `WSAAsyncSelect`... |
| ADVAPI32 | 29 | 9 | 9 | só as do NKeyCrypt: `RegOpenKeyW`, `RegQueryValueExW`, `RegSetValueExW`, `RegCloseKey`, `AllocateAndInitializeSid`, `OpenProcessToken`, `GetTokenInformation`, `EqualSid`, `FreeSid` (as outras 20 são o CryptoAPI e o registro ANSI da biblioteca) |
| WINMM | 1 | 0 | 0 | `timeGetTime` é só da biblioteca: nada importa; `winmm` na linha de link é inofensivo e não precisa |
| KERNEL32 | 109 | 14 | 89 | as 14 do jogo estão (`LoadLibraryA`, `GetProcAddress`, `MultiByteToWideChar`, `WideCharToMultiByte`, `GetVersionExA/W`, `GetLocaleInfoA`, `lstrcmpiA`, `FindFirstFileW`, `FindNextFileW`, `GetCurrentDirectoryA`, `GetCurrentProcess`, `GetLastError`, `CloseHandle`); o resto é o CRT novo |
| npkcrypt | 8 | 8 | 0 | decisão: `LoadLibrary` + `GetProcAddress` por ordinal no `NKeyCrypt` |

A linha de comentário do `CMakeLists.txt` do alvo standalone ("o C2 NWindow.dll também importa ... npkcrypt.dll (por ordinal)")
ficou velha com a decisão (o standalone não importa a `npkcrypt.dll`); o `CMakeLists.txt` não é da lista de arquivos deste
grupo e não foi mexido. O alvo do modo gancho usa o `advapi32` (NKeyCrypt) pelas bibliotecas padrão do CMake.

**Resultado hoje** (`python tools/verify_standalone.py --stock`: OK, pe 12, imports 11, recursos 7, exports 231, classes 226,
vtables 227, source 3 + 2 explicadas; `smoke_standalone.py --control-only`: ALL STAGES OK). Na sonda `/FORCE:UNRESOLVED`
(`python tools/standalone_report.py --probe` e depois `verify_standalone.py --dll build/standalone_probe/Release/NWindow_probe.dll
--map ...NWindow_probe.map`): pe, recursos, classes (225/225) e exports passam (o nome do DLL da sonda não é `NWindow.dll`; os
imports que o `/OPT:REF` joga fora por funções inalcançáveis, `appRand`, `GetSocialAnimDuration`, `recv`, aparecem como falha
até o link fechar). Achados reais da sonda, para quem fecha o link:
* o vtable do `NCListCtrl` (e das 3 filhas) tinha 75 slots contra 74 do original: o `DrawRowToolTip` declarado `virtual` ("slot
  gone" no C2); corrigido em `NCListCtrl.h`, `NCMatchListCtrl.h` e `NCSiegeInfoMainWnd.h` (não é mais virtual; ciclo igual);
* 8 vtables ainda falham (`NCDialogBox`, `NCKeyCryptWarningDialogBox`, `NCWarningDialogBox`: slots 29/43 do `NCWnd`;
  `NCGMQuestWnd` 71, `NCNpcViewrWnd` 58, `NCQuestInfoWnd` 25/36, `NCQuestItemWnd` 72, `NCTrainListBox` 70): são os `NW_STOCK` do
  p5calls2 (o corpo é um `int3` que o ICF dobrou entre classes: aparece como `NCTrainListBox::Clear`); somem quando eles fecharem.

**Quando o link fechar** (na ordem): `python tools/standalone_report.py` (0 símbolos), `python tools/verify_standalone.py --dll
build/standalone/Release/NWindow.dll --map build/standalone/Release/NWindow.map` (tudo OK; as 8 vtables acima e `appRand`,
`GetSocialAnimDuration`, `recv` têm de sumir), `python tools/smoke_standalone.py` (a mesma saída do original, sem a
`npkcrypt.dll` no System do standalone), `python tools/port/emu/t_nkeycrypt.py 1500 1`.

## Resultado (07/10/2026): o NWindow.dll standalone linka, verifica e carrega fora do jogo
Commit 233bdee. Os cinco grupos (p5glob, p5calls1, p5calls2, p5nprot, p5ctor) foram fundidos; o link fechou sem nenhum símbolo
faltando e o `build/standalone/Release/NWindow.dll` (1.509.888 bytes) passa em tudo o que existe hoje:
* `python tools/verify_standalone.py --dll build/standalone/Release/NWindow.dll --map build/standalone/Release/NWindow.map`:
  pe 12 ok, imports 11 ok + 2 explicados (GIsSSE e a vtable do FExec: só o código original os usa), recursos 7 ok, exports 231 ok
  (os 227 nomes com os ordinais do original), classes 226 ok (225 registros iguais aos do original), vtables 227/227 (as 5
  que falharam eram sobrescritas redundantes: `NCDialogBox::SetFocus/ShowWindow/OnKeyDown`, `NCNpcViewrWnd::OnKeyDown`, e o
  `NCGMQuestWnd::OnTab` que era um `int3`; o original aponta o slot direto para a função da classe base), source 3 ok.
* `python tools/smoke_standalone.py`: os 9 estágios (bind, dllmain, exports, queue, init, registered, load, construct-font,
  construct-console) passam e a saída, 353 linhas incluindo o log do Core, é a MESMA do `NWindow.dll` original.
* `python tools/gen_globals.py --check ...`: 82 globais lidas de volta do DLL montado, 0 diferentes do objeto original.
* `python tools/port/check_internal_ctors.py`: 225 registros compilados idênticos aos originais, 0 problemas.
* Nenhuma função com corpo `int3` sobrou no standalone (as dez `NW_UNPORTED_NORET` que restam na source são código só do C1,
  sem chamador no C2: o `/OPT:REF` as remove do link).
* Modo gancho: 3.451 ganchos, 0 sem explicação, regcheck limpo, 0 breakers (o `cycle.bat` continua a prova do modo gancho).

Erros que só o standalone mostrou e que foram consertados: `NCObject::PlayInterfaceSound` era `int3` (toda janela com som
quebrava), `NCWnd::Snap` usava `GetCursorPos`, 3 auxiliares do NKeyCrypt em `NCGameOptionWnd` eram `int3`, 54 das 63 tabelas
do C1 eram iguais mas `GServerNames` (todos os nomes deslocados de 1), `GKeypadTextures` (13 pares, não 11), `GHtmlTagInfo`
(51 tags, não 50) e o endereço do `GTextAlignParser` eram do C1.

### O que falta da fase 5
1. **Teste no jogo**: instalar o `NWindow.dll` standalone no lugar de "original + NWindowDecomp.dll" em `C2_Client\system_nwtest`.
   O `install_test.py --standalone` recusa pastas fora de `build/` de propósito: precisa da permissão do usuário. Mostra o que
   só o jogo exercita (o `Init` do console e as ~45 janelas, fontes e texturas, IME, callbacks de rede, a saída).
2. **Teste diferencial do standalone**: rodar os testes de emulação (tools/port/emu, ~30) com o standalone no lugar do
   `NWindowDecomp.dll` (o código compilado com `NW_HOOK_MODE=0` e `/Ob1` tem de fazer o mesmo que o do modo gancho, e as
   globais agora são nossas, não as da imagem original). Os testes localizam as funções pelo log do smoke; no standalone é
   preciso o mapa.
3. Decisões abertas: GameGuard/NKeyCrypt (padrão: sem GameGuard, KeyCrypt por `LoadLibrary`), e os 8 remendos binários.
