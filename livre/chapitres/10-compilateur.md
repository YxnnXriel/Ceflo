---
numero: 10
titre: le compilateur
note: l'arbre devient une suite
partie: Le bytecode
---

## 10.1 Un visitor qui écrit au lieu d'agir

Au chapitre 9, on a lu le bytecode de `somme` octet par octet. On y a vu des
choses qu'aucune instruction ne dit : `total` est la case 2, la condition
saute 14 octets plus loin, `i += 1` a droit à son instruction rapide. Toutes
ces décisions ont été prises par `src/vm/compiler.c`, avant que la première
instruction s'exécute. Ce chapitre les prend une par une.

La bonne nouvelle, c'est que tu connais déjà la forme du compilateur. Son
commentaire d'en-tête le dit tout de suite :

```c fichier="src/vm/compiler.c" lignes="5-10"
 * [NOTE]
 *      Chaque noeud de l'AST a sa fonction compile_*, comme les visitor_visit_*
 *      de src/visitor.c. Une expression laisse sa valeur au sommet de la pile.
 *      Une instruction laisse la pile comme elle l'a trouvée : entre deux
 *      instructions, la pile ne contient que les variables locales. C'est ce
 *      qui permet de donner à chaque locale une case fixe.
```

C'est le visitor du chapitre 7, avec un seul changement : là où
`visitor_visit_binop` calculait `a + b`, `compile_binop` écrit les
instructions qui le calculeront.

```c fichier="src/vm/compiler.c" lignes="936-944" fonction="compile_binop"
static void compile_binop(Compiler *c, ast_t *node){
    if(node->op == TOKEN_AND || node->op == TOKEN_OR){
        compile_logical(c,node);
        return;
    }
    compile_expr(c,node->left);
    compile_expr(c,node->right);
    emit_op(c,binary_op(node->op),node);
}
```

Le fils gauche, le fils droit, l'opérateur : c'est l'ordre de la machine à
pile qu'on a vu en 9.4. `binary_op` traduit un jeton (`TOKEN_PLUS`) en
instruction (`OP_ADD`). Le dernier argument de `emit_op`, `node`, est le noeud
qu'on range dans `chunk->nodes` pour les messages d'erreur.

Les fonctions d'émission sont toutes de la même famille :

```c fichier="src/vm/compiler.c" lignes="124-145"
static void emit_byte(Compiler *c, uint8_t byte, ast_t *node){
    chunk_write(current_chunk(c),byte,node);
}

static void emit_op(Compiler *c, OpCode op, ast_t *node){
    emit_byte(c,(uint8_t)op,node);
}

static void emit_u16(Compiler *c, int value, ast_t *node){
    emit_byte(c,(uint8_t)((value >> 8) & 0xFF),node);
    emit_byte(c,(uint8_t)(value & 0xFF),node);
}

static void emit_op_u8(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_byte(c,(uint8_t)value,node);
}

static void emit_op_u16(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_u16(c,value,node);
}
```

`emit_u16` écrit l'octet de poids fort d'abord, comme le veut `chunk.h`.
`current_chunk` rend le chunk de la fonction qu'on est en train de compiler :
on verra en 10.8 qu'il y en a une pile.

### Deux aiguillages

Comme dans le visitor, tout part de deux `switch`. `compile_expr` pour ce qui
a une valeur, `compile_statement` pour le reste. Voici la fin du second :

```c fichier="src/vm/compiler.c" lignes="1626-1635" fonction="compile_statement"
        case AST_INCLUDE: compile_include(c,node); break;
        case AST_EXPORT: compile_export(c,node); break;

        // Une expression seule : sa valeur ne sert à rien.
        default:
            compile_expr(c,node);
            emit_op(c,OP_POP,node);
            break;
    }
}
```

Ces trois lignes du `default` gardent la promesse du commentaire d'en-tête.
`println(x)` seul sur sa ligne est une expression : elle laisse le `none` de
`println` sur la pile. Le `POP` qui suit le retire, et la pile revient à ce
qu'elle était avant l'instruction. C'est le `POP` qu'on voyait après chaque
`CALL` au chapitre 9.

Pourquoi tenir autant à cette promesse ? Parce que c'est elle qui rend les
variables locales possibles. Si chaque instruction laisse la pile intacte,
alors à tout moment entre deux instructions, la pile ne contient que les
variables locales, dans l'ordre où on les a déclarées. La troisième variable
est toujours à la même place. Le compilateur peut lui donner un numéro de case
sans jamais exécuter le programme. On y vient en 10.3.

Enfin, le point d'entrée :

```c fichier="src/vm/compiler.c" lignes="1646-1661" fonction="compile_module"
ObjFunction *compile_module(VM *vm, ast_t *program, ObjModule *module){
    Compiler c = {.vm = vm,.module = module};
    table_init(&c.file_names);
    scan_file_names(&c,program);

    FuncState fs;
    func_state_begin(&c,&fs,FUNC_SCRIPT,program);
    compile_statements(&c,program);
    emit_op(&c,OP_NONE,program);
    emit_op(&c,OP_RETURN,program);
    ObjFunction *function = func_state_end(&c);

    table_free(&c.file_names);
    free(c.global_kinds);
    return function;
}
```

Le fichier entier devient une fonction sans paramètre. On compile ses
instructions, on ajoute `NONE` et `RETURN`, et on rend la fonction. C'est
le `NONE` / `RETURN` de la ligne 1 qu'on voyait à la fin de chaque
désassemblage. `src/vm/module.c` fait ensuite tout le chemin : il appelle le
parser, puis `compile_module`, puis la VM sur la fonction obtenue.

L'arbre n'est parcouru qu'une fois. Après `compile_module`, la VM ne
regardera plus jamais un noeud, sauf pour afficher une erreur.

## 10.2 Les constantes

Le cas le plus simple de `compile_expr` est un nombre, un caractère ou une
chaîne écrits dans le code :

```c fichier="src/vm/compiler.c" lignes="1563-1568" fonction="compile_expr"
    switch(node->type){
        case AST_NONE: emit_op(c,OP_NONE,node); break;
        case AST_BOOL: emit_op(c,node->bool_value ? OP_TRUE : OP_FALSE,node); break;
        case AST_CHAR:
        case AST_NUMBER:
        case AST_STRING: emit_constant(c,constant_of(c,node),node); break;
```

`none`, `true` et `false` ont leur propre instruction d'un octet : pas besoin
de constante pour eux. Les autres passent par `constant_of`, qui fabrique la
`Value`, et par `emit_constant` :

```c fichier="src/vm/compiler.c" lignes="147-160"
static int make_constant(Compiler *c, Value value, ast_t *node){
    int index = chunk_add_constant(current_chunk(c),value);
    if(index > 0xFFFF)
        visitor_error(node,"trop de constantes dans une fonction (65536 au plus)");
    return index;
}

static int name_constant(Compiler *c, const char *name, ast_t *node){
    return make_constant(c,OBJ_VAL(string_from(c->vm,name)),node);
}

static void emit_constant(Compiler *c, Value value, ast_t *node){
    emit_op_u16(c,OP_CONSTANT,make_constant(c,value,node),node);
}
```

Un opérande `u16` ne dépasse pas 65 535 : une fonction qui aurait plus de
constantes que ça reçoit une erreur claire, au lieu d'un numéro tronqué qui
lirait la mauvaise constante. C'est un réflexe à prendre en écrivant un
compilateur : chaque fois qu'on range un nombre dans un opérande, on vérifie
qu'il y tient. On retrouvera ce test pour les sauts, les locales et les
arguments.

`visitor_error` est la même fonction d'erreur que celle du visitor : elle
affiche le message sous le noeud et arrête le programme. Une erreur de
compilation ressemble donc à toutes les autres.

Une seule expression est calculée d'avance, dans `compile_unop` :

```c fichier="src/vm/compiler.c" lignes="946-953" fonction="compile_unop"
static void compile_unop(Compiler *c, ast_t *node){
    if(node->op == TOKEN_MINUS && node->right->type == AST_NUMBER){
        emit_constant(c,NUMBER_VAL(-node->right->number_value),node);
        return;
    }
    compile_expr(c,node->right);
    emit_op(c,node->op == TOKEN_NOT ? OP_NOT : OP_NEGATE,node);
}
```

Le lexer ne connaît pas les nombres négatifs : pour lui, `-3` est un `-` suivi
de `3`. Sans ce test, chaque `-3` du programme coûterait deux instructions,
`CONSTANT 3` puis `NEGATE`. Avec lui, la constante `-3` est rangée telle
quelle. C'est la réponse à la deuxième question de la fin du chapitre 9.

### Un premier compilateur qui tourne

Avec ce qu'on a écrit jusqu'ici, on peut déjà compiler de vrais programmes, à
condition qu'ils ne contiennent que des expressions et des appels de
`println`. Avant d'aller plus loin, on le vérifie pour de vrai. Le dépôt a déjà
sa VM : on s'en sert pour essayer notre compilateur, en attendant d'écrire la
nôtre au chapitre 11.

