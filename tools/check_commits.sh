#!/usr/bin/env bash
set -euo pipefail

MIN_COMMITS="${MIN_COMMITS:-5}"
MIN_GAP_SECONDS="${MIN_GAP_SECONDS:-1800}"
WINDOW_DAYS="${WINDOW_DAYS:-7}"

if ! command -v git >/dev/null 2>&1; then
    echo "Loi: khong tim thay git." >&2
    exit 1
fi

mapfile -t commits < <(
    git log --all --since="${WINDOW_DAYS} days ago" --format='%H%x09%at%x09%ae' --reverse
)

if ((${#commits[@]} == 0)); then
    echo "Loi: khong co commit nao trong ${WINDOW_DAYS} ngay qua." >&2
    exit 1
fi

declare -A author_counts=()
declare -A previous_by_author=()
declare -A previous_hash_by_author=()
previous_timestamp=
previous_hash=

for entry in "${commits[@]}"; do
    IFS=$'\t' read -r hash timestamp email <<<"$entry"
    author_counts["$email"]=$(( ${author_counts["$email"]:-0} + 1 ))

    if [[ -n "${previous_by_author["$email"]:-}" ]]; then
        gap=$((timestamp - previous_by_author["$email"]))
        if ((gap < MIN_GAP_SECONDS)); then
            echo "Loi: hai commit cua ${email} cach nhau ${gap}s (< ${MIN_GAP_SECONDS}s)." >&2
            echo "  ${previous_hash_by_author["$email"]} -> ${hash}" >&2
            exit 1
        fi
    fi
    previous_by_author["$email"]=$timestamp
    previous_hash_by_author["$email"]=$hash

    if [[ -n "$previous_timestamp" ]]; then
        gap=$((timestamp - previous_timestamp))
        if ((gap < MIN_GAP_SECONDS)); then
            echo "Loi: hai commit tren repo cach nhau ${gap}s (< ${MIN_GAP_SECONDS}s)." >&2
            echo "  ${previous_hash} -> ${hash}" >&2
            exit 1
        fi
    fi
    previous_timestamp=$timestamp
    previous_hash=$hash
done

failed=0
for email in "${!author_counts[@]}"; do
    count=${author_counts["$email"]}
    if ((count < MIN_COMMITS)); then
        echo "Loi: ${email} co ${count}/${MIN_COMMITS} commit trong ${WINDOW_DAYS} ngay qua." >&2
        failed=1
    else
        echo "OK: ${email} co ${count} commit."
    fi
done

if ((failed)); then
    exit 1
fi

echo "OK: commit hop le (toi thieu ${MIN_GAP_SECONDS}s giua moi commit tren repo)."
