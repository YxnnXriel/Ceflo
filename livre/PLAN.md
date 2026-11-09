# Le plan du livre

**Écrire CeFlo** construit, dans l'ordre où on l'a vraiment écrit, un langage de
script complet en C : d'abord un interpréteur qui parcourt l'arbre (le
« visitor »), puis, quand on mesure qu'il est trop lent, un compilateur en
bytecode et une machine virtuelle.

Le lecteur sait programmer en C (pointeurs, `malloc`, structures). Il ne sait
rien des compilateurs. À la fin, il doit pouvoir réécrire CeFlo, ou un autre
langage, sans le livre.

Le dépôt est sur la branche `feat/bytecode` :
- l'avant du langage, commun aux deux interpréteurs : `src/lexer.c`,
  `src/parser.c`, `src/ast.c`, `src/token.c`, `src/error.c`, `src/str.c`,
  `src/list.c`, `src/common.c` ;
- l'interpréteur qui parcourt l'arbre (compilé en `./Ceflo_ref`) :
  `src/visitor.c`, `src/Value.c`, `src/value_expr.c`, `src/typer.c`,
  `src/scope.c`, `src/gc.c`, `src/map.c`, `src/bulletin.c`, `src/os.c`,
  `src/console.c`, `src/main.c` ;
- la machine virtuelle (compilée en `./Ceflo`) : `src/vm/`.
- `GRAMMAIRE.md` décrit le langage complet. `docs/VM.md` explique la VM.

Les sous-chapitres ci-dessous sont un point de départ : l'auteur d'un chapitre
peut les couper, les fusionner ou en ajouter, tant que le chapitre couvre ce qui
est prévu et ne déborde pas sur ses voisins.

## Partie 1 : Lire le texte

### 1. un fichier, une chaîne (`01-fichier.md`)
Ce qu'on va construire et comment le livre se lit. CeFlo en cinq minutes (un
petit programme qui montre le langage). Le chemin d'un programme :
texte, jetons, arbre, exécution. L'organisation du dépôt et le `Makefile`.
Lire un fichier entier (`readFile`, `src/str.c`). Garder le texte pour afficher
les erreurs avec la ligne et la flèche (`src/error.c`). Les petits outils dont
tout le reste se sert : `ceflo_alloc`, `buffer_t`, `list_t` (`src/common.c`,
`src/str.c`, `src/list.c`). Un premier `main` qui lit un fichier et l'affiche.

### 2. le lexer (`02-lexer.md`)
Ce qu'est un jeton (`token_t`, la liste des jetons et sa macro dans
`src/include/token.h`, `src/token.c`). Avancer dans le texte en tenant ligne et
colonne (`lexer_advance`, `lexer_peek`). Sauter les espaces et les commentaires.
Les noms et les mots-clés. Les nombres. Les chaînes et leurs échappements. Les
symboles d'un ou deux caractères. `newline_before` et l'absence de
point-virgule. Les erreurs de syntaxe du lexer. Fin du chapitre : un petit
programme qui affiche les jetons d'un fichier.

### 3. le parser (`03-parser.md`)
Lire `GRAMMAIRE.md`. La descente récursive : une fonction par règle
(`src/parser.c`). Avancer et exiger un jeton (`parser_eat`). Les expressions et
la priorité des opérateurs, étage par étage. Les appels, les accès `a.b` et
`a[i]`. Les instructions : `var`, `if`, `while`, `for`, `for ... in`, `return`.
Les fonctions, les structures, les énumérations, `match`. Les messages d'erreur
de syntaxe qui pointent le bon jeton. Fin : afficher l'arbre d'un programme.

### 4. l'arbre (`04-arbre.md`)
Un seul `struct` pour tous les noeuds (`src/include/ast.h`) : pourquoi ce
choix, et ce qu'il coûte. Allouer les noeuds et tout libérer à la fin
(`src/ast.c`). Ce que l'arbre garde pour les erreurs (fichier, ligne, colonne).
Les vérifications qu'on fait dès la lecture (valeurs déjà testées dans un
`match`, `break` hors d'une boucle...). L'arbre ne change jamais pendant
l'exécution : pourquoi c'est important pour la suite.

## Partie 2 : Exécuter l'arbre

