# Development Quick Start

Быстрый старт для разработчиков проекта Advanced Log Viewer.

## 1️⃣ Клонирование и подготовка

```bash
# Клонируем репозиторий
git clone <repository-url>
cd AdvancedLogViewer

# Создаем виртуальное окружение
python3 -m venv venv
source venv/bin/activate  # Linux/Mac
# или для Windows:
# venv\Scripts\activate

# Устанавливаем зависимости
pip install -r requirements.txt
```

## 2️⃣ Структура веток

```
main          → стабильная версия (production)
develop       → основная разработка
feature/*     → новые функции (создавайте от develop)
bugfix/*      → исправления ошибок (создавайте от develop)
```

**Создание новой ветки**:

```bash
git checkout develop
git pull origin develop
git checkout -b feature/my-feature
```

## 3️⃣ Запуск приложения

```bash
# Просто запустить
python AdvancedLogViewer.py

# Или через make (если есть)
make run
```

## 4️⃣ Сборка бинарника

```bash
# Установка PyInstaller
pip install pyinstaller

# Быстрая сборка
pyinstaller AdvancedLogViewer.spec

# Результат в dist/
./dist/AdvancedLogViewer
```

**Подробнее**: смотрите `BUILD.md`

## 5️⃣ Проверка кода

```bash
# Форматирование (Black)
pip install black
black AdvancedLogViewer.py

# Линтинг (Flake8)
pip install flake8
flake8 AdvancedLogViewer.py
```

## 6️⃣ Коммиты

```bash
# Примеры правильных сообщений коммитов:
git commit -m "feat(ui): добавить темный режим"
git commit -m "fix(serial): исправить потерю данных"
git commit -m "docs: обновить README"
git commit -m "refactor: разделить код на модули"
```

**Форматы**:
- `feat` - новая функция
- `fix` - исправление
- `docs` - документация
- `style` - форматирование
- `refactor` - рефакторинг
- `test` - тесты
- `chore` - техническое обслуживание

## 7️⃣ Pull Request

1. Создайте ветку от `develop`
2. Внесите изменения
3. Отправьте в репозиторий: `git push origin feature/my-feature`
4. Создайте PR на GitHub (target: `develop`)

**Перед PR убедитесь**:
- [ ] Код запускается без ошибок
- [ ] Нет ошибок линтера (`flake8`)
- [ ] Код отформатирован (`black`)
- [ ] Функция протестирована
- [ ] Обновлена документация (если нужно)

## 📚 Документация

- **README.md** - основная документация
- **BUILD.md** - как собрать бинарник
- **CONTRIBUTING.md** - полный гайд по разработке
- **CHANGELOG.md** - история изменений
- **VERSION.md** - информация о версии

## 🐛 Репортинг ошибок

Создайте Issue на GitHub с:
- Описанием проблемы
- Шагами воспроизведения
- Ожидаемым и текущим поведением
- Информацией о среде (OS, Python версия)

## ✨ Полезные команды

```bash
# Обновить develop с remote
git checkout develop && git pull origin develop

# Посмотреть историю
git log --oneline -10

# Посмотреть различия
git diff develop..feature/my-feature

# Синхронизировать ветку с develop
git rebase develop

# Отменить последний коммит (не отправленный)
git reset --soft HEAD~1
```

## 🚀 После внесения изменений

```bash
# 1. Проверка кода
black AdvancedLogViewer.py
flake8 AdvancedLogViewer.py

# 2. Коммит
git add .
git commit -m "feat(feature): description"

# 3. Отправка
git push origin feature/my-feature

# 4. Pull Request на GitHub
```

---

**Вопросы?** Смотрите `CONTRIBUTING.md` или создайте Issue.

**Готово к работе!** 🎉
