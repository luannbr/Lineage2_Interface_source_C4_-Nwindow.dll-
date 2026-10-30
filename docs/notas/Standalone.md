> **Nota do C2 (06/10/2026):** este documento é do projeto do C1 e descreve o `NWindow.dll` do C1 (178 classes, endereços `0x100…` do C1). Os fatos do C2 e o que já foi retargetado estão em [../PLANO_C2.md](../PLANO_C2.md); os endereços daqui não valem para a imagem do C2 (o `db/fmap.csv` traduz C1 → C2).

# Standalone (fase 5) — inventário

O que falta para a nossa source gerar o `NWindow.dll` sozinha, sem o original na memória.
Levantado em 26/09/2026 sobre o master `0e9567f`, a partir de `orig/*.dll`, do export do Ghidra
(`analysis/ghidra/asm`), de `analysis/classes.json` e de `db/functions.csv`. Os números mudam
conforme as rodadas avançam: `python tools/standalone_report.py` refaz as contas (seção 10).

**27/09/2026: o `NWindow.dll` standalone linka e carrega fora do jogo.**
- Na rodada 11 do lobby, o esqueleto. Com as globais do status e as exceções do menus, o master
  `23ad825` linka:
  - 1.266.688 bytes (o original tem 1.277.972);
  - passa 100 % no `verify_standalone.py` do clan: pe, recursos, exports, classes, vtables.
- Com `/Ob1` (rodada 12, decisão do coordenador) o DLL tem 1.253.888 bytes. O `verify_standalone`
  continua OK, e o `difftest --standalone` fica em 10 iguais e 1 pulado (`vectors`).
- Na rodada 12, o smoke standalone (seção 11) carrega o nosso `NWindow.dll` pelo caminho da
  engine, fora do jogo. Ele faz tudo que o original faz até o `Init` do console:
  - registra as 178 classes;
  - carrega `NWindow.ConsoleWnd` e `NWindow.L2Font`;
  - constrói os dois objetos.

  A saída é a mesma do original, linha a linha.

**Resumo.**
- O modo gancho está praticamente fechado: dos 3.011 corpos do jogo no banco, 2.738 estão feitos.
- Os 273 que sobram não pedem decompilação à mão:
  - 176 são `scalar deleting destructor`;
  - 76 são `InternalConstructor`;
  - o resto são instâncias de template, cópias dobradas e o `DllMain`.
  - No standalone o compilador e os macros do UE2 geram todos eles, como geram os 542
    inicializadores estáticos.
- O trabalho da fase 5 é, portanto, de montagem:
  - o `IMPLEMENT_CLASS` e o `IMPLEMENT_PACKAGE`;
  - as definições das globais que hoje moram no original;
  - os poucos andaimes que ainda chamam código original;
  - os recursos (7 cursores).

## 1. Como o `NWindow.dll` entra no processo

- **Nenhum módulo importa o `NWindow.dll`.** Conferi o `L2.exe` e todas as DLLs do System de teste
  (só o `NWindow.dll` remendado importa a `NWindowDecomp.dll`).
- É um **pacote nativo do UE2 sem `.u`** (não existe `NWindow.u` no System).
  - O `Engine.dll` carrega duas classes pelo ini:
    - `ini:Engine.Engine.L2Console` (`StaticLoadClass` de `UL2ConsoleWnd`), valor `NWindow.ConsoleWnd`;
    - `ini:Engine.Engine.L2Font`, valor `NWindow.L2Font`.
  - O Core cria o `UPackage` "NWindow" e o `UPackage::Bind` faz o `LoadLibrary("NWindow.dll")`
    (nome do pacote + `.dll`).
  - Os inicializadores estáticos da DLL constroem as 178 `UClass` (cada uma entra na lista
    `GAutoRegister`) e o `ProcessRegistrants` as registra no pacote.
- **Exports do original: 180, todos procurados por nome.**

  | Ordinal | Nome | O que é |
  |---|---|---|
  | 1 | `_DllMain@12` | `0x10024080`, o `DllMain` do `IMPLEMENT_PACKAGE`: `hInstance = hInInstance; return 1;` |
  | 2 | `GPackage` | `0x1010ac88`, `TCHAR[] = L"NWindow"` (dado) |
  | 3–180 | `autoclass<Classe>` | `UClass*` = `&Classe::PrivateStaticClass`, em ordem alfabética |

  O Core tem a string `autoclass%s` e acha as classes com `GetProcAddress`; nenhum módulo usa os
  ordinais.
- **O contrato com a engine:**
  - o arquivo tem de se chamar `NWindow.dll`;
  - `GPackage` = `L"NWindow"`;
  - cada `UClass` com o mesmo nome (`TEXT(#TClass)+1`: `NCButton` → "CButton"), o mesmo tamanho,
    as mesmas flags e o mesmo pai;
  - as vtables com os slots do pai da engine (`UObject`, `UL2ConsoleWnd` com 285 slots, `UL2Font`).
    O verificador do modo gancho já confere todas elas: o `NW_CHECK_VTABLE` está nas 178 classes.

## 2. O que o `NWindow.dll` importa

| DLL | Total | Chamado pelo nosso código | Só por vtables/thunks | Só pelos inicializadores | Só por código original |
|---|---|---|---|---|---|
| Core | 151 | 120 | 24 | 5 | 2 |
| Engine | 82 | 61 | 19 | 2 | 0 |
| IMM32 | 16 | 16 | | | |
| VERSION | 3 | 3 | | | |
| USER32 | 13 | 13 | | | |
| KERNEL32 | 70 | 8 | 62 do CRT estático | | |

- **Core e Engine.**
  - As `.lib` do build (`third_party/ue_import/*.def`) já têm todos os exports dos dois; o
    standalone linka com as mesmas.
  - Os nomes decorados das nossas declarações já batem com os exports: o build do modo gancho
    exige isso.
  - Os 5 dos inicializadores:
    - o construtor nativo e o destrutor de `UClass`;
    - `UObject::StaticConfigName`;
    - `UObject::StaticConstructor`;
    - `UObject::PrivateStaticClass`.
  - Os 2 do Engine são os `PrivateStaticClass` de `UL2ConsoleWnd` e de `UL2Font`.
  - Os 2 "só por código original":
    - `UObject::operator delete`, que os destrutores de apagar gerados pelo compilador chamam;
    - `GIsSSE`, do `appMemcpy` que o original emitiu fora de linha.
