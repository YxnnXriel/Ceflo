# CeFlo en bytecode

L'ancien interpréteur parcourt l'arbre du programme (l'AST) et exécute chaque
noeud. C'est simple, mais lent : chaque nombre est alloué, et chaque nom est
cherché dans une table de hachage à chaque lecture.

La nouvelle version compile d'abord l'AST en **bytecode**, une suite
d'instructions simples, puis une **machine virtuelle** (VM) les exécute.

```
[Lexer] ==> [Parser] ==> [AST] ==> [Compilateur] ==> [Bytecode] ==> [VM]
 inchangés                          src/vm/compiler.c                src/vm/vm.c
```

Le but : un langage qu'on peut utiliser en production, plus rapide que Python,
avec une VM qu'un étudiant peut lire en un après-midi.

## Les quatre idées qui font la vitesse

1. **Une valeur se copie comme un int.** `Value` fait 16 octets : none, bool,
   char et number ne demandent aucune allocation (`src/vm/value.h`).
2. **Les noms sont résolus à la compilation.** Une variable locale est une case
   de la pile, une globale une case du module. Aucune recherche par nom pendant
   l'exécution.
3. **Les attributs ont une place fixe.** Une structure connaît ses attributs :
   `p.x` lit la case 0 d'un tableau.
4. **Le GC ne tourne qu'aux points sûrs.** Personne ne protège ses valeurs
   temporaires, sauf une native qui rappelle du CeFlo (`src/vm/memory.h`).

## Les fichiers

| Fichier | Rôle |
|---|---|
| `src/vm/value.h` | `Value`, et les règles : vrai/faux, `==`, noms des types |
| `src/vm/object.h` | les objets du GC : str, list, map, fonctions, structures, modules |
| `src/vm/chunk.h` | **la liste des instructions** et ce que chacune fait à la pile |
| `src/vm/memory.h` | allocations et garbage collector |
| `src/vm/vm.h` | la VM, les appels depuis le C, les erreurs |
| `src/vm/compiler.h` | AST vers bytecode |
| `src/vm/natives.h` | builtins, méthodes, `@type()`, `os`, `console`, bibliothèques .so |
| `src/vm/ceflo.h` | ce qu'inclut une bibliothèque native |

L'ancien interpréteur reste compilé sous le nom `Ceflo_ref` : **il fait foi**.
Quand la VM et lui ne donnent pas la même chose, c'est la VM qui a tort, sauf
pour les différences voulues ci-dessous.

## Les différences voulues

- **Des erreurs trouvées plus tôt.** Le compilateur signale à la compilation
  ce qu'il sait déjà faux : affecter une constante, redéfinir une fonction
  ou une structure. Le message reste le même, mais il arrive avant que le
  programme ait commencé.
- **`format` lit un texte écrit dans le code.** `format("${a}")` est compilé :
  `${a}` voit les variables locales. Avec un texte calculé pendant
  l'exécution, `format` donne une erreur claire.
- **Les bibliothèques .so ont une nouvelle forme :**
  `Value f(VM *vm, int argc, Value *args)` (voir `ceflo.h`). Celles de `lib/`
  sont réécrites.
- **Un attribut garde sa place.** Affecter une méthode (`p.avance = ...`) est
  une erreur : `avance` n'est pas un attribut.
- **`x += e` lit `x` avant de calculer `e`**, comme Python, JavaScript et
  Java. Si `e` modifie `x`, l'ancien interpréteur utilisait la nouvelle
  valeur ; la VM utilise celle d'avant. Pareil pour `l[i] += e` et `o.a += e`.
- **Un type nommé est une globale.** Dans `var p::Point`, `Point` est cherché
  dans les globales du fichier.

## Les limites connues

Ce que la VM ne fait pas encore comme l'ancien interpréteur. Aucun test ni
exemple ne tombe dessus.

- **Une locale déclarée plus bas.** Dans une fonction, une lfunc ne voit pas
  une variable locale déclarée après elle :
  `var g = lfunc(){ return y }; var y = 3; g()` donne « `y` n'est pas
  définie ». Au niveau du fichier, `y` est une globale et ça marche.
- **`match` sans `else`.** L'erreur « ce match ne traite pas » se déclenche
  quand le sujet est un élément d'énumération, ou quand un `case` contient un
  `a.b`. Un `case p.x` qui ne correspond pas donne donc l'erreur, même sans
  énumération.
- **`import` de tout dans un bloc.** Les noms vont dans les globales du
  fichier, pas dans le bloc.
- **Construire un long texte avec `+=`.** Chaque `texte += ligne` recopie tout
  le texte, comme en Java ou en Lua : la boucle est quadratique. Python évite
  cette copie grâce à son comptage de références.

## Commandes

```bash
make                # Ceflo (la VM) et Ceflo_ref (l'ancien interpréteur)
make test           # tous les tests, avec la VM
make diff           # chaque test et chaque exemple : VM et Ceflo_ref donnent-ils la même chose ?
make bench          # la vitesse de la VM, de Ceflo_ref et de Python
make sanitize       # ASan, puis un GC qui tourne à chaque point sûr
./Ceflo --dis f.cf  # affiche le bytecode de f.cf avant de l'exécuter
```

Pour compiler à plusieurs dans le même dossier sans se gêner :
`make BUILD=build/moi`.

## La réécriture à plusieurs

Chaque partie a un responsable, et **personne ne modifie le fichier d'un
autre**. Pour changer un fichier qui n'est pas le sien, on demande à son
responsable.

| Partie | Responsable | Fichiers |
|---|---|---|
| Valeurs, objets, mémoire | `runtime` | `src/vm/value.c` `object.c` `table.c` `map.c` `memory.c` `chunk.c` `print.c` `binary.c` |
| Compilateur | `compilateur` | `src/vm/compiler*.c` |
| Machine virtuelle | `vm` | `src/vm/vm.c` `module.c` `debug.c` `main.c` |
| Natives | `natives` | `src/vm/natives/*.c`, `lib/*/*.c` |
| Tests et mesures | `tests` | `tests/run.sh` `tests/diff.sh` `tests/vm/` `bench/` |
| Contrat et intégration | `main` | les en-têtes `src/vm/*.h`, `Makefile`, `docs/` |

Un en-tête de `src/vm/` est le contrat entre plusieurs parties. Pour le changer,
le responsable écrit à `main` ce qu'il veut changer et pourquoi. Une exception :
`compilateur` et `vm` peuvent ajouter ou modifier ensemble des instructions dans
`chunk.h`, dès qu'ils sont d'accord.

Personne ne fait de `git commit` ni de `git push` : l'auteur du projet s'en
occupe.
