#!/usr/bin/env bash

set -euo pipefail

if command -v get_idf >/dev/null 2>&1; then
    get_idf
elif [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
    # shellcheck disable=SC1090
    source "${IDF_PATH}/export.sh"
elif [[ -f "${HOME}/esp/esp-idf/export.sh" ]]; then
    # shellcheck disable=SC1090
    source "${HOME}/esp/esp-idf/export.sh"
else
    echo "Error: ESP-IDF environment not found." >&2
    echo "Run in an ESP-IDF shell (get_idf) or set IDF_PATH." >&2
    exit 1
fi

python3 "$(dirname "$0")/create_combined_bin.py" "$@"