Crée `compiler_v1.c` dans un dossier d'essai. Il commence par
`#include "compiler.h"` et `#include "error.h"` (pour `visitor_error`), déclare
`static void compile_expr(Compiler *c, ast_t *node);` pour les appels
récursifs, et reprend `emit_byte`,
`emit_op`, `emit_op_u8`, `emit_op_u16`, `make_constant` et `binary_op` tels
qu'on les a vus, avec un `Compiler` réduit à deux champs :

```c fichier="compiler_v1.c"
typedef struct{
    VM *vm;
    ObjFunction *function;      // la fonction qu'on remplit
}Compiler;

static Chunk *current_chunk(Compiler *c){
    return &c->function->chunk;
}
```

Le `compile_expr` de cette première version ne connaît que sept sortes de
noeuds. Les noms, en particulier, sont tous des builtins : on n'a pas encore
de variables.

```c fichier="compiler_v1.c"
static void compile_expr(Compiler *c, ast_t *node){
    switch(node->type){
        case AST_NONE: emit_op(c,OP_NONE,node); break;
        case AST_BOOL: emit_op(c,node->bool_value ? OP_TRUE : OP_FALSE,node); break;
        case AST_NUMBER:
            emit_op_u16(c,OP_CONSTANT,make_constant(c,NUMBER_VAL(node->number_value),node),node);
            break;
        case AST_STRING:{
            Value text = OBJ_VAL(string_copy(c->vm,node->string_value,node->string_lenght));
            emit_op_u16(c,OP_CONSTANT,make_constant(c,text,node),node);
            break;
        }
        // Pour l'instant, un nom est forcément un builtin : println, print...
        case AST_VARIABLE:{
            Value name = OBJ_VAL(string_from(c->vm,node->variable_name));
            emit_op_u16(c,OP_GET_BUILTIN,make_constant(c,name,node),node);
            break;
        }
```

Un appel compile la fonction appelée, puis chaque argument, puis `CALL` avec
le nombre d'arguments : c'est l'ordre `[f a1..an]` que demande `chunk.h`.
Les opérateurs suivent la règle du fils gauche, du fils droit, puis de
l'opérateur :

```c fichier="compiler_v1.c"
        case AST_FUNCTION_CALL:{
            list_t *args = node->function_call_args;
            compile_expr(c,node->left);
            for(int i = 0; i < args->Size; i++)
                compile_expr(c,List_Get(args,i));
            emit_op_u8(c,OP_CALL,args->Size,node);
            break;
        }
        case AST_BINOP:
            compile_expr(c,node->left);
            compile_expr(c,node->right);
            emit_op(c,binary_op(node->op),node);
            break;
        case AST_UNOP:
            compile_expr(c,node->right);
            emit_op(c,node->op == TOKEN_NOT ? OP_NOT : OP_NEGATE,node);
            break;
        default:
            visitor_error(node,"pas encore compilé");
    }
}
```

Le `default` est important pendant qu'on construit : un noeud qu'on ne sait
pas encore compiler donne une erreur claire, à sa position, au lieu d'un
bytecode incomplet qui planterait plus loin. Il reste l'instruction et le
point d'entrée :

```c fichier="compiler_v1.c"
// Une instruction : une expression dont on jette la valeur.
static void compile_statement(Compiler *c, ast_t *node){
    compile_expr(c,node);
    emit_op(c,OP_POP,node);
}

ObjFunction *compile_module(VM *vm, ast_t *program, ObjModule *module){
    Compiler c = {.vm = vm,.function = function_new(vm,module)};
    c.function->definition = program;
    for(int i = 0; i < program->list_compound->Size; i++)
        compile_statement(&c,List_Get(program->list_compound,i));
    emit_op(&c,OP_NONE,program);
    emit_op(&c,OP_RETURN,program);
    return c.function;
}
```

On compile la VM du dépôt avec ce fichier à la place de `src/vm/compiler.c`
(dans le dossier d'essai, la commande est celle du `Makefile`, avec
`compiler_v1.c` au lieu de `src/vm/compiler.c`), et on l'essaie :

```ceflo fichier="expr.cf"
println(2 * (3 + 4) - 1)
println("moins : ", -5, " ", 10 / 4)
```

```terminal
$ ./Ceflo_c1 --dis expr.cf
== expr.cf ==
0000     1  GET_BUILTIN           0  "println"
0003     |  CONSTANT              1  2
0006     |  CONSTANT              2  3
0009     |  CONSTANT              3  4
0012     |  ADD
0013     |  MULTIPLY
0014     |  CONSTANT              4  1
0017     |  SUBTRACT
0018     |  CALL                  1
0020     |  POP
0021     2  GET_BUILTIN           0  "println"
0024     |  CONSTANT              5  "moins : "
0027     |  CONSTANT              6  5
0030     |  NEGATE
0031     |  CONSTANT              7  " "
0034     |  CONSTANT              8  10
0037     |  CONSTANT              3  4
0040     |  DIVIDE
0041     |  CALL                  4
0043     |  POP
0044     1  NONE
0045     |  RETURN

13
moins : -5 2.5
$ ./Ceflo_c1 boucle.cf
erreur: pas encore compilé
  --> boucle.cf:1:1
    1 | func somme(n){
      | ^
```

Une centaine de lignes de compilateur, et CeFlo calcule. Le `4` de la ligne 2
réutilise la constante 3 de la ligne 1 : c'est `chunk_add_constant` qui
dédoublonne. Et `-5` coûte encore deux instructions, `CONSTANT 5` puis
`NEGATE` : c'est exactement ce que corrige le test de `compile_unop` qu'on
vient de lire. La boucle du chapitre 9, elle, définit une fonction, et notre
`default` l'arrête proprement à la ligne 1. Le reste du chapitre fait tomber
ces `default` un par un, jusqu'au `compiler.c` du dépôt.

## 10.3 Les variables locales deviennent des cases

C'est le travail le plus important du compilateur, celui qui supprime les
cinq recherches de nom par tour de boucle. Le principe tient en une phrase :
le compilateur tient, pendant qu'il compile une fonction, la liste des
variables locales qui seraient sur la pile à ce moment de l'exécution.

```c fichier="src/vm/compiler.c" lignes="48-55"
typedef struct{
    ObjString *name;    // NULL : une case cachée (sujet d'un match, parcours d'un for ... in...)
    int depth;
    bool is_captured;   // une closure la garde : elle quitte la pile par OP_CLOSE_UPVALUE
    bool is_const;
    DefKind kind;
    TypeRef type;
}Local;
```

Une `Local` est ce que le compilateur sait d'une case : le nom de sa variable,
la profondeur du bloc qui l'a déclarée, si elle est constante, son type. Elle
n'existe qu'à la compilation. La VM ne verra que des numéros. Les `Local`
d'une fonction sont rangées dans son `FuncState` :

```c fichier="src/vm/compiler.c" lignes="90-100"
typedef struct FuncState{
    struct FuncState *enclosing;
    ObjFunction *function;
    FuncKind kind;
    StructInfo *structure;
    Local locals[LOCALS_MAX];
    int local_count;
    Upvalue upvalues[UPVALUES_MAX];
    int scope_depth;
    Loop *loop;
}FuncState;
```

`locals[i]` est la case `i` de la pile de la fonction. `scope_depth` compte
les blocs ouverts : 0 au niveau de la fonction, 1 dans un `if`, 2 dans un
`while` dans ce `if`.

### Déclarer, retrouver, oublier

Déclarer une variable, c'est ajouter une `Local` au bout du tableau :

```c fichier="src/vm/compiler.c" lignes="286-295" fonction="add_local"
static int add_local(Compiler *c, ObjString *name, ast_t *node){
    FuncState *fs = c->current;
    if(fs->local_count == LOCALS_MAX)
        visitor_error(node,"trop de variables locales dans une fonction (%d au plus)",LOCALS_MAX);

    Local *local = &fs->locals[fs->local_count];
    *local = (Local){.name = name,.depth = fs->scope_depth,.kind = DEF_VARIABLE};
    local->type = (TypeRef){TYPE_ANY,NULL};
    return fs->local_count++;
}
```

Remarque ce que cette fonction n'écrit pas : aucune instruction. Pour `var x
= a * 2`, le compilateur écrit le code de `a * 2`, qui laisse sa valeur au
sommet de la pile, puis appelle `add_local`. La valeur est déjà dans la bonne
case. Déclarer une variable ne coûte rien à l'exécution.

Retrouver une variable, c'est chercher son nom dans ce tableau, en partant de
la fin :

```c fichier="src/vm/compiler.c" lignes="322-328" fonction="find_local"
static int find_local(FuncState *fs, ObjString *name){
    for(int i = fs->local_count - 1; i >= 0; i--){
        if(fs->locals[i].name == name)
            return i;
    }
    return -1;
}
```

Partir de la fin, c'est trouver d'abord la variable du bloc le plus proche :
c'est le masquage, comme dans la chaîne des portées du chapitre 6. Et
`fs->locals[i].name == name` compare deux adresses, pas deux textes. Les noms
viennent de `string_from`, qui interne les chaînes : deux noms de même texte
sont le même objet. C'est une recherche par nom, oui, mais elle a lieu une
seule fois, à la compilation, et elle rend un numéro qu'on écrit dans le code.

Oublier, enfin, se fait à la sortie d'un bloc :

```c fichier="src/vm/compiler.c" lignes="302-313"
static void begin_scope(Compiler *c){
    c->current->scope_depth += 1;
}