### 5. les valeurs (`05-valeurs.md`)
`value_t` et ses types (`src/include/Value.h`, `src/Value.c`). Fabriquer une
valeur. Vrai et faux, l'égalité, l'affichage (`Value_to_buffer`). Les chaînes
qui ne changent jamais. Les listes et les maps (`src/map.c`, l'ordre
d'insertion). Les opérateurs (`src/value_expr.c`) et leurs messages d'erreur.

### 6. les portées (`06-portees.md`)
Une portée, c'est une table de noms et un lien vers la portée parente
(`src/scope.c`, `src/list.c`). Chercher un nom de proche en proche. Quand un
bloc a-t-il sa propre portée (`has_definition`) ? Les types déclarés avec `::`
et les constantes (`src/typer.c`). Les closures : une fonction se souvient de
sa portée.

### 7. le visitor (`07-visitor.md`)
Exécuter l'arbre : une fonction par sorte de noeud (`src/visitor.c`). Les
instructions et les signaux (`return`, `break`, `continue`). Les appels de
fonction. Les structures et les objets. `match`, `for ... in`. Les modules et
`import`. Les fonctions fournies (`src/bulletin.c`) et le module `os`. Le `main`
d'un script et son code de sortie. À la fin, CeFlo marche : `make test-ref`.

### 8. le garbage collector (`08-gc.md`)
Pourquoi on ne peut pas juste appeler `free`. Mark and sweep (`src/gc.c`). Les
racines, et la pile des temporaires que le visitor vide à chaque instruction.
Les réservoirs d'objets (`pool_t`, `src/common.c`). Chasser les bugs de mémoire :
`make sanitize`, le GC qui tourne à chaque allocation, valgrind.

## Partie 3 : Le bytecode

### 9. les instructions (`09-instructions.md`)
Mesurer : le visitor face à Python (`make bench`). Pourquoi c'est lent (chaque
nombre alloué, chaque nom cherché). Une valeur de 16 octets
(`src/vm/value.h`). Une machine à pile. Le chunk : code, constantes, et le
noeud de chaque octet pour les erreurs (`src/vm/chunk.h`, `src/vm/chunk.c`).
La liste des instructions. Le désassembleur (`src/vm/debug.c`, `--dis`).

### 10. le compilateur (`10-compilateur.md`)
Parcourir l'arbre une fois et écrire du bytecode (`src/vm/compiler.c`). Les
constantes. Les variables locales deviennent des cases de pile. Les globales
du fichier et les builtins. Les sauts : `if`, `while`, `for`, `and`, `or`. Les
fonctions. Les types vérifiés par `OP_CHECK_TYPE`. `format("${...}")` compilé.

### 11. la machine virtuelle (`11-vm.md`)
La boucle qui lit une instruction et l'exécute (`src/vm/vm.c`). La pile et les
`CallFrame`. Les appels de fonctions CeFlo et natives. Les erreurs à la bonne
ligne (`vm_error`). Le GC aux points sûrs (`src/vm/memory.c`). Les chaînes
internées et les autres. Computed goto, et les mesures finales.

### 12. closures et upvalues (`12-closures.md`)
Le problème : une variable de pile qui doit survivre à sa fonction. Les
upvalues ouvertes et fermées. `OP_CLOSURE`, `OP_GET_UPVALUE`,
`OP_CLOSE_UPVALUE`. Le piège des boucles : `for ... in` donne une variable
neuve à chaque tour.

### 13. structures et méthodes (`13-structures.md`)
Des attributs à place fixe (`ObjStruct`, `ObjInstance`). `self` implicite dans
une méthode. `__init__` selon le nombre d'arguments, `__str__`, `__cast__`.
Les énumérations et `match`. `OP_INVOKE` sans créer de méthode liée.

### 14. modules et natives (`14-modules.md`)
`import` et `export` dans la VM (`src/vm/module.c`). Les natives : builtins,
méthodes de list/map/str (`src/vm/natives/`). Les modules `os` et `console`.
Écrire une bibliothèque `.so` (`src/vm/ceflo.h`, `lib/`). Ce qu'on a construit,
ce qu'il reste à faire, et comment écrire son propre langage maintenant.
