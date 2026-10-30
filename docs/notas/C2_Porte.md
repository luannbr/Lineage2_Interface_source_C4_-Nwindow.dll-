# Porte C1 → C2: o pipeline em `tools/port/`

Como a source do C2 sai da source do C1 sem recomeçar. O binário alvo é
`orig/NWindow.dll` (build 03/03/2005); a source de partida é a árvore do C1 (`NW_C1_ROOT`, padrão
`<C1 repo>`), que fica só para leitura. Tudo é modo gancho
primeiro: o que ainda não foi portado simplesmente continua rodando o código original do C2.

## Os mapas (cada um um script, cada um uma saída em `db/`)

| Passo | Script | Saída | O que responde |
|---|---|---|---|
| 1 | `match.py` | `db/fmap.csv` | qual função do C2 é a recompilação de cada função do C1 |
| 2 | `maps.py` | `db/amap.csv`, `analysis/port/votes.json`, `analysis/port/constants.csv` | endereços de globais/tabelas/strings, deslocamentos de campo (votos) e constantes que mudaram |
| 3 | `layout.py` | `db/layout.json` | onde cada campo de cada classe ficou no objeto do C2 |
| 4 | `vslots.py` | `db/vslots.json` | qual slot de vtable do C2 é cada slot do C1, e os slots que só existem no C2 |
| 5 | `eligible.py` | `db/eligible.csv`, `analysis/port/drift.csv` | quais funções casadas podem ser portadas copiando o corpo do C1 |
| 6 | `gen.py` | `include/nwindow/*.h`, `src/nwindow/*.cpp`, `db/gen_report.csv` | a source do C2 |
| — | `packet.py` | (tela) | o pacote de trabalho de uma função: casamento, source C1, diff de instruções anotado com os campos do C2, decompilação |

Ordem: `match.py`, `maps.py`, `layout.py`, `vslots.py`, `eligible.py`, `gen.py [--out DIR]`
(`--out build/gen_stage` gera numa pasta de ensaio sem tocar na árvore).

### 1. Casamento de funções (`match.py`)
As duas builds saem da mesma árvore de fontes e o linker põe os objetos na mesma ordem, então as funções
dos dois `.text` são duas sequências quase alinhadas. Âncoras: funções únicas nas duas imagens e iguais
sob uma impressão digital que sobrevive a mudança de layout (as strings wide que a função cita, o hash dos
tokens estritos, o hash sem os deslocamentos de campo, o hash do formato). A maior cadeia crescente de
âncoras fica; entre duas âncoras uma programação dinâmica monótona alinha o resto por similaridade
(≥ 0,55); por fim a passada da vtable casa slots entre dois slots já alinhados com a mesma distância.
Funções que são membros de vtable de classes sem parentesco nunca casam (as classes novas copiam o código
das antigas: sem essa regra o NCDialogBox casava com o NCGaraDialogBox).
Categoria de cada par: **A** igual; **B** igual depois de mascarar os deslocamentos de campo; **C** mesmo
formato, constantes diferentes; **D** formato diferente.
Medido: 2.724 das 3.009 funções do jogo do C1 (79,7 % dos bytes) têm par.

### 2. Operandos (`maps.py`)
Cada par é percorrido instrução a instrução (1:1 nos de mesmo formato, só nos blocos iguais do diff nos
outros). Registra pares de endereço (globais, tabelas, strings: 14 mil, 37 ambíguos), pares de
deslocamento `[this+d1] → [this+d2]` por classe e as constantes que mudaram. O `this` é rastreado
(ecx até o primeiro clobber, mais um registrador preservado que recebe `mov reg, ecx` uma vez): votos de
campo só vêm de operandos baseados em `this`, o que tirou o ruído dos acessos a outros objetos.