static void end_scope(Compiler *c, ast_t *node){
    FuncState *fs = c->current;
    fs->scope_depth -= 1;
    while(fs->local_count > 0 && fs->locals[fs->local_count - 1].depth > fs->scope_depth){
        emit_pop_local(c,&fs->locals[fs->local_count - 1],node);
        fs->local_count -= 1;
    }
}
```

Chaque locale plus profonde que le bloc qu'on retrouve est retirée deux fois :
de la liste du compilateur (`local_count -= 1`) et de la pile de la VM (un
`POP`). `emit_pop_local` écrit un `CLOSE_UPVALUE` à la place du `POP` quand
une closure garde la variable : c'est le chapitre 12.

### Sur un exemple

```ceflo fichier="blocs.cf"
func essai(a){
    var x = a * 2
    if x > 5 {
        var y = x + 1
        var x = x + 10
        println(x, " ", y)
    }
    println(x)
}
essai(4)
```

```terminal
$ ./Ceflo --dis blocs.cf
== blocs.cf ==
0000     1  CLOSURE               0  <func essai>
0003     |  DEFINE_GLOBAL         0
0006    10  GET_GLOBAL            0
0009     |  CONSTANT              1  4
0012     |  CALL                  1
0014     |  POP
0015     1  NONE
0016     |  RETURN

== essai ==
0000     2  GET_LOCAL             1
0002     |  CONSTANT              0  2
0005     |  MULTIPLY
0006     3  GET_LOCAL             2
0008     |  CONSTANT              1  5
0011     |  GREATER
0012     |  JUMP_IF_FALSE        27  -> 0042
0015     4  GET_LOCAL             2
0017     |  CONSTANT              2  1
0020     |  ADD
0021     5  GET_LOCAL             2
0023     |  CONSTANT              3  10
0026     |  ADD
0027     6  GET_BUILTIN           4  "println"
0030     |  GET_LOCAL             4
0032     |  CONSTANT              5  " "
0035     |  GET_LOCAL             3
0037     |  CALL                  3
0039     |  POP
0040     4  POP
0041     |  POP
0042     8  GET_BUILTIN           4  "println"
0045     |  GET_LOCAL             2
0047     |  CALL                  1
0049     |  POP
0050     1  NONE
0051     |  RETURN

18 9
8
```

Suis les cases. La case 0 est réservée (on verra pourquoi en 10.8), `a` est la
case 1. `var x = a * 2` laisse son résultat en case 2. Dans le `if`, `y` prend
la case 3, et le second `x` la case 4.

Regarde l'octet 21. `var x = x + 10` lit `GET_LOCAL 2`, l'ancien `x`, alors
qu'on est en train de déclarer le nouveau. C'est voulu, et c'est ce que
faisait le visitor : la valeur se calcule avant que le nom existe.
`compile_one_definition` le dit dans un commentaire :

```c fichier="src/vm/compiler.c" lignes="617-630" fonction="compile_one_definition" focus="623-627"
    FuncState *fs = c->current;
    int slot = find_local_in_scope(fs,name);
    if(slot >= 0){
        type = merge_type(node,name,fs->locals[slot].type,type);
        compile_definition_value(c,node,type,name);
        emit_op_u8(c,OP_SET_LOCAL_POP,slot,node);
    }else{
        // La valeur se calcule avant que le nom existe : `var x = x + 1` lit l'ancien x.
        compile_definition_value(c,node,type,name);
        slot = add_local(c,name,node);
    }
    fs->locals[slot].type = type;
    fs->locals[slot].is_const = node->is_const;
    fs->locals[slot].kind = DEF_VARIABLE;
```

Si on avait appelé `add_local` avant `compile_definition_value`, le `x` de
`x + 10` aurait trouvé la nouvelle case 4, encore vide. Le premier cas, lui,
traite `var x` écrit deux fois dans le même bloc : la deuxième déclaration
réutilise la case de la première, comme une affectation.

À la fin du bloc, les octets 40 et 41 sont les deux `POP` de `end_scope`, pour
la case 4 puis la case 3. Après eux, `x` désigne de nouveau la case 2, et la
ligne 8 affiche `8`.

### Les blocs sans variable

Un dernier détail, qui vient du chapitre 6 :

```c fichier="src/vm/compiler.c" lignes="1300-1308" fonction="compile_compound"
static void compile_compound(Compiler *c, ast_t *node){
    if(!node->has_definition){
        compile_statements(c,node);
        return;
    }
    begin_scope(c);
    compile_statements(c,node);
    end_scope(c,node);
}
```

Le parser a déjà noté si un bloc déclare une variable (`has_definition`). Un
bloc qui n'en déclare pas n'ouvre pas de portée. Pour le visitor, c'était une
table de hachage qu'on n'allouait pas. Ici, ça ne change rien au code produit
(un bloc sans variable n'a rien à retirer), mais la règle reste la même dans
les deux interpréteurs, et c'est ce qu'on veut.

## 10.4 Les globales et les builtins

Un nom qui n'est pas une locale est cherché ailleurs. `resolve_name` essaie
dans l'ordre du commentaire d'en-tête :

```c fichier="src/vm/compiler.c" lignes="477-498" fonction="resolve_name"
static Name resolve_name(Compiler *c, const char *chars, ast_t *node){
    Name result = {.name = string_from(c->vm,chars)};
    result.type = (TypeRef){TYPE_ANY,NULL};

    if(resolve_in_function(c->current,result.name,&result,node))
        return result;

    Value unused;
    if(!c->has_import_all && !table_get(&c->file_names,result.name,&unused) &&
       table_get(&c->vm->builtins,result.name,&unused)){
        result.kind = NAME_BUILTIN;
        result.index = make_constant(c,OBJ_VAL(result.name),node);
        return result;
    }

    // Un nom inconnu devient une globale : la VM dira qu'elle n'est pas définie.
    result.kind = NAME_GLOBAL;
    result.index = global_index(c,result.name,node);
    result.is_const = c->module->is_const[result.index];
    result.type = c->module->types[result.index];
    return result;
}
```

`resolve_in_function` cherche dans les locales de la fonction, puis dans
celles des fonctions qui l'entourent (ce sont les upvalues du chapitre 12).
Si le nom n'y est pas, c'est un builtin ou une globale du fichier.

### Une globale, c'est une case du module

Un fichier compilé a son `ObjModule`, et ses globales sont un simple tableau
de valeurs. `module_global_index` rend la case d'un nom, et la crée si c'est
la première fois qu'on le voit :

```c fichier="src/vm/object.c" lignes="248-268" fonction="module_global_index" focus="262-266"
int module_global_index(VM *vm, ObjModule *module, ObjString *name){
    Value index;
    if(table_get(&module->indexes,name,&index))
        return (int)AS_NUMBER(index);

    int count = module->globals.count;
    if(count == module->capacity){
        int capacity = GROW_CAPACITY(module->capacity);
        module->names = VM_GROW_ARRAY(vm,ObjString*,module->names,module->capacity,capacity);
        module->types = VM_GROW_ARRAY(vm,TypeRef,module->types,module->capacity,capacity);
        module->is_const = VM_GROW_ARRAY(vm,bool,module->is_const,module->capacity,capacity);
        module->capacity = capacity;
    }

    value_array_write(&module->globals,UNDEFINED_VAL);
    module->names[count] = name;
    module->types[count] = (TypeRef){TYPE_ANY,NULL};
    module->is_const[count] = false;
    table_set(&module->indexes,name,NUMBER_VAL(count));
    return count;
}
```

Une case neuve vaut `UNDEFINED_VAL`, la valeur invisible du chapitre 9. Elle
existe dès la compilation, mais elle ne reçoit sa valeur qu'à l'exécution,
quand le `var` est atteint (`OP_DEFINE_GLOBAL`). Ça règle un problème qui
paraît insoluble au premier abord : une fonction qui utilise une globale
déclarée plus bas.

```ceflo fichier="globales.cf"
func salue(){
    println(message)
}
var message = "bonjour"
salue()
```

```terminal
$ ./Ceflo --dis globales.cf
== globales.cf ==
0000     1  CLOSURE               0  <func salue>
0003     |  DEFINE_GLOBAL         0
0006     4  CONSTANT              1  "bonjour"
0009     |  DEFINE_GLOBAL         1
0012     5  GET_GLOBAL            0
0015     |  CALL                  0
0017     |  POP
0018     1  NONE
0019     |  RETURN

