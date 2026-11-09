---
numero: 12
titre: closures et upvalues
note: une variable qui survit
partie: Le bytecode
---

## 12.1 Le compteur qui ne devrait pas marcher

Voici le petit programme du `README`, celui qu'on montre pour dire qu'une
fonction CeFlo « se souvient de ses variables » :

```ceflo fichier="compteur.cf"
func Compteur() {
    var n = 0
    return lfunc() {
        n += 1
        return n
    }
}

var a = Compteur()
var b = Compteur()
println(a(), " ", a(), " ", a())
println(b())
```

```terminal
$ ./Ceflo compteur.cf
1 2 3
1
```

`a` compte 1, 2, 3. `b` a son propre `n` et repart de 1. Avec le visitor, ce
résultat n'avait rien d'étonnant : chaque appel créait une portée (`newScope`,
au chapitre 6), une vraie table allouée sur le tas, et la `lfunc` gardait un
pointeur vers elle. Tant que la `lfunc` vivait, la portée vivait aussi, et le
GC du chapitre 8 s'occupait du reste.

Au chapitre 10, on a jeté ces tables. Une variable locale est devenue une case
de la pile, et `n` est la case 1 de `Compteur` (la case 0 est la fonction
appelée elle-même). Regarde le bytecode de `Compteur` :

```terminal
$ ./Ceflo --dis compteur.cf
...
== Compteur ==
0000     2  CONSTANT              0  0
0003     3  CLOSURE               1  <func lfunc>
0006      |                       local 1
0008     |  RETURN
...
```

`CONSTANT 0` pose le `0` au sommet de la pile : c'est `n`, dans la case 1.
Puis `RETURN`, et au chapitre 11 on a écrit ce que fait `RETURN` : il remet
`stack_top` à `slots`. La case 1 n'appartient plus à personne. Le prochain
appel de fonction va l'écraser avec ses propres variables. Voici la pile de la
VM, case par case, pendant `var a = Compteur()` :

```text
pendant Compteur()            après le RETURN           pendant println(a(), ...)
+---------------------+       +---------------------+   +---------------------+
| 0  <script>         |       | 0  <script>         |   | 0  <script>         |
| 1  Compteur  slot 0 |       |                     |   | 1  println          |
| 2  n = 0     slot 1 |       |                     |   | 2  a        <- n ?  |
| 3  <lfunc>          |       |                     |   | ...                 |
+---------------------+       +---------------------+   +---------------------+
```

La case 2 de la pile, qui était `n`, contient `a` elle-même quelques
instructions plus tard. On verra en 12.5 que ce n'est pas une crainte théorique : sans le
code de ce chapitre, c'est exactement ce que lit la `lfunc`.

Et pourtant, trois appels plus tard, `a()` lit toujours son `n` et l'augmente.
La réponse est dans la ligne `local 1` sous le `CLOSURE`, qu'on n'a pas encore
expliquée. Elle dit « cette `lfunc` a besoin de la case 1 de la
fonction qui l'entoure ». Tout ce chapitre sert à comprendre ce que la VM fait
de cette ligne.

Le problème tient en une phrase : **une variable de pile doit parfois survivre
à sa fonction**. Les deux solutions évidentes sont mauvaises. Allouer toutes
les variables sur le tas, comme le visitor, c'est perdre tout ce qu'on a gagné
au chapitre 9. Copier la valeur dans la `lfunc` au moment où on la crée, c'est
faux dès que deux fonctions partagent la même variable, ou dès que la fonction
qui l'a créée la modifie encore après. On va voir ce cas dans un instant, et
il casse toutes les solutions simples.

La solution de CeFlo est celle de Lua, et celle que décrit Robert Nystrom
dans *Crafting Interpreters* : une petite boîte, l'**upvalue**, qui pointe sur
la case de la pile tant que la fonction tourne, et qui garde la valeur pour
elle quand la case disparaît.

Pour y arriver, on va construire, dans l'ordre :

1. les deux objets, `ObjUpvalue` et `ObjClosure` (12.2) ;
2. dans le compilateur, la recherche des variables capturées, et les octets
   qui suivent `OP_CLOSURE` (12.3) ;
3. dans la VM, la création des upvalues et leur partage (12.4) ;
4. leur fermeture, au retour d'une fonction et à la fin d'un bloc (12.5) ;
5. le cas des boucles, où une variable doit être neuve à chaque tour (12.6).

### Ce que font les autres

Tous les langages qui ont des closures ont dû résoudre ce problème, et ils ne
l'ont pas tous résolu pareil. Regarder leurs choix aide à comprendre le nôtre.

**Python** décide à la compilation, comme nous, quelles variables sont
capturées. Mais il les traite autrement dès le début de la fonction : une
variable capturée devient une **cell**, une petite boîte allouée sur le tas
au moment où la fonction démarre. Voici le compteur en Python 3.12 :

```python fichier="compteur.py"
import dis

def compteur():
    n = 0
    def f():
        nonlocal n
        n += 1
        return n
    return f

dis.dis(compteur)
```

```terminal
$ python3 compteur.py
              0 MAKE_CELL                1 (n)

  3           2 RESUME                   0

  4           4 LOAD_CONST               1 (0)
              6 STORE_DEREF              1 (n)

  5           8 LOAD_CLOSURE             1 (n)
             10 BUILD_TUPLE              1
...
```

`MAKE_CELL` est la première instruction : la boîte existe avant même que `n`
ait une valeur, et la fonction elle-même lit et écrit `n` à travers elle
(`STORE_DEREF`). C'est simple, et chaque accès à `n` dans `compteur` paie une
indirection, même si la closure n'est jamais créée.

**JavaScript**, dans V8, fait de même à une plus grande échelle : toutes les
variables capturées d'une fonction sont rangées dans un « contexte » alloué
sur le tas à l'entrée de la fonction.

**Java** a choisi de ne pas avoir le problème : une lambda ne peut capturer
qu'une variable *effectivement finale*, qu'on ne modifie plus. La lambda en
reçoit une copie, et `n += 1` dans une lambda est refusé à la compilation.
**C++** laisse le programmeur choisir, `[n]` par copie ou `[&n]` par
référence. Une capture par référence d'une variable locale qui a disparu est
un bug classique de C++, qui lit la pile comme notre dessin plus haut.

**Lua** a inventé la solution que CeFlo reprend : la variable reste sur la
pile, où la fonction qui la déclare la lit sans aucun coût, et elle n'est
déplacée sur le tas qu'au dernier moment, quand elle quitte la pile. Le
document de Roberto Ierusalimschy sur Lua 5.0 (« The Implementation of Lua
5.0 ») la décrit en deux pages.

