# Desktop Ruler 0.2.2

Desktop Ruler measures distances, paths, areas, perimeters, angles, and counts on a frozen screen capture or a local image. It is an independent implementation inspired by workflows described in the [Universal Desktop Ruler documentation](https://avpsoft.com/products/udruler/); it does not include AVPSoft source code or artwork.

## Build and run

Build with a C++17 compiler, `make`, `pkg-config`, and Qt 6 Widgets and Qt 6 DBus development files. Qt 6 Test is also needed for `make test`. Screen capture requires `grim` and a compatible Wayland compositor; opening a local image does not.

```sh
make
make test
./desktop-ruler
./desktop-ruler --image /path/to/image.png
./desktop-ruler --tray
```

Run `./desktop-ruler --version` to check the binary version. To add a user-local application menu entry, run `./install.sh install` (default prefix: `~/.local`). Remove it with `./install.sh uninstall`.

## Use

Choose **Active screen** (default) or a specific output and press **Capture screen**, or **Open image**. Select a mode and press **Measure**. If no image is loaded, Measure starts capture first. On supported Hyprland versions, the controls hide and a small pinned picker follows you across workspaces: switch to the target workspace, then press **Take here**. The picker disappears before the screenshot. **Cancel** restores the controls without changing the current image or results. On other Wayland compositors, or if Hyprland cannot pin the picker, it offers a clearly marked 5-second switch window and an immediate **Take here** action. The image stays frozen: **Next measurement** reuses it, while **New capture** updates it. The controls window is compact and floating when Hyprland's runtime window commands are available. Only settings relevant to the selected mode appear; magnifier zoom is under **Magnifier options**.

On the image, left-click points or drag to trace a curve. For a distance or calibration, left-click the start and right-click the endpoint; the right click records that endpoint and finishes. Add intermediate points with left click for a longer path. **Done** or Enter finishes an already marked path. For a closed area or perimeter, mark at least three points. **Undo**, Backspace, or Ctrl+Z removes the last point or stroke; **Cancel** or Escape returns to the controls. Tab hides/shows the canvas buttons so pixels beneath them remain reachable. For an angle, left-click the vertex and first ray, then right-click the second ray to finish; you can also place all three points with left clicks and finish with a right click. Count mode numbers clicks. Automatic color area starts from a clicked pixel; RGB tolerance controls how far each channel may differ from that seed.

Pixel values refer to pixels in the image. To calibrate, left-click the start and intermediate points, then right-click the endpoint. In the dialog, enter **Known length** and **Unit**, then press **Apply**. The last applied scale and unit are restored when the app restarts; cancelling keeps them unchanged, and **Reset scale** returns to pixels across restarts. Calibration does not estimate physical monitor DPI. Manually traced area and perimeter use the contour. Automatic color-region area counts occupied pixels, and its perimeter counts exposed pixel edges, including holes.

After measuring, the result and its editable name appear above **Next measurement**. Open **History (N)** to view past results; double-click a history label (or press F2) to rename it. Copy a selected row or all rows as TSV, export CSV, or save a marked result as a cropped PNG. **Full image PNG** saves the unmarked source. **Region PNG** lets you select and save a rectangular part of the current frozen image. History lasts until cleared or quit; it is not retained between runs.

**Shortcuts** edits key bindings for modes, capture, and history. The default local bindings are Ctrl+Alt+1 through Ctrl+Alt+8 for modes, Ctrl+Alt+C for capture, and Ctrl+Alt+H for history. Local shortcuts work while the controls window is active and focus is outside text inputs. The Shortcuts dialog can optionally request global shortcuts through the desktop portal for the current run. The compositor may ask for approval or different bindings, or may not support the portal; global shortcuts are not guaranteed. On Hyprland, registration exposes actions but does not assign keys; use the compositor’s [global binding setup](https://wiki.hypr.land/configuring/core/binds/globals/) with `desktop-ruler:mode_0` through `mode_7`, `desktop-ruler:capture` or `desktop-ruler:history`. Local bindings persist between runs, while global activation must be requested again after restart.

`--tray` starts with only the tray icon when a system tray is available. If a system tray is available, closing the controls window hides it in the tray; **Quit** exits the app. Screen capture targets one selected output and depends on `grim` support in the active Wayland session. This design measures a frozen image, not a changing live screen.

## License

This application's source is licensed under the MIT License; see [LICENSE](LICENSE). Qt and other system libraries are separate dependencies with their own licenses. Qt's open-source and commercial licensing options are described in the [official Qt licensing documentation](https://doc.qt.io/qt-6/licensing.html).

---

# Экранная линейка 0.2.2

Desktop Ruler измеряет расстояния, пути, площади, периметры, углы и количество объектов на неподвижном снимке экрана или в локальном изображении. Это независимая реализация процессов, описанных в [документации Universal Desktop Ruler](https://avpsoft.com/products/udruler/); исходный код и графические материалы AVPSoft не используются.

## Сборка и запуск

Для сборки нужны компилятор C++17, `make`, `pkg-config`, файлы разработки Qt 6 Widgets и Qt 6 DBus. Для `make test` дополнительно нужен Qt 6 Test. Захват экрана требует `grim` и совместимого композитора Wayland; открытие локального файла работает без него.

```sh
make
make test
./desktop-ruler
./desktop-ruler --image /путь/к/изображению.png
```

Версию бинарного файла показывает `./desktop-ruler --version`. Для ярлыка в меню текущего пользователя выполните `./install.sh install` (по умолчанию в `~/.local`). Удаление: `./install.sh uninstall`.

## Работа

Выберите **«Текущий экран»** (по умолчанию) или конкретный выход и нажмите **«Захват экрана»** либо **«Открыть файл»**. Выберите режим и нажмите **«Измерить»**. Если изображения ещё нет, эта кнопка сначала откроет выбор захвата. В поддерживаемой версии Hyprland основное окно скроется, а небольшое закреплённое окно будет доступно на других рабочих столах: перейдите к нужному и нажмите **«Снять здесь»**. Перед снимком окно выбора исчезнет. **«Отмена»** возвращает управление без изменения снимка и результатов. В других Wayland-композиторах либо при ошибке закрепления окно явно показывает отсчёт 5 секунд для перехода и кнопку немедленного снимка. Снимок остаётся неподвижным: **«Следующее измерение»** использует его снова, а **«Новый снимок»** обновляет. Основное окно компактное и плавающее при доступности команд управления окнами Hyprland. Показываются только нужные для режима настройки; увеличение лупы находится в **«Настройках лупы»**.

На изображении ставьте точки левой кнопкой или ведите мышью для кривой. Для расстояния или калибровки левой кнопкой отметьте начало, правой — конец: она сохранит конечную точку и завершит измерение. Промежуточные точки пути добавляйте левой кнопкой. **«Готово»** или Enter завершают уже отмеченный путь. Для замкнутой площади или периметра нужны минимум три точки. **«Отменить»**, Backspace или Ctrl+Z убирают последнюю точку либо штрих; **«Назад»** или Escape возвращают к настройкам. Tab скрывает/показывает кнопки на холсте, чтобы можно было отметить пиксели под ними. Для угла левой кнопкой отметьте вершину и первый луч, а правой — второй луч и завершение; также можно поставить все три точки левой и завершить правой. Режим подсчёта нумерует щелчки. Автоматическая область начинается с выбранного пикселя; допуск RGB задаёт допустимое отклонение каждого канала от его цвета.

Пиксельные значения относятся к пикселям изображения. Для калибровки левой кнопкой отметьте начало и промежуточные точки, правой — конец. В появившемся окне заполните **«Известная длина»** и **«Единица измерения»**, затем нажмите **«Применить»**. Последние применённые масштаб и единица восстанавливаются после перезапуска; отмена их не меняет, а **«Сброс масштаба»** возвращает пиксели и после перезапуска. Калибровка не определяет физический DPI монитора. Ручная площадь и периметр рассчитываются по контуру. Автоматическая область считает занятые пиксели, а её периметр — открытые стороны пикселей, включая границы отверстий.

После измерения результат и поле его названия появляются над кнопкой **«Следующее измерение»**. Кнопка **«История (N)»** открывает предыдущие результаты; метку в таблице можно переименовать двойным щелчком или F2. Выбранную строку или всю историю можно скопировать как TSV, экспортировать в CSV, а размеченный результат сохранить как обрезанный PNG. **«Снимок PNG»** сохраняет весь исходный кадр без отметок. **«Область PNG»** позволяет выделить и сохранить прямоугольный фрагмент текущего неподвижного изображения. История хранится до очистки или выхода, но не между запусками.

Кнопка **«Сочетания»** настраивает клавиши для режимов, захвата и истории. По умолчанию локальные сочетания Ctrl+Alt+1…8 выбирают режимы, Ctrl+Alt+C делает снимок, Ctrl+Alt+H открывает историю. Они работают при активном окне настроек, когда фокус не находится в поле ввода. В диалоге можно отдельно запросить глобальные сочетания через портал рабочего стола до выхода из программы. Композитор может попросить подтверждение или назначить другие клавиши; портал может быть недоступен. В Hyprland регистрация не назначает клавиши: их связывают с `desktop-ruler:mode_0`…`mode_7`, `desktop-ruler:capture` и `desktop-ruler:history` в [настройках композитора](https://wiki.hypr.land/configuring/core/binds/globals/). Локальные настройки сохраняются между запусками, глобальные нужно включать заново.

`--tray` запускает программу свёрнутой, если доступен трей. Если доступен системный трей, закрытие окна скрывает его в трее; **«Выход»** завершает программу. `grim` захватывает один выбранный выход в совместимой с ним сессии Wayland. Измеряется неподвижный снимок, а не меняющийся экран.

## Лицензия

Исходный код приложения распространяется по лицензии MIT; см. [LICENSE](LICENSE). Qt и другие системные библиотеки являются отдельными зависимостями со своими лицензиями. Условия открытой и коммерческой лицензий Qt приведены в [официальной документации Qt](https://doc.qt.io/qt-6/licensing.html).
