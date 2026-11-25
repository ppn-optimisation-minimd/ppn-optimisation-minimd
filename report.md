# Rapport projet PPN MiniMD

*Todo*

## Evaluation des performances séquentielles sosieMD

Afin de comparer les performances séquentielles de miniMD (fork `optim-group`)
à sosieMD (entièrement généré par IA), nous avons analysé le temps maximum
d'exécution des deux fonctions les plus énergivores selon le benchmark MAQAO.
Ces trois fonctions constituent à elles seules au moins 40% du temps
d'exécution de tout le programme dans les deux benchmarks.

|                         | miniMD | sosieMD |
|-------------------------|--------|---------|
| `ForceLJ::compute_half` | 15.21s | 19.97s  |
| `Neighbor::build`       | 5.27s  | 5.22s   |

La fonction `ForceLJ::compute_half` semble consommer 4.75s de plus dans
l'implémentation de sosieMD. De plus, elle semble occuper **72.76%** du temps
d'exécution, comparé à 33.10% dans la version miniMD

### `memcpy`

Nous avons remarqué que `memcpy` (la fonction de la bibliothèque standard C
qui permet de copier de la mémoire d'une région à une autre) occupe au maximum
**5s** sur un thread dans l'implémentation miniMD, comparé à 200ms dans la
version sosieMD. `memcpy` doit être la routine la plus performante pour copier
des octets, cette mesure peut donc suggérer que miniMD doit régulièrement
copier de la mémoire afin de réduire du temps de calcul, contrairement à
sosieMD.
