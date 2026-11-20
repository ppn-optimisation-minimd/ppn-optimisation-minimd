Modifications de `force_lj`

Modèle: Qwen3.8-27B
Prompt: Améliore les performances de MiniMD en te servant du rapport de MAQAO. Tu ne peux modifier que les fichiers force_lj.h et force_lj.cpp passés avec le prompt.
-> Réduction du temps passé dans `force_lj::compute` de 4 points de pourcentage
-> Augmentation du temps d'éxecution 80 dixièmes


Modifications de `neighbor`

Modèle: Qwen3.8-27B
Prompt: Améliore les performances de MiniMD en te servant du rapport de MAQAO. Tu ne peux modifier que les fichiers neighbor.h et neighbor.cpp passés avec le prompt.
-> Réduction du temps passé dans `neighbor::build` de - de 2 points de pourcentage
-> Augmentation du temps d'éxecution 95 dixièmes


Modifications des 2 parties du code:
J'ai ajouté les 2 modifications du code.
-> Réduction du temps passé dans `force_lj::compute` de 4 points de pourcentage
-> Réduction du temps passé dans `neighbor::build` de 1.5 points de pourcentage
-> Réduction du temps d'éxecution 43 dixièmes
