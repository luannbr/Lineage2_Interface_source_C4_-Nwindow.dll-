# NWindow.dll Lineage II C4 — восстановленный исходный код на C++

[Português](README.md) · [English](README.en.md) · **Русский**

Пофункциональное восстановление `NWindow.dll` клиента Lineage II **Chronicle 4 (Scions of Destiny)** — нативной
DLL интерфейса (окна, элементы управления, HTML диалогов NPC, IME, чат, консоль, мессенджер, реплеи). Цель —
оригинальная сборка от 13.01.2006 (TimeDateStamp `0x43C73652`, Visual C++ .NET 2002 `13.00.9466`, md5
`afab3ae3fa4f85f7f4c323715e5155e3`; 341 класс, 343 экспорта). Один и тот же код собирается двумя способами:

| Цель CMake | Результат | Что это |
|---|---|---|
| `NWindowDecomp` | `build/bin/Release/NWindowDecomp.dll` | **режим хуков**: работает рядом с оригинальным `NWindow.dll` и заменяет каждую уже восстановленную функцию `jmp` на нашу |
| `NWindowStandalone` | `build/standalone/Release/NWindow.dll` | **standalone**: весь `NWindow.dll` только из нашего кода, вместо оригинала |

Это четвёртый проект серии (C1 → C2 → C3 → C4): исходники C3, уже проверенные в игре, перенесены на C4 собственным
конвейером (`tools/port`) и доделаны вручную. Заметки C2 и C3 остаются в `docs/`, потому что метод, инструменты и
многие данные пришли оттуда.

## Как это работает

**Режим хуков.** Установщик (`tools/install_test.py`) добавляет в оригинальный `NWindow.dll` импорт
`NWindowDecomp.dll`. Она загружается раньше, чем выполняется любой код оригинала, и ставит `jmp` на вход каждой
восстановленной функции (макрос `NW_HOOK_*` рядом с каждым определением, `src/re/NwHook.h`). Всё остальное — по-прежнему
код оригинала. Любую функцию можно отключить в `NWindowDecomp.ini` (`[Hooks] Disable=`), чтобы локализовать ошибку.

**Standalone (этап 5).** С `NW_HOOK_MODE=0` макросы хуков пусты, и код сам собирает `NWindow.dll`: 341 `UClass`
(`src/standalone/Classes.gen.cpp`), глобальные переменные (`NwGlobals.cpp`), экспорты с оригинальными ординалами
(`src/standalone/NWindow.def`) и цветные курсоры (извлекаются из `orig/NWindow.dll` при сборке). Движок
(`Core.dll`/`Engine.dll`) загружает нашу DLL так же, как загружал оригинал.

**GameGuard.** Статическая библиотека nProtect GameGuard (`NPG*`, сторонний код) в standalone **не входит**: её точки
входа — задокументированные заглушки (`include/nwindow/NGameGuard.h`, `src/standalone/NwGameGuard.cpp`). Без неё вход
в C4 останавливался после `Init` (сервер входа ждёт `AuthGameGuard`); standalone отвечает на запрос сам, нулями
(`NConsoleWnd::GameGuardQueryX`, `src/nwindow/NConsoleWndC2.cpp`). NKeyCrypt (`npkcrypt.dll`) загружается через
`LoadLibrary`/`GetProcAddress`.

## Состояние

- **Режим хуков:** **5 456 из 7 202** функций, подлежащих восстановлению, перехвачены (**93,19 %** байт кода). Из
  оставшихся 587 — служебные (*scalar deleting destructors*, `InternalConstructor`), 1 038 существуют только в
  standalone (статическая инициализация, `DllMain`); 121 (34 КБ) остаются без хука, почти все — библиотека
  GameGuard, вне рамок проекта. Актуальный счёт: [docs/PROGRESSO.md](docs/PROGRESSO.md) (генерирует
  `tools/build_db.py`). Судьи зелёные: `hooks: 5456 installed, 0 errors`, `verify_hooks` 0 необъяснённых, `regcheck`
  0 проблем, `tools/port/checks.py` `CHECKS: ALL OK`, `verify_data` 0 расхождений. Сборки хуков до 5 387 функций
  входили в мир в игре без исключений.
- **Standalone:** линкуется (0 неразрешённых символов), проходит `verify_standalone.py` (PE, импорты, ресурсы,
  экспорты, классы и vtable совпадают с оригиналом) и `smoke_standalone.py` (все 9 этапов, вывод идентичен
  оригиналу); дифференциальный набор в эмуляторе (`tools/port/emu`, Unicorn) гоняется против него: 127 записей,
  35 204 случая, 0 расхождений. **В игре один только standalone `NWindow.dll` входит в аккаунт, показывает список
  серверов и входит в мир.** Окна, открываемые после этого, проверены в эмуляторе; проверка в клиенте окно за окном
  пока частичная (`Verificadas no jogo: 0` в базе).
