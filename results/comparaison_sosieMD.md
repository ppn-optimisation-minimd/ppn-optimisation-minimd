### Performances

- Total Time:
    mMD: 48.28
    sMD: 33.66

- Array access efficiency:
    mMD: 60%
    sMD: 92.9

- Code quality:
    sMD: Cumulative Outermost/In between loops coverage (71.97%) greater than cumulative innermost loop coverage (23.82%)

- Speedup potentiel:
    mMD: ~2.1
    sMD: ~5.7


### Architecture

Taille du code:
    mMD: 282,4 Ko
    sMD: 152.1 Ko

Nombre de fichiers:
    mMD: 26 fichiers
    sMD: 24 fichiers - + de commentaires dans les headers
    -> Il y a des équivalences entre les fichiers/classes

Taille des fichiers:
    mMD: fichiers + gros
        ex: 450 lignes pour `force_lj.cpp` dans mMD contre 150 lignes pour sMD
        ex: 500 lignes pour `neighbor.cpp` dans mMD contre 180 lignes pour sMD

Explication: mMD a des fonctions implémentées inutilisées comme `ForceLJ::compute_original`.

Lisibilité:
    Les lignes de codes en elles-même sont pas plus ou moins lisibles

    sMD: `main` explicitement trouvable dans `main.cpp` alors qu'il est dans `ljs.cpp` pour mMD. -> Les noms des fichiers sont un peu plus clairs.