== salue ==
0000     2  GET_BUILTIN           0  "println"
0003     |  GET_GLOBAL            1
0006     |  CALL                  1
0008     |  POP
0009     1  NONE
0010     |  RETURN

bonjour
```

Quand le compilateur arrive dans `salue`, il n'a jamais vu `message`. Il lui
donne quand même la case 1, et écrit `GET_GLOBAL 1`. Deux lignes plus bas,
`var message` trouve la case 1 déjà créée et écrit `DEFINE_GLOBAL 1`. À
l'exécution, `salue` est appelée après le `DEFINE_GLOBAL`, et lit `bonjour`.

Si on appelle `salue()` avant le `var`, la case vaut encore `UNDEFINED`, et
c'est la VM qui donne l'erreur, au bon endroit :

```terminal
$ ./Ceflo trop_tot.cf
erreur: `message` n'est pas définie
  --> trop_tot.cf:2:13
    2 |     println(message)
      |             ^
```

C'est le même message, à la même colonne, que celui de `Ceflo_ref`. Le
compilateur ne refuse pas un nom inconnu : il ne peut pas savoir si le code
qui l'utilise s'exécutera avant ou après la définition.

### Un builtin, sauf si le fichier le cache

Les builtins (`println`, `input`, `argv`...) sont dans une table de la VM,
visibles depuis tous les fichiers. Mais un fichier a le droit de définir son
propre `println`, et alors c'est le sien qui compte, même là où il n'est pas
encore défini. C'est le rôle de `file_names`, rempli par `scan_file_names`
avant de compiler quoi que ce soit : la liste des noms définis directement
dans le fichier, même plus bas. Un nom de cette liste n'est jamais un builtin.

```ceflo fichier="cache.cf"
println("avant")
func println(x){
    print("[", x, "] ")
}
println("après")
```

```terminal
$ ./Ceflo --dis cache.cf
== cache.cf ==
0000     1  GET_GLOBAL            0
0003     |  CONSTANT              0  "avant"
0006     |  CALL                  1
0008     |  POP
0009     2  CLOSURE               1  <func println>
0012     |  DEFINE_GLOBAL         0
0015     5  GET_GLOBAL            0
0018     |  CONSTANT              2  "après"
0021     |  CALL                  1
0023     |  POP
0024     1  NONE
0025     |  RETURN

== println ==
0000     3  GET_BUILTIN           0  "print"
0003     |  CONSTANT              1  "["
0006     |  GET_LOCAL             1
0008     |  CONSTANT              2  "] "
0011     |  CALL                  3
0013     |  POP
0014     2  NONE
0015     |  RETURN

avant
[après] 
```

Les deux appels sont `GET_GLOBAL 0`. Le premier arrive avant le
`DEFINE_GLOBAL 0` : la case vaut `UNDEFINED`. Plutôt qu'une erreur, la VM
cherche alors un builtin du même nom, et le vrai `println` affiche `avant`.
Le second appel trouve la fonction du fichier. C'est exactement ce que fait
`Ceflo_ref`, où la portée du fichier ne contient pas encore `println` à la
ligne 1, et où la recherche remonte jusqu'aux builtins.

## 10.5 Affecter

Une affectation range une valeur dans une case. Pour une locale, c'est
`SET_LOCAL_POP`, pour une globale `SET_GLOBAL` puis `POP`. La partie
intéressante est `+=`, et l'ordre dans lequel on fait les choses.

```c fichier="src/vm/compiler.c" lignes="1279-1294" fonction="compile_assignement"
    if(name.kind == NAME_BUILTIN){
        name.kind = NAME_GLOBAL;
        name.index = global_index(c,name.name,target);
    }
    if(is_add_local(&name,node)){
        emit_op_u8(c,OP_ADD_LOCAL,name.index,node);
        emit_u16(c,make_constant(c,constant_of(c,node->assignement_value),node),node);
        return;
    }
    if(compound)
        emit_get_name(c,&name,target);
    compile_assigned_value(c,node);
    if(name.kind != NAME_GLOBAL)
        emit_check(c,CHECK_VARIABLE,name.type,name.name,target);
    emit_store_name(c,&name,target);
}
```

D'abord le raccourci : `is_add_local` reconnaît `x += constante` sur une
locale sans type, et le remplace par `ADD_LOCAL`. C'est le choix qu'on a vu
faire pour `i += 1` au chapitre 9, et refuser pour `total += i`.

Sinon, pour `x += e`, on pousse `x` (`emit_get_name`), puis on compile `e` et
l'opération (`compile_assigned_value`), puis on range. `x` est donc lu
**avant** de calculer `e`. Le visitor faisait l'inverse, et la différence se
voit dès que `e` modifie `x` :

```ceflo fichier="affecte.cf"
var x = 1
func double_x(){
    x = x * 2
    return 10
}
x += double_x()
println(x)
```

```terminal
$ ./Ceflo affecte.cf
11
$ ./Ceflo_ref affecte.cf
12
```

La VM lit `x = 1`, appelle `double_x` (qui met `x` à 2 et rend 10), et range
`1 + 10`. Le visitor appelait d'abord `double_x`, puis lisait le nouveau `x`.
C'est l'une des rares différences voulues entre les deux interpréteurs, notée
dans `docs/VM.md` : Python, JavaScript et Java donnent tous `11`. Ça n'a pas
été un choix de goût. Sur une machine à pile, lire la cible d'abord est la
seule façon de ne l'évaluer qu'une fois.

On le voit mieux avec une liste :

```ceflo fichier="liste.cf"
func f(){
    var l = [1, 2, 3]
    l[1] += 5
    println(l)
}
f()
```

```terminal
$ ./Ceflo --dis liste.cf
...
== f ==
0000     2  CONSTANT              0  1
0003     |  CONSTANT              1  2
0006     |  CONSTANT              2  3
0009     |  LIST                  3
0012     3  GET_LOCAL             1
0014     |  CONSTANT              0  1
0017     |  DUP2
0018     |  GET_INDEX
0019     |  CONSTANT              3  5
0022     |  ADD
0023     |  SET_INDEX             2  -> 0028
0026     |  SET_LOCAL_POP         1
0028     4  GET_BUILTIN           4  "println"
0031     |  GET_LOCAL             1
0033     |  CALL                  1
0035     |  POP
0036     1  NONE
0037     |  RETURN

[ 1 , 7 , 3 ]
```

(Les `...` remplacent le chunk du fichier, sans intérêt ici.) `l` et `1` sont
poussés une seule fois. `DUP2` les double, `GET_INDEX` en consomme une copie
pour lire `l[1]`, et la seconde copie attend sous la somme pour `SET_INDEX`.
Si l'index était `f()`, `f` ne serait appelée qu'une fois.

`SET_INDEX` a un opérande de saut, et c'est une petite astuce. Une liste se
modifie sur place : `SET_INDEX` range la valeur et saute par-dessus le
`SET_LOCAL_POP`. Mais une chaîne ne change jamais (chapitre 5) : `s[0] = 'x'`
fabrique une nouvelle chaîne. Dans ce cas, `SET_INDEX` pose la nouvelle
chaîne sur la pile et ne saute pas, et le `SET_LOCAL_POP` qui suit la range
dans la variable à la place de l'ancienne. Le compilateur ne sait pas si `l`
sera une liste ou une chaîne, alors il écrit les deux chemins.

## 10.6 Les sauts en avant

Un `if` est la première instruction dont le compilateur ne connaît pas tout le
code au moment de l'écrire. Pour `if x > 5 { ... }`, il faut un
`JUMP_IF_FALSE` qui saute par-dessus le bloc. Mais quand on écrit ce saut, le
bloc n'est pas encore compilé : on ne connaît pas sa longueur.

La solution est vieille comme les assembleurs : on écrit un saut avec une
distance bidon, on retient où il est, et on revient corriger la distance quand
on arrive à destination.

```c fichier="src/vm/compiler.c" lignes="162-185"
// Retourne la place du décalage, à corriger avec patch_jump.
static int emit_jump(Compiler *c, OpCode op, ast_t *node){
    emit_op(c,op,node);
    emit_u16(c,0xFFFF,node);
    return current_chunk(c)->count - 2;
}

// Le saut arrive ici : le décalage se compte depuis l'octet qui suit le u16.
static void patch_jump(Compiler *c, int offset, ast_t *node){
    Chunk *chunk = current_chunk(c);
    int distance = chunk->count - offset - 2;
    if(distance > 0xFFFF)
        visitor_error(node,"ce bloc est trop long : un saut dépasse 65535 octets");
    chunk->code[offset] = (uint8_t)((distance >> 8) & 0xFF);
    chunk->code[offset + 1] = (uint8_t)(distance & 0xFF);
}

