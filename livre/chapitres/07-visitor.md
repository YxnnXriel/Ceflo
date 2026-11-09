---
numero: 7
titre: le visitor
note: l'arbre se met à marcher
partie: Exécuter l'arbre
---

Tout est prêt, et rien ne tourne. Le lexer découpe `fib.cf` en jetons, le parser en fait un arbre, le chapitre 5 sait fabriquer `832040` et l'afficher, le chapitre 6 sait retrouver `n` dans la bonne portée. Mais personne ne prend l'arbre pour le faire avancer. Il manque la pièce qui regarde un noeud `AST_IF`, calcule sa condition, et choisit la branche.

Cette pièce s'appelle le **visitor**, et c'est la plus satisfaisante du livre à écrire. On part d'un fichier vide. Vingt minutes plus tard, `println(1 + 2 * 3)` affiche `7`. Une heure plus tard, `fib(20)` affiche `6765`. À la fin du chapitre, ton fichier est exactement `src/visitor.c`, et les 216 tests du dépôt passent dessus.

::: etapes
texte -> jetons -> arbre -> visitor -> résultat
:::

## 7.1 Une fonction par sorte de noeud

Un interpréteur qui parcourt l'arbre (on dit aussi *tree-walking interpreter*) suit une idée très simple : pour exécuter un noeud, on exécute ses enfants, puis on combine leurs résultats. Pour `1 + 2 * 3`, le parser du chapitre 3 a construit cet arbre :

```text
        AST_BINOP (+)
        /           \
 AST_NUMBER 1     AST_BINOP (*)
                  /           \
           AST_NUMBER 2    AST_NUMBER 3
```

Exécuter le `+`, c'est exécuter son enfant gauche (qui rend `1`), puis son enfant droit. Exécuter le `*`, c'est exécuter `2`, puis `3`, et multiplier. La récursion descend jusqu'aux feuilles, puis remonte en portant les valeurs :

```text
visite +                      
  visite 1          -> 1
  visite *
    visite 2        -> 2
    visite 3        -> 3
    2 * 3           -> 6
  1 + 6             -> 7
```

La priorité des opérateurs, qui a coûté un chapitre entier au parser, ne coûte rien ici. Elle est déjà dans la forme de l'arbre : le `*` est plus bas, donc il est calculé avant.

Le nom « visitor » vient d'un patron de conception du livre *Design Patterns* (Gamma, Helm, Johnson et Vlissides, 1994). En Java, on y écrit une méthode `visitBinop`, `visitNumber`... par sorte de noeud, et chaque noeud appelle la bonne avec un `accept`. C'est ce que fait l'interpréteur `jlox` de *Crafting Interpreters*. Le module `ast` de Python propose la même chose avec `NodeVisitor`, qui cherche une méthode nommée `visit_` suivie du nom de la classe du noeud.

En C, il n'y a ni classes ni méthodes, et c'est plus simple : un `switch` sur `node->type`, et une fonction `visitor_visit_*` par `case`. Le chapitre 4 a choisi un seul `struct` pour tous les noeuds, avec un champ `type` : ce choix paie ici. On aurait pu aussi ranger les fonctions dans un tableau de pointeurs indexé par `type`, ce qui évite le `switch` ; mais un compilateur C transforme déjà un `switch` dense en table de sauts, et le `switch` se lit mieux.

Il faut décider de ce que rend une visite. Le commentaire en tête de `src/visitor.c` le dit : une expression retourne sa valeur, une instruction retourne `NULL`. `1 + 2` rend un `value_t *` ; `var x = 1` et `while` ne rendent rien. Une seule signature pour tout le monde :

```c
static value_t *visitor_visit_xxx(visitor_t *visitor, ast_t *node);
```

Et le `visitor_t` ? C'est l'état de l'exécution, ce qui n'est pas dans l'arbre. Il est déclaré dans `src/include/visitor.h` :

```c fichier="src/include/visitor.h" lignes="15-28"
    // Ce qui interrompt le déroulement normal des instructions
    typedef enum{
        SIGNAL_NONE,
        SIGNAL_RETURN,
        SIGNAL_BREAK,
        SIGNAL_CONTINUE,
    }visitor_signal;

    typedef struct VISITOR_STRUCT{
        scope_t *scope;            // la portée courante
        scope_t *builtins;         // println, input... le parent de chaque fichier
        visitor_signal signal;
        value_t *return_value;
    }visitor_t;
```

`scope` est la portée courante, celle du chapitre 6. `builtins` est la portée qui contient `println` et ses amies : la parente de chaque fichier. `signal` et `return_value` attendront la section 7.4. Quatre champs, c'est tout l'état d'un programme CeFlo qui tourne. Le reste est dans l'arbre, qui ne change pas, et dans les valeurs, que les portées désignent.

### Comment on va travailler

Le vrai `src/visitor.c` fait 1 142 lignes. On va le réécrire depuis un fichier vide, en cinq étapes qui compilent toutes. Chaque étape est un fichier de `livre/essais/chapitres-05-08/ch7/` (`etape1.c` à `etape5.c`), et un petit script le compile avec tout le reste de `src/` à la place de `src/visitor.c` :

```sh fichier="livre/essais/chapitres-05-08/ch7/compile.sh"
#!/bin/sh
# ./compile.sh etape1 : compile l'interpréteur avec ce visitor à la place de src/visitor.c
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O2 -I$SRC/include -o $1 $1.c \
    $(ls $SRC/*.c | grep -v '/visitor.c$') -rdynamic -lm -ldl
```

Le `grep -v` enlève `src/visitor.c` de la liste, et `src/vm/` n'y est pas (`ls src/*.c` ne descend pas dans les dossiers). On obtient un interpréteur complet dont tu as écrit le visitor. Les autres fichiers ont besoin de cinq fonctions du visitor : `visitor_visit`, `visitor_call`, `visitor_new_object`, `visitor_run_file` et `visitor_free`. Chaque étape doit donc les fournir, quitte à ce que certaines ne fassent encore rien.

## 7.2 Le premier programme

Le but de la première étape est ce programme :

```ceflo fichier="un.cf"
var x = 4
var nom = "CeFlo"
println(1 + 2 * 3)
println(x * x, " ", nom + "!")
println("x vaut ", x, ", et x == 4 vaut ", x == 4)
```

Il a besoin de six sortes de noeuds : les littéraux (`4`, `"CeFlo"`), `var`, une variable lue, un opérateur, un appel. Commence le fichier par les en-têtes :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
// Étape 1 du chapitre 7 : des expressions, des variables et println.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "visitor.h"
#include "error.h"
#include "parser.h"
#include "value_expr.h"
#include "typer.h"
#include "bulletin.h"
```

### Le centre : visiter un noeud

Écris d'abord la fonction que tout le monde appelle. Elle ne fait que transmettre au `switch` :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
value_t *visitor_visit(visitor_t *visitor, ast_t *node){
    value_t *value = visitor_dispatch(visitor,node);
    if(value != NULL)
        gc_temp_push(value);
    return value;
}
```

La ligne `gc_temp_push` est une dette envers le garbage collector. Chaque valeur calculée est notée dans la « pile des temporaires », pour que le GC ne la libère pas avant la fin de l'instruction. Pourquoi c'est nécessaire, et ce qui se passe sans elle, c'est tout le chapitre 8. Pour l'instant, recopie-la et fais-lui confiance.

Puis le `switch`, au-dessus de `visitor_visit` dans le fichier :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
static value_t *visitor_dispatch(visitor_t *visitor, ast_t *node){
    switch(node->type){
        case AST_NONE: return makeNone();
        case AST_BOOL: return makeBool(node->bool_value);
        case AST_CHAR: return makeChar(node->char_value);
        case AST_NUMBER: return makeNumber(node->number_value);
        case AST_STRING: return makeStringLenght(node->string_value,node->string_lenght);
        case AST_VARIABLE: return visitor_visit_variable(visitor,node);
        case AST_BINOP: return visitor_visit_binop(visitor,node);
        case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);
        case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);
        default:
            visitor_error(node,"ce noeud n'est pas encore exécuté (type %d)",node->type);
    }
}
```

Les littéraux sont la partie facile : le parser a rangé la valeur dans le noeud, on en fabrique un `value_t` avec les fonctions du chapitre 5. Remarque que `AST_NUMBER` fabrique un nouveau nombre à chaque visite. Le `1` d'une boucle qui tourne un million de fois est alloué un million de fois. On aurait pu le fabriquer une fois et le ranger dans le noeud ; on ne le fait pas, pour que l'arbre reste une description du programme, sans valeurs dedans. Le chapitre 10 réglera ce problème proprement, avec une table de constantes.

Le `default` est le filet de sécurité de cette étape. Tout noeud qu'on n'a pas encore écrit donne un message clair au lieu d'un plantage. `visitor_error` ne revient jamais (elle est déclarée `_Noreturn` dans `src/include/error.h`), et c'est pour ça que le compilateur ne se plaint pas qu'on sorte du `switch` sans `return`.

### Variables et opérateurs

Les quatre fonctions appelées par le `switch` vont au-dessus de lui. D'abord les variables, en version simple :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
static value_t *visitor_visit_variable_definition(visitor_t *visitor, ast_t *node){
    for(; node != NULL; node = node->left){
        value_t *value = node->variable_definition_value != NULL ?
            visitor_visit(visitor,node->variable_definition_value) : makeNone();
        scope_define(visitor->scope,node->variable_definition_name,value);
    }
    return NULL;
}

static value_t *visitor_visit_variable(visitor_t *visitor, ast_t *node){
    hash_t *item = scope_lookup(visitor->scope,node->variable_name);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item->value;
}
```