- **KERNEL32.**
  - O código do jogo usa 8: `MultiByteToWideChar`, `WideCharToMultiByte`, `GetVersionExA/W`,
    `GetLocaleInfoA`, `LoadLibraryA`, `GetProcAddress` e `lstrcmpiA`.
  - Os outros 62 são do CRT estático do VC 7.0 (heap, TLS, stdio) e somem com o CRT novo, que
    importa os seus.

## 3. As 178 `UClass`

Os dados estão em `analysis/classes.json` (`tools/extract_classes.py` os lê dos inicializadores).

**O que é igual em todas:**
- pacote `NWindow` (o `GPackage`), `Within` = `UObject` e GUID 0;
- `StaticConstructor` = `UObject::StaticConstructor` (Core);
- config = `UObject::StaticConfigName()` (Core); nenhuma classe muda;
- flags de objeto `RF_Public|RF_Standalone|RF_Transient|RF_Native` (`0x4084004`).

**Flags de classe.**
- 174 com 0.
- `CLASS_Abstract` em `NCAbstractMain` e `NCCmdTarget`.
- `CLASS_Config` em `NConsoleWnd` e `NL2Font`.

**Pais.**
- 175 são classes do próprio pacote.
- 3 vêm de fora: `NCObject` : `UObject`, `NConsoleWnd` : `UL2ConsoleWnd`, `NL2Font` : `UL2Font`.

**`sizeof(UClass)` do C1 = 0x520.** É o passo entre os `PrivateStaticClass` no `.data`. O
standalone precisa da `UClass` completa só para definir esses objetos.

**O construtor nativo do Core do C1:**
`??0UClass@@QAE@W4ENativeConstructor@@KKPAV0@1VFGuid@@PBG33KP6AXPAX@ZP8UObject@@AEXXZ@Z`, isto é,
`UClass(ENativeConstructor, DWORD Size, DWORD ClassFlags, UClass* Super, UClass* Within, FGuid,
const TCHAR* Name, const TCHAR* Package, const TCHAR* Config, DWORD Flags, void(*)(void*),
void (UObject::*)())`. É o do UE2 (UT2003).

**O inicializador de cada classe (119 bytes)** empilha:
- `EC_NativeConstructor` e `sizeof`;
- as flags e o `StaticClass()` do pai;
- `UObject::StaticClass()` e o GUID zerado;
- `TEXT("NCButton")+1`, `GPackage` e `StaticConfigName()`;
- `0x4084004`, o `InternalConstructor` e o `&UObject::StaticConstructor`.

Depois chama `UClass::UClass` e registra no `atexit` o destrutor (11 bytes:
`mov ecx, &PSC; jmp [~UClass]`). É o `IMPLEMENT_CLASS` do UE2 sem nenhuma mudança.

**O `autoclassX`.** No original é outro inicializador de 11 bytes
(`mov [autoclassX], &X::PrivateStaticClass`). Com o compilador novo vira inicialização constante.

**`InternalConstructor`: `new ((EInternal*)X) TClass;`.**
- Toda classe precisa de um construtor chamável sem argumentos. Sem parênteses depois de
  `TClass`: com eles, uma classe sem construtor próprio seria zerada por inteiro, cabeçalho do
  `UObject` incluído.
- No original são 176 corpos distintos (o de `NCObject`/`NCCmdTarget`/`NCAbstractMain` é um só)
  do tipo `if (X) jmp Classe::Classe`.
- **26 passam argumentos:** eram os valores padrão dos construtores no header do original
  (`NCCheckBox(TEXT("default"), 0, 0)`, as barras de status com 100, os itens com 32…). Os 17
  construtores nossos que não tinham esses padrões ganharam os do original (rodada 11).
- **`NCCmdTarget` e `NCAbstractMain` não têm `InternalConstructor` próprio.** É o
  `DECLARE_ABSTRACT_CLASS` do UE2: a classe herda o do pai e registra o do `NCObject`
  (`0x100042f0`). Por isso os três são um só. O `NCAbstractMain` nem poderia ter: tem virtuais
  puras (`_purecall` nos slots 24–28). No nosso código é o `NW_DECLARE_ABSTRACT_CLASS`.
- 76 aparecem no banco como `todo`; os outros o Ghidra não separou.

**Divergências entre os nossos `NW_DECLARE_CLASS` e o registro do original.** Não têm efeito no
modo gancho, mas o standalone precisa respeitá-las.
- `NL2Font`: o nosso passa flags 0 e o original registra `CLASS_Config`. É o caso de corrigir o
  header (`NW_DECLARE_CLASS(NL2Font, UL2Font, CLASS_Config, …)`). O registro gerado já usa
  `CLASS_Config` (as flags vêm do `classes.json`); a correção do header só deixa o
  `StaticClassFlags` coerente.
- `NCTestHtmlViewer` (base C++ `NCNPCHtmlViewer`) e `NCTradeItemWnd` (base C++ `NCItemWnd`):
  - o original registra as duas com a `UClass` pai **`NCWnd`**; o `DECLARE_CLASS` delas no
    original dizia `NCWnd`;
  - logo, no original o `Super::` dessas classes é `NCWnd` e `IsA(NCNPCHtmlViewer)` é falso para
    um `NCTestHtmlViewer`;
  - o nosso header manteve a base C++ como `Super`. O `IMPLEMENT_CLASS` gerado usa o pai do
    original (classes.json), não o `Super` do header.

## 4. Os inicializadores (os 542 `init` do banco)

**A tabela do CRT (`__xc_a`..`__xc_z`)** vai de `0x10106000` a `0x101065a8` e tem 362 entradas:

| Quantas | O que |
|---|---|
| 1 | `0x100c6285` `__security_init_cookie`: o original foi compilado com `/GS` |
| 178 | `Classe::PrivateStaticClass`: construtor nativo de `UClass` + `atexit(~UClass)` |
| 178 | `autoclassClasse = Classe::StaticClass()` |
| 2 | `GCoolTimeTextures`/`GCoolTimeEndTextures` (`0x101213d4`/`e0`): construtor de `TArray` + `atexit` |
| 2 | `GHtmlAlignStack`/`GHtmlVAlignStack` (`0x10126a9c`/`0x101255f8`): só o `atexit` (o `TArray` zerado não precisa de construtor) |

