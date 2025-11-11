# Build Instructions / Инструкция по сборке

## Краткая версия

```bash
pyinstaller AdvancedLogViewer.spec
```

Бинарник будет в `dist/AdvancedLogViewer` (Linux) или `dist/AdvancedLogViewer.exe` (Windows).

---

## Полная инструкция

### 1. Подготовка окружения

#### Linux

```bash
# Установка Python и pip (если нет)
sudo apt-get update
sudo apt-get install python3 python3-pip python3-venv

# Создание виртуального окружения (опционально, но рекомендуется)
python3 -m venv venv
source venv/bin/activate

# Установка зависимостей
pip install -r requirements.txt
```

#### Windows

```cmd
# Установка зависимостей (при наличии Python и pip)
pip install -r requirements.txt

# Или используйте виртуальное окружение
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

### 2. Установка PyInstaller

```bash
pip install pyinstaller
```

### 3. Сборка бинарника

#### Стандартная сборка (рекомендуется)

```bash
pyinstaller AdvancedLogViewer.spec
```

Это использует готовую конфигурацию в файле `AdvancedLogViewer.spec`.

#### Ручная сборка (если нет .spec файла)

```bash
pyinstaller --onefile \
  --windowed \
  --name AdvancedLogViewer \
  --add-data "settings.ini:." \
  AdvancedLogViewer.py
```

**Опции**:
- `--onefile` - создать один исполняемый файл (медленнее при запуске)
- `--windowed` - скрыть консоль (только для GUI)
- `--add-data` - включить дополнительные файлы
- `-i icon.ico` - добавить иконку (если есть)

### 4. Результаты сборки

```
dist/
├── AdvancedLogViewer        # Готовый бинарник (Linux)
└── AdvancedLogViewer.exe    # Готовый бинарник (Windows)
```

Запуск:
- **Linux**: `./dist/AdvancedLogViewer`
- **Windows**: `dist\AdvancedLogViewer.exe` или двойной клик

---

## Очистка артефактов сборки

Удаление временных файлов:

```bash
rm -rf build/ dist/ *.egg-info
```

Команда для автоматизации:

```bash
python -c "import shutil; shutil.rmtree('build', ignore_errors=True); shutil.rmtree('dist', ignore_errors=True)"
```

---

## Решение проблем

### "No module named 'serial'"

```bash
pip install pyserial
```

### "No module named 'tkinter'" (Linux)

```bash
sudo apt-get install python3-tk
```

### Большой размер бинарника

Используйте опцию `--onedir` вместо `--onefile` - она быстрее запускается:

```bash
pyinstaller --onedir --windowed --name AdvancedLogViewer AdvancedLogViewer.py
```

### Проблемы с иконкой/ресурсами

1. Убедитесь, что все файлы ресурсов указаны в опции `--add-data`
2. Проверьте пути в коде приложения (используйте `sys._MEIPASS` для PyInstaller)

---

## CI/CD Integration

Для автоматизации сборки в GitHub Actions:

```yaml
name: Build Executable

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: 3.9
      - name: Install dependencies
        run: |
          pip install -r requirements.txt
          pip install pyinstaller
      - name: Build executable
        run: pyinstaller AdvancedLogViewer.spec
      - name: Upload artifact
        uses: actions/upload-artifact@v2
        with:
          name: AdvancedLogViewer
          path: dist/
```

---

## Дополнительные команды

### Проверка совместимости (dry-run)

```bash
pyinstaller --onefile --dry-run AdvancedLogViewer.py
```

### Вывод подробной информации

```bash
pyinstaller -v AdvancedLogViewer.spec
```

### Использование существующей конфигурации

```bash
pyinstaller --onefile --windowed AdvancedLogViewer.py
# Это создаст `AdvancedLogViewer.spec`, который можно редактировать
```

---

## Версионирование

Для включения версии в бинарник добавьте в начало `AdvancedLogViewer.py`:

```python
__version__ = "1.0.0"
```

И используйте при сборке:

```bash
pyinstaller --onefile --version-file=version.txt AdvancedLogViewer.py
```

---

**Статус**: Актуально для PyInstaller 5.x+
