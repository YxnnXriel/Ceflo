#!/bin/bash
# Lance l'extension dans un vrai Visual Studio Code, avec un profil temporaire et sans fenêtre.

ICI=$(cd "$(dirname "$0")" && pwd)
RACINE=$(cd "$ICI/../../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

export CEFLO_TEST_RESULTAT="$TMP/resultat"

# `code` rend la main tout de suite : il faut lancer le vrai programme, et attendre qu'il finisse.
CODE=$(dirname "$(readlink -f "$(command -v code)")")/../code
[ -x "$CODE" ] || CODE=/usr/share/code/code

xvfb-run -a "$CODE" \
    --user-data-dir "$TMP/profil" \
    --extensions-dir "$TMP/extensions" \
    --disable-gpu \
    --disable-workspace-trust \
    --extensionDevelopmentPath "$ICI/.." \
    --extensionTestsPath "$ICI/vscode_suite.js" \
    "$RACINE" > "$TMP/sortie" 2>&1

if [ -f "$CEFLO_TEST_RESULTAT" ]; then
    cat "$CEFLO_TEST_RESULTAT"
    grep -q "0 échoués" "$CEFLO_TEST_RESULTAT"
    exit $?
fi

echo "Visual Studio Code n'a pas donné de résultat :"
tail -20 "$TMP/sortie"
exit 1
