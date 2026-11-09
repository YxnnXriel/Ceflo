#!/usr/bin/env python3
"""Construit le site du livre « Écrire CeFlo ».

    python3 livre/construire.py
    python3 livre/construire.py --chapitres DOSSIER --sortie DOSSIER

Lit livre/chapitres/*.md (format décrit dans livre/FORMAT.md) et écrit
livre/site/ : index.html, une page par chapitre et style.css.
Bibliothèque standard seulement.
"""

import argparse
import html
import re
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
DEPOT = ICI.parent
DEPOT_EN_LIGNE = "https://github.com/YxnnXriel/Ceflo"

POLICES = ("https://fonts.googleapis.com/css2?family=Unbounded:wght@600;800"
           "&family=Geist:wght@400;500;600&family=DM+Mono:wght@400;500"
           "&family=Just+Me+Again+Down+Here&family=JetBrains+Mono:wght@400;500"
           "&display=swap")

CYAN = "#34C1EE"
MENTHE, JAUNE, BLEU, ROSE = "#A6E3C6", "#F7DFA2", "#BDE8F8", "#F4A3BD"
COULEURS_PARTIES = [MENTHE, JAUNE, BLEU, ROSE]
COULEURS_JETONS = [CYAN, MENTHE, JAUNE, ROSE]
ANGLES_JETONS = ["-3deg", "2deg", "-1deg", "3deg"]

# Les phrases du sommaire de la maquette, si l'en-tête n'a pas de « resume ».
RESUMES = {
    1: "Charger le source et le garder pour les messages d'erreur.",
    2: "Découper le texte en jetons qui savent où ils commencent.",
    3: "Une fonction par règle de la grammaire, et un arbre au bout.",
    4: "Les noeuds de l'AST, et pourquoi ils ne changent jamais.",
    5: "Nombres, chaînes, listes et maps, avec leurs règles.",
    6: "Où vit chaque nom, et qui peut le voir.",
    7: "Exécuter l'arbre noeud par noeud. Ça marche, mais c'est lent.",
    8: "Marquer ce qui sert encore et libérer le reste.",
    9: "Une machine à pile, et le format du bytecode.",
    10: "Parcourir l'arbre une fois et écrire le code qui l'exécutera.",
    11: "La boucle qui lit une instruction et l'exécute.",
    12: "Garder une variable en vie après le retour de sa fonction.",
    13: "Des attributs à place fixe, et self sans avoir à l'écrire.",
    14: "import, export, et les fonctions écrites en C.",
}

AVERTISSEMENTS = []


def avertir(fichier, message):
    AVERTISSEMENTS.append(f"{fichier}: {message}")


def e(texte):
    """Échappe le HTML."""
    return html.escape(str(texte), quote=True)


def majuscule(texte):
    return texte[:1].upper() + texte[1:]


def nn(numero):
    return f"{int(numero):02d}"


# ---------------------------------------------------------------------------
# Le plan et les chapitres
# ---------------------------------------------------------------------------

def lire_plan(chemin):
    """Les parties et les chapitres prévus dans PLAN.md."""
    parties = []
    if not chemin.exists():
        return parties
    for ligne in chemin.read_text("utf-8").splitlines():
        m = re.match(r"^##\s+Partie\s+(\d+)\s*:\s*(.+?)\s*$", ligne)
        if m:
            parties.append({"numero": int(m.group(1)), "titre": m.group(2),
                            "chapitres": []})
            continue
        m = re.match(r"^###\s+(\d+)\.\s+(.+?)\s*\(`([^`]+)`\)\s*$", ligne)
        if m and parties:
            parties[-1]["chapitres"].append({
                "numero": int(m.group(1)), "titre": m.group(2),
                "fichier": m.group(3)})
    return parties


def separer_entete(texte, nom):
    lignes = texte.split("\n")
    entete = {}
    if lignes and lignes[0].strip() == "---":
        for i in range(1, len(lignes)):
            if lignes[i].strip() == "---":
                for l in lignes[1:i]:
                    if ":" in l:
                        cle, valeur = l.split(":", 1)
                        entete[cle.strip()] = valeur.strip()
                return entete, lignes[i + 1:]
        avertir(nom, "en-tête non fermé par ---")
    else:
        avertir(nom, "pas d'en-tête")
    return entete, lignes


def lire_chapitre(chemin):
    entete, lignes = separer_entete(chemin.read_text("utf-8"), chemin.name)
    m = re.match(r"^(\d+)", chemin.name)
    numero = entete.get("numero") or (m.group(1) if m else "0")
    try:
        numero = int(numero)
    except ValueError:
        avertir(chemin.name, f"numéro illisible : {numero}")
        numero = 0
    return {
        "numero": numero,
        "titre": entete.get("titre", chemin.stem),
        "note": entete.get("note", ""),
        "partie": entete.get("partie", ""),
        "resume": entete.get("resume", ""),
        "source": chemin.name,
        "page": chemin.stem + ".html",
        "blocs": lire_blocs(lignes, chemin.name),
    }


# ---------------------------------------------------------------------------
# Le lecteur de Markdown : d'abord des blocs, puis le texte en ligne
# ---------------------------------------------------------------------------

RE_CLOTURE = re.compile(r"^(`{3,}|~{3,})\s*([\w+-]*)\s*(.*)$")
RE_LISTE = re.compile(r"^( {0,3})([-*+]|\d+[.)])\s+(.*)$")


def debut_de_bloc(ligne):
    """Une ligne qui coupe un paragraphe."""
    return bool(RE_CLOTURE.match(ligne) or ligne.startswith("#")
                or ligne.startswith(">") or ligne.startswith(":::")
                or RE_LISTE.match(ligne))


def lire_attributs(texte):
    return dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', texte))


def lire_blocs(lignes, nom):
    blocs = []
    i, n = 0, len(lignes)
    while i < n:
        ligne = lignes[i].rstrip()
        if not ligne.strip():
            i += 1
            continue

        # Un bloc de code
        m = RE_CLOTURE.match(ligne)
        if m:
            cloture, langue, reste = m.groups()
            corps = []
            i += 1
            while i < n and not (lignes[i].strip().startswith(cloture[0] * len(cloture))
                                 and not lignes[i].strip().strip(cloture[0])):
                corps.append(lignes[i].rstrip("\r"))
                i += 1
            if i >= n:
                avertir(nom, "bloc de code non fermé")
            i += 1
            blocs.append({"type": "code", "langue": langue.lower(),
                          "attrs": lire_attributs(reste), "texte": "\n".join(corps)})
            continue

        # Une figure ::: jetons ou ::: etapes
        if ligne.startswith(":::"):
            tete = ligne[3:].strip()
            corps = []
            i += 1
            while i < n and lignes[i].strip() != ":::":
                corps.append(lignes[i].rstrip())
                i += 1
            if i >= n:
                avertir(nom, "figure ::: non fermée")
            i += 1
            sorte = tete.split()[0] if tete else ""
            legende = re.search(r'"([^"]*)"', tete)
            blocs.append({"type": "figure", "sorte": sorte,
                          "legende": legende.group(1) if legende else "",
                          "lignes": [c for c in corps if c.strip()]})
            if sorte not in ("jetons", "etapes"):
                avertir(nom, f"figure inconnue : ::: {sorte}")
            continue

        # Les titres
        m = re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", ligne)
        if m:
            niveau, titre = len(m.group(1)), m.group(2)
            if niveau == 2:
                mn = re.match(r"^(\d+(?:\.\d+)*)\.?\s+(.*)$", titre)
                if mn:
                    blocs.append({"type": "h2", "numero": mn.group(1), "titre": mn.group(2)})
                else:
                    if sorte_de_fin(titre) is None:
                        avertir(nom, f"sous-chapitre sans numéro : {titre}")
                    blocs.append({"type": "h2", "numero": "", "titre": titre})
            else:
                if niveau != 3:
                    avertir(nom, f"titre de niveau {niveau} rendu comme ###")
                blocs.append({"type": "h3", "titre": titre})
            i += 1
            continue

        # Les encadrés
        if ligne.startswith(">"):
            corps = []
            while i < n and lignes[i].startswith(">"):
                corps.append(re.sub(r"^> ?", "", lignes[i].rstrip()))
                i += 1
            sorte = "citation"
            if corps:
                mc = re.match(r"^\s*\[!(\w+)\]\s*(.*)$", corps[0])
                if mc:
                    sorte = mc.group(1).lower()
                    corps = ([mc.group(2)] if mc.group(2) else []) + corps[1:]
                    if sorte not in ("note", "essaie", "attention"):
                        avertir(nom, f"encadré inconnu : [!{sorte}]")
            blocs.append({"type": "encadre", "sorte": sorte,
                          "blocs": lire_blocs(corps, nom)})
            continue

        # Les listes
        m = RE_LISTE.match(ligne)
        if m:
            ordonnee = m.group(2)[0].isdigit()
            debut = int(m.group(2)[:-1]) if ordonnee else 1
            items = []
            while i < n:
                m = RE_LISTE.match(lignes[i].rstrip())
                if not m or m.group(2)[0].isdigit() != ordonnee:
                    break
                contenu = [m.group(3)]
                largeur = len(m.group(1)) + len(m.group(2)) + 1
                i += 1
                while i < n:
                    l = lignes[i].rstrip()
                    if not l.strip():
                        # Une ligne vide garde l'élément ouvert si la suite est en retrait.
                        j = i
                        while j < n and not lignes[j].strip():
                            j += 1
                        if j < n and lignes[j].startswith("  "):
                            contenu.extend([""] * (j - i))
                            i = j
                            continue
                        break
                    if l.startswith("  "):
                        # On retire le retrait de l'élément, pas plus.
                        retrait = len(l) - len(l.lstrip(" "))
                        contenu.append(l[min(retrait, largeur):])
                        i += 1
                        continue
                    if debut_de_bloc(l):
                        break
                    contenu.append(l)
                    i += 1
                items.append(lire_blocs(contenu, nom))
                # Une ligne vide entre deux éléments de la même liste
                j = i
                while j < n and not lignes[j].strip():
                    j += 1
                if j > i and j < n:
                    m2 = RE_LISTE.match(lignes[j].rstrip())
                    if m2 and m2.group(1) == "" and m2.group(2)[0].isdigit() == ordonnee:
                        i = j
            blocs.append({"type": "liste", "ordonnee": ordonnee, "debut": debut,
                          "items": items})
            continue

        # Un paragraphe
        corps = [ligne.strip()]
        i += 1
        while i < n and lignes[i].strip() and not debut_de_bloc(lignes[i]):
            corps.append(lignes[i].strip())
            i += 1
        blocs.append({"type": "p", "texte": " ".join(corps)})
    return blocs