**Os destrutores registrados no `atexit`** rodam no `DLL_PROCESS_DETACH`:
- 178 `~UClass` em `0x100d5e70..0x100d69c0`;
- 4 `~TArray` em `0x100d6000`, `0x100d6010`, `0x100d6140` e `0x100d6150`.

Os "globais sem classe em `0x100d…`" são esses: 4 `TArray` e as funções de construir e destruir
deles. **Não existe outro global com inicialização dinâmica**; os demais são dados estáticos
(valor no arquivo) ou zerados.

**Nomes errados no banco** (a correção fica com o status, rodada 10).
- As 176 linhas `Classe::PrivateStaticClass`dynamic atexit destructor'` são, na verdade, os
  inicializadores do `autoclassClasse`. O `build_db.py` supõe o destrutor em inicializador +
  0x80.
- Os destrutores de verdade estão como `sub_100d5e70`…`sub_100d69c0`.
- Os `autoclass` de `NConsoleWnd` e `NL2Font` ficam em +0x90 e estão como
  `sub_100d1da0`/`sub_100d2080`.

**No standalone nenhum inicializador é escrito à mão.**
- Os 178 `PrivateStaticClass` definidos pelo `IMPLEMENT_CLASS` e os 4 `TArray` definidos como
  globais geram o mesmo par construtor + `atexit`.
- A ordem do original é a do link dos `.obj` (`NCButton` primeiro). Um arquivo único com os
  `IMPLEMENT_CLASS` na ordem dos inicializadores a reproduz.
- Como no original, tudo roda dentro do `DllMain` (loader lock).

## 5. Globais que hoje moram no original

**67 objetos do `.data`** são lidos ou escritos pelo código do jogo.
- Hoje **só o nosso código** os toca.
- Do que ainda roda do original, só os inicializadores (`GPackage`, os 4 `TArray`) e o `DllMain`
  (`GNWindowInstance`) usam algum deles.
- Não há global do original sem nome no nosso código.
- Não existe `GDesktop`: o desktop é um campo do console (`GConsole->GetDesktop()`).

**Estado (escrito em tempo de execução): vira `T GNome = valor;` no standalone.**

| Onde | Globais (VA) |
|---|---|
| `NWindow.h` | `GConsole` 0x10122894, `GCanvas` 0x10122898, `GTextAlignParser` 0x101228a4, `GViewportHWnd` 0x10122890, `GNWindowDebug` 0x10122888, `GNWindowInstance` 0x10124284 (o `hInstance` do `IMPLEMENT_PACKAGE`) |
| `NCWnd.h` | `GFocusEntry` 0x10123d54, `GShiftDown` 0x10123d50, `GNextFrameID` 0x1010a998 (**= 0x10100**) |
| `NCConsole.cpp` | `GNextWndNumber` 0x10123d58 |
| `NCScrollWnd.h`, `NCTabControl.h` | `GNextScrollCtrlID` 0x10109de8 (**= 0x1200**), `GNextTabGroupID` 0x1010a6a8 (**= 0xFA0000**) |
| `NCGDevice.h` | `GWrapText`, `GWrapLine` (`NWrapBuffer`), `GWrapLen`, `GWrapPos` (0x1011c4c0..0x1011e4c4) |
| `NCIME.h`, `NCChineseIME.h`, `NCTaiwanIME.h` | `GImmLockIMC`/`GImmUnlockIMC`/`GImmLockIMCC`/`GImmUnlockIMCC` (ponteiros para função, 0x1011e9f8..), `GIMEUnicodeOS`/`GIMEOSChecked`, `GChineseOSVersion`(+`Read`), `GChineseNT`(+`Checked`), `GChinese`/`GTaiwanGetReadingString` |
| `NCCoolTimeIcon.h` | `GCoolTimeTextures`, `GCoolTimeEndTextures` (`TArray<UTexture*>`) |
| `NHtmlLayout.h` | `GHtmlAlignStack`, `GHtmlVAlignStack` (`TArray<INT>`), `GHtmlLinkTarget`, `GHtmlLinkMsg` (`TCHAR[256]`, via `nw::Addr`) |
| `NCAutoLogWnd.h` | `GAutoLogID`, `GAutoLogPassword` (`TCHAR[64]`, via `nw::Addr`), `GAutoLogSlot` |
| `NCDebugWnd.h` | `GNumDebugCategories`, `GDebugCategories` (tabela com os estados) |
| `NConsoleWndWindows.cpp` | `GCursorLocked` 0x1012d7bc, `GL2DebugLog` 0x1012d7b8, `GIsL2AutoLogOn` 0x1012d7c0 |
| `NL2Font.h` | `GFontBytesRead` 0x1012f190 |

**Tabelas (só leitura): viram `static const` com o conteúdo copiado do original.**
- No `.data`:
  - `GKeypadTextures`, `GChatPrefix`, `GAutoLogMessages`, `GGradeNames` e `GTellPrefix`;
  - `GCommandSeparators` (`L" ,\t\n"`) e `GServerNames`;
  - `GRaceEventNames`, `GClassTypeEventNames` e `GSexEventNames`;
  - `GMSViewCharacters`, `GDefaultCharacters` e `GClassMagicType`;
  - do HTML: `GHtmlTagInfo`, `GHtmlTagIds`, `GHtmlAlignNames`, `GHtmlEntityNames`,
    `GHtmlEntityValues`, `GHtml*Mark` e `GBoardMarks`.
- No `.rdata`: `GExpTable`, `GGMExpTable` e `GLobbyExpTable`.
- Algumas o nosso código já copiou (`GDispatchedMsgs`, `GCursorIDs` do `NCVirtualWndMain`).

**Só do modo gancho (somem):** `GStdLogicErrorVtbl`, `GStdLengthErrorVtbl` e
`GStdLengthErrorThrowInfo` (`NStdLib.h`). O objeto de exceção usa a vtable e o `ThrowInfo` do
original para o código original pegar. No standalone é um `throw` nosso.

**Como converter.** No header:
```cpp
#if NW_HOOK_MODE
#define GFocusEntry NW_GLOBAL(NWndEntry*, 0x10123d54)
#else
extern NWndEntry* GFocusEntry;
#endif
```
E a definição, com o valor inicial do original, num `.cpp` compilado só no standalone.

