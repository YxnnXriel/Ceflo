# La grammaire de CeFlo

C'est la référence du langage. Chaque règle correspond à une fonction `parser_parse_*` de [src/parser.c](src/parser.c) : pour réécrire CeFlo dans un autre langage, il suffit de suivre ce fichier de haut en bas.

## Notation

| Écriture | Sens |
| --- | --- |
| `"mot"` | ce texte exactement |
| `A B` | A puis B |
| `A \| B` | A ou B |
| `[ A ]` | A est facultatif |
| `{ A }` | A répété, zéro fois ou plus |
| `( A )` | groupe |

## Les règles

```ebnf
programme      = instructions ;

(* --------------------- instructions --------------------- *)

instructions   = { instruction | ";" } ;
bloc           = "{" instructions "}" ;

instruction    = definition_var
               | definition_func
               | definition_struct
               | definition_enum
               | si
               | tant_que
               | pour
               | retour
               | "break"
               | "continue"
               | import
               | export
               | affectation ;

affectation    = expression [ op_affectation expression ] ;
op_affectation = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

(* --------------------- définitions ---------------------- *)

definition_var = ( "var" | "const" ) une_var { "," une_var } ;
une_var        = ID [ "::" ID ] [ "=" expression ] ;

definition_func = "func" ID parametres [ type_retour ] bloc ;
lfunc           = "lfunc" parametres [ type_retour ] bloc ;

parametres     = "(" [ liste_param ] ")"
               | "->" liste_param
               | (* rien *) ;
liste_param    = param { "," param } ;
param          = ID [ "::" ID ] ;
type_retour    = ":" ( ID | "none" ) ;

definition_struct = "struct" ID "{" { definition_var | definition_func | ";" } "}" ;

definition_enum = "enum" ID "{" element { "," element } [ "," ] "}" ;
element         = ID [ "=" ( CHAINE | [ "-" ] NOMBRE ) ] ;

(* ------------------------ contrôle ---------------------- *)

si             = "if" expression bloc [ "else" ( si | bloc ) ] ;
tant_que       = "while" expression bloc ;
pour           = "for" [ definition_var | affectation ] ";"
                       [ expression ] ";"
                       [ affectation { "," affectation } ] bloc
               | "for" "var" une_var_in [ "," une_var_in ] "in" expression bloc ;
une_var_in     = ID [ "::" ID ] ;
retour         = "return" [ expression ] ;

(* ------------------------- modules ---------------------- *)

import         = "from" [ "@" ] chemin "import" ( "*" | ID | liste_noms ) ;
chemin         = CHAINE | ID { "." ID } ;
export         = "export" "::" liste_noms ;
liste_noms     = "[" [ ID { "," ID } ] "]" ;

(* ----------------------- expressions -------------------- *)
(*  de la priorité la plus faible à la plus forte           *)

expression     = ou [ "?" expression [ ":" expression ] ] ;
ou             = et { "or" et } ;
et             = egalite { "and" egalite } ;
egalite        = comparaison { ( "==" | "!=" ) comparaison } ;
comparaison    = somme { ( "<" | "<=" | ">" | ">=" ) somme } ;
somme          = terme { ( "+" | "-" ) terme } ;
terme          = facteur { ( "*" | "/" | "%" ) facteur } ;
facteur        = ( "-" | "!" ) facteur | suffixe ;
suffixe        = primaire { "." ID | "[" expression "]" | arguments } ;
arguments      = "(" [ expression { "," expression } ] ")" ;

primaire       = NOMBRE | CHAINE | CARACTERE
               | "true" | "false" | "none"
               | ID
               | "[" [ expression { "," expression } ] "]"
               | map
               | "new" ID [ arguments ]
               | "@" ID arguments
               | lfunc
               | "(" expression ")" ;

map            = "{" [ entree { "," entree } [ "," ] ] "}" ;
entree         = ( ID | CHAINE | NOMBRE | "[" expression "]" ) ":" expression
               | ID ;
```

Dans [src/parser.c](src/parser.c), `somme` s'appelle `parser_parse_expression`, et `expression` s'appelle `parser_parse_expr`.

