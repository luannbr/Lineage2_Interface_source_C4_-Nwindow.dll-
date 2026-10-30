# Как изменять интерфейс

[Português](MODIFICAR_INTERFACE.md) · [English](MODIFY_INTERFACE.md) · **Русский**

Практическое руководство по изменению интерфейса C4 в этом коде: как устроены окна, куда подключается новый код, и
пошаговые рецепты (новая панель ярлыков, кнопка, окно, цвет чата, строка подсказки). Изменения интерфейса попадают в
сборку **standalone** (`NWindowStandalone`) под вашим собственным ключом компиляции; режим хуков и обычный standalone
остаются верными оригиналу, и именно там судьи сравнивают с ним.

## 1. Как устроены окна

### Иерархия

| Класс | Файл | Роль |
|---|---|---|
| `NCObject : UObject` (0x34 байта) | `include/nwindow/NCObject.h` | основа всего; `FindTexture(TEXT("Пакет.Группа.Имя"))` загружает текстуру |
| `NCWnd` (0xE8 байт, 74 слота vtable) | `include/nwindow/NCWnd.h` | любое окно: позиция (`LocalX/Y` относительно родителя, `ScreenX/Y`, `Width`, `Height`), стиль, дочерние окна, фокус, заголовок (`FrameCtrl`), значок свёрнутого окна |
| `NControl : NCWnd` (0x1F4, 75 слотов) | `include/nwindow/NControl.h` | любой элемент управления; имеет `ID` и сообщает родителю через `NWM_COMMAND` (wParam = `ID`) |
| `NCButton`, `NCPushButton`, `NCExButton`, `NCRadioButton` | `include/nwindow/NCButton.h` ... | кнопки с обычной/нажатой текстурой (`SetNormalTex`, `SetPushedTex`, `SetText`, `SetTextID`) |
| `NCEditBox`, `NCListBox`, `NCScrollWnd`, `NCComboBox`, `NCTabControl`, `NCSliderBar`, `NCProgressCtrl`, `NCListCtrl`, `NCMultiEdit`, `NCHtmlViewer`... | `include/nwindow/` | остальные элементы управления |
| `NC*Wnd` | `include/nwindow/` | игровые окна (инвентарь, чат, панель ярлыков, статус...) |
| `NCVirtualWndMain` | `include/nwindow/NCVirtualWndMain.h` | владелец всех окон: регистрация, z-порядок, попадание мыши, фокус, рассылка сообщений, курсоры |
| `NConsoleWnd : UL2ConsoleWnd` (0x3CA4, 448 слотов) | `include/nwindow/NConsoleWnd.h` | консоль движка (сетевые события и события UI); `GConsole` |
| `NCConsole` (0x2A0) | `include/nwindow/NCConsole.h` | игровой **рабочий стол** (`GConsole->GetDesktop()`): родитель всех игровых окон, с указателем на каждое (`ChatWnd` +0xF4, `InventoryWnd` +0x120, `SystemMenuWnd` +0x148, `ShortCutWnd` +0x15C...) |

Размеры зафиксированы: они зарегистрированы в Core (размер `UClass`, `analysis/classes.json`), и оригинальный код и
движок обращаются по смещениям внутри них. `NW_ASSERT_SIZE` / `NW_ASSERT_OFFSET` в каждом заголовке это контролируют.

### Создание окна или элемента управления

Образец из оригинального кода (`NCSystemMenuWnd::OnCreate` в `src/nwindow/NCSystemMenuWnd.cpp`):

```cpp
NCButton* Button = new NCButton(TEXT("L2UI_CH3.SystemMenuWnd.systemicon1"),
                                TEXT("L2UI_CH3.SystemMenuWnd.systemicon1_down"), 0);
Button->ID = NID_SOMETHING;          // то, что придёт в NWM_COMMAND родителя
Button->SetPosition(13, 29, 36, 36); // относительно родителя (C4: виртуальный, с ещё 3 аргументами — привязка к экрану)
CreateChild(Button, NWS_VISIBLE);    // регистрирует, делает позицию абсолютной, шлёт WM_CREATE -> OnCreate
```