**No modo gancho as globais continuam `NW_GLOBAL`.** Desligar um gancho no `NWindowDecomp.ini`
faz o código original voltar a rodar, e ele usa a cópia dele. Com uma definição nossa, os dois
lados divergiriam (por exemplo, dois `GNextFrameID`). Já as tabelas só de leitura podem virar
cópias nossas nos dois modos.

**Globais do Core/Engine:** `GL2Show*`, `GIsOnlyL2Console`, `GL2GameData` etc. são imports, não
mudam.

## 6. Andaimes do modo gancho

A lista item a item e como tirar cada um é do `docs/notas/Andaimes.md` (trabalhador menus). Aqui
fica o que importa para linkar.

**Contagem em `23ad825` (27/09/2026).** "Fora do modo gancho" = as linhas que a compilação
standalone vê, sem os blocos `#if NW_HOOK_MODE`, pela mesma conta do `standalone_report.py`.

| Andaime | Usos | Fora do modo gancho | Bloqueia o link standalone? |
|---|---|---|---|
| `NW_HOOK_*` | 2.744, 134 arquivos | 2.744 | não: vazios no standalone |
| `NW_CHECK_VTABLE*` | 184, 116 arquivos | 184 | não: fica só a declaração `nwvt_X` (a `NW_HOOK_VARARGS_SLOT` do `NConsoleWndWindows.cpp` a usa) |
| `NW_STOCK` | 5, em 2 arquivos | 0 | não mais: só no ramo gancho do `NStdLib.cpp`/`NStl.cpp` |
| `nw::StockCall` | 3, em 1 arquivo | 0 | não mais: só no ramo gancho do `NStdLib.h` |
| `NW_GLOBAL` / `nw::Addr` | 44 / 31 (+ as 178 do `NClasses.gen.h`) | 0 / 0 | não mais: todas têm o ramo `extern` e a definição no `NwGlobals.cpp` (status) |
| `NClass::X()` | 36 | 36 | não: no standalone devolve o `autoclassX` |
| `nw::Field` | 0 | 0 | — |
| `nw::VirtualCall` | 1 | 1 | não: é chamada por vtable. O `NwStandalone.h` tem de continuar definindo o `nw::VirtualCall` |

**O último `nw::VirtualCall`** é o `NConsoleWnd::vfn176` (`NConsoleWndNet.cpp`), slot 174 do
`NetHandler`. É de propósito:
- o original salta para o slot com o `Params` ainda na pilha;
- o `UNetworkHandler::RequestSkillCoolTime` do cliente, que ocupa esse slot, não recebe nem tira
  nada da pilha;
- a chamada pela declaração mudaria isso.

**Os `NW_STOCK` e `StockCall` que sobram** são do ramo gancho do `NStdLib`: o `_CxxThrowException`,
o `_Xlen` e o `_Xran` antigos e o `std::exception` do CRT do original. O ramo standalone tem os
seus (menus, rodada 10).

## 7. CRT

**O original.**
- VC 7.0 (13.00.9466) com o **CRT estático** (libcmt 7.0) dentro do próprio `NWindow.dll`.
- No banco: 481 funções `crt`, 62 thunks e 1.265 funclets de EH.
- Ele **não** importa o `MSVCR70.dll`; `L2.exe`, Core, Engine, WinDrv e Window importam.
- Ou seja, o NWindow já tinha um CRT só dele, separado do da engine. O standalone com o CRT
  estático do VS2022 (`/MT`, como o `NWindowDecomp.dll`) mantém esse arranjo.

**O que o código do jogo usa do CRT:**
- strings: `wcslen`, `wcscpy`, `wcscmp`, `_wcsicmp`, `wcsncpy`, `wcscat`, `wcsstr`, `iswctype`;
- conversões: `_wtol`, `_wtof`, `atol`, `wcstoul`, `_ftol`;
- formatação e memória: `swprintf` (a assinatura antiga), `memmove`, `malloc`/`free` (8 usos);
- arquivos: `fopen`, `_wfopen`, `fread`, `fclose` (o carregador de fontes);
- gerado pelo compilador: `__chkstk`, os iteradores de construção/destruição de vetor,
  `_CxxThrowException`;
- biblioteca C++: `std::exception` e `_Xlen`/`_Xran` do `<xstring>`;
- `atexit`.

**O modo gancho já roda o nosso código sobre o CRT do VS2022, função por função.** O standalone
só tira o que ainda é do CRT velho:
- os inicializadores e o `atexit`;
- o `_DllMainCRTStartup`;
- o cookie do `/GS`;
- as peças de biblioteca da seção 6.

**O que muda e o que conferir:**
- **Alocação.**
  - O `operator new`/`delete` global vai para o `GMalloc`: é o inline do `Core.h` no original e o
    `src/re/NwMemory.cpp` no nosso.
  - Os objetos atravessam módulos (a engine libera o que o NWindow alocou), então isso é
    obrigatório; o `NwMemory.cpp` entra igual no standalone.
  - `malloc`/`free` e `FILE*` ficam dentro do módulo. Os pares são internos: fonte e IME.
- **Exceções C++.**
  - O `guard`/`unguard` pega `TCHAR*` lançado pelo Core (MSVCR70). O casamento entre módulos é
    pelo nome do tipo (`.PAG`), que exige `/Zc:wchar_t-` (já usamos).
  - O frame handler do VS2022 aceita o número mágico `0x19930520` do VC 7.0, e vice-versa. O
    modo gancho já exercita isso em todo `guard` nosso.
  - No standalone o `length_error` do `std::string` passa a ser lançado com o nosso `ThrowInfo`,
    e o `std::exception` é nosso (sem `StockCall`).
- **`/EHa`.** O `catch(...)` do `unguard` pegava GPF no VC 7.0 (antes do VS2005 era assim com
  qualquer `/EH`). Sem `/EHa` o "History:" do log de crash some. Manter.
- **`swprintf`.**
  - A assinatura sem tamanho vem do `_CRT_NON_CONFORMING_SWPRINTFS` (já definido).
  - O `%s` numa string larga é largo nos dois CRTs (semântica Microsoft). Não definir
    `_CRT_STDIO_ISO_WIDE_SPECIFIERS`.
