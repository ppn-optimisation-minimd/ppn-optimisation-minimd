# sosieMD

sosieMD est une mini-application de dynamique moléculaire équivalente à
miniMD (projet Mantevo), variante `ref`. Elle a été écrite entièrement par un
LLM (Claude Fable 5) pour être comparée à miniMD : taille et lisibilité du
code, précision, mémoire, performance.

Elle fait la même simulation que miniMD : mêmes conditions initiales, même
physique (Lennard-Jones ou EAM, unités `lj` ou `metal`), mêmes grandeurs
affichées, mêmes options de ligne de commande. Elle se lance à la place de
miniMD sans changer le reste de la ligne de commande.

Tout ce qui est affirmé dans ce fichier a été exécuté sur la machine du projet
(Intel Core Ultra 9 275HX, 24 cœurs, GCC 16.2.1, Open MPI 5.0.9).

## Compiler

```sh
make            # produit ./sosieMD
make clean
```

Le `Makefile` utilise `mpicxx` et, par défaut, les options de miniMD dans ce
projet :

```
-Ofast -g -march=native -fno-inline-functions -fno-omit-frame-pointer -fopenmp
```

auxquelles s'ajoutent `-Wall -Wextra` (aucun avertissement). Elles se
remplacent depuis la ligne de commande :

```sh
make clean && make CXXFLAGS="-O2 -g -fopenmp"
```

`-fopenmp` est nécessaire. Dépendances : bibliothèque standard C++, OpenMP,
MPI.

## Lancer

```sh
mpirun -np 1 ./sosieMD -i ../../launcher/in.param_analyse.miniMD -s 100
mpirun -np 4 ./sosieMD -i ../../launcher/in.param_analyse.miniMD -s 20 -n 200
mpirun --bind-to none -np 1 ./sosieMD -i in.eam.miniMD -s 20 -n 100 -t 4
```

- Le fichier d'entrée a le format de miniMD (14 lignes, voir
  `baseline/ref/in.lj.miniMD`).
- Avec EAM, le fichier `Cu_u6.eam` est cherché dans le répertoire courant,
  comme le fait miniMD. Le fichier YAML est écrit dans le répertoire courant.
- **Threads** : `mpirun -np 1` lie le processus à un seul cœur. Sans
  `--bind-to none`, tous les threads se partagent ce cœur et `-t 4` n'accélère
  rien (constaté : 2,35 s avec ou sans `-t 4` en listes complètes sur le cas
  court, 0,75 s avec `--bind-to none`). C'est vrai aussi pour miniMD.

### Options

Mêmes noms, même sens et mêmes valeurs par défaut que miniMD `ref`
(`./sosieMD -h` les rappelle).

| Option | Effet |
|---|---|
| `-i`, `--input_file <fichier>` | fichier d'entrée (défaut `in.lj.miniMD`) |
| `-n`, `--nsteps <n>` | nombre de pas de temps |
| `-s`, `--size <n>` | taille du réseau dans les trois directions |
| `-nx`, `-ny`, `-nz <n>` | taille par direction (`-ny`, `-nz` sans `-nx` sont ignorés, comme dans miniMD) |
| `-t`, `--num_threads <n>` | threads OpenMP par processus (défaut 1) |
| `--half_neigh <1\|0\|-1>` | demi-listes (défaut), listes complètes, demi-listes avec forces calculées sans threads |
| `-gn`, `--ghost_newton <0\|1>` | 3e loi de Newton sur les atomes fantômes (défaut 1 ; forcé à 0 avec EAM) |
| `--sort <n>` | tri des atomes tous les `n` pas (défaut : période des listes ; 0 : jamais) |
| `-b`, `--neigh_bins <n>` | nombre de cellules de voisinage par direction |
| `--ntypes <n>` | nombre de types d'atomes (défaut 4) |
| `-u`, `--units <lj\|metal>` | système d'unités |
| `-p`, `--force <lj\|eam>` | potentiel |
| `-f`, `--data_file <fichier>` | configuration lue dans un fichier de données LAMMPS |
| `--check_exchange` | avertir si un atome parcourt plus d'un sous-domaine entre deux échanges |
| `-o`, `--yaml_output <n>` | écrire le fichier YAML si `n` ≠ 0 (défaut 0) |
| `--yaml_screen` | écrire aussi le YAML sur la sortie standard |
| `-h`, `--help` | aide |
| `--safe_exchange`, `--teams <n>` | acceptés sans effet avec un avertissement (miniMD `ref` ne les utilise pas non plus) |
| `-sse <n>`, `-d`, `-dm`, `-ng`, `--skip_gpu` | sans objet (intrinsèques, GPU) : acceptés sans effet avec un avertissement |

