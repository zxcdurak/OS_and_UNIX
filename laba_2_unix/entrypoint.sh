#!/bin/sh
set -eu
set -C

DATA_DIR="${DATA_DIR:-/data}"
mkdir -p "$DATA_DIR"
cd "$DATA_DIR"

exec 9>>.lock

CONTAINER_ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' \n')

current_file=""
seq_num=0
stopping=0

cleanup() {
    flock -u 9 2>/dev/null || true
    if [ -n "$current_file" ]; then
        rm -f "./$current_file"
        current_file=""
    fi
}

trap stopping=1 TERM INT
trap cleanup EXIT

while [ "$stopping" -eq 0 ]; do
    seq_num=$((seq_num + 1))
    i=1

    flock -x 9
    while :; do
        case "$i" in
            ?)  name="00$i" ;;
            ??) name="0$i" ;;
            *)  name="$i" ;;
        esac
        if [ ! -e "$name" ]; then
            printf '%s %d\n' "$CONTAINER_ID" "$seq_num" > "$name"
            current_file="$name"
            break
        fi
        i=$((i + 1))
    done
    flock -u 9

    echo "$CONTAINER_ID: created $current_file ($seq_num)"
    [ "$stopping" -eq 0 ] || break
    sleep 1

    rm -f "./$current_file"
    echo "$CONTAINER_ID: deleted $current_file"
    current_file=""

    [ "$stopping" -eq 0 ] || break
    sleep 1
done