# C2: onda 4 (auditoria das 108 diferenças de dados que ficaram sem nota)

Depois da onda 3: 3.444 ganchos, 91,94 % dos bytes, e todo o código de jogo que não é nProtect/NKeyCrypt está com gancho ou
com a razão registrada (docs/notas/C2_Onda3.md). O que resta de dívida é de qualidade: `db/verify_data.csv` lista 108
ganchos com status `diff`: o conjunto de constantes, de deslocamentos de campo ou de strings que a função original usa
não é o da nossa. São quase todos ganchos da onda 1 (corpo copiado do C1 pelo gerador) que nunca receberam uma nota.
Cada um é um de dois casos, e é preciso decidir qual:

1. **Artefato benigno** (o compilador gerou outro código para a mesma coisa): escreva uma linha ESPECÍFICA em
   `db/verify_data_notes.csv` (`va,"explicação"`), dizendo o que cada item da diferença é. Padrões que já sabemos benignos:
   * o original inlinou o construtor da base: `disps stock-only {98,9c,a0,a4}` nos construtores de NControl derivados
     (o NControl do C2 zera +0x98, +0x9c, +0xa4 e põe -1 em +0xa0: o nosso `NControl::NControl()` faz o mesmo; confirmado em
     NCSliderBar 10025e20 pela leitura do asm dos dois lados);
   * `add` contra `lea` do endereço de SysString, constantes de intervalo, constante float contra double, endereços de
     StockCall/nw::Addr, `ebp` usado como `this`, `IsA` inlinado.
2. **Defeito real** (campo de outro lugar, constante do C1 que mudou, string errada, ramo que falta): conserte a função (leia
   o asm do original, porte à mão), rode o ciclo, e quando a função for grande use o teste diferencial com emulação
   (tools/port/emu/).

Um caso comum e que NÃO é benigno: **a nossa função tem `guard(...)` (uma string de nome) e a original não**
(`strs stock-only {} ours-only {NConsoleWnd::RequestAuthLogin}`): no C2 aquela função não tem o quadro de exceção/guard.
Tire o `guard`/`unguard` da nossa para ficar igual ao original (confira no asm do original: sem `fs:[0]`, sem o
`push` da string de guard) e deixe uma nota só se ainda sobrar diferença.

## Como olhar cada função
* A lista: `db/verify_data.csv` (colunas `c2_va,name,status,detail`; filtre `status == diff`, ordene por endereço e pegue
  a sua faixa). O arquivo é gerado pelo ciclo: o do seu worktree vale para você, não o commite.
* O original: `analysis/ghidra/asm/<va>.asm`, `analysis/ghidra/decomp/<va>.c` (se existir).
* A nossa, compilada: `python tools/port/dis_ours.py <va> --root <seu worktree> --stock` (usa o log do smoke do cycle).
* O código nosso: o arquivo em `src/nwindow/` (procure o `NW_HOOK_*` do endereço) e o corpo C1 de origem (`python
  tools/port/packet.py <va>` mostra o casamento, o corpo do C1 e o diff anotado com os campos do C2).
* O mapa de campos das classes: `db/layout.json`, `analysis/classes.json`, os cabeçalhos `include/nwindow/`.

## Grupos (três agentes, faixas de endereço; um worktree `wt/<nome>` por agente)
Os 108 endereços ordenados, divididos em três faixas iguais:
* `w4a`: de 0x10006130 a 0x10052350 (36: NCProgressCtrl, NCFrameCtrl, NPointDeque, NStdLogicError, NewTaiwanIME,
  NCObject::SetTimer, NCSliderBar, NString, HtmlParseColor, NCBoardWnd, NStlWString, NCHtmlSliderBar, NCPledgeWnd
  AddMember/UpdateMember, NChatLimiter, NCConsole::OnPaint/OnHtmlCommand e os primeiros NConsoleWnd::Request*).
* `w4b`: de 0x10052400 a 0x10056230 (36: o resto do NConsoleWnd::Request*/Receive* e os de aliança/grupo: ReceiveWithdrawAlly
  (const 0x207), ReceiveOustAlly, ReceiveDismissAlly, ReceiveWithdrawalParty, PartyAddMember, PartyDeleteAll,
  EnablePartyChat, FormatSystemMessage 10056230, os GMView*).
* `w4c`: de 0x10057200 a 0x100ddb40 (36: PawnViewerFunction, GetArmorEnchantSteps, RunHtmlCommand, GetEnchantedAttack/Defense,
  Init, GetIconName, InitForPawnViewer, ConsolePostRender, NCLoadingWnd, NCTestUIWnd, as janelas de cerco e status
  (NCSiegeInfoListCtrl, NCSiegePartInfoWnd, NCSiegeDateInfoWnd, NCSiegeInfoMainWnd::OnCreate, NCStatusWnd, NCTargetStatusWnd,
  NCPlayerStatusWnd, NCPartyStatusWnd, NCPartyWnd), NCTrainWnd).

Cada agente: para cada função da sua faixa, classifique (benigno com nota específica, ou defeito consertado), e no fim
`python tools/port/verify_data.py` não deve listar nenhuma da sua faixa como `diff`. Não toque em funções fora da sua faixa
(outro agente pode estar na mesma classe: edite só o corpo da função que é sua e mantenha o cabeçalho intacto, ou mexa o
mínimo e faça o commit logo).

## Resultado (07/10/2026)
Os três auditores terminaram e foram fundidos (commit 038d58d). As 108 diferenças de `verify_data` estão todas
classificadas: **0 `diff`** (2.042 ok + 1.402 com nota), 3.444 ganchos, verificação 3.109 ok + 335 explicadas + 0 sem
explicação, regcheck limpo, 0 breakers alcançados. Resultado da auditoria: **nenhum defeito de comportamento**; o único
defeito real foi de fidelidade, a falta do quadro `guard()` em três `OnDestroy` (NCBoardWnd 10031310, NCStatusWnd 100d2df0,
NCDetailStatusWnd 100d36c0; só muda a linha "History:" do log de falha), já consertado. Todo o resto era artefato do
compilador, cada um com uma nota específica em db/verify_data_notes.csv.

Correções ao briefing desta onda (o auditor conferiu e eu estava errado): uma string de `guard()` que aparece só do nosso lado
no verify_data NÃO prova que o original não tem guard: o original tem o mesmo quadro, com o tratador (catch funclet) fora da
faixa que o verificador lê; confira `analysis/eh.json` (lista `guard` do endereço) antes de tirar qualquer guard. O verificador
segue só UM nível de forwarder: ganchos de struct por valor e `jmp` wrappers mostram deslocamentos "stock-only" que estão
no corpo fora de linha. Ideia aberta: ensinar o verify_data a seguir o forwarder nosso (acaba com essa classe de ruído).

Testes diferenciais novos: t_fmtsys, t_w4a, t_w4c (e os das ondas anteriores): tools/port/emu/.
