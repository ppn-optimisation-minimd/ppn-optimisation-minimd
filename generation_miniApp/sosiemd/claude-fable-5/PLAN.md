# Plan de travail — sosieMD (Claude Fable 5)

Ce fichier est tenu à jour après chaque étape. Il sert à reprendre le travail
sans rien perdre si le début de la session est résumé.

## État d'avancement

| # | Étape | État |
|---|-------|------|
| 0 | Lecture complète de `baseline/ref/` et mesures de référence miniMD | fait |
| 1 | LJ, 1 processus, 1 thread, cas court | fait |
| 2 | Cas du groupe (4 M d'atomes) | fait |
| 3 | Threads OpenMP | fait |
| 4 | Processus MPI | fait |
| 5 | EAM | fait |
| 6 | Fichier de données, options restantes, YAML | fait |
| 7 | Script de validation complet | fait |
| 8 | Relecture, README, compte rendu | fait |

## Architecture retenue

Tout est dans `miniapp-llm/claude-fable-5/`. Sources dans `src/`, binaire
`sosieMD` à la racine du dossier, validation par `validate.sh`, qui exécute
tout dans `validation/` (lien symbolique vers `Cu_u6.eam`, journaux dans
`validation/logs/`). Les essais manuels ont été faits dans un dossier `work/`
du même genre, supprimé à la fin.

| Fichier | Rôle |
|---------|------|
| `src/main.cpp` | déroulement général : lecture, création, pas 0, boucle, bilan |
| `src/config.h/.cpp` | fichier d'entrée + ligne de commande → `Config` |
| `src/atoms.h/.cpp` | `Box`, `Atoms` (tableaux x, v, f, type), conditions périodiques, tri |
| `src/setup.h/.cpp` | réseau cfc, vitesses initiales, lecture d'un fichier LAMMPS |
| `src/domain.h/.cpp` | décomposition MPI, échange d'atomes, fantômes, communications |
| `src/neighbor.h/.cpp` | grille de cellules et listes de voisins |
| `src/force.h`, `force_lj.*`, `force_eam.*` | potentiels |
| `src/thermo.h/.cpp` | T, U, P et leur historique |
| `src/integrate.h/.cpp` | boucle en temps (Verlet vitesse) et chronométrage |
| `src/output.h/.cpp` | en-tête, résumé de performance, YAML |
| `src/timer.h`, `src/util.h` | chronomètres, arrêts sur erreur |

Choix de conception (décisions prises) :

- Tableaux plats `std::vector<double>` (3 valeurs par atome), fantômes rangés
  après les atomes locaux.
- Cellules en format compact (début de cellule + liste d'atomes, tri par
  comptage séquentiel, déterministe). Sert aussi au tri des atomes.
- Listes de voisins à largeur fixe (`maxneighs` cases par atome, agrandies et
  reconstruites en cas de débordement), construites en parallèle.
- OpenMP : des `parallel for` simples sur les boucles chaudes (pas de grande
  région parallèle comme miniMD). Demi-listes à plusieurs threads : mise à
  jour atomique de `f[j]`, réductions pour énergie et viriel.
- MPI : même décomposition que miniMD (grille cartésienne de surface minimale,
  échanges par 6 faces, tranches fantômes), appels MPI hors des régions
  parallèles.
- Même ordre de création des atomes, même générateur, mêmes fréquences de
  tri / reconstruction / échange que miniMD.

## Faits relevés dans miniMD `ref` (vérifiés dans le code ou par exécution)

- Compilé avec `-DNOCHUNK` : `schedule(static)`.
- Pas 0 : `exchange`, tri seulement si `--sort N>0`, `borders`, listes, forces.
- Boucle : demi-pas, puis soit `communicate`, soit (tous les `neigh_every`)
  `exchange` + tri (si `n+1 >= next_sort`) + `borders` + listes ; forces ;
  communication inverse si demi-listes et Newton fantôme ; second demi-pas ;
  thermo si `(n+1) % nstat == 0`.
- Après la boucle : forces recalculées avec énergie, thermo final seulement si
  `nstat == 0` ou `ntimes % nstat != 0`.
- EAM force `ghost_newton = 0` (message). EAM + demi-listes + plusieurs
  threads : miniMD ne calcule **aucune** force (`return` immédiat) → U, P
  aberrants (constaté : `3.48e+249`). sosieMD calcule correctement.
- EAM : `nr = nrho = (valeur du fichier) - 1` ; `z2r = 27.2*0.529*z²`.
- `thermo 0` dans le fichier d'entrée : miniMD plante (division entière par
  zéro, SIGFPE constaté). sosieMD : affichage au début et à la fin.
- `--safe_exchange`, `-d`, `-dm`, `-ng`, `--skip_gpu` figurent dans l'aide mais
  ne sont pas lus par miniMD `ref` ; `--teams` est lu puis ignoré ;
  `-sse N≠0` fait quitter miniMD `ref`.
- `-ny`/`-nz` ne sont pris en compte que si `-nx` est donné.
- YAML désactivé par défaut (`-o 0`) malgré l'aide ; `force_params` y est
  imprimé à partir de pointeurs (valeurs fausses dans miniMD).
