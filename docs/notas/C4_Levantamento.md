# C4 — levantamento da source (19/10/2026, fim da onda A do estágio 4)

Fonte: `docs/PROGRESSO.md` (gerado pelo ciclo) e `db/functions.csv` / `db/lanes_s4.json` do commit `s4-wave-a-merged`.
O que conta como "feito": função com gancho ativo na DLL (`NW_HOOK_*` no começo da linha). Os 13 ganchos desligados na fusão
(corpo alcança um stub não portado) NÃO entram na conta.

## Número principal

| | funções | bytes | % do código |
|---|---:|---:|---:|
| código a decompilar (game + init) | 7.202 | 1.448.754 | 100 % |
| **reimplementado, com gancho ativo** | **4.224** | **905.913** | **62,53 %** |
| sem gancho, por convenção (destrutores deleting, InternalConstructor) | 469 | 13.314 | 0,92 % |
| só na fase 5 (inicialização estática, DllMain) | 1.038 | 48.609 | 3,36 % |
| **falta (todo)** | **1.471** | **480.918** | **33,19 %** |

Marcos: fim do estágio 3 = 2.846 ganchos / 23,18 %; fim da onda A = 4.224 ganchos / 62,53 % (+39,35 pontos, +1.378 ganchos).
Para comparar: o C3 terminou em 4.970 ganchos / 93,03 %.

**Verificado no jogo:** os builds de 2.846 e de 3.617 ganchos entraram no mundo sem exceção. O build atual (4.224) foi só
fechado pelos julgadores (verify 4.005 ok + 219 explicados, 0 sem explicação; regcheck 4.224/0 problemas; `checks.py` ALL OK).

## Por lane (a onda A tinha as 8 primeiras; a B são as 8 seguintes)

| lane | bytes | feito | falta (bytes / funções) |
|---|---:|---:|---:|
| nconsole | 114.184 | 99,4 % | 567 / 4 |
| input1 | 96.746 | 98,5 % | 320 / 2 |
| input2 | 95.420 | 97,8 % | 16 / 1 |
| pawn | 97.704 | 95,6 % | 7 / 1 |
| items1 | 90.315 | 94,3 % | 116 / 3 |
| status1 | 98.031 | 93,5 % | 855 / 16 |
| status2 | 81.594 | 93,4 % | 695 / 12 |
| base | 71.799 | 91,3 % | 707 / 6 |
| **onda B** | | | |
| msn1 | 97.151 | 27,4 % | 65.469 / 185 |
| items2 | 90.054 | 13,3 % | 73.614 / 212 |
| items3 | 87.347 | 24,7 % | 62.710 / 134 |
| rest1 | 72.055 | 22,5 % | 50.990 / 237 |
| sys2 | 70.283 | 27,7 % | 46.804 / 89 |
| sys1 | 70.242 | 29,6 % | 45.454 / 98 |
| bgm | 61.727 | 20,8 % | 45.434 / 134 |
| msn2 | 54.782 | 23,4 % | 40.345 / 129 |
| sem lane (classes fora das 16) | 99.320 | 49,9 % | 46.815 / 208 |

A onda A deixou 3.283 bytes (0,23 % do código) nos becos sem saída que os agents listaram (corpos compartilhados, fragmentos,
GameGuard, ctor/dtor do NConsoleWnd). A onda B soma 430.820 bytes de todo = 89,6 % do que falta; com as classes "sem lane"
(46.815 bytes) chega a 99,3 %.

## O que a onda A encontrou de risco (para a onda B não repetir)

- Offsets copiados do C3: `ReceiveMacroListInfo` (macro: janela +0x150, slots +0x13C) e a mensagem de loja privada do pawn
  (+0x16A8 / +0x169C). Julgador novo `check_rawoffs` (todo offset numérico solto é lido contra o stock, `db/rawoffs_ok.txt`).
- Chamada ao stock com número errado de argumentos (`NewStock<NCCheckBox>(0x100058C0, ...)` com 3 em vez de 4). Julgador novo
  `check_stockargs` (compara com o `ret N` do stock, `db/stockargs_ok.txt`).
- `NW_UNPORTED_NORET` com `__assume(0)` fazia o compilador apagar em silêncio o código que chamava a função não portada
  (e escondia o breaker do `check_breakers`). Sem o `__assume`, 13 ganchos que chegam a stubs Msn/chat foram desligados; voltam
  quando a onda B portar os chamados.
- `NClasses.gen.h` estava com os endereços de classe do C3 (regenerado). `NToolTipSkin::Draw/DrawFull` eram stubs int3 chamados
  por janelas com gancho (agora repassam ao stock). Cabeçalhos ainda no layout C3 (dívida): `NItemInfo.h`/`NCItemWnd.h`
  (o layout C4 está em `NItemC4.h`/`NItemEntryC4.h`), `NConsoleWnd.h` (PetitionWnd é +0x3C00), `NCGDevice::DrawSpecialDigit`
  (6º argumento), `NCTabControl::AddTab` (9 argumentos), construtores novos de `NCButton`, `NCComboBox::AddItem`.
- Bug latente conhecido para o msn: `NMsnFriend(const FString&)` repassa `&FString` onde o stock espera a string por valor.

## Próximos passos

1. Teste em jogo do build de 4.224 ganchos (nada além de julgadores o cobre ainda).
2. Onda B (8 agents, mesmo método, `docs/notas/C4_Agentes.md`), com o aval do usuário: sobe de 62,5 % para ~89 % (a onda A fechou 91-99 % das suas lanes); as classes sem lane levam a ~92 %.
3. Depois: classes sem lane (46,8 KB), dívida de cabeçalhos C3, religar os 13 ganchos, servidor C4 (IDs de NPC).
