# Desktop Ruler 0.2.0

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

Choose an output and **Capture screen**, or **Open image**. Select a mode and press **Measure**. If no image is loaded, Measure captures the selected output first. The image stays frozen: measure again on the same image, or use **New capture** to update it. The window shows the current source, mode guidance, and latest result. Only settings relevant to the selected mode appear; magnifier zoom is under **Magnifier options**.

On the image, left-click points or drag to trace a curve. For a distance, mark at least two points; for a closed area or perimeter, mark at least three. **Done**, Enter, or right-click finishes. **Undo**, Backspace, or Ctrl+Z removes the last point or stroke; **Cancel** or Escape returns to the controls. Tab hides/shows the canvas buttons so pixels beneath them remain reachable. Angle mode takes the vertex and two ray points. Count mode numbers clicks. Automatic color area starts from a clicked pixel; RGB tolerance controls how far each channel may differ from that seed.

Pixel values refer to pixels in the image. Calibrate with a known length and unit; this does not estimate physical monitor DPI. Manually traced area and perimeter use the contour. Automatic color-region area counts occupied pixels, and its perimeter counts exposed pixel edges, including holes.

Results remain in session history until cleared or quit. Double-click a history label (or press F2) to rename it. Copy a selected row or all rows as TSV, export CSV, or save a marked result as a cropped PNG. **Full image PNG** saves the unmarked source. **Region PNG** lets you select and save a rectangular part of the current frozen image. History is not retained between runs.

**Shortcuts** edits key bindings for modes, capture, and history. The default local bindings are Ctrl+Alt+1 through Ctrl+Alt+8 for modes, Ctrl+Alt+C for capture, and Ctrl+Alt+H for history. Local shortcuts work while the controls window is active and focus is outside text inputs. The Shortcuts dialog can optionally request global shortcuts through the desktop portal for the current run. The compositor may ask for approval or different bindings, or may not support the portal; global shortcuts are not guaranteed. On Hyprland, registration exposes actions but does not assign keys; use the compositor’s [global binding setup](https://wiki.hypr.land/configuring/core/binds/globals/) with `desktop-ruler:mode_0` through `mode_7`, `desktop-ruler:capture` or `desktop-ruler:history`. Local bindings persist between runs, while global activation must be requested again after restart.

`--tray` starts with only the tray icon when a system tray is available. If a system tray is available, closing the controls window hides it in the tray; **Quit** exits the app. Screen capture targets one selected output and depends on `grim` support in the active Wayland session. This design measures a frozen image, not a changing live screen.

## License

This application's source is licensed under the MIT License; see [LICENSE](LICENSE). Qt and other system libraries are separate dependencies with their own licenses. Qt's open-source and commercial licensing options are described in the [official Qt licensing documentation](https://doc.qt.io/qt-6/licensing.html).

---

# Экранная линейка 0.2.0

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

Выберите выход и нажмите **«Захват экрана»** либо **«Открыть файл»**. Выберите режим и нажмите **«Измерить»**. Если изображения ещё нет, эта кнопка сначала захватит выбранный выход. Снимок остаётся неподвижным: можно измерить его снова или обновить кнопкой **«Новый снимок»**. В окне видны источник, подсказка для режима и последний результат. Показываются только нужные для режима настройки; увеличение лупы находится в **«Настройках лупы»**.

На изображении ставьте точки левой кнопкой или ведите мышью для кривой. Для расстояния нужны минимум две точки, для замкнутой площади или периметра — минимум три. **«Готово»**, Enter или правая кнопка завершают измерение. **«Отменить»**, Backspace или Ctrl+Z убирают последнюю точку либо штрих; **«Назад»** или Escape возвращают к настройкам. Tab скрывает/показывает кнопки на холсте, чтобы можно было отметить пиксели под ними. Для угла отметьте вершину и две точки лучей. Режим подсчёта нумерует щелчки. Автоматическая область начинается с выбранного пикселя; допуск RGB задаёт допустимое отклонение каждого канала от его цвета.

Пиксельные значения относятся к пикселям изображения. Калибровка по известной длине и единице не определяет физический DPI монитора. Ручная площадь и периметр рассчитываются по контуру. Автоматическая область считает занятые пиксели, а её периметр — открытые стороны пикселей, включая границы отверстий.

Результаты хранятся до очистки истории или выхода. Метку готового измерения можно переименовать двойным щелчком либо клавишей F2. Выбранную строку или всю историю можно скопировать как TSV, экспортировать в CSV, а размеченный результат сохранить как обрезанный PNG. **«Снимок PNG»** сохраняет весь исходный кадр без отметок. **«Область PNG»** позволяет выделить и сохранить прямоугольный фрагмент текущего неподвижного изображения. Между запусками история не сохраняется.

Кнопка **«Сочетания»** настраивает клавиши для режимов, захвата и истории. По умолчанию локальные сочетания Ctrl+Alt+1…8 выбирают режимы, Ctrl+Alt+C делает снимок, Ctrl+Alt+H открывает историю. Они работают при активном окне настроек, когда фокус не находится в поле ввода. В диалоге можно отдельно запросить глобальные сочетания через портал рабочего стола до выхода из программы. Композитор может попросить подтверждение или назначить другие клавиши; портал может быть недоступен. В Hyprland регистрация не назначает клавиши: их связывают с `desktop-ruler:mode_0`…`mode_7`, `desktop-ruler:capture` и `desktop-ruler:history` в [настройках композитора](https://wiki.hypr.land/configuring/core/binds/globals/). Локальные настройки сохраняются между запусками, глобальные нужно включать заново.

`--tray` запускает программу свёрнутой, если доступен трей. Если доступен системный трей, закрытие окна скрывает его в трее; **«Выход»** завершает программу. `grim` захватывает один выбранный выход в совместимой с ним сессии Wayland. Измеряется неподвижный снимок, а не меняющийся экран.

## Лицензия

Исходный код приложения распространяется по лицензии MIT; см. [LICENSE](LICENSE). Qt и другие системные библиотеки являются отдельными зависимостями со своими лицензиями. Условия открытой и коммерческой лицензий Qt приведены в [официальной документации Qt](https://doc.qt.io/qt-6/licensing.html).
