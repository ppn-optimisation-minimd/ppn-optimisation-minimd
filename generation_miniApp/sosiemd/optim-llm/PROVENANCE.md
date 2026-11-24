# Provenance de sosieMD

Ce dossier a été écrit par un LLM, pour l'étape 2 du projet (miniapp équivalente à miniMD, générée par LLM).
Cette fiche a été ajoutée après coup pour dire comment ; elle ne fait pas partie de ce que le LLM a produit.

| | |
|---|---|
| Modèle | Claude Fable 5 (`claude-fable-5-2`) |
| Niveau de réflexion | « high » |
| Outil | Claude Code 2.1.326, extension VSCode, sous-agent sans contexte préalable |
| Date | 19 novembre 2026 |
| Durée | 46 minutes, 78 appels d'outils |
| Prompt | [PROMPT.txt](PROMPT.txt), transmis une seule fois, sans relance |
| Rédaction du prompt | Claude Fable 5, à la demande d'Alexandre et d'après ses exigences |
| Entrées du LLM | `baseline/ref/` en lecture, le binaire `miniMD_openmpi`, `launcher/in.param_analyse.miniMD` |

## Ce que le LLM a écrit

`src/`, `Makefile`, `README.md`, `PLAN.md`, `validate.sh` et `.gitignore`, sans modification humaine.

## Vérification

Refaite séparément après la génération, sur une copie recompilée : aucun avertissement avec `-Wall -Wextra`, et T, U, P identiques à ceux de miniMD à tous les chiffres affichés sur :

- le cas du groupe (`-i launcher/in.param_analyse.miniMD -s 100`, 4 millions d'atomes, 20 pas) ;
- un cas court (`-s 20 -n 200`), avec 1 et 4 threads, 1 et 4 processus ;
- un cas EAM (`in.eam.miniMD -s 20 -n 100`), avec 1 et 4 processus.

Le reste de ce qu'annonce le `README.md` (options hors mesures, fichiers de données, sortie YAML) vient du compte rendu du LLM et se rejoue avec `./validate.sh`.

## Compilation

Le `Makefile` de ce dossier reprend les options principales de `baseline/ref/Makefile.openmpi`, sans `-frecord-gcc-switches` ni les macros propres à miniMD. Pour compiler avec exactement les mêmes options que miniMD :

```bash
make clean && make CXXFLAGS="-Ofast -g -march=native -fno-inline-functions -fno-omit-frame-pointer -frecord-gcc-switches -DMPICH_IGNORE_CXX_SEEK -fopenmp -DNOCHUNK -mavx -DUSE_SIMD -DPRECISION=2"
```

## Autres essais

Le même travail a d'abord été demandé à deux modèles de ROMEO, avec un prompt aux mêmes exigences : Qwen3-Coder-Next n'a rien produit (contexte limité à 32 768 tokens), Mistral Small 4 a produit un code qui ne compile pas. Ces essais ne sont pas dans le dépôt.
