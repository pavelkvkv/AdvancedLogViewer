#!/usr/bin/env bash
# screenshot.sh — снимок экрана для отладки UI.
# Использование: screenshot.sh [выходной_файл.png]
# По умолчанию снимает весь экран (KDE/Wayland — spectacle, X11 — import).
set -euo pipefail

OUT="${1:-/tmp/alv2_shot.png}"

if command -v spectacle >/dev/null 2>&1; then
    # KDE: -b фон, -n без уведомления, -f весь рабочий стол
    spectacle -b -n -f -o "$OUT" >/dev/null 2>&1
elif command -v grim >/dev/null 2>&1; then
    grim "$OUT"                       # wlroots-композиторы
elif command -v import >/dev/null 2>&1; then
    import -window root "$OUT"        # X11 / XWayland
else
    echo "Нет инструмента для снимка экрана (spectacle/grim/import)" >&2
    exit 1
fi

echo "$OUT"