static void emit_loop(Compiler *c, int start, ast_t *node){
    emit_op(c,OP_LOOP,node);
    int distance = current_chunk(c)->count - start + 2;
    if(distance > 0xFFFF)
        visitor_error(node,"cette boucle est trop longue : un saut dépasse 65535 octets");
    emit_u16(c,distance,node);
}
```

`emit_jump` rend l'offset des deux octets de distance. `patch_jump`, appelé
plus tard, calcule la distance entre la fin du saut et l'endroit où on est
maintenant, et l'écrit à sa place. Le `-2` vient de ce que la distance se
compte depuis l'octet qui suit les deux octets d'opérande.

`emit_loop` est le cas facile : un saut en arrière va vers un endroit déjà
écrit. On connaît sa distance tout de suite. Le `+2` compte les deux octets
de l'opérande qu'on n'a pas encore écrits.

### if, else, et la condition

```c fichier="src/vm/compiler.c" lignes="1310-1324" fonction="compile_if"
static void compile_if(Compiler *c, ast_t *node){
    JumpList false_jumps = {0};
    compile_condition(c,node->if_expr,&false_jumps);
    compile_statement(c,node->if_body);

    ast_t *other = node->if_other != NULL ? node->if_other : node->else_body;
    if(other == NULL){
        jump_list_patch(c,&false_jumps,node);
        return;
    }
    int end = emit_jump(c,OP_JUMP,node);
    jump_list_patch(c,&false_jumps,node);
    compile_statement(c,other);
    patch_jump(c,end,node);
}
```

Sans `else`, les sauts « condition fausse » arrivent juste après le corps.
Avec un `else`, le corps se termine par un `JUMP` qui passe par-dessus le
`else`, et les sauts « condition fausse » arrivent au début du `else`. Un
`else if` n'a rien de spécial : c'est un `if` rangé dans `if_other`.

Pourquoi une *liste* de sauts ? À cause de `and` :

```c fichier="src/vm/compiler.c" lignes="959-967" fonction="compile_condition"
static void compile_condition(Compiler *c, ast_t *node, JumpList *false_jumps){
    if(node->type == AST_BINOP && node->op == TOKEN_AND){
        compile_condition(c,node->left,false_jumps);
        compile_condition(c,node->right,false_jumps);
        return;
    }
    compile_expr(c,node);
    jump_list_add(false_jumps,emit_jump(c,OP_JUMP_IF_FALSE,node));
}
```

Dans `if a and b`, si `a` est faux, inutile de regarder `b` : on sort. Si `b`
est faux, on sort aussi. Ça fait deux sauts vers le même endroit, qu'on
corrigera ensemble. Et on n'a jamais eu besoin de fabriquer le booléen
`a and b`.

```terminal
$ ./Ceflo --dis si.cf
...
== signe ==
0000     2  GET_LOCAL             1
0002     |  CONSTANT              0  0
0005     |  GREATER
0006     |  JUMP_IF_FALSE        16  -> 0025
0009     |  GET_LOCAL             1
0011     |  CONSTANT              1  100
0014     |  LESS
0015     |  JUMP_IF_FALSE         7  -> 0025
0018     3  CONSTANT              2  "petit positif"
0021     |  RETURN
0022     2  JUMP                  4  -> 0029
0025     5  CONSTANT              3  "autre"
0028     |  RETURN
0029     1  NONE
0030     |  RETURN

petit positif
```

C'est `if n > 0 and n < 100 { return "petit positif" } else { return
"autre" }`. Les deux `JUMP_IF_FALSE` vont au même `0025`, le début du `else`.

Ce désassemblage montre aussi deux instructions qui ne serviront jamais. Le
`JUMP` de l'octet 22 suit un `RETURN` : on n'y arrivera pas. Et la fonction se
termine par `NONE` et `RETURN`, alors que les deux branches retournent déjà.
Le compilateur ajoute ce retour final dès que la dernière instruction de la
fonction n'est pas un `return`, et ici la dernière instruction est un `if`.
Ces quatre octets morts ne coûtent rien à l'exécution. Les supprimer
demanderait de savoir qu'un `if` « retourne toujours », une analyse qu'on n'a
pas voulu écrire.

### and et or comme valeurs

Quand `a or b` n'est pas une condition, mais une valeur qu'on range dans une
variable, `compile_logical` prend le relais. En CeFlo, `and` et `or` donnent
toujours un booléen (c'est la règle du visitor, `makeBool` au chapitre 7) :

```terminal
$ ./Ceflo --dis ou.cf
== ou.cf ==
0000     1  CONSTANT              0  0
0003     |  DEFINE_GLOBAL         0
0006     2  CONSTANT              1  "texte"
0009     |  DEFINE_GLOBAL         1
0012     3  GET_GLOBAL            0
0015     |  JUMP_IF_FALSE         4  -> 0022
0018     |  TRUE
0019     |  JUMP                  4  -> 0026
0022     |  GET_GLOBAL            1
0025     |  TO_BOOL
0026     |  DEFINE_GLOBAL         2
0029     4  GET_BUILTIN           2  "println"
0032     |  GET_GLOBAL            2
0035     |  CALL                  1
0037     |  POP
0038     1  NONE
0039     |  RETURN

true
```

`var c = a or b` : si `a` est vrai, on pousse `true` et on saute à la fin.
Sinon, on calcule `b` et `TO_BOOL` le transforme en booléen. Python
afficherait `texte` ; CeFlo affiche `true`, comme l'a toujours fait le
visitor.

## 10.7 Les boucles

Une boucle `while`, on l'a lue au chapitre 9 : la condition, un saut en avant
pour sortir, le corps, un `LOOP` vers la condition.

```c fichier="src/vm/compiler.c" lignes="1402-1415" fonction="compile_while"
static void compile_while(Compiler *c, ast_t *node){
    int start = current_chunk(c)->count;
    JumpList false_jumps = {0};
    compile_condition(c,node->while_expr,&false_jumps);

    Loop loop;
    int depth = c->current->scope_depth;
    loop_begin(c,&loop,depth,depth,start);
    compile_statement(c,node->while_body);
    emit_loop(c,start,node);

    jump_list_patch(c,&false_jumps,node);
    loop_end(c,&loop,node);
}
```

`start` est l'offset de la condition, retenu avant de l'écrire. `Loop` est ce
dont `break` et `continue` auront besoin : la liste des `break` à corriger à la
fin de la boucle, l'endroit où `continue` doit aller, et la profondeur des
blocs de la boucle.

### break et continue

```c fichier="src/vm/compiler.c" lignes="1387-1400"
static void compile_break(Compiler *c, ast_t *node){
    Loop *loop = c->current->loop;
    emit_leave(c,loop->break_depth,node);
    jump_list_add(&loop->breaks,emit_jump(c,OP_JUMP,node));
}

static void compile_continue(Compiler *c, ast_t *node){
    Loop *loop = c->current->loop;
    emit_leave(c,loop->continue_depth,node);
    if(loop->continue_target >= 0)
        emit_loop(c,loop->continue_target,node);
    else
        jump_list_add(&loop->continues,emit_jump(c,OP_JUMP,node));
}
```

Un `break` est un `JUMP` vers la sortie, qu'on ne connaît pas encore : il va
dans `loop->breaks`, et `loop_end` les corrige tous. Mais avant de sauter, il
faut retirer de la pile les variables des blocs qu'on quitte. Un `break` au
fond de `while ... { var a ... if ... { var b ... break } }` doit faire deux
`POP`, sinon la pile garde `a` et `b`, et toutes les cases de la suite de la
fonction sont décalées d'autant. C'est `emit_leave` :

```c fichier="src/vm/compiler.c" lignes="316-320" fonction="emit_leave"
static void emit_leave(Compiler *c, int depth, ast_t *node){
    FuncState *fs = c->current;
    for(int i = fs->local_count - 1; i >= 0 && fs->locals[i].depth > depth; i--)
        emit_pop_local(c,&fs->locals[i],node);
}
```

Compare avec `end_scope` : la même boucle, sans `local_count -= 1`. Le
compilateur écrit les `POP`, mais ne les oublie pas, parce que le code qui suit
le `break` (dans le même bloc, même s'il ne sera jamais atteint) les utilise
encore. `end_scope` les retirera à la vraie fin du bloc.

Dans un `while`, `continue` revient à la condition, déjà écrite : `emit_loop`.
Dans un `for`, il doit aller aux actions (`i += 1`), qui sont écrites *après*
le corps. C'est un saut en avant, rangé dans `loop->continues` :

```c fichier="src/vm/compiler.c" lignes="1418-1440" fonction="compile_for"
static void compile_for(Compiler *c, ast_t *node){
    begin_scope(c);
    if(node->for_init != NULL)
        compile_statement(c,node->for_init);

    int start = current_chunk(c)->count;
    JumpList false_jumps = {0};
    if(node->for_condition != NULL)
        compile_condition(c,node->for_condition,&false_jumps);

    Loop loop;
    int depth = c->current->scope_depth;
    loop_begin(c,&loop,depth,depth,-1);
    compile_statement(c,node->for_body);

    jump_list_patch(c,&loop.continues,node);
    compile_statements(c,node->for_solve);
    emit_loop(c,start,node);

    jump_list_patch(c,&false_jumps,node);
    loop_end(c,&loop,node);
    end_scope(c,node);
}
```

Le `-1` passé à `loop_begin` dit « pas de cible connue pour `continue` ». Tout
le `for` est dans une portée : `var i` ne vit que dans la boucle.

```ceflo fichier="pour.cf"
func impairs(n){
    var total = 0
    for var i = 0; i < n; i += 1 {
        if i % 2 == 0 { continue }
        if i > 7 { break }
        total += i
    }
    return total
}
println(impairs(100))
```

```terminal
$ ./Ceflo --dis pour.cf
...
== impairs ==
0000     2  CONSTANT              0  0
0003     3  CONSTANT              0  0
0006     |  GET_LOCAL             3
0008     |  GET_LOCAL             1
0010     |  LESS
0011     |  JUMP_IF_FALSE        42  -> 0056
0014     4  GET_LOCAL             3
0016     |  CONSTANT              1  2
0019     |  MODULO
0020     |  CONSTANT              0  0
0023     |  EQUAL
0024     |  JUMP_IF_FALSE         3  -> 0030
0027     |  JUMP                 19  -> 0049
0030     5  GET_LOCAL             3
0032     |  CONSTANT              2  7
0035     |  GREATER
0036     |  JUMP_IF_FALSE         3  -> 0042
0039     |  JUMP                 14  -> 0056
0042     6  GET_LOCAL             2
0044     |  GET_LOCAL             3
0046     |  ADD
0047     |  SET_LOCAL_POP         2
0049     3  ADD_LOCAL             3    3  1
0053     |  LOOP                 50  -> 0006
0056     |  POP
0057     8  GET_LOCAL             2
0059     |  RETURN