Une option inconnue ou une valeur invalide provoque une erreur et un code de
retour non nul.

### Sorties

Sur la sortie standard, dans cet ordre : l'en-tête (paramètres de
l'exécution), le tableau thermodynamique (`# Timestep T U P Time`, une ligne
aux mêmes pas que miniMD), puis le bloc `# Performance Summary:` et sa ligne
`PERF_SUMMARY`, aux mêmes colonnes que miniMD :

```
# MPI_proc OMP_threads nsteps natoms t_total t_force t_neigh t_comm t_other performance perf/thread grep_string t_extra
```

Avec `-o 1`, le fichier `sosieMD-AAAA-MM-JJ-HH-MM-SS.yaml` reprend les
paramètres, l'historique de T, U, P à 10 chiffres, les temps et les
statistiques par processus (atomes locaux, fantômes, voisins), avec les mêmes
clés que le fichier `miniMD-*.yaml`.

## Organisation du code

Tout le code est dans `src/` (2 555 lignes, commentaires compris). Lire
`main.cpp` en premier : il donne le déroulement complet.

| Fichier | Rôle |
|---|---|
| `main.cpp` | déroulement d'une exécution : lecture, création, pas 0, boucle, bilan |
| `config.h/.cpp` | `Config` : fichier d'entrée et ligne de commande |
| `atoms.h/.cpp` | `Box`, `Atoms` : tableaux x, v, f, type ; conditions périodiques ; réordonnancement |
| `setup.h/.cpp` | réseau cfc, vitesses initiales, lecture d'un fichier de données LAMMPS |
| `domain.h/.cpp` | `Domain` : décomposition entre processus MPI, migration des atomes, fantômes |
| `neighbor.h/.cpp` | `Neighbor` : grille de cellules et listes de voisins |
| `force.h` | interface `Force` commune aux potentiels |
| `force_lj.h/.cpp` | Lennard-Jones (demi-listes et listes complètes) |
| `force_eam.h/.cpp` | EAM : lecture de `Cu_u6.eam`, tables interpolées, forces |
| `thermo.h/.cpp` | `Thermo` : T, U, P et constantes d'unités |
| `integrate.h/.cpp` | boucle en temps (Verlet vitesse) et chronométrage |
| `output.h/.cpp` | en-tête, résumé de performance, YAML |
| `timer.h`, `util.h` | chronomètres, arrêts sur erreur |

Autres fichiers : `Makefile`, `validate.sh` (validation contre miniMD),
`PLAN.md` (plan et journal du travail).

### Un pas de temps (`integrate.cpp`)

1. premier demi-pas : vitesses (demi-pas), positions (pas entier) ;
2. si le pas n'est pas multiple de la période des listes : mise à jour de la
   position des fantômes (`Domain::forward_positions`) ;
3. sinon : migration des atomes sortis de leur sous-domaine
   (`Domain::exchange`), tri éventuel des atomes par cellule
   (`Atoms::reorder`), reconstruction des fantômes (`Domain::borders`) et des
   listes de voisins (`Neighbor::build`) ;
4. forces (`Force::compute`), avec énergie et viriel seulement aux pas
   affichés ;
5. avec les demi-listes et la 3e loi sur les fantômes : renvoi des forces des
   fantômes à leurs propriétaires (`Domain::reverse_forces`) ;
6. second demi-pas des vitesses ;
7. affichage de T, U, P si le pas est multiple de la période d'affichage.

## Choix de conception

- **Structures de données.** Tableaux plats `std::vector<double>` à 3 valeurs
  par atome pour x, v, f, et un `std::vector<int>` pour le type. Les atomes
  locaux d'un processus sont suivis de ses fantômes (copies d'atomes d'un
  processus voisin ou de l'autre côté de la boîte périodique).
