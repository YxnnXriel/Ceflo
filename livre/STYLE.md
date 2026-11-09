# Comment écrire ce livre

Le lecteur doit avoir envie de lire le chapitre suivant, comprendre chaque ligne
qu'il tape, et être capable, à la fin, de réécrire CeFlo seul. Tout ce qui suit
sert ces trois buts.

## La voix

- **On tutoie le lecteur.** « Tu », jamais « vous ». On écrit comme un ami qui
  connaît bien le sujet et qui est assis à côté de lui devant l'écran.
- **On dit « on » pour ce qu'on construit ensemble** : « on ajoute un champ »,
  « on lance le test ». « Je » est réservé aux rares choix personnels de
  l'auteur (« j'ai gardé ce nom parce que... »).
- **Des phrases courtes et concrètes.** Un nom de fonction, un fichier, une
  valeur précise valent mieux qu'une idée générale. « `lexer_peek` lit le
  caractère suivant sans avancer » plutôt que « le lexer dispose d'un mécanisme
  d'anticipation ».
- **On montre avant d'expliquer.** D'abord le petit programme, la sortie du
  terminal ou le bug, ensuite le pourquoi.
- **On raconte une progression.** Chaque section part d'un problème que le
  lecteur ressent (« si on s'arrête là, `1.lenght()` devient un nombre ») et se
  termine quand ce problème est réglé.

## Ce qu'on n'écrit jamais

Ce sont les tics des textes générés automatiquement. Le lecteur les reconnaît
tout de suite, et il décroche.

- **Aucun tiret long ni moyen** (« — » ou « – »). On utilise une virgule, deux
  points, des parenthèses, ou on coupe la phrase en deux.
- **Pas d'annonces vides** : « Dans ce chapitre, nous allons explorer... »,
  « Plongeons dans... », « Voyons maintenant... », « Il est important de
  noter que... », « En résumé... », « Passons à... ».
- **Pas de formules creuses** : « la magie opère », « au coeur de »,
  « véritable », « puissant », « robuste », « élégant », « fascinant »,
  « crucial », « essentiel », « incontournable », « un jeu d'enfant »,
  « sans plus attendre ».
- **Pas de séries de trois adjectifs ou de trois idées pour faire joli.** Si
  deux choses suffisent, on en dit deux.
- **Pas de questions rhétoriques en cascade** (« Mais comment faire ? Et
  pourquoi ? »). Une vraie question de temps en temps, oui.
- **Pas de listes à puces quand une phrase suffit.** Les listes servent aux
  vraies énumérations (les champs d'une structure, les étapes à taper).
- **Pas de gras décoratif.** Le gras sert à un mot qu'on définit pour la
  première fois, rien d'autre.
- **Pas d'emoji**, pas de points d'exclamation à répétition.
- **Pas de conclusion qui répète le chapitre.** Un chapitre finit sur ce qui
  marche maintenant, et sur ce qui manque encore (c'est ce qui donne envie de
  tourner la page).

## Rendre le livre impossible à lâcher

- **Chaque chapitre ouvre sur une situation**, pas sur une définition : un
  programme qui ne marche pas encore, une question que le lecteur se pose, un
  chiffre surprenant (`fib(30)` prend 0,57 s avec le visitor, 0,10 s avec la
  VM).
- **Chaque section se termine sur quelque chose qui tourne.** Le lecteur tape,
  compile, et voit une sortie. On lui montre la sortie exacte.
- **On sème des questions** qu'un chapitre suivant résoudra (« pourquoi le
  visitor est-il si lent ? On le mesurera au chapitre 9 »).
- **On ose les détails qui font plaisir** : le bug qu'on aurait fait, le cas
  limite qui casse tout, la petite astuce du vrai code. Ce sont eux qui donnent
  l'impression d'apprendre quelque chose de réel.
- **Les exercices sont de vrais petits défis**, pas des questions de cours :
  « ajoute l'opérateur `**` », « fais afficher le numéro de ligne de chaque
  jeton ». Chaque exercice dit où regarder si on bloque.

## Le niveau de détail : on construit, on ne visite pas

Le modèle est *Crafting Interpreters* de Robert Nystrom : le lecteur écrit
le langage avec nous, morceau par morceau, et comprend chaque ligne qu'il tape.
Un chapitre qui se contente de commenter le code fini ne suffit pas.

- **Le lecteur tape le code.** Chaque morceau dit où il va : « dans
  `src/lexer.c`, juste après `lexer_advance` », « remplace le `case` qu'on
  avait écrit à la section 2.3 ». On part d'un fichier vide ou presque, et le
  chapitre le remplit dans l'ordre où on le construirait vraiment.
- **Ça compile à chaque étape.** À la fin de chaque sous-chapitre, le lecteur
  peut compiler et lancer quelque chose. Si une fonction du dépôt dépend d'une
  chose qui arrive plus tard, on écrit d'abord une version plus simple (testée
  pour de vrai dans `build/`), puis on la fait évoluer jusqu'au code du dépôt,
  en montrant ce qui change et pourquoi.