## Les jetons

```ebnf
ID         = ( lettre | "_" ) { lettre | chiffre | "_" } ;
NOMBRE     = chiffre { chiffre } [ "." chiffre { chiffre } ] ;
CHAINE     = '"' { caractère | échappement } '"' ;
CARACTERE  = "'" ( caractère | échappement ) "'" ;
échappement = "\n" | "\t" | "\r" | "\a" | "\b" | "\0" | "\\" | '\"' | "\'" ;
```

- **Mots réservés :** `for in while if else var const func lfunc return import from export struct enum true false none or and new continue break`
- **Commentaires :** `// jusqu'à la fin de la ligne` et `/* sur plusieurs lignes */`
- **Script :** une première ligne `#!...` est ignorée.

## Les règles que la grammaire ne montre pas

**Les `;` sont facultatifs.** Une instruction s'arrête dès que le jeton suivant ne peut plus la continuer. Pour éviter les pièges :

- `(` et `[` doivent être sur la même ligne que ce qui les précède. Sinon ils commencent une nouvelle instruction.
- `return` seul sur sa ligne ne retourne rien : la valeur doit commencer sur la même ligne.

**Vérifications du parser :**

- `return` s'utilise seulement dans une fonction.
- `break` et `continue` s'utilisent seulement dans une boucle de la même fonction.
- `export` s'utilise seulement hors d'un bloc.
- `const` doit recevoir une valeur.
- `self` ne se déclare qu'une fois. Ce n'est pas un vrai paramètre : l'appelant ne le passe pas.
- Dans une structure, seul `__init__` peut avoir plusieurs versions, avec des nombres de paramètres différents.

## Le sens du langage

### Valeurs

| Type | Exemple | Faux quand | Partage |
| --- | --- | --- | --- |
| `none` | `none` | toujours | copie |
| `bool` | `true` | `false` | copie |
| `char` | `'a'` | `'\0'` | copie |
| `number` | `3.14` | `0` | copie |
| `str` | `"abc"` | `""` | copie |
| `list` | `[1, 2]` | `[]` | **référence** |
| `map` | `{ a: 1 }` | `{}` | **référence** |
| objet | `new Point()` | jamais | **référence** |
| élément d'énumération | `Couleur.ROUGE` | jamais | référence |
| fonction | `lfunc(x){ }` | jamais | référence |

Une chaîne ne change jamais vraiment : `s[0] = 'X'` range une nouvelle chaîne dans `s`. Une chaîne et ses index comptent en octets : `"é".lenght()` vaut 2.

### Opérateurs

- `+` assemble du texte dès qu'un des côtés est une chaîne, et met deux listes bout à bout.
- `"ab" * 3` répète la chaîne.
- Entre `bool`, `char` et `number`, le calcul donne un `number` : `'A' + 1` vaut 66, et `@char('A' + 1)` vaut `'B'`.
- `/` et `%` par zéro sont des erreurs.
- `and` et `or` retournent un `bool`, et n'évaluent le côté droit que s'il est utile.
- `==` compare les listes élément par élément, et les maps et les objets par identité. Deux valeurs de types différents ne sont pas égales, sauf `bool`, `char` et `number` entre eux, et `char` avec `str`.

### Maps

Une table clé → valeur, écrite comme en TypeScript. Sa recherche prend un temps constant, quelle que soit sa taille, et elle garde l'ordre d'insertion.

```go
var cle = "ville"
var nom = "Yann"
var t = {
    age: 22,             // un nom : la clé est la chaîne "age"
    "avec espace": 1,    // une chaîne
    3: "trois",          // un nombre
    [cle]: "Abidjan",    // une clé calculée
    nom,                 // un raccourci pour nom: nom
}

t.age                    // comme t["age"]
t.absent                 // none : une clé absente n'est pas une erreur
t.age += 1
t["nouvelle"] = true
```

