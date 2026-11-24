// Chronomètres de la boucle en temps, avec les mêmes rubriques que miniMD.
#pragma once

#include <mpi.h>

enum TimerId {
  TIME_TOTAL,  // toute la boucle en temps
  TIME_COMM,   // communications (fantômes, échanges, tri des atomes)
  TIME_FORCE,  // calcul des forces
  TIME_NEIGH,  // construction des listes de voisins
  TIME_EXTRA,  // part de TIME_COMM passée dans échange + tri + fantômes
  TIME_COUNT
};

struct Timer {
  double total[TIME_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};

  // Point de départ de la prochaine mesure.
  void start() { last_ = MPI_Wtime(); }

  // Ajoute à la rubrique `id` le temps écoulé depuis le dernier appel à
  // start() ou stamp(), et repart de maintenant.
  void stamp(TimerId id)
  {
    const double now = MPI_Wtime();
    total[id] += now - last_;
    last_ = now;
  }

 private:
  double last_ = 0.0;
};
