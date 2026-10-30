# NWindow do C2 a partir da nossa source do C1 — estudo de viabilidade

Pedido do dono em 30/09/2026: "se dá para aproveitar o nosso source do C1 para fazer um do C2 e pegar só o
que falta e o que atualizou dele para o nosso e fazer uma source dele". Só análise: **nenhum arquivo da
source foi mudado**. Binário estudado: `<C2 client>\system\NWindow.dll`.

**Conclusão: viável e bem mais barato que recomeçar, mas não é "copiar e ajustar".** O esqueleto do C1 (as
178 classes, a hierarquia, as ferramentas) serve inteiro. O código serve em cerca de dois terços, e todo
layout de classe e toda vtable mudam, então os cabeçalhos têm de ser regenerados. O trabalho realmente novo
é 14 % do código do C2, mais as 47 classes novas.

## Os dois binários

| | C1 (`orig/NWindow.dll`) | C2 |
|---|---|---|
| Compilação (PE) | 25/06/2004, VC++ 7.0, CRT estático | **03/03/2005**, o mesmo compilador e linker (o arquivo no disco é de 27/12/2022: só a cópia) |
| Tamanho / `.text` | 1.277.972 / 0xD59D0 (874.960) | 1.835.028 / 0x105020 (1.069.088), **+22 %** |
| Exports | 180 (178 classes) | 227 (**225 classes**) |
| Imports | 335 (Core 151, Engine 82) | 452 (Core 161, Engine 98, kernel32 +39, **advapi32 29, ws2_32 12, `npkcrypt.dll` 8**, winmm 1) |
| Funções (Ghidra com sementes) | 5.366 | 7.117, 94,4 % do `.text` coberto |

Os imports novos, pelos nomes (inferência, não li o código): `ws2_32` (`socket`, `connect`, `WSAAsyncSelect`…) é
o cliente de MSN embutido (`MSNSocket`, `NCMSNWnd`); `advapi32` traz o registro, o token de segurança do
usuário e a CryptoAPI (`CryptImportKey`, `CryptVerifySignature`, `CryptDecrypt`…); e **`npkcrypt.dll`**, importada
**por ordinal** (8 funções), é o nProtect KeyCrypt (proteção de teclado, par do `NCKeyCryptWarningDialogBox`). O
C1 não tem nada disso. O `npkcrypt.dll` está na pasta do cliente do C2; a source do C2 precisaria declará-lo
(um `.def`/import por ordinal) ou fazer o `LoadLibrary` dele.

**O NWindow do C2 não roda no Engine do C1.** Ele importa 10 funções do Engine que o do C1 não exporta
(`FL2GameData::GetItemData`, `GetHennaData`, `GetRecipeDataByIndex`, `GetRecipeDataBy2Condition`,
`GetItemAdditionalName`, `GetCmdStringByActionID`, `FParser::GetRemainStr`, `APawn::GetSocialAnimDuration`,
`UCanvas::MakeBMPToDXT1`, `MakeThaiCharArray`) e 5 do Core. Uma source do C2 tem alvo no cliente do C2
(que já está no disco, com o Engine, o Core e o resto dele).

## Classes (`tools/c2/extract_classes.py`: o mesmo extrator do C1, 225 classes lidas sem erro)

- **As 178 classes do C1 continuam todas no C2**, nenhuma mudou de pai, nenhuma foi removida.
- **47 classes novas.** Por assunto (os nomes vêm do export; `L2WordWrapperWnd` e `MSNSocket` aparecem como
  `N2…`/`NSNSocket` na ferramenta porque ela põe um `N` na frente):
  - **Receitas/craft (14):** `NCRecipeWnd`… `NCRecipeBookWnd`, `NCRecipeBookItemWnd`, `NCRecipeBuyListWnd`,
    `NCRecipeBuyManufactureWnd`, `NCRecipeManufactureWnd`, `NCRecipeManufactureItemWnd`, `NCRecipeShopWnd`,
    `NCRecipeShopItemWnd`, `NCRecipeListBox`, `NCRecipeTree`, `NCRecipeTreeCtrl`, `NCRecipeTreeWnd`. São as
    janelas nativas reais que o `F6_Craft.md` reconstruiu à mão, a partir da especificação, como "L2 extras".
  - **Macros (5):** `NCMacroListWnd`, `NCMacroEditWnd`, `NCMacroEditList`, `NCMacroInfoWnd`, `NCMacroArrayWnd`.
  - **Henna/tatuagem (3):** `NCHennaInfoWnd`, `NCHennaListWnd`, `NCHennaListBox`.
  - **Castelo/manor e mapa:** `NCManorShopWnd`, `NCManorShopItemWnd`, `NTownMapWnd`, `NCObserverWnd`.
  - **Controles novos:** `NCTree`, `NCTreeCtrl`, `NCTVItem`, `NCSliderCtrl`, `NCThumbButton`, `NCCalcWnd`,
    `NCCalcEditCtrl`, `NCDynamicLinker`, `L2WordWrapperWnd`.
  - **Avisos e diálogos:** `NCWarningDialogBox`, `NCGaraDialogBox`, `NCKeyCryptWarningDialogBox`,
    `NCAnnounceWnd`, `NCEulaWnd`, `NCCreditWnd`, `NCXMasSealWnd`, `NCEmpowerWnd`, `NCPetitionBeginWnd`.
  - **Outros:** `NCMSNWnd` + `MSNSocket` (MSN), `NCThaiIME` (IME tailandês), `NCGMSnoopWnd` + `NCGMSnoopManager`.
  - Soma de `sizeof` das novas: 106.208 bytes de objeto.