- **Chaque ligne a sa raison.** On explique les lignes qui ne sont pas
  évidentes une par une : pourquoi ce test, pourquoi cet ordre, ce qui
  casserait sans elle. Montrer le bug qu'on éviterait est souvent la meilleure
  explication.
- **Chaque choix a ses concurrents.** Quand le code fait un choix (une macro
  plutôt que deux listes, un seul `struct` pour tous les noeuds, la descente
  récursive plutôt qu'un générateur de parser), on dit ce qu'on aurait pu faire
  d'autre, ce que font d'autres langages (Python, Lua, JavaScript, C), et
  pourquoi CeFlo a choisi ainsi.
- **La théorie arrive quand on en a besoin**, pas avant : la grammaire au
  moment d'écrire le parser, la notion de racine au moment où le GC libère trop.
  On donne les vrais mots (« analyse lexicale », « descente récursive »,
  « mark and sweep », « upvalue ») pour que le lecteur puisse chercher ailleurs.
- **Des dessins quand le texte peine** : l'état de la pile instruction par
  instruction, l'arbre d'une expression, les portées emboîtées. On les dessine
  en texte dans un bloc ` ```text `, ou avec `::: etapes` et `::: jetons`.
- **Chaque chapitre finit par deux sections** :
  - `## Défis` : trois ou quatre exercices de difficulté croissante, qui
    obligent à modifier le vrai code (ajouter un opérateur, changer une règle,
    mesurer une variante). Chacun dit où regarder pour commencer.
  - `## Note de conception` : une page plus libre sur une question plus large
    soulevée par le chapitre (pourquoi les messages d'erreur comptent autant
    que le langage, faut-il des points-virgules, le prix d'un GC...), avec un
    avis assumé.
- **Un chapitre complet fait entre 7 000 et 12 000 mots.** La longueur vient
  des explications et des étapes, jamais du remplissage.

## L'exactitude

- **Tout le code montré vient du dépôt**, tel quel, avec son fichier et ses
  vrais numéros de ligne. On le vérifie avec `sed -n 'A,Bp' fichier` avant de
  l'écrire. Si on coupe un extrait, on le dit.
- **Toute sortie de terminal est réelle** : on lance la commande, on copie ce
  qu'elle affiche.
- **Tout programme CeFlo montré tourne** avec `./Ceflo` (et donne la sortie
  annoncée).
- **Un petit programme C d'essai** (afficher les jetons, par exemple) est écrit
  et compilé pour de vrai avant d'être mis dans le livre.
- Si le code du dépôt a une bizarrerie (`lenght`, `colone`), on la garde et on
  l'explique en une phrase, sans en faire une histoire.

## La mise en forme

- Un sous-chapitre (`##`) traite une seule idée, et dure entre 500 et 1 500 mots.
- Un paragraphe dépasse rarement cinq lignes.
- Un bloc de code dépasse rarement 30 lignes. Au-delà, on le coupe et on
  explique chaque morceau.
- Les notes de marge (`[!note]`) sont courtes : deux ou trois phrases, une
  anecdote, une précision, un lien vers un autre chapitre.