def typographie(texte):
    """Espaces insécables du français, sur du texte brut."""
    texte = re.sub(r"(?<=\S) ([;!?»])", " \\1", texte)
    texte = re.sub(r"(?<=\S) :(?=\s|$)", " :", texte)
    texte = re.sub(r"« ", "« ", texte)
    return texte


def en_ligne(texte):
    """Le texte d'un paragraphe : code, liens, gras, italique."""
    morceaux = []

    def garder(fragment):
        morceaux.append(fragment)
        return f"\x00{len(morceaux) - 1}\x00"

    # Le code en ligne d'abord, pour que rien ne soit interprété dedans.
    def code(m):
        contenu = m.group(2)
        if contenu.startswith(" ") and contenu.endswith(" ") and contenu.strip():
            contenu = contenu[1:-1]
        return garder(f"<code>{e(contenu)}</code>")
    texte = re.sub(r"(`+)(.+?)\1", code, texte)
    texte = e(typographie(texte)).replace("&#x27;", "'")

    def lien(m):
        url = m.group(2).strip()
        externe = re.match(r"^https?://", url)
        attr = ' rel="noopener"' if externe else ""
        return f'<a href="{url}"{attr}>{m.group(1)}</a>'
    # Les liens (le texte du lien peut contenir du code déjà mis de côté)
    texte = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lien, texte)
    texte = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", texte)
    texte = re.sub(r"(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?![\w*])", r"<em>\1</em>", texte)
    texte = re.sub(r"(?<![\w_])_(?=\S)(.+?)(?<=\S)_(?![\w_])", r"<em>\1</em>", texte)
    while "\x00" in texte:
        texte = re.sub(r"\x00(\d+)\x00", lambda m: morceaux[int(m.group(1))], texte)
    return texte


def texte_brut(texte):
    """Le texte sans balises (pour les titres de la colonne et <title>)."""
    return re.sub(r"<[^>]+>", "", en_ligne(texte))


# ---------------------------------------------------------------------------
# La coloration du code, thème Dark+
# ---------------------------------------------------------------------------

C_CONTROLE, C_STOCKAGE, C_TYPE, C_APPEL = "#C586C0", "#569CD6", "#4EC9B0", "#DCDCAA"
C_CONSTANTE, C_NOMBRE, C_CHAINE, C_COMMENTAIRE = "#4FC1FF", "#B5CEA8", "#CE9178", "#6A9955"
C_VARIABLE, C_PONCTUATION, C_ECHAPPEMENT = "#9CDCFE", "#D4D4D4", "#D7BA7D"

C_MOTS_CONTROLE = {"return", "if", "else", "while", "for", "do", "break", "continue",
                   "switch", "case", "default", "goto"}
C_MOTS_STOCKAGE = {"static", "struct", "typedef", "const", "void", "int", "char", "bool",
                   "double", "float", "long", "short", "unsigned", "signed", "sizeof",
                   "enum", "union", "extern", "inline", "volatile", "register", "auto",
                   "restrict", "_Bool", "true", "false", "NULL"}

CF_MOTS_CONTROLE = {"if", "else", "return", "match", "case", "for", "in", "while",
                    "break", "continue"}
CF_MOTS_STOCKAGE = {"func", "var", "const", "struct", "enum", "new", "true", "false",
                    "none", "self", "lfunc", "and", "or", "from", "import", "export"}
CF_TYPES = {"int", "float", "str", "string", "bool", "char", "list", "map", "func_t",
            "any", "none_t"}

RE_C = re.compile(r"""
 (?P<com>//[^\n]*|/\*[\s\S]*?(?:\*/|\Z))
|(?P<pre>^[ \t]*\#[ \t]*[A-Za-z_]+)
|(?P<str>"(?:[^"\\\n]|\\.)*"?)
|(?P<chr>'(?:[^'\\\n]|\\.)*'?)
|(?P<num>\b(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)[uUlLfF]*\b|\.\d+\b)
|(?P<id>[A-Za-z_]\w*)
|(?P<esp>\s+)
|(?P<op>->|::|.)
""", re.X | re.M)

RE_CF = re.compile(r"""
 (?P<com>\A\#![^\n]*|//[^\n]*|/\*[\s\S]*?(?:\*/|\Z))
|(?P<str>"(?:[^"\\\n]|\\.)*"?)
|(?P<chr>'(?:[^'\\\n]|\\.)*'?)
|(?P<num>\b\d+(?:\.\d+)?\b)
|(?P<id>[A-Za-z_]\w*)
|(?P<esp>\s+)
|(?P<op>->|::|.)
""", re.X)


def colorer_chaine(texte, interpolation=False):
    """Une chaîne : échappements en #D7BA7D, et ${...} recoloré pour CeFlo."""
    sortie = []
    motif = r"(\\.|\$\{[^}\n]*\})" if interpolation else r"(\\.)"
    for morceau in re.split(motif, texte):
        if not morceau:
            continue
        if morceau.startswith("\\"):
            sortie.append((C_ECHAPPEMENT, morceau))
        elif interpolation and morceau.startswith("${"):
            sortie.append((C_STOCKAGE, "${"))
            sortie.extend(colorer_ceflo(morceau[2:-1]))
            sortie.append((C_STOCKAGE, "}"))
        else:
            sortie.append((C_CHAINE, morceau))
    return sortie


def suivi_dune_parenthese(source, fin):
    return re.match(r"[ \t]*\(", source[fin:]) is not None


def colorer_c(source):
    jetons = []
    precedent = ""      # le dernier jeton significatif
    directive = ""
    pos = 0
    while pos < len(source):
        # Après #include, le <fichier.h> est une chaîne.
        if directive == "include" and source[pos] == "<":
            m = re.match(r"<[^>\n]*>", source[pos:])
            if m:
                jetons.append((C_CHAINE, m.group(0)))
                pos += len(m.group(0))
                directive = ""
                continue
        m = RE_C.match(source, pos)
        sorte, texte = m.lastgroup, m.group(0)
        pos = m.end()
        if sorte == "com":
            jetons.append((C_COMMENTAIRE, texte))
            continue
        if sorte == "esp":
            if "\n" in texte:
                directive = ""
            jetons.append((C_PONCTUATION, texte))
            continue
        if sorte == "pre":
            directive = re.sub(r"[\s#]", "", texte)
            jetons.append((C_CONTROLE, texte))
        elif sorte in ("str", "chr"):
            jetons.extend(colorer_chaine(texte))
        elif sorte == "num":
            jetons.append((C_NOMBRE, texte))
        elif sorte == "id":
            if precedent in (".", "->"):
                couleur = C_VARIABLE
            elif texte in C_MOTS_CONTROLE:
                couleur = C_CONTROLE
            elif texte in C_MOTS_STOCKAGE:
                couleur = C_STOCKAGE
            elif texte.endswith("_t") or texte == "TokenEnum":
                couleur = C_TYPE
            elif re.fullmatch(r"[A-Z][A-Z0-9_]+", texte):
                couleur = C_CONSTANTE
            elif suivi_dune_parenthese(source, pos):
                couleur = C_APPEL
            elif re.fullmatch(r"[A-Z][a-z0-9]+(?:[A-Z][a-z0-9]*)*", texte):
                couleur = C_TYPE        # ObjString, CallFrame...
            else:
                couleur = C_VARIABLE
            jetons.append((couleur, texte))
        else:
            jetons.append((C_PONCTUATION, texte))
        precedent = texte
    return jetons


def colorer_ceflo(source):
    jetons = []
    precedent = ""
    pos = 0
    while pos < len(source):
        m = RE_CF.match(source, pos)
        sorte, texte = m.lastgroup, m.group(0)
        pos = m.end()
        if sorte == "com":
            jetons.append((C_COMMENTAIRE, texte))
            continue
        if sorte == "esp":
            jetons.append((C_PONCTUATION, texte))
            continue
        if sorte == "str":
            jetons.extend(colorer_chaine(texte, interpolation=True))
        elif sorte == "chr":
            jetons.extend(colorer_chaine(texte))
        elif sorte == "num":
            jetons.append((C_NOMBRE, texte))
        elif sorte == "id":
            if precedent == ".":
                couleur = C_CONSTANTE if re.fullmatch(r"[A-Z][A-Z0-9_]*", texte) else (
                    C_APPEL if suivi_dune_parenthese(source, pos) else C_VARIABLE)
            elif texte in CF_MOTS_CONTROLE:
                couleur = C_CONTROLE
            elif texte in CF_MOTS_STOCKAGE:
                couleur = C_STOCKAGE
            elif precedent in ("::", "@", ":") and texte in CF_TYPES:
                couleur = C_TYPE
            elif precedent == "new" or precedent in ("struct", "enum"):
                couleur = C_TYPE
            elif re.fullmatch(r"[A-Z][A-Z0-9_]+", texte):
                couleur = C_CONSTANTE
            elif precedent == "func" or suivi_dune_parenthese(source, pos):
                couleur = C_APPEL
            elif texte[0].isupper():
                couleur = C_TYPE
            else:
                couleur = C_VARIABLE
            jetons.append((couleur, texte))
        else:
            jetons.append((C_PONCTUATION, texte))
        precedent = texte
    return jetons


PY_MOTS_CONTROLE = {"if", "elif", "else", "for", "while", "return", "import", "from", "break", "continue", "try", "except", "with", "yield", "pass", "raise", "in"}
PY_MOTS_STOCKAGE = {"def", "class", "lambda", "nonlocal", "global", "None", "True", "False", "and", "or", "not", "is"}
RE_PY = re.compile(r"""(?P<com>#[^\n]*)|(?P<str>"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')|(?P<num>\b\d+(?:\.\d+)?\b)|(?P<id>[A-Za-z_]\w*)|(?P<esp>\s+)|(?P<autre>.)""")