C'est le chapitre 6 sans les types : on calcule la valeur, on la range sous son nom. `var x` sans `=` vaut `none`. La boucle sur `left` gère `var a = 1, b = 2`. La lecture remonte les portées avec `scope_lookup`. Tu connais déjà la version complète des deux fonctions (section 6.4 et 6.2) ; on la mettra à l'étape 2.

L'opérateur ne fait que visiter ses deux côtés et passer la main à `value_expr`, qui contient toutes les règles du chapitre 5 :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
static value_t *visitor_visit_binop(visitor_t *visitor, ast_t *node){
    value_t *left = visitor_visit(visitor,node->left);
    value_t *right = visitor_visit(visitor,node->right);
    return value_expr(visitor,node,left,node->op,right);
}
```

L'ordre des deux lignes est une décision du langage : la gauche est calculée avant la droite. `f() + g()` appelle `f` en premier. C l'a laissé indéfini, et c'est la source de bugs célèbres ; Java, Python et JavaScript ont choisi la gauche d'abord, comme CeFlo. Ici, ce choix tient à l'ordre de deux lignes, et le vrai code le rappelle dans un commentaire (« Les arguments s'évaluent de gauche à droite », au-dessus de `VISIT_ARGS`) pour qu'il ne change pas par accident.

### L'appel

Il ne reste que `println(...)`. Un appel, c'est trois choses : trouver la fonction (le noeud `left` de l'appel, ici la variable `println`), calculer les arguments, appeler.

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    if(function->type == BUILTIN)
        return function->as.builtinValue.function(visitor,node,args,argc);
    visitor_error(node,"%s n'est pas une fonction",Value_type_name(function));
}

static value_t *visitor_visit_function_call(visitor_t *visitor, ast_t *node){
    value_t *function = visitor_visit(visitor,node->left);
    int argc = node->function_call_args->Size;
    value_t *args[argc > 0 ? argc : 1];
    for(int i = 0; i < argc; i++)
        args[i] = visitor_visit(visitor,List_Get(node->function_call_args,i));
    return visitor_call(visitor,node,function,args,argc);
}
```

La fonction est cherchée comme n'importe quelle variable : `println` n'est pas un mot réservé, c'est un nom de la portée `builtins`, dont la valeur est de type `BUILTIN` (chapitre 5). Un script peut donc écrire `var println = 3`, et c'est voulu.

Les arguments vont dans un tableau à taille variable sur la pile C (un *VLA*, permis depuis C99). C'est plus rapide qu'un `malloc` et ça se libère tout seul au retour. Le `argc > 0 ? argc : 1` évite un tableau de taille 0, que le C interdit. Une `BUILTIN` reçoit ce tableau tel quel : c'est la forme `ceflo_func_t` du chapitre 5, la même que `bulletin_println` dans `src/bulletin.c`.

### Lancer un fichier

Dernière pièce : `main.c` appelle `visitor_run_file`. Pour cette étape, il lit le fichier, crée deux portées et exécute les instructions une par une :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
static value_t *visitor_visit_statements(visitor_t *visitor, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        size_t mark = gc_temp_mark();
        visitor_visit(visitor,List_Get(node->list_compound,i));
        gc_temp_reset(mark);
    }
    return NULL;
}
```

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
int visitor_run_file(visitor_t *visitor, int argc, char **argv){
    memset(visitor,0,sizeof(*visitor));
    visitor->builtins = newScope(NULL);
    gc_root(visitor->builtins);
    bulletin_define_all(visitor->builtins,argc,argv);

    lexer_t lexer;
    if(!newLexer(&lexer,argv[0])){
        fprintf(stderr,"erreur: impossible d'ouvrir `%s`\n",argv[0]);
        exit(1);
    }
    ast_t *program = parser_parse(&lexer);

    visitor->scope = newScope(visitor->builtins);
    gc_root(visitor->scope);
    visitor_visit_statements(visitor,program);
    return 0;
}
```

`gc_temp_mark` et `gc_temp_reset` encadrent chaque instruction : à la fin de `println(...)`, les temporaires de la ligne sont oubliés. C'est l'autre moitié de la dette de `gc_temp_push`. `bulletin_define_all` remplit `builtins` avec `println`, `print`, `input`, `format`... et la liste `argv`. La portée du fichier est fille de `builtins` : c'est pour ça qu'un script voit `println` sans l'importer, et qu'il peut le redéfinir en le masquant.

Ajoute en bas du fichier les deux fonctions que réclament les autres fichiers, en attendant leur tour :

```c fichier="livre/essais/chapitres-05-08/ch7/etape1.c"
// Ce qui arrivera aux étapes suivantes.
value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    (void)visitor;(void)structure;(void)args;(void)argc;
    visitor_error(node,"pas encore de structures");
}

void visitor_free(void){
}
```

Compile, et lance :

```terminal
$ ./compile.sh etape1
$ ./etape1 un.cf
7
16 CeFlo!
x vaut 4, et x == 4 vaut true
```

Prends le temps de regarder cette sortie. Un texte que tu as écrit est passé par un lexer, un parser et une centaine de lignes de visitor, et il a calculé. `x * x` a cherché `x` dans la portée, deux fois. `nom + "!"` a assemblé du texte avec la règle du chapitre 5. `x == 4` a rendu le `true` unique de `makeBool`.

Et ce qui n'est pas encore écrit le dit poliment :

```terminal
$ cat pas_encore.cf
if true { println(1) }
$ ./etape1 pas_encore.cf
erreur: ce noeud n'est pas encore exécuté (type 27)
  --> pas_encore.cf:1:1
    1 | if true { println(1) }
      | ^
```

Le type 27 est `AST_IF`, le vingt-huitième de l'énumération de `src/include/ast.h` (on compte depuis 0). C'est la prochaine étape.

## 7.3 Les blocs, les conditions et les boucles

L'étape 2 apprend à choisir et à répéter. Copie `etape1.c` en `etape2.c`, et remplace les deux fonctions de variables par leurs versions complètes du chapitre 6 : `visitor_check_type`, `visitor_definition_type` et `visitor_visit_variable_definition` (lignes 76 à 111 de `src/visitor.c`), puis `visitor_find_variable` et `visitor_visit_variable` (lignes 113 à 126), qui gardent le hash du nom dans le noeud. Ajoute aussi `visitor_visit_compound` (lignes 60 à 70), qu'on a écrite en 6.3.

### if

Un `if` est une expression à calculer, puis une branche à visiter :

```c fichier="src/visitor.c" lignes="476-484"
static value_t *visitor_visit_if(visitor_t *visitor, ast_t *node){
    if(Value_is_true(visitor_visit(visitor,node->if_expr)))
        return visitor_visit(visitor,node->if_body);
    if(node->if_other != NULL)
        return visitor_visit(visitor,node->if_other);
    if(node->else_body != NULL)
        return visitor_visit(visitor,node->else_body);
    return NULL;
}
```

`Value_is_true` est la règle du chapitre 5 : `0`, `""`, `[]` sont faux. Le `if` de CeFlo est exactement le `if` du C qui l'exécute. C'est la beauté d'un interpréteur d'arbre : chaque construction du langage est écrite avec la construction équivalente du langage hôte.

`if_other` contient un `else if`. Le parser (chapitre 3) a rangé `else if c { }` comme un deuxième noeud `AST_IF` dans le premier, ce qui donne une chaîne : visiter `if_other`, c'est recommencer avec la condition suivante. `if_body` et `else_body` sont des `AST_COMPOUND`, qui créeront leur portée s'ils déclarent un nom.

### while

```c fichier="src/visitor.c" lignes="528-539"
static value_t *visitor_visit_while(visitor_t *visitor, ast_t *node){
    size_t mark = gc_temp_mark();

    while(Value_is_true(visitor_visit(visitor,node->while_expr))){
        visitor_visit(visitor,node->while_body);
        if(visitor_loop_should_stop(visitor))
            return NULL;
        gc_temp_reset(mark);
    }

    return NULL;
}
```

Le `while` de CeFlo est un `while` C. Deux lignes demandent une explication. `gc_temp_reset(mark)` à chaque tour : une boucle de dix millions de tours est une seule instruction pour `visitor_visit_statements`, qui ne viderait les temporaires qu'à la fin. Sans ce `reset`, la pile des temporaires garderait les dix millions de conditions calculées, et le GC ne pourrait jamais rien libérer. `visitor_loop_should_stop` concerne `break`, et on l'écrit en 7.4 ; recopie-la tout de suite (lignes 517 à 526), elle ne fait rien tant qu'aucun signal n'est levé.

