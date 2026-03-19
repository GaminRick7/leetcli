#!/usr/bin/env bash
set -e

CONFIG_DIR="$HOME/.leetcli"
CONFIG_FILE="$CONFIG_DIR/config.json"

mkdir -p "$CONFIG_DIR"
mkdir -p /workspace/problems

if [ ! -f "$CONFIG_FILE" ]; then
    if [ -n "$LEETCODE_SESSION" ] || [ -n "$LEETCODE_CSRF" ] || [ -n "$DEFAULT_LANG" ] || [ -n "$GEMINI_KEY" ]; then
        cat > "$CONFIG_FILE" <<EOF
{
    "lang": "${DEFAULT_LANG:-cpp}",
    "problems_dir": "/workspace/problems/",
    "gemini_key": "${GEMINI_KEY:-}",
    "leetcode_session": "${LEETCODE_SESSION:-}",
    "csrf_token": "${LEETCODE_CSRF:-}"
}
EOF
    fi
fi

exec /usr/local/bin/leetcli "$@"