- `new` для класса пакета — это `operator new` UE2 (объект — `UObject`, выделяется размером своей зарегистрированной
  `UClass`).
- `CreateChild(child, style)` (`NCWnd`, inline) = `AddChildWindow` + `AttachEntry` + экранная позиция + `WM_CREATE`.
  Рабочий стол регистрирует свои окна вручную (`NCConsole::OnCreate`: `AddChildWindow(W, style)`, `HideWindow`,
  `AttachEntry`).
- Стили (`NWS_*` в `NCWnd.h`): `NWS_VISIBLE`, `NWS_TOPMOST` (отдельный список, поверх), `NWS_TITLEBAR` (`NCWnd::OnCreate`
  создаёт заголовок `NCFrameCtrl`), `NWS_MINIMIZABLE`, `NWS_NOCLOSE`, `NWS_CLIPTOPARENT`...
- Уничтожение: в `OnDestroy` владельца `RemoveChildWindow(Child->Entry); delete Child;` (см.
  `NCSystemMenuWnd::OnDestroy`). `NCWnd::OnDestroy` уничтожает только заголовок и значок.

### Сообщения, отрисовка, подсказки

- Всё приходит через `WindowProc(Msg, wParam, lParam)` (слот 25) с номерами сообщений Win32; `NCWnd` раздаёт их
  виртуальным функциям (слоты C4): `OnTimer` (36), `OnCreate` (37), `OnDestroy` (38), `ShowWindow` (48), `HideWindow`
  (49), `MoveWindow` (51), `OnPaint(NCGDevice*)` (62), `OnDrawToolTip` (63), `OnChar` (64), `OnKeyDown` (65),
  `OnMouseMove` (67), `OnLButtonDown/Up` (68/69), `OnRButtonDown/Up` (70/71), `OnMove` (72), `OnMouseWheel` (73).
- Элемент управления сообщает родителю через `NWM_COMMAND` (`0x501`), `wParam` = `ID` элемента; родитель обрабатывает
  это в своём `WindowProc` (пример: `NCSystemMenuWnd::WindowProc`).
- Отрисовка (`OnPaint`) идёт через `NCGDevice` (`include/nwindow/NCGDevice.h`): `SetClip(ScreenX, ScreenY, W, H)`,
  `BeginPaint(ScreenX, ScreenY)` (дальше координаты относительные), `DrawTexture(x, y, w, h, u, v, ul, vl, Tex)`,
  `DrawText(x, y, цвет, текст, ..., L2FONT_Normal, ...)`, `EndPaint`, `ReleaseClip`. Текстуры загружаются один раз в
  `OnCreate` через `FindTexture` (пакеты клиента `L2UI*`; в C4 есть набор `L2UI_CH3.*`). См. `NCSystemMenuWnd::OnPaint`.
- Подсказка: `OnDrawToolTip(NCGDevice*)` окна под мышью, вызывается после отрисовки.

### Консоль и рабочий стол

`NConsoleWnd` получает события сервера и игры и владеет рабочим столом `NCConsole`. `NCConsole::OnCreate`
(`src/nwindow/NCConsole.cpp`) создаёт все игровые окна (позиции по размеру экрана), `NCConsole::OnDestroy` их уничтожает,
`NCConsole::Reset` выполняется, когда игрок выходит из игры. `NConsoleWnd::Tick` вызывается каждый кадр;
`NConsoleWnd::HookShortCutKey` первым видит клавиши (F1..F12, Alt+буква, 1..= в режиме «Enter для чата»).

## 2. Куда подключается новый код