- La colonne Time de la dernière ligne thermo (appel final) n'a pas de sens
  dans miniMD (heure courante moins durée).
- Fichier de données : types tirés par `rand()`, la boîte est supposée partir
  de 0, une seule ligne lue dans `Masses`, nombre de cellules déduit de la
  densité si `-b` absent.

## Valeurs de référence mesurées (miniMD, cette machine, 1 proc, 1 thread)

- Cas court : pas 0 `1.440000e+00 -6.773368e+00 -5.019707e+00`,
  pas 100 `7.522746e-01 -5.750134e+00 2.187181e-01`,
  pas 200 `7.571498e-01 -5.758905e+00 1.900173e-01`, t_total ≈ 1.26 s.
- EAM : pas 0 `6.000000e+02 -3.554527e+06 -1.641970e+05`,
  pas 100 `2.861774e+02 -3.511991e+06 -1.455078e+05`, t_total ≈ 1.21 s.
- Identiques à l'affichage avec `-t 4`, `-np 4`, `-np 2 -t 2` (LJ) et `-np 4`
  (EAM).

## Journal

- Étape 0 : lecture de tous les fichiers de `baseline/ref/`, exécutions de
  référence dans `work/`.
- Étape 1 : première version complète du code (tous les fichiers de `src/`
  sauf EAM), compilation sans avertissement du premier coup. Cas court :
  T, U, P identiques à miniMD sur les 10 chiffres du YAML aux pas 0, 100, 200 ;
  Nghost, Nswaps, Neighs identiques. t_total 1,34 s contre 1,26 s.
- Étape 2 : cas du groupe, YAML identique à 10 chiffres (pas 0 et 20).
  t_total 14,8 s (miniMD 14,0 s) ; t_force 11,9 / 10,8 ; t_neigh 2,36 / 2,74.
  Mémoire maximale 2,41 Go (miniMD 2,49 Go).
- Étape 3 : `-t 1, 2, 4, 8` : mêmes 10 chiffres au pas 200, demi-listes et
  listes complètes. Constat : `mpirun -np 1` lie le processus à un cœur ;
  il faut `--bind-to none` pour que les threads accélèrent (vrai aussi pour
  miniMD).
- Étape 4 : `-np 2, 3, 4, 6, 8`, `-np 2 -t 2`, `--half_neigh 0`, `-gn 0` :
  mêmes 10 chiffres que miniMD et mêmes statistiques Nlocal / Nghost / Neighs.
- Étape 5 : EAM (`force_eam.*`). Mêmes 10 chiffres que miniMD aux pas 0 et 100
  en `-np 1`, `-np 4`, listes complètes `-t 4`, `-np 4`, `-np 3 -t 2`.
  Demi-listes avec threads : sosieMD donne les bons chiffres, miniMD non.
  Décision : les tables EAM reprennent directement les valeurs du fichier
  (miniMD les fait passer par une interpolation de Lagrange sur la même grille,
  identité à l'arrondi près).
- Étape 6 : fichiers de données fabriqués (réseau cfc perturbé, 2 048
  atomes, LJ et cuivre) lus par les deux programmes, mêmes 10 chiffres en
  `-np 1` et `-np 4`. Options `--sort`, `-b`, `-nx/-ny/-nz`, `--ntypes`,
  `--half_neigh -1`, `-gn 0`, `-u`, `-p`, petites boîtes (`-s 1, 2, 3`),
  `-np 3, 5, 6, 8, 12, 24` : mêmes 10 chiffres que miniMD. Seul écart :
  `--half_neigh -1 -gn 0`, où miniMD compte deux fois les paires
  local-fantôme dans U et P. YAML : identique hors temps, `variant` et
  `force_params`. `--check_exchange` : les avertissements de miniMD sont
  faux (positions de référence non initialisées puis permutées par le tri).
  Ajout d'un arrêt propre quand un atome sort de la grille (les deux
  programmes plantaient sur une simulation instable).
- Étape 7 : `validate.sh` (mode `--quick`). Vérifié qu'il échoue (code 1)
  avec un faux miniMD dont une valeur est altérée de 1,3e-6 au pas 100 et du
  dernier chiffre au pas 0.
- Étape 8 : relecture du code (chronométrage total déplacé dans `run`,
  `Domain` connaît son rang dès sa construction, marge sur le tableau des
  voisins), README, cas du groupe en `-t 4`, `-t 8`, `-np 4`, `-np 2 -t 2`
  (mêmes valeurs que miniMD), dossier `work/` supprimé.
- Validation finale complète (`./validate.sh`, 3 min 05 s) : 36 vérifications,
  aucun échec ; sortie dans `validation/rapport.txt`. Temps du cas du groupe
  dans cette exécution : miniMD 14,05 s (force 10,78, voisins 2,76),
  sosieMD 15,05 s (force 12,06, voisins 2,41).

## Reste à faire

Rien de prévu. Pistes laissées à l'étape d'optimisation : voir la fin du
README (drapeaux testés à l'exécution dans le noyau LJ, ajouts atomiques en
demi-listes à plusieurs threads, parties séquentielles, mémoire des listes).
