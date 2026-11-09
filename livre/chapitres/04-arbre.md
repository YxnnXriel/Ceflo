---
numero: 4
titre: l'arbre
note: ce que le parser laisse derrière lui
partie: Lire le texte
---

Le programme `fib.cf` fait 94 octets. Son arbre en fait 9 632.

```terminal
$ livre/essais/chapitres-01-04/compte livre/essais/chapitres-01-04/fib.cf livre/essais/chapitres-01-04/tour.cf exemple/6_lexer.cf tests/structures.cf
un noeud : 344 octets
livre/essais/chapitres-01-04/fib.cf             94 octets de texte,    28 noeuds,     9632 octets d'arbre
livre/essais/chapitres-01-04/tour.cf          1029 octets de texte,   124 noeuds,    42656 octets d'arbre
exemple/6_lexer.cf             1629 octets de texte,   189 noeuds,    65016 octets d'arbre
tests/structures.cf            3143 octets de texte,   479 noeuds,   164776 octets d'arbre
```

Cinquante fois le texte, à peu près, quel que soit le fichier. Un nombre comme `2`, un seul octet dans le fichier, devient un noeud de 344 octets, dont il n'utilise que six champs sur quarante-six. Et pourtant, ce n'est pas une erreur de conception, ou du moins pas une erreur qu'on aurait dû éviter. C'est un choix, fait en connaissant son prix, et ce chapitre explique lequel.

Au chapitre 3, on a fabriqué des noeuds sans trop se demander ce qu'ils étaient. On a vu `left` servir à une soustraction, puis à chaîner des variables. On a vu `if_other` porter le `else if` d'un `if`, puis le « alors » d'un ternaire. On a vu des noeuds créés et jamais rangés nulle part. Il est temps de regarder `src/include/ast.h` et `src/ast.c` en face : ce que contient un noeud, comment on le crée et comment on le libère, ce qu'il garde pour les messages d'erreur, ce que le parser vérifie avant de le rendre, et pourquoi, une fois construit, l'arbre ne doit plus jamais bouger.

## 4.1 Toutes les sortes de noeuds

Commence par la liste des sortes. Le parser en a utilisé trente-quatre, plus une qu'on n'a pas encore vue :

```c fichier="src/include/ast.h" lignes="16-39"
    typedef enum{
        AST_NOOP,
        AST_COMPOUND,            // { instructions... }

        // Valeurs écrites dans le code
        AST_NONE,
        AST_BOOL,
        AST_CHAR,
        AST_NUMBER,
        AST_STRING,
        AST_LIST,
        AST_MAP,                 // { a: 1 } : list_compound contient clé, valeur, clé, valeur...
        AST_LFUNC,

        // Expressions
        AST_VARIABLE,
        AST_OBJ_ACCESS,          // a.b
        AST_LIST_ACCESS,         // a[b]
        AST_FUNCTION_CALL,       // a(b)
        AST_STRUCT,              // new A(b)
        AST_TYPE_CALL,           // @A(b)
        AST_BINOP,
        AST_UNOP,
        AST_TERNARY,
```

Les valeurs écrites dans le code d'abord : ce que les livres appellent les **littéraux**. Une `lfunc` en fait partie : c'est une fonction écrite au milieu d'une expression, une valeur comme une autre. Puis les expressions, tout ce qui calcule quelque chose à partir d'autre chose. Remarque que `AST_STRUCT` n'est pas la définition d'une structure mais `new A(b)` : le nom vient de ce que l'expression *fabrique*. La suite :

```c fichier="src/include/ast.h" lignes="41-58"
        // Instructions
        AST_VARIABLE_DEFINITION,
        AST_FUNCTION_DEFINITION,
        AST_STRUCT_DEFINITION,
        AST_ENUM_DEFINITION,     // chaque élément est un AST_NUMBER ou un AST_STRING, nommé par variable_name
        AST_ASSIGNEMENT,
        AST_RETURN,
        AST_BREAK,
        AST_CONTINUE,
        AST_IF,
        AST_MATCH,               // if_expr : le sujet, list_compound : les AST_CASE, else_body
        AST_CASE,                // list_compound : les valeurs, if_body : le bloc
        AST_WHILE,
        AST_FOR,
        AST_FOR_IN,              // for var a, b in c : for_init, for_condition (c), for_body
        AST_INCLUDE,
        AST_EXPORT,
    }ast_type;
```

Les instructions. `AST_INCLUDE` est le `from ... import`, et `ASSIGNEMENT` garde l'orthographe historique du dépôt.

Il y a une chose qu'on ne voit nulle part : pas d'`AST_ELSE_IF`, pas d'`AST_COMPOUND_ASSIGN` pour `+=`, pas d'`AST_MAP_SHORTHAND` pour `{ age }`, pas de ternaire sans `else`. Le parser les a tous ramenés à des formes plus simples : un `if` dans un `if`, une affectation avec un opérateur, une clé suivie d'une variable, un ternaire complété par un `AST_NONE`. On appelle ça le **désucrage** (*desugaring*), parce que ces écritures sont du « sucre syntaxique », une facilité pour celui qui écrit qui ne change rien au sens. Chaque sucre retiré par le parser, c'est un cas de moins pour le visitor, pour le compilateur, et pour toi quand tu les écriras.

Le programme `arbre` du chapitre 3 permet de voir ce désucrage à l'oeuvre :

```terminal
$ cat livre/essais/chapitres-01-04/sucre.cf
x += 1
var t = n > 0 ? "oui"
if a { } else if b { }
$ livre/essais/chapitres-01-04/arbre livre/essais/chapitres-01-04/sucre.cf
  1:1   COMPOUND
  1:3     ASSIGNEMENT +
  1:1       cible: VARIABLE x
  1:6       valeur: NUMBER 1
  2:5     VARIABLE_DEFINITION t
  2:15      valeur: TERNARY
  2:11        si: BINOP >
  2:9           gauche: VARIABLE n
  2:13          droite: NUMBER 0
  2:17        alors: STRING "oui"
  3:1         sinon: NONE
  3:1     IF
  3:4       si: VARIABLE a
  3:8       alors: COMPOUND
  3:15      sinon si: IF
  3:18        si: VARIABLE b
  3:22        alors: COMPOUND
```

Trois écritures, et aucune sorte de noeud nouvelle. Regarde la position du `NONE` inventé : `3:1`, le début de la ligne suivante. Le parser l'a créé avec `parser_new_node`, qui prend la position du jeton courant, et le jeton courant était déjà le `if` de la ligne 3. Ce noeud ne produira jamais d'erreur, donc ce n'est pas grave. Mais c'est un bon rappel : un noeud que le parser invente a la position qu'il trouve sous la main, pas forcément celle qu'on voudrait.

