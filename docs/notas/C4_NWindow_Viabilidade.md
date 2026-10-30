# NWindow do C4 a partir da nossa source do C2 (e do C3) — estudo de viabilidade

Pedido do dono em 08/10/2026, depois do estudo do C3 (`C3_NWindow_Viabilidade.md`): "e comparado ao C4? o nosso C2 e depois do C3".
Só análise: **nenhum arquivo da source foi mudado**. Binário estudado:
`<C4 client>\system\NWindow.dll` (md5 `afab3ae3…`). Dados em `build/c4_study/` (fora do git): censo do Ghidra
(`funcs.tsv`), tabela de classes, e duas comparações com `tools/c2/compare_nwindow.py` (`compare_c2/`: a nossa source do C2 como base;
`compare_c3/`: a imagem do C3 como base, com a classificação de funções do C3 derivada pela heurística do C2 — o C3 ainda não tem source).

**Conclusão: viável, com a mesma ferramenta e o mesmo compilador. O C4 é o maior dos três (código de jogo 58 % maior que o do C2), e a
distância C2→C4 é grande demais para um salto direto confortável (46 % do código pede mão); mas o passo C3→C4 é pequeno (12 % pede mão).**
A rota C2 → C3 → C4 custa em total o mesmo trabalho "de mão" que o salto direto (643 KB contra 647 KB, ver abaixo) e entrega o C3 de brinde, com
cada passo verificável contra um binário perto do anterior. Minha recomendação: **C3 primeiro, C4 depois, sobre a source do C3**.

## Os três binários

| | C2 | C3 | C4 |
|---|---|---|---|
| Compilação (PE) | 03/03/2005 | 15/09/2005 | **13/01/2006** |
| Compilador / linker | VC++ 7.0 (build 9466), CRT estático | o mesmo | **o mesmo** (cabeçalho Rich com os mesmos ids de produto e build): a comparação função a função vale |
| Tamanho / `.text` | 1,83 MB / 1,07 MB | 2,73 MB / 1,52 MB | **3,11 MB / 1,69 MB** |
| Seções | `.text .rdata .data .rsrc .reloc` | as mesmas | as mesmas, sem empacotamento |
| Exports | 227 | 311 | **343** |
| Imports | 452 | 530 | **548** (Engine 126, Core 208) |
| Funções (Ghidra) | 7.117 | 10.992 | **12.470** |
| Código de jogo | 885 KB | 1.269 KB | **1.401 KB** |
| Nomes de guard (`Classe::Método`) | 383 | 696 | **944** |

## Classes

* **341 classes**: 305 em comum com o C3, **36 novas** sobre o C3 (**129 novas** sobre o C2), 4 que o C3 tinha e o C4 perdeu (`NCPetitionListBox`,
  `NCTutorialQuestionWnd`, `NCVIPShopItemListBox`, `NCVIPShopItemListWnd`). Contra o C2 sumiram 13 (as 9 do estudo do C3, mais essas 4).
  Um pai mudou: `NCChatListBox` passa de `NCListBox` para a nova `NCTextListBox`.
* As 36 novas sobre o C3 (13 KB de objeto, **52 KB de código só nos virtuais próprios**, 211 virtuais):

  | Assunto | Classes | Código dos virtuais |
  |---|---:|---:|
  | Manor (castelo/agricultura: `NCManor*`) | 13 | 23 KB |
  | Encantar item, peso do inventário, canal de comando, lista de quests | 6 | 11 KB |
  | Olimpíada (`NCOlympiad*`) | 3 | 4 KB |
  | Replay (lista, logo) | 3 | 4 KB |
  | Pesca, viewport, botão de efeito, VIP shop | 4 | 4 KB |
  | Chat novo (comando, mensagem de sistema, abas, `NCTextListBox`) | 5 | 4 KB |
  | Torre dos Heróis | 2 | 3 KB |

  Ao contrário do C3, **não há um bloco "morto" como o messenger**: tudo isto é interface de jogo (Manor, Olimpíada, Heróis, encantar), que o servidor C4 exercita.
  E o messenger/replay do C3 continuam no C4 (as 93 classes do C3 estão todas lá).