- Технические заметки (`docs/`) — на португальском (заметки C4, написанные как руководства для агентов, — на
  английском); комментарии в коде — на английском.

## Требования

- Windows, **Visual Studio 2022** с C++ для x86 (MSVC v143), **CMake ≥ 3.20**;
- **Python 3** с `pefile` и `capstone` (`pip install pefile capstone`): нужны для сборки standalone (извлечение
  курсоров) и всем инструментам; `unicorn` (`pip install unicorn`) — для набора эмулятора;
- **ваш собственный клиент Lineage II C4**. В репозитории нет ни одного файла игры: скопируйте в `orig/` файлы из
  [orig/README.md](orig/README.md) (у `NWindow.dll` должен быть указанный там md5);
- для судей (`verify_hooks.py`, `regcheck.py`, `verify_standalone.py`, `build_db.py`, `tools/port/*`): экспорт Ghidra в
  `analysis/ghidra/`, его создаёт `tools/ghidra/run_full.sh` (Ghidra 11.3.2 и JDK 21; `NWD_GHIDRA_HOME` и
  `GHIDRA_JAVA_HOME`). Это долго; запускайте в фоне.

## Сборка

В *Developer Command Prompt* x86 VS 2022 (или после `vcvars32.bat`), в корне репозитория:

```bat
cmake -S . -B build -A Win32 -DNWD_GAME_SOURCES=ON
cmake --build build --config Release
cmake --build build --config Release --target NWindowStandalone
```

`-DNWD_GAME_SOURCES=ON` включает игровой код (`src/nwindow`); без него (значение по умолчанию, оставшееся с первого
этапа переноса) `NWindowDecomp` — это голый рантайм хуков, а `NWindowStandalone` не линкуется. Первый `--build`
собирает DLL режима хуков и тестовые инструменты (`nwd_smoke`, `nwd_difftest`) и не требует `orig/`;
`NWindowStandalone` требует `orig/NWindow.dll` и пишет ещё `NWindow.map` (для `tools/port/map_addr.py`). Предупреждения
C4244/C4838/C4005 известны (преобразования, повторяющие оригинал).

## Установка и проверка

Используйте **копию** папки `system` клиента (например `system_nwtest`), никогда не ту, из которой играете
(установщик отказывается работать с папкой с именем `system`). Команды ниже берут папку из `--system` или из
`NWD_CLIENT_SYSTEM`:

```bat
xcopy /E /I /Y "C:\...\Lineage II\system" "C:\...\Lineage II\system_nwtest"
set NWD_CLIENT_SYSTEM=C:\...\Lineage II\system_nwtest
```

**Режим хуков:**

```bat
python tools\install_test.py
python tools\install_test.py --restore
```

Первая команда записывает `NWindow.dll`, пересобранный из `orig/NWindow.dll` с дополнительным импортом,
`NWindowDecomp.dll` и `NWindowDecomp.ini` (бывший там `NWindow.dll` сохраняется как `NWindow.dll.pre-decomp-bak`);
вторая возвращает прежний файл. `NWindowDecomp.log` в этой папке перечисляет каждый установленный хук. В `.ini`:
`[Hooks] Enabled=0` оставляет весь оригинальный код; `Disable=` принимает адрес (`1000cc40`), метод
(`NCGDevice::DrawText`) или класс (`NCGDevice::*`).

**Standalone:** закройте игру, сохраните `NWindow.dll` тестовой папки и скопируйте поверх него
`build\standalone\Release\NWindow.dll`. Только этот файл (`npkcrypt.dll` клиента остаётся на месте). Если процесс
падает, standalone пишет `NWindow_crash.log` рядом с `l2.exe`: `python tools\port\map_addr.py --log NWindow_crash.log`
называет функции. Чтобы вернуться, положите оригинал обратно.

**Цикл** (сборка, smoke, судьи, установка, база; останавливается на первом неудачном шаге):

```bat
python tools\cycle.py --game-source --no-install
python tools\cycle.py --game-source --no-install --standalone
tools\port\cycle.bat
```