`AST_NOOP`, enfin, ne sort jamais du parser. Il ne fait rien. On le retrouvera à la section 4.4, où il sert de support à autre chose qu'un programme.

## 4.2 Un seul struct pour tous les noeuds

Voici le noeud lui-même. On le lit en cinq morceaux. D'abord ce que tous les noeuds ont en commun :

```c fichier="src/include/ast.h" lignes="60-73"
    typedef struct AST_STRUCT{

        ast_type type;

        /* ======  position, pour les erreurs ====== */
            const char *file;
            int line;
            int colone;

        /* ======  AST_VARIABLE_DEFINITION ====== */
            char *variable_definition_name;
            struct AST_STRUCT *variable_definition_value;
            char *variable_definition_static_type_name;
            bool is_const;
```

La sorte, et la position dans le fichier. Puis, tout de suite, les champs d'une seule sorte de noeud : `var x::int = 3` range `x` dans `variable_definition_name`, `int` dans `variable_definition_static_type_name`, et le noeud du `3` dans `variable_definition_value`. Ces champs-là existent dans *tous* les noeuds, y compris un `AST_NUMBER` qui n'en fera jamais rien.

```c fichier="src/include/ast.h" lignes="75-95"
        /* ======  AST_VARIABLE, AST_OBJ_ACCESS ====== */
            char *variable_name;
            uint32_t variable_hash;     // AST_VARIABLE : calculé à la première lecture

        /* ======  AST_FUNCTION_DEFINITION, AST_LFUNC ====== */
            char *function_definition_name;
            list_t *function_definition_args;
            struct AST_STRUCT *function_definition_body;
            char *function_definition_return_type_name;
            bool is_define_self;

        /* ======  AST_FUNCTION_CALL, AST_STRUCT, AST_TYPE_CALL ====== */
            char *function_call_name;
            list_t *function_call_args;

        /* ======  valeurs écrites dans le code ====== */
            double number_value;
            bool bool_value;
            char char_value;
            char *string_value;
            int string_lenght;
```

Chaque groupe est annoncé par un commentaire qui dit quelles sortes de noeuds s'en servent. `variable_hash` porte un commentaire étrange, « calculé à la première lecture » : c'est le seul champ qui change après le parser, et on y reviendra à la section 4.6.

```c fichier="src/include/ast.h" lignes="97-123"
        /* ======  binop, unop, accès, appel ====== */
            struct AST_STRUCT *left;
            struct AST_STRUCT *right;
            TokenEnum op;

        /* ======  return ====== */
            struct AST_STRUCT *return_expr;

        /* ======  if, ternary, match, case ====== */
            struct AST_STRUCT *if_expr;
            struct AST_STRUCT *if_body;
            struct AST_STRUCT *if_other;
            struct AST_STRUCT *else_body;

        /* ======  while ====== */
            struct AST_STRUCT *while_expr;
            struct AST_STRUCT *while_body;

        /* ======  for ====== */
            struct AST_STRUCT *for_init;
            struct AST_STRUCT *for_condition;
            struct AST_STRUCT *for_solve;
            struct AST_STRUCT *for_body;

        /* ======  assignement ====== */
            struct AST_STRUCT *assignement_left;
            struct AST_STRUCT *assignement_value;
```

Les enfants. `op` est un `TokenEnum` : le parser a rangé le type du jeton de l'opérateur, `TOKEN_PLUS` ou `TOKEN_AND`, sans inventer une autre liste d'opérateurs. Le visitor et le compilateur feront un `switch` sur ces jetons.

```c fichier="src/include/ast.h" lignes="125-143"
        /* ======  list_access ====== */
            struct AST_STRUCT *list_ptr;

        /* ======  struct definition, enum definition ====== */
            char *struct_definition_name;
            list_t *struct_definition_item;

        /* ======  include ====== */
            bool is_dll;
            bool is_import_all;

        /* ======  compound, list, include, export ====== */
            list_t *list_compound;
            bool has_definition;   // le bloc déclare un nom : il lui faut sa propre porté

        /* ======  tous les noeuds, pour les libérer à la fin ====== */
            struct AST_STRUCT *next;

    }ast_t;
```

Et le dernier champ, `next`, qui n'appartient à aucune sorte de noeud en particulier : il relie tous les noeuds entre eux, dans l'ordre inverse de leur création. C'est lui qui permettra de tout libérer (section 4.4).

Quarante-six champs, tous présents dans chaque noeud. Un `AST_BREAK`, qui n'a besoin que de sa sorte et de sa position, porte quand même trente-deux pointeurs, dont au moins trente valent `NULL`.

### Pourquoi ce choix

La façon « propre » d'écrire un arbre en C, c'est une étiquette suivie d'une union : chaque sorte de noeud a ses propres champs, et ils se partagent la même place en mémoire. Pour en mesurer le gain, j'en ai écrit une esquisse dans `livre/essais/chapitres-01-04/union.c` :

```c fichier="livre/essais/chapitres-01-04/union.c"
typedef struct NODE{
    int type;
    const char *file;
    int line;
    int colone;
    struct NODE *next;
    union{
        double number;
        struct{ char *text; int lenght; } string;
        struct{ char *name; } variable;
        struct{ struct NODE *left, *right; TokenEnum op; } binop;
        struct{ struct NODE *condition, *body, *other, *otherwise; } branch;
        struct{ struct NODE *init, *condition, *step, *body; } loop;
        struct{ char *name, *return_type; list_t *args; struct NODE *body; bool self; } function;
        struct{ char *name, *type; struct NODE *value, *next; bool is_const; } definition;
        struct{ list_t *items; bool has_definition; } block;
    } as;
}node_t;
```