* **Layout:** as 305 classes em comum mudaram de `sizeof` (todas) e 301 de número de slots. A base `NCWnd` vai a **0xE8 bytes e 74 slots**
  (C2: 0x98/67; C3: 0xC8/68), `NCObject` e `NCCmdTarget` crescem 0xC bytes (0x28→0x34, é a primeira vez que a base do objeto muda), e o `NConsoleWnd`
  vai a **448 slots** (C2: 335; C3: 405: +43 callbacks de rede). 252 classes ganham exatamente 6 slots; 195 exatamente 0x20 bytes.
  **Um caso a vigiar:** `NCVIPShopItemInfoWnd` passa de 0x1AFC (C2) para **0x58674 bytes (355 KB)**; sem ele as classes em comum só crescem 5,4 % desde o C2 (2,0 % desde o C3).
  O solver de layout dá conta, mas é um objeto de tamanho incomum e a `new` dele precisa do `sizeof` certo.

## Imports novos sobre o C3 (o que o Engine/Core do C4 precisa oferecer)

* **Engine (+14, −3):** replay (`FL2ReplayManager::OnPublicLoad/OnSaveStart/OnSaveStop`), animação de texto (`UCanvas::SetTextAnim/StartTextAnim/EndTextAnim`),
  nome da zona (`ALineagePlayerController::InvalidateZoneName`), `APawn::GetCurAnimName`, `FLandMark::DeleteLandMark`, `UCanvas::DrawMinimapLoc`, `FL2GameData::Clear`.
* **Core (+12, −2):** globais novos (`GL2SystemMsgWnd*`, `GL2ShowZoneTitle`, `GL2PostProcessType`, `GL2FFEffectUse`, `GIsTrueFontUse`, `GCountryCode`),
  `FArray::AddZeroed`, `FMemStack::GetThreadSafeMemStack`.
* **imm32:** 16→13 (sem `ImmAssociateContext/CreateContext/DestroyContext`): a entrada de texto (IME) do `NCEditBox` foi refeita (8 slots novos, 0x30 bytes a mais).
* kernel32/user32/advapi32/ws2_32/wininet/shlwapi: mesma contagem de imports; `npkcrypt` continua 8 por ordinal.

## Quanto da source serve (função a função)

Funções do C4 pelo que a base tem para elas, sobre o código de jogo do C4 (1.401 KB; CRT/init/EH à parte, 186 KB):

| | base = C2 (nossa source) | base = C3 |
|---|---:|---:|
| Idênticas | 94 KB · 6,7 % | 190 KB · 13,6 % |
| Mesma forma | 199 KB · 14,2 % | 493 KB · 35,2 % |
| ≥ 90 % parecidas | 213 KB · 15,2 % | 346 KB · 24,7 % |
| 75–90 % | 247 KB · 17,7 % | 199 KB · 14,2 % |
| 50–75 % | 282 KB · 20,1 % | 75 KB · 5,4 % |
| < 50 % ou sem par (novas) | 365 KB · 26,1 % | 96 KB · 6,9 % |
| **Pede mão (50–75 % + novas)** | **647 KB · 46 %** | **171 KB · 12 %** |

Do lado da base: do código de jogo do C3, 14,5 % é idêntico no C4, 38,3 % mesma forma, 26,5 % ≥ 90 %, 12,8 % 75–90 % e só 7,9 % mudou muito ou sumiu.
Do lado do C2: 28,6 % do código de jogo do C2 mudou muito ou sumiu até o C4 (C2→C3: 20 %).
Onde o C2 mais mudou até o C4: `NConsoleWnd` (33 KB de 103 KB), `NCConsole` 60 %, `NCChatWnd` 68 % (o chat foi refeito), `NCDetailStatusWnd`/`NCGMDetailStatusWnd`
~93 %, `NCVIPShopItemInfoWnd` 88 %, `NCGaraDialogBox` 55 %, `NCGameOptionWnd` 62 %, `NCGMWnd` 64 %, `NCMinimapWnd` 81 %, `NCMultiEdit` 25 % (de 27 KB).

**A rota em cadeia contra o salto direto**, em bytes que pedem mão (50–75 % + novas):