### 3. Layout (`layout.py`)
Por classe, dos pais para os filhos: os campos do cabeçalho do C1 em ordem de declaração recebem um
delta (offset C2 − offset C1) constante por trechos; trocar de delta custa 4 votos (inserção/remoção de
campos); campos sem voto continuam o delta dos vizinhos (`derived`, marcados no cabeçalho gerado e
proibidos nos corpos copiados). Exemplo conferido à mão: o NCWnd ganhou um campo de 4 bytes depois de
`DragMouseY`.

### 4. Slots virtuais (`vslots.py`)
Alinhamento monótono por classe dos slots do C1 com os do C2 (mesmo nome importado, mesma função pelo
mapa, ou herdado do pai com o mapa do pai). Mostra o que o estudo de viabilidade já via: `NCObject` 24→25
(o `UObject` ganhou `ClearL2Game` no slot 22), `NCWnd` 62→67 (novos 22, 51–54).

### 5. Elegibilidade (`eligible.py`)
Um corpo do C1 compilado contra os cabeçalhos do C2 é equivalente à função do C2 se: o formato é igual
(categoria A/B), todo deslocamento baseado em `this` cai onde o layout diz, todo endereço tem par, o `ret N`
é igual, e nenhuma string nem constante float apontada difere. Deslocamentos de outros objetos que mudaram
ficam como nota (`unverified`): quem decide é a conferência depois da compilação.

### 6. Geração (`gen.py`)
Cabeçalhos: linhas reescritas só onde os mapas mandam (endereço da UClass, `// +0xOFF`, preenchimentos
`BYTE _UnknownXXX[n]` para os bytes novos do C2, `NW_ASSERT_*`, `// <slot> 0xVA`, e `NewVirtual<slot>()`
para virtual que só o C2 tem). Fontes: cada unidade (tudo até uma linha `NW_HOOK_*`) elegível é copiada com
endereço e comentários reescritos; a que não é vira stub `NW_NAKED … NW_STOCK(va C2)` (salta para a função
original) ou, num construtor, `NW_UNPORTED()` (o vtable compilado precisa do construtor definido; o gancho
não é instalado, então nunca roda). Um construtor só é portado se todos os construtores dos ancestrais
nossos também forem (o construtor portado chama o da base); código que faz `new X` de classe com construtor
não portado não é copiado. Endereços literais no corpo passam pelos mapas, ou o corpo não é copiado.

## O que o pipeline não faz (e quem faz)
* As 47 classes novas e as funções sem par: decompilação à mão com o método do C1 (`packet.py` ajuda).
* Categoria D (formato diferente) e C (constantes): porte assistido pelo `packet.py`; a conferência depois
  da compilação (`verify_hooks`) é o juiz.
* Tipos auxiliares que não são classes do UE2 (`NString`, `NItemEntry`, `NListCell`, `NHtml*`…): sem mapa de
  layout; as estruturas compartilhadas com o Engine (`FL2GameData`: a base de `SysString` foi de +0x2EFB4
  para +0x4BC8C) precisam de cabeçalho novo.
* Mudanças de assinatura de funções de baixo nível (o `NCGDevice::DrawText` e o `GetTextExtent` ganharam um
  argumento no C2: aparece como `add esp,0x1C → 0x20` em 58 funções).

## Números do dia (06/10/2026)
Geração completa: 1.371 unidades portadas (121 KB do C1), 1.164 stubs, 209 sem par. A categoria D vale
770 funções / 346 KB.

## Teste diferencial com emulação (tools/port/emu)
Para funções que só o C2 tem (sem corpo do C1 para comparar), `tools/port/emu/` roda a função original e a nossa lado a lado no unicorn, com os imports do engine simulados em Python, e compara o log de chamadas e a memória escrita (`python tools/port/emu/t_num.py`, `t_fill.py`, `t_equip.py`; precisa de `build/bin/Release/NWindowDecomp.dll` e `pip install unicorn`). Foi escrito pela frente dos tool tips: ~9.600 casos aleatórios do Fill/Layout/Draw sem diferença, e uma mutação de um byte é pega. Serve para verificar outras funções grandes só do C2.
