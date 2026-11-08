#!/bin/sh
# Compare deux interpréteurs : ./tests/diff.sh ./Ceflo ./Ceflo_ref
#
# Pour chaque tests/*.cf, tests/erreurs/*.cf, tests/vm/*.cf et exemple/*.cf, les
# deux doivent donner la même sortie, la même première ligne d'erreur et le même
# code de sortie. Le second fait foi. Pour tests/vm/erreurs, la position de
# l'erreur (la ligne `-->`) doit aussi être la même.
#
# CEFLO_TEST_SKIP="x y" saute les tests x et y. CEFLO_DIFF_LIMIT=60 change la
# limite de temps d'un programme (en secondes).

# Les différences voulues (docs/VM.md) : « nom : raison ». Elles sont lancées,
# mais une différence n'y compte pas comme un échec.
voulues_liste(){ cat <<'FIN'
natif : les bibliothèques .so ont une nouvelle forme, celles de lib/ sont compilées pour la VM
affectation_composee : `x += e` lit x avant de calculer e, Ceflo_ref fait l'inverse
FIN
}

# Un chemin absolu sans realpath -f ni readlink -f (macOS).
absolu(){
    (cd "$(dirname "$1")" && echo "$(pwd)/$(basename "$1")")
}

NEW=$(absolu "$1")
REF=$(absolu "$2")
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIMIT=${CEFLO_DIFF_LIMIT:-60}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$NEW" ] || [ ! -x "$REF" ]; then
    echo "usage : $0 ./Ceflo ./Ceflo_ref" >&2
    exit 2
fi

# Lance une commande avec une limite de temps, avec ou sans `timeout`.
# Sans lui, un guetteur tue le programme quand la limite est passée.
limite(){
    if command -v timeout > /dev/null 2>&1; then
        timeout "$LIMIT" "$@"
        return
    fi
    exec 3<&0
    "$@" <&3 3<&- &
    pid=$!
    exec 3<&-
    (
        i=0
        while [ $i -lt "$LIMIT" ] && kill -0 $pid 2> /dev/null; do
            sleep 1
            i=$((i + 1))
        done
        kill -9 $pid 2> /dev/null
    ) > /dev/null 2>&1 &
    wait $pid
}

should_skip(){
    case " $CEFLO_TEST_SKIP " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

raison_voulue(){
    voulues_liste | sed -n "s|^$1 : ||p"
}

# Les arguments de chaque test, comme dans run.sh. Les dossiers de travail sont
# vidés avant chaque lancement : les deux interpréteurs partent du même état.
arguments(){
    rm -rf "$TMP/natif.txt" "$TMP/os"
    case $1 in
        natif) echo "$TMP/natif.txt" ;;
        module_os) echo "$TMP/os" ;;
        entree|main) echo "un deux" ;;
    esac
}

same=0
different=0
voulues=0

# Lance `script` (dans le dossier courant) avec les deux interpréteurs.
compare(){
    name=$1
    script=$2
    label=$3
    lines=$4
    input=/dev/null
    [ -f "${script%.cf}.in" ] && input="${script%.cf}.in"

    limite "$NEW" "$script" $(arguments "$name") < "$input" > "$TMP/new.out" 2> "$TMP/new.err"
    new_code=$?
    limite "$REF" "$script" $(arguments "$name") < "$input" > "$TMP/ref.out" 2> "$TMP/ref.err"
    ref_code=$?

    # Une récursion sans fin arrête Ceflo_ref quand la pile C est pleine : l'endroit
    # dépend de la taille de cette pile (plus petite sur macOS). On compare le message seul.
    if head -n 1 "$TMP/ref.err" | grep -q "trop d'appels imbriqués"; then
        lines=1
    fi
    new_err=$(head -n "$lines" "$TMP/new.err")
    ref_err=$(head -n "$lines" "$TMP/ref.err")

    if [ $new_code -eq $ref_code ] && [ "$new_err" = "$ref_err" ] && cmp -s "$TMP/new.out" "$TMP/ref.out"; then
        same=$((same + 1))
        return
    fi

    reason=$(raison_voulue "$name")
    if [ -n "$reason" ]; then
        voulues=$((voulues + 1))
        echo "VOULUE $label ($reason)"
        return
    fi

    different=$((different + 1))
    echo "DIFF   $label"
    [ $new_code -ne $ref_code ] && echo "    code : $new_code au lieu de $ref_code"
    if [ "$new_err" != "$ref_err" ]; then
        echo "$new_err" | cut -c1-200 | sed 's/^/    obtenu : /'
        echo "$ref_err" | cut -c1-200 | sed 's/^/    attendu: /'
    fi
    if ! cmp -s "$TMP/new.out" "$TMP/ref.out"; then
        # `<` : ce que donne la référence, `>` : ce que donne la VM.
        diff "$TMP/ref.out" "$TMP/new.out" | head -n 10 | sed 's/^/    /' | cut -c1-200
    fi
}

for dir in tests tests/erreurs tests/vm tests/vm/erreurs exemple; do
    [ -d "$ROOT/$dir" ] || continue
    lines=1
    [ "$dir" = tests/vm/erreurs ] && lines=2
    cd "$ROOT/$dir" || exit 1
    for script in *.cf; do
        [ -f "$script" ] || continue
        name=${script%.cf}
        should_skip "$name" && continue
        compare "$name" "$script" "$dir/$script" "$lines"
    done
done

echo "$same identiques, $different différents, $voulues différences voulues"
[ "$different" -eq 0 ]