- **`%f`.**
  - O UCRT (VS2019 16.2+) arredonda a metade exata para o par: `"%.0f"` de 2,5 dá "2"; o CRT
    velho dá "3".
  - Isso afeta os percentuais `"%.2f"` já no modo gancho. Exemplo: a fome do pet 97/800 =
    12,125 dava "12.12"; o original mostra "12.13".
  - **Resolvido:** os dois alvos linkam o `legacy_stdio_float_rounding.obj`.
  - Conferido no `NWindowDecomp.dll` montado:
    - as opções de `printf` do módulo passaram de `0x24` para `0x4` (sem
      `_CRT_INTERNAL_PRINTF_STANDARD_ROUNDING`);
    - 12,125 → "12.13", 0,125 → "0.13", 2,5 → "3", igual ao `swprintf` do CRT do original
      chamado dentro do `NWindow.dll`.
- **Infinito e NaN.**
  - O CRT do original escreve `"%.2f"` de infinito como "1.#J", de -infinito e 0/0 como "-1.#J",
    e `"%f"` de infinito como "1.#INF00".
  - O UCRT escreve "inf", "-inf", "-nan(ind)".
  - Com o bit `_CRT_INTERNAL_PRINTF_LEGACY_MSVCRT_COMPATIBILITY` nas opções do módulo, o UCRT
    escreve exatamente as do original. Esse bit só muda isso e passa a aceitar os prefixos
    `F`/`N`.
  - **Resolvido:** o `src/re/NwCrtCompat.cpp`, compilado nos dois alvos, liga o bit.
    - O inicializador dele fica em `init_seg(lib)` e é o primeiro dos 2.923 inicializadores C++
      do módulo; os outros são as 2.738 + 184 inscrições de gancho e vtable, que não formatam
      nada.
    - No módulo montado as opções ficaram `0xc`, e a saída ficou "1.#J", "-1.#J", "-1.#J",
      "1.#INF00" e "12.13".
- **`_wtol`.**
  - O do original acumula sem conferir estouro: "2147483648" → -2147483648 e "9999999999" →
    1410065407.
  - O do UCRT satura em ±2147483647.
  - Os dois aceitam dígitos de largura total ("１２" → 12), espaços e sinal na frente.
  - Diferença visível: um número de 10 dígitos digitado na caixa de quantidade (`NCDialogBox`).
  - **Resolvido:** `NWtol`/`NAtol` (`include/nwindow/NCrt.h`) copiam o laço do original. As 26
    chamadas de `_wtol` e a de `atol` usam essas cópias.
  - Os espaços usam a tabela `iswctype` do VC 7.0 (`NIswctype`): a do UCRT tem U+0085 como
    espaço.
  - Comparado com as funções do CRT do original chamadas dentro do `NWindow.dll`:
    - `NWcharToDigit` × `_wchartodigit`, 65.536 caracteres: 0 diferenças;
    - `NIswctype` × `iswctype`, 10 máscaras × 65.536: 0 diferenças;
    - `NWtol` × `_wtol`, 131.072 cadeias + 15 casos de estouro/sinal: 0 diferenças;
    - `NAtol` × `atol`: 0 diferenças.
  - No `NWindowDecomp.dll` montado não há mais `_wtol`/`wcstol`/`atol` do UCRT. O `NWtol` ficou
    fora de linha e, chamado lá dentro, também dá 0 diferenças.
- **Conferidos iguais** (comparação exaustiva com o CRT do original): `_wcsicmp` (65.535 × 7
  pares) e `_itow`.
- **Não usados pelo nosso código:** `qsort`, `rand`, `time`/`localtime`/`wcsftime`,
  `towupper`/`towlower`, `_wcsnicmp`, `wcstod`.
- **`iswctype(_ALPHA|_DIGIT)` do `NHtmlParser::IsAlnum`:** a tabela do UCRT tem ª, µ, º como
  letras e a do original não. **Resolvido:** usa `NIswctype`.
- **Outras classificações de caractere:** o nosso código só usa `toupper`, duas vezes no
  `NHtmlParser`, sobre caracteres largos. Ele é igual ao do original de -1 a 0xFFFF. Não usa
  `isw*`, `towupper`/`towlower` nem os `appIs*` do UE2.
- **`/GS` e `x87`.** O original tinha `/GS`. O `/arch:IA32` mantém o x87; `_ftol` →
  `__ftol2` trunca igual.
- **Inicialização.** O `_DllMainCRTStartup` do VS2022 roda os inicializadores e depois o
  `DllMain`. Na saída roda o `atexit`. É a mesma ordem do original.
- **Estáticos locais thread-safe.** O VS2022 usa TLS para eles; sem problema no Windows 10. O
  `/Zc:threadSafeInit-` deixaria igual ao VC 7.0, se um dia for preciso.

## 8. Recursos e cabeçalho PE

- **Recursos.**
  - 7 cursores coloridos: `RT_CURSOR` 1–7, 2.220 bytes cada.
  - Estão em 7 grupos: `RT_GROUP_CURSOR` 101, 102, 106, 107, 127, 130 e 131, idioma coreano (1042).
  - O `NCVirtualWndMain::LoadCursors` os carrega com `GNWindowInstance` e os IDs de `GCursorIDs`.
  - Não há recurso de versão.
- **No standalone** o build extrai os cursores de `orig/NWindow.dll` (`orig/` não entra no git,
  então os cursores também não).
- **Cabeçalho PE do original:** `ImageBase 0x10000000`, sem ASLR (`DllCharacteristics 0`), GUI,
  SO 4.0. O standalone usa `/BASE:0x10000000 /DYNAMICBASE:NO`. Não é obrigatório e **não evita a
  relocação**: o `Core.dll` carrega antes e fica em `0x10100000` (0x1DD000 bytes), dentro da faixa
  `0x10000000`–`0x10175000` do NWindow. O loader reloca o `NWindow.dll` original e reloca o nosso
  igual. O que a base mantém comparável são os endereços do arquivo (o nosso mapa/pdb contra a
  imagem original), não os de um crash no jogo.

## 9. Ordem de trabalho proposta