16
```

Le `continue` de la ligne 4 est le `JUMP` de l'octet 27, vers `0049`, le
`i += 1`. Le `break` de la ligne 5 est le `JUMP` de l'octet 39, vers `0056`.
Le `POP` de l'octet 56 est le `end_scope` du `for`, qui retire `i`. Le
`break` et la condition fausse arrivent tous les deux dessus : dans les deux
cas, `i` est encore sur la pile et doit partir.

### for ... in : trois cases cachées

Parcourir une liste demande de se souvenir de trois choses d'un tour à
l'autre : la liste, la position, et pour un map la liste de ses clés, relevée
avant le premier tour. Le compilateur leur réserve trois cases sans nom :

```c fichier="src/vm/compiler.c" lignes="1451-1466" fonction="compile_for_in"
static void compile_for_in(Compiler *c, ast_t *node){
    begin_scope(c);
    int depth = c->current->scope_depth;

    compile_expr(c,node->for_condition);
    emit_op(c,OP_ITER_PREP,node);
    for(int i = 0; i < 3; i++)
        add_local(c,NULL,node);

    ast_t *first = node->for_init;
    ast_t *second = first->left;

    int start = current_chunk(c)->count;
    emit_op_u8(c,OP_ITER_NEXT,second != NULL ? 2 : 1,node);
    emit_u16(c,0xFFFF,node);
    int exit = current_chunk(c)->count - 2;
```

Une `Local` dont le nom est `NULL` ne sera jamais trouvée par `find_local` :
aucun nom du programme ne peut la désigner. `ITER_PREP` transforme la liste
au sommet en ces trois valeurs, et `ITER_NEXT`, à chaque tour, pose la ou les
variables du tour, ou saute à la sortie quand c'est fini.

```terminal
$ ./Ceflo --dis dans.cf
...
== noms ==
0000     2  GET_LOCAL             1
0002     |  ITER_PREP
0003     |  ITER_NEXT             2  -> 0025
0007     3  GET_BUILTIN           0  "println"
0010     |  GET_LOCAL             5
0012     |  CONSTANT              1  " : "
0015     |  GET_LOCAL             6
0017     |  CALL                  3
0019     |  POP
0020     2  POP
0021     |  POP
0022     |  LOOP                 22  -> 0003
0025     |  POP
0026     |  POP
0027     |  POP
0028     1  NONE
0029     |  RETURN

0 : Ada
1 : Alan
```

C'est `for var i, nom in l { println(i, " : ", nom) }`. Les cases 2, 3 et 4
sont cachées, `i` est la case 5, `nom` la case 6. Les deux `POP` des octets
20 et 21 retirent `i` et `nom` à la fin de *chaque* tour, et les trois `POP`
de la fin retirent les cases cachées. Chaque tour a donc ses propres
variables, posées par `ITER_NEXT` et retirées avant `LOOP`. Ça ne change rien
ici, mais tout au chapitre 12, quand une `lfunc` du corps capture `nom`.

`match` utilise la même astuce : le sujet est calculé une fois et attend dans
une case cachée, pendant que chaque `case` le compare avec `GET_LOCAL` et
`EQUAL`. On reviendra sur `match` avec les énumérations, au chapitre 13.

## 10.8 Les fonctions

Une fonction CeFlo est compilée dans son propre chunk. Le compilateur ouvre
donc un nouveau `FuncState`, et le chaîne à celui de la fonction qui
l'entoure :

```c fichier="src/vm/compiler.c" lignes="259-274" fonction="func_state_begin" focus="270-273"
static void func_state_begin(Compiler *c, FuncState *fs, FuncKind kind, ast_t *definition){
    fs->enclosing = c->current;
    fs->function = function_new(c->vm,c->module);
    fs->function->definition = definition;
    fs->kind = kind;
    fs->structure = NULL;
    fs->local_count = 0;
    fs->scope_depth = 0;
    fs->loop = NULL;
    c->current = fs;

    // La case 0 : self dans une méthode, la fonction appelée sinon.
    Local *slot = &fs->locals[fs->local_count++];
    *slot = (Local){.name = kind == FUNC_METHOD ? string_from(c->vm,"self") : NULL,.is_const = true};
    slot->type = (TypeRef){TYPE_ANY,NULL};
}
```

Voilà la case 0 qu'on mettait de côté depuis le chapitre 9. Dans un appel
`f(a, b)`, la VM pousse `f`, puis `a`, puis `b`. Quand la fonction démarre,
ses cases commencent sur `f` lui-même : la case 0 est la fonction appelée, la
case 1 le premier argument. Le compilateur réserve donc la case 0 sans nom
(on ne peut pas la lire depuis le programme) et les paramètres prennent les
cases suivantes. Dans une méthode, la case 0 s'appelle `self`, et l'objet est
posé à la place de la fonction : `self` devient une variable locale comme les
autres. C'est le chapitre 13.

Le reste de `compile_function` suit :

```c fichier="src/vm/compiler.c" lignes="680-686" fonction="compile_function"
    begin_scope(c);
    for(int i = 0; i < args->Size; i++){
        ast_t *arg = List_Get(args,i);
        int slot = add_local(c,string_from(c->vm,arg->variable_name),arg);
        fs.locals[slot].type = make_type(c,arg->variable_definition_static_type_name);
        function->param_types[i] = fs.locals[slot].type;
    }
```

Un paramètre est une locale déclarée avant le corps. Il n'y a rien à copier :
les arguments sont déjà sur la pile, à la bonne place, poussés par l'appelant.

```c fichier="src/vm/compiler.c" lignes="696-708" fonction="compile_function"
    ast_t *body = node->function_definition_body;
    compile_statements(c,body);

    list_t *statements = body->list_compound;
    bool ends_with_return = statements->Size > 0 &&
        ((ast_t*)List_Get(statements,statements->Size - 1))->type == AST_RETURN;
    if(!ends_with_return){
        emit_op(c,OP_NONE,node);
        emit_return(c,node);
    }

    emit_closure(c,node);
}
```

Le corps, le `return none` implicite qu'on a vu en 10.6, puis `emit_closure` :

```c fichier="src/vm/compiler.c" lignes="649-660" fonction="emit_closure"
// Ferme la fonction en cours et pose sa closure dans la fonction qui l'entoure.
static ObjFunction *emit_closure(Compiler *c, ast_t *node){
    FuncState *fs = c->current;
    ObjFunction *function = func_state_end(c);

    emit_op_u16(c,OP_CLOSURE,make_constant(c,OBJ_VAL(function),node),node);
    for(int i = 0; i < function->upvalue_count; i++){
        emit_byte(c,fs->upvalues[i].is_local ? 1 : 0,node);
        emit_byte(c,fs->upvalues[i].index,node);
    }
    return function;
}
```

`func_state_end` revient à la fonction qui entoure. La fonction compilée, un
`ObjFunction` avec son chunk, devient une **constante** de cette fonction-là,
et `OP_CLOSURE` la pose sur la pile à l'exécution. Une définition de fonction
est donc une expression comme une autre : elle pousse une valeur, que
`define_top` range ensuite dans une globale (`DEFINE_GLOBAL`) ou laisse dans
sa case locale. C'est le `CLOSURE 0` suivi de `DEFINE_GLOBAL 0` au début de
chaque fichier qu'on a désassemblé.

Les octets qui suivent `OP_CLOSURE` disent quelles variables des fonctions
englobantes la nouvelle fonction capture. Ce sont les upvalues, et elles ont
le chapitre 12. Pour une fonction du fichier comme `fib`, il n'y en a aucune :

```terminal
$ ./Ceflo --dis bench/fib.cf
...
== fib ==
0000     3  GET_LOCAL             1
0002     |  CONSTANT              0  2
0005     |  LESS
0006     |  JUMP_IF_FALSE         3  -> 0012
0009     |  GET_LOCAL             1
0011     |  RETURN
0012     4  GET_GLOBAL            0
0015     |  GET_LOCAL             1
0017     |  CONSTANT              1  1
0020     |  SUBTRACT
0021     |  CALL                  1
0023     |  GET_GLOBAL            0
0026     |  GET_LOCAL             1
0028     |  CONSTANT              0  2
0031     |  SUBTRACT
0032     |  CALL                  1
0034     |  ADD
0035     |  RETURN

