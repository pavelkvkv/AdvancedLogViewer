#!/usr/bin/env bash
# test_ui.sh — обёртка для отправки команд в TestServer через Unix-сокет.
# Использование: ./test_ui.sh <команда> [аргументы...]
# Пример: ./test_ui.sh get_line_count window_0

set -euo pipefail

SOCKET="${HOME}/.local/share/AdvancedLogViewer2/test.sock"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]"
    echo "Commands: get_line_count, get_line, set_filter, clear_filter, quit"
    exit 1
fi

CMD="$*"

if command -v socat &>/dev/null; then
    echo "$CMD" | socat - UNIX-CONNECT:"$SOCKET"
elif command -v nc &>/dev/null; then
    echo "$CMD" | nc -U "$SOCKET"
else
    echo "Error: socat or nc required" >&2
    exit 1
fi
