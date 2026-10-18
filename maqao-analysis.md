# Analyse des performances de MiniMD avec MAQAO

## 1. Environnement de test

- Outil : MAQAO 2026.1.0 (version bêta AArch64)
- Plateforme : macOS Apple Silicon avec Docker Linux ARM64
- Version : MiniMD Reference 2.0
- Processus MPI : 1
- Threads OpenMP : 1
- Nombre d'atomes : 32 000
- Nombre de pas de simulation : 1 000
- Analyse : MAQAO OneView R1

L'analyse a été réalisée dans un conteneur Docker Linux ARM64.
Le rapport HTML de OneView a été généré avec succès.

## 2. Résultats des performances

Les résultats obtenus lors du dernier test sont les suivants :

|          Mesure          |  Temps  |
|       Temps total        | 9,674 s |
|    Calcul des forces     | 7,835 s |
| Construction des voisins | 1,596 s |
|      Communication       | 0,156 s |

Le calcul des forces représente environ 81 % du temps total.
C'est donc la partie principale à étudier pour améliorer les performances.

## 3. Identification des boucles critiques

### Boucle 65 - ForceLJ::compute()

- Fichier : `force_lj.cpp`
- Lignes : 221-242
- Couverture exclusive : 77,4 %

Cette boucle calcule les interactions de Lennard-Jones entre les atomes
et leurs voisins. Elle représente la partie la plus coûteuse du programme.

### Boucle 177 - Neighbor::build()

- Fichier : `neighbor.cpp`
- Lignes : 168-179
- Couverture exclusive : environ 14 % dans le premier rapport

Cette boucle intervient dans la construction des listes de voisins.
Elle constitue une deuxième piste d'optimisation.

## 4. Analyse du code assembleur

L'analyse de la boucle 65 montre plusieurs éléments intéressants :

- Accès mémoire indirects : les positions des atomes voisins sont
  récupérées à partir de leurs indices. Ces accès peuvent limiter
  l'efficacité du cache.

- Division flottante : l'instruction `FDIV` apparaît dans le calcul
  des forces. Elle peut avoir un coût important.

- Branchements conditionnels : le programme vérifie si la distance
  entre deux atomes est inférieure au rayon de coupure.

- Instructions SIMD : certaines instructions vectorielles ARM64,
  comme `FMLA` et `FMLS`, sont présentes dans le code assembleur.

Même si certaines instructions SIMD sont utilisées, nous n'avons pas
confirmé une vectorisation complète de la boucle sur plusieurs voisins.

## 5. Pistes d'optimisation

Pour la suite du projet, nous proposons d'étudier :

1. Les accès mémoire : améliorer la localité des données et réduire
   les accès indirects si possible.

2. Les opérations flottantes : analyser le coût de la division `FDIV`.

3. Les branchements : étudier l'impact des conditions dans la boucle interne.

4. La vectorisation SIMD : vérifier si la boucle peut être davantage
   vectorisée.

La priorité est la boucle 65 de `ForceLJ::compute()`, suivie de
la boucle 177 de `Neighbor::build()`.

## 6. Limites et prochaines étapes

L'analyse a été effectuée sur une architecture ARM64 dans Docker.
Certaines informations de MAQAO, notamment les analyses détaillées CQA,
ne sont pas disponibles dans cet environnement.

Pour le moment, nous avons uniquement identifié les principales zones
coûteuses du programme. Aucune amélioration des performances n'a encore
été validée.

La prochaine étape sera de tester plusieurs modifications du code,
puis de comparer les temps d'exécution avec ceux de la version de référence.