def colorer_python(source):
    """Python, pour les comparaisons : mots-clés, chaînes, nombres, commentaires #."""
    jetons = []
    pos = 0
    while pos < len(source):
        m = RE_PY.match(source, pos)
        sorte, texte = m.lastgroup, m.group(0)
        pos = m.end()
        if sorte == "com":
            jetons.append((C_COMMENTAIRE, texte))
        elif sorte == "str":
            jetons.append((C_CHAINE, texte))
        elif sorte == "num":
            jetons.append((C_NOMBRE, texte))
        elif sorte == "id":
            if texte in PY_MOTS_CONTROLE:
                jetons.append((C_CONTROLE, texte))
            elif texte in PY_MOTS_STOCKAGE:
                jetons.append((C_STOCKAGE, texte))
            elif suivi_dune_parenthese(source, pos):
                jetons.append((C_APPEL, texte))
            else:
                jetons.append((C_VARIABLE, texte))
        else:
            jetons.append((C_PONCTUATION, texte))
    return jetons


RE_MAKE_EXPR = re.compile(r"""(?P<var>\$\((?P<fn>[\w.-]+)?|\$[@<^*?%]|\))|(?P<str>"[^"\n]*"|'[^'\n]*')|(?P<autre>[^$)"']+|.)""")


def colorer_makefile(source):
    """Un Makefile : commentaires, variables, cibles et $(...)."""
    jetons = []
    for k, ligne in enumerate(source.split("\n")):
        if k:
            jetons.append((C_PONCTUATION, "\n"))
        texte, commentaire = ligne, ""
        m = re.match(r"^([^#]*?)(\s*#.*)$", ligne)
        if m and not ligne.startswith("\t"):
            texte, commentaire = m.group(1), m.group(2)
        m = re.match(r"^(\s*)([\w.-]+)(\s*(?:[:+?]?=))", texte)
        if m:
            jetons += [(C_PONCTUATION, m.group(1)), (C_VARIABLE, m.group(2)), (C_PONCTUATION, m.group(3))]
            texte = texte[m.end():]
        else:
            m = re.match(r"^([^\s=#:\t][^=#:]*?)(\s*::?)(?!=)", texte)
            if m:
                jetons += [(C_APPEL, m.group(1)), (C_PONCTUATION, m.group(2))]
                texte = texte[m.end():]
        for mm in RE_MAKE_EXPR.finditer(texte):
            if mm.group("var"):
                if mm.group("fn") and mm.group("fn") in ("addprefix", "wildcard", "patsubst", "filter-out",
                                                         "filter", "shell", "subst", "foreach", "notdir",
                                                         "basename", "dir", "sort", "addsuffix", "if"):
                    jetons += [(C_STOCKAGE, "$("), (C_APPEL, mm.group("fn"))]
                elif mm.group("fn"):
                    jetons += [(C_STOCKAGE, "$("), (C_VARIABLE, mm.group("fn"))]
                else:
                    jetons.append((C_STOCKAGE, mm.group(0)))
            elif mm.group("str"):
                jetons.append((C_CHAINE, mm.group(0)))
            else:
                jetons.append((C_PONCTUATION, mm.group(0)))
        if commentaire:
            jetons.append((C_COMMENTAIRE, commentaire))
    return jetons


def en_lignes(jetons):
    """Coupe une suite de jetons (couleur, texte) en lignes."""
    lignes = [[]]
    for couleur, texte in jetons:
        parties = texte.split("\n")
        for k, partie in enumerate(parties):
            if k:
                lignes.append([])
            if partie:
                lignes[-1].append((couleur, partie))
    return lignes


def spans(jetons):
    sortie = []
    for couleur, texte in jetons:
        if couleur == C_PONCTUATION and not texte.strip():
            sortie.append(e(texte))
        else:
            sortie.append(f'<span style="color:{couleur}">{e(texte)}</span>')
    return "".join(sortie)


def lire_intervalles(texte):
    """« 191-196,200 » donne {191, ..., 196, 200}."""
    nombres = set()
    for morceau in re.split(r"[,\s]+", texte or ""):
        m = re.fullmatch(r"(\d+)(?:-(\d+))?", morceau)
        if m:
            a = int(m.group(1))
            b = int(m.group(2) or a)
            nombres.update(range(a, b + 1))
    return nombres


# ---------------------------------------------------------------------------
# Le rendu des blocs
# ---------------------------------------------------------------------------

ICONES = {
    "accueil": '<path d="M1 6 7 1l6 5v7H9V9H5v4H1z" fill="#111111"/>',
    "sommaire": '<path d="M1 2h12M1 7h12M1 12h8" stroke="#111111" stroke-width="2"/>',
    "chapitres": '<path d="M2 1h7l3 3v9H2z" fill="#111111"/>',
    "code": '<path d="M5 3 1 7l4 4M9 3l4 4-4 4" stroke="#111111" stroke-width="2" fill="none"/>',
}
FLECHES = ('<svg width="18" height="18" viewBox="0 0 18 18" aria-hidden="true">'
           '<path d="M3 3l6 6-6 6M9 3l6 6-6 6" stroke="#111111" stroke-width="2.4" fill="none"/></svg>')
FLECHES_GAUCHE = ('<svg width="18" height="18" viewBox="0 0 18 18" aria-hidden="true">'
                  '<path d="M15 3l-6 6 6 6M9 3l-6 6 6 6" stroke="#111111" stroke-width="2.4" fill="none"/></svg>')
ICONE_CRAYON = ('<svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true">'
                '<path d="M2 14l1-4 8-8 3 3-8 8z" fill="#111111"/></svg>')
ICONE_ATTENTION = ('<svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true">'
                   '<path d="M8 1l7 13H1z" fill="#111111"/><path d="M8 6v4M8 11.5v1.2" stroke="'
                   + ROSE + '" stroke-width="1.8"/></svg>')


ICONE_AMPOULE = ('<svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true">'
                 '<path d="M8 1a5 5 0 0 0-3 9v2h6v-2a5 5 0 0 0-3-9zM5.5 14h5v1.5h-5z" fill="#111111"/></svg>')


def icone(nom):
    return f'<svg width="14" height="14" viewBox="0 0 14 14" aria-hidden="true">{ICONES[nom]}</svg>'


def rendre_code(bloc, nom):
    langue = bloc["langue"]
    attrs = bloc["attrs"]
    texte = bloc["texte"]
    if langue in ("text", "texte", "txt", ""):
        # Un dessin en texte : monospace, fond clair, sans couleurs.
        return f'<pre class="dessin" tabindex="0">{e(texte)}</pre>'
    if langue in ("sh", "shell", "bash", "terminal", "console"):
        return rendre_terminal(texte, commande_seule=(langue != "terminal" and langue != "console"))

    if langue == "c" or langue == "h":
        jetons = colorer_c(texte)
    elif langue in ("ceflo", "cf"):
        jetons = colorer_ceflo(texte)
    elif langue in ("makefile", "make"):
        jetons = colorer_makefile(texte)
    elif langue in ("python", "py"):
        jetons = colorer_python(texte)
    else:
        if langue:
            avertir(nom, f"langage inconnu : {langue}, rendu sans couleurs")
        jetons = [(C_PONCTUATION, texte)]
    lignes = en_lignes(jetons)

    premier = 1
    if attrs.get("lignes"):
        m = re.fullmatch(r"\s*(\d+)(?:\s*-\s*(\d+))?\s*", attrs["lignes"])
        if m:
            premier = int(m.group(1))
            if m.group(2) and int(m.group(2)) - premier + 1 != len(lignes):
                avertir(nom, f"{attrs.get('fichier', '?')} lignes={attrs['lignes']} "
                             f"annonce {int(m.group(2)) - premier + 1} lignes, le bloc en a {len(lignes)}")
        else:
            avertir(nom, f"lignes illisibles : {attrs['lignes']}")
    focus = lire_intervalles(attrs.get("focus"))

    fichier = attrs.get("fichier", "")
    onglet = fichier.split("/")[-1] if fichier else {"c": "extrait.c", "ceflo": "programme.cf"}.get(langue, "extrait")
    ariane = ""
    if fichier:
        morceaux = [p for p in fichier.split("/") if p]
        if attrs.get("fonction"):
            morceaux.append(attrs["fonction"])
        separateur = '<span aria-hidden="true">  ›  </span>'
        ariane = f'<div class="ariane">{separateur.join(e(p) for p in morceaux)}</div>'

    sortie = []
    for k, ligne in enumerate(lignes):
        numero = premier + k
        classe = "l f" if numero in focus else "l"
        sortie.append(f'<span class="{classe}" data-n="{numero}">{spans(ligne) or " "}</span>')
    langue_nom = {"c": "C", "h": "C", "ceflo": "CeFlo", "cf": "CeFlo", "makefile": "Makefile", "make": "Makefile"}.get(langue, langue or "texte")
    etiquette = f"Code {langue_nom}" + (f", {fichier}" if fichier else "")
    if attrs.get("lignes"):
        etiquette += f", lignes {attrs['lignes']}"
    return (f'<figure class="editeur" aria-label="{e(etiquette)}">'
            f'<div class="onglets"><span class="onglet">{e(onglet)}</span></div>'
            f'{ariane}'
            f'<pre class="defile" tabindex="0"><code>{chr(10).join(sortie)}</code></pre></figure>')


