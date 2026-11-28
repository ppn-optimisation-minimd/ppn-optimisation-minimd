Modifications de `void ForceLJ::compute`:
Retrait de *if* qui verifiait le nombre de thread, comme on fait du mono-threading, j'ai uniquement laissé les branches pour un seul thread.

J'ai ensuite forcé la version utilisé à `compute_original`: gros gros speedup mais la sémantique est affectée

J'ai enlevé les branches: `if(use_oldcompute)` -> le temps passé dans `void ForceLJ::compute` a augmenté

MAQAO indique de compiler avec `-mprefer-vector-width=512 ` -> aucun changement


-> Utiliser PRAGMA OMP SIMD