### Le ternaire, le moins, et le court-circuit

Le ternaire `c ? a : b` est un `if` qui rend une valeur :

```c fichier="src/visitor.c" lignes="511-515"
static value_t *visitor_visit_ternary(visitor_t *visitor, ast_t *node){
    return Value_is_true(visitor_visit(visitor,node->if_expr)) ?
        visitor_visit(visitor,node->if_other) :
        visitor_visit(visitor,node->else_body);
}
```

Seule la branche choisie est visitée. C'est le moment de régler la question laissée à la fin du chapitre 5 : `and` et `or` ne peuvent pas aller dans `value_expr`, parce qu'il faut parfois ne pas calculer la droite. Remplace ta `visitor_visit_binop` par la vraie :

```c fichier="src/visitor.c" lignes="436-454" focus="440-450"
// `and` et `or` n'évaluent leur côté droit que s'il est utile.
static value_t *visitor_visit_binop(visitor_t *visitor, ast_t *node){
    value_t *left = visitor_visit(visitor,node->left);

    if(node->op == TOKEN_AND){
        if(!Value_is_true(left))
            return makeBool(false);
        return makeBool(Value_is_true(visitor_visit(visitor,node->right)));
    }

    if(node->op == TOKEN_OR){
        if(Value_is_true(left))
            return makeBool(true);
        return makeBool(Value_is_true(visitor_visit(visitor,node->right)));
    }

    value_t *right = visitor_visit(visitor,node->right);
    return value_expr(visitor,node,left,node->op,right);
}
```

`and` et `or` rendent toujours un `bool`. Python et JavaScript font un autre choix : `a or b` y rend `a` lui-même s'il est vrai, ce qui permet d'écrire `nom = saisie or "anonyme"`. C'est pratique, mais `0 or []` qui vaut `[]` surprend. CeFlo a gardé la règle de C et de Java : une condition est un booléen.

L'opérateur unaire (`-x`, `!x`) est à peine plus long (lignes 456 à 466), et `visitor_visit_string` (lignes 468 à 470) remplace le `makeStringLenght` du `switch`. Ajoute-les.

### Une affectation, en version simple

Il manque `i += 1` pour qu'une boucle s'arrête. La vraie affectation doit savoir écrire dans `l[0]`, dans `t.nom`, dans `p.x` ; ce sera la section 7.6. Pour l'instant, on n'accepte qu'une variable :

```c fichier="livre/essais/chapitres-05-08/ch7/etape2.c"
// Version simple : seule une variable peut recevoir une valeur (7.6 fera le reste).
static value_t *visitor_visit_assignement(visitor_t *visitor, ast_t *node){
    if(node->assignement_left->type != AST_VARIABLE)
        visitor_error(node,"pas encore : seule une variable peut être affectée");

    hash_t *item = visitor_find_variable(visitor,node->assignement_left);
    value_t *value = visitor_visit(visitor,node->assignement_value);
    if(node->op != TOKEN_EQUAL)
        value = value_expr(visitor,node,item->value,node->op,value);

    if(item->is_const)
        visitor_error(node,"`%s` est une constante",item->key);
    item->value = value;
    return NULL;
}
```

Le parser a rangé `i += 1` comme une affectation dont `op` vaut `TOKEN_PLUS`, et `i = 1` avec `op` à `TOKEN_EQUAL`. `+=` est donc un `value_expr` de l'ancienne valeur et de la nouvelle, puis une affectation. On n'écrit `item->value` qu'à la fin : c'est la case de la table du chapitre 6, et la modifier change la variable pour tous ceux qui la voient.

Ajoute au `switch` les nouveaux `case` :

```c fichier="livre/essais/chapitres-05-08/ch7/etape2.c"
        case AST_NOOP: return NULL;
        case AST_COMPOUND: return visitor_visit_compound(visitor,node);
```

```c fichier="livre/essais/chapitres-05-08/ch7/etape2.c"
        case AST_UNOP: return visitor_visit_unop(visitor,node);
        case AST_TERNARY: return visitor_visit_ternary(visitor,node);
```

```c fichier="livre/essais/chapitres-05-08/ch7/etape2.c"
        case AST_ASSIGNEMENT: return visitor_visit_assignement(visitor,node);
        case AST_IF: return visitor_visit_if(visitor,node);
        case AST_WHILE: return visitor_visit_while(visitor,node);
```

et essaie :

```ceflo fichier="deux.cf"
var i = 1
var total = 0
while i <= 10 {
    if i % 2 == 0 {
        total += i
    } else if i == 5 {
        println("cinq")
    }
    i += 1
}
println("somme des pairs : ", total)
println(total > 20 ? "grand" : "petit", " ", !true, " ", -i)
```

```terminal
$ ./compile.sh etape2
$ ./etape2 deux.cf
cinq
somme des pairs : 30
grand false -11
```

Avec des conditions et des boucles, le langage sait déjà tout calculer en théorie : c'est ce qu'on appelle être *Turing-complet*. En pratique, sans fonctions, aucun programme sérieux ne s'écrit.

## 7.4 Les signaux : return, break, continue

Avant les fonctions, il faut résoudre un problème que `return` pose à tout interpréteur d'arbre. Regarde :

```ceflo
func premier_multiple(n, de) {
    for var k = 1; k <= n; k += 1 {
        if k % de != 0 { continue }
        return k
    }
    return none
}
```

Quand le `return k` s'exécute, la pile C contient une dizaine d'appels emboîtés : l'appel de la fonction, qui visite son corps, qui visite le `for`, qui visite le bloc du `for`, qui visite les instructions, qui visite le `return`. Le `return` doit tout interrompre jusqu'à l'appel de fonction. Le `continue` doit interrompre le bloc, mais pas le `for`. Comment une fonction C du fond de la pile interrompt-elle celles qui l'ont appelée ?

Il y a trois réponses classiques.

La première, ce sont les exceptions du langage hôte. `jlox`, écrit en Java, lance une exception `Return` qui remonte jusqu'à l'appel. C'est court à écrire, mais le C n'a pas d'exceptions. Son équivalent, `setjmp` et `longjmp`, saute par-dessus les fonctions sans leur laisser le temps de remettre leur portée en place (`visitor->scope = outer` ne serait jamais exécuté).

La deuxième, c'est un code de retour : chaque fonction de visite rendrait « normal », « return », « break », et tout le monde le testerait. C'est ce que fait CPython pour ses erreurs : une fonction C qui échoue rend `NULL`, et chaque appelant le teste. Mais nos fonctions rendent déjà une valeur.

La troisième, celle de CeFlo, est un drapeau. `return` range sa valeur et lève `visitor->signal`. Tous ceux qui exécutent une suite d'instructions regardent le drapeau après chacune, et s'arrêtent s'il est levé. Celui que le signal concerne le baisse.

```c fichier="src/visitor.c" lignes="664-669"
static value_t *visitor_visit_return(visitor_t *visitor, ast_t *node){
    visitor->return_value = node->return_expr != NULL ?
        visitor_visit(visitor,node->return_expr) : makeNone();
    visitor->signal = SIGNAL_RETURN;
    return NULL;
}
```

`break` et `continue` sont encore plus courts : une ligne chacun dans le `switch` (`visitor->signal = SIGNAL_BREAK; return NULL;`). Puis la vraie `visitor_visit_statements` remplace la tienne :

```c fichier="src/visitor.c" lignes="45-58" focus="51-53"
// Exécute les instructions dans la portée courante.
static value_t *visitor_visit_statements(visitor_t *visitor, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        size_t mark = gc_temp_mark();
        visitor_visit(visitor,List_Get(node->list_compound,i));

        // On garde les temporaires : ils contiennent peut-être la valeur de `return`.
        if(visitor->signal != SIGNAL_NONE)
            return NULL;

        gc_temp_reset(mark);
    }
    return NULL;
}
```

Quand un signal est levé, on sort sans vider les temporaires. La valeur de `return k` y est notée, et elle doit survivre jusqu'à ce que l'appel de fonction la récupère : si on la vidait, le GC pourrait la libérer en chemin.

Le `if` et le `while` n'ont rien à faire de plus pour transmettre le signal : le `if` rend la main dès que sa branche a fini, et la boucle regarde le signal après chaque tour, avec la fonction qu'on a recopiée en 7.3 :

```c fichier="src/visitor.c" lignes="517-526"
// Après le corps d'une boucle : retourne true si la boucle doit s'arrêter.
static bool visitor_loop_should_stop(visitor_t *visitor){
    if(visitor->signal == SIGNAL_BREAK){
        visitor->signal = SIGNAL_NONE;
        return true;
    }
    if(visitor->signal == SIGNAL_CONTINUE)
        visitor->signal = SIGNAL_NONE;
    return visitor->signal == SIGNAL_RETURN;
}
```

Lis-la cas par cas. `break` : la boucle est concernée, elle baisse le drapeau et s'arrête. `continue` : elle baisse le drapeau et continue, le reste du corps a déjà été sauté par `visitor_visit_statements`. `return` : elle n'est pas concernée, elle s'arrête sans toucher au drapeau, pour que celui-ci remonte jusqu'à la fonction.

