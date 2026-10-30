# Como modificar a interface

**Português** · [English](MODIFY_INTERFACE.md) · [Русский](MODIFY_INTERFACE.ru.md)

Guia prático para mudar a interface do C4 nesta source: como as janelas são montadas, onde o código novo se
encaixa e receitas passo a passo (uma barra de atalhos nova, um botão, uma janela, uma cor de chat, uma linha de
tooltip). As mudanças de interface entram no build **standalone** (`NWindowStandalone`) atrás de uma chave de
compilação sua; o modo hook e o standalone puro continuam fiéis ao original, e é neles que os juízes comparam.

## 1. Como as janelas são montadas

### A hierarquia

| Classe | Arquivo | Papel |
|---|---|---|
| `NCObject : UObject` (0x34 bytes) | `include/nwindow/NCObject.h` | base de tudo; `FindTexture(TEXT("Pacote.Grupo.Nome"))` carrega uma textura |
| `NCWnd` (0xE8 bytes, 74 slots de vtable) | `include/nwindow/NCWnd.h` | toda janela: posição (`LocalX/Y` relativo ao pai, `ScreenX/Y`, `Width`, `Height`), estilo, filhos, foco, barra de título (`FrameCtrl`), ícone minimizado |
| `NControl : NCWnd` (0x1F4, 75 slots) | `include/nwindow/NControl.h` | todo controle; tem um `ID` e avisa o pai com `NWM_COMMAND` (wParam = `ID`) |
| `NCButton`, `NCPushButton`, `NCExButton`, `NCRadioButton` | `include/nwindow/NCButton.h` ... | botões com texturas normal/pressionada (`SetNormalTex`, `SetPushedTex`, `SetText`, `SetTextID`) |
| `NCEditBox`, `NCListBox`, `NCScrollWnd`, `NCComboBox`, `NCTabControl`, `NCSliderBar`, `NCProgressCtrl`, `NCListCtrl`, `NCMultiEdit`, `NCHtmlViewer`... | `include/nwindow/` | os outros controles |
| `NC*Wnd` | `include/nwindow/` | as janelas do jogo (inventário, chat, barra de atalhos, status...) |
| `NCVirtualWndMain` | `include/nwindow/NCVirtualWndMain.h` | dono de todas as janelas: registro, ordem z, hit test, foco, despacho de mensagens, cursores |
| `NConsoleWnd : UL2ConsoleWnd` (0x3CA4, 448 slots) | `include/nwindow/NConsoleWnd.h` | o console da engine (eventos de rede e de UI); `GConsole` |
| `NCConsole` (0x2A0) | `include/nwindow/NCConsole.h` | o **desktop** do jogo (`GConsole->GetDesktop()`): pai de todas as janelas do jogo, com um ponteiro para cada (`ChatWnd` +0xF4, `InventoryWnd` +0x120, `SystemMenuWnd` +0x148, `ShortCutWnd` +0x15C...) |

Os tamanhos são fixos: estão registrados no Core (tamanho da `UClass`, `analysis/classes.json`), e o código original e
a engine indexam neles. `NW_ASSERT_SIZE` / `NW_ASSERT_OFFSET` em cada cabeçalho garantem isso.

### Criar uma janela ou um controle

O padrão do código original (`NCSystemMenuWnd::OnCreate` em `src/nwindow/NCSystemMenuWnd.cpp`):

```cpp
NCButton* Button = new NCButton(TEXT("L2UI_CH3.SystemMenuWnd.systemicon1"),
                                TEXT("L2UI_CH3.SystemMenuWnd.systemicon1_down"), 0);
Button->ID = NID_ALGUMA_COISA;       // o que chega no NWM_COMMAND do pai
Button->SetPosition(13, 29, 36, 36); // relativo ao pai (C4: virtual com 3 argumentos a mais, a fixação na tela)
CreateChild(Button, NWS_VISIBLE);    // registra, torna a posição absoluta, manda WM_CREATE -> OnCreate
```

- O `new` de uma classe do pacote é o `operator new` do UE2 (o objeto é um `UObject`, alocado com o tamanho da sua
  `UClass` registrada).
- `CreateChild(filho, estilo)` (`NCWnd`, inline) = `AddChildWindow` + `AttachEntry` + posição de tela + `WM_CREATE`. O
  desktop registra as suas janelas à mão (`NCConsole::OnCreate`: `AddChildWindow(W, estilo)`, `HideWindow`,
  `AttachEntry`).