1. **Esqueleto** (feito na rodada 11): alvo `NWindowStandalone` que compila tudo com
   `NW_HOOK_MODE=0` e tem:
   - o `IMPLEMENT_CLASS` das 178 classes (gerado);
   - o `IMPLEMENT_PACKAGE`, o `NwMemory.cpp` e os cursores.
   O link falha e a lista do que falta é a lista de trabalho (seção 10).
2. Tirar os `NW_STOCK`/`StockCall` restantes (Andaimes.md).
3. Globais: o ramo `#else` com `extern` nos headers e as definições com os valores do original.
   As tabelas podem ser geradas por uma ferramenta que lê o `.data` e segue os ponteiros de string.
4. Corrigir as flags do `NL2Font`.
5. Primeiro link completo: **feito**, no master `23ad825` (status: globais; menus: exceções do
   `std`; clan: flags do `NL2Font`).
6. Testar fora do jogo: **feito na rodada 12** (seção 11). O `NWindow.dll` nosso é carregado,
   registrado, e `ConsoleWnd`/`L2Font` são carregados e construídos pelo Core, como o Engine faz. A
   saída é igual à do original. Falta o teste no System de teste, no lugar do original +
   `NWindowDecomp.dll`. **Esse teste precisa da permissão do usuário.**
7. Verificação própria do standalone: **feito** (`tools/verify_standalone.py`, clan). Compara pelo
   mapa do link as vtables, as classes, os exports, os recursos e o cabeçalho PE com o original.

## 10. O esqueleto e o relatório (rodada 11)

**Como montar.** O alvo fica fora do `ALL`: um erro nele não para o build do modo gancho.
- O `cycle.py` o monta num passo próprio, depois das conferências do modo gancho.
- Montagem, `verify_standalone` e `difftest --standalone` bloqueiam (clan, rodada 11).
- O smoke (seção 11) também bloqueia desde o `683f28e` (coordenador, depois do primeiro ciclo verde).

```bat
python tools\standalone_report.py            :: monta o NWindowStandalone e lista o que falta
python tools\standalone_report.py --probe    :: também o link forçado: exports, recursos, mapa
python tools\standalone_report.py --md       :: a mesma lista em tabelas Markdown
cmake --build build --config Release --target NWindowStandalone
```

| Saída | Onde |
|---|---|
| `NWindow.dll` + `NWindow.map` (quando linkar) | `build/standalone/Release/` |
| sonda: `NWindow_probe.dll` + `.map` (`/FORCE:UNRESOLVED`) | `build/standalone_probe/Release/` |
| log do último build do relatório | `build/standalone_build.log` |

A sonda existe para conferir exports, recursos e mapa antes do primeiro link de verdade. **Nunca
carregar a sonda:** todo símbolo que falta aponta para o endereço 0.

**O que entra no alvo.**
- `src/nwindow/*.cpp` com `NW_HOOK_MODE=0`, compilados uma vez numa biblioteca de objetos
  (`nwd_standalone_objs`) que os dois links usam.
- `src/standalone/*.cpp` (glob: o `NwGlobals.cpp` do status entra sozinho quando existir).
- Do `src/re`, só o `NwMemory.cpp` (`new`/`delete` pelo `GMalloc`) e o `NwCrtCompat.cpp` (infinito
  e NaN como o VC 7.0). Nada do instalador de ganchos nem os prólogos.
- O `legacy_stdio_float_rounding.obj`, como no modo gancho.

**`src/standalone`.**

| Arquivo | O que é |
|---|---|
| `NwStandalone.h` | os macros do modo gancho para `NW_HOOK_MODE=0`: `NW_HOOK_*` vazios; `NW_CHECK_VTABLE` só com o `nwvt_X`; `NW_STOCK`/`NW_GLOBAL`/`nw::Addr`/`nw::StockCall` declarados e nunca definidos, para o link nomear cada um; a `UClass` opaca (0x520) com o construtor nativo do Core |
| `NWindowPackage.cpp` | `NW_IMPLEMENT_PACKAGE(NWindow)`: `GPackage = L"NWindow"`, `GNWindowInstance` e o `DllMain` do original (`0x10024080`: guarda o `hInstance`, devolve 1) |
| `Classes.gen.cpp` | os 178 `NW_IMPLEMENT_CLASS`, na ordem dos inicializadores do original (`NCButton` primeiro) |
| `NWindow.def` | os 180 exports com os ordinais do original: `_DllMain@12` @1, `GPackage` @2, os `autoclass` @3–@180 |

**`tools/gen_implement_class.py`** gera o `Classes.gen.cpp` e o `NWindow.def` a partir do
`classes.json`.
- O pai e as flags de classe vêm do registro do original, não do header: `NCTestHtmlViewer` e
  `NCTradeItemWnd` ficam sob `NCWnd`, e o `NL2Font` tem `CLASS_Config`.
- Com o `orig/NWindow.dll` presente, confere também o que o `classes.json` não guarda. Relê os
  178 inicializadores e exige:
  - `EC_NativeConstructor` = 0;
  - o nome `TEXT("NClasse")+1`;
  - o pacote = o `GPackage` exportado;
  - o config = o retorno do `UObject::StaticConfigName()`;
  - o `UObject::StaticConstructor`.
  Exige ainda que a tabela de exports do original seja a do `.def`. Tudo confere.
- `--check` só compara.

**Os cursores.** O `tools/extract_cursors.py` roda no build.
- Escreve os 7 `.cur` e um `.rc` em `build/standalone_res/`.
- No `.rc` os cursores vão na ordem dos `RT_CURSOR` do original (107, 102, 101, 106, 127, 130,
  131): o `rc` os numera 1–7 nessa ordem.

**Decisões.**
- **`NW_IMPLEMENT_CLASS`** (`NWindow.h`) é o `IMPLEMENT_CLASS` do UE2 com o pai e as flags como
  argumentos.
  - O `autoclassX` sai pelo `.def`, não por `DLL_EXPORT`: os ordinais ficam explícitos.
  - O `autoclassX` vira inicialização constante (no original era um inicializador de 11 bytes).
  - O `.CRT$XCU` tem 178 entradas, uma por `PrivateStaticClass`.
  - O inicializador de `NCButton` empilha os mesmos argumentos que o do original, depois chama o
    `atexit(~UClass)`.
- **`NW_DECLARE_CLASS`, ramo standalone:**
  - `PrivateStaticClass` privado e `StaticClass()` devolvendo o nosso;
  - `InternalConstructor` como no UE2 (seção 3).