Ce n'est pas un arbre complet pour CeFlo (il manque des cas), mais il a la bonne taille. Une union fait la taille de son plus gros membre, ici la fonction, avec ses quatre pointeurs et son booléen :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -o livre/essais/chapitres-01-04/union livre/essais/chapitres-01-04/union.c
$ livre/essais/chapitres-01-04/union
avec une union : 72 octets
```

72 octets au lieu de 344 : presque cinq fois moins. C'est ce que fait CPython. Son arbre est décrit dans un petit langage, ASDL (`Parser/Python.asdl`), et un script en tire des structures C avec un champ `kind` et une union `v`. Clang, le compilateur C de LLVM, écrit une classe C++ par sorte de noeud. Le jlox de *Crafting Interpreters*, en Java, génère une classe par sorte avec un petit programme. Et Lua... n'a pas d'arbre du tout. Son parser écrit le bytecode au fur et à mesure qu'il lit, en une seule passe. C'est la solution la plus économe, et la plus difficile à lire et à modifier.

CeFlo a choisi l'autre bout. Ce qu'il y gagne, tu l'as déjà utilisé sans y penser :

- **N'importe quel champ se lit sans danger.** Dans une union, lire `as.binop.left` sur un noeud qui est en fait un nombre donne n'importe quoi. Ici, ça donne `NULL`, grâce à `calloc`. Le programme `arbre` du chapitre 3 testait `variable_definition_static_type_name` sur *tous* les noeuds, sans regarder leur sorte, et c'est ce qui lui permettait d'afficher le type d'un paramètre comme celui d'une variable.
- **Un champ peut servir à plusieurs sortes sans rien déclarer.** Le paramètre d'une fonction est un `AST_VARIABLE`, mais il range son type dans `variable_definition_static_type_name`, un champ des définitions. Avec une union, il aurait fallu ajouter un membre.
- **On peut parcourir l'arbre sans connaître les sortes.** C'est l'objet de la suite.

### Un parcours qui ne regarde pas la sorte

Le programme qui a compté les noeuds du début du chapitre n'a pas de `switch`. Il suit tous les pointeurs vers des enfants, quelle que soit la sorte du noeud :

```c fichier="livre/essais/chapitres-01-04/compte.c"
// Tous les noeuds ont les mêmes champs : on suit chaque pointeur, sans regarder la sorte.
static int count_node(ast_t *node){
    if(node == NULL)
        return 0;

    ast_t *children[] = {
        node->variable_definition_value, node->function_definition_body,
        node->left, node->right, node->return_expr,
        node->if_expr, node->if_body, node->if_other, node->else_body,
        node->while_expr, node->while_body,
        node->for_init, node->for_condition, node->for_solve, node->for_body,
        node->assignement_left, node->assignement_value, node->list_ptr,
    };

    int total = 1;
    for(size_t i = 0; i < sizeof(children) / sizeof(children[0]); i++)
        total += count_node(children[i]);
    total += count_list(node->function_definition_args);
    total += count_list(node->function_call_args);
    total += count_list(node->struct_definition_item);
    total += count_list(node->list_compound);
    return total;
}
```

Dix-huit pointeurs et quatre listes : c'est tout ce qu'un noeud peut avoir comme enfants. Les champs inutilisés valent `NULL`, et `count_node(NULL)` rend 0. Avec une union, ce programme serait impossible : il faudrait savoir, pour chaque sorte, quels membres sont des enfants. Ici, il tient en vingt lignes et ne cassera pas le jour où on ajoutera une sorte de noeud. Le reste du programme :

```c fichier="livre/essais/chapitres-01-04/compte.c"
static int count_list(list_t *list){
    int total = 0;
    for(int i = 0; list != NULL && i < list->Size; i++)
        total += count_node(List_Get(list,i));
    return total;
}
```

```c fichier="livre/essais/chapitres-01-04/compte.c"
int main(int argc, char **argv){
    ceflo_stack_init();
    printf("un noeud : %zu octets\n",sizeof(ast_t));

    for(int i = 1; i < argc; i++){
        lexer_t lexer;
        if(!newLexer(&lexer,argv[i])){
            fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[i]);
            return 1;
        }
        size_t text = lexer.lenght_file_code;
        int nodes = count_node(parser_parse(&lexer));
        printf("%-28s %6zu octets de texte, %5d noeuds, %8zu octets d'arbre\n",
            argv[i],text,nodes,(size_t)nodes * sizeof(ast_t));
    }

    clearAst();
    error_free_sources();
    return 0;
}
```

Avec la déclaration en avance de `count_node` et les mêmes `#include` qu'`arbre.c`, il se compile comme lui. Il lit plusieurs fichiers à la suite, et ne libère tout qu'à la fin : on verra que c'est la façon de faire de CeFlo.

### Ce que ça coûte

Le prix, on l'a vu : 344 octets par noeud. Pour un langage de script, dont les programmes font rarement plus de quelques milliers de lignes, c'est quelques mégaoctets au pire. Le visitor alloue bien plus que ça pour ses valeurs dès la première boucle. Le coût n'est pas là.

Le vrai coût, c'est que le compilateur C ne t'aide plus. Rien n'empêche d'écrire `node->while_body` sur un noeud `AST_FOR` : ça compile, ça rend `NULL`, et le programme plante ailleurs, ou pire, ne fait rien sans rien dire. Avec une union et des noms par sorte, l'erreur sauterait aux yeux en relisant (`node->as.loop.body` sur un `while`), même si le C ne la détecterait pas non plus. Avec les classes de Clang, elle ne compilerait pas.

La seule défense, ce sont les commentaires de `ast.h`, et la discipline de les relire. La section suivante en fait le tour.

## 4.3 Les champs qui changent de sens

Au chapitre 3, plusieurs sortes de noeuds ont emprunté les champs d'autres sortes. Voici la liste complète, relevée dans `src/parser.c` :

| Champ | Sens principal | Aussi utilisé par |
| --- | --- | --- |
| `left` | opérande gauche (`BINOP`, accès, appel) | la définition suivante dans `var a, b` |
| `if_expr` | condition d'un `IF` | condition d'un `TERNARY`, sujet d'un `MATCH` |
| `if_body` | bloc d'un `IF` | bloc d'un `CASE` |
| `if_other` | le `else if` d'un `IF` | le « alors » d'un `TERNARY` |
| `else_body` | le `else` d'un `IF` | le « sinon » d'un `TERNARY`, le `else` d'un `MATCH` |
| `for_condition` | condition d'un `FOR` | ce qu'on parcourt dans un `FOR_IN` |
| `list_compound` | instructions d'un `COMPOUND` | éléments d'une `LIST`, clés et valeurs d'un `MAP`, `CASE` d'un `MATCH`, valeurs d'un `CASE`, noms d'un `INCLUDE` ou d'un `EXPORT` |
| `variable_name` | nom d'une `VARIABLE`, d'un attribut | chemin d'un `INCLUDE`, nom d'un élément d'énumération |
| `variable_definition_static_type_name` | type d'une variable | type d'un paramètre |

Le piège le plus sournois est le ternaire. Pour un `if`, `if_other` est un autre `if` facultatif. Pour un ternaire, c'est la valeur quand la condition est vraie, et il ne vaut jamais `NULL`. Le programme `arbre` du chapitre 3 a dû le traiter à part :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
            print_node("alors",node->type == AST_IF ? node->if_body : node->if_other,depth);
