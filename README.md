# CeFlo

Un petit langage de script, interprété, écrit en C sans aucune dépendance. Sa syntaxe tient en quelques lignes et son interpréteur se lit de bout en bout.

```go
struct Personne {
    var Nom
    var Age::int

    func __init__(nom, age) {
        Nom = nom
        Age = age
    }

    func __str__() {
        return Nom + " a " + Age + " ans"
    }
}

var liste = [new Personne("Yann", 22), new Personne("Grace", 18)]

for var i = 0; i < liste.lenght(); i += 1 {
    println(liste[i])
}
```

## Installer

Il faut `gcc` et `make`, sous Linux :

```bash
make
```

```bash
./Ceflo exemple/1_hello.cf
```

Un script peut avoir une fonction `main`. Elle se lance toute seule après le haut du fichier, reçoit ses arguments dans `argv`, et sa valeur devient le code de sortie du programme :

```go
func main(argv) {
    if argv.lenght() < 2 {
        println("utilisation : ", argv[0], " nom")
        return 1
    }
    println("Bonjour ", argv[1])
    return 0
}
```

```bash
./Ceflo mon_script.cf Yann
```

`main` est facultatif : un script sans `main` s'exécute de haut en bas, et `argv` y est aussi disponible.

Il peut aussi s'exécuter directement, si `Ceflo` est dans le `PATH` et que sa première ligne est `#!/usr/bin/env Ceflo`.

## Le langage en bref

```go
// Variables : le type est facultatif, mais une fois donné il ne change plus
var nom = "Ceflo"
var age::int = 21
const PI = 3.14

// Conditions et boucles
if age < 18 {
    println("mineur")
} else if age == 18 {
    println("tout juste")
} else {
    println("majeur")
}

var i = 0
while i < 3 { i += 1 }

for var j = 0; j < 3; j += 1 {
    if j == 1 { continue }
    println(j)
}

// Fonctions
func Double(x) { return x * 2 }
func Somme -> x, y : int { return x + y }
func Bonjour { println("Bonjour") }

// Une fonction est une valeur, et elle se souvient de ses variables
func Compteur() {
    var n = 0
    return lfunc() { n += 1; return n }
}

// Listes et chaînes
var l = [1, 2, "trois"]
l.append(4)
println(l[0], " ", l.lenght())
println(format("${nom} a ${age} ans"))
println(age > 18 ? "majeur" : "mineur")

// Tables clé -> valeur, comme en TypeScript
var profil = { nom: "Yann", age: 22 }
profil.ville = "Abidjan"
println(profil["nom"], " ", profil.has("age"))

for var cle, valeur in profil {
    println(cle, " : ", valeur)
}

// Énumérations : un vrai type, pas des nombres déguisés
enum Couleur { ROUGE, VERT, BLEU }
var teinte::Couleur = Couleur.VERT
println(teinte.name, " ", teinte.value)

// Les fichiers et le système
from os import [lire_lignes, dossier, commande]
println(dossier("."))
println(commande("date").sortie)

// Pour changer de type, on le demande
var saisie = @int("42")
```

Le dossier [exemple](exemple) montre tout cela pas à pas. [GRAMMAIRE.md](GRAMMAIRE.md) décrit le langage complet : sa grammaire, ses types et ses règles.

## Comment ça marche

```
[Lexer] ==> [Parser] ==> [AST] ==> [Visitor]
```

| Fichier | Rôle |
| --- | --- |
| [src/token.c](src/token.c) | la liste des jetons |
| [src/lexer.c](src/lexer.c) | découpe le texte en jetons |
| [src/parser.c](src/parser.c) | construit l'arbre : une fonction par règle de la grammaire |
| [src/ast.c](src/ast.c) | les noeuds de l'arbre, qui ne changent jamais à l'exécution |
| [src/visitor.c](src/visitor.c) | exécute l'arbre : une fonction par type de noeud |
| [src/Value.c](src/Value.c) | les valeurs, leur affichage, les méthodes de `list` et `str` |
| [src/value_expr.c](src/value_expr.c) | les opérateurs `+ - * / == < ...` |
| [src/typer.c](src/typer.c) | la vérification de `::type` et les conversions `@type()` |
| [src/scope.c](src/scope.c) | les portées : les noms visibles à un endroit du code |
| [src/gc.c](src/gc.c) | le garbage collector |
| [src/map.c](src/map.c) | la table de hachage du type `map` |
| [src/bulletin.c](src/bulletin.c) | les fonctions fournies : `println`, `input`, `format`... |
| [src/os.c](src/os.c) | le module `os` : fichiers, dossiers, système |
| [src/error.c](src/error.c) | l'affichage des erreurs |
| [src/str.c](src/str.c), [src/list.c](src/list.c), [src/common.c](src/common.c) | les outils : chaînes, listes, tables, mémoire |

Pour le lire ou le réécrire dans un autre langage, l'ordre le plus simple est : `token`, `lexer`, `parser` avec `GRAMMAIRE.md` sous les yeux, `Value`, `scope`, puis `visitor`.

## Écrire une bibliothèque en C

Une fonction native reçoit des valeurs déjà calculées et retourne une valeur. Elle n'a pas à s'occuper de la mémoire.

```c
#include "visitor.h"

value_t *mathCarre(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"mathCarre",argc,1);
    Value_check_type(node,"mathCarre",args[0],NUMBER);
    return makeNumber(AS_NUMBER(args[0]) * AS_NUMBER(args[0]));
}
```

```bash
gcc -fPIC -shared -Isrc/include -o libcarre.so carre.c
```

```go
from @"./libcarre.so" import [mathCarre]
println(mathCarre(4))
```

[lib/math](lib/math) et [lib/file](lib/file) sont deux exemples complets, compilés par `make libs`. Ces bibliothèques ne se chargent pour l'instant que sous Linux.

## Tester

```bash
make test
```

`make sanitize` lance les mêmes tests sous AddressSanitizer, puis avec un garbage collector qui se déclenche à chaque allocation. Les tests sont dans [tests](tests) : un fichier `x.cf`, et sa sortie attendue dans `x.out`.

`make val` lance `test.cf` sous valgrind, avec un interpréteur compilé sans réservoir d'objets (`CEFLO_NO_POOL`), pour chercher les fuites de mémoire, et `make val VAL_FILE=exemple/4_listes.cf` lance un autre script. Il faut avoir installé valgrind.

## Éditeur

[editor/vscode](editor/vscode) apporte à Visual Studio Code la coloration et l'autocomplétion du code CeFlo : attributs et méthodes après un `.`, chemins et noms après `from ... import`, types après `::`, clés d'un map. Il souligne aussi ce qui échouera sûrement à l'exécution, comme `p.x = "texte"` sur un `x::int`.

```bash
make editor
```

`make editor-test` teste l'autocomplétion, seule puis dans un vrai Visual Studio Code.

## Contact

GitHub : [YxnnXriel](https://github.com/YxnnXriel). Pour une question ou pour contribuer : yannariel13@gmail.com

## Licence

MIT, voir [LICENCE](LICENCE).
