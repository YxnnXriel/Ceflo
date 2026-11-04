#!/bin/sh
# Lance tous les tests : ./tests/run.sh ./Ceflo
#
#   tests/x.cf + tests/x.out   la sortie du programme doit être exactement x.out
#   tests/x.in                 (facultatif) ce qui est donné sur l'entrée standard
#   tests/erreurs/x.cf         doit échouer, et sa première ligne dit le message attendu
#   exemple/*.cf               doit se terminer sans erreur
#
# CEFLO_TEST_SKIP="x y" permet de sauter les tests x et y.

CEFLO=$(realpath "$1")
ROOT=$(dirname "$(dirname "$(realpath "$0")")")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Un test qui boucle ne doit pas bloquer toute la suite.
ceflo(){
    timeout 120 "$CEFLO" "$@"
}

should_skip(){
    case " $CEFLO_TEST_SKIP " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

passed=0
failed=0

fail(){
    failed=$((failed + 1))
    echo "ÉCHEC  $1"
    [ -n "$2" ] && echo "$2" | head -15
}

cd "$ROOT/tests" || exit 1

for script in *.cf; do
    name=${script%.cf}
    input=/dev/null
    [ -f "$name.in" ] && input="$name.in"

    should_skip "$name" && continue

    case $name in
        natif) arguments="$TMP/natif.txt" ;;
        module_os) arguments="$TMP/os" ;;
        entree|main) arguments="un deux" ;;
        *) arguments="" ;;
    esac

    ceflo "$script" $arguments < "$input" > "$TMP/out" 2> "$TMP/err"
    code=$?

    if [ $code -ne 0 ]; then
        fail "$name (code $code)" "$(cat "$TMP/err")"
    elif ! difference=$(diff "$name.out" "$TMP/out"); then
        fail "$name" "$difference"
    else
        passed=$((passed + 1))
    fi
done

cd "$ROOT/tests/erreurs" || exit 1

for script in *.cf; do
    should_skip "${script%.cf}" && continue

    expected=$(head -n 1 "$script" | sed 's|^// attendu: ||')
    ceflo "$script" < /dev/null > "$TMP/out" 2> "$TMP/err"
    code=$?
    actual=$(head -n 1 "$TMP/err")

    # Le début du message suffit : la fin peut venir du système.
    case $actual in
        "$expected"*) matches=1 ;;
        *) matches=0 ;;
    esac

    if [ $code -ne 1 ] || [ $matches -ne 1 ]; then
        fail "erreurs/$script (code $code)" "attendu : $expected
obtenu  : $(head -n 3 "$TMP/err")"
    else
        passed=$((passed + 1))
    fi
done

# Du code démesuré doit donner une erreur, pas un segfault. Ces fichiers sont trop
# gros pour le dépôt : on les crée ici.
check_too_deep(){
    "$2" > "$TMP/profond.cf"
    ceflo "$TMP/profond.cf" < /dev/null > "$TMP/out" 2> "$TMP/err"
    code=$?
    case $(head -n 1 "$TMP/err") in
        "$3"*) [ $code -eq 1 ] && passed=$((passed + 1)) || fail "$1 (code $code)" ;;
        *) fail "$1 (code $code)" "$(head -n 1 "$TMP/err" | cut -c1-200)" ;;
    esac
}

long_sum(){
    awk 'BEGIN { printf "println(1"; for(i = 0; i < 500000; i++) printf "+1"; print ")" }'
}

deep_parens(){
    awk 'BEGIN { printf "var a = "; for(i = 0; i < 300000; i++) printf "("; print "" }'
}

check_too_deep "somme très longue" long_sum "erreur: trop d'appels imbriqués"
check_too_deep "parenthèses très profondes" deep_parens "erreur de syntaxe: code trop profondément imbriqué"

# La valeur retournée par main devient le code de sortie du programme.
check_exit_code(){
    echo "$2" > "$TMP/code.cf"
    ceflo "$TMP/code.cf" < /dev/null > /dev/null 2> "$TMP/err"
    code=$?
    [ $code -eq "$3" ] && passed=$((passed + 1)) || fail "$1 : code $code au lieu de $3"
}

check_exit_code "main retourne 7" 'func main() { return 7 }' 7
check_exit_code "main sans return" 'func main(argv::list) : none { }' 0
check_exit_code "exit dans main" 'func main() { exit(3); return 7 }' 3

cd "$ROOT/exemple" || exit 1

for script in *.cf; do
    name=${script%.cf}
    input=/dev/null
    [ -f "$name.in" ] && input="$name.in"

    if ceflo "$script" < "$input" > "$TMP/out" 2> "$TMP/err"; then
        passed=$((passed + 1))
    else
        fail "exemple/$script" "$(cat "$TMP/err")"
    fi
done

echo "$passed réussis, $failed échoués"
[ "$failed" -eq 0 ]