- Estilos (`NWS_*` em `NCWnd.h`): `NWS_VISIBLE`, `NWS_TOPMOST` (lista separada, por cima), `NWS_TITLEBAR` (o
  `NCWnd::OnCreate` cria uma barra de título `NCFrameCtrl`), `NWS_MINIMIZABLE`, `NWS_NOCLOSE`, `NWS_CLIPTOPARENT`...
- Destruir: no `OnDestroy` do dono, `RemoveChildWindow(Filho->Entry); delete Filho;` (veja
  `NCSystemMenuWnd::OnDestroy`). O `NCWnd::OnDestroy` só destrói a barra de título e o ícone.

### Mensagens, pintura, tooltips

- Tudo chega pelo `WindowProc(Msg, wParam, lParam)` (slot 25) com os números de mensagem do Win32; o `NCWnd` despacha
  para as virtuais (slots do C4): `OnTimer` (36), `OnCreate` (37), `OnDestroy` (38), `ShowWindow` (48), `HideWindow`
  (49), `MoveWindow` (51), `OnPaint(NCGDevice*)` (62), `OnDrawToolTip` (63), `OnChar` (64), `OnKeyDown` (65),
  `OnMouseMove` (67), `OnLButtonDown/Up` (68/69), `OnRButtonDown/Up` (70/71), `OnMove` (72), `OnMouseWheel` (73).
- Um controle avisa o pai com `NWM_COMMAND` (`0x501`), `wParam` = o `ID` do controle; o pai trata no seu `WindowProc`
  (exemplo: `NCSystemMenuWnd::WindowProc`).
- A pintura (`OnPaint`) usa o `NCGDevice` (`include/nwindow/NCGDevice.h`): `SetClip(ScreenX, ScreenY, W, H)`,
  `BeginPaint(ScreenX, ScreenY)` (daí em diante as coordenadas são relativas), `DrawTexture(x, y, w, h, u, v, ul, vl,
  Tex)`, `DrawText(x, y, cor, texto, ..., L2FONT_Normal, ...)`, `EndPaint`, `ReleaseClip`. As texturas são carregadas uma
  vez no `OnCreate` com `FindTexture` (os pacotes `L2UI*` do cliente; o C4 tem o conjunto `L2UI_CH3.*`). Veja
  `NCSystemMenuWnd::OnPaint`.
- Tooltip: `OnDrawToolTip(NCGDevice*)` da janela sob o mouse, chamado depois da pintura.

### Console e desktop

O `NConsoleWnd` recebe os eventos do servidor e do jogo e é dono do desktop `NCConsole`. O `NCConsole::OnCreate`
(`src/nwindow/NCConsole.cpp`) cria todas as janelas do jogo (posições pelo tamanho da tela), o `NCConsole::OnDestroy`
destrói, o `NCConsole::Reset` roda quando o jogador sai do jogo. O `NConsoleWnd::Tick` roda a cada quadro; o
`NConsoleWnd::HookShortCutKey` vê as teclas primeiro (F1..F12, Alt+letra, o 1..= do modo "Enter para falar").

## 2. Onde o código novo se encaixa

Esta árvore não tem uma camada de recursos própria. Os blocos `#if NW_FEATURES` em `src/nwindow` são pontos de chamada
dos recursos do projeto do C1, herdados pelos forks: nada aqui define `NW_FEATURES` e `src/features/` não existe, então
eles compilam como nada. No C4, use o padrão que o experimento separado abaixo usou:

| Peça | Onde | O que faz |
|---|---|---|
| uma chave sua, por exemplo `NW_CUSTOM=1` | `CMakeLists.txt`, `target_compile_definitions(nwd_standalone_objs ...)` | definida só para os objetos do standalone; o alvo de hook nunca a vê, então `verify_hooks`/`checks.py` continuam comparando o código fiel |
| os seus fontes | `src/custom/*.cpp` (+ um `file(GLOB NWD_CUSTOM_SOURCES ...)` acrescentado ao `nwd_standalone_objs`), cabeçalhos em `include/nwindow/` | o código do recurso, no estilo da source |
| pontos de chamada | dentro de funções originais, `#if NW_CUSTOM ... #endif` com `// custom: ...` | a única mudança no código decompilado; o ramo original fica como está |
| pontos de entrada | `NCConsole::OnCreate` (depois das janelas originais), `NCConsole::OnDestroy` (antes delas), `NCConsole::Reset`, `NConsoleWnd::Tick`, `NConsoleWnd::HookShortCutKey` | criar / destruir / zerar / trabalho por quadro / teclas |
| configurações | um arquivo seu ao lado do `l2.exe` (`GetModuleFileName` + `GetPrivateProfileInt/String`) | nunca chaves novas nos ini do jogo |
| um teste unitário | um `add_executable` da lógica que não depende da engine (sem a source de jogo, mesmas flags) | o que dá para conferir fora do jogo |

