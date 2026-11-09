---
numero: 2
titre: le lexer
note: on découpe le texte
partie: Lire le texte
---

À la fin du chapitre 1, on avait un texte en mémoire. Regarde ce que ce texte sera devenu à la fin de celui-ci :

```terminal
$ cat livre/essais/chapitres-01-04/douze.cf
var x = 12
println(x + 1)
$ livre/essais/chapitres-01-04/jetons livre/essais/chapitres-01-04/douze.cf
  1:1     TOKEN_VAR          var
  1:5     TOKEN_ID           x
  1:7     TOKEN_EQUAL        =
  1:9     TOKEN_INTEGER      12
  2:1   | TOKEN_ID           println
  2:8     TOKEN_LPARENT      (
  2:9     TOKEN_ID           x
  2:11    TOKEN_PLUS         +
  2:13    TOKEN_INTEGER      1
  2:14    TOKEN_RPARENT      )
  3:1   | TOKEN_EOF          fin de fichier
```

Vingt-six octets sont devenus onze **jetons**. Chacun a une sorte (`TOKEN_VAR`, `TOKEN_ID`...), un texte, une ligne et une colonne. Les espaces ont disparu. Le `|` devant `println` dit qu'un retour à la ligne le précède : retiens-le, c'est lui qui remplacera les points-virgules. Et le dernier jeton, `TOKEN_EOF`, n'existe pas dans le fichier : le lexer l'invente pour dire « c'est fini », et le parser s'en servira pour savoir quand s'arrêter.

Ce travail s'appelle l'**analyse lexicale**, et le programme qui le fait, le **lexer** (on dit aussi *scanner* ou *tokenizer*). Il tient dans `src/lexer.c`, 339 lignes, et on va les écrire toutes, dans l'ordre où on les écrirait si on partait de rien.

## 2.1 Ce qu'est un jeton

Un jeton, dans CeFlo, c'est cette structure :

```c fichier="src/include/token.h" lignes="83-90"
typedef struct{
    TokenEnum type;
    char *value;           // texte du jeton (alloué seulement si token_owns_value)
    int lenght;            // taille de value (une chaîne peut contenir \0)
    int line;
    int colone;
    bool newline_before;   // le jeton commence une nouvelle ligne
}token_t;
```

Elle tient dans 32 octets et se passe par valeur, comme un `int` un peu gros. Le lexer n'en fabrique jamais de tableau : le parser demande le jeton suivant quand il en a besoin, et il n'en garde que deux à la fois. Un fichier de dix mille lignes ne coûte donc pas dix mille jetons en mémoire.

