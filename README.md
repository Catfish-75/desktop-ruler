# Desktop Ruler

Desktop Ruler measures distances, paths, areas, perimeters, angles, and counts on a frozen screen capture or a local image. It is an independent implementation inspired by workflows described in the [Universal Desktop Ruler documentation](https://avpsoft.com/products/udruler/); it does not include AVPSoft source code or artwork.

## Build and run

Install a C++17 compiler, `make`, `pkg-config`, Qt 6 Widgets and Qt 6 Test development files. `grim` is needed to capture a screen in a compatible Wayland session.

```sh
make
make test
./desktop-ruler
./desktop-ruler --image /path/to/image.png
```

To add a user-local application menu entry, run `./install.sh install`. The default prefix is `~/.local`. Remove it with `./install.sh uninstall`.

## Use

Choose a screen and capture it, or open an image. Select a mode and start measuring. Enter or right-click finishes; Escape cancels or returns; Backspace or Ctrl+Z undoes the last point or stroke. “Next measurement” reuses the current image, and “Fresh capture” takes another screenshot.

Pixel values refer to pixels in the image. Calibration converts those values using a known length and a unit entered by the user; it does not estimate the monitor’s physical DPI. Manually traced polygon area and perimeter use the traced contour. Automatic color-region area counts occupied pixels, while its perimeter counts exposed pixel edges, including hole boundaries.

Double-click a history label (or press F2) to rename a completed measurement.

Results and annotated screenshots stay in session history until cleared or the app exits. Copy selected or all rows as TSV, export results as CSV, and save annotated PNG images when needed. History is not retained between runs.

Screen capture uses `grim` and targets one selected output per measurement session. The app is intended for grim-compatible Wayland compositors such as Hyprland. Opening a local image does not require screen capture.

## License

This application’s source is licensed under the MIT License; see [LICENSE](LICENSE). Qt and other system libraries are separate dependencies with their own licenses. Qt’s open-source and commercial licensing options are described in the [official Qt licensing documentation](https://doc.qt.io/qt-6/licensing.html).

---

# Экранная линейка

Desktop Ruler измеряет расстояния, пути, площади, периметры, углы и количество объектов на неподвижном снимке экрана или в локальном изображении. Это независимая реализация процессов, описанных в [документации Universal Desktop Ruler](https://avpsoft.com/products/udruler/); исходный код и графические материалы AVPSoft не используются.

## Сборка и запуск

Установите компилятор C++17, `make`, `pkg-config`, файлы разработки Qt 6 Widgets и Qt 6 Test. Для снимка экрана в совместимой Wayland-сессии нужна программа `grim`.

```sh
make
make test
./desktop-ruler
./desktop-ruler --image /путь/к/изображению.png
```

Чтобы добавить ярлык в меню текущего пользователя, выполните `./install.sh install`. По умолчанию используется `~/.local`. Удаление: `./install.sh uninstall`.

## Работа

Выберите экран и сделайте снимок либо откройте изображение. Выберите режим и начните измерение. Enter или правая кнопка завершает; Escape отменяет или возвращает; Backspace или Ctrl+Z отменяет последнюю точку или штрих. «Следующее измерение» использует текущее изображение, «Новый снимок» захватывает экран заново.

Пиксельные значения относятся к пикселям изображения. Калибровка переводит их в заданные пользователем единицы по известной длине; она не определяет физический DPI монитора. Площадь и периметр вручную обведённого контура рассчитываются по его геометрии. Автоматическая цветовая область считает занятые пиксели, а её периметр — открытые стороны пикселей, включая границы отверстий.

Для переименования готового измерения дважды щёлкните по метке в истории или нажмите F2.

Результаты и изображения с разметкой доступны в истории до её очистки или выхода из программы. Можно копировать выбранные или все строки как TSV, сохранять результаты в CSV и изображения в PNG. Между запусками история не сохраняется.

Для захвата экрана используется `grim`; за один раз измеряется один выбранный выход. Целевая среда — совместимые с grim Wayland-композиторы, включая Hyprland. Для локального изображения захват экрана не нужен.

## Лицензия

Исходный код приложения распространяется по лицензии MIT; см. [LICENSE](LICENSE). Qt и другие системные библиотеки являются отдельными зависимостями со своими лицензиями. Условия открытой и коммерческой лицензий Qt приведены в [официальной документации Qt](https://doc.qt.io/qt-6/licensing.html).