- **Clés :** une clé est une chaîne, un nombre ou un élément d'énumération. `1` et `"1"` sont deux clés différentes, et `Couleur.ROUGE` n'est ni l'une ni l'autre. Un `char` compte comme une chaîne d'un seul octet, ce qui donne `t[mot[i]]`. Un autre type est une erreur.
- **Mots réservés :** une clé comme `if` ou `new` s'écrit entre guillemets, et se lit avec `t["if"]`.
- **Fonctions :** dans `t.f()`, si la clé `f` contient une fonction, elle est appelée. Sinon `f` est une méthode de map. Une clé `keys` qui contient un nombre ne cache donc pas `t.keys()`.

### Parcourir avec `for ... in`

| Parcouru | `for var a in c` | `for var a, b in c` |
| --- | --- | --- |
| `map` | `a` : la clé | `a` : la clé, `b` : la valeur |
| `list` | `a` : l'élément | `a` : l'index, `b` : l'élément |
| `str` | `a` : le caractère | `a` : l'index, `b` : le caractère |

- Les variables n'existent que dans la boucle, et chaque tour a les siennes : une `lfunc` créée dans la boucle garde la valeur de son tour.
- Elles se changent sans toucher au map ou à la liste. Pour modifier le contenu, on écrit `c[a] = ...`.
- **Modifier un map dans sa boucle :** ses clés sont relevées avant le premier tour. Une clé enlevée est sautée, une clé ajoutée n'est pas parcourue, et une valeur changée se lit à jour.
- **Modifier une liste dans sa boucle :** sa taille est relue à chaque tour. Un élément ajouté est donc parcouru.
- Une chaîne ne change jamais : c'est celle du départ qui est parcourue.

### Portées

- Chaque fonction a sa portée. Chaque bloc `{ }` en a une dès qu'il déclare un nom.
- Une fonction voit l'endroit où elle est **écrite**, pas celui d'où elle est appelée. Une `lfunc` se souvient donc de ses variables, même après la fin de la fonction qui l'a créée.
- Les variables, les fonctions et les structures partagent les mêmes noms. `var x` peut redéfinir une variable de la même portée, mais une fonction ou une structure ne se redéfinit pas.

### Types déclarés

`var x::type` et `func f(x::type) : type` fixent le type : **il ne change plus et rien n'est converti**. Une valeur d'un autre type est une erreur, à chaque affectation, passage ou retour. Cette vérification se fait à l'exécution, quand la ligne est atteinte.

```go
var a::int = 1
a = 2               // accepté
a = "2"             // erreur : `a` est de type int, pas str
a = 2.5             // erreur : `a` est de type int, pas float
var a = "x"         // erreur : une deuxième déclaration garde le type
var a::str = "x"    // erreur : `a` est déjà de type int
```

| Type | Accepte | Sans valeur |
| --- | --- | --- |
| `int` | un nombre entier | `0` |
| `float`, `number` | tous les nombres | `0` |
| `bool` | `true`, `false` | `false` |
| `char` | un caractère | `'\0'` |
| `str`, `string` | une chaîne | `""` |
| `list` | une liste | `[]` |
| `map` | un map | `{}` |
| une structure | un objet de cette structure, ou `none` | `none` |
| une énumération | un de ses éléments, ou `none` | `none` |

Une variable déclarée sans type reste libre : `var x = 1` puis `x = "a"` est accepté.

### Conversions

Seul `@type(valeur)` convertit :

| Vers | Conversion |
| --- | --- |
| `int` | nombre tronqué |
| `float`, `number` | nombre |
| `bool` | selon la vérité de la valeur |
| `char` | code ou premier caractère |
| `str`, `string` | le texte de la valeur |
| `map` | seulement un map, tel quel |
| une structure | avec un seul argument, l'objet ou `none` tel quel, sinon `__cast__` |
| une énumération | l'élément qui a cette valeur, ou `none` si aucun ne l'a |

Vers un nombre, une chaîne donne le nombre qu'elle contient, ou sinon **sa longueur** : `@int("42")` vaut 42 et `@int("abc")` vaut 3.

`@list(a, b)` crée une liste. `@Point(a, b)` équivaut à `new Point(a, b)`, et `@Point(a)` aussi quand la structure n'a pas de `__cast__`.

### Structures