- **Layout: 174 das 178 classes mudaram de `sizeof` e 177 mudaram de número de slots de vtable.** Não é
  exceção, é tudo, porque as bases cresceram: `NCObject` 24→25 slots, `NCCmdTarget` 37→38, `NCWnd` 0x94→0x98
  bytes e 62→67 slots, `NControl` 0x1A0→0x1A4 e 63→68. Cada classe filha herda o deslocamento.
  `NConsoleWnd` foi de 285 para **335 slots** (+50 callbacks da rede: pacotes novos do C2) e de 0x3740 para
  0x35B8 bytes.
  - O deslocamento é regular e dá para automatizar: alinhando os slots do C1 com os do C2 pelo corpo da
    função, os slots novos aparecem em posições fixas (no `NCWnd`: 24, 25, 36, 37, 47, 48, 51–54). Uma
    tabela C1→C2 por base corrige o índice de todas as filhas.

## Funções (`tools/c2/compare_nwindow.py`)

As funções são comparadas depois de trocar todo endereço (chamadas, globais, strings, vtables) por um
símbolo: uma função recompilada igual casa mesmo com tudo ao redor movido. "Mesmo formato" ignora offsets de
campo, registradores e constantes (o que muda quando o layout da classe mexe); "parecida" é a razão do
`difflib` sobre as instruções do melhor candidato (método grosseiro: serve para ordem de grandeza).

**Do ponto de vista do C2 (o que a nossa source já tem para cada função dele), em bytes de código:**

| Categoria | Funções | Bytes | % |
|---|---:|---:|---:|
| Idênticas a uma do C1 | 1.126 | 61.454 | 6,1 % |
| Mesmo formato (só offsets/registradores mudam) | 1.455 | 203.059 | 20,1 % |
| ≥ 90 % parecidas | 420 | 205.113 | 20,3 % |
| 75–90 % parecidas | 400 | 186.926 | 18,5 % |
| 50–75 % parecidas | 281 | 98.881 | 9,8 % |
| **< 50 % ou sem par (novas)** | **403** | **138.483** | **13,7 %** |
| CRT, inicializadores estáticos, thunks | 1.183 | 85.394 | 8,5 % |
| Funclets de exceção (o compilador gera) | 1.849 | 30.283 | 3,0 % |

Então, dos 1.009.593 bytes do C2: 11,5 % o compilador gera sozinho; **46 %** é igual ou quase igual (≥ 90 %) ao
que temos; 18,5 % pede um ajuste moderado; **23,5 % (< 75 %) é reescrita**, e uns 14 % disso é código sem
equivalente no C1.

**Do ponto de vista do C1 (quanto do nosso código sobrevive no C2)**, 729.500 bytes de funções do jogo:
7,1 % idênticas, 27,0 % mesmo formato, 25,4 % ≥ 90 %, 19,1 % 75–90 %, **21,5 % < 75 % ou sumidas**.

**Por classe:** 64 das 178 classes têm ≥ 90 % dos bytes idênticos, do mesmo formato ou ≥ 90 % parecidos;
44 têm menos de 50 %. As que mais mudaram (`analysis/c2/class_delta.csv` tem todas):
- a família dos **itens** (`NCPrivateBuyItemWnd`, `NCPrivateShopItemWnd`, `NCShopItemWnd`, `NCInvenItemWnd`,
  `NCEquipItemWnd`, `NCTradeItemWnd`, `NCPetInvenItemWnd`, `NCDeliverItemWnd`, `NCStoreItemWnd`): 8 a 12 KB
  cada uma com mudança grande, e o `sizeof` encolheu 4 a 12 bytes (o C2 guarda o item de outro jeito);