832040
```

Trente-six octets. Le `fib` à l'intérieur de `fib` est `GET_GLOBAL 0` : la
fonction se trouve elle-même dans la case 0 du module, en une lecture de
tableau. Pour le visitor, c'était une recherche de `fib` dans la portée de
l'appel, puis dans celle du fichier, à chacun des 2,7 millions d'appels.

### Une fonction locale qui s'appelle elle-même

Il reste un piège. Une fonction définie *dans* une autre est une locale. Si
on la compile comme une variable (d'abord la valeur, ensuite `add_local`),
son propre nom n'existe pas encore pendant qu'on compile son corps, et un
appel récursif deviendrait une globale inconnue. `declare_early` crée la case
d'abord :

```c fichier="src/vm/compiler.c" lignes="584-592" fonction="declare_early"
// Une fonction ou une structure locale se voit elle-même : sa case existe avant son code.
// Retourne false si la case n'est pas encore créée (une globale, ou une redéfinition).
static bool declare_early(Compiler *c, ObjString *name, DefKind kind, ast_t *node){
    if(is_global_scope(c) || find_local_in_scope(c->current,name) >= 0)
        return false;
    int slot = add_local(c,name,node);
    c->current->locals[slot].kind = kind;
    return true;
}
```

La case existe pendant la compilation du corps, et la closure que pousse
`OP_CLOSURE` y atterrit ensuite, puisque c'est la case suivante de la pile.
La fonction se voit donc elle-même. Comme elle est dans une autre fonction,
elle se voit à travers une upvalue :

```ceflo fichier="locale.cf"
func main(){
    func fact(n){
        if n <= 1 { return 1 }
        return n * fact(n - 1)
    }
    println(fact(10))
}
```

```terminal
$ ./Ceflo locale.cf
3628800
```

Dans le désassemblage, l'appel récursif de `fact` est un `GET_UPVALUE 0`.
On expliquera cette ligne au chapitre 12.

## 10.9 Les types vérifiés

CeFlo laisse déclarer un type : `var n::int`, `func f(x::str)`. Le visitor
vérifiait ces types à chaque affectation, en relisant le nom du type dans
l'arbre. Le compilateur sait, lui, quelles lignes touchent une variable typée.
Il écrit à ces endroits-là, et seulement là, une instruction `CHECK_TYPE`.

```c fichier="src/vm/compiler.c" lignes="212-230" fonction="emit_check"
// OP_CHECK_TYPE, si la variable a un type. Une même vérification n'est rangée qu'une fois.
static void emit_check(Compiler *c, CheckKind kind, TypeRef type, ObjString *subject, ast_t *node){
    if(type.kind == TYPE_ANY)
        return;

    Chunk *chunk = current_chunk(c);
    int index = -1;
    for(int i = 0; i < chunk->check_count && index < 0; i++){
        TypeCheck *check = &chunk->checks[i];
        if(check->kind == kind && check->type.name == type.name && check->subject == subject)
            index = i;
    }
    if(index < 0)
        index = chunk_add_check(chunk,(TypeCheck){kind,type,subject});
    if(index > 0xFFFF)
        visitor_error(node,"trop de vérifications de type dans une fonction");

    emit_op_u16(c,OP_CHECK_TYPE,index,node);
}
```

La première ligne fait tout le gain : une variable sans type (`TYPE_ANY`) ne
coûte aucune vérification. Les autres reçoivent un `TypeCheck`, rangé dans le
tableau `checks` du chunk qu'on a laissé de côté au chapitre 9 : le genre de
vérification (variable, argument, retour), le type attendu, et le nom à citer
dans le message.

`TypeRef` (chapitre 9, `value.h`) range les types de base dans un `enum`
(`TYPE_INT`, `TYPE_STR`...), décidé une fois à la compilation par
`make_type`. Tester un `int` à l'exécution, c'est tester que la valeur est un
nombre entier, sans comparer de texte.

```ceflo fichier="types.cf"
func moyenne(a::int, b::int) : float {
    return (a + b) / 2
}
var compte::int
println(compte, " ", moyenne(3, 4))
println(moyenne(3, "4"))
```

```terminal
$ ./Ceflo --dis types.cf
== types.cf ==
0000     1  CLOSURE               0  <func moyenne>
0003     |  DEFINE_GLOBAL         0
0006     4  CONSTANT              1  0
0009     |  CHECK_TYPE            0  variable `compte` :: int
0012     |  DEFINE_GLOBAL         1
0015     5  GET_BUILTIN           2  "println"
0018     |  GET_GLOBAL            1
0021     |  CONSTANT              3  " "
0024     |  GET_GLOBAL            0
0027     |  CONSTANT              4  3
0030     |  CONSTANT              5  4
0033     |  CALL                  2
0035     |  CALL                  3
0037     |  POP
0038     6  GET_BUILTIN           2  "println"
0041     |  GET_GLOBAL            0
0044     |  CONSTANT              4  3
0047     |  CONSTANT              6  "4"
0050     |  CALL                  2
0052     |  CALL                  1
0054     |  POP
0055     1  NONE
0056     |  RETURN

== moyenne ==
0000     1  GET_LOCAL             1
0002     |  CHECK_TYPE            0  argument `a` :: int
0005     |  POP
0006     |  GET_LOCAL             2
0008     |  CHECK_TYPE            1  argument `b` :: int
0011     |  POP
0012     2  GET_LOCAL             1
0014     |  GET_LOCAL             2
0016     |  ADD
0017     |  CONSTANT              0  2
0020     |  DIVIDE
0021     |  CHECK_TYPE            2  retour `moyenne` :: float
0024     |  RETURN

0 3.5
erreur: `b` est de type int, pas str
  --> types.cf:6:16
    6 | println(moyenne(3, "4"))
      |                ^
```

Trois choses à voir. `var compte::int` sans valeur reçoit `0`, la valeur par
défaut de son type, écrite par `emit_default`. Chaque paramètre typé est
vérifié à l'entrée de la fonction : `GET_LOCAL`, `CHECK_TYPE`, `POP`, la pile
reste intacte. Et la valeur de retour est vérifiée juste avant `RETURN`.

Regarde aussi où pointe l'erreur. Le `CHECK_TYPE` qui échoue est dans
`moyenne`, à la ligne 1, mais le message montre la ligne 6, l'appel fautif.
Un mauvais argument est la faute de l'appelant, et c'est là que tu dois aller
corriger. La VM retrouve ce noeud dans le cadre d'appel de l'appelant, au
chapitre 11.

### Les erreurs trouvées avant de commencer

Un compilateur voit tout le fichier avant d'exécuter la première ligne. Il
peut donc refuser certaines erreurs tout de suite. Affecter une constante,
par exemple :

```ceflo fichier="constante.cf"
const PI = 3.14
println("avant")
PI = 3
```

```terminal
$ ./Ceflo constante.cf
erreur: `PI` est une constante
  --> constante.cf:3:1
    3 | PI = 3
      | ^
$ ./Ceflo_ref constante.cf
avant
erreur: `PI` est une constante
  --> constante.cf:3:1
    3 | PI = 3
      | ^
```

Le message est le même, mais la VM ne lance même pas le programme : `avant`
n'est jamais affiché. C'est `check_not_const`, appelé dans
`compile_assignement`, qui lit le `is_const` rangé par la déclaration. Une
fonction ou une structure définies deux fois dans la même portée sont
refusées de la même façon (`compile_function_definition`, `check_new_type`).
`docs/VM.md` range ces cas parmi les différences voulues : le programme
aurait échoué de toute façon, autant le dire avant qu'il ait écrit la moitié
d'un fichier.

## 10.10 format, compilé

Dernier morceau du compilateur, et le plus surprenant. CeFlo a un builtin
`format` qui remplace les `${...}` d'un texte par leur valeur :
`format("${nom} a ${age} ans")`. Le visitor le faisait à l'exécution : il
découpait le texte, lançait le lexer et le parser sur chaque `${...}`, et
exécutait l'arbre obtenu dans la portée courante.

La VM ne peut plus faire ça. À l'exécution, la portée courante n'existe plus :
il n'y a que des cases numérotées, et plus aucun nom. Si `format` reçoit
`"${nom}"` pendant l'exécution, il n'a aucun moyen de savoir que `nom` est la
case 1. La solution est de faire le travail plus tôt, à la compilation, quand
les noms existent encore. `compile_call` repère les appels à `format` dont
l'argument est un texte écrit dans le code :

```c fichier="src/vm/compiler.c" lignes="1086-1094" fonction="compile_call"
        if(is_format_call(&name,node)){
            bool is_printf = strcmp(name.name->chars,"printf") == 0;
            if(is_printf)
                emit_op_u16(c,OP_GET_BUILTIN,name_constant(c,"print",callee),callee);
            compile_format(c,node,List_Get(node->function_call_args,0));
            if(is_printf)
                emit_op_u8(c,OP_CALL,1,node);
            return;
        }