```

Pourquoi ne pas avoir donné au ternaire son propre champ ? Parce que chaque champ ajouté pèse huit octets de plus dans *tous* les noeuds. Avec un seul `struct`, réutiliser un champ est la seule façon de ne pas grossir. C'est la contrepartie directe du choix de la section 4.2, et elle se règle par une seule habitude : avant de lire un champ, relis le commentaire de la sorte dans `ast.h`. Pour ceux qui n'en ont pas (le ternaire), la fonction du parser qui fabrique le noeud fait foi.

> [!attention]
> Une variable chaînée par `left` est un vrai piège pour qui parcourt l'arbre. Un `var a = 1, b = 2` n'est qu'*un* élément dans la liste du bloc : le noeud de `a`. Celui de `b` est dans `a->left`. Un visitor qui oublie de suivre `left` déclarera `a` et jamais `b`, sans aucune erreur. Le chapitre 7 suit cette chaîne explicitement.

## 4.4 Allouer, et tout libérer d'un coup

Tout `src/ast.c` tient en soixante lignes. Si tu veux l'écrire toi-même, fais-le dans `livre/essais/chapitres-01-04/ast.c`, et compile tes essais avec lui au lieu de celui du dépôt. Il commence par la création :

```c fichier="src/ast.c" lignes="12-28" focus="17-18"
static ast_t *first_ast;

ast_t *newAst(ast_type type){
    ast_t *node = ceflo_alloc(sizeof(*node));
    node->type = type;
    node->next = first_ast;
    first_ast = node;
    return node;
}

ast_t *newAstWithLine(ast_type type, const char *file, int line, int colone){
    ast_t *node = newAst(type);
    node->file = file;
    node->line = line;
    node->colone = colone;
    return node;
}
```

`ceflo_alloc` donne un bloc rempli de zéros : les quarante-quatre champs qu'on ne touche pas sont à `NULL`, `0` ou `false`, ce qui est exactement la valeur « absent » de chacun. Puis les lignes 17 et 18 accrochent le noeud en tête d'une liste chaînée de *tous* les noeuds jamais créés, à travers `next`. Cette liste ne suit pas la forme de l'arbre : elle suit l'ordre de création, à l'envers.

Pour `var x = 1 + 2`, le parser crée cinq noeuds, dans cet ordre : le bloc, la définition, le `1`, le `+` (créé seulement quand on voit l'opérateur, après avoir lu son opérande gauche), puis le `2`. L'arbre et la chaîne ne se ressemblent pas du tout :

```text
l'arbre (les enfants)                 la chaîne (next), depuis first_ast

COMPOUND                              first_ast
  └─ VARIABLE_DEFINITION x               │
       └─ BINOP +                        ▼
            ├─ NUMBER 1               NUMBER 2 ─▶ BINOP + ─▶ NUMBER 1
            └─ NUMBER 2                  ─▶ VARIABLE_DEFINITION ─▶ COMPOUND ─▶ NULL
```

Chaque noeud est dans les deux structures à la fois. L'arbre sert à exécuter, la chaîne ne sert qu'à libérer.

`newAstWithLine` ajoute la position. Le parser n'appelle jamais `newAst` directement : il passe par `parser_new_node`, qui donne la position du jeton courant.

La libération :

```c fichier="src/ast.c" lignes="30-52"
static void freeAst(ast_t *node){
    free(node->variable_definition_name);
    free(node->variable_definition_static_type_name);
    free(node->variable_name);
    free(node->function_definition_name);
    free(node->function_definition_return_type_name);
    free(node->function_call_name);
    free(node->string_value);
    free(node->struct_definition_name);
    List_Free(node->function_definition_args);
    List_Free(node->function_call_args);
    List_Free(node->struct_definition_item);
    List_Free(node->list_compound);
    free(node);
}

void clearAst(void){
    while(first_ast != NULL){
        ast_t *next = first_ast->next;
        freeAst(first_ast);
        first_ast = next;
    }
}
```

`freeAst` libère ce que le noeud *possède* : ses chaînes (toutes copiées par `parser_eat_id` ou `allowChar`) et ses listes. Il ne libère pas ses enfants, et c'est le point important. `List_Free` libère le tableau de pointeurs de la liste, pas les noeuds pointés : ceux-là seront libérés par leur propre passage dans la boucle de `clearAst`. Chaque noeud est dans la chaîne une fois, il est donc libéré une fois. Et `free(NULL)` ne faisant rien, on appelle `free` sur tous les champs sans regarder la sorte.

### Pourquoi pas une libération récursive ?

La façon naturelle de libérer un arbre est de partir de la racine et de descendre : libérer les enfants, puis le noeud. On aurait pu l'écrire comme `count_node`. Il y a trois raisons de ne pas le faire.

La première, c'est qu'il y a des noeuds qui ne sont dans aucun arbre. On peut le voir en comptant les noeuds alloués, plutôt que ceux qu'on retrouve depuis la racine. L'éditeur de liens GNU a une option pour ça : `-Wl,--wrap=ceflo_alloc` fait passer chaque appel à `ceflo_alloc` venu d'un autre fichier par une fonction `__wrap_ceflo_alloc` qu'on écrit nous-mêmes, et qui peut appeler la vraie sous le nom `__real_ceflo_alloc` :

```c fichier="livre/essais/chapitres-01-04/allocs.c"
void *__real_ceflo_alloc(size_t size);

static int allocated;

void *__wrap_ceflo_alloc(size_t size){
    if(size == sizeof(ast_t))
        allocated += 1;
    return __real_ceflo_alloc(size);
}

static void print_allocated(void){
    printf("noeuds alloués : %d\n",allocated);
}

