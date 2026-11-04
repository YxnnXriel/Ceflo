# CeFlo pour Visual Studio Code

La coloration, l'autocomplétion et les erreurs du code CeFlo (`.cf` et `.ceflo`). Depuis la racine du projet :

```bash
make editor
```

## Ce qui est proposé

| Vous écrivez | Proposé |
| --- | --- |
| `p.` | les attributs et les méthodes de l'objet, même en chaîne : `p.Job.` |
| `t.` et `t["` | les clés et les méthodes du map |
| `"abc".`, `[1].` | les méthodes de `str` et de `list` |
| `from ` | les fichiers `.cf` et les dossiers ; après `from @"`, les bibliothèques `.so` |
| `from outils import ` ou `import [` | les noms publics du module, ou les fonctions de la bibliothèque native |
| `from os import ` | les fonctions du module `os`, avec leur description |
| `Couleur.` et `Couleur.ROUGE.` | les éléments de l'énumération et `values`, puis `name` et `value` |
| `new ` | les structures |
| `::`, `@`, `func f() : ` | les types, les structures et les énumérations |
| ailleurs | les variables visibles, les fonctions, les structures, les énumérations, `println`... et les mots du langage |

Le type d'une valeur se déduit du code : `new Point()`, `var p::Point`, `self.Job = new Metier()`, le type de retour d'une fonction, un `for ... in`. Quand il reste inconnu, comme pour `p` dans `func f(p) { p. }`, tout ce qui pourrait convenir est proposé.

## Les erreurs

L'éditeur souligne en rouge ce qui échouera sûrement à l'exécution, avec le même message que l'interpréteur :

```go
struct Point {
    var x::int = 0
}

var p = new Point()
p.x = "Ynn Ariel"       // `x` est de type int, pas str
p.z = 1                 // `Point` n'a pas d'attribut `z`
p.rien()                // `Point` n'a pas de méthode `rien`
```

Il vérifie les types déclarés (variables, attributs, paramètres, retours), le nombre d'arguments d'un appel, les attributs et les méthodes, les constantes, et les éléments des énumérations.

Le code juste ne doit jamais être souligné. Tout ce qui peut changer à l'exécution est donc ignoré : une variable sans type réaffectée, un élément de liste, un attribut sans type, le retour d'une fonction sans `: type`.

## Le code

- [analyse.js](analyse.js) comprend le code. Il ne dépend pas de l'éditeur, et supporte du code à moitié écrit.
- [extension.js](extension.js) le branche sur Visual Studio Code.
