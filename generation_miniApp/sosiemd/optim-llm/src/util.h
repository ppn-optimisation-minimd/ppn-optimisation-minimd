// Petits outils communs : arrêts sur erreur.
#pragma once

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <string>

// Arrêt sur une erreur que tous les processus détectent de la même façon
// (fichier d'entrée, ligne de commande...). Le processus 0 affiche le message,
// puis chacun termine MPI proprement et sort avec un code non nul.
[[noreturn]] inline void fatal(const std::string& message)
{
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) std::fprintf(stderr, "ERROR: %s\n", message.c_str());
  MPI_Finalize();
  std::exit(1);
}

// Arrêt sur une erreur qu'un seul processus peut rencontrer en cours de
// calcul : il affiche le message et interrompt toute l'exécution.
[[noreturn]] inline void abort_run(const std::string& message)
{
  std::fprintf(stderr, "ERROR: %s\n", message.c_str());
  MPI_Abort(MPI_COMM_WORLD, 1);
  std::exit(1);
}