- **`NW_DECLARE_ABSTRACT_CLASS`** para `NCCmdTarget` e `NCAbstractMain`. No modo gancho é só um
  nome para `NW_DECLARE_CLASS` com `CLASS_Abstract`.
- **`NClass::X()`** (`NClasses.gen.h`, `tools/gen_class_ids.py`): no standalone devolve o
  `autoclassX`.
- **Engine:**
  - `UE2Core.h` ganhou `UObject::StaticConstructor`;
  - `UE2Engine.h` ganhou `UL2ConsoleWnd::StaticClass()` (o pai do `NConsoleWnd`). O `UL2Font` já
    tinha.
- **Cabeçalho PE:**
  - `/BASE:0x10000000 /DYNAMICBASE:NO /NXCOMPAT:NO /MANIFEST:NO`: `DllCharacteristics` 0 e nenhum
    recurso além dos cursores, como no original;
  - `/MAP /MAPINFO:EXPORTS` para o verificador do clan (`tools/verify_standalone.py`).
- **Transitório:** 9 globais que o `NWindow.h` e o `NCWnd.h` só declaram no modo gancho.
  - São `GConsole`, `GCanvas`, `GTextAlignParser`, `GNWindowDebug`, `GViewportHWnd`,
    `GNWindowInstance`, `GFocusEntry`, `GShiftDown` e `GNextFrameID`.
  - Estão declaradas como `extern` no `NwStandalone.h`. Saem de lá quando esses headers tiverem o
    ramo standalone (status).
  - O `GNWindowInstance` é definido pelo `IMPLEMENT_PACKAGE`, como o `hInstance` do UE2. O
    `NwGlobals.cpp` não deve defini-lo.

**Primeiro link (27/09/2026, sobre `eb11e6a` + rodada 11 do lobby).**
- Compila: 138 objetos, 0 avisos.
- **49 símbolos sem definição.** Nenhum do CRT; nenhum import do Core/Engine (todas as
  declarações batem com as `.lib`).

| Grupo | Símbolos | Sites | De quem |
|---|---|---|---|
| `NW_STOCK` | 3 (`0x100bda6f` `_CxxThrowException`, `0x100bd553` `_Xlen`, `0x100bd4fb` `_Xran`) | `NStdLib.cpp`, `NStl.cpp` | menus |
| `nw::StockCall` | 2 assinaturas | 3, o `std::exception` do `NStdLib.h` | menus |
| `NW_GLOBAL` | 35 `StockData<va>` + 8 declaradas sem definição | uma linha cada | status |
| `nw::Addr` | 1 | 30 (tabelas e `TCHAR[]`, seção 5) | status |

- A sonda linka com essas 49 em branco e mostra que o resto já é igual ao original:
  - exports: 180, mesmos nomes e ordinais;
  - recursos: os 14 (7 cursores + 7 grupos) byte a byte iguais;
  - `ImageBase` `0x10000000`, `DllCharacteristics` 0;
  - imports: os mesmos do Core, do Engine, do USER32, do IMM32 e do VERSION, menos dois do Core
    (149 de 151):
    - `GIsSSE`, do `appMemcpy` fora de linha do original;
    - `??_7FExec@@6B@`: só o construtor do `NL2Font` do original (`0x1005b7a0`) grava a vtable
      intermediária do `FExec`; o nosso não grava.
  - KERNEL32: 73, os do CRT novo.
- A lista item a item é a saída do `standalone_report.py`.

**Ensaio de merge (27/09/2026, branch descartada).** Juntei nesta branch as `agent/status`
(`a1c43f0`) e `agent/menus` (`03f13d8`) só para testar:
- os merges não dão conflito;
- **o link fecha:** `build/standalone/Release/NWindow.dll`, 1.267.712 bytes (o original tem
  1.277.972), 0 avisos;
- exports e recursos iguais aos do original;
- o `.CRT$XCU` tem 182 inicializadores (178 `UClass` + os 4 `TArray`), como previsto na seção 4;
- o `python tools/gen_globals.py --check` do status leu as 69 globais do DLL montado pelo mapa:
  0 diferenças.

Só com a `agent/status` faltavam 6 símbolos, todos do `NStdLib` (os `NW_STOCK`, os `StockCall` e as
3 `nw::Addr` do `std::exception`), que a `agent/menus` tira. O DLL do ensaio não foi carregado.

## 11. Carregar fora do jogo (rodada 12)

**Como rodar.** O `cycle.py` roda isto sozinho, depois do `verify_standalone` e do `difftest
--standalone`, e para o ciclo se falhar.

```bat
python tools\smoke_standalone.py          :: o nosso e o original, lado a lado
python tools\smoke_standalone.py --show   :: com a saída inteira do nosso
```

| System (em `build/`) | `NWindow.dll` |
|---|---|
| `smoke_standalone` | o nosso standalone (+ `.map`/`.pdb`) |
| `smoke_standalone_stock` | o `orig/NWindow.dll` sem remendo: o controle |

- Os dois têm as DLLs da engine do cliente de teste e nenhuma `NWindowDecomp.dll`.
- Quem instala é o `tools/install_test.py --system <pasta> --standalone [DLL]`. Ele **recusa
  qualquer pasta fora de `build/`**: o teste no jogo precisa da permissão do usuário.
- Fase 6: o `NWindowL2` passa pelo mesmo smoke com `--l2`, num System próprio (`build/smoke_l2`,
  mais `build/smoke_l2_patches` para os remendos no WinDrv/Network/Engine), com as classes de
  `tests/standalone/l2_extras.csv`. Ver `docs/notas/F6_Infra.md`. O fiel nunca recebe essa opção.

**O que o `nwd_smoke --standalone` faz** (`tools/smoke/nwd_smoke_standalone.cpp`). Segue o caminho
da engine com as funções do próprio Core, um estágio por vez, cada um sob SEH:

