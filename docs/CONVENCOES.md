# Convenções

## Código decompilado (`include/nwindow`, `src/nwindow`)

- **Uma classe por arquivo**, com o nome da classe C++ (`NCGDevice.h/.cpp`). Funções na ordem do
  original quando possível.
- **Estilo UE2**: `INT`, `DWORD`, `TCHAR`, `UBOOL`, `TEXT("…")`, `guard/unguard` onde o original
  tem (o nome no `guard` é o da string original), `appMalloc/appFree`, `FString`, `TArray`.
- **C++ simples**: nada de STL, lambdas, `auto` ou templates novos no código do jogo. Mantém a
  porta aberta para uma compilação com o VC 7.0 e deixa o código parecido com o original.
- **Endereço em tudo**: declaração com `// 0x1000cc40`, definição com o `NW_HOOK_*` logo abaixo.
- **Nomes**: os que vêm de string `guard`, export ou vtable da engine são originais. Os que
  inventamos levam `(name ours)` no comentário (e `(nome nosso)` no `db/names.csv`) até algo confirmar.
- **Layout**: todo campo com offset conhecido tem `NW_ASSERT_OFFSET`, toda classe `NW_ASSERT_SIZE`
  com o tamanho do `IMPLEMENT_CLASS` (`analysis/classes.md`). Campo desconhecido: `UnknownXX`
  com o offset no nome.
- **Fiel antes de bonito**: reproduzir o comportamento exato, inclusive o esquisito (ex.:
  `NCGDevice::DrawText` passa `X` como 5º argumento do `GetTextExtent` — está no original).
  Correções e melhorias vêm depois, como mudança separada e anotada.

## Macros do modo hook (`src/re/NwHook.h`)

| Macro | Para |
|---|---|
| `NW_HOOK_METHOD(va, Classe, Metodo)` | método virtual ou não |
| `NW_HOOK_METHOD_SIG(va, Classe, Metodo, Sig)` | método sobrecarregado |
| `NW_HOOK_VARARGS(va, Classe, Metodo, Sig)` | método `...` (cdecl, não virtual) |
| `NW_HOOK_CTOR(va, Classe)` / `NW_HOOK_DTOR(va, Classe)` | construtor padrão / destrutor |
| `NW_HOOK_FUNC(va, Funcao)` | função livre ou estática |
| `NW_NAKED ... NW_STOCK(va)` | declarar e usar uma função ainda não decompilada |
| `nw::StockCall<R>(va, obj, args…)` | chamar método de classe ainda não declarada |
| `nw::Field<T>(obj, offset)` | campo de objeto de classe ainda não declarada |
| `NW_GLOBAL(Tipo, va)` | global do original |

Modo prova (`[Debug] Marker=1` no `NWindowDecomp.ini`): marcas que só o nosso código desenha
(linha amarela no topo da tela, `#` na senha, cursor vermelho), para conferir no jogo que os ganchos
estão ativos. Os pontos no código decompilado usam `NW_DEBUG_MARKER()` (`re/NwDebug.h`, 0 fora do
modo hook) e levam o comentário "proof mode, not stock".

Nunca fazer hook de `scalar deleting destructor` (o original chama o destrutor, que já é nosso).

## Armadilhas já conhecidas

- **Sobrecargas virtuais**: o MSVC as agrupa em ordem **inversa** de declaração na vtable.
  Declarar a do slot maior primeiro (ver `UCanvas` em `include/ue2/UE2Engine.h`).
- **`TCHAR`** é `unsigned short` (`/Zc:wchar_t-`): é o que faz os nomes decorados baterem com
  `Core.dll` (`PBG`, `PAG`).
- **`/EHa`**: sob o VC 7.0 o `catch(...)` do `unguard` pegava GPF; é o que monta o "History:" do log
  de crash. **`/arch:IA32`**: float em x87 como o original.
- **Inteiro × constante float / inteiro** (barras e percentuais): o original faz `fild; fmul
  [const]; fidiv; _ftol` com os inteiros exatos no x87. O nosso compilador (`/fp:precise`)
  arredonda para float cada conversão int→float (`fild; fstp dword; fld`), o que muda o
  resultado acima de 2^24 (exp dos níveis 61+). Escrever essas contas em `DOUBLE`
  (`(DOUBLE)(Exp - Base) * 189.0 / (Next - Base)`): int32 em double é exato.
- **Código dobrado pelo linker (ICF)**: funções idênticas têm um endereço só
  (`NCObject::NCObject` = `NCCmdTarget::NCCmdTarget` = `NCAbstractMain::NCAbstractMain`). O hook
  vale para todas; a coluna `notes` do banco mostra os casos.