```go
struct Point {
    var x::int = 0
    const DIMENSION = 2

    func __init__(self, depart) { self.x = depart }     // new Point(1)
    func __init__() { }                                 // new Point()
    func __str__() { return "x=" + x }                  // println(point)
    func __cast__(valeur) { return new Point(valeur) }  // @Point(3)
    func avance(n) { x += n; return self }
}
```

Un objet est une portée qui contient ses attributs et ses méthodes. Dans une méthode, `x` et `self.x` désignent donc le même attribut, et `self` est toujours disponible même sans être déclaré. `__cast__` reçoit la valeur à convertir et s'appelle sans objet : il ne peut pas utiliser `self`.

### Énumérations

Une énumération est un vrai type : ses éléments ne se confondent pas avec des nombres ou du texte.

```go
enum Couleur { ROUGE, VERT, BLEU }
enum Http { OK = 200, INTROUVABLE = 404 }
enum Niveau { BAS = "bas", HAUT = "haut" }

var c::Couleur = Couleur.ROUGE
c = Couleur.VERT            // accepté
c = 1                       // erreur : `c` est de type Couleur, pas int

Http.OK.name                // "OK"
Http.OK.value               // 200
Http.values()               // [ Http.OK , Http.INTROUVABLE ]
var noms = { [Couleur.ROUGE]: "rouge" }
```

- **Accès :** un élément s'écrit toujours `Couleur.ROUGE`, jamais `ROUGE` seul. Un élément inconnu est une erreur.
- **Valeurs :** une valeur est un nombre ou un texte écrit tel quel, sans calcul. Sans `=`, c'est la valeur précédente + 1, et 0 pour la première. Après un texte, le `=` est obligatoire.
- **Doublons :** deux noms ou deux valeurs identiques dans une énumération sont une erreur.
- **Immuable :** `Couleur.ROUGE = 1`, `Couleur = 1` et `new Couleur()` sont des erreurs.
- **Comparaison :** chaque élément n'existe qu'une fois. `==` compare son identité : `Couleur.ROUGE == 0` vaut `false`. `<` et les calculs sont des erreurs : on écrit `a.value < b.value`.
- **Vers un élément :** `@Http(200)` donne `Http.OK`, et `@Http(999)` donne `none`. Une valeur qui n'est ni un nombre ni un texte est une erreur, et `@Http(Http.OK)` ou `@Http(none)` la rendent telle quelle.
- **Depuis un élément :** `@int(Http.OK)` donne 200, et un élément dont la valeur est un texte ne se convertit pas en nombre. `@str(Http.OK)` donne `"OK"`.
- **Texte :** `println(Http.OK)` et `"" + Http.OK` écrivent `Http.OK`.
- **Portée :** une énumération suit les règles d'une structure pour les portées et les modules. Elle ne se redéfinit pas dans la même portée.
- **Parcours :** `for var c in Couleur.values() { }`.

### Fonctions fournies

| Fonction | Rôle |
| --- | --- |
| `println(a, b...)`, `print(a, b...)` | affiche les valeurs, avec ou sans retour à la ligne |
| `input(texte...)` | affiche le texte et lit une ligne ; `none` à la fin de l'entrée |
| `nextInt(texte...)` | lit une ligne qui contient un nombre |
| `format("${a + 1}")` | calcule chaque `${...}` ; `\${` écrit un vrai `${` |
| `printf("${a}")` | affiche `format(...)` |
| `exit(code)` | arrête le programme |
| `argv` | la liste des arguments ; `argv[0]` est le nom du script |

Ces noms ne sont pas réservés : un script peut les redéfinir.

| Méthode | Rôle |
| --- | --- |
| `liste.append(a, b...)` | ajoute à la fin |
| `liste.remove(i)` | enlève et retourne l'élément `i` |
| `liste.get(i)` | l'élément `i`, ou `none` s'il n'existe pas |
| `liste.lenght()` | le nombre d'éléments |
| `liste.filter(f)` | une nouvelle liste, avec les éléments où `f` est vraie |
| `liste.print()` | affiche la liste |
| `t.get(cle)`, `t.get(cle, defaut)` | la valeur, ou `none` ou `defaut` si la clé n'existe pas |
| `t.set(cle, valeur)` | ajoute ou remplace, et retourne `t` |
| `t.has(cle)` | `true` si la clé existe, même si sa valeur est `none` |
| `t.remove(cle)` | enlève la clé et retourne sa valeur, ou `none` |
| `t.keys()`, `t.values()` | une nouvelle liste, dans l'ordre d'insertion |
| `t.lenght()` | le nombre de clés |
| `texte.lenght()` | le nombre d'octets |
| `texte.format(a, b...)` | remplace chaque `{}` |
| `texte.split(sep)` | découpe en liste |