**Classes.** Uma classe de janela nova que **não acrescenta campo** pode viver sob uma `UClass` original:
`static UClass* StaticClass() { return NCWnd::StaticClass(); }` (ou a classe original que ela estende), um
`operator new` que chama `StaticAllocateObject(StaticClass(), ...)` e `NW_ASSERT_SIZE` igual ao tamanho da base. A
tabela de classes, os exports, o `verify_standalone` e o `smoke_standalone` continuam vendo exatamente as classes
originais. O estado vai para variáveis estáticas do seu `.cpp`. Uma classe com campos próprios precisa de `UClass` e
export próprios, que os juízes do standalone recusariam até aprenderem essa lista (o projeto do C1 fez isso com
`NW_DECLARE_L2_CLASS` e `tests/standalone/l2_extras.csv`).

## 3. Receita: barras de atalhos novas (como no Interlude/Essence)

Isto descreve a abordagem implementada para o C4 num **experimento separado** (um repositório de desenvolvimento à
parte, branch `feature/barras-potions`, sob `#if NW_CUSTOM`; **não** faz parte desta source). Ele compilou e passou nas
verificações do standalone fora do jogo (`smoke_standalone` 9/9, `verify_standalone` OK, um teste unitário próprio); o
teste no jogo segue um roteiro próprio.

**Os fatos em que se apoia.** A barra do C4 é um `NCShortCutWnd` só (`include/nwindow/NCShortCutWnd.h`, desktop
`+0x15C`), criado no `NCConsole::OnCreate` com `new NCShortCutWnd(12, 10, 32)`: 12 slots × 10 páginas, os 120 slots em
`Items` (`+0x200`) e o estado das teclas em `Keys` (`+0x1FC`), a página mostrada em `Page` (`+0x270`). O slot de um atalho
é `Page * 12 + posição`, o número plano que o servidor guarda e devolve. O servidor aceita as páginas 0..11, então barras
que mostram as páginas 0..9 não pedem mudança no servidor.

**A ideia.** Uma barra extra **não tem slots próprios**: `NCExtraShortCutWnd : NCShortCutWnd` não acrescenta campo,
vive sob a `UClass` da barra principal e mostra outra página da mesma tabela de 120 slots.

1. **Criar** o conjunto no `NCConsole::OnCreate`, depois das janelas originais (`NCustomFeatures::CreateWindows(this)`
   sob `#if NW_CUSTOM`). No `OnCreate` da barra extra, depois que o `OnCreate` original criou tabelas próprias, liberá-las
   e apontar para as da principal (`Items = Main->Items; Keys = Main->Keys;`); `Page` é da própria barra (páginas
   padrão 1..5).
2. **Destruir** no `NCConsole::OnDestroy`, **antes** das janelas originais: o `OnDestroy` da barra extra põe `Items` e
   `Keys` em `NULL` primeiro (são da principal; senão são liberadas duas vezes), depois o `OnDestroy` original.
3. **Um relógio só.** Só a barra principal faz o tick dos cool times (o `NConsoleWnd::Tick` chama o `Tick` dela); a extra
   só os pinta. Seis barras com tick fariam os tempos correrem seis vezes mais rápido.
4. **Seguir a principal** a cada quadro (`NConsoleWnd::Tick` → o `Tick` do conjunto): a mesma orientação (`bHorizontal`,
   e `UpdateLayout()` quando muda), empilhadas à esquerda de uma barra vertical ou acima de uma horizontal (do outro
   lado quando a tela não tem espaço), escondidas quando ela se esconde. Arrastar uma barra extra move a principal
   (`MoveWindow` repassa) e o conjunto acompanha.
5. **Arrastar e soltar não muda nada**: todo ponto de soltura (inventário, paperdoll, ações, skills, macros, receitas, a
   própria barra) pergunta `IsA(NCShortCutWnd::StaticClass())` e depois `SlotAtPoint(X, Y)`, que devolve o slot global
   `Page * 12 + i`; o registro vai para o servidor com esse slot. O clique num slot usa o mesmo índice global.
6. **Mudanças de dados ficam na principal**: `SetShortCut` / `DeleteShortCut` / `ClearShortCuts` / `UpdateItem` rodam só
   nela (o `ReceiveShortCutList`, o `AddShortCutItem`... do console já chamam só a principal); as extras só repintam.
