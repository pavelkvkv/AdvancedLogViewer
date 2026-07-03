#!/usr/bin/env bash
# smoke_test.sh — базовый smoke-тест: запуск приложения в --test-mode,
# отправка команды quit, проверка корректного завершения.
# Использование: ./smoke_test.sh [путь_к_бинарнику]

set -euo pipefail

BINARY="${1:-./build/system/alv2}"
SOCKET="${HOME}/.local/share/AdvancedLogViewer2/test.sock"
TIMEOUT=5

fail() {
    echo "FAIL: $1" >&2
    # Попробуем убить процесс, если он ещё жив
    if [[ -n "${APP_PID:-}" ]] && kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID" 2>/dev/null || true
    fi
    exit 1
}

pass() {
    echo "PASS: $1"
}

# Проверяем наличие бинарника
if [[ ! -x "$BINARY" ]]; then
    fail "Binary not found or not executable: $BINARY"
fi

# Удаляем старый сокет
rm -f "$SOCKET"

# Запускаем приложение в тестовом режиме
# QT_QPA_PLATFORM=offscreen — без реального дисплея
export QT_QPA_PLATFORM=offscreen
"$BINARY" --test-mode &
APP_PID=$!

# Ждём появления сокета
elapsed=0
while [[ ! -S "$SOCKET" ]]; do
    sleep 0.2
    elapsed=$(echo "$elapsed + 0.2" | bc)
    if (( $(echo "$elapsed >= $TIMEOUT" | bc -l) )); then
        fail "Socket did not appear within ${TIMEOUT}s"
    fi
done
pass "Application started, socket available"

# Функция отправки команды
send_cmd() {
    local cmd="$1"
    if command -v socat &>/dev/null; then
        echo "$cmd" | socat -t2 - UNIX-CONNECT:"$SOCKET"
    elif command -v nc &>/dev/null; then
        echo "$cmd" | nc -U -q1 "$SOCKET"
    else
        fail "socat or nc required"
    fi
}

# Тест 1: отправляем quit
RESPONSE=$(send_cmd "quit" 2>/dev/null || true)
if [[ "$RESPONSE" == *"OK"* ]]; then
    pass "quit command accepted"
else
    fail "quit returned unexpected response: $RESPONSE"
fi

# Ждём завершения процесса
elapsed=0
while kill -0 "$APP_PID" 2>/dev/null; do
    sleep 0.2
    elapsed=$(echo "$elapsed + 0.2" | bc)
    if (( $(echo "$elapsed >= $TIMEOUT" | bc -l) )); then
        fail "Application did not exit within ${TIMEOUT}s after quit"
    fi
done

wait "$APP_PID" 2>/dev/null || true
pass "Application exited cleanly"

echo "---"
echo "All smoke tests passed."