Вторая команда добавляет шаги standalone (объявления, сборка, verify, smoke). `tools\port\cycle.bat` сам вызывает
`vcvars32.bat` и запускает цикл режима хуков, а затем судей переноса (`verify_data.py`, `check_breakers.py`,
`checks.py`). Без `--no-install` цикл ещё и устанавливает сборку в тестовую папку (игра должна быть закрыта). Цикл
копирует из этой папки DLL, нужные загрузчику (`Core.dll`, `Engine.dll`, `npkcrypt.dll` и их зависимости), в
`build/smoke_system`.

## Пути машины

Ни один инструмент не содержит путей машины автора: папка вне репозитория берётся из параметра командной строки
или из одной из этих переменных (параметр важнее; инструмент, которому она нужна и которой нет, останавливается и
говорит, какая):

| Переменная | Указывает на | Кто использует |
|---|---|---|
| `NWD_CLIENT_SYSTEM` | папку `system` тестового клиента (копию: установка пишет в неё) | `--system` по умолчанию у `install_test.py`, `cycle.py`, `smoke_standalone.py`, `difftest.py`; `--log` у `verify_hooks.py` и `tools/port/verify_args.py`; файлы `.u` для `ue2_layouts.py` |
| `NWD_GHIDRA_HOME` | установку Ghidra (с `support/analyzeHeadless.bat`; `GHIDRA` по-прежнему важнее) | `tools/ghidra/*.sh`, `tools/c2/ghidra_census.sh` |
| `NWD_C3_ROOT` | рабочую копию проекта C3 (с её `orig/` и `analysis/ghidra/`): основа переноса | `tools/port/*` (`match`, `maps`, `layout`, `vslots`, `eligible`, `gen`...), `ue2_check_fields_by_code.py`, `ue2_disp_diff.py`, `ue2_guard_pairs.py`, `ue2_console_slots_c4.py` |
| `NWD_C1_ROOT` | рабочую копию проекта C1 | `ue2_vtable_diff.py`, `ue2_ret_diff.py`, `ue2_gamedata_regions.py`, `ue2_console_slots.py`, `tools/c2/compare_nwindow.py` |
| `NWD_C4_SERVER` | папку развёртывания сервера (страницы в `gameserver/data/html-en`) | `difftest.py`, набор страниц `server` (без неё пропускается) |
| `NWD_C2_SYSTEM`, `NWD_C3_SYSTEM` | папку `system` клиентов C2 и C3 | `tools/c2/packet_formats.py`; `tools/c3/*_formats.py`, `net_boundary.py`, `gen_recipe_ids.py` |
| `NWD_C3_SERVER_SRC`, `NWD_C3_SPEC`, `NWD_DAT_CRYPT`, `NWD_JAVAC` | исходники тестового сервера C3, спецификацию `C3 Rise Of Darkness.html`, `oldclient_crypt.py` редактора `.dat`, `javac` | инструменты аудита пакетов C3 (`tools/c3/`) |

## Судьи

«Зелёный» — каждый завершается с кодом 0; цикл запускает первые четыре (и судей standalone с `--standalone`),
`tools\port\cycle.bat` добавляет судей переноса.

| Судья | Команда | Что доказывает |
|---|---|---|
| smoke | `build\smoke\Release\nwd_smoke.exe build\smoke_system` | пропатченный `NWindow.dll` загружается вне игры, и каждый хук сообщает `ok` (`NWindowDecomp.log`: `hooks: 5456 installed, 5456 registered, 0 errors`) |
| verify | `python tools/verify_hooks.py --log build/smoke_system/NWindowDecomp.log` | каждая наша функция против оригинальной на уровне ассемблера: байты, снимаемые `ret`, слоты vtable и вызываемые импорты; расхождение допускается только с объяснением в `db/verify_notes.csv` |
| regcheck | `python tools/regcheck.py --game-src` | ни один оригинальный вызывающий код не полагается на регистр, который наша функция может изменить (оригинал собран с `/GL`) |
| db | `python tools/build_db.py --game-src` | заново генерирует `db/functions.csv` и `docs/PROGRESSO.md` |
| verify_data | `python tools/port/verify_data.py` | константы и смещения данных каждой функции совпадают с оригиналом (`db/verify_data_notes.csv` объясняет несовпадающие) |
| check_breakers | `python tools/port/check_breakers.py` | ни одна перехваченная функция не доходит до `NW_UNPORTED()` (`int3`) |
| checks | `python tools/port/checks.py` | 15 статических проверок, перечисленных в заголовке файла (возврат в `eax`/`st0`, `ret N` заглушек, кадры стека, размеры, константные аргументы, CRT, адреса, «сырые» смещения, литералы, виртуальные слоты, `vt_audit`, `vt_slots`...): `CHECKS: ALL OK` |
| standalone: объявления | `python tools/verify_standalone.py --checks source` | наши `NW_DECLARE_CLASS` / `NW_CHECK_VTABLE` против оригинальной таблицы классов |
| standalone: verify | `python tools/verify_standalone.py --dll build/standalone/Release/NWindow.dll` | PE, импорты, ресурсы, экспорты, классы и vtable совпадают с оригиналом, без запуска |
| standalone: smoke | `python tools/smoke_standalone.py --dll build/standalone/Release/NWindow.dll` | загрузка так, как это делает движок: 341 класс зарегистрирован, `L2Console`/`L2Font` созданы, вывод равен оригинальному |
| эмулятор | `python tools/port/emu/run_suite.py --mode hook` / `--mode sa --cases 30 --seed 1` | части DLL выполняются в Unicorn с оригинальным кодом и с нашим (хуки или standalone) и записывают одно и то же ([docs/notas/C4_EmuSA.md](docs/notas/C4_EmuSA.md)) |