- **Registradores entre chamadas (otimização de programa inteiro)**: o original foi compilado
  com `/GL`; um chamador pode continuar usando `ecx`/`edx`/`eax` depois de chamar direto uma
  função que (ele sabe) não os altera. A nossa versão segue a convenção padrão e pode alterá-los,
  então todo chamador do original que ainda roda não pode depender disso. O
  `tools/regcheck.py` (roda no `cycle.py`) aponta esses casos: fazer o gancho dos chamadores
  junto. `--all` lista os pares que só estão seguros porque os dois lados são nossos (desligar
  um só no `.ini` quebra); `--ini <arquivo>` confere uma lista `Disable=`.
- **Declarações da engine**: o que é importado (`CORE_API`/`ENGINE_API`) tem de gerar exatamente o
  nome decorado exportado — o link falha se não gerar. A ordem das virtuais vem de
  `python tools/engine_vtables.py <Classe>`.
- **CRT (`printf`)**: o original usa o CRT estático do VC 7.0; o nosso usa o UCRT do VS2022.
  - O UCRT arredonda a metade exata para o par: `"%.2f"` de 12,125 dava "12.12". O original dá
    "12.13" (a fome do pet, 97/800).
  - Os dois alvos linkam o `legacy_stdio_float_rounding.obj` (`NWD_CRT_COMPAT_OBJS` no
    `CMakeLists.txt`), que devolve o arredondamento antigo. Não tirar.
  - Infinito e NaN: o original escreve `"%.2f"` de infinito como "1.#J" e `"%f"` como "1.#INF00";
    o UCRT escreve "inf"/"nan".
  - O `src/re/NwCrtCompat.cpp` (compilado nos dois alvos) liga nas opções de `printf` do módulo
    o bit `_CRT_INTERNAL_PRINTF_LEGACY_MSVCRT_COMPATIBILITY`, que devolve o formato antigo. O
    inicializador roda antes de qualquer outro nosso (`init_seg(lib)`). Não tirar.
- **CRT (`_wtol`/`atol`)**: o `_wtol` do VC 7.0 soma os dígitos sem conferir estouro
  ("9999999999" → 1410065407); o do UCRT satura em 2147483647.
  - Usar sempre `NWtol`/`NAtol` (`include/nwindow/NCrt.h`, cópias do laço do original, com os
    mesmos dígitos Unicode do `_wchartodigit`).
  - Nunca usar `_wtol`/`_wtoi`/`_ttoi`/`atol`/`atoi` do UCRT.
  - `iswctype` abaixo de 256: a tabela do VC 7.0 difere da do UCRT em 0x85 (espaço) e em ª, µ, º
    (letra). `NIswctype` usa a do original.
  - Usar `NIswctype` no lugar de `iswctype`/`isw*` (o tokenizador HTML já usa).
  - Conferidos iguais nos dois CRTs, caractere por caractere: `_wcsicmp`, `_itow`, `toupper`
    (de -1 a 0xFFFF), `isspace` estreito e os dígitos de largura total.

## Banco (`db/`)

| Arquivo | Quem escreve |
|---|---|
| `functions.csv` | gerado por `tools/build_db.py` (não editar); a coluna `name_src` diz de onde veio o nome (`manual`, `source`, `guard`, `export`, `class`, `dtor`, `vslot`, `ghidra`, `fmap:A/B/C/D`, `fmap-slot`, `vfn`) |
| `names.csv` | à mão: nome de função do original que ainda não reimplementamos |
| `vslots.csv` | à mão: nome de slot virtual (`classe,slot,nome`) — herdado pelas filhas; os números são os do C2 (`vslots_c1.csv` + `vslots.json` → `tools/c2/remap_vslots.py`) |
| `verify_notes.csv` | à mão: diferença do `verify_hooks.py` explicada |
| `verified.csv` | à mão: função testada no jogo |
| `morto.csv` | à mão: função que nenhum caminho do jogo alcança no modo gancho (status `morto`, com o motivo); o `build_db.py` confere os chamadores |
| `extra_functions.csv`, `not_functions.csv` | à mão: entradas de função que o Ghidra perdeu / fragmentos que ele tomou por função |
| `fmap.csv`, `amap.csv`, `layout.json`, `vslots.json`, `eligible.csv` | gerados pelo `tools/port` (C1 → C2): o mapa de funções, de slots e de layouts |
| `c1/` | as listas do C1 (endereços do C1), só para consulta |

Todos os `.csv` à mão do C2 usam endereços do C2.

## Documentação

Português nos `docs/`; comentários de código em inglês, como no resto do workspace.
