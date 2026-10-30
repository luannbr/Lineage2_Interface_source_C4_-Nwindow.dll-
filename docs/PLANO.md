# Plano e método

Este plano é o do projeto do C1 (herdado pelo C2), apontado para o `NWindow.dll` do C3 (Rise of Darkness). O que é
específico do C3 — constantes da imagem, decisões, o que foi retargetado, o que falta — está em
**[PLANO_C3.md](PLANO_C3.md)**; o do C2 ficou em [PLANO_C2.md](PLANO_C2.md).

## O binário

| Fato | Valor |
|---|---|
| Arquivo | `NWindow.dll` stock C3, 2.732.052 bytes, md5 `f0362880…`, TimeDateStamp `0x4329071F` (15/09/2005) |
| Compilador | **Visual C++ .NET 2002 (7.0, cl 13.00.9466)**, CRT estático (libcmt 7.0), linker 7.0 (Rich header: o mesmo do C1 e do C2) |
| Código | `.text` 0x1738E0 bytes (0x10001000–0x101748E0); ImageBase `0x10000000`, sem ASLR |
| Exports | 311: `GPackage`, `_DllMain@12`, 309 `autoclass*` |
| Imports | 530: Core.dll 198, Engine.dll 115, KERNEL32 113, ADVAPI32 30, USER32 17, IMM32 16, WS2_32 13, WININET 8, `npkcrypt.dll` 8 (por ordinal), VERSION 6, SHLWAPI 4, GDI32 1, WINMM 1 |
| RTTI | não (compilado com /GR-) |
| Funções | 10.992 no Ghidra: 5.607 do jogo, 939 inicializadores estáticos, 3.843 funclets de EH, 518 do CRT, 84 thunks, 1 fragmento |
| A decompilar | **6.546 funções, 1.312.496 bytes** (jogo + inicializadores) |

De onde vêm os nomes (nenhum PDB existe):
- **309 classes** com tamanho exato, classe-pai, construtor e vtable: dos inicializadores do
  `IMPLEMENT_CLASS` (`tools/extract_classes.py` → `analysis/classes.md`);
- **718 funções** com o nome original: as strings dos macros `guard()` do UE2, achadas pelas
  tabelas de exceção (`tools/eh_scan.py`);
- slots de vtable: o nome dado a um slot (ex.: 55 = `OnPaint`) vale para todas as classes filhas;
- as vtables da engine (`UCanvas`, `UObject`…) com assinatura completa, lidas do próprio
  `Engine.dll`/`Core.dll` (`tools/engine_vtables.py`);
- **o projeto do C2** (a base): cada função do C3 que o `tools/port/match.py` casou com uma do C2 herda o nome
  (`db/fmap.csv`), com a menor prioridade de todas (o `tools/build_db.py` grava a origem de cada nome na
  coluna `name_src`).

## Método: modo hook, função por função

Mesmo caminho de projetos como re3 e OpenRCT2: o executável original continua rodando e cada função
reimplementada o substitui no lugar, até não sobrar nada do original.

1. **Escolher** funções `todo` em `db/functions.csv` (de preferência uma classe inteira, das
   folhas para a base ou das mais usadas).
2. **Ler** `analysis/ghidra/decomp/<va>.c` e **conferir no assembly** `analysis/ghidra/asm/<va>.asm`:
   o decompilador do Ghidra erra número de parâmetros (conferir o `ret N`), confunde
   parâmetros com endereço de retorno e não sabe os slots de vtable.
3. **Escrever** em C++ no estilo UE2 em `src/nwindow/<Classe>.cpp`, com o endereço em comentário e o
   `NW_HOOK_*` logo abaixo de cada função. Layout de classe com `NW_ASSERT_OFFSET/SIZE`.
4. **`python tools/cycle.py`**: compila, instala, carrega fora do jogo (todo hook tem de dar `ok`),
   e o `verify_hooks.py` compara com o original: bytes desempilhados no `ret`, slots de vtable
   chamados e imports chamados. Toda diferença tem de ser entendida; as que não são bug vão
   para `db/verify_notes.csv` com a explicação.
5. **Testar no jogo** com o System de teste; passando, registrar em `db/verified.csv`.
6. Um bug no jogo? Desligar hooks no `NWindowDecomp.ini` (`Disable=NCGDevice::*`, um endereço,
   um método) até isolar a função.

O que o verificador já pegou na primeira classe (no C1): o MSVC põe **sobrecargas virtuais em ordem
inversa** na vtable. `UCanvas::DrawStretchedTex(5 args)` está no slot 35 e a de 9 args no 34;
declaradas na ordem natural, o nosso código chamava o slot errado.

## Fases

| Fase | Conteúdo | Estado no C2 |
|---|---|---|
| 0 Fundação | pasta, análise, banco, build, hooks, instalador, smoke test, verificador | **feito (06/10/2026)**, com a source de jogo vazia |
| 1 Base da UI | `NCObject`, `NCGDevice`; `NCCmdTarget`, `NCWnd` (0x98, 67 slots), `NControl` (0x1A4, 68 slots), `NCVirtualWndMain` (despacho de mensagens), `NL2Font`, `NHtmlAnalyzer` | a source vem do `tools/port` (C1 → C2) |
| 2 Controles | `NCButton`, `NCEditBox`, `NCListBox`, `NCScrollWnd`, `NCComboBox`, `NCTabControl`, `NCSliderBar`, `NCProgressCtrl`, `NCListCtrl`, `NCMultiEdit`, `NCHtmlViewer`… e os novos (`NCTree`, `NCTreeCtrl`, `NCSliderCtrl`, `NCThumbButton`…) | |
| 3 Janelas | ~110 classes `NC*Wnd` do C1 + as 47 novas do C2 (receitas, macros, henna, manor, MSN, GM snoop…) | |
| 4 Console e sistemas | `NConsoleWnd` (0x35B8 bytes, 335 slots: os callbacks de rede que o Engine chama), IME (5 variantes, com a tailandesa), HTML | |
| 5 Standalone | `IMPLEMENT_CLASS` das 225 classes, globais e inicializadores próprios, CRT moderno, o import por ordinal da `npkcrypt.dll`; a source gera `NWindow.dll` sozinha e substitui o original | pendências em [PLANO_C2.md](PLANO_C2.md) |
| 6 Evolução | features novas (a camada do C1 — hover de item, combat log, craft — não vale no C2 como está) | fora do escopo por ora |

Na fase 5 não pode sobrar nenhum `NW_STOCK`, `nw::StockCall` ou `nw::Field` — são andaimes do
modo hook para falar com código ainda não decompilado.

## Pendências de ferramenta

- Renomear no projeto Ghidra (`ghidra/census/project`, `tools/ghidra/run_names_export.sh`) a partir do
  `db/functions.csv` e reexportar: o decompilador melhora muito com nomes e tipos.
- Compilação *matching* (byte a byte) exige o VC 7.0 (13.00.9466), que não está instalado. Por isso
  o código decompilado evita C++ moderno: dá para tentar mais tarde.