Une question vient tout de suite : et si `break` est écrit hors d'une boucle ? Le drapeau remonterait jusqu'au fichier, et personne ne le baisserait. C'est pour ça que le parser refuse `break` hors d'une boucle, et `return` hors d'une fonction (chapitre 4). Une vérification à la lecture évite un cas impossible à l'exécution.

La boucle `for` classique s'écrit avec les mêmes pièces :

```c fichier="src/visitor.c" lignes="541-560"
static value_t *visitor_visit_for(visitor_t *visitor, ast_t *node){
    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);

    if(node->for_init != NULL)
        visitor_visit(visitor,node->for_init);

    size_t mark = gc_temp_mark();

    while(node->for_condition == NULL || Value_is_true(visitor_visit(visitor,node->for_condition))){
        visitor_visit(visitor,node->for_body);
        if(visitor_loop_should_stop(visitor))
            break;
        visitor_visit_statements(visitor,node->for_solve);
        gc_temp_reset(mark);
    }

    visitor->scope = outer;
    return NULL;
}
```

Le `for` a toujours sa portée, pour que le `var k` de l'initialisation n'existe que dans la boucle. Une seule portée pour toute la boucle : le `k` du premier tour est le même que celui du dernier. Retiens ce détail, il reviendra en 7.8. `for_solve` est la liste des affectations de fin de tour (`k += 1`, et d'autres séparées par des virgules), exécutée comme une suite d'instructions. Pourquoi `break` et pas `return NULL` ici ? Pour passer par `visitor->scope = outer` : sortir d'une boucle doit toujours remettre la portée d'avant.

## 7.5 Les appels de fonction

L'étape 3 ajoute les fonctions CeFlo. Une définition, on l'a vue en 6.5 : elle range dans la portée une valeur `FUNCTION` qui garde son noeud et sa portée (lignes 303 à 310). Une `lfunc` est la même chose sans nom, en une ligne du `switch` : `case AST_LFUNC: return makeFunction(node,visitor->scope);`.

L'appel est la fonction la plus importante du fichier. On en a vu le milieu en 6.5 ; voici le début :

```c fichier="src/visitor.c" lignes="312-322"
static value_t *visitor_call_function(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    ast_t *definition = function->as.functionValue.definition;
    ast_t *where = node != NULL ? node : definition;

    if(argc != definition->function_definition_args->Size){
        visitor_error(where,"`%s` prend %d argument(s), mais en reçoit %d",
            definition->function_definition_name,
            definition->function_definition_args->Size,
            argc
        );
    }
```

`where` est l'endroit où pointer une erreur. D'habitude c'est l'appel, mais une fonction peut être appelée depuis le C sans noeud : `Value_to_buffer` appelle `__str__` avec `node == NULL` (chapitre 5). On pointe alors la définition. CeFlo exige le bon nombre d'arguments ; Python fait pareil, JavaScript remplit les absents avec `undefined` et ignore ceux en trop. Une erreur tout de suite vaut mieux qu'un `undefined` qui se promène.

Puis le milieu, qu'on connaît : une portée neuve, fille de la `closure`, les paramètres rangés dedans, le corps exécuté (lignes 324 à 336). Et la fin :

```c fichier="src/visitor.c" lignes="338-357"
    value_t *result = visitor->signal == SIGNAL_RETURN ? visitor->return_value : makeNone();
    visitor->signal = SIGNAL_NONE;
    visitor->return_value = NULL;

    const char *return_type = definition->function_definition_return_type_name;
    if(return_type != NULL && !typer_has_type(visitor,where,result,return_type)){
        visitor_error(where,"`%s` doit retourner une valeur de type %s, pas %s",
            definition->function_definition_name,
            return_type,
            typer_type_name(result)
        );
    }

    visitor->scope = caller;

    gc_temp_reset(mark);
    gc_temp_push(result);

    return result;
}
```

C'est ici que le signal `return` s'arrête. Si le corps s'est terminé par un `return`, on prend sa valeur ; sinon (on est tombé au bout du corps), la fonction rend `none`. Puis on baisse le drapeau. Un `break` ne peut pas arriver jusqu'ici, le parser l'a garanti.

Les trois dernières lignes avant le `return` sont un petit bijou. Pendant l'appel, le corps a créé des centaines de temporaires, tous notés depuis `mark`. On les oublie tous d'un coup (`gc_temp_reset(mark)`), puis on note seulement le résultat, qui doit vivre jusqu'à ce que l'appelant l'utilise. Sans ça, une fonction récursive comme `fib` garderait en vie tout ce qu'elle a calculé, à tous les étages.

`visitor_call` choisit entre une fonction CeFlo et une fonction C :

```c fichier="src/visitor.c" lignes="359-374"
value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    if(function->type == FUNCTION)
        return visitor_call_function(visitor,node,function,args,argc);

    if(function->type == BUILTIN)
        return function->as.builtinValue.function(visitor,node,args,argc);

    visitor_error(node,"%s n'est pas une fonction",Value_type_name(function));
}

// Les arguments s'évaluent de gauche à droite, avant l'appel.
#define VISIT_ARGS(visitor, node, name)                                                     \
    int name##_count = (node)->function_call_args->Size;                                    \
    value_t *name[name##_count > 0 ? name##_count : 1];                                     \
    for(int name##_i = 0; name##_i < name##_count; name##_i++)                              \
        name[name##_i] = visitor_visit(visitor,List_Get((node)->function_call_args,name##_i))
```

`VISIT_ARGS` est le tableau d'arguments de l'étape 1, transformé en macro parce que quatre fonctions en ont besoin (l'appel, l'appel de méthode, `new`, `@type`). Une fonction ne pourrait pas le faire : le tableau à taille variable doit être déclaré dans la fonction qui l'utilise, sinon il disparaît au retour. `name##_count` colle le nom donné à `_count` : `VISIT_ARGS(visitor,node,args)` déclare `args` et `args_count`.

Il reste une protection, dans `visitor_visit`. Remplace la tienne par la vraie :

```c fichier="src/visitor.c" lignes="1066-1080" focus="1072-1073"
/**
 * @brief visite un noeud. Sa valeur reste en vie jusqu'à la fin de l'instruction :
 *        dans `println(x, change())`, change() peut réaffecter x pendant que
 *        l'ancienne valeur attend son tour pour être affichée.
 */
value_t *visitor_visit(visitor_t *visitor, ast_t *node){
    if(ceflo_stack_exceeded())
        visitor_error(node,"trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais");

    value_t *value = visitor_dispatch(visitor,node);
    if(value != NULL)
        gc_temp_push(value);

    return value;
}
```

Chaque appel CeFlo coûte une dizaine d'appels C emboîtés, et chacun prend de la place sur la pile C. Une récursion sans fin finirait en segfault, sans un mot. `ceflo_stack_exceeded` (chapitre 1, `src/common.c`) compare l'adresse courante de la pile à celle du début du programme, et on s'arrête avec un message 256 Ko avant la limite. C'est un test par noeud visité, mais il ne coûte qu'une soustraction.

Ajoute à ton `switch` les `case` de `AST_LFUNC`, `AST_FUNCTION_DEFINITION`, `AST_RETURN`, `AST_BREAK`, `AST_CONTINUE` et `AST_FOR`, et lance le programme de 7.4, complété :

```ceflo fichier="trois.cf"
func fib(n) {
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}
println(fib(20))

func premier_multiple(n, de) {
    for var k = 1; k <= n; k += 1 {
        if k % de != 0 { continue }
        return k
    }
    return none
}
println(premier_multiple(100, 7), " ", premier_multiple(5, 7))

var j = 0
while true {
    j += 1
    if j == 3 { break }
}
println("sorti à ", j)
```

```terminal
$ ./compile.sh etape3
$ ./etape3 trois.cf
6765
7 none
sorti à 3
```

`6765`. C'est le vingtième nombre de Fibonacci, calculé par 21 891 appels de fonction que ton visitor a exécutés un par un, chacun avec sa portée, ses paramètres, son `return` qui remonte par le drapeau. Si tu fais une pause, fais-la ici, le temps de lancer `fib(25)` et regarder ton programme travailler.

Et la récursion sans fin donne un message au lieu d'un plantage :

```ceflo fichier="infini.cf"
func sans_fin(n) { return sans_fin(n + 1) }
sans_fin(0)
```

```terminal
$ ./Ceflo_ref infini.cf
erreur: trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais
  --> infini.cf:1:35
    1 | func sans_fin(n) { return sans_fin(n + 1) }
      |                                   ^
```

## 7.6 Les listes, les maps, et les places

L'étape 4 donne au langage ses données. Les constructions sont directes :

```c fichier="src/visitor.c" lignes="132-147"
static value_t *visitor_visit_list_init(visitor_t *visitor, ast_t *node){
    value_t *list = makeList();
    for(int i = 0; i < node->list_compound->Size; i++)
        Value_list_append(list,visitor_visit(visitor,List_Get(node->list_compound,i)));
    return list;
}

static value_t *visitor_visit_map_init(visitor_t *visitor, ast_t *node){
    value_t *map = makeMap();
    for(int i = 0; i < node->list_compound->Size; i += 2){
        value_t *key = visitor_visit(visitor,List_Get(node->list_compound,i));
        value_t *value = visitor_visit(visitor,List_Get(node->list_compound,i + 1));
        Value_map_set(node,map,key,value);
    }
    return map;
}
```

Un map littéral est rangé par le parser comme une liste à plat : clé, valeur, clé, valeur. `{ age: 22 }` a une clé `AST_STRING` `"age"`, et `{ [cle]: 1 }` une clé qui est une expression quelconque : les deux se visitent de la même façon.

Lire `a[i]` dépend de ce qu'est `a` (lignes 149 à 176). Une liste vérifie que l'index est un entier dans les bornes, avec `visitor_get_index`, qui écrit « l'index 5 n'existe pas (taille : 3) ». Une chaîne rend un `char`, fabriqué à la volée. Un map rend la valeur, ou `none` pour une clé absente. Lire `a.b` (lignes 285 à 297) est une clé de map, un élément d'énumération, ou un attribut d'objet. Et `a.f(x)` est un appel de méthode (lignes 376 à 404), qui cherche d'abord une fonction rangée dans le map, puis une méthode de l'objet, puis une méthode de `Value_call_method` (chapitre 5). Recopie ces fonctions : elles ne posent qu'une question, « qu'est-ce que la valeur de gauche ? ».

### Affecter n'importe où

La vraie difficulté est de l'autre côté du `=`. Une affectation peut écrire dans une variable, une case de liste, une clé de map, un attribut d'objet, et même un caractère de chaîne, qui ne change jamais (chapitre 5). Et `+=` doit lire l'ancienne valeur au même endroit. Le piège est de calculer l'endroit deux fois :

```ceflo
l[f()] += 1
```

Si on calcule `l[f()]` pour lire, puis `l[f()]` pour écrire, `f` est appelée deux fois, et peut rendre deux index différents. La réponse du vrai code est une **place** : on calcule une fois où écrire, on le range dans une petite structure, puis on lit et on écrit à travers elle.

```c fichier="src/visitor.c" lignes="675-686"
/*
    La cible d'une affectation s'évalue une seule fois : dans `l[f()] += 1`,
    la fonction f n'est appelée qu'une fois.
*/
typedef struct{
    enum{ PLACE_VARIABLE, PLACE_LIST, PLACE_STRING, PLACE_MAP } kind;
    hash_t *item;
    value_t *container;
    value_t *key;       // PLACE_MAP : NULL dans `t.nom`, où la clé est le nom du noeud
    int index;
    ast_t *node;
}place_t;
```

Un attribut d'objet est une `PLACE_VARIABLE` : l'objet est une portée (chapitre 5, `Obj_t`), son attribut est une case `hash_t`, exactement comme une variable. C'est l'avantage d'avoir fait des objets avec des portées. `visitor_get_place` (lignes 688 à 728) remplit la structure selon la forme de la cible : une variable donne sa case ; `a.b` visite `a`, puis choisit map ou attribut ; `a[i]` visite `a` et `i` et vérifie l'index.

Lire et écrire à travers une place sont deux `switch` (lignes 730 à 742, et 768 à 793). L'écriture garde les règles du chapitre 6 au même endroit, pour toutes les variables et tous les attributs :

```c fichier="src/visitor.c" lignes="768-782"
static void visitor_place_set(visitor_t *visitor, place_t *place, value_t *value){
    switch(place->kind){
        case PLACE_VARIABLE:
            if(place->item->is_const)
                visitor_error(place->node,"`%s` est une constante",place->item->key);
            if(place->item->type_name != NULL)
                visitor_check_type(visitor,place->node,place->item->key,value,place->item->type_name);
            place->item->value = value;
            break;
        case PLACE_LIST:
            // La liste a pu rétrécir pendant le calcul de la valeur.
            if(place->index >= AS_LIST(place->container)->Size)
                visitor_error(place->node,"l'index %d n'existe plus",place->index);
            AS_LIST(place->container)->Item[place->index] = value;
            break;
```

Regarde le commentaire de `PLACE_LIST`. L'index a été vérifié quand on a calculé la place. Mais entre-temps, on a calculé la valeur, et la valeur peut appeler une fonction qui vide la liste : `l[2] = l.remove(0) + l.remove(0)`. Sans ce second test, on écrirait hors du tableau. C'est le genre de cas qu'on ne voit qu'après l'avoir eu :

```terminal
$ cat retreci.cf
var l = [1, 2, 3]
l[2] = l.remove(0) + l.remove(0)
$ ./Ceflo_ref retreci.cf
erreur: l'index 2 n'existe plus
  --> retreci.cf:2:2
    2 | l[2] = l.remove(0) + l.remove(0)
      |  ^
```

Et le caractère de chaîne ? `s[0] = 'p'` ne peut pas modifier la chaîne. Il en construit une nouvelle, puis l'affecte à la place qui contenait la chaîne, ce qui demande de calculer la place du parent :

```c fichier="src/visitor.c" lignes="760-766"
    string_t *old = AS_STRING(place->container);
    value_t *string = makeStringLenght(old->str,old->lenght);
    AS_STRING(string)->str[place->index] = carac;

    place_t parent = visitor_get_place(visitor,place->node->left);
    visitor_place_set(visitor,&parent,string);
}
```

C'est le code promis au chapitre 5 : `a[0] = 'p'` range une nouvelle chaîne dans `a`, et `b` garde l'ancienne. Remarque la récursion : `t.nom[0] = 'Y'` écrit une chaîne neuve dans la clé `nom` de `t`, en passant par une place `PLACE_MAP`.

Enfin, l'affectation elle-même remplace ta version simple :

```c fichier="src/visitor.c" lignes="795-807"
static value_t *visitor_visit_assignement(visitor_t *visitor, ast_t *node){
    place_t place = visitor_get_place(visitor,node->assignement_left);
    value_t *value = visitor_visit(visitor,node->assignement_value);

    if(node->op != TOKEN_EQUAL){
        value_t *current = visitor_place_get(&place);
        gc_temp_push(current);
        value = value_expr(visitor,node,current,node->op,value);
    }

    visitor_place_set(visitor,&place,value);
    return NULL;
}
```

Une place, une valeur, une lecture si c'est `+=`, une écriture. Mais regarde l'ordre : la valeur de droite est calculée avant de lire l'ancienne valeur. Pour `x += change()`, où `change` modifie `x`, ça compte :

```ceflo fichier="compose.cf"
var x = 1
func change(){ x = 100; return 1 }
x += change()
println(x)
```

```terminal
$ ./Ceflo_ref compose.cf
101
$ ./Ceflo compose.cf
2
```

Les deux interpréteurs du dépôt ne sont pas d'accord. `Ceflo_ref` calcule `change()` d'abord (`x` vaut alors 100), puis lit `x` et ajoute 1. La machine virtuelle lit `x` d'abord (1), puis calcule `change()`, et range 2. Python, JavaScript et Java font comme la VM : `x += e` se lit « `x = x + e` », et `x` est à gauche, donc lu en premier. C'est une des deux différences voulues entre les deux interpréteurs, notées dans `docs/VM.md` et dans `tests/diff.sh`. Tu la retrouveras dans un instant, quand on lancera les tests.

Ajoute au `switch` `AST_LIST`, `AST_MAP`, `AST_OBJ_ACCESS` et `AST_LIST_ACCESS`, et `visitor_visit_function_call` passe à sa vraie version, qui envoie `a.f()` vers l'appel de méthode (lignes 406 à 414) :

```ceflo fichier="quatre.cf"
var l = [10, 20, 30]
l[1] += 5
l.append(40)
var profil = { nom: "Yann", age: 22 }
profil.age += 1
profil["ville"] = "Abidjan"
var mot = "chat"
mot[0] = 'p'
println(l, " ", l.lenght())
println(profil)
println(mot, " ", mot[3])
```

```terminal
$ ./compile.sh etape4
$ ./etape4 quatre.cf
[ 10 , 25 , 30 , 40 ] 4
{ "nom" : "Yann" , "age" : 23 , "ville" : "Abidjan" }
phat t
```

## 7.7 Les structures et les objets

`GRAMMAIRE.md` dit : « Un objet est une portée qui contient ses attributs et ses méthodes. » C'est littéral. Définir une structure, c'est ranger une valeur `STRUCT` qui garde son noeud et la portée où elle est écrite (lignes 193 à 197), comme une fonction. Créer un objet, c'est créer une portée et l'y remplir :

```c fichier="src/visitor.c" lignes="238-271"
value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    ast_t *definition = structure->as.structValue.definition;
    ast_t *init = visitor_find_init(definition,argc);

    if(init == NULL && argc > 0){
        visitor_error(node,"`%s` n'a pas de __init__ qui prend %d argument(s)",
            definition->struct_definition_name,argc);
    }

    scope_t *self = newScope(structure->as.structValue.closure);
    value_t *object = makeObject(structure,self);
    scope_define(self,"self",object)->is_const = true;

    scope_t *outer = visitor->scope;
    visitor->scope = self;

    for(int i = 0; i < definition->struct_definition_item->Size; i++){
        ast_t *item = List_Get(definition->struct_definition_item,i);

        if(item->type == AST_VARIABLE_DEFINITION){
            visitor_visit_variable_definition(visitor,item);
        }else if(strcmp(item->function_definition_name,"__init__") != 0 &&
                 strcmp(item->function_definition_name,"__cast__") != 0){
            scope_define(self,item->function_definition_name,makeFunction(item,self));
        }
    }

    visitor->scope = outer;

    if(init != NULL)
        visitor_call(visitor,node,makeFunction(init,self),args,argc);

    return object;
}
```

Suis les étapes. On cherche le `__init__` qui a le bon nombre de paramètres (`visitor_find_init`, lignes 222 à 232 : c'est la surcharge par nombre d'arguments que permet `GRAMMAIRE.md`). On crée la portée `self`, fille de la portée où la structure a été écrite, et on y range le nom `self`, constant, qui désigne l'objet.

Puis on exécute les déclarations de la structure dans cette portée. `var x::int = 0` y devient un attribut, avec son type, par la même fonction que toute variable. Chaque méthode devient une `FUNCTION` dont la `closure` est `self`. C'est toute l'explication de la règle « dans une méthode, `x` et `self.x` désignent le même attribut » : le corps d'une méthode s'exécute dans une portée fille de `self`, donc `x` y est trouvé en remontant, comme le `n` du compteur du chapitre 6.

Enfin, `__init__` est appelé comme n'importe quelle fonction. `__init__` et `__cast__` ne sont pas rangés dans l'objet : on ne peut pas écrire `p.__init__()`.

Ce choix a un prix, qu'il faut regarder en face. Chaque objet porte une copie de chaque méthode : un `Point` avec dix méthodes, créé un million de fois, fabrique dix millions de valeurs `FUNCTION`. Python range les méthodes dans la classe, une seule fois, et les retrouve à travers l'objet. Lua fait pareil avec les métatables. Le chapitre 13 fera de même dans la VM, avec `OP_INVOKE`. Ici, la simplicité a gagné : un objet est une portée, et tout le reste du visitor (lecture, affectation, appel) marche dessus sans une ligne de plus.

Les énumérations demandent moins : `makeEnum` (chapitre 5) a tout fait, et `visitor_visit_enum_definition` (lignes 199 à 203) range l'énumération sous son nom, comme constante. `visitor_visit_new` (lignes 416 à 425) cherche la structure par son nom et refuse `new Couleur()`. `@Point(1, 2)` passe par `typer_typer` (chapitre 6).

## 7.8 match et for ... in

`match` est un `if` en chaîne, avec l'égalité du chapitre 5 :

```c fichier="src/visitor.c" lignes="490-509"
static value_t *visitor_visit_match(visitor_t *visitor, ast_t *node){
    value_t *subject = visitor_visit(visitor,node->if_expr);
    bool has_enum = subject->type == ENUM_VALUE;

    for(int i = 0; i < node->list_compound->Size; i++){
        ast_t *ast_case = List_Get(node->list_compound,i);
        for(int j = 0; j < ast_case->list_compound->Size; j++){
            value_t *value = visitor_visit(visitor,List_Get(ast_case->list_compound,j));
            has_enum = has_enum || value->type == ENUM_VALUE;
            if(Value_equals(subject,value))
                return visitor_visit(visitor,ast_case->if_body);
        }
    }

    if(node->else_body != NULL)
        return visitor_visit(visitor,node->else_body);
    if(has_enum)
        visitor_error(node,"ce match ne traite pas %s",AS_STRING(Value_to_string(visitor,subject))->str);
    return NULL;
}
```

Le sujet est calculé une fois. Les valeurs des `case` sont calculées dans l'ordre, et on s'arrête à la première égale. Pas de `break` à écrire, et pas de chute dans le `case` suivant comme en C. La dernière règle est la plus intéressante : si on parle d'énumération et que rien ne correspond, c'est une erreur. Un `match` sur une couleur qui oublie `BLEU` est presque toujours un bug, surtout le jour où on ajoute `VIOLET` à l'énumération. Rust et Swift le vérifient à la compilation ; CeFlo, qui ne connaît pas les types avant l'exécution, le vérifie quand ça arrive.

`for ... in` parcourt une liste, un map ou une chaîne (lignes 649 à 662, un simple aiguillage). Chaque tour passe par une fonction commune :

```c fichier="src/visitor.c" lignes="571-584"
// Un tour de boucle a sa propre portée : une lfunc y garde la valeur de ce tour-là.
static bool visitor_for_in_turn(visitor_t *visitor, ast_t *node, value_t *first, value_t *second){
    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);

    visitor_for_in_define(visitor,node->for_init,first);
    if(node->for_init->left != NULL)
        visitor_for_in_define(visitor,node->for_init->left,second);

    visitor_visit(visitor,node->for_body);
    visitor->scope = outer;

    return !visitor_loop_should_stop(visitor);
}
```

Une portée neuve par tour, et c'est la différence avec le `for` classique de 7.4. Elle se voit dès qu'on crée des fonctions dans la boucle :

```ceflo fichier="boucle_for.cf"
var fonctions = []
for var i = 0; i < 3; i += 1 {
    fonctions.append(lfunc() { return i })
}
for var f in fonctions {
    print(f(), " ")
}
println()

var autres = []
for var j in [0, 1, 2] {
    autres.append(lfunc() { return j })
}
for var f in autres {
    print(f(), " ")
}
println()
```

```terminal
$ ./Ceflo_ref boucle_for.cf
3 3 3 
0 1 2 
```

Dans le `for` classique, les trois `lfunc` ont pour `closure` la même portée, celle du `for`, où il n'y a qu'un `i`, qui vaut 3 à la fin. Dans le `for ... in`, chaque `lfunc` garde la portée de son tour, avec son propre `j`. JavaScript a connu exactement ce problème avec `var`, et l'a réglé en 2015 avec `let`, qui donne une variable neuve à chaque tour, même dans un `for` classique. Le chapitre 12 retrouvera ce piège dans la VM, où il est bien plus difficile à résoudre.

Le parcours d'un map (lignes 586 à 616) relève les clés avant le premier tour, pour que le corps puisse ajouter ou enlever des clés sans casser la boucle. Il a une petite astuce : tant que le map ne bouge pas, la paire suivante dans `entries` est la bonne, et on évite une recherche par tour. La liste relit sa taille à chaque tour (lignes 618 à 633) : un élément ajouté pendant la boucle est parcouru.

L'étape 5 ajoute tout ça au `switch` (`AST_STRUCT`, `AST_TYPE_CALL`, `AST_STRUCT_DEFINITION`, `AST_ENUM_DEFINITION`, `AST_MATCH`, `AST_CASE`, `AST_FOR_IN`) et remplace le faux `visitor_new_object` par le vrai :

```ceflo fichier="cinq.cf"
struct Point {
    var x::int = 0
    var y::int = 0

    func __init__(a, b) {
        x = a
        self.y = b
    }

    func __str__() {
        return "(" + x + ", " + y + ")"
    }

    func avance(n) {
        x += n
        return self
    }
}
```

```ceflo fichier="cinq.cf"
enum Couleur { ROUGE, VERT, BLEU }

var p = new Point(1, 2)
println(p.avance(3).avance(1))

for var c in Couleur.values() {
    match c {
        case Couleur.ROUGE { println(c, " est chaud") }
        case Couleur.VERT, Couleur.BLEU { println(c.name, " est froid") }
    }
}

for var cle, valeur in { a: 1, b: 2 } {
    println(cle, " -> ", valeur)
}
```

```terminal
$ ./compile.sh etape5
$ ./etape5 cinq.cf
(5, 2)
Couleur.ROUGE est chaud
VERT est froid
BLEU est froid
a -> 1
b -> 2
```

`println(p...)` a trouvé `__str__` dans l'objet et l'a appelé : c'est `Value_object_to_buffer` du chapitre 5, qui appelle `visitor_call`, qui exécute ton visitor. Tout se tient.

## 7.9 Les modules et les bibliothèques en C

Il reste à faire parler les fichiers entre eux. `from outils.texte import [crie]` doit trouver `outils/texte.cf`, l'exécuter, et copier `crie` dans la portée courante. Le coeur est une fonction qui exécute une source dans une portée neuve :

```c fichier="src/visitor.c" lignes="813-833"
// Les fichiers déjà chargés : un fichier ne s'exécute qu'une seule fois.
static table_t modules;
static list_t *dll_handles;

static scope_t *visitor_run_source(visitor_t *visitor, lexer_t *lexer){
    ast_t *program = parser_parse(lexer);

    scope_t *scope = newScope(visitor->builtins);
    gc_root(scope);

    ast_t *holder = newAst(AST_NOOP);
    allowChar(&holder->variable_name,lexer->fileName);
    Hash_Set(&modules,holder->variable_name,scope);

    scope_t *outer = visitor->scope;
    visitor->scope = scope;
    visitor_visit_statements(visitor,program);
    visitor->scope = outer;

    return scope;
}
```

C'est le `visitor_run_file` de l'étape 1, rangé dans une fonction. La portée du fichier est une racine du GC : un module reste chargé jusqu'à la fin. Son nom va dans la table `modules`, la table du chapitre 6 réutilisée, pour qu'un fichier importé deux fois ne s'exécute qu'une fois.

Le `holder` est une ruse. La table ne copie pas ses clés (chapitre 6), et le nom du fichier doit vivre jusqu'à la fin. On le copie donc dans un noeud d'arbre vide, que `clearAst` libérera avec tous les autres à la sortie du programme. Personne n'a à se souvenir de le libérer.

L'import lui-même (lignes 964 à 996) cherche le chemin à côté du fichier qui importe, puis depuis le dossier courant (`visitor_resolve_path`), le charge s'il ne l'est pas encore, puis copie les noms. La copie se fait dans `visitor_import_names` :

```c fichier="src/visitor.c" lignes="884-898"
static void visitor_import_names(visitor_t *visitor, ast_t *node, scope_t *module){
    for(int i = 0; i < node->list_compound->Size; i++){
        ast_t *name = List_Get(node->list_compound,i);
        hash_t *item = scope_get(module,name->variable_name);

        if(item == NULL)
            visitor_error(name,"`%s` n'existe pas dans `%s`",name->variable_name,node->variable_name);
        if(!visitor_is_exported(module,name->variable_name))
            visitor_error(name,"`%s` n'est pas exporté par `%s`",name->variable_name,node->variable_name);

        hash_t *copy = scope_define(visitor->scope,name->variable_name,item->value);
        copy->type_name = item->type_name;
        copy->is_const = item->is_const;
    }
}
```

« Copier un nom », c'est créer une case dans la portée courante avec la même valeur. Ce n'est pas la même case. La conséquence se voit avec un compteur de module :

```ceflo fichier="outils/texte.cf"
println("chargement de outils/texte.cf")
var compteur = 0
func crie(mot) {
    compteur += 1
    return mot + "!"
}
func secret() { return 42 }
export :: [crie, compteur]
```

```ceflo fichier="modules.cf"
from outils.texte import [crie]
from outils.texte import [crie, compteur]
println(crie("bonjour"), " ", compteur)
from outils.texte import [secret]
```

```terminal
$ ./Ceflo_ref modules.cf
chargement de outils/texte.cf
bonjour! 0
erreur: `secret` n'est pas exporté par `outils/texte.cf`
  --> modules.cf:4:27
    4 | from outils.texte import [secret]
      |                           ^
```

Le module n'est chargé qu'une fois, malgré deux `import`. `crie` a bien augmenté `compteur`, mais celui du module : notre copie, faite avant l'appel, vaut toujours 0. Python fait exactement pareil avec `from module import compteur`, et pour la même raison : on a copié une valeur, pas un lien vers la variable. Pour suivre la valeur du module, il faut une fonction qui la rend. `export :: [crie, compteur]` (lignes 1001 à 1012) a rangé la liste des noms publics dans `exports` de la portée du fichier, et `secret` n'y est pas.

Un module fourni par l'interpréteur, comme `os`, passe par le même chemin : si aucun fichier `os.cf` n'existe, `visitor_standard_module` (lignes 941 à 962) crée une portée et la remplit avec `os_define_all`, une table de fonctions C comme celle de `println`.

### Une fonction écrite en C

`from @"..." import [...]` charge une bibliothèque `.so`. C'est `dlopen` pour ouvrir le fichier, `dlsym` pour trouver chaque nom, et une valeur `BUILTIN` pour le ranger (lignes 902 à 929). Une fonction de bibliothèque a donc exactement la forme de `println` : la `ceflo_func_t` de `src/include/Value.h`. On peut en écrire une en dix lignes :

```c fichier="livre/essais/chapitres-05-08/ch7/carre.c"
// Une fonction écrite en C pour Ceflo_ref : elle a la forme de ceflo_func_t (src/include/Value.h).
#include "Value.h"
#include "visitor.h"

value_t *carre(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"carre",argc,1);
    Value_check_type(node,"carre",args[0],NUMBER);
    return makeNumber(AS_NUMBER(args[0]) * AS_NUMBER(args[0]));
}
```

```ceflo fichier="natif.cf"
from @"./libcarre.so" import [carre]
println(carre(12))
println(carre("douze"))
```

```terminal
$ gcc -std=c11 -Wall -Wextra -fPIC -shared -I../../../src/include -o libcarre.so carre.c
$ ../../../Ceflo_ref natif.cf
144
erreur: `carre` attend un argument de type number, pas str
  --> natif.cf:3:14
    3 | println(carre("douze"))
      |              ^
```

La bibliothèque appelle `makeNumber` et `Value_check_args`, qui ne sont pas dans le `.so` : ils sont dans `Ceflo_ref`. C'est le rôle de l'option `-rdynamic` du `Makefile` : elle publie les fonctions de l'exécutable, pour que `dlopen` les trouve au chargement.

> [!attention]
> Écris `"./libcarre.so"`, pas `"libcarre.so"`. Quand le chemin ne contient aucun `/`, `dlopen` ne regarde pas le dossier courant : il cherche dans les dossiers système des bibliothèques, et échoue avec « cannot open shared object file ». Le `./` suffit.

Les bibliothèques de `lib/` (`make libs`) ont une autre forme, celle de la VM (`src/vm/ceflo.h`, chapitre 14). Elles ne se chargent pas dans `Ceflo_ref`, et c'est l'autre différence voulue des tests.

## 7.10 Les fonctions fournies

`println`, `input`, `format` et les autres vivent dans `src/bulletin.c`. La plupart sont courtes, parce que le chapitre 5 a fait le travail :

```c fichier="src/bulletin.c" lignes="9-20"
static value_t *bulletin_print(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)node;
    for(int i = 0; i < argc; i++)
        Value_print(visitor,stdout,args[i]);
    return makeNone();
}

static value_t *bulletin_println(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    bulletin_print(visitor,node,args,argc);
    printf("\n");
    return makeNone();
}
```

Elles sont rangées dans un tableau de noms, parcouru par `bulletin_define_all` (lignes 177 à 196), sur le modèle des méthodes du chapitre 5. Ajouter une fonction fournie, c'est écrire une fonction et ajouter une ligne au tableau.

`format` est la seule qui mérite qu'on s'y arrête, parce qu'elle exécute du CeFlo écrit dans une chaîne. `format("${nom} aura ${age + 1} ans")` doit lire `age + 1` comme une expression, la calculer dans la portée de l'appelant, et coller le résultat. Elle utilise tout ce qu'on a :

```c fichier="src/bulletin.c" lignes="91-109"
// Un même ${...} n'est analysé qu'une fois, même dans une boucle.
static table_t format_cache;

static ast_t *bulletin_format_parse(const char *expr){
    hash_t *cached = Hash_Get(&format_cache,expr);
    if(cached != NULL)
        return cached->value;

    lexer_t lexer;
    newLexerWithString(&lexer,"format",expr);
    ast_t *node = parser_parse_one(&lexer);

    // La clé doit vivre aussi longtemps que le cache : un noeud de l'AST la garde.
    ast_t *holder = newAst(AST_NOOP);
    allowChar(&holder->variable_name,expr);
    Hash_Set(&format_cache,holder->variable_name,node);

    return node;
}
```

Un lexer sur une chaîne, le parser du chapitre 3 pour une seule expression (`parser_parse_one`), et l'arbre obtenu est rangé dans un cache : un `format` dans une boucle ne relit pas son texte à chaque tour. Puis `bulletin_format` (lignes 111 à 161) parcourt le texte, trouve chaque `${...}` en comptant les accolades, et appelle `visitor_visit` sur l'arbre. C'est un interpréteur dans l'interpréteur, en vingt lignes, parce que le visitor visite n'importe quel arbre, d'où qu'il vienne.

```ceflo fichier="fournies.cf"
from os import [commande, existe]
var nom = "Yann"
var age = 22
println(format("${nom} aura ${age + 1} ans, \${pas calculé}"))
println(existe("fournies.cf"), " ", existe("absent.txt"))
var r = commande("echo depuis le shell")
println(r.code, " ", r.sortie)
```

```terminal
$ ./Ceflo_ref fournies.cf
Yann aura 23 ans, ${pas calculé}
true false
0 depuis le shell

```

La ligne vide finale est le retour à la ligne de `echo`, gardé dans `r.sortie`. Le module `os` (`src/os.c`) suit une règle écrite en tête du fichier : CeFlo n'a pas d'exceptions, donc un fichier absent rend `none` ou `false`, et le script décide quoi faire. Seule une mauvaise utilisation, comme `lire(3)`, arrête le programme.

## 7.11 main, le code de sortie, et les tests

Il reste la vraie `visitor_run_file`, qui remplace celle de l'étape 1 :

```c fichier="src/visitor.c" lignes="1115-1129"
int visitor_run_file(visitor_t *visitor, int argc, char **argv){
    memset(visitor,0,sizeof(*visitor));

    visitor->builtins = newScope(NULL);
    gc_root(visitor->builtins);
    bulletin_define_all(visitor->builtins,argc,argv);

    lexer_t lexer;
    if(!newLexer(&lexer,argv[0])){
        fprintf(stderr,"erreur: impossible d'ouvrir `%s`\n",argv[0]);
        exit(1);
    }

    return visitor_run_main(visitor,visitor_run_source(visitor,&lexer));
}
```

Le fichier principal passe par `visitor_run_source`, comme un module, puis par `visitor_run_main` :

```c fichier="src/visitor.c" lignes="1090-1113"
static int visitor_run_main(visitor_t *visitor, scope_t *scope){
    hash_t *item = scope_get(scope,"main");
    if(item == NULL)
        return 0;

    value_t *function = item->value;
    if(function->type != FUNCTION || function->as.functionValue.closure != scope)
        return 0;

    ast_t *definition = function->as.functionValue.definition;
    int argc = definition->function_definition_args->Size;
    if(argc > 1)
        visitor_error(definition,"`main` prend au plus un paramètre : argv");

    value_t *arguments = scope_get(visitor->builtins,"argv")->value;
    value_t *result = visitor_call(visitor,definition,function,&arguments,argc);

    if(result->type == NONE)
        return 0;
    if(!typer_has_type(visitor,definition,result,"int"))
        visitor_error(definition,"`main` doit retourner un int, pas %s",typer_type_name(result));

    return (int)Value_number_to_char(AS_NUMBER(result)) & 0xFF;
}
```

Trois tests avant d'appeler. Pas de `main` : on a fini. Un `main` qui n'est pas une fonction (une variable peut s'appeler ainsi) : on a fini aussi. Un `main` dont la `closure` n'est pas la portée du fichier : c'est un `main` importé d'un module, avec `import *` ou par son nom, et on ne le lance pas. Ce dernier test est une jolie application des closures : une fonction sait dans quel fichier elle a été écrite, sans qu'on ait rien ajouté pour ça.

Le code de sortie d'un processus Unix tient sur un octet. `& 0xFF` garde l'octet du bas, comme le fait le C pour `return 256` dans `main`. `argc` vaut 0 ou 1 : `visitor_call` passe `&arguments` dans les deux cas, mais n'en lit que `argc`.

```ceflo fichier="sortie.cf"
println("le haut du fichier s'exécute d'abord")

func main(argv) {
    if argv.lenght() < 2 {
        println("utilisation : ", argv[0], " nom")
        return 1
    }
    println("Bonjour ", argv[1])
    return 0
}
```

```terminal
$ ./Ceflo_ref sortie.cf; echo "code : $?"
le haut du fichier s'exécute d'abord
utilisation : sortie.cf nom
code : 1
$ ./Ceflo_ref sortie.cf Yann; echo "code : $?"
le haut du fichier s'exécute d'abord
Bonjour Yann
code : 0
```

Ajoute les dernières fonctions (le module `include` et `export`, `visitor_free`, qui ferme les bibliothèques avec `dlclose`), complète le `switch` avec `AST_INCLUDE` et `AST_EXPORT`, et ton fichier est maintenant `src/visitor.c`, à la ligne près. `main.c` l'appelle, et nettoie tout à la sortie avec `atexit` :

```c fichier="src/main.c" lignes="23-32"
// Appelée à chaque sortie du programme, même après une erreur.
static void ceflo_clean(void){
    fflush(stdout);
    visitor_free();
    bulletin_free();
    gc_free_all();
    Hash_Free_Pool();
    clearAst();
    error_free_sources();
}
```

`atexit` est appelée même quand `visitor_error` fait `exit(1)` au milieu d'une instruction. Le `fflush(stdout)` y est pour ça : ce qu'un script a affiché avant une erreur sort avant le message.

### Les tests

Le dépôt a des dizaines de programmes de test, chacun avec sa sortie attendue (`tests/x.cf` et `tests/x.out`), des programmes qui doivent échouer avec un message précis (`tests/erreurs/`), et les exemples. `tests/run.sh` lance tout. Compile le vrai `Ceflo_ref` et lance-les :

```terminal
$ make Ceflo_ref
make: « Ceflo_ref » est à jour.
$ make test-ref
...
ÉCHEC  natif (code 127)
/home/arya/Bureau/PROG/Shift-Projet/Ceflo Interpreter/Ceflo_ref: symbol lookup error: ../lib/math/libmath.so: undefined symbol: native_check_args
ÉCHEC  vm/affectation_composee
1,5c1,5
< 2
< [ 2 ]
< { "n" : 2 }
< 2
< 0
---
> 101
> [ 101 ]
> { "n" : 101 }
> 200
> 99
216 réussis, 2 échoués
```

Ne t'inquiète pas des deux échecs : tu les connais. `natif` charge une bibliothèque de `lib/`, compilée pour la VM, qui cherche `native_check_args`, une fonction que `Ceflo_ref` n'a pas. `vm/affectation_composee` est le `x += change()` de la section 7.6 : la sortie attendue est celle de la VM. Les deux sont écrits dans la liste des différences voulues de `tests/diff.sh`. Sautons-les :

```terminal
$ CEFLO_TEST_SKIP="natif affectation_composee" tests/run.sh ./Ceflo_ref
216 réussis, 0 échoués
```

Les 216 vérifications passent. Les tests de `tests/erreurs/` vérifient même la première ligne de chaque message d'erreur. Ton visitor exécute les exemples du dépôt, le lexer en CeFlo de `exemple/6_lexer.cf`, les structures, les modules, `match`, les closures. CeFlo marche.

## Défis

1. **Une instruction `repeat`.** Ajoute `repeat n { ... }`, qui exécute le bloc `n` fois. Il faut un jeton (`src/include/token.h`), un noeud et une fonction de parser (chapitre 3), puis une `visitor_visit_repeat` sur le modèle de `visitor_visit_while`. N'oublie ni `visitor_loop_should_stop`, ni `gc_temp_reset` à chaque tour : lance une boucle de dix millions de tours sans lui, et regarde la mémoire avec `/usr/bin/time -v`.

2. **Les valeurs par défaut des paramètres.** Fais accepter `func salue(nom, poli = true)`. Le parser doit garder la valeur dans le noeud du paramètre (`variable_definition_value` existe déjà), et `visitor_call_function` doit accepter moins d'arguments et visiter la valeur par défaut des manquants. Dans quelle portée faut-il la visiter, celle de l'appelant ou celle de la fonction ? Essaie les deux avec `func f(a, b = a * 2)`.

3. **Une pile d'appels dans les erreurs.** Quand une erreur arrive dans une fonction, affiche aussi les appels qui y ont mené, comme Python (`Traceback`). Garde dans `visitor_t` une liste des noeuds d'appel en cours, ajoute en entrée de `visitor_call_function` et retire en sortie. Le plus délicat : `visitor_error` est dans `src/error.c` et ne connaît pas le visitor.

4. **Mesurer le prix des portées.** `fib(30)` crée une portée par appel. Compte-les : ajoute un compteur dans une copie de `src/scope.c`, affiche-le à la fin, et compare au nombre d'appels (2 692 537 pour `fib(30)`, comme le dit le chapitre 1). Puis demande-toi combien de ces portées pourraient être évitées, et ce qu'il faudrait savoir avant l'exécution pour les éviter. Le chapitre 10 répond.

## Note de conception

Un interpréteur qui parcourt l'arbre a mauvaise réputation, parce qu'il est lent. On le verra en chiffres au chapitre 9. Mais regarde ce qu'il a coûté : un fichier de 1 142 lignes, écrit en cinq étapes, où chaque construction du langage est écrite avec la construction équivalente du C. Le `if` de CeFlo est un `if`, son `while` un `while`, sa récursion une récursion. Une erreur pointe directement le noeud fautif, parce que le noeud est là, dans la main de la fonction qui échoue.

Beaucoup de langages ont commencé ainsi. Ruby a été un interpréteur d'arbre jusqu'à la version 1.9, en 2007. Ce n'est pas un brouillon : c'est un interpréteur complet, qu'on peut lire et corriger en une soirée.

Il a surtout un usage qu'on oublie : il sert de référence. Quand on a écrit la machine virtuelle, il était là pour dire ce que chaque programme devait afficher. `make diff` lance tous les tests et tous les exemples sur les deux interpréteurs et compare. Chaque fois qu'ils ne sont pas d'accord, l'un des deux a un bug, ou bien le langage avait un coin mal défini, qu'il a fallu trancher (comme `x += change()`). Un compilateur optimisé est difficile à rendre juste ; un interpréteur simple qui dit la vérité rend ce travail possible.

Mon avis : si tu écris un langage, écris d'abord celui-ci. Il te dira si ton langage est agréable avant que tu aies investi dans sa vitesse. Puis garde-le, même quand tu auras mieux.

Il reste une promesse qu'on n'a pas tenue. Depuis le début du chapitre, on appelle `gc_temp_push`, `gc_temp_reset`, `gc_root`, et on fabrique des millions de valeurs sans jamais en libérer une seule. Qui les libère, et comment sait-il qu'elles ne servent plus ? Et que se passerait-il si on retirait une seule de ces lignes ? Le chapitre 8 les retire, pour voir.
