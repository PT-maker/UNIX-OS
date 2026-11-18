#!/bin/sh

DIR=${DATA_DIR:-/data}
LOCK=$DIR/.lock
ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' \n')
seq=0
file=""

set -C

trap 'rm -f -- "$file"; exit 0' INT TERM

exec 9>>"$LOCK"

while :; do
    seq=$((seq + 1))

    flock 9
    n=1
    while :; do
        case $n in
            ?)  cand=$DIR/00$n ;;
            ??) cand=$DIR/0$n ;;
            *)  cand=$DIR/$n ;;
        esac
        [ -e "$cand" ] || break
        n=$((n + 1))
    done
    file=$cand
    printf '%s\n%s\n' "$ID" "$seq" > "$file"
    flock -u 9

    sleep 1 & wait $!
    rm -f -- "$file"
    file=""
    sleep 1 & wait $!
done