7. **Botões.** Um botão Expand na principal (um `NCPushButton` filho com um `ID` novo depois dos originais, `0xFF00000A`;
   os da barra vão até `NID_SHORTCUT_PAD` `0xFF000009`), tratado no ramo `NWM_COMMAND` do `NCShortCutWnd::WindowProc`: uma
   barra a mais por clique, todas escondidas depois da quinta. Cada barra extra mantém as setas de página
   (`OnNextClicked`/`OnPrevClicked`, páginas 0..9 em círculo); os botões de girar e de game pad são criados (o layout
   original espera por eles) mas ficam escondidos.
8. **Teclas.** No `NConsoleWnd::HookShortCutKey`, antes do código original: com o chat no modo "Enter para falar", 1..0,-,=
   vão para a primeira barra extra (fora disso repetem F1..F12) e Q..P,[,] para a segunda, enquanto essas barras estão
   visíveis. Os rótulos são pintados sobre as texturas das teclas F no `OnPaint` da barra extra.
9. **Salvo** no ini próprio do cliente (`system\NWindowCustom.ini`): quantas barras estão visíveis e a página de cada uma.

O mesmo experimento acrescentou uma janela de **auto potions** (`NCAutoPotionWnd`, sob a `UClass` do `NCWnd`, sem
campo): um slot por tipo de poção preenchido pelo inventário, o clique direito liga, um timer de 200 ms compara HP/MP/CP
com um limite por personagem e chama `NConsoleWnd::RequestUseItem(ObjectID)` depois do intervalo mínimo de cada tipo;
soltar um item do inventário nela é um ramo `#if NW_CUSTOM` no código de soltura do `NCInvenItemWnd`.

## 4. Receita: um botão novo numa janela existente

Exemplo: mais um botão no menu do sistema (`NCSystemMenuWnd`).

1. Os seus arquivos (`src/custom/MeuBotao.cpp`, `include/nwindow/MeuBotao.h`): `MeuBotao_Create(NCWnd* Dono)`,
   `MeuBotao_OnCommand(NCWnd* Dono, WPARAM ID)`, `MeuBotao_Destroy(NCWnd* Dono)`; o ponteiro do botão é uma estática do
   `.cpp` (nunca um campo novo do `NCSystemMenuWnd`).
2. Criar no fim do `NCSystemMenuWnd::OnCreate`, em `#if NW_CUSTOM`: `new NCButton(normal, pressionada, 0)`, um `ID` que
   nenhum controle daquela janela usa (veja o enum `NID_SYSMENU_*`), `SetPosition` dentro da janela,
   `Dono->CreateChild(Botao, NWS_VISIBLE)`.
3. Tratar o clique no ramo `NWM_COMMAND` do `NCSystemMenuWnd::WindowProc`:
   `if (MeuBotao_OnCommand(this, wParam)) return 1;` (em `#if NW_CUSTOM`).
4. Destruir no `NCSystemMenuWnd::OnDestroy`, antes do `NCWnd::OnDestroy()`: `RemoveChildWindow(Botao->Entry);
   delete Botao;`.
5. Se a janela tiver de crescer, mude o `SetPosition` dela no `NCConsole::OnCreate` e o fundo no `OnPaint`, sempre em
   `#if NW_CUSTOM`, deixando o ramo original intacto.

## 5. Receita: uma janela nova

1. Uma classe derivada de `NCWnd` (ou de uma janela original) que **não acrescenta campo** (seção 2): o estado em
   estáticas do seu `.cpp`, `NW_ASSERT_SIZE(NCMinhaWnd, 0xE8)` para provar.
2. Sobrescreva as virtuais que precisar: `OnCreate` (chame `NCWnd::OnCreate()` primeiro, carregue as texturas com
   `FindTexture`, crie a barra de título e os controles), `OnDestroy` (destrua os filhos, depois `NCWnd::OnDestroy()`),
   `WindowProc` (`NWM_COMMAND`), `OnPaint`, `OnDrawToolTip`, `OnTimer`. Mantenha os comentários de slot do `NCWnd.h`: um
   override com outra assinatura vira slot novo e quebra a classe.
3. Crie a partir do `NCConsole::OnCreate` (o seu ponto de entrada, depois das janelas originais) com o registro do
   desktop (`SetPosition`, `AddChildWindow(W, estilo)`, `AttachEntry`, `HideWindow`), e destrua a partir do
   `NCConsole::OnDestroy`, antes das originais. Mostre com `ShowWindow()` e `SetHeadWindow(Entry)` (`BringChildToTop`).
4. Modelos: `NCSystemMenuWnd` (barra de título `NCFrameCtrl(1, 0, 0)`, botões, fundo, texto) e a barra de atalhos
   (`NCShortCutWnd`: texturas, botões, layouts por orientação, tooltip).

## 6. Receita: uma cor de chat nova

