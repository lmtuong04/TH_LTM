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
    git log --all --since="${WINDOW_DAYS} days ago" \
        --format='%H%x09%at%x09%ae' --reverse
)

if ((${#commits[@]} == 0)); then
    echo "OK: khong co commit nao trong ${WINDOW_DAYS} ngay qua."
    exit 0
fi

declare -A previous_by_author=()
declare -A previous_hash_by_author=()
previous_timestamp=
previous_hash=
gap_warnings=0

for entry in "${commits[@]}"; do
    IFS=$'\t' read -r hash timestamp email <<<"$entry"
    if [[ -n "${previous_by_author["$email"]:-}" ]]; then
        gap=$((timestamp - previous_by_author["$email"]))
        if ((gap < MIN_GAP_SECONDS)); then
            echo "Canh bao: hai commit cua ${email} cach nhau ${gap}s (< ${MIN_GAP_SECONDS}s)." >&2
            echo "  ${previous_hash_by_author["$email"]} -> ${hash}" >&2
            gap_warnings=1
        fi
    fi
    previous_by_author["$email"]=$timestamp
    previous_hash_by_author["$email"]=$hash

    if [[ -n "$previous_timestamp" ]]; then
        gap=$((timestamp - previous_timestamp))
        if ((gap < MIN_GAP_SECONDS)); then
            echo "Canh bao: hai commit tren repo cach nhau ${gap}s (< ${MIN_GAP_SECONDS}s)." >&2
            echo "  ${previous_hash} -> ${hash}" >&2
            gap_warnings=1
        fi
    fi
    previous_timestamp=$timestamp
    previous_hash=$hash
done

commit_count=${#commits[@]}
if ((commit_count < MIN_COMMITS)); then
    echo "Loi: repository co ${commit_count}/${MIN_COMMITS} commit trong ${WINDOW_DAYS} ngay qua." >&2
    exit 1
fi

echo "OK: repository co ${commit_count} commit trong ${WINDOW_DAYS} ngay qua."
if ((gap_warnings)); then
    echo "OK: khoang cach commit chi la canh bao tam thoi."
fi