В этом дереве нет собственного слоя функций. Блоки `#if NW_FEATURES` в `src/nwindow` — это точки вызова функций
проекта C1, унаследованные через форки: здесь ничто не определяет `NW_FEATURES`, а `src/features/` не существует, так
что они компилируются в ничто. Для C4 используйте схему отдельного эксперимента, описанного ниже:

| Часть | Где | Что делает |
|---|---|---|
| ваш ключ, например `NW_CUSTOM=1` | `CMakeLists.txt`, `target_compile_definitions(nwd_standalone_objs ...)` | определён только для объектов standalone; цель режима хуков его не видит, так что `verify_hooks`/`checks.py` продолжают сравнивать верный оригиналу код |
| ваши исходники | `src/custom/*.cpp` (+ `file(GLOB NWD_CUSTOM_SOURCES ...)`, добавленный в `nwd_standalone_objs`), заголовки в `include/nwindow/` | код функции в стиле исходников |
| точки вызова | внутри оригинальных функций, `#if NW_CUSTOM ... #endif` с `// custom: ...` | единственное изменение декомпилированного кода; оригинальная ветка остаётся как есть |
| точки входа | `NCConsole::OnCreate` (после оригинальных окон), `NCConsole::OnDestroy` (до них), `NCConsole::Reset`, `NConsoleWnd::Tick`, `NConsoleWnd::HookShortCutKey` | создание / уничтожение / сброс / работа каждый кадр / клавиши |
| настройки | собственный файл рядом с `l2.exe` (`GetModuleFileName` + `GetPrivateProfileInt/String`) | никогда не новые ключи в ini игры |
| модульный тест | `add_executable` для логики, не зависящей от движка (без игрового кода, те же флаги) | то, что можно проверить вне игры |

**Классы.** Новый класс окна, который **не добавляет полей**, может жить под оригинальной `UClass`:
`static UClass* StaticClass() { return NCWnd::StaticClass(); }` (или оригинальный класс, который он расширяет),
`operator new`, вызывающий `StaticAllocateObject(StaticClass(), ...)`, и `NW_ASSERT_SIZE`, равный размеру базы. Тогда
таблица классов, экспорты, `verify_standalone` и `smoke_standalone` по-прежнему видят ровно оригинальные классы.
Состояние — в статических переменных вашего `.cpp`. Классу с собственными полями нужны своя `UClass` и свой экспорт,
которые судьи standalone отвергнут, пока их не научат этому списку (проект C1 делал это через `NW_DECLARE_L2_CLASS` и
`tests/standalone/l2_extras.csv`).

## 3. Рецепт: новые панели ярлыков (как в Interlude/Essence)

Здесь описан подход, реализованный для C4 в **отдельном эксперименте** (собственный репозиторий разработки, ветка
`feature/barras-potions`, под `#if NW_CUSTOM`; в этот код он **не входит**). Он собрался и прошёл проверки standalone вне
игры (`smoke_standalone` 9/9, `verify_standalone` OK, собственный модульный тест); проверка в игре идёт по его
собственному сценарию.

**Факты, на которые он опирается.** Панель C4 — один `NCShortCutWnd` (`include/nwindow/NCShortCutWnd.h`, рабочий стол
`+0x15C`), создаваемый в `NCConsole::OnCreate` как `new NCShortCutWnd(12, 10, 32)`: 12 слотов × 10 страниц, 120 слотов
в `Items` (`+0x200`), состояние клавиш в `Keys` (`+0x1FC`), показанная страница в `Page` (`+0x270`). Слот ярлыка —
`Page * 12 + позиция`, плоский номер, который сервер хранит и возвращает. Сервер принимает страницы 0..11, поэтому
панелям, показывающим страницы 0..9, изменения сервера не нужны.

**Идея.** Дополнительная панель **не владеет слотами**: `NCExtraShortCutWnd : NCShortCutWnd` не добавляет полей,
живёт под `UClass` основной панели и показывает другую страницу той же таблицы из 120 слотов.