O servidor não manda cor: manda o **tipo** do chat (`Say2`), e o cliente escolhe a cor no `NConsoleWnd::Say2`
(`src/nwindow/NConsoleWndEvents.cpp`, original `0x1007d200`). Tipos que o C4 não conhece caem no último `else`,
desenhados em `0xFFDCDCDC` na aba geral (tipo 0). As falas de NPC passam pelo `NConsoleWnd::NpcSay`, logo abaixo.

1. Acrescente o seu tipo antes do último `else`, em `#if NW_CUSTOM`:
   `else if (Type == 18) GetDesktop()->Say(ObjectID, Name, Text, 0xFF408CFF, Type);` com o ramo `#else` mantendo a
   cadeia original intacta (ou ponha a cadeia inteira nos dois ramos).
2. O último argumento é o tipo sob o qual a janela de chat arquiva a linha (abas e filtros); um tipo que a janela de chat
   não conhece pode não aparecer em nenhuma aba além da geral: passe 0 nesse caso, como o `else` original.
3. Não reaproveite tipos com efeitos colaterais (o TELL põe quem falou na lista de resposta; o ANNOUNCEMENT troca o nome).
4. Mudar a cor de um tipo que o C4 **já** colore é mudar um ramo fiel: faça sob `#if NW_CUSTOM` dentro do `Say2`,
   mantendo a cor original no ramo `#else`.

## 7. Receita: uma linha nova no tooltip de item

O tooltip de item das grades de itens é desenhado pelas rotinas de `include/nwindow/NItemToolTip.h` (páginas do
inventário, paperdoll, troca, armazém, lojas privadas, inventário do pet): `SetTexts`/`SetStoreTexts` preenchem
`ToolTipName` e `ToolTipText` (o texto que **dimensiona** a caixa) e `DrawLines` desenha as linhas, 13 pixels uma da outra
(`DrawTypeLines`, depois `DrawTailLines`, que termina com a descrição).

1. Acrescente a linha nos dois lugares, em `#if NW_CUSTOM`: no texto que dimensiona (para a caixa crescer) e no desenho
   (por exemplo em `DrawTailLines`, antes da descrição, com `DrawLabel`/`DrawValue` e `Y += 13`).
2. As rotinas são templates instanciados por cada grade com parâmetros diferentes: confira cada cópia que mudar.
3. Os tooltips das outras janelas são o `OnDrawToolTip` delas (slot 63): dê à caixa a altura a mais onde ela é calculada
   e depois desenhe a linha, os dois sob a chave.

## 8. O ciclo de build e teste

```bat
cmake -S . -B build -A Win32 -DNWD_GAME_SOURCES=ON
cmake --build build --config Release --target NWindowStandalone
python tools\verify_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\smoke_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\cycle.py --game-source --no-install
```

1. Compile o `NWindowStandalone` até não haver erro nem aviso novo.
2. `verify_standalone` e `smoke_standalone` têm de continuar passando: com classes que não acrescentam campo, a tabela de
   classes e a saída comparada com o original não mudam. Se uma janela nova mudar a saída do smoke, explique e ajuste a
   comparação com justificativa; nunca esconda diferença.
3. Rode o ciclo do modo hook: ele prova que o código fiel não mudou (a chave não é definida lá).
4. Teste no jogo: feche o jogo, guarde o `NWindow.dll` de uma **cópia** da pasta `system` e copie
   `build\standalone\Release\NWindow.dll` por cima. As ferramentas não desenham a interface: posição, arte e arrasto só
   aparecem no jogo. Num crash: `NWindow_crash.log` ao lado do `l2.exe` + `python tools\port\map_addr.py --log
   NWindow_crash.log`.

## 9. Regras de segurança

- Nunca acrescente campos nem slots virtuais novos a uma classe original: o tamanho dela está registrado na engine e o
  código original indexa nela e na vtable. Sobrescrever um slot que a classe já tem pode.
- No código decompilado, só o ponto de chamada, sob a sua chave com um comentário `// custom: ...`; o ramo original fica
  como está. O build de hook nunca recebe a chave.
- Confira cada nome de textura nos pacotes do cliente C4 antes de usar (uma textura do Interlude/Essence muitas vezes não
  existe no C4).
- O mesmo protocolo do cliente original: o servidor não muda; respeite os limites de pacote dele (registros de atalho
  mandados rápido demais podem ser descartados).
- Configurações num arquivo seu, nunca chaves novas nos ini do jogo.
- Teste numa cópia do cliente, nunca na pasta em que você joga; nenhum binário do jogo vai para o repositório.
- Commits pequenos, um assunto cada, com os juízes verdes.