> [!note]
> Le mot « upvalue » vient de Lua. Il désigne une variable qui vient « d'en
> haut », d'une fonction qui entoure celle qui l'utilise. Le visitor n'en
> avait pas besoin : chez lui, toutes les variables étaient déjà sur le tas.

## 12.2 Une boîte qui pointe sur la pile

Avant d'écrire du code, regarde le cas qui casse la copie. Deux `lfunc`
partagent le même `n`, et la fonction qui les crée continue de le lire :

```ceflo fichier="partage.cf"
func Paire() {
    var n = 0
    var plus = lfunc() { n += 1 }
    var lire = lfunc() { return n }
    plus()
    plus()
    println("dans Paire : ", n, " ", lire())
    return [plus, lire]
}

var p = Paire()
p[0]()
println("après : ", p[1]())
```

```terminal
$ ./Ceflo partage.cf
dans Paire : 2 2
après : 3
```

Pendant que `Paire` tourne, `plus` modifie `n` et `Paire` voit le changement :
`n` vaut 2 sur la pile. Après le retour, `plus` et `lire` continuent de
partager un seul `n`, qui passe à 3. Si chaque `lfunc` avait sa copie, la
première ligne afficherait `0 0`.

Il faut donc un objet que plusieurs closures désignent ensemble, et qui sache
où se trouve la valeur en ce moment. C'est `ObjUpvalue`. Dans
`src/vm/object.h`, l'énumération `ObjType` a déjà ses deux lignes depuis le
chapitre 11, `OBJ_CLOSURE` et `OBJ_UPVALUE` (lignes 31 et 32). On écrit les
deux types dans la section des fonctions, juste après `ObjFunction` :

```c fichier="src/vm/object.h" lignes="200-217" focus="202-207"
    // Une variable capturée. Tant que sa fonction tourne, elle vit sur la pile
    // (`location` pointe dans la pile) ; ensuite elle est copiée dans `closed`.
    typedef struct ObjUpvalue{
        Obj obj;
        Value *location;
        Value closed;
        struct ObjUpvalue *next;    // les upvalues encore ouvertes, triées par adresse
    }ObjUpvalue;

    typedef struct{
        Obj obj;
        ObjFunction *function;
        ObjUpvalue **upvalues;
        int upvalue_count;
    }ObjClosure;

    ObjClosure *closure_new(struct VM *vm, ObjFunction *function);
    ObjUpvalue *upvalue_new(struct VM *vm, Value *slot);
```

Tout tient dans le champ `location`. Une upvalue a deux vies :

- **ouverte** : la fonction qui a déclaré la variable tourne encore.
  `location` pointe dans la pile, sur la case de la variable. Lire l'upvalue,
  c'est lire la case. Écrire dedans, c'est écrire dans la case, et la fonction
  voit le changement sans rien savoir.
- **fermée** : la case va disparaître. On copie sa valeur dans `closed`, et on
  fait pointer `location` sur `closed`, à l'intérieur de l'upvalue elle-même.

Le code qui lit une upvalue ne fait jamais la différence : il écrit
`*upvalue->location`, ouverte ou fermée. C'est ce qui rend l'idée si peu
chère. Le passage de l'une à l'autre se fait une seule fois, au moment où la
case quitte la pile.

La deuxième structure est `ObjClosure`. Au chapitre 10, une fonction compilée
était un `ObjFunction` : son code, ses constantes, son nombre de paramètres.
C'est un objet fixe, créé une fois par le compilateur. Mais `Compteur()`
appelée deux fois donne deux `lfunc` différentes, avec le même code et deux
`n` différents. La closure est ce couple : un pointeur vers le code partagé,
et un tableau d'upvalues propre à cette création-là.

```c fichier="src/vm/object.c" lignes="117-138"
ObjClosure *closure_new(VM *vm, ObjFunction *function){
    ObjUpvalue **upvalues = NULL;
    if(function->upvalue_count > 0){
        upvalues = VM_ALLOCATE(vm,ObjUpvalue*,function->upvalue_count);
        for(int i = 0; i < function->upvalue_count; i++)
            upvalues[i] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(vm,ObjClosure,OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjUpvalue *upvalue_new(VM *vm, Value *slot){
    ObjUpvalue *upvalue = ALLOCATE_OBJ(vm,ObjUpvalue,OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NONE_VAL;
    upvalue->next = NULL;
    return upvalue;
}
```

