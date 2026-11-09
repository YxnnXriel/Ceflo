# Le format des chapitres

Chaque chapitre est un fichier Markdown dans `livre/chapitres/`, nommé avec son
numéro sur deux chiffres : `02-lexer.md`. `livre/construire.py` le transforme en
page du site.

## L'en-tête

```
---
numero: 2
titre: le lexer
note: on découpe le texte
partie: Lire le texte
---
```

`titre` est en minuscules (il s'affiche dans le cadre de sélection), `note` est
la petite phrase manuscrite à côté de l'étiquette du chapitre.

## Le texte

- `## 2.3 Lire un nombre` : un sous-chapitre, toujours numéroté.
- `### ...` : un intertitre dans un sous-chapitre, sans numéro, à utiliser peu.
- `## Défis` et `## Note de conception` : les deux dernières sections de chaque
  chapitre, sans numéro.
- Un dessin en texte (pile, arbre, portées) : un bloc ` ```text `.
- Paragraphes séparés par une ligne vide. `*italique*`, `**gras**`,
  `` `code` ``, `[lien](url)`.
- Listes avec `-` ou `1.`.

## Le code

Un bloc de code C du dépôt :

````
```c fichier="src/lexer.c" lignes="184-203" fonction="lexer_collect_digit" focus="191-196"
static token_t lexer_collect_digit(lexer_t *lexer){
...
}
```
````

- `fichier` : le chemin dans le dépôt, affiché dans l'onglet et le fil d'Ariane.
- `lignes` : les vrais numéros de ligne (le premier sert à numéroter).
- `fonction` : facultatif, ajouté au fil d'Ariane.
- `focus` : facultatif, les lignes surlignées dont parle le texte.
- Un code écrit pour le livre (un programme d'essai) n'a pas de `lignes` :
  `fichier="essai_lexer.c"`.

Les autres langages : `ceflo` pour un programme CeFlo, `sh` pour une commande,
`terminal` pour une session complète (commande et sortie, telles qu'affichées) :

````
```terminal
$ ./Ceflo nombre.cf
erreur de syntaxe: nombre mal formé
  --> nombre.cf:1:9
```
````

## Les encadrés

```
> [!note]
> Une note de marge : deux ou trois phrases, affichées en post-it à côté du texte.

> [!essaie]
> Un exercice. Ce qu'il faut faire, puis où regarder si on bloque.

> [!attention]
> Un piège classique, un bug qu'on ferait sûrement.
```

## Les jetons

Pour montrer comment une ligne est découpée, une figure en étiquettes :

```
::: jetons "var x = 12, une fois passée dans le lexer"
var TOKEN_VAR
x TOKEN_ID
= TOKEN_EQUAL
12 TOKEN_INTEGER
:::
```

Chaque ligne : le texte du jeton, puis son nom. La légende entre guillemets est
écrite à la main sous la figure.

## Les schémas simples

```
::: etapes
texte -> jetons -> arbre -> bytecode -> résultat
:::
```

Une chaîne d'étapes séparées par `->`, affichée en étiquettes reliées.
