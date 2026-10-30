# NWindow do C3 a partir da nossa source do C2 — estudo de viabilidade

Pedido do dono em 08/10/2026: "ANALISE apenas a possibilidade de fazermos o NWindow.dll do C3". Só análise: **nenhum arquivo da
source foi mudado**. Binário estudado: `<C3 client>\system\NWindow.dll`
(md5 `f0362880…`). Os dados do estudo ficam em `build/c3_study/` (fora do git): o censo de funções do Ghidra (`funcs.tsv`), a tabela de
classes (`analysis/classes.json`) e a comparação (`compare/`); as ferramentas são as do C2 (`tools/c2/ghidra_census.sh`,
`tools/extract_classes.py` apontado para outro DLL, `tools/c2/compare_nwindow.py` com a nossa árvore do C2 como base).

**Conclusão: viável, e mais barato por byte do que foi o C2, porque a base agora é a source do C2 (portada, verificada e já
rodando no jogo) em vez da do C1. Mas é um trabalho do tamanho do C2 inteiro:** o código de jogo é 38 % maior, há 93 classes novas e
a primeira tarefa é refazer as definições do Engine para o cliente C3. Metade do código serve quase como está; o que é novo de verdade
é cerca de um quinto, e boa parte dele é um cliente de MSN/Messenger que não dá para testar de ponta a ponta.

## Os dois binários

| | C2 | C3 |
|---|---|---|
| Compilação (PE) | 03/03/2005 | **15/09/2005**, mesmo compilador e linker (VC++ 7.0, build 9466, CRT estático): a comparação função a função funciona como no C2 |
| Tamanho / `.text` | 1.835.028 / 1.069.088 | **2.732.052 / 1.521.888** (+49 % / +42 %) |
| Seções | `.text .rdata .data .rsrc .reloc` | as mesmas, sem empacotamento (o Ghidra analisou normalmente) |
| Exports | 227 (225 classes) | **311 (309 classes)** |
| Imports | 452 | **530**: Engine 98→115, Core 161→198, mais `wininet` (8), `shlwapi` (4), `gdi32` (1) e `version` 3→6; `npkcrypt` continua 8 por ordinal |
| Funções (Ghidra com sementes) | 7.117 | **10.992** (+54 %): 1.575 de CRT/init/thunks, 3.793 funclets de EH, **5.624 de jogo (1,27 MB)** |

Pelos nomes dos imports (inferência, não li o código): o C3 traz o **modo replay** (`FL2ReplayManager`, `GL2ReplayMode`, `GIsBenchmarking`),
o **messenger** completo (`GMsn*`, `wininet` para o HTTP de login/e-mail), **zonas** (`GetCurrentZoneID/Name`), **efeitos visuais de skill**
(`USkillVisualEffect`), **radar/minimapa desenhados pelo Engine** (`DrawMinimapTex`, `DrawRadarBack`) e `FBlowFish`.

## Classes

* **309 classes**: 216 em comum com o C2, **93 novas**, **9 só do C2** (`NCMSNWnd` e `MSNSocket`, que o C3 troca pelo messenger novo,
  `NCNoticeWnd`, `NCMatchOptionWnd`, `NCTestUIWnd`, e as `NCRecipeTree*`/`NCTVItem`, trocadas pelas `NCRecipeGaraTree*`). Nenhuma classe mudou de pai.
* As 93 novas, por assunto (soma dos `sizeof`: 57 KB de objeto; **140 KB de código só nos virtuais próprios**, fora construtores e auxiliares):

  | Assunto | Classes | Código dos virtuais |
  |---|---:|---:|
  | MSN/Messenger (`MsnWnd`, `ChatWnd`, `L2FriendWnd`, caixas de diálogo de amigo/grupo…) | 32 | 45 KB |
  | Observador / replay / transmissão de GM (`NCBroadcast*`, `NCReplay*`, `NCBenchMarkResultWnd`) | 16 | 32 KB |
  | Login / lobby / criação / carregamento / opções / barras (`NCLoginFunctionWnd`, `NCLobbyStartWnd`, `LoginWnd`, `StatusWnd`…) | 22 | 26 KB |
  | Árvores "Gara" (missões, receitas, SSQ) | 9 | 15 KB |
  | Sete Selos (`NCSSQ*`) | 9 | 14 KB |
  | Sala de grupo / combinação (`NCPartyRoom*`, `NCMatch*`) | 5 | 8 KB |