C'est un choix, et il y en a d'autres. Beaucoup de compilateurs découpent tout le fichier d'abord, rangent les jetons dans un tableau, et donnent ce tableau au parser. C'est plus simple à déboguer (on peut afficher tous les jetons d'un coup), et le parser peut regarder aussi loin qu'il veut en avant ou en arrière. Le lexer de CPython, celui de Lua et celui de *Crafting Interpreters* travaillent comme CeFlo, à la demande. On verra au chapitre 3 que le parser de CeFlo n'a jamais besoin de regarder plus d'un jeton en avance, et le tableau ne lui apporterait rien.

### La liste des sortes

`TokenEnum` a 63 valeurs. On pourrait les écrire dans un `enum`, puis réécrire leurs noms dans un tableau de chaînes pour les messages d'erreur, et passer sa vie à garder les deux dans le même ordre. C'est ce que fait Lua : son fichier `llex.h` déclare l'`enum`, `llex.c` déclare le tableau des noms, et un commentaire prévient qu'il faut chercher « ORDER RESERVED » avant de toucher à l'un des deux. CeFlo écrit la liste une seule fois, dans une macro :

```c fichier="src/include/token.h" lignes="12-20"
#define FOREACH_TOKEN(TOKEN)                 \
    TOKEN(TOKEN_ID, "identifiant")           \
    TOKEN(TOKEN_INTEGER, "nombre")           \
    TOKEN(TOKEN_FLOAT, "nombre")             \
    TOKEN(TOKEN_CHAR, "caractère")           \
    TOKEN(TOKEN_STRING, "chaîne")            \
    TOKEN(TOKEN_FOR, "for")                  \
    TOKEN(TOKEN_IN, "in")                    \
    TOKEN(TOKEN_WHILE, "while")              \
```

La liste continue jusqu'à la ligne 75, avec un `TOKEN(nom, texte)` par sorte : les mots réservés, puis les symboles. Chaque ligne finit par une barre oblique inverse, qui dit au préprocesseur que la macro continue à la ligne suivante.

`FOREACH_TOKEN` prend en paramètre une autre macro, et l'applique à chaque ligne. Il suffit donc de dire ce qu'on veut faire d'une ligne :

```c fichier="src/include/token.h" lignes="75-81"
    TOKEN(TOKEN_EOF, "fin de fichier")

#define GENERATE_TOKEN_ENUM(TOKEN, TEXT) TOKEN,

typedef enum{
    FOREACH_TOKEN(GENERATE_TOKEN_ENUM)
}TokenEnum;
```

Pour l'`enum`, on garde le nom et on jette le texte : le préprocesseur remplace `FOREACH_TOKEN(GENERATE_TOKEN_ENUM)` par `TOKEN_ID, TOKEN_INTEGER, TOKEN_FLOAT, ...`. Dans `src/token.c`, on fait l'inverse :

```c fichier="src/token.c" lignes="11-23"
#define GENERATE_TOKEN_STRING(TOKEN, TEXT) TEXT,

static const char *TOKEN_TO_STRING[] = {
    FOREACH_TOKEN(GENERATE_TOKEN_STRING)
};

const char *token_to_string(TokenEnum type){
    return TOKEN_TO_STRING[type];
}

bool token_owns_value(TokenEnum type){
    return type <= TOKEN_STRING;
}
```

On garde le texte : `"identifiant", "nombre", ...`. Les deux listes sont produites par la même macro, elles ne peuvent pas se décaler. `TOKEN_TO_STRING[TOKEN_PLUS]` est toujours `"+"`, puisque `TOKEN_PLUS` est le rang de la ligne `TOKEN(TOKEN_PLUS, "+")` dans la liste. Ce truc a un nom, les *X-macros*, et il sert partout où une liste doit exister sous plusieurs formes. On s'en servira une troisième fois à la fin du chapitre.

Si tu veux voir ce que produit le préprocesseur, `gcc -E` s'arrête juste après lui :

```terminal
$ gcc -E -Isrc/include src/token.c | grep -A2 "TOKEN_TO_STRING\[\] ="
static const char *TOKEN_TO_STRING[] = {
    "identifiant", "nombre", "nombre", "caractère", "chaîne", "for", "in", "while", "if", "else", "match", "case", "var", "const", "func", "lfunc", "return", "import", "from", "export", "struct", "enum", "true", "false", "none", "or", "and", "new", "continue", "break", "@", "!", "!=", "->", ":", "::", ".", ";", "?", ",", "(", ")", "{", "}", "[", "]", "=", "==", "+", "+=", "-", "-=", "*", "*=", "/", "/=", "%", "%=", ">", ">=", "<", "<=", "fin de fichier",
};
```

C'est ce texte qu'affichent les erreurs : `` `}` attendu, mais `fin de fichier` trouvé ``. Pour un mot réservé ou un symbole, c'est aussi le texte du jeton lui-même : le jeton `TOKEN_PLUS` a pour `value` la chaîne `"+"` du tableau, sans aucune copie.

### Qui libère quoi

Seuls cinq jetons ont un texte qui change d'une fois à l'autre : un nom, un entier, un décimal, un caractère, une chaîne. Pour eux, le lexer alloue une copie, et il faudra la libérer. Pour les 58 autres, `value` pointe dans `TOKEN_TO_STRING`, et un `free` dessus ferait planter le programme.

`token_owns_value` fait la différence avec une seule comparaison : `type <= TOKEN_STRING`. Ça marche parce que les cinq sortes qui possèdent leur texte sont les cinq premières de la liste. `freeToken` s'en sert :

```c fichier="src/token.c" lignes="25-29"
void freeToken(token_t *token){
    if(token_owns_value(token->type))
        free(token->value);
    token->value = NULL;
}
```

La dernière ligne met `value` à `NULL` après le `free`. Si quelqu'un libère deux fois le même jeton, le second `free(NULL)` ne fera rien, au lieu de corrompre la mémoire.

> [!attention]
> L'ordre de `FOREACH_TOKEN` fait donc partie du code. Le jour où tu ajoutes un jeton, mets-le après `TOKEN_STRING`. Si tu l'ajoutes en tête, `freeToken` appellera `free` sur une chaîne du tableau, et le programme plantera loin de l'endroit où tu as fait l'erreur.

Ces deux fichiers, `token.h` et `token.c`, sont les premiers à écrire. Ils ne dépendent que de `common.h` (pour `bool`), et se compilent seuls :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -c src/token.c -o livre/essais/chapitres-01-04/token.o
```

## 2.2 Le banc d'essai

Avant d'écrire le lexer, on écrit le programme qui le testera. C'est une habitude à prendre pour tout le livre : un petit outil qui montre ce que fait le code vaut mieux que cent `printf` semés dans le code lui-même. Celui-ci ouvre un fichier, demande les jetons un par un, et les affiche jusqu'à `TOKEN_EOF`. C'est le programme `jetons` du début du chapitre.

Il n'a besoin que de l'interface du lexer, déclarée dans `src/include/lexer.h` :

```c fichier="src/include/lexer.h" lignes="13-28"
typedef struct{
    const char *fileName;
    const char *fileCode;
    size_t lenght_file_code;
    int index;
    char current_char;
    int line;
    int colone;
}lexer_t;

// Retourne false si le fichier ne peut pas être lu.
bool newLexer(lexer_t *lexer, const char *path);
void newLexerWithString(lexer_t *lexer, const char *name, const char *source);

void lexer_advance(lexer_t *lexer);
token_t lexer_get_next_token(lexer_t *lexer);
```

Le lexer garde sa position dans le texte, et rien d'autre. `index` est l'octet où l'on est, `current_char` l'octet lui-même (on le lit si souvent qu'on le garde à part), `line` et `colone` la position qu'on donnera au prochain jeton. `fileName` est le nom stable rendu par `error_add_source` au chapitre 1. Et deux fonctions comptent pour l'extérieur : `newLexer` pour ouvrir un fichier, `lexer_get_next_token` pour avancer d'un jeton.

Pour afficher le nom d'une sorte (`TOKEN_VAR` plutôt que `var`), on reprend `FOREACH_TOKEN` une troisième fois. L'opérateur `#` du préprocesseur transforme un argument de macro en chaîne : `#TOKEN` devient `"TOKEN_VAR"`.

```c fichier="livre/essais/chapitres-01-04/jetons.c"
// Affiche les jetons d'un fichier CeFlo, un par ligne.
#include <stdio.h>
#include <string.h>

#include "lexer.h"

// La même liste que l'enum, mais avec les noms : #TOKEN fait de TOKEN_ID la chaîne "TOKEN_ID".
#define GENERATE_TOKEN_NAME(TOKEN, TEXT) #TOKEN,
static const char *TOKEN_NAMES[] = {
    FOREACH_TOKEN(GENERATE_TOKEN_NAME)
};

// Affiche le texte d'un jeton, en rendant visibles les \n d'une chaîne.
static void print_value(const token_t *token){
    int lenght = token->type == TOKEN_STRING || token->type == TOKEN_CHAR ?
        token->lenght : (int)strlen(token->value);
    for(int i = 0; i < lenght; i++){
        char c = token->value[i];
        if(c == '\n') printf("\\n");
        else if(c == '\t') printf("\\t");
        else if(c == '\0') printf("\\0");
        else putchar(c);
    }
}
```

`print_value` se sert du champ `lenght` pour une chaîne et un caractère, et de `strlen` pour le reste. On verra pourquoi à la section 2.8 : une chaîne CeFlo peut contenir un octet nul. La boucle principale :

```c fichier="livre/essais/chapitres-01-04/jetons.c"
int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr,"utilisation : %s fichier.cf\n",argv[0]);
        return 2;
    }

    lexer_t lexer;
    if(!newLexer(&lexer,argv[1])){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    while(true){
        token_t token = lexer_get_next_token(&lexer);
        printf("%3d:%-3d %s %-18s ",token.line,token.colone,
            token.newline_before ? "|" : " ",TOKEN_NAMES[token.type]);
        print_value(&token);
        putchar('\n');

        bool end = token.type == TOKEN_EOF;
        freeToken(&token);
        if(end)
            break;
    }
    return 0;
}
```

Chaque jeton est libéré dès qu'il est affiché, avec `freeToken`, qui ne libère que ce qui doit l'être. On note `end` avant de libérer, par habitude : on ne touche plus à un jeton une fois libéré, même à un champ qui reste lisible.

Ce programme ne compilera pas tant que le lexer n'existe pas. On va l'écrire dans `livre/essais/chapitres-01-04/lexer.c`, en trois étapes, et compiler `jetons` à chaque étape avec la même commande :

```sh
gcc -std=c11 -Wall -Wextra -Isrc -Isrc/include -o livre/essais/chapitres-01-04/jetons livre/essais/chapitres-01-04/jetons.c livre/essais/chapitres-01-04/lexer.c src/token.c src/error.c src/str.c src/common.c
```

Le `-Isrc` permet aux `#include "include/lexer.h"` du lexer de trouver leurs fichiers depuis `livre/essais/chapitres-01-04/`. À la fin du chapitre, ton `lexer.c` sera identique à celui du dépôt.

## 2.3 Avancer dans le texte

Le lexer commence par son en-tête et l'initialisation :

```c fichier="src/lexer.c" lignes="8-25" fonction="lexer_init" focus="18"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "include/lexer.h"
#include "include/error.h"
#include "include/str.h"

static void lexer_init(lexer_t *lexer, const char *name, char *code){
    lexer->fileName = error_add_source(name,code);
    lexer->fileCode = code;
    lexer->lenght_file_code = strlen(code);
    lexer->line = 1;
    lexer->colone = 1;
    lexer->index = 0;
    lexer->current_char = code[0];
}
```

La ligne 18 confie le texte au module des erreurs, et garde le nom stable qu'il rend : c'est ce pointeur qu'on comparera au moment d'une erreur, et que chaque noeud de l'arbre recevra. On compte les lignes et les colonnes à partir de 1, comme les éditeurs. Une colonne 0 ne veut rien dire pour quelqu'un qui cherche sa faute.

`lexer_init` est `static` : elle n'existe que dans ce fichier. Deux fonctions publiques s'en servent. Ouvrir un fichier, c'est le lire avec `readFile` du chapitre 1, puis initialiser le lexer :

```c fichier="src/lexer.c" lignes="30-44" fonction="newLexer" focus="38-41"
bool newLexer(lexer_t *lexer, const char *path){
    char *code = readFile(path);
    if(code == NULL)
        return false;

    lexer_init(lexer,path,code);

    // Un script peut commencer par #!/usr/bin/env Ceflo
    if(code[0] == '#' && code[1] == '!'){
        while(lexer->current_char != '\n' && lexer->current_char != '\0')
            lexer_advance(lexer);
    }

    return true;
}
```

Les lignes 38 à 41 permettent d'écrire `#!/usr/bin/env Ceflo` en tête d'un script, de le rendre exécutable avec `chmod +x`, et de le lancer comme un programme. Le système lit ces deux caractères, `#!`, et lance le programme nommé derrière en lui passant le fichier. CeFlo n'a pas de commentaire qui commence par `#` : cette ligne-là, et seulement elle, est sautée avant même le premier jeton. Remarque le test `code[1]` : si le fichier ne contient que `#`, `code[1]` est le `\0` final, et on ne lit rien en dehors du tableau.

L'autre fonction publique, `newLexerWithString`, lit une chaîne au lieu d'un fichier. On la verra à la fin du chapitre.

Tout le reste du lexer avance avec trois petites fonctions :

```c fichier="src/lexer.c" lignes="55-78" focus="59-64"
void lexer_advance(lexer_t *lexer){
    if(lexer->index >= (int)lexer->lenght_file_code)
        return;

    if(lexer->current_char == '\n'){
        lexer->line += 1;
        lexer->colone = 1;
    }else{
        lexer->colone += 1;
    }

    lexer->index += 1;
    lexer->current_char = lexer->fileCode[lexer->index];
}

static char lexer_peek(lexer_t *lexer){
    if(lexer->index + 1 >= (int)lexer->lenght_file_code)
        return '\0';
    return lexer->fileCode[lexer->index + 1];
}

static bool lexer_at_end(lexer_t *lexer){
    return lexer->index >= (int)lexer->lenght_file_code;
}
```

`lexer_advance` passe à l'octet suivant. Si celui qu'on quitte est un `\n`, on change de ligne et on revient en colonne 1, sinon on avance d'une colonne. C'est le seul endroit du programme qui tient ces deux compteurs : tant que tout le monde avance avec `lexer_advance`, les positions des erreurs seront justes. Le jour où une fonction écrira `lexer->index += 2` pour aller plus vite, les colonnes se décaleront, et l'erreur apparaîtra trois fichiers plus loin, dans un message dont la flèche tombe à côté.

Arrivé à la fin, `lexer_advance` ne fait plus rien (lignes 56 et 57). On peut donc l'appeler une fois de trop sans risque, et `current_char` reste sur le `\0` final que `Buffer_Append` avait posé au chapitre 1. C'est ce `\0` qui protège tout le lexer : aucune boucle ne lit au-delà du texte, même si elle oublie de tester la fin.

`lexer_peek` regarde l'octet suivant sans avancer, et rend `\0` s'il n'y en a pas. Le lexer n'a besoin de regarder qu'un octet plus loin, jamais deux : c'est assez pour distinguer `=` de `==`, `/` de `//`, et `1.5` de `1.lenght()`.

Le `(int)` devant `lenght_file_code` est là parce que `index` est un `int` et la taille un `size_t`. Comparer les deux sans conversion ferait crier `-Wextra` (comparaison entre signé et non signé). Ça limite un fichier source à deux gigaoctets, ce qui laisse de la marge.

## 2.4 Un premier lexer : les symboles

On a de quoi écrire un lexer qui marche, même s'il ne connaît presque rien. Il saura sauter les espaces, reconnaître la fin du fichier et les symboles, et refuser tout le reste. Les nombres, les noms, les chaînes et les commentaires viendront ensuite, un par un.

### Sauter les espaces

Avant chaque jeton, il y a peut-être des espaces, des tabulations, des retours à la ligne. Aucun ne produit de jeton. Pour l'étape 1, une version simple de la fonction qui les saute, à placer juste après `lexer_at_end` :

```c fichier="livre/essais/chapitres-01-04/lexer_etape1.c"
// Étape 1 : seulement les espaces et les retours à la ligne. Les commentaires viendront à l'étape 2.
static bool lexer_skip_ignored(lexer_t *lexer){
    bool newline = false;

    while(!lexer_at_end(lexer)){
        char carac = lexer->current_char;

        if(carac == '\n'){
            newline = true;
            lexer_advance(lexer);
        }else if(carac == ' ' || carac == '\t' || carac == '\r'){
            lexer_advance(lexer);
        }else{
            break;
        }
    }

    return newline;
}
```

Le `\r` est sauté comme un espace : c'est ce qui permet de lire un fichier écrit sous Windows, où chaque ligne finit par `\r\n`. C'est aussi la suite du `"rb"` de `readFile` : on a gardé les octets tels quels, et c'est ici qu'on les ignore.

La fonction rend un booléen : a-t-on franchi un retour à la ligne ? Le lexer ne garde pas les retours à la ligne comme des jetons, mais il retient qu'il en a vu un. On verra à la section 2.9 pourquoi c'est si important.

### Fabriquer un jeton

Deux petites fonctions fabriquent les jetons de symboles :

```c fichier="src/lexer.c" lignes="260-277" focus="274-276"
static token_t lexer_make_token(lexer_t *lexer, TokenEnum type, int size){
    token_t token = {
        .type = type,
        .value = (char*)token_to_string(type),
        .line = lexer->line,
        .colone = lexer->colone
    };
    for(int i = 0; i < size; i++)
        lexer_advance(lexer);
    return token;
}

// Symbole qui peut être suivi d'un second caractère : `=` ou `==`, `+` ou `+=`...
static token_t lexer_check_token(lexer_t *lexer, char next, TokenEnum type_next, TokenEnum type_alone){
    if(lexer_peek(lexer) == next)
        return lexer_make_token(lexer,type_next,2);
    return lexer_make_token(lexer,type_alone,1);
}
```

`lexer_make_token` note la position *avant* d'avancer : un jeton est à l'endroit où il commence. Puis il avance de la taille du symbole, avec `lexer_advance`, pour que les colonnes restent justes. L'initialisation `{ .type = ..., .line = ... }` met à zéro tous les champs qu'elle ne nomme pas : `lenght` vaut 0 et `newline_before` vaut `false`.

Le `(char*)` devant `token_to_string` retire le `const` : le champ `value` est un `char *` parce que d'autres jetons y rangent un texte alloué. C'est `token_owns_value` qui garantit qu'on n'écrira jamais dans le tableau.

`lexer_check_token` regarde un octet plus loin : si c'est le second caractère attendu, on fabrique le jeton à deux caractères, sinon celui à un seul. Le lexer prend toujours le symbole le plus long possible. `==` n'est jamais lu comme deux `=`, et `===`, qui n'existe pas en CeFlo, donne `==` puis `=`. On appelle ça la règle du *plus long préfixe* (*maximal munch*), et presque tous les langages la suivent. C'est elle qui fait qu'en C, `a+++b` se lit `a++ + b`.

### Choisir

La fonction centrale du lexer regarde le caractère courant et choisit quoi fabriquer. Pour l'étape 1, son début est réduit à la fin du fichier :

```c fichier="livre/essais/chapitres-01-04/lexer_etape1.c"
// Étape 1 : la fin du fichier et les symboles. Les nombres, les noms et les chaînes viendront ensuite.
static token_t lexer_scan_token(lexer_t *lexer){
    char carac = lexer->current_char;

    if(lexer_at_end(lexer))
        return lexer_make_token(lexer,TOKEN_EOF,0);

```

Un jeton `TOKEN_EOF` de taille 0 : on n'avance pas, et si on redemande un jeton, on aura de nouveau `TOKEN_EOF`. Le parser pourra le regarder autant qu'il veut. La suite est la même que dans le dépôt :

```c fichier="src/lexer.c" lignes="296-323" focus="317-320"
    switch(carac){
        case '(': return lexer_make_token(lexer,TOKEN_LPARENT,1);
        case ')': return lexer_make_token(lexer,TOKEN_RPARENT,1);
        case '{': return lexer_make_token(lexer,TOKEN_LBRACKET,1);
        case '}': return lexer_make_token(lexer,TOKEN_RBRACKET,1);
        case '[': return lexer_make_token(lexer,TOKEN_LBRACE,1);
        case ']': return lexer_make_token(lexer,TOKEN_RBRACE,1);
        case '.': return lexer_make_token(lexer,TOKEN_DOT,1);
        case ',': return lexer_make_token(lexer,TOKEN_COMA,1);
        case ';': return lexer_make_token(lexer,TOKEN_SEMI,1);
        case '@': return lexer_make_token(lexer,TOKEN_AT_SIGN,1);
        case '?': return lexer_make_token(lexer,TOKEN_MARK,1);
        case ':': return lexer_check_token(lexer,':',TOKEN_DOUBLE_COLON,TOKEN_COLON);
        case '!': return lexer_check_token(lexer,'=',TOKEN_DIF,TOKEN_NOT);
        case '=': return lexer_check_token(lexer,'=',TOKEN_EQUAL_EQUAL,TOKEN_EQUAL);
        case '+': return lexer_check_token(lexer,'=',TOKEN_PLUS_EQUAL,TOKEN_PLUS);
        case '*': return lexer_check_token(lexer,'=',TOKEN_MULT_EQUAL,TOKEN_MULT);
        case '/': return lexer_check_token(lexer,'=',TOKEN_DIV_EQUAL,TOKEN_DIV);
        case '%': return lexer_check_token(lexer,'=',TOKEN_MODULO_EQUAL,TOKEN_MODULO);
        case '<': return lexer_check_token(lexer,'=',TOKEN_INF_EQUAL,TOKEN_INF);
        case '>': return lexer_check_token(lexer,'=',TOKEN_SUP_EQUAL,TOKEN_SUP);
        case '-':
            if(lexer_peek(lexer) == '>')
                return lexer_make_token(lexer,TOKEN_ARROW,2);
            return lexer_check_token(lexer,'=',TOKEN_MINUS_EQUAL,TOKEN_MINUS);
        default:
            break;
    }
```

Avec les deux outils, la liste des symboles n'est plus qu'une table. Attention aux noms : dans CeFlo, `TOKEN_LBRACKET` est l'accolade `{`, et `TOKEN_LBRACE` le crochet `[`. C'est l'inverse de l'habitude anglaise, où *bracket* désigne plutôt le crochet. Garde la table sous les yeux quand tu liras le parser, tu gagneras du temps.

Le `-` est le seul symbole à trois issues : `->` (la flèche des paramètres, `func aire -> l, h`), `-=`, ou `-` seul. On teste d'abord la flèche, puis on laisse `lexer_check_token` choisir entre les deux autres. Le `/` est pour l'instant une division ; à l'étape 2, les commentaires `//` et `/*` seront mangés avant d'arriver ici.

Et si aucun `case` ne correspond ? Le caractère n'a rien à faire dans un programme CeFlo :

```c fichier="src/lexer.c" lignes="325-332"
    // Un caractère accentué occupe plusieurs octets : on les affiche tous.
    int size = 1;
    while(((unsigned char)lexer->fileCode[lexer->index + size] & 0xC0) == 0x80)
        size += 1;

    error_syntax(lexer->fileName,lexer->line,lexer->colone,
        "caractère inattendu `%.*s`",size,&lexer->fileCode[lexer->index]);
}
```

On retrouve le test `& 0xC0` du chapitre 1. Si l'octet fautif est le début d'un caractère UTF-8, on compte les octets de suite qui le complètent, et on les affiche tous avec `%.*s`, qui prend une longueur en argument. Sans cette boucle, le message montrerait la moitié d'un `é`, un symbole illisible. La boucle ne risque pas de déborder : au pire, elle s'arrête sur le `\0` final, qui ne commence pas par les bits `10`.

La fonction n'a pas de `return` après `error_syntax`, et gcc ne s'en plaint pas : `error_syntax` est déclarée `_Noreturn` dans `error.h`.

### Le jeton suivant

Il ne reste qu'une fonction, la seule que le parser appellera :

```c fichier="src/lexer.c" lignes="334-339" fonction="lexer_get_next_token"
token_t lexer_get_next_token(lexer_t *lexer){
    bool newline = lexer_skip_ignored(lexer);
    token_t token = lexer_scan_token(lexer);
    token.newline_before = newline;
    return token;
}
```

Sauter ce qui ne compte pas, lire un jeton, et noter si un retour à la ligne a été franchi pour y arriver.

Avec la fonction `newLexerWithString` des lignes 46 à 53 (on l'expliquera à la section 2.9), ton fichier compile. On a testé cette étape dans `livre/essais/chapitres-01-04/lexer_etape1.c` :

```terminal
$ cat livre/essais/chapitres-01-04/symboles.cf
( ) == = -> -= -
:: : { }
$ livre/essais/chapitres-01-04/jetons_etape1 livre/essais/chapitres-01-04/symboles.cf
  1:1     TOKEN_LPARENT      (
  1:3     TOKEN_RPARENT      )
  1:5     TOKEN_EQUAL_EQUAL  ==
  1:8     TOKEN_EQUAL        =
  1:10    TOKEN_ARROW        ->
  1:13    TOKEN_MINUS_EQUAL  -=
  1:16    TOKEN_MINUS        -
  2:1   | TOKEN_DOUBLE_COLON ::
  2:4     TOKEN_COLON        :
  2:6     TOKEN_LBRACKET     {
  2:8     TOKEN_RBRACKET     }
  3:1   | TOKEN_EOF          fin de fichier
```

Chaque symbole à sa colonne, les doubles reconnus, la nouvelle ligne marquée. Et un vrai programme est refusé dès sa première lettre :

```terminal
$ livre/essais/chapitres-01-04/jetons_etape1 livre/essais/chapitres-01-04/douze.cf
erreur de syntaxe: caractère inattendu `v`
  --> livre/essais/chapitres-01-04/douze.cf:1:1
    1 | var x = 12
      | ^
```

C'est normal : notre lexer ne connaît pas encore les lettres.

## 2.5 Sauter les commentaires

Remplace la `lexer_skip_ignored` de l'étape 1 par celle du dépôt. Son début est le même, plus un cas :

```c fichier="src/lexer.c" lignes="85-98" fonction="lexer_skip_ignored"
static bool lexer_skip_ignored(lexer_t *lexer){
    bool newline = false;

    while(!lexer_at_end(lexer)){
        char carac = lexer->current_char;

        if(carac == '\n'){
            newline = true;
            lexer_advance(lexer);
        }else if(carac == ' ' || carac == '\t' || carac == '\r'){
            lexer_advance(lexer);
        }else if(carac == '/' && lexer_peek(lexer) == '/'){
            while(!lexer_at_end(lexer) && lexer->current_char != '\n')
                lexer_advance(lexer);
```

Un commentaire `//` avance jusqu'au `\n`, mais ne le mange pas. Au tour suivant de la grande boucle, c'est le cas `carac == '\n'` qui le traite, et qui note qu'une ligne a été franchie. Si le commentaire mangeait son `\n`, la ligne d'après ne serait pas marquée comme nouvelle, et `x = 1 // un` suivi de `(a)` à la ligne suivante deviendrait un appel `1(a)`.

Le commentaire `/* ... */` demande un peu plus de soin :

```c fichier="src/lexer.c" lignes="99-119" focus="100-101,105-106"
        }else if(carac == '/' && lexer_peek(lexer) == '*'){
            int line = lexer->line;
            int colone = lexer->colone;
            lexer_advance(lexer);
            lexer_advance(lexer);
            while(!(lexer->current_char == '*' && lexer_peek(lexer) == '/')){
                if(lexer_at_end(lexer))
                    error_syntax(lexer->fileName,line,colone,"commentaire `/*` jamais fermé");
                if(lexer->current_char == '\n')
                    newline = true;
                lexer_advance(lexer);
            }
            lexer_advance(lexer);
            lexer_advance(lexer);
        }else{
            break;
        }
    }

    return newline;
}
```

On saute les deux caractères `/*` (lignes 102 et 103), puis on avance jusqu'à trouver `*` suivi de `/`, et on saute ces deux-là aussi (lignes 111 et 112). Les deux `lexer_advance` du début ne sont pas une coquetterie. Avec un seul, `/*/` serait lu comme un commentaire ouvert et aussitôt fermé : le `*` de l'ouverture, suivi du `/` d'après, ressemble à une fermeture.

On note la position du `/*` *avant* d'avancer. Si le commentaire n'est jamais fermé, on s'en aperçoit à la fin du fichier, et à ce moment-là, la position courante ne dit rien d'utile : c'est la dernière ligne. L'erreur pointe donc l'ouverture, là où le lecteur pourra agir :

```terminal
$ cat livre/essais/chapitres-01-04/com.cf
var x = 1 /* pas
fermé
$ ./Ceflo livre/essais/chapitres-01-04/com.cf
erreur de syntaxe: commentaire `/*` jamais fermé
  --> livre/essais/chapitres-01-04/com.cf:1:11
    1 | var x = 1 /* pas
      |           ^
```

Un commentaire qui couvre plusieurs lignes compte aussi comme un retour à la ligne (lignes 107 et 108). Et les commentaires ne s'imbriquent pas : dans `/* a /* b */ c */`, le premier `*/` ferme tout, et `c */` sera lu comme du code. C'est le comportement du C. Rust, Swift et Haskell ont des commentaires qui s'imbriquent, ce qui permet de commenter un bloc qui contient déjà des commentaires ; il faut alors compter les ouvertures. C'est un des défis de la fin du chapitre.

## 2.6 Les nombres

Un chiffre commence un nombre. Ajoute ces deux tests au début de `lexer_scan_token`, juste après celui de la fin du fichier :

```c fichier="src/lexer.c" lignes="285-290"
    if(lexer_at_end(lexer))
        return lexer_make_token(lexer,TOKEN_EOF,0);
    if(isdigit((unsigned char)carac))
        return lexer_collect_digit(lexer);
    if(isalpha((unsigned char)carac) || carac == '_')
        return lexer_collect_id(lexer);
```

Le second test est pour la section suivante. L'ordre compte un peu : on teste les chiffres avant les lettres, parce qu'un nom peut contenir des chiffres mais ne peut pas commencer par l'un d'eux. Et le `(unsigned char)` devant `isdigit` n'est pas décoratif : un octet d'un caractère accentué est négatif dans un `char` signé, et passer un nombre négatif à `isdigit` est un comportement indéfini en C. Sur certaines bibliothèques, ça lit en dehors d'une table, et ça plante.

Il faut d'abord un outil pour copier un morceau du texte :

```c fichier="src/lexer.c" lignes="158-163" fonction="lexer_get_value"
static char *lexer_get_value(lexer_t *lexer, int initiale){
    int lenght = lexer->index - initiale;
    char *value = ceflo_alloc((size_t)lenght + 1);
    memcpy(value,&lexer->fileCode[initiale],(size_t)lenght);
    return value;
}
```

On lui donne l'index où le jeton a commencé ; il copie tout jusqu'à l'index courant. Il réserve `lenght + 1` octets, copie `lenght` octets, et ne pose jamais de `\0`. Il n'en a pas besoin : `ceflo_alloc` appelle `calloc`, et le dernier octet vaut déjà zéro. C'est la promesse du chapitre 1 qui sert pour la première fois.

Puis le nombre lui-même. Le cas simple tient en deux lignes : avancer tant qu'on voit des chiffres. Le cas intéressant, c'est le point.

```c fichier="src/lexer.c" lignes="184-203" fonction="lexer_collect_digit" focus="191-196"
static token_t lexer_collect_digit(lexer_t *lexer){
    token_t token = {.type = TOKEN_INTEGER, .line = lexer->line, .colone = lexer->colone};
    int initial = lexer->index;

    while(isdigit((unsigned char)lexer->current_char))
        lexer_advance(lexer);

    if(lexer->current_char == '.' && isdigit((unsigned char)lexer_peek(lexer))){
        token.type = TOKEN_FLOAT;
        lexer_advance(lexer);
        while(isdigit((unsigned char)lexer->current_char))
            lexer_advance(lexer);
    }

    if(isalpha((unsigned char)lexer->current_char) || lexer->current_char == '_')
        error_syntax(lexer->fileName,token.line,token.colone,"nombre mal formé");

    token.value = lexer_get_value(lexer,initial);
    return token;
}
```

Un point après des chiffres ne fait partie du nombre que si un chiffre le suit. Pourquoi ce `lexer_peek` ? Parce qu'en CeFlo, un nombre est une valeur comme une autre, et `1.lenght()` est du code valable. Il veut dire « appelle la méthode `lenght` sur le nombre 1 » (qui n'existe pas, mais c'est au reste de l'interpréteur de le dire).

Pour sentir le problème, j'ai recopié `src/lexer.c` dans `livre/essais/chapitres-01-04/lexer_naif.c`, en enlevant juste ce test :

```terminal
$ diff src/lexer.c livre/essais/chapitres-01-04/lexer_naif.c
191c191
<     if(lexer->current_char == '.' && isdigit((unsigned char)lexer_peek(lexer))){
---
>     if(lexer->current_char == '.'){
```

Le programme `jetons`, compilé avec ce lexer naïf, lit `1.` comme un décimal, puis voit le `l` de `lenght` collé derrière :

```terminal
$ livre/essais/chapitres-01-04/jetons_naif livre/essais/chapitres-01-04/point.cf
  1:1     TOKEN_ID           println
  1:8     TOKEN_LPARENT      (
erreur de syntaxe: nombre mal formé
  --> livre/essais/chapitres-01-04/point.cf:1:9
    1 | println(1.lenght())
      |         ^
```

Avec le vrai lexer, `1`, `.` et `lenght` sont trois jetons, et c'est la machine virtuelle qui répond, avec un message qui a du sens :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/point.cf
erreur: number n'a pas de méthode `lenght`
  --> livre/essais/chapitres-01-04/point.cf:1:11
    1 | println(1.lenght())
      |           ^
```

Python et JavaScript ont le comportement naïf. En Python, `1.real` est une erreur de syntaxe (il faut écrire `(1).real` ou `1 .real`), et en JavaScript, `1.toString()` aussi (on écrit `1..toString()`). Un seul `lexer_peek` évite ce piège, au prix d'une petite restriction : `1.` sans chiffre derrière n'est pas un nombre en CeFlo. On écrit `1.0`.

Les lignes 198 et 199 refusent un nombre collé à une lettre, comme `12abc`. Sans elles, on aurait le nombre `12` suivi du nom `abc`, et le parser dirait quelque chose d'incompréhensible plus loin (« `abc` inattendu »). Ici, l'erreur est dite tout de suite, à la bonne place, en pointant le début du nombre :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/nombre.cf
erreur de syntaxe: nombre mal formé
  --> livre/essais/chapitres-01-04/nombre.cf:1:9
    1 | var x = 12abc
      |         ^
```

Le lexer ne calcule pas la valeur du nombre. Il garde son texte, `"12"` ou `"1.5"`, et c'est le parser qui appellera `strtod`. Le jeton dit seulement si le nombre avait un point : `TOKEN_INTEGER` ou `TOKEN_FLOAT`. Les deux deviendront le même `double`, mais la grammaire distingue parfois les deux (une clé de map, une valeur d'énumération).

Pas de nombre négatif non plus : dans `-5`, le lexer voit un `TOKEN_MINUS` puis un `TOKEN_INTEGER`. Il ne peut pas savoir si ce moins est une soustraction (`a -5`) ou un signe (`f(-5)`) ; c'est le parser, qui connaît le contexte, qui en décidera.

Pas de `1e10` ni de `0xFF` enfin : CeFlo n'a que les chiffres et le point. Le second est un défi.

## 2.7 Les noms et les mots-clés

Une lettre ou un `_` commence un nom. Le lexer avance tant qu'il voit des lettres, des chiffres ou des `_`, puis copie ce qu'il a parcouru :

```c fichier="src/lexer.c" lignes="165-182" fonction="lexer_collect_id" focus="173"
static token_t lexer_collect_id(lexer_t *lexer){
    token_t token = {.line = lexer->line, .colone = lexer->colone};
    int initiale = lexer->index;

    while(isalnum((unsigned char)lexer->current_char) || lexer->current_char == '_')
        lexer_advance(lexer);

    char *value = lexer_get_value(lexer,initiale);
    token.type = lexer_precise_id(value);

    if(token_owns_value(token.type)){
        token.value = value;
    }else{
        free(value);
        token.value = (char*)token_to_string(token.type);
    }
    return token;
}
```

La position du jeton est prise *avant* d'avancer, au premier caractère du nom. Puis, ligne 173, on se demande si ce nom est un mot réservé. `lexer_precise_id` le vérifie avec une table, à placer au-dessus de `lexer_get_value` :

```c fichier="src/lexer.c" lignes="121-131"
static TokenEnum lexer_precise_id(const char *chaine){
    static const struct { const char *name; TokenEnum type; } keywords[] = {
        {"for",TOKEN_FOR},
        {"in",TOKEN_IN},
        {"while",TOKEN_WHILE},
        {"if",TOKEN_IF},
        {"else",TOKEN_ELSE},
        {"match",TOKEN_MATCH},
        {"case",TOKEN_CASE},
        {"var",TOKEN_VAR},
        {"const",TOKEN_CONST},
```

La table continue jusqu'à la ligne 148 avec les autres mots, de `{"func",TOKEN_FUNC}` à `{"break",TOKEN_BREAK}`. Elle est `static` dans la fonction : elle n'est construite qu'une fois, au chargement du programme, pas à chaque appel. Et son type est une structure anonyme, déclarée sur place, parce que personne d'autre n'en a besoin. La recherche elle-même :

```c fichier="src/lexer.c" lignes="150-156"
    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++){
        if(strcmp(chaine,keywords[i].name) == 0)
            return keywords[i].type;
    }

    return TOKEN_ID;
}
```

`sizeof(keywords) / sizeof(keywords[0])` est le nombre d'éléments du tableau, calculé par le compilateur : ajouter un mot à la table ne demande rien d'autre. Vingt-cinq mots, comparés un par un avec `strcmp`.

On pourrait faire plus rapide. *Crafting Interpreters* écrit un `switch` sur la première lettre, puis compare le reste, ce qui fait au plus une ou deux comparaisons par nom. Lua fait plus malin encore : toutes ses chaînes sont internées (une seule copie de chaque texte en mémoire), et les mots réservés sont marqués au démarrage ; reconnaître un mot-clé ne coûte alors qu'un test sur la chaîne déjà trouvée. Mais le lexer de CeFlo ne lit chaque fichier qu'une fois. Même sur un gros programme, ces vingt-cinq `strcmp` par nom ne se voient pas à côté de l'exécution.

Si le nom est un mot réservé, le texte copié ne sert plus : on le libère tout de suite, et `value` pointe sur le texte du tableau (`"for"`, `"while"`...). On retrouve la règle de la section 2.1 : un jeton qui ne possède pas son texte n'a pas de texte alloué.

Remarque ce qui *n'est pas* dans la table : `println`, `self`, `main`, `int`, `str`. Ce sont des noms ordinaires. Un script peut appeler une variable `println`, et c'est le reste de l'interpréteur qui décidera de leur sens. Moins il y a de mots réservés, moins on casse de programmes en ajoutant une fonction au langage.

> [!note]
> Les lettres accentuées ne sont pas des lettres pour `isalnum`. `var café = 1` lit le nom `caf`, puis bute sur le `é`, et c'est la boucle UTF-8 de la section 2.4 qui affiche le message en entier :
>
> `` caractère inattendu `é` ``

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/cafe.cf
erreur de syntaxe: caractère inattendu `é`
  --> livre/essais/chapitres-01-04/cafe.cf:1:8
    1 | var café = 1
      |        ^
```

Python 3 accepte les noms accentués, et JavaScript aussi. CeFlo s'en tient à l'ASCII, comme le C avant C99. C'est un défi de la fin du chapitre.

### Compiler l'étape 2

Ton lexer connaît maintenant les commentaires, les nombres et les noms. Il ne lui manque que les chaînes. On a testé cette étape dans `livre/essais/chapitres-01-04/lexer_etape2.c` :

```terminal
$ cat livre/essais/chapitres-01-04/noms.cf
// des noms et des nombres
var x2 = 3.14 /* pi */ + 1.lenght()
while _ok
$ livre/essais/chapitres-01-04/jetons_etape2 livre/essais/chapitres-01-04/noms.cf
  2:1   | TOKEN_VAR          var
  2:5     TOKEN_ID           x2
  2:8     TOKEN_EQUAL        =
  2:10    TOKEN_FLOAT        3.14
  2:24    TOKEN_PLUS         +
  2:26    TOKEN_INTEGER      1
  2:27    TOKEN_DOT          .
  2:28    TOKEN_ID           lenght
  2:34    TOKEN_LPARENT      (
  2:35    TOKEN_RPARENT      )
  3:1   | TOKEN_WHILE        while
  3:7     TOKEN_ID           _ok
  4:1   | TOKEN_EOF          fin de fichier
```

Le commentaire de la ligne 1 a disparu, mais le premier jeton porte le `|`. Le `/* pi */` aussi a disparu, et le `+` qui le suit est bien en colonne 24 : les colonnes ont continué de compter à travers le commentaire. `x2` est un nom, `while` un mot-clé, `_ok` un nom.

## 2.8 Les chaînes et les caractères

Il reste les textes entre guillemets. Ajoute ces deux tests à `lexer_scan_token`, après celui des noms :

```c fichier="src/lexer.c" lignes="291-294"
    if(carac == '"')
        return lexer_collect_quoted(lexer,'"',TOKEN_STRING);
    if(carac == '\'')
        return lexer_collect_quoted(lexer,'\'',TOKEN_CHAR);
```

Une chaîne commence par `"`, un caractère par `'`. Les deux se lisent avec la même fonction, à qui on dit quel guillemet ferme le texte :

```c fichier="src/lexer.c" lignes="235-258" fonction="lexer_collect_quoted"
static token_t lexer_collect_quoted(lexer_t *lexer, char quote, TokenEnum type){
    token_t token = {.type = type, .line = lexer->line, .colone = lexer->colone};
    buffer_t buffer;
    newBuffer(&buffer);

    lexer_advance(lexer);
    while(lexer->current_char != quote){
        if(lexer_at_end(lexer)){
            error_syntax(lexer->fileName,token.line,token.colone,
                type == TOKEN_STRING ? "chaîne jamais fermée" : "caractère jamais fermé");
        }
        lexer_collect_escape(lexer,&buffer);
    }
    lexer_advance(lexer);

    if(type == TOKEN_CHAR && buffer.size != 1){
        error_syntax(lexer->fileName,token.line,token.colone,
            "un caractère contient un seul octet, utilisez \"...\" pour une chaîne");
    }

    token.value = buffer.data;
    token.lenght = (int)buffer.size;
    return token;
}
```

Cette fois, on ne peut pas copier un morceau du fichier tel quel, comme pour un nom. Dans le fichier, `"a\tb"` fait six octets, avec une barre oblique et un `t`. Dans la chaîne, il en fait trois, avec une vraie tabulation au milieu. On construit donc le texte octet par octet dans un `buffer_t`, celui du chapitre 1, et à la fin on garde son tableau `data` comme texte du jeton. Le premier `lexer_advance` saute le guillemet ouvrant, le second le guillemet fermant.

Chaque octet passe par `lexer_collect_escape`, qui traduit les séquences. Place-la juste avant :

```c fichier="src/lexer.c" lignes="208-233" fonction="lexer_collect_escape" focus="226-230"
static void lexer_collect_escape(lexer_t *lexer, buffer_t *buffer){
    if(lexer->current_char != '\\'){
        Buffer_Append_Char(buffer,lexer->current_char);
        lexer_advance(lexer);
        return;
    }

    lexer_advance(lexer);
    switch(lexer->current_char){
        case 'n': Buffer_Append_Char(buffer,'\n'); break;
        case 't': Buffer_Append_Char(buffer,'\t'); break;
        case 'r': Buffer_Append_Char(buffer,'\r'); break;
        case 'a': Buffer_Append_Char(buffer,'\a'); break;
        case 'b': Buffer_Append_Char(buffer,'\b'); break;
        case '0': Buffer_Append_Char(buffer,'\0'); break;
        case '\\': Buffer_Append_Char(buffer,'\\'); break;
        case '"': Buffer_Append_Char(buffer,'"'); break;
        case '\'': Buffer_Append_Char(buffer,'\''); break;
        default:
            // Séquence inconnue : on la garde telle quelle (format() utilise \$).
            Buffer_Append_Char(buffer,'\\');
            Buffer_Append_Char(buffer,lexer->current_char);
            break;
    }
    lexer_advance(lexer);
}
```

Un octet ordinaire est copié. Une barre oblique inverse annonce une séquence : on avance, on regarde la lettre, et on écrit l'octet qu'elle désigne. `\"` écrit un guillemet sans fermer la chaîne : c'est pour ça qu'on traduit *avant* de tester le guillemet fermant, et que `"il dit \"bonjour\""` se lit en entier.

Une séquence inconnue n'est pas une erreur : on garde la barre et la lettre. C'est voulu. `format("\${x}")` doit pouvoir écrire un vrai `${x}`, et pour ça, le `\$` doit arriver intact jusqu'à `format`, qui fera lui-même la traduction. Python fait pareil (`"\q"` y reste `\q`), mais il avertit depuis la version 3.6 que ce sera un jour une erreur. Le C, lui, laisse chaque compilateur décider, et gcc avertit.

Et `\0` est accepté. Une chaîne CeFlo peut donc contenir un octet nul en plein milieu, et `strlen` se tromperait sur sa taille. C'est pour ça que `token_t` a un champ `lenght`, rempli ici à la ligne 256, et seulement ici : pour un nom ou un nombre, il reste à 0, et le parser se sert de `strlen`.

```terminal
$ cat livre/essais/chapitres-01-04/nul.cf
println("a\0b".lenght())
$ ./Ceflo livre/essais/chapitres-01-04/nul.cf
3
```

Rien n'interdit non plus un vrai retour à la ligne entre les guillemets : une chaîne peut s'étaler sur plusieurs lignes, et `lexer_advance` tient le compte des lignes au passage. Python demande des triples guillemets pour ça ; CeFlo ne fait pas de différence.

Les deux erreurs pointent le guillemet ouvrant, pour la même raison que le commentaire : c'est là que le lecteur peut corriger. Le test de fin (ligne 242) est indispensable : sans lui, une chaîne jamais fermée bouclerait à l'infini sur le `\0` final, puisque `lexer_advance` ne bouge plus à la fin.

La seconde erreur concerne les caractères. Un caractère CeFlo est un seul octet, comme un `char` en C. Un `é` en fait deux :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/lettre.cf
erreur de syntaxe: un caractère contient un seul octet, utilisez "..." pour une chaîne
  --> livre/essais/chapitres-01-04/lettre.cf:1:9
    1 | var c = 'é'
      |         ^
```

Le message ne dit pas seulement ce qui ne va pas, il dit quoi faire à la place. C'est une petite chose, et c'est ce qui sépare un message qu'on comprend d'un message qu'on recopie dans un moteur de recherche.

Avec ces trois fonctions, ton `lexer.c` est complet. Vérifie-le :

```terminal
$ diff livre/essais/chapitres-01-04/lexer.c src/lexer.c
```

Rien ne doit s'afficher. Recompile `jetons` avec la commande de la section 2.2.

## 2.9 La ligne d'avant

Revenons au petit `|` de notre affichage, `newline_before`. Pourquoi le garder ? Parce que CeFlo n'a pas de point-virgule obligatoire. Regarde ces deux lignes :

```ceflo
println(a)
[1, 2].print()
```

Pour nous, ce sont deux instructions. Mais si on ne regarde que les jetons, on voit `println ( a ) [ 1 , 2 ] . print ( )`, et ça se lit tout aussi bien comme « prends le résultat de `println(a)`, et lis son élément `[1, 2]` ». Le texte est ambigu, et seul le retour à la ligne permet de trancher.

Un langage peut régler ça de plusieurs façons, et la note de conception de la fin du chapitre les compare. CeFlo choisit la plus simple à écrire : le lexer ne décide rien, il note seulement `newline_before`, et le parser applique deux règles courtes. Un `(` ou un `[` en début de ligne commence une nouvelle instruction. Un `return` suivi d'un retour à la ligne ne retourne rien. On les écrira au chapitre 3, et elles tiennent en quelques conditions.

Le point-virgule existe quand même, `TOKEN_SEMI`. Il permet d'écrire deux instructions sur une ligne, comme `n += 1; return n`.

### Le texte de format()

`lexer.c` a une dernière fonction publique, qui ne lit pas de fichier :

```c fichier="src/lexer.c" lignes="49-53" fonction="newLexerWithString"
void newLexerWithString(lexer_t *lexer, const char *name, const char *source){
    char *code = NULL;
    allowChar(&code,source);
    lexer_init(lexer,name,code);
}
```

Elle sert à `format("${a + 1}")`. Le texte entre `${` et `}` est une expression CeFlo, et pour la calculer, il faut la découper en jetons comme le reste. On en fait une copie (le module des erreurs va en devenir propriétaire et la libérera, il ne faut pas lui donner un texte qui appartient à quelqu'un d'autre), on la lui confie sous le nom `"format"`, et le même lexer fait le travail. On n'écrit pas un second lexer pour ça.

### Tout ensemble

Essaie le lexer complet sur un fichier qui mélange un peu de tout :

```terminal
$ cat livre/essais/chapitres-01-04/melange.cf
/* un peu de tout */
func aire -> l::int, h : int {
    return l * h  // le produit
}
var t = "a\tb\n"
println(1.5, 1.lenght(), t != 'x')
$ livre/essais/chapitres-01-04/jetons livre/essais/chapitres-01-04/melange.cf
  2:1   | TOKEN_FUNC         func
  2:6     TOKEN_ID           aire
  2:11    TOKEN_ARROW        ->
  2:14    TOKEN_ID           l
  2:15    TOKEN_DOUBLE_COLON ::
  2:17    TOKEN_ID           int
  2:20    TOKEN_COMA         ,
  2:22    TOKEN_ID           h
  2:24    TOKEN_COLON        :
  2:26    TOKEN_ID           int
  2:30    TOKEN_LBRACKET     {
  3:5   | TOKEN_RETURN       return
  3:12    TOKEN_ID           l
  3:14    TOKEN_MULT         *
  3:16    TOKEN_ID           h
  4:1   | TOKEN_RBRACKET     }
  5:1   | TOKEN_VAR          var
  5:5     TOKEN_ID           t
  5:7     TOKEN_EQUAL        =
  5:9     TOKEN_STRING       a\tb\n
  6:1   | TOKEN_ID           println
  6:8     TOKEN_LPARENT      (
  6:9     TOKEN_FLOAT        1.5
  6:12    TOKEN_COMA         ,
  6:14    TOKEN_INTEGER      1
  6:15    TOKEN_DOT          .
  6:16    TOKEN_ID           lenght
  6:22    TOKEN_LPARENT      (
  6:23    TOKEN_RPARENT      )
  6:24    TOKEN_COMA         ,
  6:26    TOKEN_ID           t
  6:28    TOKEN_DIF          !=
  6:31    TOKEN_CHAR         x
  6:34    TOKEN_RPARENT      )
  7:1   | TOKEN_EOF          fin de fichier
```

Tout ce qu'on vient d'écrire est là. `::` et `:` sont deux jetons différents, `->` aussi. Le `\t` et le `\n` de la chaîne ont été traduits en vrais octets (c'est notre `print_value` qui les réécrit avec une barre pour qu'on les voie). Le commentaire `// le produit` a disparu sans laisser de trace, et le `}` de la ligne 4 porte son `|`. Le lexer de l'étape 2, lui, s'arrêtait sur cette ligne 5 :

```terminal
$ livre/essais/chapitres-01-04/jetons_etape2 livre/essais/chapitres-01-04/melange.cf 2>&1 | tail -7
  5:1   | TOKEN_VAR          var
  5:5     TOKEN_ID           t
  5:7     TOKEN_EQUAL        =
erreur de syntaxe: caractère inattendu `"`
  --> livre/essais/chapitres-01-04/melange.cf:5:9
    5 | var t = "a\tb\n"
      |         ^
```

Remarque l'ordre : les jetons de la ligne 5 sont affichés *avant* l'erreur, alors que `printf` écrit dans un tampon et l'erreur directement sur `stderr`. C'est le `fflush(stdout)` de `error_report`, au chapitre 1, qui garantit cet ordre.

### Ce que le lexer ne voit pas

```terminal
$ cat livre/essais/chapitres-01-04/at.cf
var a = 1 @ 2
$ livre/essais/chapitres-01-04/jetons livre/essais/chapitres-01-04/at.cf
  1:1     TOKEN_VAR          var
  1:5     TOKEN_ID           a
  1:7     TOKEN_EQUAL        =
  1:9     TOKEN_INTEGER      1
  1:11    TOKEN_AT_SIGN      @
  1:13    TOKEN_INTEGER      2
  2:1   | TOKEN_EOF          fin de fichier
```

Aucune erreur. `1 @ 2` n'a aucun sens, mais le lexer ne le sait pas. `@` est un jeton valable (il sert à `@int("42")`), `1` et `2` aussi. Le lexer vérifie que chaque mot existe, pas que la phrase a un sens. Le vrai CeFlo refuse bien ce programme, mais c'est le parser qui parle :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/at.cf
erreur de syntaxe: `identifiant` attendu, mais `nombre` trouvé
  --> livre/essais/chapitres-01-04/at.cf:1:13
    1 | var a = 1 @ 2
      |             ^
```

Après un `@`, il attendait un nom de type, comme dans `@int(...)`. Six erreurs seulement peuvent sortir de `src/lexer.c` : un commentaire jamais fermé, un nombre mal formé, une chaîne jamais fermée, un caractère jamais fermé, un caractère trop long, et un caractère inattendu. Tout le reste attendra le parser.

## Défis

1. **La puissance.** Ajoute l'opérateur `**` au lexer. Il faut un nouveau jeton dans `FOREACH_TOKEN` (et tu sais maintenant où le placer), et le `case '*'` doit choisir entre trois jetons, comme le fait déjà `case '-'`. Vérifie avec `jetons` que `2 ** 3` et `2 *= 3` donnent chacun trois jetons. Le parser s'en servira dans un défi du chapitre 3.

2. **Les hexadécimaux.** Fais accepter `0xFF` au lexer. C'est dans `lexer_collect_digit` qu'il faut regarder, avant la boucle des chiffres : `lexer_peek` te dira si le `0` est suivi d'un `x`, et `isxdigit` reconnaît les chiffres hexadécimaux. Le parser appelle `strtod`, qui sait déjà lire `"0xFF"`. Attention au test des lignes 198 et 199 : `0xFF` contient des lettres.

3. **Des noms accentués.** Fais accepter `var café = 1`. La règle la plus simple : un octet supérieur ou égal à `0x80` compte comme une lettre, dans `lexer_scan_token` et dans `lexer_collect_id`. Vérifie que la flèche d'une erreur sur la ligne `println(café, y)` tombe toujours sous le `y`. Pourquoi est-ce le cas sans rien changer dans `error.c` ?

4. **Des commentaires imbriqués.** Fais en sorte que `/* a /* b */ c */` soit un seul commentaire, comme en Rust. Il faut un compteur dans la boucle des lignes 104 à 110 : un `/*` l'augmente, un `*/` le diminue, et on s'arrête à zéro. Quel programme CeFlo existant pourrait cesser de marcher ? Lance `make test` pour le savoir.

## Note de conception : faut-il des points-virgules ?

Chaque langage doit savoir où finit une instruction, et il n'y a que quelques façons de le savoir.

Le C, Java et JavaScript dans sa forme stricte demandent un point-virgule. C'est la solution la plus simple pour le parser, et la plus pénible pour celui qui écrit : des millions de programmeurs ont passé des minutes de leur vie à chercher le `;` oublié que le compilateur signalait à la ligne d'après.

Python fait du retour à la ligne un jeton, `NEWLINE`, et de l'indentation deux autres, `INDENT` et `DEDENT`. Le lexer de Python est donc beaucoup plus compliqué (il tient une pile des niveaux d'indentation), mais la grammaire devient limpide : une instruction finit à la fin de sa ligne, sauf entre parenthèses.

Go fait insérer des points-virgules par son lexer, selon une règle fixe : à la fin d'une ligne qui se termine par un nom, un nombre, une chaîne, `)`, `]` ou `}`. C'est prévisible, et c'est pour ça qu'en Go l'accolade d'un `if` doit être sur la même ligne que lui.

JavaScript essaie de lire la suite, et n'insère un point-virgule que si ça ne marche pas. C'est la règle la plus souple et la plus traître. `return` suivi d'un retour à la ligne rend `undefined`, et une ligne qui commence par `(` se colle à la précédente. Lua n'a besoin d'aucun séparateur grâce à sa grammaire, mais Lua 5.1 refusait un appel dont la parenthèse commençait une nouvelle ligne, avec un message « ambiguous syntax », exactement pour le cas `println(a)` suivi de `[1, 2]`.

CeFlo prend le chemin du milieu : le lexer note seulement le fait (`newline_before`), sans rien inventer, et le parser décide à deux endroits précis. Je crois que c'est le bon compromis pour un petit langage. La règle s'explique en une phrase dans `GRAMMAIRE.md`, elle ne demande aucun état dans le lexer, et elle laisse écrire un calcul sur plusieurs lignes. Le prix, on le paiera au chapitre 3 : une ligne qui commence par un moins se colle à la précédente, et ce n'est écrit nulle part.

Le vrai critère, au fond, n'est pas l'élégance de la grammaire : c'est la surprise. Une bonne règle est celle que le lecteur devine avant de l'avoir lue. Aucune des solutions ci-dessus n'y arrive complètement, et c'est sans doute pour ça que la question revient à chaque nouveau langage.

---

Le texte est maintenant une suite de jetons propres, avec leur position exacte. Mais une suite, c'est plat. Dans `1 + 2 * 3`, le lexer ne voit que cinq jetons à la queue leu leu. Rien ne dit encore que la multiplication passe avant l'addition, que la `}` de la ligne 4 ferme la `{` de la ligne 2, ni que `1 @ 2` est une phrase sans verbe. Pour ça, il faut une grammaire, et un programme qui la suit jeton après jeton. C'est le parser, le plus gros fichier de cette partie, et c'est le chapitre suivant.
