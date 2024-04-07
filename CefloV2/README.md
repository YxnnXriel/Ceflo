# CeFlo v2

CeFlo v2 est un langage de programmation écrit en C. C'est un langage interprété avec un garbage collector intégré, utilisant des techniques d'interprétation rapide.

## Version actuelle : AST

Remarquez que nous sommes actuellement dans la version 2 du langage, également connue sous le nom de version AST (Abstract Syntax Tree). Cette version est une étape importante dans le développement de CeFlo, préparant le terrain pour les versions futures.

## Caractéristiques principales de Ceflo V2

1. **Syntaxe Familière** : Ceflo V2 utilise une syntaxe similaire à celle de langages de programmation populaires comme C et Python, facilitant ainsi son apprentissage pour les programmeurs.

2. **Interprété avec Garbage Collector** : Ceflo V2 est un langage interprété qui dispose d'un garbage collector intégré pour gérer automatiquement la mémoire, évitant ainsi les fuites de mémoire et simplifiant le développement.

3. **Développement Actif** : Bien que toujours en développement, Ceflo V2 bénéficie de mises à jour régulières pour améliorer ses fonctionnalités et corriger les bugs.

4. **Types de Données et Structures** : Ceflo V2 prend en charge une variété de types de données, y compris les entiers, les chaînes de caractères et les listes. Il permet également la définition de structures de données personnalisées avec attributs et méthodes.

5. **Manipulation de Listes et de Structures** : Le langage permet la manipulation de listes et de structures de données à l'aide de boucles et d'opérations d'ajout d'éléments.

6. **Fonctions et Procédures** : Ceflo V2 permet la définition de fonctions et de procédures avec des paramètres et des valeurs de retour. Il prend également en charge la surcharge de fonctions et la déclaration de fonctions sans parenthèses.

7. **Opérations Ternaires et Logiques** : Le langage prend en charge les opérations ternaires et logiques, permettant d'écrire des expressions conditionnelles de manière concise.

8. **Orienté Objet Simplifié** : Bien que n'étant pas un langage orienté objet à part entière, Ceflo V2 permet la définition de structures avec des attributs et des méthodes, offrant ainsi une certaine flexibilité pour la programmation orientée objet.

## Exemples et Utilisation

Avant de l'essayer, il est recommandé de parcourir les exemples rapides fournis pour se familiariser avec le langage. Les exemples proposés couvrent les bases de la programmation ainsi que des éléments avancés, déjà intégrés dans certains cas.



## Installation des fichiers

### Installation sous Linux

1. **Ajout au chemin d'environnement :**
   - Ajoutez le répertoire `bin/Linux` au chemin d'environnement de votre système.
2.   - Dans ce répertoire, vous trouverez également le dossier `lib`. Ajoutez également ce chemin à votre variable d'environnement.
   - Exemple (bash) :
     ```bash
     export PATH=$PATH:/chemin/vers/le/dossier/bin/Linux
     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/chemin/vers/le/dossier/bin/Linux/lib
     ```

### Installation sous Windows

1. **Ajout au chemin d'environnement :**
   - Ajoutez le répertoire `bin/Win` au chemin d'environnement de votre système.
   - Exemple (PowerShell) :
     ```powershell
     $env:Path += ";C:\chemin\vers\le\dossier\bin\Win"
     ```

## Tester l'exécutable :

- Dans votre terminal, exécutez la commande suivante pour tester l'exécutable :
  ```cmd 
    ceflo
  ```
### Intégration avec Visual Studio Code

Si vous utilisez Visual Studio Code, copiez le dossier `ceflo` qui se trouve dans le répertoire dans le dossier `.vscode/extension`.


## Conclusion

CeFlo v2 offre un potentiel prometteur pour les développeurs, malgré son état de développement. Nous continuons à évaluer sa fiabilité et à travailler sur son amélioration continue.

Merci.