| Estágio | O que roda | O que prova |
|---|---|---|
| `bind` | `Core.dll` primeiro, como o `L2.exe`; depois o `appGetDllHandle("NWindow.dll")` do Core (o que o `UPackage::Bind` usa) | a DLL e as dependências carregam; o `Engine.dll` vem junto e a `NWindowDecomp.dll` não |
| `dllmain` | lê o `GNWindowInstance` (endereço pelo mapa) | o `DllMain` rodou e guardou o módulo |
| `exports` | `appGetDllExport` do Core para cada um dos 180 nomes | todos resolvem, na ordem de ordinais do original; `GPackage = "NWindow"` |
| `queue` | percorre o `UObject::GAutoRegister` | as 178 `UClass` estão na fila, cada uma com o nome `TEXT("NClasse")+1`. O conteúdo delas é o caso `classes` do difftest, não repetido aqui |
| `init` | `FName::StaticInit` e `UObject::StaticInit`, que roda o `ProcessRegistrants` do Core, do Engine e nosso; depois o `GSys` (`USystem`) | o registro inteiro sem erro, e a fila esvazia |
| `registered` | nome, pacote, pai, `GetPropertiesSize()` e flags de cada classe registrada | as 178 batem com o `classes.json` |
| `load` | `StaticLoadClass(UL2ConsoleWnd, NULL, "ini:Engine.Engine.L2Console", NULL, LOAD_NoFail, NULL)` e o do `L2Font` com flags 0, com os argumentos exatos do Engine (`UGameEngine::Init`, `UCanvas::Init`) | o Core resolve a chave do ini e acha as nossas classes |
| `construct` | `StaticConstructObject(classe, GetTransientPackage(), NAME_None, 0, NULL, GError, NULL)`, como o Engine faz logo depois | o nosso `InternalConstructor` e os construtores do `NL2Font` e do `NConsoleWnd` rodam; os objetos saem com a nossa vtable |

**O que ocupa o lugar do `appInit` (que o `L2.exe` chama):**
- `GMalloc` sobre o heap do processo;
- `GLog`, `GError` e `GWarn` que escrevem na saída. Um erro fatal do Core encerra o estágio;
- um `GConfig` que só sabe as duas chaves do `L2.ini` (`[Engine.Engine] L2Console=NWindow.ConsoleWnd`
  e `L2Font=NWindow.L2Font`) e anota toda pergunta;
- um `GSys` sem pastas de busca. Sem ele, o `StaticLoadObject` procura o arquivo do pacote
  (`appFindPackageFile`) e cai lendo `GSys->Paths`; o original cai igual.

A saída não tem endereço. O `smoke_standalone.py` roda o nosso e o original e compara as duas saídas
inteiras, log do Core incluído.

**`/Ob1` nos objetos do standalone** (decisão do coordenador, rodada 12).
- Só é expandida em linha a função marcada `inline` ou definida na classe.
- O original deixou fora de linha funções pequenas que o `/Ob2` junta aos chamadores: o
  `NString::AllocBuffer`, o `NStdString::Assign`, o `NStlWString::Copy`, o
  `NStlWordVector::PushBack`, o `NHtmlItem::ParseEditType`… Isso indica pouca expansão em linha no
  build do original.
- Assim o `difftest --standalone` acha essas funções pelo nome.
- O alvo do modo gancho continua com `/Ob2`.

No ciclo, depois disso (master `0cab5d0` + rodada 12):
- modo gancho igual: 2744 ganchos, verificador 2549 + 195 explicadas, 0 sem explicação,
  `regcheck` limpo, difftest 11 iguais;
- standalone:
  - 1.253.888 bytes;
  - `verify_standalone` OK: pe 12, recursos 7, exports 184, classes 179, vtables 186 + 1
    explicada;
  - `difftest --standalone`: 10 iguais, 0 diferentes e 1 pulado. É o `vectors`: o
    `NStlWordVector::PushBack` (`0x1002f470`) continua dentro do chamador mesmo com `/Ob1`;
  - smoke: os 9 estágios, a mesma saída do original.

**Resultado (27/09/2026, master `23ad825` + rodada 12):**
- os 9 estágios passam com o nosso DLL e com o original;
- as duas saídas são iguais, 306 linhas;
- o log inclui as perguntas ao `GConfig`:
  - no registro, as das classes de configuração do Engine;
  - na construção, as 8 chaves `[LobbySound]` do `ALAudio.int` que o construtor do `NConsoleWnd`
    lê;
- as 178 classes registradas batem com o `classes.json`;
- nenhuma exceção no caminho.
- **Os dois DLLs são relocados:** nem o nosso nem o original ficam na base preferida, porque o
  `Core.dll` ocupa `0x10100000` (seção 8).

**Onde o carregamento fora do jogo tem de parar.** Depois de construir, o Engine continua assim:
- no `UGameEngine::Init`: `Console->Init(Engine)` (slot 236), `LoadDefaultCharacterInfo` (244) e
  `PlayIntroMusic` (255). O `Init` do console precisa do `UGameEngine` que está sendo montado:
  client, viewport, áudio, `NetHandler`;
- no `UCanvas::Init(Viewport)`: lê as chaves da fonte do país no `GConfig` e chama o `Load` do
  `NL2Font` (slot 25) com uma textura. A textura vem de pacote lido do disco, pelo `GFileManager` e
  pelas `GSys->Paths` de um `appInit` de verdade.

As duas coisas são o jogo. O smoke para depois de construir.

**Saída do processo.**
- O smoke termina com `TerminateProcess`, sem desligar as DLLs.
- Com as classes registradas e o `GObjInitialized` ainda em 1, o `atexit` de cada `~UClass`
  (Core, Engine e nosso) chega ao `UObject::~UObject` e para em "Other object in slot". O original
  faz igual.
- No jogo o `appPreExit` roda o `UObject::StaticExit` antes. A saída limpa do jogo é uma das coisas
  para olhar no teste no jogo.

**Antes do teste no jogo:**
1. As exceções do `std`: o caso `stdlib` do `difftest --standalone` caía. Com o master `0cab5d0`
   (clan: os tipos comparados pelos nomes do original) e o `/Ob1`, ele passa. O caminho do smoke
   não lança exceção nenhuma.
2. A permissão do usuário. O `install_test.py --standalone` recusa, de propósito, o System do jogo.
   A instalação lá precisa de um passo deliberado: trocar o `NWindow.dll` pelo nosso, tirar a
   `NWindowDecomp.dll` e guardar como voltar.
3. O que só o jogo exercita:
   - o `Init` do console e as ~45 janelas;
   - fontes e texturas (`NL2Font::Load`);
   - IME;
   - os callbacks da rede;
   - a saída.