1. **Создание** набора в `NCConsole::OnCreate` после оригинальных окон (`NCustomFeatures::CreateWindows(this)` под
   `#if NW_CUSTOM`). В `OnCreate` дополнительной панели, после того как оригинальный `OnCreate` создал свои таблицы,
   освободить их и указать на таблицы основной (`Items = Main->Items; Keys = Main->Keys;`); `Page` у панели свой
   (страницы по умолчанию 1..5).
2. **Уничтожение** в `NCConsole::OnDestroy`, **до** оригинальных окон: `OnDestroy` дополнительной панели сначала
   обнуляет `Items` и `Keys` (они принадлежат основной; иначе освобождаются дважды), затем оригинальный `OnDestroy`.
3. **Одни часы.** Таймеры перезарядки тикают только у основной панели (`NConsoleWnd::Tick` вызывает её `Tick`);
   дополнительная только рисует их. Шесть тикающих панелей ускорили бы таймеры вшестеро.
4. **Следовать за основной** каждый кадр (`NConsoleWnd::Tick` → `Tick` набора): та же ориентация (`bHorizontal`, и
   `UpdateLayout()` при её смене), стопкой слева от вертикальной панели или над горизонтальной (с другой стороны, если
   на экране нет места), скрыты, когда скрыта она. Перетаскивание дополнительной панели двигает основную (`MoveWindow`
   передаёт), и набор следует за ней.
5. **Перетаскивание ничего не требует**: каждое место сброса (инвентарь, кукла, действия, умения, макросы, рецепты, сама
   панель) спрашивает `IsA(NCShortCutWnd::StaticClass())`, а затем `SlotAtPoint(X, Y)`, который возвращает глобальный слот
   `Page * 12 + i`; регистрация уходит на сервер с этим слотом. Щелчок по слоту использует тот же глобальный индекс.
6. **Изменения данных — только у основной**: `SetShortCut` / `DeleteShortCut` / `ClearShortCuts` / `UpdateItem`
   выполняются только там (`ReceiveShortCutList`, `AddShortCutItem`... консоли и так вызывают только основную);
   дополнительные только перерисовываются.
7. **Кнопки.** Кнопка Expand на основной панели (дочерний `NCPushButton` с новым `ID` после оригинальных, `0xFF00000A`;
   собственные идут до `NID_SHORTCUT_PAD` `0xFF000009`), обрабатывается в ветке `NWM_COMMAND` `NCShortCutWnd::WindowProc`:
   по одной панели за щелчок, после пятой все скрываются. У каждой дополнительной панели остаются стрелки страниц
   (`OnNextClicked`/`OnPrevClicked`, страницы 0..9 по кругу); кнопки поворота и геймпада создаются (оригинальная
   раскладка их ожидает), но остаются скрытыми.
8. **Клавиши.** В `NConsoleWnd::HookShortCutKey`, до оригинального кода: при чате в режиме «Enter для чата» 1..0,-,=
   идут на первую дополнительную панель (иначе они повторяют F1..F12), а Q..P,[,] — на вторую, пока эти панели видны.
   Подписи рисуются поверх текстур клавиш F в `OnPaint` дополнительной панели.
9. **Сохранение** в собственном ini клиента (`system\NWindowCustom.ini`): сколько панелей показано и страница каждой.

Тот же эксперимент добавил окно **автоматических зелий** (`NCAutoPotionWnd`, под `UClass` `NCWnd`, без полей): по слоту
на вид зелья, заполняемому из инвентаря; правый щелчок включает слот; таймер 200 мс сравнивает HP/MP/CP с порогом для
персонажа и вызывает `NConsoleWnd::RequestUseItem(ObjectID)` после минимального интервала каждого вида; сброс предмета
из инвентаря на окно — одна ветка `#if NW_CUSTOM` в коде отпускания `NCInvenItemWnd`.

## 4. Рецепт: новая кнопка в существующем окне

Пример: ещё одна кнопка в системном меню (`NCSystemMenuWnd`).

