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

// match : le premier cas qui correspond, sans `break` à écrire
match teinte {
    case Couleur.ROUGE { println("chaud") }
    case Couleur.VERT, Couleur.BLEU { println("froid") }
}

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
[Lexer] ==> [Parser] ==> [AST] ==> [Compilateur] ==> [Bytecode] ==> [VM]
```

Le programme est d'abord traduit en bytecode, une suite d'instructions simples, puis une machine virtuelle à pile les exécute. `./Ceflo --dis fichier.cf` affiche ce bytecode. [docs/VM.md](docs/VM.md) explique les choix qui la rendent rapide.

| Fichier | Rôle |
| --- | --- |
| [src/token.c](src/token.c) | la liste des jetons |
| [src/lexer.c](src/lexer.c) | découpe le texte en jetons |
| [src/parser.c](src/parser.c) | construit l'arbre : une fonction par règle de la grammaire |
| [src/ast.c](src/ast.c) | les noeuds de l'arbre |
| [src/vm/chunk.h](src/vm/chunk.h) | **la liste des instructions**, et ce que chacune fait à la pile |
| [src/vm/compiler.c](src/vm/compiler.c) | traduit l'arbre en bytecode : une fonction par type de noeud |
| [src/vm/vm.c](src/vm/vm.c) | exécute le bytecode : un `case` par instruction |
| [src/vm/value.h](src/vm/value.h), [src/vm/object.h](src/vm/object.h) | les valeurs et les objets : chaînes, listes, maps, structures... |
| [src/vm/memory.c](src/vm/memory.c) | les allocations et le garbage collector |
| [src/vm/binary.c](src/vm/binary.c), [src/vm/print.c](src/vm/print.c) | les opérateurs `+ - * / == < ...` et le texte d'une valeur |
| [src/vm/module.c](src/vm/module.c) | `import` et `export` |
| [src/vm/debug.c](src/vm/debug.c) | le désassembleur de `--dis` |
| [src/vm/natives](src/vm/natives) | ce qui est écrit en C : `println`, les méthodes, `@type()`, `os`, `console`, les bibliothèques .so |
| [src/error.c](src/error.c) | l'affichage des erreurs |
| [src/str.c](src/str.c), [src/list.c](src/list.c), [src/common.c](src/common.c) | les outils : chaînes, listes, tables, mémoire |

Pour le lire, l'ordre le plus simple est : `token`, `lexer`, `parser` avec `GRAMMAIRE.md` sous les yeux, puis `chunk.h`, `compiler.c` et `vm.c` avec `./Ceflo --dis` sur un petit programme.

L'ancien interpréteur, qui parcourait l'arbre, est encore compilé sous le nom `Ceflo_ref` : `make diff` vérifie que les deux donnent la même chose.

## Écrire une bibliothèque en C

Une fonction native reçoit des valeurs déjà calculées et retourne une valeur. Elle n'a pas à s'occuper de la mémoire.

```c
#include "ceflo.h"

Value mathCarre(VM *vm, int argc, Value *args){
    native_check_args(vm,"mathCarre",argc,1);
    native_check_type(vm,"mathCarre",args[0],"number");
    return NUMBER_VAL(AS_NUMBER(args[0]) * AS_NUMBER(args[0]));
}
```

```bash
gcc -fPIC -shared -Isrc/vm -Isrc/include -o libcarre.so carre.c
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

Les tests sont dans [tests](tests) : un fichier `x.cf`, et sa sortie attendue dans `x.out`.

- `make diff` compare chaque test et chaque exemple entre la VM et l'ancien interpréteur.
- `make bench` mesure la vitesse de la VM, de l'ancien interpréteur et de Python ([bench](bench)).
- `make sanitize` lance les tests sous AddressSanitizer, puis avec un garbage collector qui se déclenche à chaque point sûr.
- `make val` lance `test.cf` sous valgrind pour chercher les fuites de mémoire ; `make val VAL_FILE=exemple/4_listes.cf` lance un autre script. Il faut avoir installé valgrind.

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