`tools/difftest.py` (фиксированные случаи против `tests/golden/`) всё ещё содержит адреса C1 и отказывается работать с
образом C4; в C4 дифференциальный тест — это набор эмулятора. Цикл **не** рисует интерфейс: визуальные изменения
видны только при проверке в игре.

## Структура

```
orig/          бинарные файлы вашего клиента (не в git; md5 в orig/README.md)
analysis/      факты из оригинала (классы, EH, vtable и раскладки Core/Engine); analysis/c2/, analysis/c3/ = те же
               данные для C2 и C3; analysis/ghidra/ = экспорт Ghidra (генерируется, не в git)
db/            база функций (functions.csv генерируется) + имена, заметки, проверки и карты переноса, заданные вручную;
               db/c1/, db/c2/, db/c3/ = списки предыдущих проектов
include/ue2/   объявления Core.dll/Engine.dll C4, сверенные с экспортами (gen/*.vt.inc генерируются)
include/nwindow/, src/nwindow/    декомпилированный код, один класс на файл
src/re/        рантайм режима хуков (установка хуков, лог, ini, совместимость CRT)
src/standalone/                   IMPLEMENT_CLASS / IMPLEMENT_PACKAGE, глобальные, NWindow.def, заглушки GameGuard, лог падений
tests/         эталонные выводы оригинала (golden) и списки standalone
third_party/ue_import/            .def экспортов Core/Engine (при сборке превращаются в .lib)
tools/         анализ, генерация, установка, судьи, цикл; tools/port/ = конвейер C3 -> C4, судьи переноса и эмулятор
               (tools/port/emu); tools/c2/, tools/c3/ = унаследованные инструменты (перепись функций, аудит пакетов)
docs/          план, соглашения, прогресс и технические заметки
```

## Руководства

- [docs/PLANO_C4.md](docs/PLANO_C4.md) — специфика C4 (константы образа, решения, этапы);
  [docs/PLANO.md](docs/PLANO.md) — метод; [docs/CONVENCOES.md](docs/CONVENCOES.md) — правила кода и ловушки
- [docs/notas/C4_Standalone.md](docs/notas/C4_Standalone.md) — этап 5 (standalone);
  [docs/notas/C4_EmuSA.md](docs/notas/C4_EmuSA.md) — набор эмулятора против standalone;
  [docs/notas/C4_Agentes.md](docs/notas/C4_Agentes.md) — руководство волн ручного переноса
- [docs/MODIFY_INTERFACE.ru.md](docs/MODIFY_INTERFACE.ru.md) — как изменять интерфейс (панель ярлыков, кнопка, окно, цвет чата, строка подсказки)
  · [Português](docs/MODIFICAR_INTERFACE.md) · [English](docs/MODIFY_INTERFACE.md)
- [docs/LLM.md](docs/LLM.md) — руководство для ИИ-агентов, работающих с этим кодом (на английском)
- Проект C1, откуда пришли метод и инструменты, опубликован отдельно (со своими функциями фазы 6).

## Правовая информация

Независимый проект обратной разработки ради совместимости и сохранения. Не связан с NCSoft, не уполномочен и не
одобрен ею. *Lineage II* — зарегистрированная торговая марка NCSoft Corporation. **Никакие оригинальные файлы игры
не включены** (ни DLL, ни исполняемые файлы, ни пакеты данных): каждый пользователь использует свой клиент.
Библиотека nProtect GameGuard не воспроизводится.

## Лицензия

[GNU General Public License v3.0](LICENSE).