1. Ваши файлы (`src/custom/MyButton.cpp`, `include/nwindow/MyButton.h`): `MyButton_Create(NCWnd* Owner)`,
   `MyButton_OnCommand(NCWnd* Owner, WPARAM ID)`, `MyButton_Destroy(NCWnd* Owner)`; указатель на кнопку — статическая
   переменная `.cpp` (никогда не новое поле `NCSystemMenuWnd`).
2. Создать в конце `NCSystemMenuWnd::OnCreate`, в `#if NW_CUSTOM`: `new NCButton(normal, pushed, 0)`, `ID`, который не
   использует ни один элемент этого окна (см. перечисление `NID_SYSMENU_*`), `SetPosition` внутри окна,
   `Owner->CreateChild(Button, NWS_VISIBLE)`.
3. Обработать щелчок в ветке `NWM_COMMAND` `NCSystemMenuWnd::WindowProc`:
   `if (MyButton_OnCommand(this, wParam)) return 1;` (в `#if NW_CUSTOM`).
4. Уничтожить в `NCSystemMenuWnd::OnDestroy`, до `NCWnd::OnDestroy()`: `RemoveChildWindow(Button->Entry);
   delete Button;`.
5. Если окно должно стать больше, измените его `SetPosition` в `NCConsole::OnCreate` и фон в `OnPaint` — всегда в
   `#if NW_CUSTOM`, не трогая оригинальную ветку.

## 5. Рецепт: новое окно

1. Класс, производный от `NCWnd` (или от оригинального окна), который **не добавляет полей** (раздел 2): состояние — в
   статических переменных его `.cpp`, `NW_ASSERT_SIZE(NCMyWnd, 0xE8)` как доказательство.
2. Переопределите нужные виртуальные функции: `OnCreate` (сначала `NCWnd::OnCreate()`, текстуры через `FindTexture`,
   заголовок и элементы управления), `OnDestroy` (уничтожить дочерние, затем `NCWnd::OnDestroy()`), `WindowProc`
   (`NWM_COMMAND`), `OnPaint`, `OnDrawToolTip`, `OnTimer`. Сверяйтесь с комментариями слотов в `NCWnd.h`: переопределение с
   другой сигнатурой становится новым слотом и ломает класс.
3. Создавайте его из `NCConsole::OnCreate` (ваша точка входа, после оригинальных окон) с регистрацией рабочего стола
   (`SetPosition`, `AddChildWindow(W, style)`, `AttachEntry`, `HideWindow`) и уничтожайте из `NCConsole::OnDestroy` до
   оригинальных. Показывайте через `ShowWindow()` и `SetHeadWindow(Entry)` (`BringChildToTop`).
4. Образцы: `NCSystemMenuWnd` (заголовок `NCFrameCtrl(1, 0, 0)`, кнопки, фон, текст) и панель ярлыков (`NCShortCutWnd`:
   текстуры, кнопки, раскладки по ориентации, подсказка).

## 6. Рецепт: новый цвет чата

Сервер не присылает цвет: он присылает **тип** сообщения (`Say2`), а цвет выбирает клиент в `NConsoleWnd::Say2`
(`src/nwindow/NConsoleWndEvents.cpp`, оригинал `0x1007d200`). Незнакомые C4 типы попадают в последний `else` и рисуются
`0xFFDCDCDC` на общей вкладке (тип 0). Реплики NPC идут через `NConsoleWnd::NpcSay` сразу ниже.

1. Добавьте свой тип перед последним `else`, в `#if NW_CUSTOM`:
   `else if (Type == 18) GetDesktop()->Say(ObjectID, Name, Text, 0xFF408CFF, Type);`, а ветка `#else` пусть оставляет
   оригинальную цепочку без изменений (или поместите всю цепочку в обе ветки).