__attribute__((constructor)) static void start(void){
    atexit(print_allocated);
}
```

Le fichier commence par les `#include` de `<stdio.h>`, `<stdlib.h>` et `"ast.h"`. L'attribut `constructor` fait appeler `start` avant `main` : on n'a pas à toucher au programme `compte`. On compte tout bloc de la taille d'un noeud, ce qui suffit ici : rien d'autre dans l'avant du langage n'a cette taille.

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -o livre/essais/chapitres-01-04/compte_allocs livre/essais/chapitres-01-04/compte.c livre/essais/chapitres-01-04/allocs.c src/parser.c src/ast.c src/lexer.c src/token.c src/error.c src/str.c src/list.c src/common.c -Wl,--wrap=ceflo_alloc
$ livre/essais/chapitres-01-04/compte_allocs tests/structures.cf
un noeud : 344 octets
tests/structures.cf            3143 octets de texte,   479 noeuds,   164776 octets d'arbre
noeuds alloués : 485
```

Six noeuds de plus que ceux de l'arbre. Où sont-ils ? Relis `parser_parse_function_args` au chapitre 3 : chaque paramètre est un noeud créé *avant* de savoir s'il s'appelle `self`. Un `self` n'est pas rangé dans la liste des paramètres, et son noeud reste orphelin. `tests/structures.cf` déclare exactement six méthodes avec `self` :

```terminal
$ grep -nE "\(self" tests/structures.cf
19:    func __init__(self){ self.nom = "Inconnu"; self.arme = none }
20:    func __init__(self, nom){
25:    func __init__(self, nom, arme::Arme){ self.nom = nom; self.arme = arme }
26:    func __str__(self){ return self.nom }
27:    func Attaque(self, cible::Perso){
72:struct T { var n = 1; func m(self){ return g + self.n } }
```

Une libération récursive ne les trouverait jamais : six fuites par fichier. Avec la chaîne de `next`, ils sont libérés comme les autres, sans que personne ait eu à y penser.

La deuxième raison, ce sont les noeuds `AST_NOOP`. Le visitor et `format()` gardent des tables dont les clés sont des chaînes qui doivent vivre jusqu'à la fin du programme (le nom d'un module déjà chargé, le texte d'un `${...}` déjà analysé). Plutôt que d'écrire un autre mécanisme pour les libérer, on les range dans un noeud vide :

```c fichier="src/bulletin.c" lignes="103-106"
    // La clé doit vivre aussi longtemps que le cache : un noeud de l'AST la garde.
    ast_t *holder = newAst(AST_NOOP);
    allowChar(&holder->variable_name,expr);
    Hash_Set(&format_cache,holder->variable_name,node);
```

Le noeud n'est dans aucun arbre, mais il est dans la chaîne, et `clearAst` libérera sa chaîne. C'est un usage détourné, et il est très pratique.

La troisième raison, ce sont les erreurs. Une erreur de syntaxe arrête le programme au milieu de la lecture, avec un arbre à moitié construit : un noeud créé, pas encore rangé chez son parent. Une libération récursive depuis une racine qui n'existe pas encore ne libérerait rien. La chaîne, elle, contient tout ce qui a été créé jusqu'à l'erreur. Les deux `main` du dépôt appellent `clearAst` depuis une fonction enregistrée avec `atexit`, qui tourne même après le `exit(1)` d'une erreur :

```c fichier="src/vm/main.c" lignes="24-30"
// Appelée à chaque sortie du programme, même après une erreur ou un exit() du script.
static void ceflo_clean(void){
    fflush(stdout);
    vm_free(&vm);
    clearAst();
    error_free_sources();
}
```

valgrind le confirme sur un fichier qui s'arrête au milieu d'une expression :

```terminal
$ cat livre/essais/chapitres-01-04/coupe.cf
println("un")
var x = 1 +
$ valgrind ./Ceflo livre/essais/chapitres-01-04/coupe.cf 2>&1 | grep -E "erreur|in use at exit|All heap"
erreur de syntaxe: `expression` attendu, mais `fin de fichier` trouvé
==449274==     in use at exit: 0 bytes in 0 blocks
==449274== All heap blocks were freed -- no leaks are possible
```

Pas un octet perdu, alors que le parser a été interrompu en pleine construction. (Et remarque que `un` ne s'affiche pas : on y revient à la section 4.5.)

> [!note]
> Cette chaîne, c'est une version simple de ce que les compilateurs appellent une **arène** (*arena* ou *region*) : on alloue beaucoup de petits objets qui vivent aussi longtemps les uns que les autres, et on les libère tous ensemble. Clang range ses noeuds dans un `BumpPtrAllocator`, rustc dans des arènes typées. Une vraie arène alloue de gros blocs et découpe dedans ; CeFlo paie un `calloc` par noeud, mais n'a qu'un pointeur à suivre pour tout libérer.

### La dernière fonction

```c fichier="src/ast.c" lignes="54-61" fonction="GetFunctionInStruct"
ast_t *GetFunctionInStruct(ast_t *structure, const char *name){
    for(int i = 0; i < structure->struct_definition_item->Size; i++){
        ast_t *item = List_Get(structure->struct_definition_item,i);
        if(item->type == AST_FUNCTION_DEFINITION && strcmp(name,item->function_definition_name) == 0)
            return item;
    }
    return NULL;
}
```

Elle cherche une méthode par son nom dans une structure. Le visitor s'en sert pour trouver `__cast__` (dans `src/typer.c`). C'est la seule fonction de `ast.c` qui lit l'arbre au lieu de le construire ou de le détruire.

Si tu as écrit ta copie, compare-la, puis compile `compte` avec elle :

```terminal
$ diff livre/essais/chapitres-01-04/ast.c src/ast.c
$ gcc -std=c11 -Wall -Wextra -Isrc -Isrc/include -o livre/essais/chapitres-01-04/compte livre/essais/chapitres-01-04/compte.c src/parser.c livre/essais/chapitres-01-04/ast.c src/lexer.c src/token.c src/error.c src/str.c src/list.c src/common.c
$ livre/essais/chapitres-01-04/compte livre/essais/chapitres-01-04/fib.cf
un noeud : 344 octets
livre/essais/chapitres-01-04/fib.cf             94 octets de texte,    28 noeuds,     9632 octets d'arbre
```

## 4.5 Ce que l'arbre garde pour les erreurs

Chaque noeud porte trois champs qui ne servent qu'aux messages d'erreur : `file`, `line`, `colone`. Seize octets par noeud, pour une chose qui n'arrive presque jamais. C'est pourtant la meilleure dépense de toute la structure.

Une erreur à l'exécution ressemble à une erreur de syntaxe, parce qu'elle passe par la même fonction :

```c fichier="src/error.c" lignes="98-107" fonction="error_node_v"
void error_node_v(const ast_t *node, const char *format, va_list args){
    error_report(
        "erreur",
        node != NULL ? node->file : NULL,
        node != NULL ? node->line : 0,
        node != NULL ? node->colone : 0,
        format,
        args
    );
}
```

Le visitor appelle `visitor_error(node, ...)` avec le noeud qu'il est en train d'exécuter, et la machine virtuelle fait pareil à travers `error_node_v`. Le noeud donne le fichier, la ligne et la colonne, et `error_report` du chapitre 1 fait le reste. `node->file` est le pointeur stable rendu par `error_add_source` : c'est pour ça que `error_find_line` pouvait comparer des adresses au lieu de chaînes. Tous les noeuds d'un même fichier partagent la même chaîne, et le nom n'est stocké qu'une fois.

Ce qu'une erreur montre dépend donc de la position que le parser a donnée au noeud. On a fait ces choix un par un au chapitre 3, et les voilà rassemblés :

| Noeud | Position | Créé par |
| --- | --- | --- |
| `BINOP` | l'opérateur | `parser_new_binop`, appelé sur l'opérateur |
| `OBJ_ACCESS` | le nom après le point | corrigée aux lignes 295 et 296 |
| `LIST_ACCESS` | le `[` | `parser_new_node` avant d'avancer |
| `FUNCTION_CALL` | le `(` | `parser_new_node` avant les arguments |
| `VARIABLE_DEFINITION` | le nom, pas le `var` | créé après avoir avancé sur `var` |
| `IF`, `WHILE`, `FOR` | le mot-clé | créé avant d'avancer |
| `COMPOUND` | la première instruction du bloc | créé après la `{` |

Et voici ce que ça donne à l'exécution :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/tard.cf
début
erreur: division par zéro
  --> livre/essais/chapitres-01-04/tard.cf:2:12
    2 | println(10 / 0)
      |            ^
$ ./Ceflo livre/essais/chapitres-01-04/pos_appel2.cf
erreur: number n'est pas une fonction
  --> livre/essais/chapitres-01-04/pos_appel2.cf:2:2
    2 | t(1)
      |  ^
```

La division pointe le `/`, ce qui est parfait. L'appel pointe la parenthèse, ce qui est moins naturel : on aurait préféré voir la flèche sous `t`. C'est un défi de la fin du chapitre, et il a une conséquence intéressante : changer une position dans le parser change le message des *deux* interpréteurs, puisqu'ils partagent l'arbre.

```terminal
$ ./Ceflo_ref livre/essais/chapitres-01-04/pos_appel2.cf
erreur: number n'est pas une fonction
  --> livre/essais/chapitres-01-04/pos_appel2.cf:2:2
    2 | t(1)
      |  ^
```

### Après le bytecode

On pourrait croire que la machine virtuelle, qui n'exécute plus l'arbre mais du bytecode, n'a plus besoin des noeuds. Elle les garde quand même, un par octet de code :

```c fichier="src/vm/chunk.h" lignes="144-153" focus="148"
    typedef struct{
        int count;
        int capacity;
        uint8_t *code;
        ast_t **nodes;          // le noeud de chaque octet de code
        ValueArray constants;
        TypeCheck *checks;
        int check_count;
        int check_capacity;
    }Chunk;
```

Quand une instruction échoue, la VM retrouve le noeud qui l'a produite, et affiche le même message, au même endroit, que le visitor. C'est ce qui permet à `make diff` de comparer les erreurs des deux interpréteurs octet par octet. On verra au chapitre 9 comment ce tableau est rempli, et ce qu'il coûte.

> [!note]
> CPython fait un autre choix. Une fois le bytecode produit, il jette son arbre, et ne garde qu'une table compacte qui associe chaque instruction à sa ligne et sa colonne (`co_linetable`). C'est plus économe ; CeFlo garde l'arbre parce qu'il en a besoin pour autre chose, comme on va le voir.

## 4.6 Les vérifications faites dès la lecture

Regarde ces deux programmes, et ce qu'ils affichent :

```terminal
$ cat livre/essais/chapitres-01-04/tard.cf
println("début")
println(10 / 0)
$ ./Ceflo livre/essais/chapitres-01-04/tard.cf
début
erreur: division par zéro
  --> livre/essais/chapitres-01-04/tard.cf:2:12
    2 | println(10 / 0)
      |            ^
$ cat livre/essais/chapitres-01-04/tot.cf
println("début")
func arrete() {
    break
}
$ ./Ceflo livre/essais/chapitres-01-04/tot.cf
erreur de syntaxe: `break` et `continue` s'utilisent dans une boucle
  --> livre/essais/chapitres-01-04/tot.cf:3:5
    3 |     break
      |     ^
```

Le premier affiche `début`, puis échoue. Le second échoue sans rien afficher, alors que son erreur est à la ligne 3, et que la fonction `arrete` n'est même jamais appelée. C'est que l'erreur du second est une erreur de *lecture* : le parser l'a trouvée avant qu'une seule instruction ne s'exécute. La division par zéro, elle, ne peut se voir qu'en calculant.

Toutes les erreurs qu'on peut trouver en lisant, il vaut mieux les trouver en lisant. Le programme n'a encore rien fait : il n'a pas écrit la moitié d'un fichier, il n'a pas envoyé la moitié d'une commande. Et l'erreur sera trouvée même si la ligne fautive est dans une branche qui ne s'exécute qu'un jour sur mille.

Le parser de CeFlo en vérifie une dizaine. Chacune a son test dans `tests/erreurs/` :

| Vérification | Où | Test |
| --- | --- | --- |
| `break` et `continue` dans une boucle de la même fonction | `parser_parse_loop_jump` | `break_global.cf`, `break_fonction.cf` |
| `return` dans une fonction | `parser_parse_return` | `return_global.cf` |
| `export` hors d'un bloc | `parser_parse_export` | `export_bloc.cf` |
| une `const` reçoit une valeur | `parser_parse_variable_definition` | `const_sans_valeur.cf` |
| `self` déclaré une seule fois | `parser_parse_function_args` | `self_double.cf` |
| une méthode définie une fois (sauf `__init__`) | `parser_check_struct_method` | `methode_double.cf` |
| deux noms ou deux valeurs identiques dans une énumération | `parser_check_enum_item` | `enum_doublon.cf`, `enum_meme_valeur.cf` |
| une valeur testée une fois dans un `match` | `parser_check_case_value` | `match_doublon_nombre.cf` et six autres |
| `else` après le dernier `case` | `parser_parse_match` | `match_else_avant.cf` |

On a vu au chapitre 3 comment `loop_depth` est remis à zéro dans une fonction. Le cas qui le justifie mérite d'être lancé :

```terminal
$ cat livre/essais/chapitres-01-04/break_lfunc.cf
while true {
    var f = lfunc() { break }
}
$ ./Ceflo livre/essais/chapitres-01-04/break_lfunc.cf
erreur de syntaxe: `break` et `continue` s'utilisent dans une boucle
  --> livre/essais/chapitres-01-04/break_lfunc.cf:2:23
    2 |     var f = lfunc() { break }
      |                       ^
```

### Les doublons d'un match

La vérification la plus fine est celle des `case`. Deux valeurs écrites à l'identique dans un même `match` sont une erreur, parce que le second `case` ne pourrait jamais servir :

```c fichier="src/parser.c" lignes="715-732" fonction="parser_same_case_value"
// Deux valeurs écrites à l'identique : le deuxième `case` ne pourrait jamais servir.
static bool parser_same_case_value(ast_t *a, ast_t *b){
    if(a->type != b->type)
        return false;

    switch(a->type){
        case AST_NONE: return true;
        case AST_BOOL: return a->bool_value == b->bool_value;
        case AST_CHAR: return a->char_value == b->char_value;
        case AST_NUMBER:
        case AST_STRING: return parser_enum_same_value(a,b);
        case AST_OBJ_ACCESS:
            return a->left->type == AST_VARIABLE && b->left->type == AST_VARIABLE &&
                strcmp(a->left->variable_name,b->left->variable_name) == 0 &&
                strcmp(a->variable_name,b->variable_name) == 0;
        default: return false;
    }
}
```

On compare des *noeuds*, pas des valeurs. Le parser ne calcule rien : il ne sait comparer que ce qui est écrit tel quel. Deux nombres, deux textes, deux caractères, `true`, `false`, `none`, et `Nom.ELEMENT` (un accès dont la gauche est une simple variable). Pour tout le reste, une variable, un calcul, un appel, il répond « pas pareil », même quand ce l'est. C'est la règle de prudence de toute vérification faite à la lecture : ne refuser que ce qui est *sûrement* faux.

`1` et `1.0` donnent le même `number_value`, et sont donc vus comme des doublons. En revanche, `'A'` et `65` sont deux sortes de noeuds différentes, et passent :

```terminal
$ cat livre/essais/chapitres-01-04/case_65.cf
match 65 {
    case 'A' { println("lettre") }
    case 65 { println("nombre") }
}
$ ./Ceflo livre/essais/chapitres-01-04/case_65.cf
lettre
```

Le second `case` ne servira jamais, puisque `'A' == 65` est vrai en CeFlo. Le parser aurait pu le savoir, mais il lui faudrait connaître les règles d'égalité du chapitre 5. Il s'arrête à ce qu'il peut décider seul.

La fonction qui applique la vérification compare chaque nouvelle valeur à celles de tous les `case` précédents :

```c fichier="src/parser.c" lignes="734-742" fonction="parser_check_case_value"
static void parser_check_case_value(parser_t *parser, list_t *values, ast_t *value){
    for(int i = 0; i < values->Size; i++){
        ast_t *other = List_Get(values,i);
        if(parser_same_case_value(other,value)){
            error_syntax(parser->lexer->fileName,value->line,value->colone,
                "cette valeur est déjà testée à la ligne %d",other->line);
        }
    }
}
```

Le message cite la ligne de la première occurrence : c'est la position gardée dans le noeud qui le permet. Pour `n` valeurs, on fait de l'ordre de `n²` comparaisons. Un `match` de mille `case` en ferait un demi-million, ce qui reste instantané ; personne n'écrit un `match` de mille `case`.

### Ce que le parser ne vérifie pas

Une variable qui n'existe pas n'est *pas* une erreur de lecture :

```terminal
$ cat livre/essais/chapitres-01-04/jamais.cf
println("début")
if false {
    println(pas_definie)
}
println("fin")
$ ./Ceflo livre/essais/chapitres-01-04/jamais.cf
début
fin
```

Le parser ne sait pas quels noms existent. Il pourrait le savoir pour les variables du fichier, mais pas pour les noms importés par `from outils import *` (il faudrait lire `outils.cf`), ni pour les fonctions fournies comme `println`, qu'un script a le droit de redéfinir. Python fait le même choix, et JavaScript aussi : un nom inconnu est une erreur à l'exécution, quand on l'atteint. Lua va plus loin, et rend `nil` sans erreur. Au chapitre 10, le compilateur de la VM résoudra les noms locaux à la compilation, et on verra ce qu'il fait de ceux qu'il ne trouve pas.

## 4.7 Un arbre qui ne change plus

`ast.h` le dit dans son tout premier commentaire :

```c fichier="src/include/ast.h" lignes="1-7"
/**
 * @file ast.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief l'arbre produit par le parser. Il décrit le programme et ne change
 *        jamais pendant l'exécution : les valeurs vivent dans Value.h.
 * @copyright Copyright (c) 2023
 */
```

L'arbre décrit le programme. Il ne contient pas les valeurs que le programme calcule : quand la boucle `for var i = 0; i < 3; i += 1` tourne, l'arbre ne sait pas combien vaut `i`. Il dit seulement « il y a une variable `i`, elle part de 0 ». Les valeurs, elles, vivront dans une autre structure, `value_t` (chapitre 5), qui naît et meurt pendant l'exécution.

Pourquoi cette séparation est-elle si importante ? Parce que beaucoup de choses gardent des pointeurs vers l'arbre, longtemps après le parser.

Une fonction, d'abord. Quand le visitor exécute `func fib(n) { ... }`, il ne copie pas le corps de la fonction : il fabrique une valeur qui pointe sur le noeud `AST_FUNCTION_DEFINITION`.

```c fichier="src/include/Value.h" lignes="112-115"
    value_t *makeFunction(ast_t *definition, struct SCOPE_STRUCT *closure);
    value_t *makeBuiltin(const char *name, ceflo_func_t function);
    value_t *makeStruct(ast_t *definition, struct SCOPE_STRUCT *closure);
    value_t *makeEnum(ast_t *definition);
```

Chaque appel de `fib` parcourt le même corps. Les 2 692 537 appels de `fib(30)` lisent les mêmes 20 noeuds. Si un appel modifiait l'arbre (en y rangeant la valeur de `n`, par exemple), l'appel suivant, ou l'appel du dessus dans la récursion, trouverait une valeur qui n'est pas la sienne. Une `lfunc` créée deux fois dans une boucle donne deux valeurs différentes, avec deux portées différentes, mais un seul corps : c'est ce qui rendra les closures possibles au chapitre 6.

La machine virtuelle, ensuite, garde un noeud par octet de bytecode (section 4.5). Le cache de `format()` garde l'arbre de chaque `${...}`. Les modules gardent leurs noeuds jusqu'à la fin. Tous ces pointeurs ne sont sûrs que si personne ne libère, ne déplace ou ne modifie les noeuds. D'où la règle : l'arbre est construit une fois, puis lu, puis libéré d'un coup par `clearAst`, à la toute fin.

Cette règle a une conséquence agréable pour le chapitre 8 : le garbage collector n'a pas à s'occuper de l'arbre. Il ne gère que les valeurs. L'arbre vit du début à la fin du programme, et personne n'a besoin de savoir s'il est encore utilisé.

### L'exception

Il y a un champ qui change, et le commentaire de `ast.h` l'avoue : `variable_hash`. Le visitor cherche les variables par leur nom dans des tables de hachage, et calculer le hachage d'un nom à chaque lecture coûte cher dans une boucle. Il le calcule donc une fois, à la première lecture, et le range dans le noeud :

```c fichier="src/visitor.c" lignes="113-122" focus="115-116"
// Le hash du nom ne se calcule qu'une fois par noeud, et non à chaque lecture.
static hash_t *visitor_find_variable(visitor_t *visitor, ast_t *node){
    if(node->variable_hash == 0)
        node->variable_hash = Hash_Name(node->variable_name);

    hash_t *item = scope_lookup_hashed(visitor->scope,node->variable_name,node->variable_hash);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item;
}
```

C'est une modification, mais d'un genre inoffensif : la valeur rangée dépend seulement du nom, qui ne change jamais. Tous les appels calculeraient la même. On appelle ça un **cache** ; d'un point de vue extérieur, l'arbre n'a pas changé de sens. (La valeur 0 sert de « pas encore calculé ». Un nom dont le hachage vaut vraiment 0 serait recalculé à chaque lecture, sans erreur, juste un peu plus lentement.)

`has_definition` est l'inverse : une information calculée par le parser, une fois, pour que l'exécution n'ait pas à la recalculer. Le visitor s'en sert pour ne pas créer de portée inutile :

```c fichier="src/visitor.c" lignes="60-70" focus="62-63"
// Un bloc { } : il a sa propre portée dès qu'il déclare un nom.
static value_t *visitor_visit_compound(visitor_t *visitor, ast_t *node){
    if(!node->has_definition)
        return visitor_visit_statements(visitor,node);

    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);
    visitor_visit_statements(visitor,node);
    visitor->scope = outer;
    return NULL;
}
```

Dans `tour.cf`, le corps de la boucle de `moyenne` (`total += note`) ne déclare rien : ses passages ne créent aucune portée. Le chapitre 6 dira pourquoi c'est un gain si net. On retrouve le même esprit que les vérifications de la section 4.6 : tout ce qu'on peut faire une fois, en lisant, on ne le fait pas à chaque exécution.

## Défis

1. **La flèche sous le nom.** Fais pointer l'erreur `number n'est pas une fonction` sous le `t` de `t(1)`, et plus sous la parenthèse. Une piste : dans `parser_parse_postfix`, donner au noeud `AST_FUNCTION_CALL` la position de `value`, son enfant gauche. Lance `make test` : est-ce qu'un test de `tests/erreurs/` regarde la colonne ? Lance aussi `make diff`, et explique pourquoi les deux interpréteurs restent d'accord.

2. **Un noeud plus petit.** Sans rien changer d'autre, range les champs de `ast_t` du plus grand au plus petit (pointeurs et `double`, puis `int`, puis `bool` et `char`). Recompile `compte` et mesure : on peut gagner près de cinquante octets par noeud, rien qu'en supprimant le remplissage que le compilateur insère pour aligner les champs. L'outil `pahole` (paquet `dwarves`) montre ces trous dans un binaire compilé avec `-g`.

3. **Le parser qui ne gaspille pas.** Fais en sorte qu'un paramètre `self` ne crée plus de noeud orphelin, puis vérifie avec `compte_allocs` que `tests/structures.cf` donne autant de noeuds alloués que de noeuds dans l'arbre. Il faut lire le nom avant de créer le noeud, et garder sa position à part, comme le fait `parser_parse_enum_item`.

4. **Une vraie arène.** Remplace le `calloc` de chaque noeud par un découpage dans de gros blocs de, disons, 256 noeuds, comme le fait `pool_alloc` dans `src/common.c`. Mesure le temps de `./Ceflo` sur un très gros fichier (fabrique-le avec `awk`), avant et après. Attention : `freeAst` ne doit plus appeler `free(node)`, et la chaîne de `next` n'est peut-être plus nécessaire. Qu'est-ce qui l'est encore ?

## Note de conception : la simplicité qui coûte de la mémoire

Ce chapitre a passé beaucoup de temps sur un choix qu'un professeur de compilation corrigerait en rouge : un seul `struct` de quarante-six champs pour toutes les sortes de noeuds, cinq fois plus gros que nécessaire. Je crois que c'était le bon choix pour CeFlo, et je veux dire pourquoi.

Un interpréteur se construit en couches, et chaque couche dépend de la précédente. Pendant qu'on écrit le parser, on ne sait pas encore tout ce dont le visitor aura besoin. Avec un seul `struct`, ajouter un champ est une ligne dans `ast.h` et une ligne dans `freeAst` : aucun `switch` à compléter, aucune union à réorganiser, aucun noeud existant à modifier. Un champ inutile ne coûte que de la mémoire, et `calloc` lui donne une valeur sûre. Pendant toute la croissance du langage, cette souplesse a valu bien plus que les 272 octets gaspillés par noeud.

Le prix, on l'a mesuré, et il est plus petit qu'il n'en a l'air. Un programme CeFlo de 3 000 lignes ferait quelques mégaoctets d'arbre. Le chapitre 9 montrera que ce qui rend le visitor lent, ce n'est pas la taille de ses noeuds, c'est la façon dont il les parcourt et tout ce qu'il alloue en chemin. Optimiser l'arbre aurait été optimiser la mauvaise chose.

Le vrai danger est ailleurs : dans les champs qui changent de sens. `if_other` qui veut dire deux choses, `left` qui chaîne des variables. Ce n'est pas le `struct` unique qui les a créés, c'est le refus de le faire grossir encore. Si je devais refaire CeFlo, je garderais le `struct` unique, mais je donnerais à chaque sens son propre champ, quitte à payer huit octets de plus par noeud. Un champ de plus se voit dans `ast.h`. Un champ qui change de sens ne se voit qu'en relisant le parser.

La leçon vaut au-delà de l'arbre : quand une structure de données est encore en train de naître, la rendre facile à changer passe avant la rendre compacte. On compactera quand on saura ce qu'elle doit contenir, et quand on aura mesuré que c'est elle qui coûte.

---

L'avant du langage est terminé. Un fichier est lu, découpé en jetons, et transformé en un arbre qui décrit exactement le programme, avec la position de chaque morceau et les erreurs de lecture déjà signalées. Mais cet arbre ne fait rien. `1 + 2` est un noeud `BINOP` avec deux enfants, pas le nombre 3. `"a" + 1` n'est encore ni une erreur ni la chaîne `"a1"`. Pour que l'arbre s'exécute, il faut d'abord savoir ce que le programme manipule : des nombres, des chaînes, des listes, des maps, des fonctions, chacun avec ses règles. Ce sont les valeurs, et elles ouvrent la deuxième partie.