* **Layout: 212 das 216 classes em comum mudaram de `sizeof` e 210 de número de slots.** É o mesmo mecanismo do C1→C2, só que mais
  regular: a base `NCWnd` cresceu **0x30 bytes** (0x98→0xC8) e **1 slot** (67→68), e 151 das classes ganharam exatamente 1 slot, 83 exatamente
  0x30 bytes. Na soma, as classes em comum só crescem 3,8 %. O caso extremo é o `NConsoleWnd`: **335→405 slots** (+70 callbacks de rede
  novos) e 0x35B8→0x39E8 bytes. O solver de layout e o alinhador de vtables que já temos (`tools/port/layout.py`, `vslots.py`) fazem isto.

## Quanto da nossa source do C2 serve (função a função, `compare_nwindow.py`)

Funções do C3 pelo que o C2 tem para elas (o C3 tem 1,27 MB de código de jogo; CRT/EH à parte):

| | funções | bytes | % do código de jogo |
|---|---:|---:|---:|
| Idênticas a uma função do C2 | 1.358 | 101.512 | 8 % |
| Mesma forma (só deslocamentos/registradores mudam) | 1.982 | 261.896 | 21 % |
| ≥ 90 % parecidas | 523 | 237.290 | 19 % |
| 75–90 % parecidas | 549 | 196.176 | 15 % |
| 50–75 % parecidas | 548 | 201.310 | 16 % |
| < 50 % ou sem par (novas) | 664 | 270.841 | **21 %** |

Do lado do C2 (884 KB de código de jogo): 10 % idêntico, 29 % mesma forma, 23 % ≥ 90 %, 17 % 75–90 %, **20 % < 75 % ou sumiu**.
O que mudou mais, por classe do C2 (parte < 75 %): `NConsoleWnd` 26 KB de 103 KB (os callbacks de rede), `NCConsole` 55 %,
`NCDetailStatusWnd`/`NCGMDetailStatusWnd` ~93 % (a janela de status foi refeita), `NCVIPShopItemInfoWnd` 88 %, `NCMinimapWnd` 81 %,
`NCLoginWnd` 85 %, `NCMatchWnd` 77 %, `NCGaraDialogBox` 55 %. 29 classes (44 KB) são praticamente as mesmas.

Para comparar: no C1→C2 a source do C1 servia como esqueleto e 79,7 % dos bytes do C2 tinham par; aqui a base é a source **já portada e
verificada do C2** (3.444 ganchos, 92 % do código), então o "serve como está" (idêntica + mesma forma, ~29 %) e o "serve com retoque"
(≥ 75 %, ~34 %) são trabalho de máquina mais conferência, e o que pede mão é o 37 % restante (50–75 % e novas).

## O que o trabalho tem de refazer

1. **Definições do Engine do C3** (a fundação): `UL2ConsoleWnd` com 405 slots, `FL2NetNotify`/`UL2NetHandler` (pacotes novos),
   `FL2GameData`, `User`, `ItemInfo` etc., e os imports novos (23 do Engine e 39 do Core: replay, zonas, efeitos de skill, globais do messenger). No C2 isto foi a
   "fundação A" (duas rodadas) e é pré-requisito de tudo. As ferramentas `tools/ue2_*` já fazem a extração; os layouts voltam a ser derivados do código.
2. **Retarget da infraestrutura** para a imagem do C3: constantes da imagem (md5, `TimeDateStamp`, `SizeOfCode`, entry point, blocos de thunks,
   `_purecall`, `appMemcpy`…) em `NwMain.cpp`, `build_db.py`, `verify_*`, o instalador de import (13 DLLs de import), os mapas `fmap/amap/layout/vslots` (C2→C3).