2. Последний аргумент — тип, под которым окно чата раскладывает строку (вкладки и фильтры); тип, незнакомый окну чата,
   может не появиться ни в одной вкладке, кроме общей: тогда передавайте 0, как оригинальный `else`.
3. Не используйте повторно типы с побочными эффектами (TELL добавляет говорящего в список ответа; ANNOUNCEMENT заменяет
   имя).
4. Изменить цвет типа, который C4 **уже** раскрашивает, значит изменить верную оригиналу ветку: делайте это под
   `#if NW_CUSTOM` внутри `Say2`, сохраняя оригинальный цвет в ветке `#else`.

## 7. Рецепт: новая строка в подсказке предмета

Подсказку предмета в сетках предметов рисуют функции `include/nwindow/NItemToolTip.h` (страницы инвентаря, кукла,
обмен, склад, частные лавки, инвентарь питомца): `SetTexts`/`SetStoreTexts` заполняют `ToolTipName` и `ToolTipText`
(текст, который задаёт **размер** рамки), а `DrawLines` рисует строки с шагом 13 пикселей (`DrawTypeLines`, затем
`DrawTailLines`, которая заканчивается описанием).

1. Добавьте строку в обоих местах, в `#if NW_CUSTOM`: в тексте, задающем размер (чтобы рамка выросла), и в отрисовке
   (например, в `DrawTailLines` перед описанием, через `DrawLabel`/`DrawValue` и `Y += 13`).
2. Эти функции — шаблоны, которые каждая сетка инстанцирует с разными параметрами: проверьте каждую изменённую копию.
3. Подсказки других окон — это их собственный `OnDrawToolTip` (слот 63): добавьте высоту там, где она вычисляется, а
   затем нарисуйте строку, оба места под ключом.

## 8. Цикл сборки и проверки

```bat
cmake -S . -B build -A Win32 -DNWD_GAME_SOURCES=ON
cmake --build build --config Release --target NWindowStandalone
python tools\verify_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\smoke_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\cycle.py --game-source --no-install
```

1. Собирайте `NWindowStandalone`, пока не останется ни ошибок, ни новых предупреждений.
2. `verify_standalone` и `smoke_standalone` должны по-прежнему проходить: с классами без новых полей таблица классов и
   вывод, сравниваемый с оригиналом, не меняются. Если новое окно меняет вывод smoke, объясните это и обоснованно
   поправьте сравнение; никогда не скрывайте расхождение.
3. Запустите цикл режима хуков: он доказывает, что верный оригиналу код не изменился (ключ там не определён).
4. Проверка в игре: закройте игру, сохраните `NWindow.dll` **копии** папки `system` и скопируйте поверх него
   `build\standalone\Release\NWindow.dll`. Инструменты не рисуют интерфейс: положение, графика и перетаскивание видны
   только в игре. При падении: `NWindow_crash.log` рядом с `l2.exe` + `python tools\port\map_addr.py --log
   NWindow_crash.log`.

## 9. Правила безопасности

- Никогда не добавляйте поля или новые виртуальные слоты в оригинальный класс: его размер зарегистрирован в движке, а
  оригинальный код обращается по смещениям в нём и в его vtable. Переопределять уже существующий слот можно.
- В декомпилированном коде — только точка вызова, под вашим ключом с комментарием `// custom: ...`; оригинальная ветка
  остаётся как есть. Сборка режима хуков никогда не получает этот ключ.
- Проверяйте каждое имя текстуры по пакетам клиента C4 перед использованием (текстуры Interlude/Essence в C4 часто
  отсутствуют).
- Тот же протокол, что у оригинального клиента: сервер не меняется; соблюдайте его ограничения на пакеты (слишком
  частые регистрации ярлыков могут отбрасываться).
- Настройки — в собственном файле, никогда не новые ключи в ini игры.
- Проверяйте на копии клиента, никогда в папке, из которой играете; никакие бинарные файлы игры не попадают в
  репозиторий.
- Небольшие коммиты, по одной теме, с зелёными судьями.
