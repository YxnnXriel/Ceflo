---
numero: 3
titre: le parser
note: on donne un sens aux jetons
partie: Lire le texte
---

Voici trois lignes, et ce qu'elles deviennent une fois passées dans le programme qu'on va écrire :

```terminal
$ cat livre/essais/chapitres-01-04/calcul.cf
1 + 2 * 3
(1 + 2) * 3
-x - 1 - 2
$ livre/essais/chapitres-01-04/arbre_etape1 livre/essais/chapitres-01-04/calcul.cf
  1:1   COMPOUND
  1:3     BINOP +
  1:1       gauche: NUMBER 1
  1:7       droite: BINOP *
  1:5         gauche: NUMBER 2
  1:9         droite: NUMBER 3
  3:8     BINOP -
  3:4       gauche: BINOP -
  3:1         gauche: BINOP -
  2:9           gauche: BINOP *
  2:4             gauche: BINOP +
  2:2               gauche: NUMBER 1
  2:6               droite: NUMBER 2
  2:11            droite: NUMBER 3
  3:2           droite: VARIABLE x
  3:6         droite: NUMBER 1
  3:10      droite: NUMBER 2
```

La première ligne est juste : le `*` est plus bas dans l'arbre que le `+`, donc il sera calculé d'abord, et `1 + 2 * 3` vaudra 7. Les deux suivantes sont fausses. On voulait deux expressions, `(1 + 2) * 3` et `-x - 1 - 2`, et on en a une seule, qui commence au milieu de la ligne 2 et se termine au bout de la ligne 3 : `(1 + 2) * 3 - x - 1 - 2`. Le moins du début de la ligne 3 a été lu comme une soustraction.

Ce bug n'est pas une erreur de ce livre. Le vrai CeFlo fait exactement la même chose, et à la fin de la section 3.4, tu sauras pourquoi, ce qu'on pourrait y faire, et pourquoi on ne l'a pas fait. Avant ça, il faut écrire le programme qui fabrique ces arbres : le **parser**. C'est le plus gros fichier de l'avant du langage, 1 079 lignes, et on va l'écrire en entier.

## 3.1 Une suite plate, un arbre

Le lexer nous donne une suite de jetons. Pour `1 + 2 * 3`, c'est `1`, `+`, `2`, `*`, `3`. Rien dans cette suite ne dit que `2 * 3` forme un groupe. C'est une règle que tout le monde connaît depuis l'école, mais qu'il faut écrire quelque part pour qu'un programme la suive.

Ce qu'on veut obtenir, c'est un arbre :

```text
        +
       / \
      1   *
         / \
        2   3
```

Chaque opération est un noeud, et ses deux opérandes sont ses enfants. Pour calculer un noeud, on calcule d'abord ses enfants. L'ordre des calculs est donc écrit dans la forme de l'arbre, et il n'y a plus besoin de parenthèses ni de règles de priorité : elles ont servi à construire l'arbre, puis elles ont disparu. C'est pour ça qu'on l'appelle l'arbre de syntaxe *abstraite* : il garde le sens du texte, pas son écriture.

Le travail du parser est de passer de la suite à l'arbre. On appelle ça l'**analyse syntaxique**, et pour la faire, il faut d'abord décrire précisément quelles suites de jetons forment un programme. Cette description, c'est la **grammaire**.

### Lire GRAMMAIRE.md