- **Cellules.** La boîte est découpée en cellules (5 pour 6 mailles par
  défaut, comme miniMD). Les atomes y sont rangés par un tri par comptage
  séquentiel et déterministe (début de cellule + liste d'atomes), là où
  miniMD utilise des cellules de capacité fixe remplies par des opérations
  atomiques. Le même rangement donne l'ordre du tri des atomes.
- **Listes de voisins.** Tableau à largeur fixe (`maxneighs` cases par atome,
  100 au départ), agrandi et reconstruit si un atome déborde. Construction en
  parallèle sur les atomes. Trois variantes, comme miniMD : demi-listes avec
  3e loi sur les fantômes (motif de cellules « moitié supérieure »),
  demi-listes sans, listes complètes.
- **Threads.** De simples `#pragma omp parallel for` sur les boucles chaudes
  (forces, listes, intégration, température), pas de grande région parallèle.
  En demi-listes, la force sur le voisin `j` est ajoutée par une opération
  atomique quand il y a plusieurs threads (`add_to` dans `force.h`) ; énergie
  et viriel passent par des réductions. Les listes complètes n'ont aucune
  écriture partagée. Les communications et le tri par comptage sont
  séquentiels.
- **Processus.** Même décomposition que miniMD : grille cartésienne de
  processus qui minimise la surface des sous-domaines ; migration des atomes
  par les 6 faces ; fantômes construits dimension par dimension, par tranches
  d'épaisseur égale au rayon des listes, les images périodiques passant par
  les mêmes échanges. Tous les appels MPI sont hors des régions parallèles.
- **Mêmes conditions initiales.** Même ordre de création des atomes (blocs de
  8×8×8 sites), même générateur de vitesses (Park-Miller, graine tirée du
  site), même `rand()` initialisé par `srand(5413)` pour les types.
- **Une seule table EAM.** miniMD recopie les tables du potentiel pour chaque
  paire de types (toutes identiques) ; sosieMD n'en garde qu'un exemplaire.

## Différences connues avec miniMD

Sorties :

- la chaîne de variante est `sosieMD 1.0 (MPI+OpenMP)` et le fichier YAML
  s'appelle `sosieMD-<date>.yaml` ;
- dans le YAML, `force_params` donne epsilon et sigma ; miniMD y imprime des
  pointeurs (valeurs sans rapport, `2.50 0.00` observé) ;
- la colonne `Time` de la dernière ligne thermodynamique, quand elle est
  écrite après la boucle (cas du groupe : 20 pas, affichage tous les 100),
  vaut `t_total` ; miniMD y affiche une valeur sans signification ;
- les messages d'erreur vont sur la sortie d'erreur et le code de retour est
  alors non nul ; l'aide (`-h`) est rédigée différemment.

Comportements que miniMD `ref` n'a pas ou a de travers, constatés par
exécution :

- **EAM, demi-listes, plusieurs threads** : miniMD ne calcule pas les forces
  (T reste à 600, U = 3.48e+249). sosieMD les calcule et retrouve les valeurs
  à un thread.
- **`thermo 0`** dans le fichier d'entrée (« début et fin ») : miniMD s'arrête
  sur une division par zéro ; sosieMD affiche le début et la fin.
- **`--half_neigh -1 -gn 0`** : miniMD compte deux fois les paires
  local-fantôme dans U et P (U = -6.130 au lieu de -5.772 au pas 150 du cas
  testé) ; sosieMD donne les mêmes U et P qu'avec `--half_neigh 1`. T est
  identique dans les deux programmes.
- **`--check_exchange`** : miniMD compare à des positions non initialisées au
  premier contrôle, puis permutées par le tri, et affiche des avertissements à
  tort ; sosieMD compare aux positions de la dernière reconstruction.
- **Simulation instable** (atome projeté hors de la grille) : les deux
  programmes s'arrêtent, sosieMD avec un message au lieu d'une erreur de
  segmentation.
- **Options** : miniMD ignore en silence une option inconnue et lit `0` pour
  une valeur non numérique ; sosieMD s'arrête avec un message. `-sse N` (N ≠ 0)
  fait quitter miniMD `ref` ; sosieMD avertit et continue.
- **Fichier de données** : si des atomes sont hors de la boîte `[0, L[`,
  miniMD continue avec moins d'atomes ; sosieMD s'arrête. sosieMD accepte
  plusieurs lignes dans `Masses` (seule la première compte) et un commentaire
  après le nom d'une section (`Atoms # atomic`) ; miniMD échoue sur un tel
  fichier (`inf`, puis erreur de segmentation). Comme miniMD, sosieMD ignore
  les types du fichier (ils sont tirés au hasard) et suppose que la boîte
  part de 0.

Détails internes :

- les tables EAM reprennent directement les valeurs du fichier ; miniMD les
  fait passer par une interpolation de Lagrange sur la même grille, qui est
  l'identité à l'arrondi près (écart relatif attendu de l'ordre de 1e-13 sur
  quelques points, non mesuré) ; chaque processus lit le fichier lui-même ;
- après la boucle, les forces ne sont recalculées que si une dernière ligne
  thermodynamique doit être affichée (miniMD les recalcule toujours) ;
- avec plusieurs threads en demi-listes, l'ordre des ajouts atomiques dépend
  de l'exécution, dans sosieMD comme dans miniMD : les tout derniers chiffres
  peuvent donc varier d'une exécution à l'autre (supposition tirée du code,
  non mesurée ; les 10 chiffres du YAML n'ont pas varié entre `-t 1, 2, 4, 8`
  sur le cas court).

Non repris : la simple précision (`PRECISION=1`), le remplissage à 4 valeurs
par atome (`PAD4`) et les autres variantes de compilation de miniMD.

## Validation

```sh
./validate.sh            # tout, environ 3 minutes
./validate.sh --quick    # sans le cas à 4 millions d'atomes, environ 2 minutes
```

Le script recompile sosieMD (et vérifie l'absence d'avertissement), lance
miniMD et sosieMD avec les mêmes fichiers et les mêmes options dans
`validation/`, et compare les lignes du tableau thermodynamique : au pas 0
les valeurs affichées doivent être identiques, ensuite l'écart relatif doit
rester sous 1e-6. Il affiche un tableau des écarts et sort avec un code non
nul au moindre échec. Il couvre :

1. les trois cas de validation (court, EAM, groupe) à 1 processus et 1 thread ;
2. sosieMD en `-t 4`, `-np 4` et `-np 2 -t 2` contre miniMD à 1 processus et
   1 thread, sur le cas court et le cas EAM ;
3. les options qui changent le chemin de calcul : listes complètes, 3e loi
   désactivée sur les fantômes, `--half_neigh -1`, tri (`--sort 0`, `50`),
   grille imposée (`-b`), dimensions `-nx/-ny/-nz`, `--ntypes`, petite boîte,
   `-np 3`, `-np 8`, `-p`/`-u` ;
4. la lecture d'un fichier de données LAMMPS fabriqué par le script (LJ et
   EAM, 2 048 atomes, par `-f` et par le fichier d'entrée) ;
5. la sortie YAML (fichier et `--yaml_screen`), comparée ligne à ligne à
   celle de miniMD hors temps mesurés, `variant` et `force_params`.

Dernière exécution complète (3 min 05 s) : 36 vérifications, aucun échec.
Dans toutes les comparaisons, T, U et P sont identiques à ceux de miniMD sur
les 7 chiffres affichés, à tous les pas (écart relatif nul à cette
précision). La sortie du script est conservée dans `validation/rapport.txt`
et les journaux de chaque exécution dans `validation/logs/`.

## Performances mesurées

Un processus, un thread, options de compilation par défaut, temps en secondes
de la ligne `PERF_SUMMARY` (cas court et EAM : médiane de 3 exécutions ; cas
du groupe : moyenne de 2 exécutions, lancées seules) :

| Cas | Programme | t_total | t_force | t_neigh |
|---|---|---|---|---|
| court (LJ, 32 000 atomes, 200 pas) | miniMD | 1.256 | 1.000 | 0.226 |
| | sosieMD | 1.330 | 1.108 | 0.194 |
| EAM (32 000 atomes, 100 pas) | miniMD | 1.213 | 1.057 | 0.141 |
| | sosieMD | 1.156 | 1.022 | 0.122 |
| groupe (LJ, 4 000 000 atomes, 20 pas) | miniMD | 14.04 | 10.77 | 2.75 |
| | sosieMD | 14.94 | 11.98 | 2.38 |

Les deux exécutions du cas du groupe ont donné 14,02 et 14,05 s pour miniMD,
14,82 et 15,05 s pour sosieMD. `./validate.sh` réaffiche ces temps à chaque
exécution.

Mémoire maximale sur le cas du groupe (`/usr/bin/time`, 1 processus) :
2,41 Go pour sosieMD, 2,49 Go pour miniMD.

Le code n'a pas été réglé à la main. Ce qui est connu et laissé en l'état :

- en Lennard-Jones à un thread, le calcul des forces est environ 10 % plus
  lent que celui de miniMD : le noyau teste à l'exécution des drapeaux que
  miniMD fixe à la compilation (ajout atomique ou non, énergie ou non) ;
- à plusieurs threads, les demi-listes passent par des ajouts atomiques
  coûteux (cas du groupe en `-t 4` : 8,8 s, comme miniMD : 8,7 s) ;
- le tri par comptage des cellules et toutes les communications (empaquetage
  compris) sont séquentiels ;
- les listes de voisins à largeur fixe occupent `nlocal × maxneighs` entiers
  (1,6 Go sur le cas du groupe), bien plus que le nombre de voisins rangés.
