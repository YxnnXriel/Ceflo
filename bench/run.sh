#!/bin/sh
# Mesure la vitesse : ./bench/run.sh ./Ceflo ./Ceflo_ref
#
# Pour chaque bench/x.cf : le meilleur temps sur 3 de la VM, de l'ancien
# interpréteur, et de Python si bench/x.py existe. Un gain de 4.0x veut dire
# que la VM est 4 fois plus rapide.
#
# CEFLO_BENCH_RUNS=5 change le nombre d'essais. Un seul nom en argument :
# ./bench/run.sh ./Ceflo ./Ceflo_ref fib

absolu(){
    (cd "$(dirname "$1")" && echo "$(pwd)/$(basename "$1")")
}

NEW=$(absolu "$1")
REF=$(absolu "$2")
ONLY=$3
BENCH=$(cd "$(dirname "$0")" && pwd)
RUNS=${CEFLO_BENCH_RUNS:-3}
# Des nombres avec un point, quelle que soit la langue du système.
LC_ALL=C
export LC_ALL
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

PYTHON=$(command -v python3)

# `time -p` est dans POSIX : la même sortie sous Linux et macOS.
TIME=/usr/bin/time
[ -x "$TIME" ] || TIME=time

# Le meilleur temps de `commande script`, en secondes. « erreur » si le
# programme échoue, ou si sa sortie n'est pas celle de la référence.
best(){
    best_time=""
    i=0
    while [ $i -lt "$RUNS" ]; do
        if ! $TIME -p "$@" > "$TMP/out" 2> "$TMP/time"; then
            echo "erreur"
            return
        fi
        t=$(sed -n 's/^real[[:space:]]*//p' "$TMP/time" | tail -n 1)
        if [ -z "$best_time" ] || [ "$(echo "$t $best_time" | awk '{ print ($1 < $2) }')" = 1 ]; then
            best_time=$t
        fi
        i=$((i + 1))
    done
    if [ -f "$TMP/expected" ] && ! cmp -s "$TMP/out" "$TMP/expected"; then
        echo "faux"
        return
    fi
    echo "$best_time"
}

# Le gain de b par rapport à a : a / b
gain(){
    echo "$1 $2" | awk '{ if ($1 + 0 > 0 && $2 + 0 > 0) printf "%.1fx", $1 / $2; else print "-" }'
}

cd "$BENCH" || exit 1

printf "%-12s %9s %9s %9s %11s %11s\n" "" "VM" "ancien" "Python" "vs ancien" "vs Python"
for script in *.cf; do
    name=${script%.cf}
    [ -n "$ONLY" ] && [ "$ONLY" != "$name" ] && continue

    # La référence donne la sortie attendue des deux autres.
    rm -f "$TMP/expected"
    ref=$(best "$REF" "$script")
    [ "$ref" != "erreur" ] && cp "$TMP/out" "$TMP/expected"
    new=$(best "$NEW" "$script")

    python="-"
    if [ -n "$PYTHON" ] && [ -f "$name.py" ]; then
        python=$(best "$PYTHON" "$name.py")
    fi

    printf "%-12s %9s %9s %9s %11s %11s\n" "$name" "$new" "$ref" "$python" "$(gain "$ref" "$new")" "$(gain "$python" "$new")"
done
