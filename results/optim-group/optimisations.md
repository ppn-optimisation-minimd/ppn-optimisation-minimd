Modifications de `void ForceLJ::compute`:
Retrait de *if* qui verifiait le nombre de thread, comme on fait du mono-threading, j'ai uniquement laissé les branches pour un seul thread.

J'ai ensuite forcé la version utilisé à `compute_original`: gros gros speedup