`length()` est accepté partout où `lenght()` l'est.

### La fonction `main`

```go
func main(argv) {
    println("Bonjour ", argv[1])
    return 0
}
```

- `main` est facultatif. Le fichier s'exécute d'abord de haut en bas, puis son `main` est appelé.
- `main` s'écrit sans paramètre, ou avec un seul qui reçoit `argv`, comme en C : `argv[0]` est le nom du script.
- Sa valeur devient le code de sortie : un `int`, ou rien pour `0`. Une autre valeur est une erreur.
- Seul le `main` du fichier lancé est appelé : celui d'un module importé ne se lance jamais tout seul, et `import *` ne le reprend pas.
- `main` n'est pas un mot réservé. Une variable peut s'appeler ainsi.

### Modules

- `from outils import *` lit `outils.cf`, et `from a.b import c` lit `a/b.cf`.
- Le chemin se cherche à côté du fichier qui importe, puis depuis le dossier courant.
- Un fichier ne s'exécute qu'une seule fois, même importé plusieurs fois.
- Sans `export`, tous les noms du fichier sont publics. Avec `export :: [a, b]`, seuls ces noms le sont.
- `from @"lib/math/libmath.so" import [mathSqrt]` charge des fonctions écrites en C.
- Un module fourni par CeFlo, comme `os`, s'importe de la même façon. Un fichier `os.cf` trouvé par le chemin passe avant lui.

### Le module `os`

```go
from os import [lire_lignes, dossier, commande]

for var nom in dossier(".") {
    println(nom)
}
println(commande("date").sortie)
```

CeFlo n'a pas d'exceptions : ce qui échoue, comme un fichier absent, retourne `none` ou `false`, et le script décide quoi faire. Seule une mauvaise utilisation, comme `lire(3)`, arrête le programme.

| Fonction | Rôle | Si ça échoue |
| --- | --- | --- |
| `lire(chemin)` | le contenu du fichier | `none` |
| `lire_lignes(chemin)` | la liste des lignes, sans leur fin de ligne | `none` |
| `ecrire(chemin, texte)` | remplace le contenu du fichier | `false` |
| `ajouter(chemin, texte)` | ajoute à la fin, et crée le fichier s'il manque | `false` |
| `existe(chemin)` | le fichier ou le dossier existe-t-il ? | `false` |
| `est_dossier(chemin)` | est-ce un dossier ? | `false` |
| `taille(chemin)` | le nombre d'octets | `none` |
| `dossier(chemin)` | les noms triés, sans `.` ni `..` | `none` |
| `creer_dossier(chemin)` | crée le dossier et ses parents ; un dossier existant convient | `false` |
| `supprimer(chemin)` | enlève un fichier ou un dossier vide, jamais un dossier plein | `false` |
| `renommer(ancien, nouveau)` | renomme ou déplace | `false` |
| `copier(source, cible)` | copie un fichier | `false` |
| `chemin(a, b...)` | assemble un chemin, sans doubler les `/` | |
| `dossier_courant()` | le dossier de travail | `none` |
| `changer_dossier(chemin)` | change le dossier de travail | `false` |
| `env(nom)` | la valeur d'une variable d'environnement | `none` |
| `commande(texte)` | lance dans le shell, et retourne `{ code, sortie }` | `none` |
| `lancer(texte)` | lance dans le shell, la commande affiche son texte ; retourne son code de sortie | `-1` |

`commande` et `lancer` passent leur texte au shell. Un texte qui vient de l'extérieur, comme un argument du script, peut donc lancer autre chose que prévu : il ne faut pas l'y coller sans précaution.