- `NConsoleWnd` (9,6 KB de 76,8 KB reescritos, mais 50 slots de pacote), `NCShortCutWnd` (7,3 KB de 12 KB),
  `NCMultiEdit`, `NCPledgeWnd`/`NCGMPledgeWnd` (+1.356 bytes cada), `NCSiegeInfoMainWnd` (+772), `NCTrainInfoWnd` (+516).

## Como fazer (plano)

1. **Base (horas):** parametrizar as ferramentas para outro binário — hoje `tools/nwd/image.py` e as demais
   conferem o md5 do C1 (`STOCK_MD5`); a extração de classes e o censo de funções já rodam no C2
   (`tools/c2/`). Gerar `classes.json`, o banco de funções e as vtables do C2.
2. **Transferir nomes:** toda função do C2 que casa com uma do C1 (as ~46 % acima, mais as 75–90 % com
   revisão) herda o nome, a classe e o slot. O restante recebe nome pela vtable e pelos `guard()`, como no C1.
3. **Regenerar os cabeçalhos por classe** a partir do `classes.json` do C2 (tamanho, slots) e da tabela de
   deslocamento de slots; os campos vêm do C1 e são reposicionados onde os acessos das funções
   idênticas mostram que se moveram. É o trabalho mais chato: 174 classes.
4. **Portar os corpos:** idênticas e de mesmo formato (26 %) quase de graça depois do passo 3; ≥ 90 % e
   75–90 % por diff assistido; < 75 % e as 47 classes novas por decompilação normal, com o método do C1
   (Ghidra + conferência do assembly + `verify_hooks`).
5. **Montar o standalone** (`IMPLEMENT_CLASS` das 225, globais, recursos) e verificar contra o NWindow do C2
   pelo `verify_standalone`/`smoke`/`difftest` já existentes, com o md5 do C2.
6. **Testar no jogo** do C2: precisa de um servidor com o protocolo do C2 para as janelas novas. Há um no
   disco (`<C2 L2J>` e `C2_L2J_decompiled`); não verifiquei se sobe.

**Ordem de grandeza (estimativa, não medida):** a source do C1 saiu em 3 dias de calendário (25–27/09/2026:
fundação, 3.551 funções/755 KB, standalone rodando no jogo, 364 commits) com rodadas de agentes em paralelo.
O trabalho novo do C2 é uns 25–35 % do volume do C1 (reescrita + classes novas) mais o trabalho mecânico dos
cabeçalhos; então é de **dias**, não semanas, se o ritmo se mantiver.

## Riscos e limites

- Só a fração "idêntica/mesmo formato" é certa; o "parecida" é estimativa por instruções, um `difflib` sobre o
  assembly, e uma função grande com um `if` novo no meio pode cair em 75–90 % e dar pouco trabalho, ou em
  50–75 % por ter sido reordenada pelo compilador.
- O censo do C2 cobre 94,4 % do `.text`; os 5,6 % restantes são dados dentro do `.text` e funções que nenhuma
  referência alcança. Um passo a mais de sementes pode achar algumas.
- Para as classes novas não há o que reaproveitar; o que ajuda é que os nomes e assinaturas vêm do que já
  fizemos (`NCListBox`, `NCScrollWnd`…), e que o UnrealScript da interface do C2 (`*.u`, `*.xdat`) mostra como
  cada janela é usada.
- Este NWindow só serve para o cliente do C2 (Engine/Core do C2). **Não** dá para levar o NWindow do C2 para
  o cliente do C1 sem as 10 funções do Engine que faltam: é aí que uma source do Engine do C1 ajudaria
  (`Engine_Viabilidade.md`, vantagem 3).
- Nenhuma feature da camada `NW_FEATURES` (hover de item, combat log, barras extras, craft) vale no C2
  como está: elas acertam endereços e layouts do C1. A camada de features seria portada à parte.
- Uma source derivada do `NWindow.dll` da NCsoft é privada: não publicar.

## O que foi gerado

- `analysis/c2/analysis/classes.json`/`.md`: as 225 classes do C2 (tamanho, pai, vtable, slots).
- `analysis/c2/funcs_c2_nwindow.tsv`: as 7.117 funções do C2 (endereço, tamanho, faixas).
- `analysis/c2/class_delta.csv` e `compare_summary.txt`: a comparação por classe e os totais desta nota.
- Ferramentas (`tools/c2/`): `ghidra_census.sh` (censo de funções de qualquer DLL, com as sementes),
  `gen_seeds.py`, `extract_classes.py`, `compare_nwindow.py`; `tools/ghidra/NwdFuncs.java`.
- Para refazer: `tools/c2/ghidra_census.sh NWindowC2 <NWindow.dll do C2> <pasta>`, depois
  `python tools/c2/extract_classes.py <dll> analysis/c2` e
  `python tools/c2/compare_nwindow.py <dll> <pasta>/funcs.tsv analysis/c2/analysis/classes.json`.
