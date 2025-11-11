#!/bin/bash

# build.sh - Скрипт для сборки AdvancedLogViewer
# Автоматически активирует виртуальное окружение и собирает приложение

set -e  # Выход при любой ошибке

# Определяем директорию скрипта
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🔨 Сборка AdvancedLogViewer..."
echo "📁 Директория проекта: $SCRIPT_DIR"

# Проверяем наличие виртуального окружения
if [ ! -d ".venv" ]; then
    echo "❌ Виртуальное окружение не найдено!"
    echo "📝 Создаю виртуальное окружение..."
    python3 -m venv .venv
fi

# Активируем виртуальное окружение
echo "✅ Активирую виртуальное окружение..."
source .venv/bin/activate

# Проверяем наличие PyInstaller
if ! command -v pyinstaller &> /dev/null; then
    echo "📦 PyInstaller не установлен, устанавливаю..."
    pip install pyinstaller
fi

# Проверяем наличие остальных зависимостей
if [ -f "requirements.txt" ]; then
    echo "📦 Устанавливаю зависимости из requirements.txt..."
    pip install -q -r requirements.txt
fi

# Собираем приложение
echo "🔨 Запускаю PyInstaller..."
pyinstaller AdvancedLogViewer.spec

echo ""
echo "✅ Сборка завершена успешно!"
echo "📦 Исполняемый файл находится в: ./dist/AdvancedLogViewer"