```

`printf(texte)` devient `print(format(texte))`. Et `compile_format` découpe le
texte. Les morceaux de texte simple deviennent des constantes ; chaque
`${...}` est donné au lexer et au parser, et l'arbre obtenu est compilé sur
place, comme s'il avait été écrit dans le programme :

```c fichier="src/vm/compiler.c" lignes="1029-1052" fonction="compile_format"
        }else if(at_expr){
            int start = i + 2;
            int depth = 1;
            int end = start;
            for(; end < length; end++){
                if(format[end] == '{')
                    depth += 1;
                if(format[end] == '}' && --depth == 0)
                    break;
            }
            if(depth != 0)
                visitor_error(node,"`format` : ${ n'est jamais fermé");

            char *source = ceflo_alloc((size_t)(end - start) + 1);
            memcpy(source,format + start,(size_t)(end - start));
            source[end - start] = '\0';
            lexer_t lexer;
            newLexerWithString(&lexer,"format",source);
            compile_expr(c,parser_parse_one(&lexer));
            free(source);

            parts += 1;
            has_expr = true;
            i = end;
```

Le compteur `depth` permet d'écrire une map dans un `${...}` sans que sa `}`
ferme l'expression trop tôt. Le lexer reçoit le nom de fichier `"format"`, et
c'est le seul détail qui trahit le tour de passe-passe, on va le voir.

```ceflo fichier="format.cf"
func presente(nom, age){
    println(format("${nom} a ${age + 1} ans l'an prochain"))
}
presente("Ada", 36)
var modele = "${nom}"
println(format(modele))
```

```terminal
$ ./Ceflo --dis format.cf
...
== presente ==
0000     2  GET_BUILTIN           0  "println"
0003     1  GET_LOCAL             1
0005     2  CONSTANT              2  " a "
0008     1  GET_LOCAL             2
0010     |  CONSTANT              3  1
0013     |  ADD
0014     2  CONSTANT              4  " ans l'an prochain"
0017     |  FORMAT                4
0019     |  CALL                  1
0021     |  POP
0022     1  NONE
0023     |  RETURN

Ada a 37 ans l'an prochain
erreur: `format` : le texte doit être écrit dans le code pour utiliser ${...}
  --> format.cf:6:15
    6 | println(format(modele))
      |               ^
```

L'appel à `format` a disparu. À sa place, quatre morceaux poussés l'un après
l'autre : `nom` (la case 1, lue par `GET_LOCAL`, sans aucun nom), `" a "`, le
calcul de `age + 1`, et la fin du texte. `FORMAT 4` les met bout à bout. Il
n'y a plus de lexer ni de parser à l'exécution : la chaîne finale est
assemblée en une instruction.

Le second appel, lui, reçoit une variable. Le texte n'existe qu'à
l'exécution, le compilateur n'a rien pu préparer, et le builtin `format`
refuse avec un message clair. C'est la seconde différence voulue de
`docs/VM.md` liée au compilateur.

> [!attention]
> Les lignes 1 des `GET_LOCAL` de `presente` ne sont pas une erreur du
> désassembleur. Les noeuds de `${nom}` viennent du petit lexer lancé sur
> `"nom"`, dans un fichier qui s'appelle `format` et qui n'a qu'une ligne. Une
> erreur dans un `${...}` s'affiche donc `--> format:1:4`, avec le texte de
> l'expression seule. `Ceflo_ref` fait exactement pareil ; le corriger demande
> de décaler les noeuds vers la vraie position du texte dans le fichier.

Le compilateur est complet. Il a transformé l'arbre en une suite d'octets où
chaque variable est un numéro, chaque boucle une paire de sauts, chaque type
un `CHECK_TYPE` posé au bon endroit. Il a même avalé les `${...}` de `format`.
Mais pour l'instant, ces octets dorment dans des chunks. Le chapitre 11 écrit
la boucle qui les réveille : celle qui lit `GET_LOCAL 1`, pousse la valeur, et
passe à l'octet suivant, plus de trois cents millions de fois par seconde.

## Défis

1. **Calculer d'avance.** Fais calculer au compilateur les opérations entre
   nombres écrits dans le code : `2 * (3 + 4) - 1` doit devenir un seul
   `CONSTANT 13`. Commence dans `compile_binop` : si les deux fils sont des
   `AST_NUMBER`, tu peux calculer tout de suite. Pour `2 * (3 + 4)`, le fils
   droit est un `AST_BINOP` : il faudra une fonction qui dit si un noeud a une
   valeur connue, et laquelle. Attention à `1 / 0` : l'erreur doit rester à
   l'exécution, à la bonne ligne. Vérifie avec `./Ceflo --dis` et `make test`.

2. **ADD_LOCAL pour les globales.** `var x = 1` puis `x += 1` au niveau du
   fichier n'utilise pas `ADD_LOCAL` : `x` est une globale. Ajoute
   `OP_ADD_GLOBAL` (dans `chunk.h`, `debug.c`, `compiler.c` et `vm.c`), choisie
   dans les mêmes conditions que `is_add_local`. `set_global`, dans
   `src/vm/vm.c`, vérifie que la globale est définie et n'est pas constante :
   ta version doit faire pareil. Mesure le gain sur une boucle écrite au
   niveau du fichier.

3. **Le code mort.** En 10.6, `signe` se termine par un `JUMP` et un
   `NONE`/`RETURN` qui ne serviront jamais. Fais en sorte que le compilateur
   n'écrive rien après un `return`, un `break` ou un `continue` dans le même
   bloc. Regarde `compile_statements` : il suffit d'arrêter la boucle.
   Attention aux blocs emboîtés : un `return` dans un `if` ne rend pas mort ce
   qui suit le `if`, et `end_scope` doit toujours écrire ses `POP` (le bloc
   peut être quitté par un autre chemin).

4. **Les erreurs de format au bon endroit.** Corrige le piège de 10.10 : une
   erreur dans un `${...}` doit pointer dans le vrai fichier, à la vraie
   colonne. Les noeuds viennent de `newLexerWithString` (`src/lexer.c`) ; il
   faudra donner au petit lexer le nom du fichier, la ligne et la colonne du
   texte, et vérifier que `error_node_v` (`src/error.c`) retrouve bien la
   ligne du source.

## Note de conception

### Une seule passe

Le compilateur de CeFlo lit l'arbre une seule fois. Il n'y a pas de passe pour
analyser les noms, puis une autre pour vérifier les types, puis une troisième
pour écrire le code : `compile_expr` fait tout en descendant. C'est le choix de
Lua, dont le compilateur écrit même le bytecode pendant qu'il lit les jetons,
sans construire d'arbre du tout. CPython et la plupart des compilateurs C font
l'inverse : plusieurs passes, chacune avec une seule tâche.

Une passe unique a deux qualités. Elle est rapide, ce qui compte pour un
langage de script qui recompile chaque fichier à chaque lancement. Et elle est
courte : `compiler.c` tient en 1 661 lignes, qu'on peut lire dans l'ordre. Le
prix se voit partout dans ce chapitre. `scan_file_names` est une mini-passe
cachée, juste pour savoir si un nom défini plus bas cache un builtin.
`declare_early` existe parce qu'une fonction locale doit se voir avant d'être
finie. Les sauts en avant sont écrits avec une distance bidon et corrigés plus
tard. Et la limite connue de `docs/VM.md` (une `lfunc` ne voit pas une locale
déclarée après elle) vient directement de là : quand on compile la `lfunc`, la
locale n'existe pas encore.

Ces trois astuces sont des passes qu'on n'a pas voulu écrire. À partir de
quand vaut-il mieux en écrire une vraie ? À mon avis, dès qu'on veut des
optimisations : calculer les constantes d'avance (le premier défi), supprimer
le code mort (le troisième), savoir qu'une variable n'est jamais capturée pour
lui éviter une upvalue. Chacune se greffe mal sur une passe unique, et
s'écrit en quelques dizaines de lignes sur un arbre. Pour CeFlo tel qu'il est,
un langage qu'on veut lire en un après-midi, la passe unique était le bon
choix. Pour un CeFlo qui voudrait aller encore deux fois plus vite, ce serait
la première chose à changer.