def ligne_terminal(ligne, commande_seule):
    """Colore une ligne de terminal."""
    m = re.match(r"^(\s*\$)( ?)(.*)$", ligne)
    if m:
        return (f'<span class="invite">{e(m.group(1))}{m.group(2)}</span>'
                f'<span class="cmd">{colorer_commande(m.group(3))}</span>')
    if commande_seule:
        if not ligne.strip():
            return ""
        if ligne.lstrip().startswith("#"):
            return f'<span style="color:{C_COMMENTAIRE}">{e(ligne)}</span>'
        return f'<span class="invite">$ </span><span class="cmd">{colorer_commande(ligne)}</span>'
    m = re.match(r"^(\s*(?:erreur|error|Erreur|Error)[^:]*)(:?)(.*)$", ligne)
    if m:
        return f'<span class="err">{e(m.group(1))}</span>{e(m.group(2) + m.group(3))}'
    m = re.match(r"^(\s*-->.*)$", ligne)
    if m:
        return f'<span class="gris">{e(ligne)}</span>'
    m = re.match(r"^(\s*\d*\s*\|)(.*)$", ligne)
    if m:
        reste = m.group(2)
        mm = re.fullmatch(r"(\s*)([\^~]+)(.*)", reste)
        if mm:
            reste_html = f'{e(mm.group(1))}<span class="err">{e(mm.group(2))}</span>{e(mm.group(3))}'
        else:
            reste_html = e(reste)
        return f'<span class="gris">{e(m.group(1))}</span>{reste_html}'
    return e(ligne)


def colorer_commande(commande):
    """Une commande : le programme en jaune, les options en bleu, les chaînes en orange."""
    sortie = []
    premier = True
    for morceau in re.findall(r"\s+|\"[^\"]*\"?|'[^']*'?|[^\s]+", commande):
        if not morceau.strip():
            sortie.append(e(morceau))
        elif morceau[0] in "\"'":
            sortie.append(f'<span style="color:{C_CHAINE}">{e(morceau)}</span>')
        elif morceau.startswith("#"):
            sortie.append(f'<span style="color:{C_COMMENTAIRE}">{e(morceau)}</span>')
            premier = False
        elif premier:
            sortie.append(f'<span style="color:{C_APPEL}">{e(morceau)}</span>')
            premier = False
        elif morceau.startswith("-"):
            sortie.append(f'<span style="color:{C_STOCKAGE}">{e(morceau)}</span>')
        else:
            sortie.append(e(morceau))
            if morceau in ("&&", "||", "|", ";"):
                premier = True
    return "".join(sortie)


def rendre_terminal(texte, commande_seule):
    lignes = [ligne_terminal(l, commande_seule) for l in texte.split("\n")]
    if commande_seule:
        lignes = [l for l in lignes if l]
    corps = "\n".join(f'<span class="l">{l or " "}</span>' for l in lignes)
    return ('<figure class="terminal" aria-label="Terminal">'
            '<div class="onglets-term" aria-hidden="true"><span>PROBLÈMES</span><span>SORTIE</span>'
            '<span class="actif">TERMINAL</span></div>'
            f'<pre class="defile" tabindex="0"><code>{corps}</code></pre></figure>')


def rendre_jetons(bloc, nom):
    items = []
    for k, ligne in enumerate(bloc["lignes"]):
        morceaux = ligne.strip().rsplit(None, 1)
        if len(morceaux) == 1:
            avertir(nom, f"jeton sans nom : {ligne.strip()}")
            morceaux.append("")
        texte, sorte = morceaux
        couleur = COULEURS_JETONS[k % 4]
        angle = ANGLES_JETONS[k % 4]
        items.append(f'<li><span class="jeton" style="background:{couleur};transform:rotate({angle})">'
                     f'{e(texte)}</span><span class="sorte">{e(sorte)}</span></li>')
    legende = f'<figcaption class="main">{e(typographie(bloc["legende"]))}</figcaption>' if bloc["legende"] else ""
    return f'<figure class="fig-jetons"><ol class="jetons">{"".join(items)}</ol>{legende}</figure>'


def rendre_etapes(bloc):
    etapes = [p.strip() for p in " ".join(bloc["lignes"]).split("->") if p.strip()]
    items = []
    for k, etape in enumerate(etapes):
        couleur = COULEURS_JETONS[k % 4]
        angle = ANGLES_JETONS[k % 4]
        items.append(f'<li><span class="etape" style="background:{couleur};transform:rotate({angle})">'
                     f'{en_ligne(etape)}</span></li>')
    legende = f'<figcaption class="main">{e(typographie(bloc["legende"]))}</figcaption>' if bloc["legende"] else ""
    return f'<figure class="fig-etapes"><ol class="etapes">{"".join(items)}</ol>{legende}</figure>'


def rendre_simple(blocs, nom):
    """Le contenu d'un encadré ou d'un élément de liste."""
    sortie = []
    for bloc in blocs:
        genre, h = rendre_bloc(bloc, nom)
        sortie.append(h)
    return "".join(sortie)


def rendre_liste(bloc, nom):
    items = []
    for item in bloc["items"]:
        if len(item) == 1 and item[0]["type"] == "p":
            items.append(f"<li>{en_ligne(item[0]['texte'])}</li>")
        else:
            items.append(f"<li>{rendre_simple(item, nom)}</li>")
    if bloc["ordonnee"]:
        debut = f' start="{bloc["debut"]}"' if bloc["debut"] != 1 else ""
        return f'<ol{debut}>{"".join(items)}</ol>'
    return f'<ul>{"".join(items)}</ul>'


def ancre(numero, titre):
    if numero:
        return "s" + numero.replace(".", "-")
    return "s-" + re.sub(r"[^a-z0-9]+", "-", titre.lower()).strip("-")


def rendre_bloc(bloc, nom):
    """Renvoie (genre, html) ; le genre place le bloc dans la grille."""
    t = bloc["type"]
    if t == "p":
        return "texte", f"<p>{en_ligne(bloc['texte'])}</p>"
    if t == "h2":
        a = ancre(bloc["numero"], bloc["titre"])
        num = f'<a class="num" href="#{a}">{e(bloc["numero"])}</a> ' if bloc["numero"] else ""
        sorte = SECTIONS_FIN.get(bloc["titre"].strip().lower()) if not bloc["numero"] else None
        if sorte:
            pic = ICONE_CRAYON if sorte == "defis" else ICONE_AMPOULE
            return "titre", (f'<h2 id="{a}" class="h2-fin"><a class="pic-fin" href="#{a}" aria-hidden="true" tabindex="-1">'
                             f'{pic}</a>{en_ligne(bloc["titre"])}</h2>')
        return "titre", f'<h2 id="{a}">{num}{en_ligne(bloc["titre"])}</h2>'
    if t == "section":
        return "texte", rendre_section(bloc, nom)
    if t == "h3":
        return "texte", f"<h3>{en_ligne(bloc['titre'])}</h3>"
    if t == "liste":
        return "texte", rendre_liste(bloc, nom)
    if t == "code":
        return "large", rendre_code(bloc, nom)
    if t == "figure":
        if bloc["sorte"] == "etapes":
            return "large", rendre_etapes(bloc)
        return "large", rendre_jetons(bloc, nom)
    if t == "encadre":
        contenu = rendre_simple(bloc["blocs"], nom)
        if bloc["sorte"] == "note":
            return "note", contenu
        if bloc["sorte"] == "essaie":
            return "texte", (f'<aside class="encadre essaie" aria-label="À toi">'
                             f'<span class="etiquette-noire"><span class="carre">{ICONE_CRAYON}</span>À toi</span>'
                             f'{contenu}</aside>')
        if bloc["sorte"] == "attention":
            return "texte", (f'<aside class="encadre attention" aria-label="Attention">'
                             f'<span class="etiquette-noire"><span class="carre rose">{ICONE_ATTENTION}</span>Attention</span>'
                             f'{contenu}</aside>')
        return "texte", f"<blockquote>{contenu}</blockquote>"
    return "texte", ""