Ouvre `GRAMMAIRE.md`. Les règles y sont écrites dans une notation qui s'appelle l'EBNF (*Extended Backus-Naur Form*, du nom de deux des auteurs d'Algol 60). Voici la partie qui nous occupe en premier :

```text
expression     = ou [ "?" expression [ ":" expression ] ] ;
ou             = et { "or" et } ;
et             = egalite { "and" egalite } ;
egalite        = comparaison { ( "==" | "!=" ) comparaison } ;
comparaison    = somme { ( "<" | "<=" | ">" | ">=" ) somme } ;
somme          = terme { ( "+" | "-" ) terme } ;
terme          = facteur { ( "*" | "/" | "%" ) facteur } ;
facteur        = ( "-" | "!" ) facteur | suffixe ;
```

Chaque ligne est une **règle**. À gauche du `=`, son nom. À droite, ce qu'elle reconnaît. Un texte entre guillemets est un jeton tel quel. Un nom est une autre règle. `{ A }` veut dire « A, zéro fois ou plus », `[ A ]` « A, ou rien », et `A | B` « A ou B ».

Lis la règle `somme` à voix haute : une somme, c'est un terme, suivi d'autant de « `+` ou `-`, puis un terme » qu'on veut. Et un terme, c'est un facteur suivi d'autant de « `*`, `/` ou `%`, puis un facteur » qu'on veut. Toute la priorité des opérateurs est là. Comme une somme est faite de termes, et pas l'inverse, un `*` est toujours rangé à l'intérieur d'un terme, donc plus bas dans l'arbre qu'un `+`. Il est donc calculé avant.

On lit la grammaire de haut en bas, de la priorité la plus faible (`?`, puis `or`) à la plus forte (le `-` d'un facteur). Chaque étage ne connaît que celui du dessous. Et la règle `facteur` fait référence à elle-même : `- - 5` est un moins appliqué à un facteur, qui est lui-même un moins appliqué à un facteur. Une règle qui se contient elle-même, c'est ce qui permet à une grammaire courte de décrire des textes aussi longs qu'on veut.

> [!note]
> Le dépôt a gardé des noms de fonctions qui ne suivent pas tout à fait la grammaire. La règle `somme` s'écrit `parser_parse_expression`, et la règle `expression` s'écrit `parser_parse_expr`. `GRAMMAIRE.md` le signale juste après ses règles. Garde ces deux noms en tête, c'est la seule vraie source de confusion du fichier.

Les **jetons** sont les mots de cette grammaire (on dit les *terminaux*), et les règles sont ses phrases. Le lexer a déjà vérifié que chaque mot existe. Le parser vérifie que les mots forment une phrase, et il en construit l'arbre au passage.

## 3.2 Une fonction par règle

Il y a beaucoup de façons d'écrire un parser. CeFlo prend la plus directe : une fonction C par règle de la grammaire. La fonction de `somme` appelle celle de `terme`, qui appelle celle de `facteur`, et ainsi de suite jusqu'aux nombres. Comme les règles s'appellent les unes les autres, parfois elles-mêmes, les fonctions aussi. Et comme on part de la règle du haut (`programme`) pour descendre vers les jetons, on appelle ça l'**analyse par descente récursive** (*recursive descent*).

Avec la règle `somme` sous les yeux, la fonction s'écrit presque toute seule :

```text
somme = terme { ( "+" | "-" ) terme } ;

    lire un terme
    tant que le jeton courant est + ou - :
        le noter, avancer
        lire un terme
        faire un noeud avec les deux
```

Chaque `{ ... }` devient une boucle `while`. Chaque `|` devient un `if` ou un `switch` sur le jeton courant. Chaque jeton entre guillemets devient « vérifier que c'est bien lui, puis avancer ». Chaque nom de règle devient un appel de fonction. Une fois qu'on a vu cette correspondance, on peut traduire n'importe quelle règle de `GRAMMAIRE.md` en C, et c'est exactement ce que fait `src/parser.c`.

### Ce qu'on aurait pu faire d'autre

La descente récursive est la plus vieille méthode, et elle a de la concurrence.

On peut faire écrire le parser par un programme. On donne la grammaire à un **générateur de parser** (yacc, puis bison, dans le monde du C ; ANTLR pour Java), et il produit un parser complet, souvent sous la forme d'une grande table d'états. C'est ce que faisait GCC jusqu'en 2004 pour le C++ et 2006 pour le C. Puis il est passé à un parser écrit à la main, en descente récursive, surtout pour donner de meilleurs messages d'erreur. Clang, le compilateur C d'LLVM, a été écrit à la main dès le départ. Lua lit ses programmes avec une descente récursive écrite à la main, dans un seul fichier, `lparser.c`. Python, lui, a fait le chemin inverse : depuis la version 3.9, son parser est généré à partir d'une grammaire PEG, et c'est un vrai succès.

On peut aussi traiter les opérateurs avec une autre technique : le **parser de Pratt**, où chaque opérateur porte un nombre, sa priorité, et où une seule fonction traite tous les étages. C'est ce que fait *Crafting Interpreters* dans sa seconde partie. C'est plus compact quand il y a beaucoup de niveaux de priorité.

CeFlo garde la descente récursive pure, pour une raison simple : on peut lire le fichier à côté de `GRAMMAIRE.md` et retrouver chaque règle. Le prix, c'est une fonction par étage de priorité, toutes presque identiques. On le verra, ça fait sept fonctions qui se ressemblent beaucoup, et c'est très bien comme ça.

### Où écrire

Le meilleur moyen de comprendre ce chapitre est d'écrire ton propre parser en le suivant, dans `livre/essais/chapitres-01-04/parser.c`, et de laisser celui du dépôt intact pour comparer. On va le construire en quatre étapes. À chaque étape, il compile, et un petit programme, `arbre`, affiche l'arbre qu'il produit. Pour les étapes intermédiaires, certaines fonctions sont plus simples que celles du dépôt : elles sont signalées, avec le fichier `livre/essais/chapitres-01-04/parser_etapeN.c` où on les a testées. À la fin de l'étape 4, ton fichier doit être identique à `src/parser.c`.

## 3.3 Le parser et ses deux jetons

Un parser a besoin de peu de choses : le lexer pour demander les jetons, et le jeton qu'il est en train de regarder. CeFlo en garde un de plus, le précédent, et trois compteurs qu'on expliquera quand ils serviront :

```c fichier="src/include/parser.h" lignes="16-29"
    typedef struct{
        lexer_t *lexer;
        token_t current;
        token_t prev;
        int function_depth;   // pour refuser `return` hors d'une fonction
        int loop_depth;       // pour refuser `break` hors d'une boucle
        int block_depth;      // pour refuser `export` dans un bloc
    }parser_t;

    // Analyse tout un fichier.
    ast_t *parser_parse(lexer_t *lexer);

    // Analyse une seule expression, pour les ${...} de format().
    ast_t *parser_parse_one(lexer_t *lexer);
```

Cet en-tête est déjà dans le dépôt, avec `ast.h` qui décrit les noeuds. On n'a pas besoin de connaître `ast.h` en détail pour l'instant : un noeud est un `ast_t`, il a un `type` (`AST_NUMBER`, `AST_BINOP`...), et des champs pour chaque sorte de noeud. Le chapitre 4 lui est consacré.

Commence ton fichier par son en-tête, et deux macros :

```c fichier="src/parser.c" lignes="1-16"
/**
 * @file parser.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief analyse descendante : une fonction par règle de GRAMMAIRE.md
 * @copyright Copyright (c) 2023
 */

#include <stdlib.h>
#include <string.h>

#include "include/parser.h"
#include "include/error.h"
#include "include/str.h"

#define TYPE(parser) ((parser)->current.type)
#define VALUE(parser) ((parser)->current.value)
```

`TYPE(parser)` et `VALUE(parser)` reviennent des centaines de fois. Avec eux, `while(TYPE(parser) == TOKEN_PLUS)` se lit d'un coup d'oeil. Les parenthèses autour de `parser` dans la macro sont l'hygiène habituelle : elles protègent le jour où on écrit `TYPE(&p)`.

Pour l'étape 1, ajoute une seule déclaration en avance, qu'on expliquera bientôt :

```c fichier="livre/essais/chapitres-01-04/parser_etape1.c"
static ast_t *parser_parse_expr(parser_t *parser);
```

### Les erreurs

Viennent ensuite les deux fonctions qui arrêtent tout :

```c fichier="src/parser.c" lignes="29-42"
static _Noreturn void parser_error_expected(parser_t *parser, const char *expected){
    error_syntax(
        parser->lexer->fileName,
        parser->current.line,
        parser->current.colone,
        "`%s` attendu, mais `%s` trouvé",
        expected,
        token_to_string(TYPE(parser))
    );
}

static _Noreturn void parser_error_here(parser_t *parser, const char *message){
    error_syntax(parser->lexer->fileName,parser->current.line,parser->current.colone,"%s",message);
}
```

Toutes les erreurs du parser pointent le jeton *courant*, celui qui ne convient pas. Le message nomme le jeton avec `token_to_string`, le texte de la X-macro du chapitre 2 : `` `)` attendu, mais `var` trouvé ``. Le `"%s"` de `parser_error_here` n'est pas de trop : passer un message directement comme format serait un bug le jour où il contient un `%`.

Une erreur arrête le programme (`error_syntax` appelle `exit`). Le parser de CeFlo ne cherche donc jamais à « se rattraper » pour trouver d'autres erreurs plus loin. On en reparle dans la note de conception.

### Avancer

```c fichier="src/parser.c" lignes="44-69" focus="60-62"
static void newParser(parser_t *parser, lexer_t *lexer){
    memset(parser,0,sizeof(*parser));
    parser->lexer = lexer;
    parser->prev.type = TOKEN_EOF;
    parser->current = lexer_get_next_token(lexer);
}

static void freeParser(parser_t *parser){
    freeToken(&parser->prev);
    freeToken(&parser->current);
}

/**
 * @brief passe au jeton suivant : l'ancien jeton reste lisible dans parser->prev
 */
static void parser_advance(parser_t *parser){
    freeToken(&parser->prev);
    parser->prev = parser->current;
    parser->current = lexer_get_next_token(parser->lexer);
}

static void parser_eat(parser_t *parser, TokenEnum type){
    if(TYPE(parser) != type)
        parser_error_expected(parser,token_to_string(type));
    parser_advance(parser);
}
```

`newParser` demande tout de suite le premier jeton : le parser a toujours un jeton courant, même au début. `prev` reçoit le type `TOKEN_EOF`, qui ne possède pas son texte : le premier `freeToken(&parser->prev)` de `parser_advance` ne libérera donc rien. Sans cette ligne, `memset` aurait laissé `prev.type` à 0, c'est-à-dire `TOKEN_ID`, et `freeToken` aurait appelé `free(NULL)`. Ce n'est pas un plantage (`free(NULL)` ne fait rien), mais c'est le genre de chance sur laquelle on ne construit pas.

`parser_advance` est la seule fonction qui avance. Elle libère le jeton d'avant-hier, décale le jeton courant dans `prev`, et en demande un neuf. Ainsi, après avoir avancé, on peut encore lire le texte du jeton qu'on vient de dépasser. C'est ce qui permet le schéma qu'on retrouvera partout : regarder le type courant, avancer, et se servir de ce qu'on a vu.

`parser_eat` est la traduction directe d'un jeton entre guillemets dans la grammaire : « ici il faut un `)` ». Si c'est bien lui, on avance. Sinon, on s'arrête avec un message qui dit les deux : ce qu'on voulait et ce qu'on a trouvé.

> [!attention]
> Le texte d'un nom (`VALUE(parser)`) est libéré dès le deuxième `parser_advance` qui suit. Si tu ranges ce pointeur dans un noeud, il deviendra invalide un jeton plus tard. Il faut toujours en faire une copie : c'est ce que fait `parser_eat_id`, un peu plus bas.

### Créer un noeud

```c fichier="src/parser.c" lignes="78-97"
static ast_t *parser_new_node(parser_t *parser, ast_type type){
    return newAstWithLine(type,parser->lexer->fileName,parser->current.line,parser->current.colone);
}

static void parser_check_depth(parser_t *parser){
    if(ceflo_stack_exceeded())
        parser_error_here(parser,"code trop profondément imbriqué");
}

/**
 * @brief lit un nom et en donne une copie
 */
static char *parser_eat_id(parser_t *parser){
    char *name = NULL;
    if(TYPE(parser) != TOKEN_ID)
        parser_error_expected(parser,"identifiant");
    allowChar(&name,VALUE(parser));
    parser_advance(parser);
    return name;
}
```

`parser_new_node` crée un noeud et lui donne la position du jeton courant. C'est un détail qui décide de la qualité de tous les messages d'erreur à venir : selon le moment où on appelle `parser_new_node`, un noeud se souviendra du mot `var` ou du nom qui le suit, de l'opérateur `+` ou de son opérande gauche. On fera ces choix un par un, et le chapitre 4 en tire les conséquences.

`parser_check_depth` protège la pile C. La descente récursive a une faiblesse : une expression comme `((((((1))))))` appelle une fonction par parenthèse. Avec trois cent mille parenthèses, la pile déborde et le programme meurt d'un *segmentation fault*, sans message. `ceflo_stack_exceeded` (dans `src/common.c`) mesure la distance entre l'endroit actuel de la pile et l'endroit noté au démarrage par `ceflo_stack_init`, et dit quand on approche de la limite. On s'arrête alors avec une vraie erreur.

`parser_eat_id` lit un nom, en fait une copie avec `allowChar`, et avance. La copie appartiendra au noeud qui la reçoit, et c'est `clearAst` qui la libérera à la fin du programme.

On saute pour l'instant `parser_match` (lignes 71 à 76) : on n'en aura besoin qu'à l'étape 2, et gcc se plaindrait d'une fonction jamais utilisée.

## 3.4 Le premier étage : des nombres et des sommes

On commence par le bas de la grammaire : les choses qui ne contiennent rien d'autre. Un nombre d'abord :

```c fichier="src/parser.c" lignes="117-122" fonction="parser_parse_number"
static ast_t *parser_parse_number(parser_t *parser){
    ast_t *number = parser_new_node(parser,AST_NUMBER);
    number->number_value = strtod(VALUE(parser),NULL);
    parser_advance(parser);
    return number;
}
```

C'est ici que le texte `"12"` du lexer devient enfin le nombre 12, avec `strtod`. Entiers et décimaux finissent dans le même `double` : CeFlo n'a qu'un type de nombre. Le noeud est créé *avant* d'avancer, pour prendre la position du nombre lui-même.

### Le primaire

La règle `primaire` dit ce qui peut se trouver au tout début d'une expression. Pour l'étape 1, on n'en garde que trois cas : un nombre, un nom, une parenthèse.

```c fichier="livre/essais/chapitres-01-04/parser_etape1.c"
static ast_t *parser_parse_primary(parser_t *parser){
    ast_t *node = NULL;

    switch(TYPE(parser)){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            return parser_parse_number(parser);
        case TOKEN_ID:
            node = parser_new_node(parser,AST_VARIABLE);
            node->variable_name = parser_eat_id(parser);
            return node;
        case TOKEN_LPARENT:
            parser_advance(parser);
            node = parser_parse_expr(parser);
            parser_eat(parser,TOKEN_RPARENT);
            return node;
        default:
            parser_error_expected(parser,"expression");
    }
}
```

Le `switch` regarde un seul jeton et choisit. C'est la propriété qui rend la descente récursive simple : dans CeFlo, le premier jeton suffit toujours à savoir quelle règle suivre. On dit que la grammaire est *LL(1)* (lue de gauche à droite, un jeton d'avance), presque partout. On verra les deux endroits où elle triche.

La parenthèse est le cas qui fait tout boucler. Entre `(` et `)`, il peut y avoir n'importe quelle expression, y compris une somme, qui contient des termes, qui contiennent des facteurs, qui contiennent des primaires, qui peuvent être une nouvelle parenthèse. `parser_parse_primary` doit donc appeler `parser_parse_expr`, qui n'est écrite que plus bas dans le fichier : c'est la déclaration en avance qu'on a ajoutée en haut. Et remarque qu'aucun noeud n'est créé pour la parenthèse : on rend directement l'expression qu'elle contient. Les parenthèses ont fait leur travail en forçant l'ordre de la lecture, et l'arbre n'a plus besoin d'elles.

Le `default` n'a pas de `return`, et gcc ne s'en plaint pas : `parser_error_expected` est `_Noreturn`, il sait qu'on ne sort jamais de cet appel.

### Le facteur

Vient la règle `facteur = ( "-" | "!" ) facteur | suffixe`. Les suffixes (`.nom`, `[i]`, `(args)`) viendront à l'étape 2. En attendant, un suffixe sera juste un primaire. Ajoute cette version temporaire :

```c fichier="livre/essais/chapitres-01-04/parser_etape1.c"
// Étape 1 : pas encore de suffixes. On les écrira à l'étape 2.
static ast_t *parser_parse_postfix(parser_t *parser){
    return parser_parse_primary(parser);
}
```

Puis le vrai facteur, celui du dépôt :

```c fichier="src/parser.c" lignes="317-329" fonction="parser_parse_factor" focus="324"
static ast_t *parser_parse_factor(parser_t *parser){
    parser_check_depth(parser);

    if(TYPE(parser) == TOKEN_MINUS || TYPE(parser) == TOKEN_NOT){
        ast_t *unop = parser_new_node(parser,AST_UNOP);
        unop->op = TYPE(parser);
        parser_advance(parser);
        unop->right = parser_parse_factor(parser);
        return unop;
    }

    return parser_parse_postfix(parser);
}
```

La ligne 324 est la règle qui se contient elle-même. Après un `-`, on lit un facteur entier, qui peut lui-même commencer par un `-`. `-x` donne un noeud `AST_UNOP` dont l'enfant droit est la variable `x`, et `- - x` deux noeuds empilés.

C'est aussi ici qu'on appelle `parser_check_depth`. Chaque parenthèse repasse forcément par un facteur (somme, terme, facteur, primaire, parenthèse, et on recommence), donc surveiller la pile ici suffit à toutes les expressions.

### Le terme et la somme

On arrive aux opérateurs à deux opérandes. Un petit outil d'abord :

```c fichier="src/parser.c" lignes="331-337" fonction="parser_new_binop"
static ast_t *parser_new_binop(parser_t *parser, ast_t *left){
    ast_t *binop = parser_new_node(parser,AST_BINOP);
    binop->left = left;
    binop->op = TYPE(parser);
    parser_advance(parser);
    return binop;
}
```

On l'appelle quand le jeton courant est l'opérateur. Le noeud prend donc la position de l'opérateur, ce qui est exactement ce qu'on veut : quand `10 / 0` échouera, la flèche pointera le `/`. On range l'opérande gauche, on note l'opérateur, on avance. Il ne reste à l'appelant qu'à lire l'opérande droit.

```c fichier="src/parser.c" lignes="339-361" focus="342-346"
static ast_t *parser_parse_term(parser_t *parser){
    ast_t *value = parser_parse_factor(parser);

    while(TYPE(parser) == TOKEN_MULT || TYPE(parser) == TOKEN_DIV || TYPE(parser) == TOKEN_MODULO){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_factor(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_expression(parser_t *parser){
    ast_t *value = parser_parse_term(parser);

    while(TYPE(parser) == TOKEN_PLUS || TYPE(parser) == TOKEN_MINUS){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_term(parser);
        value = binop;
    }

    return value;
}
```

C'est la traduction mot pour mot de `terme = facteur { ( "*" | "/" | "%" ) facteur }`. La boucle a une conséquence qu'il faut bien voir. Déroule-la sur `10 - 3 - 2` :

```text
value = 10
voit -   value = (10 - 3)
voit -   value = ((10 - 3) - 2)
```

À chaque tour, l'arbre déjà construit devient l'enfant *gauche* du nouveau noeud. On obtient `(10 - 3) - 2`, qui vaut 5, et pas `10 - (3 - 2)`, qui vaudrait 9. On dit que les opérateurs sont **associatifs à gauche**, et c'est ce que tout le monde attend pour la soustraction et la division.

> [!attention]
> La tentation, en lisant une règle comme `somme = terme { "+" terme }`, est de l'écrire avec une récursion : `somme = terme [ "+" somme ]`. Ça reconnaît les mêmes textes, mais l'arbre penche à droite, et `10 - 3 - 2` vaut 9. Le bug ne se voit qu'avec `-` et `/`, jamais avec `+` et `*`. C'est le meilleur moyen de passer une semaine à chercher pourquoi un seul test sur cent échoue.

La boucle a un autre avantage, plus discret : elle ne consomme pas de pile. Une somme de cinq cent mille termes donne un arbre très profond, mais le parser le construit dans une simple boucle. On le vérifiera à la section 3.10.

### Assembler l'étape 1

Il manque deux morceaux pour que tout compile : `parser_parse_expr`, qu'on a promis en haut du fichier, et `parser_parse`, la fonction publique. Pour l'instant, une expression est une somme, et un programme une suite d'expressions :

```c fichier="livre/essais/chapitres-01-04/parser_etape1.c"
// Étape 1 : une expression, c'est une somme. Les étages du dessus viendront à l'étape 2.
static ast_t *parser_parse_expr(parser_t *parser){
    return parser_parse_expression(parser);
}
```

```c fichier="livre/essais/chapitres-01-04/parser_etape1.c"
// Étapes 1 et 2 : un programme est une suite d'expressions.
ast_t *parser_parse(lexer_t *lexer){
    parser_t parser;
    newParser(&parser,lexer);

    ast_t *block = parser_new_node(&parser,AST_COMPOUND);
    block->list_compound = newList();
    while(TYPE(&parser) != TOKEN_EOF)
        List_Append(block->list_compound,parser_parse_expr(&parser));

    freeParser(&parser);
    return block;
}
```

Un `AST_COMPOUND` est un bloc : une liste de noeuds dans `list_compound`. On lit des expressions jusqu'au jeton de fin.

Pour voir le résultat, il faut un programme qui affiche un arbre. On l'écrira en détail à la fin du chapitre, section 3.11 : tape-le maintenant si tu veux suivre chaque étape, ou contente-toi de lire les sorties. Il se compile avec ton parser à la place de celui du dépôt :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc -Isrc/include -o livre/essais/chapitres-01-04/arbre_etape1 livre/essais/chapitres-01-04/arbre.c livre/essais/chapitres-01-04/parser_etape1.c src/ast.c src/lexer.c src/token.c src/error.c src/str.c src/list.c src/common.c
```

Aucun avertissement. Le `-Isrc` permet aux `#include "include/parser.h"` de trouver leurs fichiers depuis `livre/essais/chapitres-01-04/`. Et c'est la sortie du début du chapitre qu'on obtient, avec son bug.

### Le moins qui commence une ligne

Relis la règle `somme`. Après un terme, si le jeton suivant est un `-`, la somme continue. Rien ne dit que ce `-` doit être sur la même ligne. À la ligne 3 de `calcul.cf`, le `-` de `-x` suit le `3` de la ligne 2, et la boucle de `parser_parse_expression` le prend.

Le vrai CeFlo fait pareil, et on peut le prendre en défaut :

```terminal
$ cat livre/essais/chapitres-01-04/moins.cf
var a = 5
var b = a
-1
println(b)
$ ./Ceflo livre/essais/chapitres-01-04/moins.cf
4
```

On s'attendait à 5, avec une ligne `-1` qui ne sert à rien. On a `var b = a - 1`. Au chapitre 2, on a vu que le lexer note `newline_before` pour régler ce genre d'ambiguïté, et on verra à l'étape 2 que le parser s'en sert pour `(` et `[`. Il ne s'en sert pas pour les opérateurs, et c'est un choix défendable : une ligne qui commence par `-` ou par `+` est presque toujours la suite d'un calcul trop long, comme

```ceflo
var total = prix_hors_taxe
    + taxe
    - remise
```

qui marche très bien en CeFlo, et qui casserait si on coupait l'expression au retour à la ligne. Une instruction qui *commence* par un moins, elle, ne fait rien d'utile : elle calcule une valeur et la jette. JavaScript a fait le même choix, avec le même piège. Go a fait l'inverse : son lexer met un point-virgule à la fin d'une ligne qui finit par un nombre ou un nom, et un calcul sur plusieurs lignes doit laisser son opérateur en fin de ligne. Ce n'est pas écrit dans `GRAMMAIRE.md`, et ça mériterait de l'être.

## 3.5 Tous les étages

L'étape 2 complète les expressions. Ajoute d'abord `parser_match`, qu'on avait sautée :

```c fichier="src/parser.c" lignes="71-76" fonction="parser_match"
static bool parser_match(parser_t *parser, TokenEnum type){
    if(TYPE(parser) != type)
        return false;
    parser_advance(parser);
    return true;
}
```

C'est la cousine de `parser_eat` pour les morceaux facultatifs de la grammaire, ceux entre `[ ]` : si le jeton est là, on le mange et on rend `true`, sinon on ne touche à rien. `if(parser_match(parser,TOKEN_COLON))` se lit « s'il y a un `:` ».

Puis, au-dessus de la somme, les étages qui manquent. Ils ont tous la même forme :

```c fichier="src/parser.c" lignes="363-386"
static ast_t *parser_parse_comparison(parser_t *parser){
    ast_t *value = parser_parse_expression(parser);

    while(TYPE(parser) == TOKEN_SUP || TYPE(parser) == TOKEN_SUP_EQUAL ||
          TYPE(parser) == TOKEN_INF || TYPE(parser) == TOKEN_INF_EQUAL){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_expression(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_equality(parser_t *parser){
    ast_t *value = parser_parse_comparison(parser);

    while(TYPE(parser) == TOKEN_EQUAL_EQUAL || TYPE(parser) == TOKEN_DIF){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_comparison(parser);
        value = binop;
    }

    return value;
}
```

```c fichier="src/parser.c" lignes="388-410"
static ast_t *parser_parse_and(parser_t *parser){
    ast_t *value = parser_parse_equality(parser);

    while(TYPE(parser) == TOKEN_AND){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_equality(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_or(parser_t *parser){
    ast_t *value = parser_parse_and(parser);

    while(TYPE(parser) == TOKEN_OR){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_and(parser);
        value = binop;
    }

    return value;
}
```

Quatre fonctions, une seule idée. Chaque étage lit l'étage du dessous, puis boucle tant qu'il voit un de ses opérateurs. Pour ajouter un niveau de priorité, on écrit une fonction de plus et on change un seul appel.

Le tableau des priorités de CeFlo, de la plus forte à la plus faible :

| Étage | Opérateurs | Fonction |
| --- | --- | --- |
| facteur | `-x` `!x` | `parser_parse_factor` |
| terme | `*` `/` `%` | `parser_parse_term` |
| somme | `+` `-` | `parser_parse_expression` |
| comparaison | `<` `<=` `>` `>=` | `parser_parse_comparison` |
| égalité | `==` `!=` | `parser_parse_equality` |
| et | `and` | `parser_parse_and` |
| ou | `or` | `parser_parse_or` |
| condition | `? :` | `parser_parse_expr` |

Ces choix ne sont pas arbitraires. `and` est sous `==` pour que `a == 1 and b == 2` se lise `(a == 1) and (b == 2)`, sans parenthèses. Et `and` passe avant `or`, comme la multiplication avant l'addition, ce que font C, Python, Java et à peu près tout le monde.

> [!note]
> Le C a une priorité célèbre pour être fausse : `x & 1 == 0` s'y lit `x & (1 == 0)`, parce que `&` est sous `==`. Dennis Ritchie a raconté que c'était un reste d'une époque où `&` servait aussi de « et » logique, et qu'il était trop tard pour changer quand `&&` est arrivé. Une table de priorités, c'est une décision qu'on ne peut plus jamais reprendre.

### La condition

L'étage du haut est un peu différent :

```c fichier="src/parser.c" lignes="412-427" fonction="parser_parse_expr" focus="422-424"
// condition ? alors : sinon     (sans `: sinon`, la valeur vaut none)
static ast_t *parser_parse_expr(parser_t *parser){
    ast_t *value = parser_parse_or(parser);

    if(TYPE(parser) != TOKEN_MARK)
        return value;

    ast_t *ternary = parser_new_node(parser,AST_TERNARY);
    parser_advance(parser);
    ternary->if_expr = value;
    ternary->if_other = parser_parse_expr(parser);
    ternary->else_body = parser_match(parser,TOKEN_COLON) ?
        parser_parse_expr(parser) : parser_new_node(parser,AST_NONE);

    return ternary;
}
```

Pas de boucle ici : après le `?`, on rappelle `parser_parse_expr` elle-même, ce qui rend l'opérateur associatif à *droite*. `a ? b : c ? d : e` se lit `a ? b : (c ? d : e)`, comme en C. La partie `: sinon` est facultative, une petite fantaisie de CeFlo : sans elle, on fabrique un noeud `AST_NONE`, et `x > 0 ? "positif"` vaut `none` quand `x` est négatif. Remarque que c'est le *parser* qui invente ce noeud : l'interpréteur n'aura jamais à traiter un ternaire sans `else`, il en verra toujours un complet.

Les noms des champs (`if_expr`, `if_other`, `else_body`) sont empruntés au noeud `if`. Le chapitre 4 explique pourquoi un même champ sert à plusieurs sortes de noeuds.

## 3.6 Les suffixes

Entre le facteur et le primaire, il reste les suffixes : ce qui se colle derrière une valeur. `a.b`, `a[0]`, `f(1, 2)`, et toutes leurs combinaisons, comme `eleves[0].moyenne()`. La règle :

```text
suffixe        = primaire { "." ID | "[" expression "]" | arguments } ;
arguments      = "(" [ expression { "," expression } ] ")" ;
```

Un primaire, puis autant de suffixes qu'on veut. On reconnaît la forme `{ ... }` : encore une boucle. Commence par les arguments d'un appel, qui serviront à plusieurs endroits :

```c fichier="src/parser.c" lignes="103-115" fonction="parser_parse_call_args"
static list_t *parser_parse_call_args(parser_t *parser){
    list_t *args = newList();

    parser_eat(parser,TOKEN_LPARENT);
    if(TYPE(parser) != TOKEN_RPARENT){
        do{
            List_Append(args,parser_parse_expr(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }
    parser_eat(parser,TOKEN_RPARENT);

    return args;
}
```

Retiens ce motif, il revient six fois dans le fichier : « si ce n'est pas tout de suite la fin, lire un élément, puis recommencer tant qu'il y a une virgule ». La grammaire `[ expression { "," expression } ]` devient un `if` (les crochets) qui contient un `do ... while` (l'accolade). Avec ce motif, une virgule est toujours *suivie* d'un élément. `f(1, 2,)` est donc une erreur : après la dernière virgule, on lit une expression, et on trouve `)`.

Place cette fonction juste après `parser_eat_id`, avant `parser_parse_number` : en C, une fonction `static` doit être déclarée avant qu'on s'en serve, et l'ordre du fichier du dépôt suit cette contrainte.

Puis remplace la version temporaire de `parser_parse_postfix` par la vraie :

```c fichier="src/parser.c" lignes="281-315" fonction="parser_parse_postfix" focus="299,306"
/**
 * @brief les suites `.nom`, `[index]` et `(args)`
 *
 * Comme les `;` sont facultatifs, `[` et `(` doivent rester sur la même ligne :
 * sinon `println(a)` suivi de `[1,2].print()` serait lu comme un accès à la liste.
 */
static ast_t *parser_parse_postfix(parser_t *parser){
    ast_t *value = parser_parse_primary(parser);

    while(true){
        if(TYPE(parser) == TOKEN_DOT){
            ast_t *access = parser_new_node(parser,AST_OBJ_ACCESS);
            parser_advance(parser);
            access->left = value;
            access->colone = parser->current.colone;
            access->line = parser->current.line;
            access->variable_name = parser_eat_id(parser);
            value = access;
        }else if(TYPE(parser) == TOKEN_LBRACE && !parser->current.newline_before){
            ast_t *access = parser_new_node(parser,AST_LIST_ACCESS);
            parser_advance(parser);
            access->left = value;
            access->list_ptr = parser_parse_expr(parser);
            parser_eat(parser,TOKEN_RBRACE);
            value = access;
        }else if(TYPE(parser) == TOKEN_LPARENT && !parser->current.newline_before){
            ast_t *call = parser_new_node(parser,AST_FUNCTION_CALL);
            call->left = value;
            call->function_call_args = parser_parse_call_args(parser);
            value = call;
        }else{
            return value;
        }
    }
}
```

Trois suffixes, trois branches, et à chaque tour l'arbre construit devient l'enfant gauche du suivant, comme dans les étages d'opérateurs. `a.b[0](1, 2)` donne un appel, dont la fonction est un accès par index, dont la liste est un accès à l'attribut `b` de `a`. Lu de haut en bas, l'arbre se lit à l'envers du texte : la dernière chose écrite est la dernière calculée.

Les lignes 299 et 306 sont la règle annoncée au chapitre 2. Un `[` ou un `(` qui commence une ligne ne continue pas l'expression d'avant : c'est le début d'une nouvelle instruction. On n'applique pas la règle au `.`, et c'est voulu : une ligne qui commence par `.` ne peut rien commencer d'autre, alors autant permettre d'enchaîner les méthodes sur plusieurs lignes.

Regarde aussi les lignes 295 et 296 : le noeud d'un accès `.nom` est créé sur le point, puis on corrige sa position pour celle du nom. Quand `p.age` échouera parce que `age` n'existe pas, la flèche pointera `age`, pas le point. Tu en as vu le résultat au chapitre 2 : `` number n'a pas de méthode `lenght` `` pointait le `l` de `lenght`.

> [!note]
> Pourquoi un appel est-il un suffixe, et pas une règle à part comme `ID "(" arguments ")"` ? Parce qu'en CeFlo, une fonction est une valeur. `f(1)(2)` appelle ce que `f(1)` rend, et `liste[0]()` appelle la fonction rangée dans la liste. Si l'appel exigeait un nom, ces deux programmes seraient impossibles à écrire.

## 3.7 Les valeurs écrites dans le code

Il reste à compléter le primaire : tout ce qu'on peut écrire directement dans le texte. Les chaînes, les caractères et les booléens d'abord :

```c fichier="src/parser.c" lignes="124-145"
static ast_t *parser_parse_string(parser_t *parser){
    ast_t *string = parser_new_node(parser,AST_STRING);
    string->string_lenght = parser->current.lenght;
    string->string_value = ceflo_alloc((size_t)string->string_lenght + 1);
    memcpy(string->string_value,VALUE(parser),(size_t)string->string_lenght);
    parser_advance(parser);
    return string;
}

static ast_t *parser_parse_char(parser_t *parser){
    ast_t *carac = parser_new_node(parser,AST_CHAR);
    carac->char_value = VALUE(parser)[0];
    parser_advance(parser);
    return carac;
}

static ast_t *parser_parse_bool(parser_t *parser){
    ast_t *boolean = parser_new_node(parser,AST_BOOL);
    boolean->bool_value = TYPE(parser) == TOKEN_TRUE;
    parser_advance(parser);
    return boolean;
}
```

Pour la chaîne, on copie avec `memcpy` et la longueur du jeton, pas avec `allowChar` qui s'arrêterait au premier `\0` : c'est le `lenght` du chapitre 2 qui sert enfin. Le `+ 1` et `calloc` donnent un `\0` final gratuit, pour les fonctions C qui en voudraient un.

### Les listes

```c fichier="src/parser.c" lignes="147-160" fonction="parser_parse_list"
static ast_t *parser_parse_list(parser_t *parser){
    ast_t *list = parser_new_node(parser,AST_LIST);
    list->list_compound = newList();

    parser_eat(parser,TOKEN_LBRACE);
    if(TYPE(parser) != TOKEN_RBRACE){
        do{
            List_Append(list->list_compound,parser_parse_expr(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }
    parser_eat(parser,TOKEN_RBRACE);

    return list;
}
```

Le même motif que les arguments, avec des crochets. Et la même conséquence : pas de virgule finale.

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/e_virgule.cf
erreur de syntaxe: `expression` attendu, mais `]` trouvé
  --> livre/essais/chapitres-01-04/e_virgule.cf:1:15
    1 | var l = [1, 2,]
      |               ^
```

C'est l'erreur du chapitre 1, quand j'avais mis une virgule après le dernier élève.

### new et @

```c fichier="src/parser.c" lignes="162-179"
// new Nom(args)  ou  new Nom
static ast_t *parser_parse_new(parser_t *parser){
    ast_t *node = parser_new_node(parser,AST_STRUCT);
    parser_eat(parser,TOKEN_NEW);
    node->function_call_name = parser_eat_id(parser);
    node->function_call_args = TYPE(parser) == TOKEN_LPARENT && !parser->current.newline_before ?
        parser_parse_call_args(parser) : newList();
    return node;
}

// @type(args)
static ast_t *parser_parse_type_call(parser_t *parser){
    ast_t *node = parser_new_node(parser,AST_TYPE_CALL);
    parser_eat(parser,TOKEN_AT_SIGN);
    node->function_call_name = parser_eat_id(parser);
    node->function_call_args = parser_parse_call_args(parser);
    return node;
}
```

`new Point` peut s'écrire sans parenthèses, et la règle du retour à la ligne s'applique encore : `new Point` en fin de ligne, suivi d'une ligne qui commence par `(`, ne prend pas cette parenthèse. Pour `@`, les parenthèses sont obligatoires, et c'est `parser_eat_id` qui produit l'erreur qu'on avait vue avec `1 @ 2` au chapitre 2 : `` `identifiant` attendu, mais `nombre` trouvé ``.

### Les maps

Le map est la valeur la plus riche à lire. Une entrée peut s'écrire de cinq façons :

```ceflo
var t = { nom: "Awa", "avec espace": 1, 3: "trois", [cle]: 2, age }
```

Un nom, une chaîne, un nombre, une clé calculée entre crochets, et le raccourci `age` qui veut dire `age: age`. Tout se passe dans une fonction :

```c fichier="src/parser.c" lignes="181-206" focus="200"
static ast_t *parser_string_from_id(parser_t *parser){
    ast_t *string = parser_new_node(parser,AST_STRING);
    string->string_lenght = (int)strlen(VALUE(parser));
    allowChar(&string->string_value,VALUE(parser));
    return string;
}

/**
 * @brief une entrée de map, comme en TypeScript :
 *        { nom: 1, "avec espace": 2, 3: 4, [calcul]: 5, raccourci }
 */
static void parser_parse_map_entry(parser_t *parser, list_t *items){
    ast_t *key = NULL;

    if(TYPE(parser) == TOKEN_ID){
        key = parser_string_from_id(parser);
        parser_advance(parser);

        // { raccourci } veut dire { raccourci: raccourci }
        if(TYPE(parser) == TOKEN_COMA || TYPE(parser) == TOKEN_RBRACKET){
            ast_t *variable = newAstWithLine(AST_VARIABLE,key->file,key->line,key->colone);
            allowChar(&variable->variable_name,key->string_value);
            List_Append(items,key);
            List_Append(items,variable);
            return;
        }
```

Un nom de clé devient une chaîne : `{ nom: 1 }` a la clé `"nom"`, pas la valeur de la variable `nom`. Puis vient l'un des deux endroits où la grammaire triche avec la règle du « un jeton suffit ». Après un nom, on ne sait pas encore si c'est `nom: valeur` ou le raccourci `nom`. On regarde le jeton d'*après* (ligne 200) : une virgule ou une accolade fermante, c'est le raccourci. On fabrique alors à la main le noeud `AST_VARIABLE` qu'on aurait obtenu en écrivant `nom: nom`. Là encore, l'interpréteur n'aura qu'une seule forme à traiter.

```c fichier="src/parser.c" lignes="207-222"
    }else if(TYPE(parser) == TOKEN_STRING){
        key = parser_parse_string(parser);
    }else if(TYPE(parser) == TOKEN_INTEGER || TYPE(parser) == TOKEN_FLOAT){
        key = parser_parse_number(parser);
    }else if(TYPE(parser) == TOKEN_LBRACE){
        parser_advance(parser);
        key = parser_parse_expr(parser);
        parser_eat(parser,TOKEN_RBRACE);
    }else{
        parser_error_expected(parser,"clé");
    }

    parser_eat(parser,TOKEN_COLON);
    List_Append(items,key);
    List_Append(items,parser_parse_expr(parser));
}
```

Les clés et les valeurs vont toutes dans la même liste, à la suite : clé, valeur, clé, valeur. C'est le commentaire de `AST_MAP` dans `ast.h`. Une liste de paires aurait été plus propre, mais aurait demandé une structure de plus.

```c fichier="src/parser.c" lignes="224-237" fonction="parser_parse_map" focus="231-232"
static ast_t *parser_parse_map(parser_t *parser){
    ast_t *map = parser_new_node(parser,AST_MAP);
    map->list_compound = newList();

    parser_eat(parser,TOKEN_LBRACKET);
    while(TYPE(parser) != TOKEN_RBRACKET){
        parser_parse_map_entry(parser,map->list_compound);
        if(!parser_match(parser,TOKEN_COMA))
            break;
    }
    parser_eat(parser,TOKEN_RBRACKET);

    return map;
}
```

Voilà la réponse à la question du chapitre 1. Le map n'utilise pas le motif `do ... while`, mais une boucle qui teste l'accolade fermante *avant* chaque entrée. Après une virgule, on revient au `while`, et si c'est `}`, on s'arrête. `{ a: 1, }` est donc accepté. Les deux motifs sont aussi simples l'un que l'autre ; la liste aurait pu être écrite comme le map. C'est une incohérence du langage, et c'est un des défis de la fin du chapitre.

### Le primaire complet

Remplace le primaire de l'étape 1 par celui du dépôt. Pour l'étape 2, on n'en retire que les lignes 269 et 270, le cas `TOKEN_LFUNC`, qui appelle une fonction qu'on n'écrira qu'à l'étape 4 :

```c fichier="src/parser.c" lignes="239-268" fonction="parser_parse_primary"
static ast_t *parser_parse_primary(parser_t *parser){
    ast_t *node = NULL;

    switch(TYPE(parser)){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            return parser_parse_number(parser);
        case TOKEN_STRING:
            return parser_parse_string(parser);
        case TOKEN_CHAR:
            return parser_parse_char(parser);
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parser_parse_bool(parser);
        case TOKEN_NONE:
            node = parser_new_node(parser,AST_NONE);
            parser_advance(parser);
            return node;
        case TOKEN_ID:
            node = parser_new_node(parser,AST_VARIABLE);
            node->variable_name = parser_eat_id(parser);
            return node;
        case TOKEN_LBRACE:
            return parser_parse_list(parser);
        case TOKEN_LBRACKET:
            return parser_parse_map(parser);
        case TOKEN_NEW:
            return parser_parse_new(parser);
        case TOKEN_AT_SIGN:
            return parser_parse_type_call(parser);
```

La suite du `switch` est celle de l'étape 1 (le cas `TOKEN_LPARENT` et le `default`). Remplace aussi la `parser_parse_expr` temporaire par la vraie, celle de la section 3.5, qui commence par `parser_parse_or`. Compile avec la même commande qu'à l'étape 1 (en remplaçant `etape1` par `etape2`), toujours sans avertissement, et essaie :

```terminal
$ cat livre/essais/chapitres-01-04/expr.cf
a.b[0](1, 2)
x < 2 or !ok and y == 3
n > 0 ? "positif" : "négatif"
[1, 2] + {nom: "Awa", 3: true, age}
$ livre/essais/chapitres-01-04/arbre_etape2 livre/essais/chapitres-01-04/expr.cf
  1:1   COMPOUND
  1:7     FUNCTION_CALL
  1:4       appelé: LIST_ACCESS
  1:3         liste: OBJ_ACCESS b
  1:1           gauche: VARIABLE a
  1:5         index: NUMBER 0
  1:8       arg: NUMBER 1
  1:11      arg: NUMBER 2
  2:7     BINOP or
  2:3       gauche: BINOP <
  2:1         gauche: VARIABLE x
  2:5         droite: NUMBER 2
  2:14      droite: BINOP and
  2:10        gauche: UNOP !
  2:11          droite: VARIABLE ok
  2:20        droite: BINOP ==
  2:18          gauche: VARIABLE y
  2:23          droite: NUMBER 3
  3:7     TERNARY
  3:3       si: BINOP >
  3:1         gauche: VARIABLE n
  3:5         droite: NUMBER 0
  3:9       alors: STRING "positif"
  3:21      sinon: STRING "négatif"
  4:8     BINOP +
  4:1       gauche: LIST
  4:2         NUMBER 1
  4:5         NUMBER 2
  4:10      droite: MAP
  4:11        STRING "nom"
  4:16        STRING "Awa"
  4:23        NUMBER 3
  4:26        BOOL true
  4:32        STRING "age"
  4:32        VARIABLE age
```

Lis la ligne 2 de bas en haut. Le `!` ne s'applique qu'à `ok`. Le `==` est sous le `and`, qui est sous le `or`. Le parser a placé chaque parenthèse invisible au bon endroit. À la dernière ligne, le raccourci `age` a donné deux noeuds à la même position, la clé `"age"` et la variable `age`. Et la ligne 4 commence par un `[` en début de ligne, que le parser n'a pas collé à l'expression du dessus.

## 3.8 Les instructions

Une expression calcule une valeur. Une instruction fait quelque chose : déclarer une variable, répéter un bloc, sortir d'une boucle. L'étape 3 ajoute les instructions, sauf celles qui définissent des fonctions et des structures.

### Les blocs

Commence par le bas : une suite d'instructions, et un bloc entre accolades. Ajoute ces deux déclarations en avance en haut du fichier, à côté de `parser_parse_expr`, parce que `if` et `while` appelleront `parser_parse_block` avant qu'elle soit écrite :

```c fichier="src/parser.c" lignes="20-21"
static ast_t *parser_parse_block(parser_t *parser);
static ast_t *parser_parse_expr(parser_t *parser);
```

Puis, tout en bas du fichier, à la place du `parser_parse` de l'étape 1 :

```c fichier="src/parser.c" lignes="1021-1043" focus="1033-1034"
static bool parser_is_definition(ast_t *node){
    return node->type == AST_VARIABLE_DEFINITION || node->type == AST_FUNCTION_DEFINITION ||
        node->type == AST_STRUCT_DEFINITION || node->type == AST_ENUM_DEFINITION || node->type == AST_INCLUDE;
}

/**
 * @brief les instructions, jusqu'à `}` ou la fin du fichier
 */
static ast_t *parser_parse_statments(parser_t *parser){
    ast_t *block = parser_new_node(parser,AST_COMPOUND);
    block->list_compound = newList();

    while(TYPE(parser) != TOKEN_RBRACKET && TYPE(parser) != TOKEN_EOF){
        if(parser_match(parser,TOKEN_SEMI))
            continue;

        ast_t *child = parser_parse_statment(parser);
        block->has_definition = block->has_definition || parser_is_definition(child);
        List_Append(block->list_compound,child);
    }

    return block;
}
```

On lit des instructions jusqu'à une `}` ou la fin du fichier. Les `;` sont sautés là, et seulement là : ils ne font partie d'aucune instruction, ils les séparent quand on veut en mettre deux sur une ligne. C'est toute la place qu'ils ont dans CeFlo.

`has_definition` note si le bloc déclare un nom. Ça ne change rien à l'arbre, mais l'interpréteur en profitera : un bloc qui ne déclare rien n'a pas besoin de sa propre table de variables. On calcule l'information une fois ici, plutôt qu'à chaque passage dans le bloc (chapitre 6).

```c fichier="src/parser.c" lignes="1045-1067" focus="1062-1063"
static ast_t *parser_parse_block(parser_t *parser){
    parser_check_depth(parser);

    parser_eat(parser,TOKEN_LBRACKET);
    parser->block_depth += 1;
    ast_t *block = parser_parse_statments(parser);
    parser->block_depth -= 1;
    parser_eat(parser,TOKEN_RBRACKET);

    return block;
}

ast_t *parser_parse(lexer_t *lexer){
    parser_t parser;
    newParser(&parser,lexer);

    ast_t *node = parser_parse_statments(&parser);
    if(TYPE(&parser) != TOKEN_EOF)
        parser_error_here(&parser,"`}` en trop");

    freeParser(&parser);
    return node;
}
```

Un bloc, c'est une `{`, des instructions, une `}`. Un programme, c'est des instructions jusqu'à la fin du fichier. Comme `parser_parse_statments` s'arrête aussi sur une `}`, il faut vérifier à la fin qu'on est bien arrivé au bout : sinon, il y a une accolade fermante qui ne ferme rien.

### L'affectation

Voici le deuxième endroit où la grammaire n'est pas LL(1). Une instruction peut commencer par `a.b[0]`, et on ne sait qu'*après* l'avoir lu si c'est une affectation (`a.b[0] = 1`) ou juste une expression (`a.b[0]`, un appel comme `f(x)`...). L'astuce est de ne pas chercher à savoir à l'avance :

```c fichier="src/parser.c" lignes="965-990" focus="983,989"
static bool parser_is_assign_target(ast_t *node){
    return node->type == AST_VARIABLE || node->type == AST_OBJ_ACCESS || node->type == AST_LIST_ACCESS;
}

static bool parser_get_assign_op(TokenEnum type, TokenEnum *op){
    switch(type){
        case TOKEN_EQUAL: *op = TOKEN_EQUAL; return true;
        case TOKEN_PLUS_EQUAL: *op = TOKEN_PLUS; return true;
        case TOKEN_MINUS_EQUAL: *op = TOKEN_MINUS; return true;
        case TOKEN_MULT_EQUAL: *op = TOKEN_MULT; return true;
        case TOKEN_DIV_EQUAL: *op = TOKEN_DIV; return true;
        case TOKEN_MODULO_EQUAL: *op = TOKEN_MODULO; return true;
        default: return false;
    }
}

// expression   ou   cible = expression   ou   cible += expression
static ast_t *parser_parse_assignement(parser_t *parser){
    ast_t *value = parser_parse_expr(parser);
    TokenEnum op;

    if(!parser_get_assign_op(TYPE(parser),&op))
        return value;

    if(!parser_is_assign_target(value))
        parser_error_here(parser,"on ne peut pas affecter une valeur à cette expression");
```

On lit d'abord une expression, n'importe laquelle. Si elle n'est pas suivie d'un `=` ou d'un `+=`, c'était une expression seule, et on la rend. Sinon, on vérifie *après coup* qu'elle peut recevoir une valeur : une variable, un attribut ou une case. `f() = 3` et `1 + 2 = 3` sont refusés, avec la flèche sur le `=` :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/e_affect.cf
erreur de syntaxe: on ne peut pas affecter une valeur à cette expression
  --> livre/essais/chapitres-01-04/e_affect.cf:1:7
    1 | 1 + 2 = 3
      |       ^
```

C'est exactement ce que fait *Crafting Interpreters*, et ce que font la plupart des parsers écrits à la main. L'autre solution serait de lire assez loin pour trouver le `=` avant de commencer, ce qui demande de revenir en arrière dans les jetons. Ici, rien à défaire : l'arbre de la cible est déjà le bon.

`parser_get_assign_op` a une petite idée en plus : `+=` est rangé avec l'opérateur `TOKEN_PLUS`, et `=` avec `TOKEN_EQUAL`. L'interpréteur n'aura qu'un seul noeud `AST_ASSIGNEMENT`, et pour `a += 1`, il calculera `a + 1` avec le même code que pour une addition ordinaire. La fin de la fonction construit le noeud :

```c fichier="src/parser.c" lignes="992-999"
    ast_t *assign = parser_new_node(parser,AST_ASSIGNEMENT);
    parser_advance(parser);
    assign->op = op;
    assign->assignement_left = value;
    assign->assignement_value = parser_parse_expr(parser);

    return assign;
}
```

En CeFlo, l'affectation est une instruction, pas une expression. `a = b = 1` ne marche pas, et `if x = 1 { }` non plus. En C, ce dernier est une source de bugs si célèbre que les compilateurs ont fini par avertir. CeFlo l'interdit par sa grammaire.

### Les variables

```c fichier="src/parser.c" lignes="433-470" focus="462-466"
// x::type
static char *parser_parse_static_type(parser_t *parser){
    if(TYPE(parser) == TOKEN_NONE)
        parser_error_here(parser,"`none` ne peut pas être le type d'une variable");
    return parser_eat_id(parser);
}

// var a = 1, b::int = 2    /    const K = 3
static ast_t *parser_parse_variable_definition(parser_t *parser){
    bool is_const = TYPE(parser) == TOKEN_CONST;
    parser_advance(parser);

    ast_t *first = NULL;
    ast_t *last = NULL;

    do{
        ast_t *var = parser_new_node(parser,AST_VARIABLE_DEFINITION);
        var->is_const = is_const;
        var->variable_definition_name = parser_eat_id(parser);

        if(parser_match(parser,TOKEN_DOUBLE_COLON))
            var->variable_definition_static_type_name = parser_parse_static_type(parser);

        if(parser_match(parser,TOKEN_EQUAL)){
            var->variable_definition_value = parser_parse_expr(parser);
        }else if(is_const){
            parser_error_here(parser,"une constante doit recevoir une valeur");
        }

        if(first == NULL)
            first = var;
        else
            last->left = var;
        last = var;
    }while(parser_match(parser,TOKEN_COMA));

    return first;
}
```

Le type déclaré avec `::` n'est qu'un nom ici : `int`, `list`, `Point`. Le parser ne sait pas quels types existent, il garde le nom, et c'est l'interpréteur qui le vérifiera. `none` est refusé tout de suite parce que c'est un mot réservé, pas un nom.

La boucle permet `var a = 1, b = 2`. Mais une fonction ne rend qu'un noeud, et on en a deux. Les lignes 462 à 466 les chaînent : chaque définition range la suivante dans son champ `left`. `var a, b, c` devient trois noeuds en file, et l'interpréteur suivra les `left`. C'est le seul endroit où `left` ne veut pas dire « opérande gauche ».

### if, while, for

```c fichier="src/parser.c" lignes="698-713" fonction="parser_parse_if" focus="706-709"
static ast_t *parser_parse_if(parser_t *parser){
    ast_t *ast_if = parser_new_node(parser,AST_IF);
    parser_advance(parser);

    ast_if->if_expr = parser_parse_expr(parser);
    ast_if->if_body = parser_parse_block(parser);

    if(parser_match(parser,TOKEN_ELSE)){
        if(TYPE(parser) == TOKEN_IF)
            ast_if->if_other = parser_parse_if(parser);
        else
            ast_if->else_body = parser_parse_block(parser);
    }

    return ast_if;
}
```

Pas de parenthèses autour de la condition, mais des accolades obligatoires. Ce n'est pas une question de goût : sans parenthèses, il faut un signe qui dise où finit la condition, et c'est la `{`. Go et Rust ont fait le même choix pour la même raison. Le C a fait l'inverse (parenthèses obligatoires, accolades facultatives), et il a hérité du problème du *dangling else* : dans `if(a) if(b) x(); else y();`, à quel `if` appartient le `else` ? Avec des accolades obligatoires, la question ne se pose jamais.

`else if` n'est pas un mot-clé. Après `else`, si le jeton est `if`, on lit un nouveau `if` entier et on le range dans `if_other`. Une chaîne de `else if` devient une chaîne de noeuds `AST_IF`. Et `else println(1)`, sans accolades, est refusé par `parser_parse_block`, qui exige une `{`.

```c fichier="src/parser.c" lignes="793-806" fonction="parser_parse_while" focus="799-801"
static ast_t *parser_parse_while(parser_t *parser){
    ast_t *ast_while = parser_new_node(parser,AST_WHILE);
    parser_advance(parser);

    ast_while->while_expr = parser_parse_expr(parser);

    parser->loop_depth += 1;
    ast_while->while_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_while;
}

static ast_t *parser_parse_assignement(parser_t *parser);
```

Voilà le premier des trois compteurs de `parser_t`. Pendant qu'on lit le corps d'une boucle, `loop_depth` est positif. Un `break` lu à ce moment-là est à sa place, un `break` lu ailleurs ne l'est pas :

```c fichier="src/parser.c" lignes="866-873" fonction="parser_parse_loop_jump"
static ast_t *parser_parse_loop_jump(parser_t *parser, ast_type type){
    if(parser->loop_depth == 0)
        parser_error_here(parser,"`break` et `continue` s'utilisent dans une boucle");

    ast_t *jump = parser_new_node(parser,type);
    parser_advance(parser);
    return jump;
}
```

On compte au lieu de garder un booléen, parce que les boucles s'imbriquent : en sortant d'une boucle intérieure, on doit rester « dans une boucle ». Le chapitre 4 reviendra sur ces vérifications faites pendant la lecture. La ligne 806 annonce `parser_parse_assignement`, dont la boucle `for` va avoir besoin.

La boucle `for` a deux formes, et elles commencent pareil :

```ceflo
for var i = 0; i < n; i += 1 { }
for var cle, valeur in table { }
```

Les deux commencent par `for var`, et `var i = 0` comme `var cle, valeur` sont des définitions de variables valables. On ne sait quelle boucle on lit qu'en arrivant au `;` ou au `in`. Le parser lit donc d'abord la définition, puis regarde :

```c fichier="src/parser.c" lignes="833-864" fonction="parser_parse_for" focus="842-843"
static ast_t *parser_parse_for(parser_t *parser){
    ast_t *ast_for = parser_new_node(parser,AST_FOR);
    parser_advance(parser);

    if(TYPE(parser) != TOKEN_SEMI){
        ast_for->for_init = TYPE(parser) == TOKEN_VAR ?
            parser_parse_variable_definition(parser) : parser_parse_assignement(parser);
    }

    if(TYPE(parser) == TOKEN_IN && ast_for->for_init->type == AST_VARIABLE_DEFINITION)
        return parser_parse_for_in(parser,ast_for);

    parser_eat(parser,TOKEN_SEMI);

    if(TYPE(parser) != TOKEN_SEMI)
        ast_for->for_condition = parser_parse_expr(parser);
    parser_eat(parser,TOKEN_SEMI);

    ast_for->for_solve = parser_new_node(parser,AST_COMPOUND);
    ast_for->for_solve->list_compound = newList();
    if(TYPE(parser) != TOKEN_LBRACKET){
        do{
            List_Append(ast_for->for_solve->list_compound,parser_parse_assignement(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }

    parser->loop_depth += 1;
    ast_for->for_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_for;
}
```

Chaque partie du `for` classique peut être vide : `for ; ; { }` est une boucle infinie. La troisième partie (`for_solve`, « ce qu'on fait à chaque tour ») est rangée dans un `AST_COMPOUND`, un bloc, parce qu'elle peut contenir plusieurs affectations séparées par des virgules : `i += 1, j -= 1`. Ce bloc existe même vide, ce qui évite à l'interpréteur de tester `NULL`.

À la ligne 842, si le jeton est `in`, on change de cap. La fonction `parser_parse_for_in` reprend le noeud déjà commencé. Elle vérifie d'abord que la définition lue a la bonne forme : une ou deux variables (on suit la chaîne des `left`), sans valeur. C'est encore le principe de l'affectation : lire large, puis contrôler.

```c fichier="src/parser.c" lignes="813-831" fonction="parser_parse_for_in" focus="822"
static ast_t *parser_parse_for_in(parser_t *parser, ast_t *ast_for){
    ast_t *first = ast_for->for_init;
    ast_t *second = first->left;

    if(first->variable_definition_value != NULL || (second != NULL && second->variable_definition_value != NULL))
        parser_error_here(parser,"les variables de `for ... in` ne reçoivent pas de valeur");
    if(second != NULL && second->left != NULL)
        parser_error_here(parser,"`for ... in` prend une ou deux variables");

    ast_for->type = AST_FOR_IN;
    parser_advance(parser);
    ast_for->for_condition = parser_parse_expr(parser);

    parser->loop_depth += 1;
    ast_for->for_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_for;
}
```

Le noeud a été créé `AST_FOR` sur le mot `for`, et on le transforme en `AST_FOR_IN` sans le recréer. L'expression parcourue est rangée dans `for_condition`, un nom emprunté. Place cette fonction avant `parser_parse_for`, juste après la ligne 806.

### L'aiguillage

Il manque la fonction qui choisit l'instruction. Celle du dépôt connaît toutes les instructions ; pour l'étape 3, on n'en garde que celles qu'on vient d'écrire. Place-la juste avant `parser_is_definition` :

```c fichier="livre/essais/chapitres-01-04/parser_etape3.c"
// Étape 3 : les fonctions, les structures, `match` et les modules viendront à l'étape 4.
static ast_t *parser_parse_statment(parser_t *parser){
    switch(TYPE(parser)){
        case TOKEN_VAR:
        case TOKEN_CONST: return parser_parse_variable_definition(parser);
        case TOKEN_IF: return parser_parse_if(parser);
        case TOKEN_WHILE: return parser_parse_while(parser);
        case TOKEN_FOR: return parser_parse_for(parser);
        case TOKEN_BREAK: return parser_parse_loop_jump(parser,AST_BREAK);
        case TOKEN_CONTINUE: return parser_parse_loop_jump(parser,AST_CONTINUE);
        default: return parser_parse_assignement(parser);
    }
}
```

Le premier jeton suffit encore : un mot-clé dit quelle instruction commence, et tout le reste commence par une expression, qui sera peut-être une affectation.

> [!attention]
> Si gcc dit `implicit declaration of function 'parser_parse_assignement'`, c'est la déclaration en avance de la ligne 806 qui manque : `parser_parse_for` appelle une fonction écrite beaucoup plus bas. Ce message veut presque toujours dire ça.

Compile l'étape 3 et essaie-la :

```terminal
$ cat livre/essais/chapitres-01-04/instructions.cf
var total = 0, n::int = 3
for var i = 0; i < n; i += 1 {
    if i == 1 { continue } else if i > 5 { break }
    total += i
}
for var cle, valeur in {a: 1} { println(cle) }
while total > 0 { total -= 1 }
$ livre/essais/chapitres-01-04/arbre_etape3 livre/essais/chapitres-01-04/instructions.cf
  1:1   COMPOUND
  1:5     VARIABLE_DEFINITION total
  1:13      valeur: NUMBER 0
  1:16    VARIABLE_DEFINITION n ::int
  1:25      valeur: NUMBER 3
  2:1     FOR
  2:9       départ: VARIABLE_DEFINITION i
  2:13        valeur: NUMBER 0
  2:18      condition: BINOP <
  2:16        gauche: VARIABLE i
  2:20        droite: VARIABLE n
  2:23      pas: COMPOUND
  2:25        ASSIGNEMENT +
  2:23          cible: VARIABLE i
  2:28          valeur: NUMBER 1
  3:5       corps: COMPOUND
  3:5         IF
  3:10          si: BINOP ==
  3:8             gauche: VARIABLE i
  3:13            droite: NUMBER 1
  3:17          alors: COMPOUND
  3:17            CONTINUE
  3:33          sinon si: IF
  3:38            si: BINOP >
  3:36              gauche: VARIABLE i
  3:40              droite: NUMBER 5
  3:44            alors: COMPOUND
  3:44              BREAK
  4:11        ASSIGNEMENT +
  4:5           cible: VARIABLE total
  4:14          valeur: VARIABLE i
  6:1     FOR_IN
  6:9       départ: VARIABLE_DEFINITION cle
  6:14      VARIABLE_DEFINITION valeur
  6:24      dans: MAP
  6:25        STRING "a"
  6:28        NUMBER 1
  6:33      corps: COMPOUND
  6:40        FUNCTION_CALL
  6:33          appelé: VARIABLE println
  6:41          arg: VARIABLE cle
  7:1     WHILE
  7:13      tant que: BINOP >
  7:7         gauche: VARIABLE total
  7:15        droite: NUMBER 0
  7:19      corps: COMPOUND
  7:25        ASSIGNEMENT -
  7:19          cible: VARIABLE total
  7:28          valeur: NUMBER 1
```

`total += i` est un `ASSIGNEMENT` avec l'opérateur `+`. Le `else if` est un `IF` rangé dans le premier. Les deux variables du `for ... in` sont chaînées par `left` (notre `arbre` les affiche au même niveau). Et regarde la ligne 6 : `{a: 1}` est lu comme un map, puis `{ println(cle) }` comme le bloc. Le parser n'a pas confondu les deux accolades, parce qu'à l'endroit où il attend une expression, une `{` ne peut être qu'un map, et à l'endroit où il attend un bloc, qu'un bloc.

## 3.9 Fonctions, structures, énumérations, match

L'étape 4 termine le langage. Ajoute les deux dernières déclarations en avance du dépôt, et le cas `TOKEN_LFUNC` du primaire (lignes 269 et 270) :

```c fichier="src/parser.c" lignes="18-23"
static ast_t *parser_parse_statment(parser_t *parser);
static ast_t *parser_parse_statments(parser_t *parser);
static ast_t *parser_parse_block(parser_t *parser);
static ast_t *parser_parse_expr(parser_t *parser);
static ast_t *parser_parse_factor(parser_t *parser);
static ast_t *parser_parse_function_definition(parser_t *parser);
```

### Les fonctions

CeFlo accepte quatre façons d'écrire une fonction, et le commentaire du dépôt les résume :

```c fichier="src/parser.c" lignes="496-513"
/**
 * @brief les quatre écritures de CeFlo
 *
 *      func a(x, y) : int { }      avec parenthèses
 *      func a -> x, y : int { }    avec une flèche
 *      func a { }                  sans paramètre
 *      lfunc(x) { }                sans nom : c'est une valeur
 */
static ast_t *parser_parse_function_definition(parser_t *parser){
    bool is_lfunc = TYPE(parser) == TOKEN_LFUNC;
    ast_t *function = parser_new_node(parser,is_lfunc ? AST_LFUNC : AST_FUNCTION_DEFINITION);
    parser_advance(parser);

    if(is_lfunc){
        allowChar(&function->function_definition_name,"lfunc");
    }else{
        function->function_definition_name = parser_eat_id(parser);
    }
```

Une `lfunc` est une fonction sans nom, qu'on écrit au milieu d'une expression (c'est le cas `TOKEN_LFUNC` du primaire). On lui donne quand même le nom `"lfunc"`, pour que les messages d'erreur aient quelque chose à afficher.

```c fichier="src/parser.c" lignes="515-540" focus="530-537"
    if(parser_match(parser,TOKEN_ARROW)){
        parser_parse_function_args(parser,function,TOKEN_LBRACKET);
    }else if(parser_match(parser,TOKEN_LPARENT)){
        parser_parse_function_args(parser,function,TOKEN_RPARENT);
        parser_eat(parser,TOKEN_RPARENT);
    }else{
        function->function_definition_args = newList();
    }

    if(parser_match(parser,TOKEN_COLON)){
        // `: none` veut dire que le retour n'est pas vérifié
        if(!parser_match(parser,TOKEN_NONE))
            function->function_definition_return_type_name = parser_eat_id(parser);
    }

    int loop_depth = parser->loop_depth;
    parser->loop_depth = 0;
    parser->function_depth += 1;

    function->function_definition_body = parser_parse_block(parser);

    parser->function_depth -= 1;
    parser->loop_depth = loop_depth;

    return function;
}
```

Les paramètres se lisent jusqu'à un jeton de fin qu'on passe en argument : `)` après une parenthèse, `{` après une flèche. Remarque qu'avec la flèche, on ne mange pas la `{` : c'est le bloc qui la mangera.

Les lignes 530 à 537 sont plus subtiles qu'elles n'en ont l'air. En entrant dans un corps de fonction, on met `loop_depth` à zéro, puis on le remet comme avant en sortant. Pourquoi ? Regarde :

```ceflo
while true {
    var f = lfunc() { break }
}
```

Le `break` est bien écrit à l'intérieur d'un `while`. Mais il est dans une fonction, qui sera peut-être appelée bien après la fin de la boucle, ou jamais. Il n'y a aucune boucle à arrêter. En remettant le compteur à zéro dans chaque fonction, le parser refuse ce programme. Et `function_depth` permet à `return` de savoir s'il est dans une fonction :

```c fichier="src/parser.c" lignes="681-696" fonction="parser_parse_return" focus="689-690"
static ast_t *parser_parse_return(parser_t *parser){
    if(parser->function_depth == 0)
        parser_error_here(parser,"`return` en dehors d'une fonction");

    ast_t *ast_return = parser_new_node(parser,AST_RETURN);
    parser_advance(parser);

    // Un `return` seul sur sa ligne ne retourne rien.
    bool has_value = TYPE(parser) != TOKEN_SEMI && TYPE(parser) != TOKEN_RBRACKET &&
        TYPE(parser) != TOKEN_EOF && !parser->current.newline_before;

    if(has_value)
        ast_return->return_expr = parser_parse_expr(parser);

    return ast_return;
}
```

Voilà la seconde règle du retour à la ligne. Un `return` peut être suivi d'une valeur ou de rien, et sans point-virgule, il faut décider où il s'arrête. Si le jeton suivant est sur une autre ligne, `return` ne rend rien :

```terminal
$ cat livre/essais/chapitres-01-04/e_return.cf
func f() {
    return
        42
}
println(f())
$ ./Ceflo livre/essais/chapitres-01-04/e_return.cf
none
```

Le `42` est devenu une instruction à part, qu'on calcule et qu'on jette. C'est le piège le plus connu de JavaScript, qui fait pareil. Mais là où JavaScript l'a subi, CeFlo l'a choisi : la règle est écrite en une ligne, dans `GRAMMAIRE.md` et ici.

Les paramètres eux-mêmes :

```c fichier="src/parser.c" lignes="472-494" fonction="parser_parse_function_args" focus="486-492"
// a, b::int     `self` n'est pas un vrai paramètre : l'appelant ne le passe pas
static void parser_parse_function_args(parser_t *parser, ast_t *function, TokenEnum end){
    function->function_definition_args = newList();

    if(TYPE(parser) == end)
        return;

    do{
        ast_t *arg = parser_new_node(parser,AST_VARIABLE);
        arg->variable_name = parser_eat_id(parser);

        if(parser_match(parser,TOKEN_DOUBLE_COLON))
            arg->variable_definition_static_type_name = parser_parse_static_type(parser);

        if(strcmp(arg->variable_name,"self") == 0){
            if(function->is_define_self)
                error_syntax(arg->file,arg->line,arg->colone,"`self` est déclaré plusieurs fois");
            function->is_define_self = true;
        }else{
            List_Append(function->function_definition_args,arg);
        }
    }while(parser_match(parser,TOKEN_COMA));
}
```

Un paramètre `self` n'est pas rangé dans la liste : l'appelant ne le passe jamais, c'est l'objet sur lequel on appelle la méthode. On note seulement que la fonction l'a écrit.

### Les structures

```c fichier="src/parser.c" lignes="567-591" fonction="parser_parse_struct_definition"
static ast_t *parser_parse_struct_definition(parser_t *parser){
    ast_t *structure = parser_new_node(parser,AST_STRUCT_DEFINITION);
    parser_advance(parser);
    structure->struct_definition_name = parser_eat_id(parser);
    structure->struct_definition_item = newList();

    parser_eat(parser,TOKEN_LBRACKET);
    while(!parser_match(parser,TOKEN_RBRACKET)){
        if(parser_match(parser,TOKEN_SEMI))
            continue;

        ast_t *item = NULL;
        if(TYPE(parser) == TOKEN_VAR || TYPE(parser) == TOKEN_CONST){
            item = parser_parse_variable_definition(parser);
        }else if(TYPE(parser) == TOKEN_FUNC){
            item = parser_parse_function_definition(parser);
            parser_check_struct_method(parser,structure,item);
        }else{
            parser_error_here(parser,"une structure contient seulement des `var`, des `const` et des `func`");
        }
        List_Append(structure->struct_definition_item,item);
    }

    return structure;
}
```

Une structure n'est pas un bloc : elle ne contient que des `var`, des `const` et des `func`, et on le vérifie à la lecture. Le message d'erreur dit exactement ce qui est permis, ce qui vaut mieux qu'un `` `}` attendu ``. Chaque méthode passe par `parser_check_struct_method` (lignes 542 à 565), qui refuse deux méthodes du même nom, sauf `__init__` avec des nombres de paramètres différents : c'est la seule surcharge de CeFlo.

### Les énumérations

```c fichier="src/parser.c" lignes="593-615" fonction="parser_parse_enum_value"
// A = 1, A = -1 ou A = "x". Sans `=`, c'est la valeur précédente + 1, et 0 pour la première.
static ast_t *parser_parse_enum_value(parser_t *parser, ast_t *previous){
    if(!parser_match(parser,TOKEN_EQUAL)){
        if(previous != NULL && previous->type != AST_NUMBER)
            parser_error_here(parser,"une valeur qui suit un texte s'écrit avec son `=`");

        ast_t *number = parser_new_node(parser,AST_NUMBER);
        number->number_value = previous != NULL ? previous->number_value + 1 : 0;
        return number;
    }

    if(TYPE(parser) == TOKEN_STRING)
        return parser_parse_string(parser);

    bool is_negative = parser_match(parser,TOKEN_MINUS);
    if(TYPE(parser) != TOKEN_INTEGER && TYPE(parser) != TOKEN_FLOAT)
        parser_error_expected(parser,"nombre ou chaîne");

    ast_t *number = parser_parse_number(parser);
    if(is_negative)
        number->number_value = -number->number_value;
    return number;
}
```

Une valeur d'énumération n'est pas une expression : c'est un nombre ou un texte écrit tel quel. Le parser calcule lui-même les valeurs implicites (précédente + 1), et il gère le moins à la main, parce que `-1` est un `TOKEN_MINUS` suivi d'un nombre et qu'on ne veut pas ouvrir la porte à `A = 1 + 1`. Chaque élément est un noeud `AST_NUMBER` ou `AST_STRING` qui porte son nom dans `variable_name` (lignes 641 à 651). `parser_parse_enum_definition` (lignes 654 à 675) lit la liste, avec une virgule finale permise, comme le map.

### match

```c fichier="src/parser.c" lignes="762-791" fonction="parser_parse_match" focus="782-787"
// match sujet { case a { } case b, c { } else { } }
static ast_t *parser_parse_match(parser_t *parser){
    ast_t *match = parser_new_node(parser,AST_MATCH);
    parser_advance(parser);

    match->if_expr = parser_parse_expr(parser);
    match->list_compound = newList();

    parser_eat(parser,TOKEN_LBRACKET);
    while(true){
        if(parser_match(parser,TOKEN_SEMI))
            continue;
        if(TYPE(parser) != TOKEN_CASE)
            break;
        List_Append(match->list_compound,parser_parse_case(parser,match));
    }

    if(match->list_compound->Size == 0)
        parser_error_here(parser,"un `match` contient au moins un `case`");

    if(parser_match(parser,TOKEN_ELSE)){
        match->else_body = parser_parse_block(parser);
        while(parser_match(parser,TOKEN_SEMI)){ }
        if(TYPE(parser) == TOKEN_CASE)
            parser_error_here(parser,"`else` se place après le dernier `case`");
    }
    parser_eat(parser,TOKEN_RBRACKET);

    return match;
}
```

Les lignes 782 à 787 sont un bon exemple de message pensé pour le lecteur. Après un `else`, on attend la `}` du `match`. Si on trouve un `case`, `parser_eat` dirait `` `}` attendu, mais `case` trouvé ``, ce qui est vrai mais n'aide pas. On teste ce cas précis et on dit ce qu'il faut faire. Chaque `case` est lu par `parser_parse_case` (lignes 745 à 760), qui vérifie au passage qu'aucune valeur n'est testée deux fois. Le chapitre 4 montre cette vérification en détail.

### Les modules, et l'aiguillage complet

`from ... import` et `export` (lignes 879 à 959) n'ont rien de neuf : un chemin, un mot-clé, une liste de noms avec le motif `do ... while`. Le chemin `a.b.c` est transformé en `"a/b/c.cf"` dès la lecture, avec un `buffer_t`. Et `export` vérifie `block_depth`, le troisième compteur : on ne l'accepte qu'au niveau du fichier, hors de tout bloc.

Enfin, remplace l'aiguillage de l'étape 3 par celui du dépôt :

```c fichier="src/parser.c" lignes="1001-1019" fonction="parser_parse_statment"
static ast_t *parser_parse_statment(parser_t *parser){
    switch(TYPE(parser)){
        case TOKEN_VAR:
        case TOKEN_CONST: return parser_parse_variable_definition(parser);
        case TOKEN_FUNC: return parser_parse_function_definition(parser);
        case TOKEN_STRUCT: return parser_parse_struct_definition(parser);
        case TOKEN_ENUM: return parser_parse_enum_definition(parser);
        case TOKEN_RETURN: return parser_parse_return(parser);
        case TOKEN_IF: return parser_parse_if(parser);
        case TOKEN_MATCH: return parser_parse_match(parser);
        case TOKEN_WHILE: return parser_parse_while(parser);
        case TOKEN_FOR: return parser_parse_for(parser);
        case TOKEN_BREAK: return parser_parse_loop_jump(parser,AST_BREAK);
        case TOKEN_CONTINUE: return parser_parse_loop_jump(parser,AST_CONTINUE);
        case TOKEN_FROM: return parser_parse_include(parser);
        case TOKEN_EXPORT: return parser_parse_export(parser);
        default: return parser_parse_assignement(parser);
    }
}
```

Avec `parser_parse_one` (lignes 1069 à 1079), qui lit une seule expression pour les `${...}` de `format`, ton fichier est complet. Compare-le à celui du dépôt :

```terminal
$ diff livre/essais/chapitres-01-04/parser.c src/parser.c
```

Rien ne doit s'afficher. Si quelque chose sort, c'est presque toujours une fonction placée dans un autre ordre : remets-la à sa place et recommence.

```terminal
$ cat livre/essais/chapitres-01-04/definitions.cf
enum Feu { ROUGE, ORANGE = 5, VERT }
struct Point {
    var x::int = 0
    func avance -> n : Point { x += n; return self }
}
var f = lfunc(a) { return a * 2 }
match Feu.VERT {
    case Feu.ROUGE, Feu.ORANGE { println("stop") }
    else { println("roule") }
}
$ livre/essais/chapitres-01-04/arbre livre/essais/chapitres-01-04/definitions.cf
  1:1   COMPOUND
  1:1     ENUM_DEFINITION Feu
  1:12      NUMBER ROUGE = 0
  1:19      NUMBER ORANGE = 5
  1:31      NUMBER VERT = 6
  2:1     STRUCT_DEFINITION Point
  3:9       VARIABLE_DEFINITION x ::int
  3:18        valeur: NUMBER 0
  4:5       FUNCTION_DEFINITION avance : Point
  4:20        param: VARIABLE n
  4:32        corps: COMPOUND
  4:34          ASSIGNEMENT +
  4:32            cible: VARIABLE x
  4:37            valeur: VARIABLE n
  4:40          RETURN
  4:47            VARIABLE self
  6:5     VARIABLE_DEFINITION f
  6:9       valeur: LFUNC lfunc
  6:15        param: VARIABLE a
  6:20        corps: COMPOUND
  6:20          RETURN
  6:29            BINOP *
  6:27              gauche: VARIABLE a
  6:31              droite: NUMBER 2
  7:1     MATCH
  7:11      sujet: OBJ_ACCESS VERT
  7:7         gauche: VARIABLE Feu
  8:5       CASE
  8:14        valeur: OBJ_ACCESS ROUGE
  8:10          gauche: VARIABLE Feu
  8:25        valeur: OBJ_ACCESS ORANGE
  8:21          gauche: VARIABLE Feu
  8:34        bloc: COMPOUND
  8:41          FUNCTION_CALL
  8:34            appelé: VARIABLE println
  8:42            arg: STRING "stop"
  9:12      sinon: COMPOUND
  9:19        FUNCTION_CALL
  9:12          appelé: VARIABLE println
  9:20          arg: STRING "roule"
```

`VERT` vaut 6 : le parser a calculé `5 + 1`. `self` est une variable ordinaire dans le corps, et le `;` entre `x += n` et `return self` a simplement séparé deux instructions. `Feu.VERT` est un accès à l'attribut `VERT` de la variable `Feu` : le parser ne sait pas que `Feu` est une énumération, et n'a pas besoin de le savoir.

## 3.10 Les erreurs qui pointent le bon jeton

Un parser qui accepte les bons programmes, c'est la moitié du travail. L'autre moitié, c'est ce qu'il dit des mauvais. Tous les messages de CeFlo pointent le jeton courant, celui où le parser a compris que quelque chose n'allait pas. La plupart du temps, c'est le bon endroit :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/e_case.cf
erreur de syntaxe: cette valeur est déjà testée à la ligne 2
  --> livre/essais/chapitres-01-04/e_case.cf:3:13
    3 |     case 2, 1 { }
      |             ^
```

Mais pas toujours. Quand quelque chose *manque*, le parser ne s'en rend compte qu'au jeton suivant, qui peut être sur une autre ligne :

```terminal
$ cat livre/essais/chapitres-01-04/e_paren.cf
println(1 + 2
var x = 3
$ ./Ceflo livre/essais/chapitres-01-04/e_paren.cf
erreur de syntaxe: `)` attendu, mais `var` trouvé
  --> livre/essais/chapitres-01-04/e_paren.cf:2:1
    2 | var x = 3
      | ^
```

La parenthèse manque à la fin de la ligne 1, et la flèche pointe le début de la ligne 2. Le message est vrai, et un lecteur attentif comprend. Même chose pour une constante sans valeur, où le parser ne voit l'oubli qu'en tombant sur l'instruction suivante :

```terminal
$ cat livre/essais/chapitres-01-04/e_const.cf
const K
println(K)
$ ./Ceflo livre/essais/chapitres-01-04/e_const.cf
erreur de syntaxe: une constante doit recevoir une valeur
  --> livre/essais/chapitres-01-04/e_const.cf:2:1
    2 | println(K)
      | ^
```

La flèche sous `println` est trompeuse : c'est la ligne 1 qu'il faut corriger. Le parser a pourtant tout ce qu'il faut pour mieux faire. `parser->prev` est le dernier jeton lu, `K`, avec sa ligne et sa colonne. Pointer juste après lui, quand le jeton courant commence une nouvelle ligne, donnerait la bonne place. C'est le deuxième défi de ce chapitre.

### Les programmes démesurés

Il reste à vérifier la promesse de `parser_check_depth`. On fabrique deux fichiers absurdes avec `awk`, comme le fait `tests/run.sh` :

```terminal
$ awk 'BEGIN { printf "var a = "; for(i = 0; i < 300000; i++) printf "("; print "" }' > livre/essais/chapitres-01-04/parens.cf
$ ./Ceflo livre/essais/chapitres-01-04/parens.cf 2>&1 | head -2
erreur de syntaxe: code trop profondément imbriqué
  --> livre/essais/chapitres-01-04/parens.cf:1:119377
```

Trois cent mille parenthèses ouvrantes. Le parser s'est arrêté à la 119 369e, proprement, avant que la pile ne déborde. Sans ce test, c'était un *segmentation fault*.

```terminal
$ awk 'BEGIN { printf "println(1"; for(i = 0; i < 500000; i++) printf "+1"; print ")" }' > livre/essais/chapitres-01-04/somme.cf
$ ./Ceflo livre/essais/chapitres-01-04/somme.cf 2>&1 | head -2
erreur: trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais
  --> livre/essais/chapitres-01-04/somme.cf:1:508572
```

Cinq cent mille additions. Cette fois, le message ne commence pas par « erreur de syntaxe » : le parser a lu tout le fichier sans problème, grâce à la boucle de `parser_parse_expression` qui ne consomme pas de pile. C'est le compilateur de la VM, qui parcourt l'arbre récursivement, qui a dû s'arrêter. L'arbre de cette somme est un peigne de cinq cent mille noeuds penché à gauche, et tous ceux qui le parcourent récursivement doivent se protéger.

> [!attention]
> Le programme `arbre` de la section suivante appelle `ceflo_stack_init()` avant de lire. Sans cette ligne, `ceflo_stack_exceeded` compare l'adresse de la pile à zéro, croit la pile pleine, et le tout premier facteur s'arrête : `code trop profondément imbriqué` sur `var x = 12`. L'essai est dans `livre/essais/chapitres-01-04/arbre_sans_pile.c`, et le message envoie chercher au mauvais endroit : rien n'est imbriqué dans `var x = 12`.

## 3.11 Afficher l'arbre

Le programme qui a produit toutes les sorties de ce chapitre tient en 166 lignes. Il parcourt l'arbre récursivement et affiche une ligne par noeud : sa position, son rôle chez son parent, sa sorte, et ce qu'il contient en propre. Il commence par les noms des sortes de noeuds, dans l'ordre de l'`enum` de `ast.h` :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
// Affiche l'arbre qu'un fichier CeFlo devient une fois passé dans le parser.
#include <stdio.h>

#include "parser.h"
#include "error.h"

static const char *NAMES[] = {
    "NOOP", "COMPOUND", "NONE", "BOOL", "CHAR", "NUMBER", "STRING", "LIST", "MAP",
    "LFUNC", "VARIABLE", "OBJ_ACCESS", "LIST_ACCESS", "FUNCTION_CALL", "STRUCT",
    "TYPE_CALL", "BINOP", "UNOP", "TERNARY", "VARIABLE_DEFINITION",
    "FUNCTION_DEFINITION", "STRUCT_DEFINITION", "ENUM_DEFINITION", "ASSIGNEMENT",
    "RETURN", "BREAK", "CONTINUE", "IF", "MATCH", "CASE", "WHILE", "FOR", "FOR_IN",
    "INCLUDE", "EXPORT",
};

static void print_node(const char *role, ast_t *node, int depth);

static void print_list(const char *role, list_t *list, int depth){
    if(list == NULL)
        return;
    for(int i = 0; i < list->Size; i++)
        print_node(role,List_Get(list,i),depth);
}
```

Ici, pas de X-macro : `ast.h` déclare ses types dans un `enum` ordinaire, et il faut recopier la liste à la main. C'est exactement le problème que `FOREACH_TOKEN` évite pour les jetons. Si un jour `ast.h` gagne une sorte de noeud, ce tableau sera décalé.

La ligne d'un noeud :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
// Une ligne par noeud : son rôle chez son parent, sa sorte, et ce qu'il contient en propre.
static void print_line(const char *role, ast_t *node, int depth){
    printf("%3d:%-3d %*s",node->line,node->colone,depth * 2,"");
    if(role != NULL)
        printf("%s: ",role);
    printf("%s",NAMES[node->type]);

    // Un élément d'énumération est un NUMBER ou un STRING qui porte un nom.
    if((node->type == AST_NUMBER || node->type == AST_STRING) && node->variable_name != NULL)
        printf(" %s =",node->variable_name);
```

`%*s` avec une chaîne vide affiche autant d'espaces que l'argument d'avant : deux par niveau de profondeur. La suite affiche, selon la sorte, la valeur, le nom ou l'opérateur :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
    switch(node->type){
        case AST_NUMBER: printf(" %g",node->number_value); break;
        case AST_STRING: printf(" \"%s\"",node->string_value); break;
        case AST_CHAR: printf(" '%c'",node->char_value); break;
        case AST_BOOL: printf(" %s",node->bool_value ? "true" : "false"); break;
        case AST_VARIABLE:
        case AST_OBJ_ACCESS:
        case AST_INCLUDE: printf(" %s",node->variable_name); break;
        case AST_BINOP:
        case AST_UNOP:
        case AST_ASSIGNEMENT: printf(" %s",token_to_string(node->op)); break;
        case AST_VARIABLE_DEFINITION:
            printf(" %s%s",node->is_const ? "const " : "",node->variable_definition_name);
            break;
        case AST_FUNCTION_DEFINITION:
        case AST_LFUNC: printf(" %s",node->function_definition_name); break;
        case AST_STRUCT:
        case AST_TYPE_CALL: printf(" %s",node->function_call_name); break;
        case AST_STRUCT_DEFINITION:
        case AST_ENUM_DEFINITION: printf(" %s",node->struct_definition_name); break;
        default: break;
    }
    if(node->variable_definition_static_type_name != NULL)
        printf(" ::%s",node->variable_definition_static_type_name);
    if(node->function_definition_return_type_name != NULL)
        printf(" : %s",node->function_definition_return_type_name);
    putchar('\n');
}
```

Pour un opérateur, `token_to_string` donne son texte : le parser a rangé le `TokenEnum` dans `op`, et la X-macro le rend lisible. Les deux derniers `if` marchent sur tous les noeuds parce que `calloc` a mis à `NULL` tous les champs qu'on n'a pas remplis.

Le parcours lui-même, premier morceau :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
static void print_node(const char *role, ast_t *node, int depth){
    if(node == NULL)
        return;
    print_line(role,node,depth);
    depth += 1;

    switch(node->type){
        case AST_VARIABLE_DEFINITION:
            print_node("valeur",node->variable_definition_value,depth);
            // var a, b : les suivantes sont accrochées à `left`, au même niveau.
            print_node(NULL,node->left,depth - 1);
            return;
        case AST_FUNCTION_DEFINITION:
        case AST_LFUNC:
            print_list("param",node->function_definition_args,depth);
            print_node("corps",node->function_definition_body,depth);
            return;
        case AST_STRUCT_DEFINITION:
        case AST_ENUM_DEFINITION:
            print_list(NULL,node->struct_definition_item,depth);
            return;
        case AST_FUNCTION_CALL:
            print_node("appelé",node->left,depth);
            print_list("arg",node->function_call_args,depth);
            return;
        case AST_STRUCT:
        case AST_TYPE_CALL:
            print_list("arg",node->function_call_args,depth);
            return;
```

Le test `node == NULL` au début permet d'appeler `print_node` sur un champ facultatif sans vérifier : un `if` sans `else` a un `else_body` à `NULL`, et rien ne s'affiche. Deuxième morceau :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
        case AST_OBJ_ACCESS:
        case AST_UNOP:
        case AST_BINOP:
            print_node("gauche",node->left,depth);
            print_node("droite",node->right,depth);
            return;
        case AST_LIST_ACCESS:
            print_node("liste",node->left,depth);
            print_node("index",node->list_ptr,depth);
            return;
        case AST_ASSIGNEMENT:
            print_node("cible",node->assignement_left,depth);
            print_node("valeur",node->assignement_value,depth);
            return;
        case AST_RETURN:
            print_node(NULL,node->return_expr,depth);
            return;
        case AST_IF:
        case AST_TERNARY:
            print_node("si",node->if_expr,depth);
            print_node("alors",node->type == AST_IF ? node->if_body : node->if_other,depth);
            if(node->type == AST_IF)
                print_node("sinon si",node->if_other,depth);
            print_node("sinon",node->else_body,depth);
            return;
```

Le ternaire range son « alors » dans `if_other`, alors que le `if` y range son `else if`. Notre programme doit connaître ce genre de détail pour chaque sorte de noeud, et le chapitre 4 dira ce que ça coûte. Dernier morceau :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
        case AST_MATCH:
            print_node("sujet",node->if_expr,depth);
            print_list(NULL,node->list_compound,depth);
            print_node("sinon",node->else_body,depth);
            return;
        case AST_CASE:
            print_list("valeur",node->list_compound,depth);
            print_node("bloc",node->if_body,depth);
            return;
        case AST_WHILE:
            print_node("tant que",node->while_expr,depth);
            print_node("corps",node->while_body,depth);
            return;
        case AST_FOR:
        case AST_FOR_IN:
            print_node("départ",node->for_init,depth);
            print_node(node->type == AST_FOR ? "condition" : "dans",node->for_condition,depth);
            print_node("pas",node->for_solve,depth);
            print_node("corps",node->for_body,depth);
            return;
        default:
            print_list(NULL,node->list_compound,depth);
            return;
    }
}
```

Le `default` couvre tous les noeuds qui rangent leurs enfants dans `list_compound` : les blocs, les listes, les maps, les imports. Et le `main` :

```c fichier="livre/essais/chapitres-01-04/arbre.c"
int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr,"utilisation : %s fichier.cf\n",argv[0]);
        return 2;
    }

    // Le parser surveille la pile C : il doit savoir où elle commence.
    ceflo_stack_init();

    lexer_t lexer;
    if(!newLexer(&lexer,argv[1])){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    ast_t *program = parser_parse(&lexer);
    print_node(NULL,program,0);

    clearAst();
    error_free_sources();
    return 0;
}
```

`clearAst` libère tous les noeuds d'un coup, et `error_free_sources` le texte du fichier. Compilé avec le parser du dépôt, cette fois :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -o livre/essais/chapitres-01-04/arbre livre/essais/chapitres-01-04/arbre.c src/parser.c src/ast.c src/lexer.c src/token.c src/error.c src/str.c src/list.c src/common.c
$ livre/essais/chapitres-01-04/arbre livre/essais/chapitres-01-04/fib.cf
  1:1   COMPOUND
  1:1     FUNCTION_DEFINITION fib
  1:10      param: VARIABLE n
  2:5       corps: COMPOUND
  2:5         IF
  2:10          si: BINOP <
  2:8             gauche: VARIABLE n
  2:12            droite: NUMBER 2
  2:16          alors: COMPOUND
  2:16            RETURN
  2:23              VARIABLE n
  3:5         RETURN
  3:23          BINOP +
  3:15            gauche: FUNCTION_CALL
  3:12              appelé: VARIABLE fib
  3:18              arg: BINOP -
  3:16                gauche: VARIABLE n
  3:20                droite: NUMBER 1
  3:28            droite: FUNCTION_CALL
  3:25              appelé: VARIABLE fib
  3:31              arg: BINOP -
  3:29                gauche: VARIABLE n
  3:33                droite: NUMBER 2
  5:8     FUNCTION_CALL
  5:1       appelé: VARIABLE println
  5:12      arg: FUNCTION_CALL
  5:9         appelé: VARIABLE fib
  5:13        arg: NUMBER 30
```

C'est le programme du chapitre 1, celui qui fait ses 2 692 537 appels. Tout ce que l'interpréteur saura jamais de `fib` est dans ces vingt-huit lignes.

## Défis

1. **La puissance.** Si tu as ajouté le jeton `**` au chapitre 2, fais-le lire au parser. Il doit passer avant `*` et après le moins d'un facteur, et être associatif à *droite* : `2 ** 3 ** 2` vaut `2 ** 9`. Il faut un étage de plus entre `parser_parse_term` et `parser_parse_factor`, et la récursion de `parser_parse_expr` (le ternaire) te montre comment obtenir l'associativité à droite. Vérifie avec `arbre` que le noeud du second `**` est l'enfant droit du premier.

2. **La flèche au bon endroit.** Quand le jeton courant commence une nouvelle ligne, fais pointer `parser_error_expected` et `parser_error_here` juste après la fin du jeton précédent (`parser->prev`). Pour connaître sa fin, il faut sa longueur : `strlen(prev.value)` pour un nom, mais attention aux chaînes, dont le texte a été traduit. Le plus simple est d'ajouter au lexer la colonne de fin de chaque jeton. Essaie sur `e_paren.cf` et `e_const.cf`.

3. **La virgule finale.** Rends le langage cohérent : accepte une virgule finale dans une liste, dans les arguments d'un appel, ou dans aucun des deux. Il faut toucher `parser_parse_list` et `parser_parse_call_args`, et ajouter un test dans `tests/`. Lance `make test` et `make diff` : les deux interpréteurs partagent le parser, ils doivent rester d'accord.

4. **Le moins en début de ligne.** Fais refuser `var b = a` suivi d'une ligne `-1`, sans casser l'écriture d'un calcul sur plusieurs lignes quand l'opérateur est en fin de ligne. Une piste : dans `parser_parse_expression`, refuser un `-` qui commence une ligne *quand le jeton d'après est collé à lui*. Est-ce une bonne règle ? Écris trois programmes qui la mettent en difficulté avant de décider.

## Note de conception : écrire son parser à la main

On entend souvent qu'écrire un parser à la main est une perte de temps, que des outils le font mieux depuis cinquante ans. Ce chapitre montre le contraire, et je crois que c'est la bonne leçon à en tirer.

Le parser de CeFlo fait mille lignes, et chacune se lit. On a vu qu'une règle de grammaire devient une fonction de dix lignes, toujours la même forme. On a vu que les deux endroits où la grammaire n'est pas simple (le raccourci du map, l'affectation) se règlent par un regard un jeton plus loin, ou en lisant d'abord et en vérifiant ensuite. Aucun outil n'aurait rendu ça plus court. Il l'aurait rendu plus opaque.

Et surtout, un générateur aurait produit d'autres messages. « une structure contient seulement des `var`, des `const` et des `func` », « `else` se place après le dernier `case` », « les variables de `for ... in` ne reçoivent pas de valeur » : aucun de ces messages ne sort d'une table d'états. Ils sortent de quelqu'un qui s'est demandé, à cet endroit précis du code, quelle erreur un humain avait le plus de chances de faire. C'est la raison pour laquelle GCC a abandonné bison, et c'est la raison pour laquelle presque tous les langages qui comptent ont aujourd'hui un parser écrit à la main.

Le prix existe, il faut le dire. Une grammaire écrite à la main peut devenir ambiguë sans qu'on le remarque : le moins en début de ligne en est un exemple, et un générateur l'aurait peut-être signalé comme un conflit. La seule défense, c'est d'avoir une grammaire écrite à côté du code (`GRAMMAIRE.md`), et des tests qui couvrent les cas limites (`tests/erreurs/` en contient 153).

L'autre limite, c'est que CeFlo s'arrête à la première erreur. Un compilateur sérieux continue : il saute jusqu'au prochain endroit sûr (une `}`, un mot-clé d'instruction), et il cherche les erreurs suivantes. On appelle ça la *récupération sur erreur*, et c'est ce qui permet à ton éditeur de souligner trois fautes à la fois. Pour un langage de script dont les programmes se relancent en une fraction de seconde, s'arrêter à la première erreur est un choix raisonnable. Il rend le code beaucoup plus simple : jamais d'arbre à moitié faux à manipuler, jamais de jeton manquant à inventer.

---

Le parser rend un arbre. On l'a affiché, on a vu ses noeuds, leurs positions, leurs champs qui changent de sens d'une sorte à l'autre (`left` qui chaîne des variables, `if_other` qui porte l'« alors » d'un ternaire). Mais on ne s'est pas encore demandé ce qu'est vraiment un noeud en mémoire, combien il coûte, qui le libère, ni pourquoi tous ces noeuds partagent une seule et même structure de quarante-six champs. C'est le sujet du chapitre 4, et c'est là qu'on comprendra pourquoi cet arbre, une fois construit, ne doit plus jamais changer.