3. **Código novo**: as 93 classes (140 KB só de virtuais) mais as 664 funções sem par. Quase um terço é o **messenger** (`Msn::*`,
   `LSwitchboardSession`, `DispatchProcess`…: 320 funções novas com nome de guard), que usa HTTP (`wininet`) e protocolo de MSN que não existe mais:
   dá para portar e verificar contra o binário, não para testar funcionando. Replay/observador/benchmark também dependem de partes do Engine do C3.
4. **Retoque** das ~1.100 funções 50–90 % parecidas e **conferência** das ~3.300 idênticas/mesma forma (o `verify_data` e o `regcheck` do C2 valem; os
   ids de `SysString` mudam, a conferência de constantes e strings pega isso).
5. Os 8 remendos binários / nProtect: o C3 importa `npkcrypt` igual ao C2; falta ver se o `NWindow.dll` desse cliente tem remendos (não há `.BAK` na pasta).

## Estimativa (a partir dos tokens reais do C2)

Base medida nas transcrições da sessão do C2 (39 subagentes: fundação 7, onda 1 6, onda 2 13, onda 3 5, auditoria 3, standalone 5): **8.898 chamadas,
28,0 M tokens de escrita de cache, 3,60 G de leitura de cache, 32 horas-agente**. Por fase (leitura de cache): fundação 0,55 G, onda 1 0,98 G, onda 2 1,41 G,
onda 3 0,39 G, auditoria 0,05 G, standalone 0,21 G. A sessão principal (merges, ciclo, testes no jogo, estudos) somou mais 738 chamadas, 6,5 M de escrita e 0,38 G de leitura.

Bytes que pedem mão (50–75 % + novas) na comparação C1→C2: 237 KB; C2→C3: **472 KB (2,0×)**. Retoque (75–90 %): 187 KB → 196 KB (1,05×). Mecânico (idêntica, mesma
forma, ≥ 90 %): 469 KB → 600 KB. Ponderando (mão 1, retoque 0,4, mecânico 0,05) o C3 dá cerca de 1,7× o trabalho variável do C2; descontando o que já existe
(ferramentas, lista de armadilhas, parte da fundação), 25 a 30 %: **C3 completo ≈ 1,0 a 1,3× o custo de agentes do C2; sem messenger/replay/observador/benchmark ≈ 0,8 a 1,0×**
(incerteza de ±40 %). Isto **revisa** a estimativa "50 a 70 %" da primeira versão desta nota, que foi um julgamento sem a base em tokens.
Hook mode primeiro (como no C2), standalone depois.

## Pontos a decidir antes de começar

* **Messenger (45 KB de virtuais + ~320 funções auxiliares):** portar tudo (só verificação estática/emulação), deixar como está no original no modo gancho
  e ficar de fora do standalone, ou desligar no standalone (como o nProtect)?
* **Replay/observador/benchmark/transmissão de GM:** dependem de partes do Engine do C3; incluir ou deixar para depois?
* **Servidor:** o `PORTE-C3.md` do Lucera diz que o servidor loga, cria personagem e entra no mundo com o cliente C3, então há como testar no jogo, como no C2.
* **Base da source:** a do C2 já portada (recomendado) ou um fork limpo do C1 para cada versão.

## Riscos

* O layout das 216 classes em comum é derivado por solver: as definições de campos que o C2 nomeou à mão (`Unknown…`) precisam de nova leitura onde o C3 inseriu campos.
* O Engine do C3 é outra imagem: o hook mode só roda sobre o `Engine.dll`/`Core.dll` do C3 (que estão na pasta do cliente).
* O que não se mede aqui: a similaridade da parte nova, a presença de remendos no `NWindow.dll` do C3, e se há mais código do nProtect ligado estaticamente (o bloco do C2 tinha 83 funções).

Ver também `C4_NWindow_Viabilidade.md` (o mesmo estudo para o C4, com a comparação C2 → C3 → C4).