On écrit ces deux fonctions dans `src/vm/object.c`, après `function_new`. Le
tableau est alloué avant l'objet, et ses cases valent `NULL` : la VM les
remplira juste après, en lisant les octets qui suivent `OP_CLOSURE`. Il n'y a
pas de piège avec le GC ici, puisqu'il ne tourne jamais pendant une
allocation (c'est la règle de `memory.h`, vue au chapitre 11).

Dans CeFlo, toute fonction est une closure, même celles qui ne capturent rien.
`println(Compteur)` et `CALL` voient toujours un `ObjClosure`. Une closure
sans upvalue coûte un objet de plus, une seule fois par définition, et la VM
n'a qu'un seul cas à traiter dans `OP_CALL`. C'est pour ça que le bytecode du
fichier commence par `CLOSURE 0 <func Compteur>`, et pas par un
`CONSTANT`.

### Le GC doit les connaître

Comme pour tout nouvel objet, trois endroits de `src/vm/memory.c` doivent le
connaître. `object_size` donne `sizeof(ObjClosure)` et `sizeof(ObjUpvalue)`
(lignes 66 et 67). `object_free` libère le tableau d'upvalues d'une closure
(lignes 101 à 105), mais pas les upvalues elles-mêmes : ce sont des objets du
GC, partagés, qui vivent leur propre vie. Et `gc_blacken` marque ce que chacun
tient :

```c fichier="src/vm/memory.c" lignes="224-233"
        case OBJ_CLOSURE:{
            ObjClosure *closure = (ObjClosure*)object;
            gc_mark_object(vm,(Obj*)closure->function);
            for(int i = 0; i < closure->upvalue_count; i++)
                gc_mark_object(vm,(Obj*)closure->upvalues[i]);
            break;
        }
        case OBJ_UPVALUE:
            gc_mark_value(vm,((ObjUpvalue*)object)->closed);
            break;
```

Une upvalue marque `closed`, et jamais `*location`. Tant qu'elle est ouverte,
`closed` vaut `none` et la vraie valeur est sur la pile, que le GC marque de
toute façon. Une fois fermée, `closed` est la valeur. `gc_mark_object`
accepte un pointeur `NULL` (ligne 148) : une closure dont la VM n'a pas encore
rempli toutes les cases peut être marquée sans danger.

On peut compiler : `make` passe, et rien n'a changé dans le comportement. Il
reste à créer ces objets.

> [!essaie]
> Ajoute à `partage.cf` une troisième `lfunc`, `remet`, qui écrit `n = 0`.
> Appelle-la depuis le haut du fichier, puis `p[1]()`. Tu dois lire `0` :
> les trois closures partagent bien une seule boîte.

## 12.3 Le compilateur repère les variables capturées

La VM ne cherche jamais un nom pendant l'exécution, on l'a promis au chapitre
9. C'est donc le compilateur qui doit savoir, dès la compilation, que `n` dans
la `lfunc` n'est ni une locale ni une globale, mais la case 1 de la fonction
qui l'entoure.

Il le sait parce que chaque fonction en cours de compilation a son
`FuncState`, et que ces `FuncState` forment une chaîne par leur champ
`enclosing` : la `lfunc` pointe sur `Compteur`, qui pointe sur le code du
fichier. C'est la chaîne des portées du visitor, mais elle n'existe que
pendant la compilation. Chaque `FuncState` tient deux tableaux :

```c fichier="src/vm/compiler.c" lignes="48-60" focus="51,57-60"
typedef struct{
    ObjString *name;    // NULL : une case cachée (sujet d'un match, parcours d'un for ... in...)
    int depth;
    bool is_captured;   // une closure la garde : elle quitte la pile par OP_CLOSE_UPVALUE
    bool is_const;
    DefKind kind;
    TypeRef type;
}Local;

typedef struct{
    uint8_t index;
    bool is_local;      // une locale de la fonction qui entoure, sinon une de ses upvalues
}Upvalue;
```

Le tableau `upvalues` s'ajoute au `FuncState` du chapitre 10, à côté de
`locals` (ligne 97 de `compiler.c`) : `Upvalue upvalues[UPVALUES_MAX]`, avec
`UPVALUES_MAX` à 256. Le nombre d'entrées utilisées n'est pas dans le
`FuncState` mais dans la fonction compilée, `function->upvalue_count`, parce
que la VM en aura besoin pour allouer le tableau de la closure.

`Local` est une case de la pile, vue au chapitre 10. Il gagne ici le champ
`is_captured`, qui servira à la fin de la portée. `Upvalue` est nouveau : une
entrée dit d'où vient la variable. Si `is_local` est vrai, `index` est une case
de la pile de la fonction qui entoure. Sinon, c'est une upvalue de cette
fonction-là, qu'elle a elle-même reçue d'encore plus haut.

La recherche d'un nom est `resolve_in_function`. Son début cherche dans les
locales, puis dans les attributs si on est dans une méthode (on en parlera au
chapitre 13). La partie qui nous intéresse vient après :

```c fichier="src/vm/compiler.c" lignes="457-475" fonction="resolve_in_function" focus="457-458,462-467"
    if(fs->enclosing == NULL || !resolve_in_function(fs->enclosing,name,out,node))
        return false;

    // Trouvé plus loin : la fonction le capture.
    FuncState *outer = fs->enclosing;
    if(out->kind == NAME_LOCAL){
        outer->locals[out->index].is_captured = true;
        out->index = add_upvalue(fs,out->index,true,node);
        out->kind = NAME_UPVALUE;
    }else if(out->kind == NAME_UPVALUE){
        out->index = add_upvalue(fs,out->index,false,node);
    }else{
        if(out->self_kind == NAME_LOCAL)
            outer->locals[out->self_index].is_captured = true;
        out->self_index = add_upvalue(fs,out->self_index,out->self_kind == NAME_LOCAL,node);
        out->self_kind = NAME_UPVALUE;
    }
    return true;
}
```

La fonction s'appelle elle-même sur `fs->enclosing`. Si le nom n'est trouvé
nulle part dans la chaîne, elle retourne `false`, et `resolve_name` en fera une
globale ou un builtin. S'il est trouvé, il y a deux cas :

1. Le nom est une **locale de la fonction juste au-dessus**. On marque cette
   locale `is_captured`, et on ajoute à notre fonction une upvalue
   `is_local = true` qui désigne sa case.
2. Le nom est une **upvalue de la fonction juste au-dessus**. Il vient de plus
   haut encore. On ajoute une upvalue `is_local = false` qui désigne l'upvalue
   de la fonction au-dessus.

Le troisième bloc, `else`, traite un attribut lu sans `self.` depuis une
`lfunc` écrite dans une méthode : c'est `self` qu'on capture alors. On le
retrouvera au chapitre 13.

Le deuxième cas est celui qu'on oublie. Ce programme le montre :

```ceflo fichier="imbrique.cf"
func Externe() {
    var x = "externe"
    func Milieu() {
        return lfunc() { return x }
    }
    return Milieu()
}

var f = Externe()
println(f())
```

```terminal
$ ./Ceflo --dis imbrique.cf
...
== Externe ==
0000     2  CONSTANT              0  "externe"
0003     3  CLOSURE               1  <func Milieu>
0006      |                       local 1
0008     6  GET_LOCAL             2
0010     |  CALL                  0
0012     |  RETURN

== Milieu ==
0000     4  CLOSURE               0  <func lfunc>
0003      |                       upvalue 0
0005     |  RETURN

== lfunc ==
0000     4  GET_UPVALUE           0
0002     |  RETURN

externe
```

`Milieu` n'utilise jamais `x`. Pourtant elle a une upvalue : la `lfunc` en a
besoin, et quand la `lfunc` est créée, elle ne peut prendre `x` que dans les
mains de `Milieu`. La récursion de `resolve_in_function` a donc tiré `x` à
travers toutes les fonctions intermédiaires. `Externe` dit à `Milieu` « prends
ma case 1 » (`local 1`), et `Milieu` dit à la `lfunc` « prends mon upvalue 0 »
(`upvalue 0`).

Arrête-toi sur l'ordre dans lequel les cas se présentent. `resolve_in_function`
cherche d'abord dans la fonction courante, et ne remonte qu'en cas d'échec.
Un paramètre `n` de la `lfunc` cacherait donc le `n` de `Compteur`, comme dans
le visitor. Et la récursion modifie chaque fonction intermédiaire **en
remontant** : c'est `Milieu` qui reçoit son upvalue avant la `lfunc`, puisque
l'appel sur `fs->enclosing` revient avant qu'on ajoute l'upvalue de `fs`.

Une dernière fonction, `add_upvalue`, évite les doublons :

```c fichier="src/vm/compiler.c" lignes="418-429"
static int add_upvalue(FuncState *fs, int index, bool is_local, ast_t *node){
    int count = fs->function->upvalue_count;
    for(int i = 0; i < count; i++){
        if(fs->upvalues[i].index == index && fs->upvalues[i].is_local == is_local)
            return i;
    }
    if(count == UPVALUES_MAX)
        visitor_error(node,"une fonction capture trop de variables (%d au plus)",UPVALUES_MAX);

    fs->upvalues[count] = (Upvalue){(uint8_t)index,is_local};
    return fs->function->upvalue_count++;
}
```

Une `lfunc` qui lit `n` dix fois n'a qu'une upvalue pour `n`. L'index
retourné est celui qu'on écrit après `OP_GET_UPVALUE`, sur un octet : d'où la
limite de 256.

Reste à écrire les octets qui suivent `OP_CLOSURE`. C'est le travail de
`emit_closure`, appelée à la fin de `compile_function` :

```c fichier="src/vm/compiler.c" lignes="650-660" fonction="emit_closure"
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

L'ordre compte. `func_state_end` remonte d'un cran (`c->current` redevient la
fonction qui entoure), puis on écrit l'instruction dans le code de celle-ci.
On garde `fs` de côté avant de remonter : c'est lui qui a la liste des
upvalues, et il vit sur la pile du C de `compile_function` jusqu'à son
retour.

`OP_CLOSURE` est la seule instruction de CeFlo dont la taille varie : trois
octets, plus deux par upvalue. Son commentaire dans `chunk.h` le dit :

```c fichier="src/vm/chunk.h" lignes="81-82"
        OP_CLOSURE,         // u16 k, puis pour chaque upvalue : u8 is_local, u8 index
                            //                 [] -> [closure de la fonction constants[k]]
```

Les trois autres instructions du chapitre sont plus simples, et on les ajoute
à l'énumération dans la section des variables : `OP_GET_UPVALUE` et
`OP_SET_UPVALUE` avec un opérande d'un octet (lignes 43 et 44), et
`OP_CLOSE_UPVALUE` sans opérande (ligne 49). Le désassembleur, lui, a besoin
d'une fonction à part pour `OP_CLOSURE`, puisqu'il doit savoir combien de
paires suivent :

```c fichier="src/vm/debug.c" lignes="186-201" fonction="closure"
static int closure(Chunk *chunk, int offset){
    int k = read_u16(chunk,offset + 1);
    printf("%-18s %4d  ",opcode_name(OP_CLOSURE),k);
    print_constant(chunk->constants.values[k]);
    printf("\n");
    offset += 3;

    ObjFunction *function = AS_FUNCTION(chunk->constants.values[k]);
    for(int i = 0; i < function->upvalue_count; i++){
        int is_local = chunk->code[offset];
        int index = chunk->code[offset + 1];
        printf("%04d      |    %-18s %s %d\n",offset,"",is_local ? "local" : "upvalue",index);
        offset += 2;
    }
    return offset;
}
```

Le nombre de paires se lit dans la fonction constante, `upvalue_count`. Les
lignes `local 1` et `upvalue 0` du désassemblage viennent du `printf` de la
ligne 197, dont le format décale la barre d'un caractère par rapport aux
autres lignes : c'est pour ça que `|` n'est pas aligné dans nos sorties.

> [!attention]
> Un nom se résout au moment où le compilateur le lit. Dans une fonction,
> `var g = lfunc(){ return y }` suivi de `var y = 3` donne
> « `y` n'est pas définie » : quand la `lfunc` est compilée, `y` n'est pas
> encore une locale, et devient une globale qui n'existe pas. L'ancien
> interpréteur, qui cherchait le nom à l'exécution, affichait 3. C'est une des
> limites écrites dans `docs/VM.md`.

## 12.4 OP_CLOSURE ouvre les boîtes

Côté VM, `OP_CLOSURE` lit la fonction dans les constantes, crée la closure,
puis lit les paires d'octets une par une :

```c fichier="src/vm/vm.c" lignes="873-883" focus="876,880"
    CASE(OP_CLOSURE):{
        ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure *closure = closure_new(vm,function);
        PUSH(OBJ_VAL(closure));
        for(int i = 0; i < closure->upvalue_count; i++){
            int is_local = READ_BYTE();
            int index = READ_BYTE();
            closure->upvalues[i] = is_local ? capture_upvalue(vm,slots + index) : frame->closure->upvalues[index];
        }
        NEXT();
    }
```

La ligne 880 est le chapitre entier en une ligne. Si la variable est une case
de la fonction qui tourne (`is_local`), on capture l'adresse `slots + index`.
Sinon, la fonction qui tourne a déjà cette upvalue dans sa propre closure
(`frame->closure->upvalues[index]`) : on copie le pointeur, et les deux
closures partagent la même boîte.

Les instructions qui lisent et écrivent tiennent sur une ligne chacune :

```c fichier="src/vm/vm.c" lignes="718-719"
    CASE(OP_GET_UPVALUE): PUSH(*frame->closure->upvalues[READ_BYTE()]->location); NEXT();
    CASE(OP_SET_UPVALUE): *frame->closure->upvalues[READ_BYTE()]->location = PEEK(0); NEXT();
```

Deux indirections de plus qu'un `OP_GET_LOCAL`, et aucun test pour savoir si
l'upvalue est ouverte ou fermée.

Il reste `capture_upvalue`, qui doit régler le cas de `partage.cf` : quand
`plus` puis `lire` capturent la case 1 de `Paire`, elles doivent recevoir la
**même** upvalue. Sinon `plus` écrirait dans une boîte et `lire` lirait dans
une autre, dès qu'elles seraient fermées. La VM garde donc la liste de toutes
les upvalues encore ouvertes, dans `vm->open_upvalues`, et la consulte avant
d'en créer une :

```c fichier="src/vm/vm.c" lignes="121-138" fonction="capture_upvalue"
static ObjUpvalue *capture_upvalue(VM *vm, Value *slot){
    ObjUpvalue *previous = NULL;
    ObjUpvalue *upvalue = vm->open_upvalues;
    while(upvalue != NULL && upvalue->location > slot){
        previous = upvalue;
        upvalue = upvalue->next;
    }
    if(upvalue != NULL && upvalue->location == slot)
        return upvalue;

    ObjUpvalue *created = upvalue_new(vm,slot);
    created->next = upvalue;
    if(previous == NULL)
        vm->open_upvalues = created;
    else
        previous->next = created;
    return created;
}
```

Avec trois fonctions emboîtées qui ont chacune une variable capturée, la
liste ressemble à ceci (les numéros de cases sont un exemple) :

```text
vm->open_upvalues
   |
   v
+--------------+     +--------------+     +--------------+
| case 9       | --> | case 5       | --> | case 2       | --> NULL
| (Milieu)     |     | (Externe)    |     | (Paire)      |
+--------------+     +--------------+     +--------------+
  le plus haut                               le plus bas
```

La liste est triée par adresse, de la plus haute dans la pile à la plus
basse. La boucle avance tant que les upvalues désignent des cases plus hautes
que celle qu'on cherche. Elle s'arrête soit sur la bonne case (on la
retourne), soit à l'endroit où la nouvelle doit être insérée pour garder
l'ordre. On comprendra à la section suivante pourquoi ce tri vaut la peine.

### Et si on ne cherchait pas ?

Les lignes 128 et 129, qui retournent l'upvalue déjà ouverte, ont l'air d'une
optimisation : sans elles, on créerait seulement une upvalue de trop. J'ai
compilé une copie de `src/` sans ces deux lignes, `Ceflo_sans_partage` :

```terminal
$ ./Ceflo_sans_partage partage.cf
dans Paire : 2 2
après : 2
$ ./Ceflo_sans_partage compteur.cf
1 2 3
1
```

Le compteur marche, et la première ligne de `partage.cf` aussi : tant que
`Paire` tourne, les deux upvalues pointent sur la même case, et partagent sans
le savoir. Le bug arrive au retour. Chaque upvalue copie la case dans son
propre `closed`, et `plus` et `lire` ont maintenant chacune leur `n`. `p[0]()`
augmente le sien, `p[1]()` lit l'autre, et affiche 2 au lieu de 3. C'est le
pire genre de bug : il ne se voit que quand deux closures partagent une
variable **et** survivent à leur fonction.

Dans `partage.cf`, le bytecode de `Paire` montre les deux captures de la même
case, et un détail qu'on pourrait trouver inquiétant :

```terminal
$ ./Ceflo --dis partage.cf
...
== Paire ==
0000     2  CONSTANT              0  0
0003     3  CLOSURE               1  <func lfunc>
0006      |                       local 1
0008     4  CLOSURE               2  <func lfunc>
0011      |                       local 1
0013     5  GET_LOCAL             2
0015     |  CALL                  0
0017     |  POP
0018     6  GET_LOCAL             2
0020     |  CALL                  0
0022     |  POP
0023     7  GET_BUILTIN           3  "println"
0026     |  CONSTANT              4  "dans Paire : "
0029     |  GET_LOCAL             1
...
```

À l'octet 29, `Paire` lit `n` avec un simple `GET_LOCAL 1`, alors que `n` est
capturée. C'est correct : tant que `Paire` tourne, l'upvalue est ouverte et
pointe sur cette case. `plus` écrit dans la case, `Paire` lit la case. La
fonction qui déclare une variable ne paie jamais rien pour qu'une autre la
capture.

### La fonction qui se capture elle-même

Voici le détail qui fait plaisir. Une fonction locale récursive :

```ceflo fichier="recursive.cf"
func Calcul() {
    func fact(n) {
        if n <= 1 { return 1 }
        return n * fact(n - 1)
    }
    return fact(5)
}
println(Calcul())
```

```terminal
$ ./Ceflo --dis recursive.cf
...
== Calcul ==
0000     2  CLOSURE               0  <func fact>
0003      |                       local 1
0005     6  GET_LOCAL             1
0007     |  CONSTANT              1  5
0010     |  CALL                  1
0012     |  RETURN

== fact ==
...
0013     4  GET_LOCAL             1
0015     |  GET_UPVALUE           0
0017     |  GET_LOCAL             1
...
120
```

Dans `fact`, le nom `fact` n'est pas une locale : c'est la case 1 de
`Calcul`, celle où la closure va être rangée. `declare_early`, au chapitre 10,
a créé cette case avant de compiler le corps, justement pour qu'une fonction
locale se voie elle-même. `fact` capture donc la case qui va la contenir.

Regarde l'ordre dans `OP_CLOSURE` : `PUSH` (ligne 876) vient **avant** la
boucle de capture. Quand `capture_upvalue(vm, slots + 1)` s'exécute, la case 1
contient déjà la closure de `fact`. Si on avait inversé les deux, l'upvalue
désignerait la bonne case quand même (elle pointe sur une adresse, pas sur
une valeur), mais il aurait suffi d'un GC entre les deux pour perdre la
closure qui n'est encore sur aucune pile. Avec le `PUSH` d'abord, la closure
est sur la pile, donc vivante, pendant toutes ses captures.

> [!essaie]
> Écris une fonction `Liste()` qui crée une liste locale `l` et retourne deux
> `lfunc` : `ajoute(x)` qui fait `l.append(x)`, et `taille()`. Désassemble :
> combien d'upvalues a chaque `lfunc` ? Ajoute ensuite une troisième `lfunc`
> qui en contient une quatrième qui lit `l`, et repère la ligne `upvalue 0`.

## 12.5 Fermer la boîte quand la case disparaît

Une upvalue ouverte pointe dans la pile. Si on ne fait rien, après le retour de
`Compteur`, elle pointerait sur une case réutilisée par le prochain appel, et
`a()` lirait n'importe quoi. Il faut fermer chaque upvalue au moment exact où
sa case quitte la pile. Ce moment arrive dans deux situations.

La première est le retour de la fonction. Toutes ses cases disparaissent
d'un coup :

```c fichier="src/vm/vm.c" lignes="884-895" focus="886"
    CASE(OP_RETURN):{
        Value result = POP();
        close_upvalues(vm,slots);
        vm->frame_count--;
        vm->stack_top = slots;
        if(vm->frame_count == base)
            return result;
        PUSH(result);
        LOAD_FRAME();
        SAFEPOINT();
        NEXT();
    }
```

Avant de lire `close_upvalues`, regarde ce qui arrive sans elle. J'ai copié
`src/` dans un dossier à part, mis la ligne 886 en commentaire, et recompilé.
Le compteur devient :

```terminal
$ ./Ceflo_essai compteur.cf
erreur: l'opérateur `+` ne s'applique pas à func et number
  --> compteur.cf:4:11
    4 |         n += 1
      |           ^
```

La `lfunc` a lu une fonction à la place de `n`. Suis la pile : le code du
fichier est dans la case 0 de la pile, `Compteur` a commencé à la case 1, et
`n` était donc la case 2 de la pile. Pour la ligne `println(a(), ...)`, le
fichier pousse `println` dans la case 1, puis `a` dans la case 2. L'upvalue
restée ouverte pointe toujours sur la case 2, qui contient maintenant la
closure `a` elle-même. Le bug ne se voit même pas au bout de plusieurs appels :
il arrive au premier.

`close_upvalues(vm, slots)` ferme toutes les upvalues qui pointent sur une
case à partir de `slots`, c'est-à-dire toutes celles de la fonction qui
retourne :

```c fichier="src/vm/vm.c" lignes="140-148" fonction="close_upvalues"
// Les variables capturées qui quittent la pile gardent leur valeur dans l'upvalue.
static void close_upvalues(VM *vm, Value *last){
    while(vm->open_upvalues != NULL && vm->open_upvalues->location >= last){
        ObjUpvalue *upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}
```

Voilà pourquoi la liste est triée. Les upvalues à fermer sont celles des cases
les plus hautes, et elles sont toutes en tête de liste. La boucle les ferme une
par une et s'arrête à la première qui pointe plus bas. Elle ne parcourt jamais
les upvalues des fonctions appelantes, même avec cent appels imbriqués qui ont
chacun leurs variables capturées. Une fonction qui ne capture rien paie un
seul test : `vm->open_upvalues` est vide, ou pointe plus bas que `slots`.

Les lignes 144 et 145 sont le passage d'une vie à l'autre : la valeur est
copiée dans `closed`, et `location` pointe désormais dans l'upvalue. Les
closures qui la tiennent ne voient rien changer.

```text
ouverte (Compteur tourne)              fermée (après le RETURN)

pile                                   ObjUpvalue
+-----------+                          +------------------+
| 2  n = 0  | <---+                    | location ---+    |
+-----------+     |                    | closed = 0 <+    |
                  |                    +------------------+
ObjUpvalue        |                           ^
+-------------+   |                           |
| location ---+---+                     closure a : upvalues[0]
| closed none |
+-------------+
       ^
closure a : upvalues[0]
```

La deuxième situation est plus discrète : un bloc qui se termine au milieu
d'une fonction. Ses locales quittent la pile, mais la fonction continue.
C'est `end_scope`, au chapitre 10, qui retirait ces locales une par une avec
`OP_POP`. Il a une variante :

```c fichier="src/vm/compiler.c" lignes="297-313" focus="299"
// Une locale qui quitte la pile : si une closure la garde, elle part dans son upvalue.
static void emit_pop_local(Compiler *c, Local *local, ast_t *node){
    emit_op(c,local->is_captured ? OP_CLOSE_UPVALUE : OP_POP,node);
}

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

Le champ `is_captured`, posé par `resolve_in_function`, sert ici. Une locale
que personne ne capture part avec `OP_POP`, comme avant. Une locale capturée
part avec `OP_CLOSE_UPVALUE`, qui ferme les upvalues de la case du sommet
avant de la retirer :

```c fichier="src/vm/vm.c" lignes="752-755"
    CASE(OP_CLOSE_UPVALUE):
        close_upvalues(vm,vm->stack_top - 1);
        vm->stack_top--;
        NEXT();
```

Le compilateur sait à la compilation quelle locale est capturée, et choisit la
bonne instruction. Le `OP_POP` ordinaire reste une seule soustraction : on n'a
rien ajouté au cas fréquent.

### break et continue

Un `break` quitte un bloc sans passer par sa fin, et donc sans passer par le
code qu'`end_scope` y a écrit. Si une variable du bloc est capturée, son
upvalue resterait ouverte, et la case serait réutilisée. Le compilateur écrit
donc, juste avant le saut, les mêmes instructions de sortie que la fin du
bloc. C'est `emit_leave`, qui appelle `emit_pop_local` pour chaque locale plus
profonde que la boucle, sans les oublier (le bloc continue après le `break`,
pour le code qui suit) :

```c fichier="src/vm/compiler.c" lignes="315-320" fonction="emit_leave"
// break et continue : retire de la pile les locales des blocs quittés, sans les oublier.
static void emit_leave(Compiler *c, int depth, ast_t *node){
    FuncState *fs = c->current;
    for(int i = fs->local_count - 1; i >= 0 && fs->locals[i].depth > depth; i--)
        emit_pop_local(c,&fs->locals[i],node);
}
```

```ceflo fichier="sortie.cf"
var gardees = []
for var mot in ["un", "deux", "stop", "trois"] {
    var copie = mot + "!"
    gardees.append(lfunc() { return copie })
    if mot == "stop" { break }
}
for var f in gardees {
    print(f(), " ")
}
println()
```

```terminal
$ ./Ceflo sortie.cf
un! deux! stop! 
$ ./Ceflo --dis sortie.cf
...
0045     5  GET_LOCAL             4
0047     |  CONSTANT              2  "stop"
0050     |  EQUAL
0051     |  JUMP_IF_FALSE         5  -> 0059
0054     |  CLOSE_UPVALUE
0055     |  POP
0056     |  JUMP                  5  -> 0064
0059     3  CLOSE_UPVALUE
0060     2  POP
0061     |  LOOP                 42  -> 0022
0064     |  POP
...
```

Le chemin du `break` (octets 54 à 56) et la fin normale d'un tour (octets 59
et 60) font la même chose : fermer `copie`, la case 5, puis retirer `mot`, la
case 4. Le `break` saute ensuite à l'octet 64, qui retire les trois cases
cachées du parcours. J'ai essayé, sur une copie de `src/`, un `emit_leave`
qui n'écrit que des `OP_POP` :

```terminal
$ ./Ceflo_sans_leave sortie.cf
un! deux! <func print>
```

Les deux premières closures ont été fermées par la fin normale de leur tour.
La troisième, sortie par le `break`, a gardé une upvalue ouverte sur la case
5. Dans la deuxième boucle, `print(f(), " ")` pousse `print` dans cette case
avant d'appeler `f`, et la closure le lit.

Une question reste : qui garde l'upvalue en vie ? Le GC du chapitre 11 la
trouve de deux façons. Une upvalue ouverte est dans la liste de la VM, que
`gc_mark_roots` parcourt :

```c fichier="src/vm/memory.c" lignes="294-302" focus="299-300"
static void gc_mark_roots(VM *vm){
    for(Value *slot = vm->stack; slot < vm->stack_top; slot++)
        gc_mark_value(vm,*slot);
    for(int i = 0; i < vm->frame_count; i++)
        gc_mark_object(vm,(Obj*)vm->frames[i].closure);
    for(ObjUpvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
        gc_mark_object(vm,(Obj*)upvalue);
    for(int i = 0; i < vm->root_count; i++)
        gc_mark_value(vm,*vm->roots[i]);
```

Une upvalue fermée n'est plus dans cette liste, mais elle est tenue par les
closures qui l'utilisent : `OBJ_CLOSURE` marque chacune de ses upvalues, et
`OBJ_UPVALUE` marque sa valeur `closed` (lignes 224 à 233 de `memory.c`).
Quand la dernière closure meurt, l'upvalue meurt avec elle. Une upvalue
ouverte, elle, n'a pas besoin de marquer sa valeur : la case est sur la pile,
et la pile est une racine.

On peut maintenant suivre `compteur.cf` jusqu'au bout. `Compteur` pose `0`
dans sa case 1. `OP_CLOSURE` crée la closure et une upvalue ouverte sur la
case 1. `OP_RETURN` ferme l'upvalue : `closed` vaut 0, `location` pointe sur
`closed`. `a()` exécute `GET_UPVALUE 0`, `ADD`, `SET_UPVALUE 0` : il lit et
écrit dans `closed`. Le deuxième appel de `Compteur` crée une autre upvalue,
qui sera fermée à son tour, et `b` repart de 0.

```terminal
$ ./Ceflo compteur.cf
1 2 3
1
```

> [!essaie]
> Refais l'expérience de l'autre côté : garde `close_upvalues` dans
> `OP_RETURN`, mais remplace `OP_CLOSE_UPVALUE` par `OP_POP` dans
> `emit_pop_local`. Quel programme de ce chapitre casse alors, et pourquoi
> pas `compteur.cf` ? Indice : cherche une variable capturée qui quitte la
> pile avant la fin de sa fonction. Travaille sur une copie de `src/`, comme
> ci-dessus.

## 12.6 Le piège des boucles

Il reste un cas que tout le monde rencontre un jour, en JavaScript, en Python
ou en Go. On crée des fonctions dans une boucle, et chacune doit se souvenir
du tour où elle est née :

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
$ ./Ceflo boucle_for.cf
3 3 3 
0 1 2 
```

Les deux boucles se ressemblent, et ne donnent pas la même chose. Ce n'est pas
un accident de la VM : l'ancien interpréteur affiche exactement les mêmes deux
lignes, et c'est ce que dit `GRAMMAIRE.md` : dans un `for ... in`, « chaque
tour a les siennes ». Le bytecode montre où est la différence :

```terminal
$ ./Ceflo --dis boucle_for.cf
== boucle_for.cf ==
0000     1  LIST                  0
0003     |  DEFINE_GLOBAL         0
0006     2  CONSTANT              0  0
0009     |  GET_LOCAL             1
0011     |  CONSTANT              1  3
0014     |  LESS
0015     |  JUMP_IF_FALSE        20  -> 0038
0018     3  GET_GLOBAL            0
0021     |  CLOSURE               2  <func lfunc>
0024      |                       local 1
0026     |  INVOKE                3  "append"  (1 args)
0030     |  POP
0031     2  ADD_LOCAL             1    4  1
0035     |  LOOP                 29  -> 0009
0038     |  CLOSE_UPVALUE
...
```

Dans la boucle `for` classique, `i` est déclarée par l'initialisation, une
seule fois, dans une portée qui couvre toute la boucle (`compile_for`
commence par `begin_scope` et finit par `end_scope`). Les trois closures
capturent la case 1. À chaque tour, `capture_upvalue` trouve l'upvalue déjà
ouverte sur cette case et la retourne : les trois closures partagent une
seule boîte. Le seul `CLOSE_UPVALUE` est à l'octet 38, après la boucle, quand
`i` vaut déjà 3.

Dans le `for ... in`, la même boucle vue de près :

```terminal
$ ./Ceflo --dis boucle_for.cf
...
0079    11  CONSTANT              0  0
0082     |  CONSTANT              4  1
0085     |  CONSTANT              8  2
0088     |  LIST                  3
0091     |  ITER_PREP
0092     |  ITER_NEXT             1  -> 0113
0096    12  GET_GLOBAL            1
0099     |  CLOSURE               9  <func lfunc>
0102      |                       local 4
0104     |  INVOKE                3  "append"  (1 args)
0108     |  POP
0109    11  CLOSE_UPVALUE
0110     |  LOOP                 21  -> 0092
...
```

Les cases 1, 2 et 3 sont les trois cases cachées du parcours (la liste, les
clés relevées, l'index : chapitre 10). `j` est la case 4, posée par
`ITER_NEXT` à chaque tour. Et le `CLOSE_UPVALUE` est **dans** la boucle, à
l'octet 109, juste avant `LOOP`. À la fin de chaque tour, l'upvalue de `j` est
fermée avec la valeur de ce tour. Au tour suivant, `ITER_NEXT` pose une
nouvelle valeur dans la case 4, `capture_upvalue` ne trouve plus d'upvalue
ouverte sur elle, et en crée une neuve. Trois tours, trois boîtes.

Ce `CLOSE_UPVALUE` vient d'un `begin_scope` placé au bon endroit dans
`compile_for_in` :

```c fichier="src/vm/compiler.c" lignes="1463-1494" fonction="compile_for_in" focus="1468-1469,1488"
    int start = current_chunk(c)->count;
    emit_op_u8(c,OP_ITER_NEXT,second != NULL ? 2 : 1,node);
    emit_u16(c,0xFFFF,node);
    int exit = current_chunk(c)->count - 2;

    begin_scope(c);
    declare_for_in_variable(c,first);
    Local *x = &c->current->locals[c->current->local_count - 1];
    if(second != NULL){
        declare_for_in_variable(c,second);
        Local *y = x + 1;
        if(x->type.kind != TYPE_ANY){
            emit_op_u8(c,OP_GET_LOCAL,c->current->local_count - 2,first);
            emit_check(c,CHECK_VARIABLE,x->type,x->name,first);
            emit_op(c,OP_POP,first);
        }
        emit_check(c,CHECK_VARIABLE,y->type,y->name,second);
    }else{
        emit_check(c,CHECK_VARIABLE,x->type,x->name,first);
    }

    Loop loop;
    loop_begin(c,&loop,depth,depth + 1,-1);
    compile_statement(c,node->for_body);
    jump_list_patch(c,&loop.continues,node);
    end_scope(c,node);
    emit_loop(c,start,node);

    patch_jump(c,exit,node);
    loop_end(c,&loop,node);
    end_scope(c,node);
}
```

Il y a deux portées. L'extérieure, ouverte plus haut dans la fonction, garde
les trois cases cachées pour toute la boucle. L'intérieure (ligne 1468) ne
contient que les variables du tour, et elle se ferme à la ligne 1488, **avant**
`emit_loop`. Le code de fin de portée est donc exécuté à chaque tour. Une
variable du tour qui n'est pas capturée part avec un `POP` : c'est le cas du
`f` des boucles qui appellent les fonctions. Une variable capturée part avec
un `CLOSE_UPVALUE`. Le `continue` du corps saute juste avant ce
`end_scope` (ligne 1487), pour que la fermeture ait lieu même quand on saute
la fin du corps.

Le `for` classique de CeFlo garde le comportement du C et de l'ancien
interpréteur : une seule variable pour toute la boucle. Pour qu'une closure
garde le tour où elle est née, il suffit de déclarer une variable dans le
corps. Le corps est un bloc qui déclare un nom, il a donc sa portée
(`has_definition`, chapitre 6), fermée à chaque tour :

```ceflo fichier="copie.cf"
var fonctions = []
for var i = 0; i < 3; i += 1 {
    var k = i
    fonctions.append(lfunc() { return k })
}
for var f in fonctions {
    print(f(), " ")
}
println()
```

```terminal
$ ./Ceflo copie.cf
0 1 2 
$ ./Ceflo --dis copie.cf
== copie.cf ==
...
0018     3  GET_LOCAL             1
0020     4  GET_GLOBAL            0
0023     |  CLOSURE               2  <func lfunc>
0026      |                       local 2
0028     |  INVOKE                3  "append"  (1 args)
0032     |  POP
0033     3  CLOSE_UPVALUE
0034     2  ADD_LOCAL             1    4  1
0038     |  LOOP                 32  -> 0009
...
```

`var k = i` ne coûte qu'un `GET_LOCAL 1` : la copie de `i` arrive au sommet
de la pile, qui est la case 2, et c'est `k`. Le `CLOSE_UPVALUE` de l'octet 33
est revenu dans la boucle.

> [!note]
> JavaScript a eu exactement ce problème avec `var`, et l'a réglé en 2015 avec
> `let`, qui donne une variable neuve à chaque tour même dans un `for`
> classique. Go a fait le même changement en 2024, dans sa version 1.22. Pour
> le faire dans CeFlo, il faudrait recopier `i` dans une case neuve au début
> de chaque tour, et la recopier en sens inverse avant les actions du `for`.

Tu as maintenant toutes les fonctions de CeFlo dans la VM : des fonctions qui
se passent comme des valeurs, qui gardent leurs variables, qui se partagent
une variable et qui s'appellent elles-mêmes, sans qu'aucune variable ordinaire
ne quitte la pile. Le bench `closures` (un compteur appelé un million de fois,
cent `lfunc` créées dans un `for ... in`, un `filter`) le mesurera au
chapitre 14.

Il manque une chose que les programmes du `README` utilisent partout :
`struct Personne`, `new`, et des méthodes qui écrivent `Nom = nom` sans
`self.`. Dans le visitor, un objet était une portée comme les autres. Dans la
VM, on n'a plus de portées. Le chapitre 13 range les attributs dans des cases
fixes, comme les locales.

## Défis

1. **Une variable neuve à chaque tour.** Fais que le `for` classique donne une
   variable neuve à chaque tour, comme `let` en JavaScript. Dans
   `compile_for`, ouvre une portée intérieure autour du corps, copie `i` dans
   une case de cette portée, compile le corps contre cette case, puis recopie
   sa valeur dans `i` avant `for_solve`. Écris d'abord à la main le bytecode
   que tu veux pour `boucle_for.cf`, puis compare avec `--dis`. Lance ensuite
   `make test` : `tests/vm/closures.cf` vérifie exprès l'ancien comportement
   (« une seule variable pour toute la boucle »). Changer le langage, c'est
   aussi changer ce test.

2. **Le prix du tri.** Remplace, dans une copie de `src/`, la liste triée de
   `capture_upvalue` par un simple ajout en tête, et fais parcourir toute la
   liste à `close_upvalues` (il faut alors comparer chaque `location` à `last`
   sans s'arrêter). Mesure `bench/closures.cf` et `bench/fib.cf` avec
   `bench/run.sh ./Ceflo ./Ceflo_ref closures`. Le tri vaut-il ses dix lignes ?

3. **Des captures à la Java.** Ajoute une option de compilation qui refuse de
   modifier une variable capturée : `n += 1` dans la `lfunc` du compteur
   donnerait « `n` est capturée : elle ne se modifie pas ». Le bon endroit est
   `compile_assignement`, quand le nom résolu est un `NAME_UPVALUE`. Que
   deviendrait `compteur.cf` ? Écris-le autrement, avec une liste d'un élément,
   et explique pourquoi ça marche encore.

4. **La locale déclarée plus bas.** Corrige la limite de `docs/VM.md` :
   `var g = lfunc(){ return y }` suivi de `var y = 3`, dans une fonction. Une
   piste : avant de compiler un bloc, relever les noms qu'il déclare (comme
   `scan_file_names` le fait pour le fichier), et réserver leurs cases. Le
   piège est que ces cases doivent exister sur la pile au moment où la `lfunc`
   les capture : il faudra pousser un `none` pour chacune au début du bloc.

## Note de conception

Toutes les solutions de ce chapitre partent d'une question : qui paie pour
les closures ? Python et V8 font payer la fonction qui déclare la variable :
dès qu'une variable est capturée quelque part, chaque accès passe par une
boîte sur le tas, même si la closure n'est jamais créée. Java fait payer le
programmeur, qui n'a pas le droit de modifier ce qu'il capture. C++ lui fait
payer ses erreurs. Lua, et CeFlo à sa suite, font payer la closure
elle-même, et seulement au moment où elle en a besoin : la fonction qui
déclare `n` la lit avec un `GET_LOCAL`, la closure la lit avec deux
indirections, et la boîte n'est remplie qu'au départ de la case.

Le prix de cette solution n'est pas en vitesse, il est en complexité : une
liste triée de plus dans la VM, deux états pour une upvalue, une instruction
`OP_CLOSE_UPVALUE` que le compilateur doit placer au bon endroit, et le bug de
la section 12.4 qui guette quiconque oublie de partager. C'est la partie de la
VM où j'ai le plus souvent écrit un test qui cassait. Je pense pourtant que
c'est le bon choix pour un langage comme CeFlo, où les `lfunc` sont partout
(`filter`, les callbacks, les compteurs) mais où la plupart des variables ne
sont jamais capturées. On ne paie que pour ce qu'on utilise, et le cas
fréquent reste une lecture de case.

Il y a une leçon plus large dans le piège des boucles. Le `for` classique de
CeFlo donne `3 3 3`, et c'est cohérent avec le C et avec l'ancien
interpréteur. Mais c'est presque toujours une surprise pour celui qui l'écrit.
JavaScript et Go ont fini par changer leur langage pour cette seule raison,
au prix d'une incompatibilité. Quand le comportement le plus simple à
implémenter et le comportement que les gens attendent divergent, ce sont les
gens qui ont raison, tôt ou tard. Si tu écris ton propre langage, donne une
variable neuve à chaque tour dès le premier jour : ça coûte un
`OP_CLOSE_UPVALUE` par tour, et seulement quand une closure la capture.