| Rota | KB |
|---|---:|
| C2 → C3 (estudo do C3) | 472 |
| C3 → C4 | 171 |
| **C2 → C3 → C4** | **643** |
| **C2 → C4 direto** | **647** |

O trabalho "de mão" é praticamente o mesmo; o que muda é que a cadeia entrega o C3 de graça, cada passo é verificável contra um binário vizinho
(o `verify_data`/`regcheck` do C2 continuam valendo), e a faixa de retoque (75–90 %) é feita em dois passos menores. O custo da cadeia é que parte do retoque
é feito duas vezes: 395 KB (196 + 199) contra 247 KB no salto direto.

## O que o trabalho tem de refazer (além do que o estudo do C3 já listou)

1. **Fundação do Engine do C4**: `UL2ConsoleWnd` com 448 slots, os formatos de pacote novos (Manor, Olimpíada, Heróis, encantar, canal de comando, pesca), `FL2GameData`,
   o novo `NCObject` de 0x34 bytes (o cabeçalho de **toda** classe muda: o solver precisa partir do novo tamanho da base).
2. **Retarget da infraestrutura** para a imagem do C4 (as mesmas constantes que o C3: md5, carimbo, `SizeOfCode`, entry point, blocos de thunks, o instalador de imports).
3. **Código novo**: 36 classes (52 KB de virtuais) e o que sobra do C3 (140 KB em 93 classes). O `NConsoleWnd` ganha 43 callbacks e 38 nomes de guard.
4. **Retoque** da faixa 50–90 % e **conferência** das idênticas/mesma forma.

## Estimativa (a partir dos tokens reais do C2; ver a nota do C3)

Unidade: o custo de agentes do C2 (8.898 chamadas, 28,0 M de escrita de cache, 3,60 G de leitura, 32 horas-agente). Pelos mesmos pesos da nota do C3 (mão 1, retoque 0,4,
mecânico 0,05) e o mesmo desconto de reaproveitamento:

* **C3 sobre a source do C2:** ≈ 1,0 a 1,3× (0,8 a 1,0× sem messenger/replay).
* **C4 sobre a source do C3 já feita** (171 KB de mão, 199 KB de retoque, 1.029 KB mecânicos): ≈ **0,5 a 0,8×**.
* **C4 direto sobre a source do C2** (647 KB de mão, 247 KB de retoque): ≈ **1,5 a 2,0×**.
* A cadeia C3 + C4 soma ≈ 1,5 a 2,1×, no mesmo patamar do salto direto, mas entrega os dois clientes e passos verificáveis.

Isto revisa os "50–70 %", "25–40 %" e "90–110 %" da primeira versão, que eram julgamento sem base em tokens; a incerteza continua de ±40 %.

## Pontos a decidir antes de começar

* **Rota:** C3 primeiro e C4 sobre ele (recomendado), ou saltar direto ao C4? Se o interesse real é só o C4, o salto direto é possível, mas sem os dois degraus de verificação.
* **Messenger / replay / benchmark:** continuam no C4 (a decisão do estudo do C3 vale para os dois).
* **Servidor:** o `PORTE-C4.md` do Lucera descreve o servidor adaptado ao C4 ("C4 e C5 são quase o mesmo protocolo"), então o teste no jogo é possível, como no C2.
* **Remendos binários / nProtect:** o C4 importa `npkcrypt` igual; ainda sem verificação de remendos (a pasta do cliente não tem `.BAK`).

## Riscos

* O `NCObject` maior (0x34) desloca **todos** os campos de **todas** as classes (no C3 não mudou): a fundação precisa refazer a base antes de qualquer porte.
* `NCVIPShopItemInfoWnd` de 355 KB (ver acima) e `NCHennaInfoWnd` (+0x438), `NCRecipe*` (+0x22C a +0x230), `NCPawnViewrWnd`/`NCNpcViewrWnd`/`NCMSViewrWnd` (+0x1E8 a +0x1EC): objetos grandes cujo layout deve ser relido.
* O Engine do C4 é outra imagem: o hook mode só roda sobre o `Engine.dll`/`Core.dll` do C4.
* A heurística de classificação das funções do C3 (o C3 ainda não tem source) pode errar poucos KB entre "jogo" e "infra"; as proporções não mudam de forma relevante.