def hauteur_estimee(fragment, par_ligne, interligne, en_plus):
    """Une hauteur grossière en pixels, d'après la longueur du texte."""
    texte = html.unescape(re.sub(r"<[^>]+>", "", fragment))
    return -(-max(len(texte), 1) // par_ligne) * interligne + en_plus


SECTIONS_FIN = {"défis": "defis", "defis": "defis", "note de conception": "conception"}


def sorte_de_fin(titre):
    """« Défis » ou « Note de conception », suivis ou non d'un sous-titre après « : »."""
    debut = titre.split(":")[0].strip().lower()
    return SECTIONS_FIN.get(debut)


def regrouper_sections(blocs):
    """Range ce qui suit « ## Défis » ou « ## Note de conception » dans un seul bloc."""
    sortie = []
    courant = None
    for bloc in blocs:
        if bloc["type"] == "h2":
            courant = None
            sorte = sorte_de_fin(bloc["titre"]) if not bloc["numero"] else None
            if sorte:
                courant = {"type": "section", "sorte": sorte, "titre": bloc["titre"], "blocs": []}
                sortie.append(bloc)
                sortie.append(courant)
                continue
        if courant is not None:
            courant["blocs"].append(bloc)
        else:
            sortie.append(bloc)
    return sortie


def rendre_section(bloc, nom):
    contenu = bloc["blocs"]
    if bloc["sorte"] == "conception":
        corps = rendre_contenu_section(contenu, nom)
        return (f'<div class="conception"><span class="main conception-note" aria-hidden="true">pourquoi c\'est fait comme ça</span>'
                f'{corps}</div>')
    # Les défis : un par ###, sinon un par élément de liste, sinon un par paragraphe.
    intro, defis = [], []
    if any(b["type"] == "h3" for b in contenu):
        for b in contenu:
            if b["type"] == "h3":
                defis.append({"titre": b["titre"], "blocs": []})
            elif defis:
                defis[-1]["blocs"].append(b)
            else:
                intro.append(b)
    elif any(b["type"] == "liste" for b in contenu):
        vu = False
        for b in contenu:
            if b["type"] == "liste" and not vu:
                vu = True
                defis = [{"titre": "", "blocs": item} for item in b["items"]]
            elif not vu:
                intro.append(b)
            else:
                defis[-1]["blocs"].append(b)
    else:
        for b in contenu:
            if b["type"] == "p" or not defis:
                defis.append({"titre": "", "blocs": [b]})
            else:
                defis[-1]["blocs"].append(b)
    items = []
    for k, d in enumerate(defis):
        couleur = COULEURS_JETONS[k % 4]
        angle = ANGLES_JETONS[k % 4]
        titre = f'<h3>{en_ligne(d["titre"])}</h3>' if d["titre"] else ""
        items.append(f'<li class="defi"><span class="etiquette-defi" style="background:{couleur};transform:rotate({angle})">'
                     f'Défi {k + 1}</span><div class="defi-corps">{titre}{rendre_contenu_section(d["blocs"], nom)}</div></li>')
    return f'{rendre_contenu_section(intro, nom)}<ol class="defis">{"".join(items)}</ol>'


def rendre_contenu_section(blocs, nom):
    """Comme rendre_simple, mais une note y devient un post-it en ligne."""
    sortie = []
    for b in blocs:
        genre, h = rendre_bloc(b, nom)
        if genre == "note":
            h = f'<aside class="postit en-ligne" aria-label="Note">{h}</aside>'
        sortie.append(h)
    return "".join(sortie)


def rendre_corps(blocs, nom):
    """Place les blocs dans la grille : texte à gauche, notes dans la marge."""
    blocs = regrouper_sections(blocs)
    rangees = []  # [genre, html, [notes]]
    en_attente = []  # notes qui suivent un bloc large : on les met à la rangée suivante
    chapeau = True
    for bloc in blocs:
        genre, h = rendre_bloc(bloc, nom)
        if bloc["type"] == "h2":
            chapeau = False
        elif bloc["type"] == "p" and chapeau:
            h = h.replace("<p>", '<p class="chapeau">', 1)
            chapeau = False
        if genre == "note":
            if rangees and rangees[-1][0] == "texte" and not en_attente:
                rangees[-1][2].append(h)
            else:
                en_attente.append(h)
            continue
        if en_attente and genre == "texte":
            rangees.append([genre, h, en_attente])
            en_attente = []
            continue
        if en_attente:
            rangees.append(["vide", "", en_attente])
            en_attente = []
        rangees.append([genre, h, []])
    if en_attente:
        rangees.append(["vide", "", en_attente])

    sortie = []
    nb_notes = 0
    for r, (genre, h, notes) in enumerate(rangees, start=1):
        if genre != "vide":
            classe = {"texte": "b-texte", "titre": "b-texte b-titre", "large": "b-large"}[genre]
            sortie.append(f'<div class="{classe}" style="--r:{r}">{h}</div>')
        if notes:
            # Une note plus haute que son paragraphe déborde sur les paragraphes
            # suivants (sans note), plutôt que d'ouvrir un grand blanc.
            besoin = sum(hauteur_estimee(n, 28, 22.5, 50) for n in notes)
            dispo = hauteur_estimee(h, 70, 32, 22) if genre != "vide" else 0
            etendue = 1
            while dispo < besoin and r + etendue <= len(rangees):
                g2, h2, n2 = rangees[r + etendue - 1]
                if g2 not in ("texte", "titre") or n2:
                    break
                dispo += 90 if g2 == "titre" else hauteur_estimee(h2, 70, 32, 22)
                etendue += 1
            postits = []
            for note in notes:
                angle = "1.5deg" if nb_notes % 2 == 0 else "-1.5deg"
                nb_notes += 1
                postits.append(f'<aside class="postit" style="--a:{angle}" aria-label="Note">{note}</aside>')
            sortie.append(f'<div class="marge" style="--r:{r};--s:{etendue}">{"".join(postits)}</div>')
    return "\n".join(sortie)


# ---------------------------------------------------------------------------
# Les pages
# ---------------------------------------------------------------------------

FAVICON = ("data:image/svg+xml," + re.sub(r"\s+", " ", f"""
<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect x='1' y='1' width='30' height='30'
fill='%2334C1EE' stroke='%23111' stroke-width='2'/><text x='16' y='23' font-family='Arial Black,sans-serif'
font-weight='900' font-size='22' text-anchor='middle' fill='%23111'>c</text></svg>""").strip())


def tete(titre, description):
    return f"""<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{e(titre)}</title>
<meta name="description" content="{e(description)}">
<link rel="icon" href="{FAVICON}">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="{e(POLICES)}">
<link rel="stylesheet" href="style.css">
</head>
<body>
<a class="evitement" href="#contenu">Aller au texte</a>
"""


def barre(actif, droite, lien_chapitres):
    liens = [("accueil", "index.html", "Accueil"),
             ("sommaire", "index.html#sommaire", "Sommaire"),
             ("chapitres", lien_chapitres, "Chapitres"),
             ("code", DEPOT_EN_LIGNE, "Le code")]
    items = []
    for cle, href, texte in liens:
        classe = ' class="actif" aria-current="page"' if cle == actif else ""
        items.append(f'<a href="{e(href)}"{classe}>{icone(cle)}{texte}</a>')
    return (f'<header class="barre"><div class="barre-int">'
            f'<nav class="nav-livre" aria-label="Livre">{"".join(items)}</nav>{droite}</div></header>\n'
            + regle())


def regle():
    marques = "".join(f"<span>{k}</span>" for k in range(0, 2100, 100))
    return f'<div class="regle" aria-hidden="true">{marques}</div>\n'


def cadre(contenu, classe=""):
    poignees = "".join(f'<span class="p {p}" aria-hidden="true"></span>' for p in ("hg", "hd", "bg", "bd"))
    return f'<div class="cadre {classe}">{poignees}{contenu}</div>'


def sommaire_complet(plan, chapitres):
    """Les parties du plan, complétées par les chapitres présents hors plan."""
    parties = []
    vus = set()
    for k, partie in enumerate(plan):
        liste = []
        for prevu in partie["chapitres"]:
            ch = chapitres.get(prevu["numero"])
            liste.append({"numero": prevu["numero"],
                          "titre": ch["titre"] if ch else prevu["titre"],
                          "resume": (ch and ch["resume"]) or RESUMES.get(prevu["numero"]) or (ch and ch["note"]) or "",
                          "chapitre": ch})
            vus.add(prevu["numero"])
        parties.append({"label": f"Partie {partie['numero']}", "titre": partie["titre"],
                        "couleur": COULEURS_PARTIES[k % 4], "chapitres": liste})
    # Des chapitres qui ne sont pas dans le plan : rangés par leur « partie ».
    for num in sorted(chapitres):
        if num in vus:
            continue
        ch = chapitres[num]
        partie = next((p for p in parties if p["titre"].lower() == ch["partie"].lower()), None)
        if partie is None:
            partie = {"label": "Hors plan", "titre": ch["partie"] or "Autres",
                      "couleur": COULEURS_PARTIES[len(parties) % 4], "chapitres": []}
            parties.append(partie)
        partie["chapitres"].append({"numero": num, "titre": ch["titre"],
                                    "resume": ch["resume"] or ch["note"], "chapitre": ch})
        partie["chapitres"].sort(key=lambda c: c["numero"])
    return parties


def page_chapitre(ch, parties, ordre, total):
    nom = ch["source"]
    corps = rendre_corps(ch["blocs"], nom)
    sous = [b for b in ch["blocs"] if b["type"] == "h2"]

    # La colonne de gauche
    colonne = []
    for partie in parties:
        lignes = []
        for c in partie["chapitres"]:
            numero = f'<span class="n">{nn(c["numero"])}</span>'
            titre = e(majuscule(c["titre"]))
            if c["chapitre"] is ch:
                sous_liens = "".join(
                    f'<li><a href="#{ancre(b["numero"], b["titre"])}"><span class="n">{e(b["numero"])}</span>'
                    f'<span>{e(texte_brut(b["titre"]))}</span></a></li>' for b in sous)
                sous_liste = f'<ol class="sous">{sous_liens}</ol>' if sous else ""
                lignes.append(f'<li class="courant"><a href="#contenu" aria-current="page">{numero}<span>{titre}</span></a>{sous_liste}</li>')
            elif c["chapitre"]:
                lignes.append(f'<li><a href="{e(c["chapitre"]["page"])}">{numero}<span>{titre}</span></a></li>')
            else:
                lignes.append(f'<li class="absent">{numero}<span>{titre} <span class="bientot">bientôt</span></span></li>')
        colonne.append(f'<div class="partie"><span class="etiquette" style="background:{partie["couleur"]}">'
                       f'{e(partie["titre"])}</span><ol>{"".join(lignes)}</ol></div>')

    # Précédent et suivant
    k = ordre.index(ch)
    if k > 0:
        p = ordre[k - 1]
        precedent = (f'<a class="prec" href="{e(p["page"])}" rel="prev"><span class="petit">Chapitre {nn(p["numero"])}</span>'
                     f'<span class="grand">{e(majuscule(p["titre"]))}</span></a>')
    else:
        precedent = ('<a class="prec" href="index.html#sommaire"><span class="petit">Le livre</span>'
                     '<span class="grand">Le sommaire</span></a>')
    if k + 1 < len(ordre):
        s = ordre[k + 1]
        suivant = (f'<a class="suiv" href="{e(s["page"])}" rel="next"><span class="carre">{FLECHES}</span>'
                   f'<span class="txt"><span class="petit">Chapitre {nn(s["numero"])}</span>'
                   f'<span class="grand">{e(majuscule(s["titre"]))}</span></span></a>')
    else:
        prochain = next((c for p in parties for c in p["chapitres"]
                         if c["numero"] > ch["numero"] and not c["chapitre"]), None)
        if prochain:
            suivant = (f'<div class="suiv attente"><span class="txt"><span class="petit">Chapitre {nn(prochain["numero"])}, bientôt</span>'
                       f'<span class="grand">{e(majuscule(prochain["titre"]))}</span></span></div>')
        else:
            suivant = (f'<a class="suiv" href="index.html"><span class="carre">{FLECHES}</span>'
                       f'<span class="txt"><span class="petit">Fin du livre</span><span class="grand">Accueil</span></span></a>')

    note = f'<span class="main note-titre">{e(typographie(ch["note"]))}</span>' if ch["note"] else ""
    titre_page = f"Chapitre {ch['numero']}, {ch['titre']} · Écrire CeFlo"
    droite = f'<span class="compteur">Chapitre {nn(ch["numero"])} / {nn(total)}</span>'
    return (tete(titre_page, ch["note"] or ch["titre"])
            + barre("chapitres", droite, ch["page"])
            + f"""<div class="grille">
<div class="page">
<nav class="colonne" aria-label="Chapitres">{"".join(colonne)}</nav>
<main class="article" id="contenu">
<div class="entete">
<div class="ligne-etiquette"><span class="etiquette grande" style="background:{MENTHE}">Chapitre {nn(ch["numero"])}</span>{note}</div>
{cadre(f'<h1>{e(ch["titre"])}</h1>', "cadre-chapitre")}
</div>
<div class="corps">
{corps}
</div>
<nav class="pied" aria-label="Chapitre précédent et suivant">{precedent}{suivant}</nav>
</main>
</div>
</div>
</body>
</html>
""")


def compter_tests():
    """Les tests de tests/run.sh : sorties attendues, erreurs et exemples."""
    tests = DEPOT / "tests"
    if not tests.is_dir():
        return 0
    n = len(list(tests.glob("*.out"))) + len(list(tests.glob("erreurs/*.cf")))
    n += len(list(tests.glob("vm/*.out"))) + len(list(tests.glob("vm/erreurs/*.cf")))
    n += len(list((DEPOT / "exemple").glob("*.cf")))
    return n


def page_accueil(parties, ordre, total):
    premier = ordre[0]["page"] if ordre else "#sommaire"
    cartes = []
    for partie in parties:
        lignes = []
        for c in partie["chapitres"]:
            interieur = (f'<span class="n">{nn(c["numero"])}</span><span class="txt">'
                         f'<span class="titre">{e(majuscule(c["titre"]))}</span>'
                         f'<span class="resume">{en_ligne(c["resume"])}</span></span>')
            if c["chapitre"]:
                lignes.append(f'<li><a href="{e(c["chapitre"]["page"])}">{interieur}</a></li>')
            else:
                lignes.append(f'<li class="absent"><div>{interieur}<span class="bientot">bientôt</span></div></li>')
        cartes.append(f'<section class="carte" aria-labelledby="p-{e(partie["label"]).replace(" ", "")}">'
                      f'<div class="carte-tete" style="background:{partie["couleur"]}">'
                      f'<h3 id="p-{e(partie["label"]).replace(" ", "")}">{e(partie["titre"])}</h3>'
                      f'<span>{e(partie["label"])}</span></div><ol>{"".join(lignes)}</ol></section>')

    lexer = DEPOT / "src" / "lexer.c"
    lignes_lexer = len(lexer.read_text("utf-8", errors="replace").splitlines()) if lexer.exists() else 0
    nb_tests = compter_tests()
    deco_lexer = f'<div class="deco d1">lexer.c, {lignes_lexer} lignes</div>' if lignes_lexer else ""
    deco_tests = ('<div class="deco d3"><svg width="34" height="34" viewBox="0 0 34 34" aria-hidden="true">'
                  '<path d="M3 3l26 10-11 3-4 12z" fill="#E5245F" stroke="#111111" stroke-width="1.5"/></svg>'
                  f'<span>{nb_tests} tests</span></div>') if nb_tests else ""
    droite = (f'<div class="barre-droite"><span class="pastille">{len(ordre)} / {total} écrits</span>'
              f'<a class="bouton-ligne" href="{e(premier)}">Lire le livre</a></div>')
    cible = '<svg width="34" height="34" viewBox="0 0 34 34" aria-hidden="true" class="pic"><circle cx="17" cy="17" r="15" fill="none" stroke="#2DBE7E" stroke-width="3.5"/><circle cx="17" cy="17" r="8" fill="none" stroke="#2DBE7E" stroke-width="3.5"/><circle cx="17" cy="17" r="2.5" fill="#2DBE7E"/></svg>'
    carres = '<svg width="30" height="30" viewBox="0 0 30 30" aria-hidden="true" class="pic"><rect x="2" y="2" width="11" height="11" rx="2" fill="#E5245F"/><rect x="17" y="2" width="11" height="11" rx="2" fill="#E5245F"/><rect x="2" y="17" width="11" height="11" rx="2" fill="#E5245F"/><rect x="17" y="17" width="11" height="11" rx="5.5" fill="#E5245F"/></svg>'
    return (tete("Écrire CeFlo", "Un langage de script en C, du lexer à la machine virtuelle.")
            + barre("accueil", droite, premier)
            + f"""<main id="contenu">
<section class="grille heros">
<div class="heros-int">
<span class="main sur-titre">on va écrire</span>
{cadre('<h1>ceflo</h1>', "cadre-heros")}
<p class="sous-titre"><span class="point" aria-hidden="true"></span>Un langage de script en C, du lexer à la machine virtuelle</p>
<p class="promesse">On écrit un vrai langage {cible} ligne par ligne, et chaque chapitre se termine sur du code qui tourne {carres}.</p>
<a class="bouton" href="{e(premier)}"><span class="carre">{FLECHES}</span>Commencer la lecture</a>
{deco_lexer}
<div class="deco d2">puis un compilateur en bytecode</div>
{deco_tests}
<div class="deco d4"><span>Écrit en C</span><svg width="32" height="32" viewBox="0 0 34 34" aria-hidden="true"><path d="M3 3l26 10-11 3-4 12z" fill="#F2B33D" stroke="#111111" stroke-width="1.5"/></svg></div>
<span class="deco d5 main">sans aucune bibliothèque</span>
</div>
</section>
<section id="sommaire" class="grille sommaire" aria-labelledby="titre-sommaire">
<div class="sommaire-int">
{cadre('<h2 id="titre-sommaire">le sommaire</h2>', "cadre-fin")}
<div class="cartes">{"".join(cartes)}</div>
</div>
</section>
</main>
</body>
</html>
""")


# ---------------------------------------------------------------------------
# La feuille de style
# ---------------------------------------------------------------------------

CSS = r"""/* Écrire CeFlo : la feuille de style du site (générée par construire.py). */
:root{
  --encre:#111111; --texte:#2B2B2B; --trait:#E4E4E4; --gris:#6B6B6B; --gris-num:#6E6E6E;
  --cyan:#34C1EE; --menthe:#A6E3C6; --jaune:#F7DFA2; --bleu:#BDE8F8; --rose:#F4A3BD;
  --mono:'DM Mono',ui-monospace,monospace; --code:'JetBrains Mono',ui-monospace,monospace;
  --main:'Just Me Again Down Here',cursive; --titre:'Unbounded',sans-serif;
}
*,*::before,*::after{box-sizing:border-box}
html{-webkit-text-size-adjust:100%;scroll-padding-top:24px}
body{margin:0;background:#FFFFFF;color:var(--encre);font-family:'Geist',system-ui,sans-serif;overflow-x:hidden}
a{color:var(--encre);text-decoration-thickness:1px;text-underline-offset:3px}
a:hover{color:#444444}
a:focus-visible,[tabindex]:focus-visible{outline:3px solid var(--cyan);outline-offset:2px}
code{font-family:var(--code);font-variant-ligatures:none;font-size:.84em;background:#F1F1F1;padding:.12em .38em;overflow-wrap:anywhere}
.evitement{position:absolute;left:-9999px;top:0;background:var(--encre);color:#FFF;padding:10px 16px;z-index:10}
.evitement:focus{left:8px;top:8px}
.main{font-family:var(--main);font-weight:400}

.grille{background-image:linear-gradient(#F3F3F3 1px,transparent 1px),linear-gradient(90deg,#F3F3F3 1px,transparent 1px);background-size:100px 100px}
.regle{height:34px;border-bottom:1px solid var(--trait);display:flex;overflow:hidden;font-family:var(--mono);font-size:11px;color:#9A9A9A;
  background-image:linear-gradient(90deg,#C8C8C8 1px,transparent 1px),linear-gradient(90deg,#E2E2E2 1px,transparent 1px);
  background-size:100px 12px,20px 6px;background-repeat:repeat-x}
.regle span{width:100px;flex:none;padding-top:16px;transform:translateX(-8px)}
.regle span:first-child{transform:none;padding-left:2px}

/* La barre de navigation */
.barre{border-bottom:1px solid var(--trait);background:#FFFFFF}
.barre-int{padding:0 16px;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:0 12px;font-family:var(--mono);font-size:14px;text-transform:uppercase}
.nav-livre{display:flex;flex-wrap:wrap;align-items:stretch}
.nav-livre a{display:flex;align-items:center;gap:8px;padding:14px 16px;text-decoration:none}
.nav-livre a.actif{background:var(--cyan)}
.nav-livre a:hover{background:#F1F1F1;color:var(--encre)}
.nav-livre a.actif:hover{background:var(--cyan)}
.compteur{padding:8px 0;color:var(--gris)}
.barre-droite{display:flex;align-items:center;gap:10px;padding:8px 0}
.pastille{background:#F1F1F1;border-radius:999px;padding:7px 12px}
.bouton-ligne{display:flex;align-items:center;gap:8px;border:1px solid var(--encre);padding:8px 14px;text-decoration:none}
.bouton-ligne:hover{background:var(--encre);color:#FFF}

/* Le cadre de sélection et ses poignées */
.cadre{position:relative;border:2px solid var(--cyan)}
.cadre .p{position:absolute;width:12px;height:12px;background:var(--cyan);border:1px solid var(--encre)}
.cadre .hg{left:-7px;top:-7px}.cadre .hd{right:-7px;top:-7px}.cadre .bg{left:-7px;bottom:-7px}.cadre .bd{right:-7px;bottom:-7px}
.cadre-fin{border:1.5px solid var(--encre);padding:6px 14px}
.cadre-fin .p{width:8px;height:8px;background:#FFF;border:1.5px solid var(--encre)}
.cadre-fin .hg{left:-5px;top:-5px}.cadre-fin .hd{right:-5px;top:-5px}.cadre-fin .bg{left:-5px;bottom:-5px}.cadre-fin .bd{right:-5px;bottom:-5px}
.cadre-fin h2{margin:0;font-weight:500;font-size:40px;letter-spacing:-.03em}

.etiquette{align-self:flex-start;font-family:var(--mono);font-size:12px;text-transform:uppercase;padding:3px 8px}
.etiquette.grande{font-size:14px;padding:5px 10px}
.bientot{display:inline-block;font-family:var(--mono);font-size:11px;text-transform:uppercase;background:#F1F1F1;color:#595959;padding:1px 6px;margin-left:4px;transform:rotate(-2deg);vertical-align:1px}

/* La page d'un chapitre */
.page{max-width:1360px;margin:0 auto;padding:64px 32px 110px;display:flex;flex-wrap:wrap;gap:56px;align-items:flex-start}
.colonne{flex:1 1 220px;max-width:250px;display:flex;flex-direction:column;gap:26px;background:#FFF;border:1px solid var(--trait);padding:20px;position:sticky;top:16px;max-height:calc(100vh - 32px);overflow-y:auto}
.colonne .partie{display:flex;flex-direction:column;gap:9px}
.colonne ol{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:9px}
.colonne li > a,.colonne li.absent{display:flex;gap:10px;text-decoration:none;font-size:15px;line-height:1.4;color:#444444}
.colonne li.absent{color:#6E6E6E}
.colonne li > a:hover{color:var(--encre);text-decoration:underline}
.colonne .n{width:22px;flex:none;font-family:var(--mono);font-size:12.5px;padding-top:2px;color:var(--gris-num)}
.colonne li.courant > a{color:var(--encre);font-weight:600}
.colonne ol.sous{gap:6px;margin:8px 0 4px 32px;padding-left:10px;border-left:2px solid var(--cyan)}
.colonne ol.sous a{display:flex;gap:8px;text-decoration:none;font-size:14px;line-height:1.35;color:#444444}
.colonne ol.sous a:hover{color:var(--encre);text-decoration:underline}
.colonne ol.sous .n{width:auto;min-width:28px;font-size:11.5px}

.article{flex:999 1 560px;min-width:0;display:flex;flex-direction:column;gap:28px;container-type:inline-size;container-name:article}
.entete{display:flex;flex-direction:column;align-items:flex-start;gap:18px;margin-bottom:6px}
.ligne-etiquette{display:flex;flex-wrap:wrap;align-items:center;gap:6px 14px}
.note-titre{font-size:30px;line-height:1.1;transform:rotate(-3deg)}
.cadre-chapitre{padding:10px 24px;margin-left:6px;max-width:calc(100% - 12px)}
.cadre-chapitre h1{margin:0;font-family:var(--titre);font-weight:800;font-size:clamp(38px,9vw,76px);line-height:1.05;letter-spacing:-.04em;overflow-wrap:anywhere}

/* Le corps : une grille, le texte à gauche et les notes dans la marge */
.corps{display:grid;grid-template-columns:minmax(0,1fr);row-gap:22px;font-size:19px;line-height:1.7;color:var(--texte)}
.corps > *{min-width:0}
.b-texte{max-width:700px}
.b-large{max-width:760px;margin:6px 0}
.corps p{margin:0}
.corps p + p,.corps p + ul,.corps p + ol,.corps ul + p,.corps ol + p{margin-top:22px}
.corps .chapeau{font-size:25px;line-height:1.45;font-weight:500;letter-spacing:-.015em;color:var(--encre)}
.corps h2{margin:28px 0 0;font-weight:600;font-size:34px;line-height:1.2;letter-spacing:-.03em;color:var(--encre)}
.corps h2 .num{font-family:var(--mono);font-weight:400;font-size:.5em;letter-spacing:0;vertical-align:.35em;margin-right:6px;padding:2px 7px;background:var(--bleu);text-decoration:none}
.corps h2 .num:hover{background:var(--cyan)}
.corps h3{margin:10px 0 0;font-weight:600;font-size:23px;letter-spacing:-.02em;color:var(--encre)}
.corps ul,.corps ol{margin:0;padding-left:1.3em}
.corps li{margin:4px 0}
.corps li > p + ul,.corps li > p + ol{margin-top:4px}
.corps li::marker{font-family:var(--mono);color:#555}
.corps ul > li::marker{content:"■  ";font-size:.6em;color:var(--cyan)}
.corps strong{font-weight:600;color:var(--encre)}
.corps blockquote{margin:0;padding:4px 0 4px 20px;border-left:3px solid var(--trait)}

/* Les post-it de la marge */
.marge{display:flex;flex-direction:column;gap:18px;max-width:340px}
.postit{background:var(--jaune);padding:16px 18px;transform:rotate(var(--a,1.5deg));font-size:15px;line-height:1.5;color:#222;box-shadow:0 1px 0 rgba(0,0,0,.06)}
.postit p + p{margin-top:10px}
.postit code,.encadre code{background:rgba(255,255,255,.55)}

/* À toi, Attention */
.encadre{border:2px solid var(--encre);padding:22px 24px 22px;margin:8px 0;position:relative;color:var(--encre)}
.encadre.essaie{background:var(--menthe)}
.encadre.attention{background:var(--rose)}
.encadre p + p{margin-top:14px}
.etiquette-noire{display:inline-flex;align-items:center;gap:10px;background:var(--encre);color:#FFF;font-family:var(--mono);font-size:14px;text-transform:uppercase;padding:4px 14px 4px 4px;margin:-38px 0 12px -8px;transform:rotate(-2deg)}
.etiquette-noire .carre{width:28px;height:28px;background:var(--cyan);display:flex;align-items:center;justify-content:center}
.etiquette-noire .carre.rose{background:var(--rose)}
.encadre > .etiquette-noire{display:flex;width:max-content}

/* Les figures */
.fig-jetons,.fig-etapes{margin:10px 0;display:flex;flex-direction:column;gap:16px}
.jetons,.etapes{list-style:none;margin:0;padding:0;display:flex;flex-wrap:wrap;gap:14px;align-items:flex-end}
.jetons li{display:flex;flex-direction:column;gap:10px;align-items:flex-start;margin:0}
.jeton{font-family:var(--code);font-variant-ligatures:none;font-size:26px;line-height:1.3;border:2px solid var(--encre);padding:10px 20px;white-space:pre;color:var(--encre)}
.sorte{font-family:var(--mono);font-size:12.5px;color:#555}
.etapes{align-items:center;gap:10px}
.etapes li{display:flex;align-items:center;gap:10px;margin:0}
.etapes li:not(:last-child)::after{content:"";width:30px;height:14px;flex:none;background:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 14'%3E%3Cpath d='M0 7h27M21 1l6 6-6 6' stroke='%23111' stroke-width='2' fill='none'/%3E%3C/svg%3E") no-repeat center}
.etape{font-family:var(--mono);font-size:16px;text-transform:uppercase;border:2px solid var(--encre);padding:9px 16px;color:var(--encre);line-height:1.3}
.etape code{background:transparent;padding:0;font-size:1em;text-transform:none}
figcaption.main{font-size:24px;line-height:1.2;color:#444}

/* L'éditeur façon VS Code, thème Dark+ */
.editeur,.terminal{margin:0;display:flex;flex-direction:column;background:#1E1E1E;border:1px solid var(--encre);overflow:hidden}
.onglets{display:flex;align-items:stretch;background:#252526;font-family:var(--code);font-size:13px}
.onglet{padding:10px 16px;background:#1E1E1E;color:#FFF;border-top:2px solid var(--cyan)}
.ariane{padding:6px 18px 2px;font-family:var(--code);font-size:12px;color:#A9A9A9;white-space:pre;overflow:hidden;text-overflow:ellipsis}
.defile{margin:0;overflow-x:auto;font-variant-ligatures:none;font-feature-settings:"liga" 0,"calt" 0;font-family:var(--code);font-size:15px;line-height:1.65;color:#D4D4D4;tab-size:4;-webkit-overflow-scrolling:touch}
.defile code{display:block;min-width:max-content;background:transparent;padding:0;font-size:1em;overflow-wrap:normal;white-space:pre}
.editeur .defile{padding:8px 0 16px}
.editeur .l{display:inline-block;min-width:100%;padding-right:22px}
.editeur .l::before{content:attr(data-n);display:inline-block;width:58px;padding-right:22px;text-align:right;color:#858585;user-select:none;-webkit-user-select:none}
.editeur .l.f{background:#2A2D2E}
.editeur .l.f::before{color:#C6C6C6}
.defile::-webkit-scrollbar{height:10px}
.defile::-webkit-scrollbar-thumb{background:#424242}
.defile::-webkit-scrollbar-track{background:#1E1E1E}

.onglets-term{display:flex;gap:22px;padding:10px 18px 6px;font-family:var(--code);font-size:11.5px;letter-spacing:.04em;color:#969696}
.onglets-term .actif{color:#E7E7E7;border-bottom:1px solid #E7E7E7;padding-bottom:3px}
.terminal .defile{padding:10px 22px 18px;color:#CCCCCC}
.terminal .l{display:inline-block;min-width:100%}
.terminal .invite{color:var(--cyan);user-select:none;-webkit-user-select:none}
.terminal .cmd{color:#E7E7E7}
.terminal .err{color:#F14C4C}
.terminal .gris{color:#8A8A8A}

/* Précédent, suivant */
.pied{max-width:760px;display:flex;flex-wrap:wrap;gap:16px;padding-top:44px;margin-top:20px;border-top:1px solid var(--trait)}
.pied a,.pied .attente{flex:1 1 260px;text-decoration:none}
.prec{display:flex;flex-direction:column;gap:6px;background:#FFF;border:1px solid var(--encre);padding:16px 20px}
.prec:hover{background:#F6F6F6}
.suiv{display:flex;align-items:center;justify-content:space-between;gap:14px;background:var(--encre);color:#FFF;padding:10px 20px 10px 10px}
.suiv:hover{color:#FFF;background:#2A2A2A}
.suiv .carre,.bouton .carre{width:44px;height:44px;flex:none;background:var(--cyan);display:flex;align-items:center;justify-content:center}
.suiv .txt{display:flex;flex-direction:column;gap:4px;align-items:flex-end;text-align:right}
.petit{font-family:var(--mono);font-size:12.5px;text-transform:uppercase;color:var(--gris)}
.suiv .petit{color:#BDBDBD}
.grand{font-size:20px;font-weight:500}
.suiv.attente{background:#FFF;color:#595959;border:1px dashed #9A9A9A;justify-content:flex-end;padding:16px 20px}
.suiv.attente .petit{color:#595959}

/* L'accueil */
.heros{position:relative;overflow:hidden;padding:120px 24px 110px}
.heros-int{position:relative;max-width:1100px;margin:0 auto;display:flex;flex-direction:column;align-items:center;gap:34px}
.sur-titre{font-size:40px;line-height:1}
.cadre-heros{padding:18px 36px}
.cadre-heros h1{margin:0;font-family:var(--titre);font-weight:800;font-size:clamp(64px,17vw,150px);line-height:1;letter-spacing:-.04em}
.sous-titre{margin:0;display:flex;align-items:center;gap:12px;font-family:var(--mono);font-size:15px;text-transform:uppercase;letter-spacing:.02em;text-align:center}
.sous-titre .point{width:13px;height:13px;border-radius:999px;background:var(--cyan);flex:none}
.promesse{margin:70px 0 0;font-size:clamp(28px,5vw,46px);font-weight:500;line-height:1.14;letter-spacing:-.035em;text-align:center;max-width:820px}
.promesse .pic{vertical-align:-3px;width:.74em;height:.74em}
.bouton{display:flex;align-items:center;gap:12px;background:var(--encre);color:#FFF;text-decoration:none;padding:8px 18px 8px 8px;font-family:var(--mono);font-size:15px;text-transform:uppercase}
.bouton:hover{color:#FFF;background:#2A2A2A}
.bouton .carre{width:40px;height:40px}
.deco{position:absolute}
.d1{left:30px;top:70px;transform:rotate(-10deg);background:var(--menthe);padding:12px 20px;font-size:19px;font-weight:500}
.d2{right:20px;top:92px;transform:rotate(-4deg);background:var(--jaune);padding:12px 20px;font-size:19px;font-weight:500}
.d3{right:10px;top:600px;display:flex;flex-direction:column;align-items:flex-start}
.d3 svg{transform:translate(-14px,4px)}
.d3 span{transform:rotate(12deg);background:#E5245F;color:#FFF;border:2px solid var(--encre);border-radius:999px;padding:10px 26px;font-family:var(--mono);font-size:16px;text-transform:uppercase}
.d4{left:0;top:690px;display:flex;align-items:flex-start;gap:6px}
.d4 span{transform:rotate(-3deg);background:#F2B33D;border:2px solid var(--encre);border-radius:999px;padding:12px 26px;font-family:var(--mono);font-size:16px;text-transform:uppercase}
.d4 svg{transform:rotate(80deg) translate(-20px,0)}
.d5{left:20px;top:600px;font-size:30px;transform:rotate(-6deg)}

.sommaire{border-top:1px solid var(--trait);padding:90px 24px 120px}
.sommaire-int{max-width:1100px;margin:0 auto;display:flex;flex-direction:column;align-items:center;gap:60px}
.cartes{width:100%;display:flex;flex-wrap:wrap;gap:28px;align-items:flex-start}
.carte{flex:1 1 320px;min-width:0;display:flex;flex-direction:column;background:#FFF;border:1px solid var(--trait)}
.carte-tete{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:14px 18px;border-bottom:1px solid var(--trait)}
.carte-tete h3{margin:0;font-size:24px;font-weight:500;letter-spacing:-.02em}
.carte-tete span{font-family:var(--mono);font-size:13px;text-transform:uppercase;white-space:nowrap}
.carte ol{list-style:none;margin:0;padding:0}
.carte li > a,.carte li.absent > div{display:flex;gap:14px;text-decoration:none;padding:14px 18px;border-top:1px solid #F0F0F0}
.carte li:first-child > a,.carte li:first-child > div{border-top:0}
.carte li > a:hover{background:#F7F7F7}
.carte li > a:hover .titre{text-decoration:underline}
.carte .n{width:26px;flex:none;font-family:var(--mono);font-size:14px;padding-top:3px;color:var(--gris-num)}
.carte .txt{display:flex;flex-direction:column;gap:4px;min-width:0;flex:1}
.carte .titre{font-size:19px;font-weight:500}
.carte .resume{font-size:15.5px;line-height:1.5;color:#555}
.carte li.absent .titre{color:#595959}
.carte li.absent .bientot{align-self:flex-start;flex:none}

/* Les dessins en texte */
.dessin{margin:0;background:#FAFAFA;border:1px solid var(--trait);padding:16px 20px;font-family:var(--code);font-variant-ligatures:none;font-size:14.5px;line-height:1.5;color:#222;overflow-x:auto;white-space:pre;tab-size:4}

/* Les sections de fin : Défis et Note de conception */
.corps h2.h2-fin{display:flex;align-items:center;gap:14px;margin-top:44px;padding-top:30px;border-top:1px solid var(--trait)}
.pic-fin{width:40px;height:40px;flex:none;background:var(--encre);display:flex;align-items:center;justify-content:center}
.pic-fin svg{width:18px;height:18px}
.pic-fin svg path{fill:var(--cyan)}
.defis{list-style:none;margin:8px 0 0;padding:0;display:flex;flex-direction:column;gap:30px}
.corps .defis > li.defi{margin:0;display:flex;flex-direction:column;align-items:flex-start;gap:10px}
.etiquette-defi{font-family:var(--mono);font-size:14px;text-transform:uppercase;border:2px solid var(--encre);padding:5px 12px;color:var(--encre);line-height:1.2}
.defi-corps{border-left:2px solid var(--encre);padding:2px 0 2px 20px;margin-left:10px;width:calc(100% - 10px)}
.defi-corps h3{margin:0 0 6px;font-size:21px}
.defi-corps > * + *{margin-top:14px}
.conception{position:relative;background:#FFF;border:1px solid var(--encre);box-shadow:6px 6px 0 var(--bleu);padding:26px 26px 24px;margin-top:6px}
.conception-note{position:absolute;right:18px;top:-24px;font-size:26px;line-height:1;background:#FFF;padding:0 8px;transform:rotate(-2deg)}
.conception > * + *{margin-top:16px}
.postit.en-ligne{max-width:340px;margin:18px 0 0 auto}

/* Deux colonnes dans le corps quand l'article est assez large */
@container article (min-width: 940px){
  .corps{grid-template-columns:minmax(0,700px) minmax(180px,240px);column-gap:48px}
  .b-texte{grid-column:1;grid-row:var(--r)}
  .b-large{grid-column:1 / -1;grid-row:var(--r)}
  .marge{grid-column:2;grid-row:var(--r) / span var(--s,1);align-self:start}
}

/* Tablette et téléphone */
@media (max-width: 899px){
  .page{padding:40px 16px 80px;gap:40px}
  .colonne{order:2;max-width:none;flex-basis:100%;position:static;max-height:none}
  .article{order:1}
}
@media (max-width: 1180px){.deco{display:none}}
@media (max-width: 600px){
  .barre-int{padding:0 4px;font-size:12.5px}
  .nav-livre a{padding:12px 7px;gap:5px}
  .compteur{padding:0 10px 10px}
  .barre-droite{padding:0 10px 10px}
  .corps{font-size:17px;line-height:1.65}
  .corps .chapeau{font-size:21px}
  .corps h2{font-size:27px}
  .note-titre{font-size:26px}
  .cadre-chapitre{padding:8px 14px}
  .defile{font-size:13px}
  .editeur .l::before{width:44px;padding-right:14px}
  .terminal .defile{padding:10px 14px 16px}
  .jeton{font-size:20px;padding:8px 14px}
  .encadre{padding:20px 16px 18px}
  .conception{padding:30px 16px 18px;box-shadow:4px 4px 0 var(--bleu)}
  .conception-note{font-size:22px;right:8px}
  .defi-corps{padding-left:14px}
  .dessin{font-size:12.5px;padding:12px 14px}
  .marge{max-width:none}
  .postit{transform:rotate(calc(var(--a,1.5deg) / 2))}
  .heros{padding:72px 16px 80px}
  .sur-titre{font-size:32px}
  .cadre-heros{padding:12px 20px}
  .promesse{margin-top:30px}
  .sommaire{padding:60px 16px 80px}
  .cadre-fin h2{font-size:30px}
  .cartes{gap:20px}
  .carte{flex-basis:100%}
}
@media (prefers-reduced-motion: no-preference){html{scroll-behavior:smooth}}
@media print{.barre,.regle,.colonne,.pied{display:none}.grille{background:none}.page{display:block;padding:0}}
"""


# ---------------------------------------------------------------------------

def construire(dossier_chapitres, sortie, plan_md):
    plan = lire_plan(plan_md)
    total = sum(len(p["chapitres"]) for p in plan) or 0

    chapitres = {}
    for chemin in sorted(dossier_chapitres.glob("*.md")):
        if not re.match(r"^\d", chemin.name):
            continue
        ch = lire_chapitre(chemin)
        if ch["numero"] in chapitres:
            avertir(chemin.name, f"numéro {ch['numero']} déjà pris par {chapitres[ch['numero']]['source']}, ignoré")
            continue
        chapitres[ch["numero"]] = ch
    total = max([total] + list(chapitres))
    ordre = [chapitres[k] for k in sorted(chapitres)]
    parties = sommaire_complet(plan, chapitres)

    sortie.mkdir(parents=True, exist_ok=True)
    # Les pages d'anciens chapitres disparus
    attendues = {ch["page"] for ch in ordre}
    for ancien in sortie.glob("[0-9]*.html"):
        if ancien.name not in attendues:
            ancien.unlink()
    (sortie / "style.css").write_text(CSS, "utf-8")
    (sortie / "index.html").write_text(page_accueil(parties, ordre, total), "utf-8")
    for ch in ordre:
        (sortie / ch["page"]).write_text(page_chapitre(ch, parties, ordre, total), "utf-8")
    return ordre


def main():
    lecteur = argparse.ArgumentParser(description="Construit le site du livre Écrire CeFlo.")
    lecteur.add_argument("--chapitres", type=Path, default=ICI / "chapitres",
                         help="le dossier des chapitres .md (livre/chapitres)")
    lecteur.add_argument("--sortie", type=Path, default=ICI / "site",
                         help="le dossier du site (livre/site)")
    lecteur.add_argument("--plan", type=Path, default=ICI / "PLAN.md")
    args = lecteur.parse_args()

    if not args.chapitres.is_dir():
        print(f"pas de dossier {args.chapitres} : le site n'aura que l'accueil", file=sys.stderr)
    ordre = construire(args.chapitres, args.sortie, args.plan)
    for a in AVERTISSEMENTS:
        print("attention:", a, file=sys.stderr)
    print(f"{len(ordre)} chapitre(s) -> {args.sortie / 'index.html'}")


if __name__ == "__main__":
    main()
