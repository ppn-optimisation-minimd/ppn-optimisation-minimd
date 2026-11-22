// Décomposition de la boîte entre processus MPI et communications entre
// sous-domaines voisins. Avec un seul processus, les mêmes routines servent à
// fabriquer les images périodiques.
#pragma once

#include <vector>

#include "atoms.h"

class Domain {
 public:
  int me = 0;      // rang de ce processus
  int nprocs = 1;
  int procgrid[3] = {1, 1, 1};  // nombre de processus par dimension

  Domain();

  // Choisit la grille de processus, fixe les bornes box.lo / box.hi de ce
  // processus et prépare les échanges de fantômes pour un rayon `cutneigh`.
  void setup(double cutneigh, Box& box);

  // Donne à leur nouveau propriétaire les atomes sortis du sous-domaine.
  // À appeler avant borders(), à chaque reconstruction des listes.
  void exchange(Atoms& atoms);

  // Reconstitue les fantômes : chaque processus envoie à ses voisins les
  // atomes proches de ses faces. Mémorise les listes d'envoi pour la suite.
  void borders(Atoms& atoms);

  // Met à jour la position des fantômes (pas de temps sans reconstruction).
  void forward_positions(Atoms& atoms);

  // Rend à leurs propriétaires les forces accumulées sur les fantômes.
  void reverse_forces(Atoms& atoms);

  // Recopie sur les fantômes une valeur par atome connue de son propriétaire.
  void forward_per_atom(std::vector<double>& values);

  // Nombre total d'atomes envoyés comme fantômes par ce processus.
  int total_sent() const;

  // Option --check_exchange : mémorise les positions, puis signale un atome
  // qui a parcouru plus que la taille du sous-domaine depuis.
  void remember_positions(const Atoms& atoms);
  void warn_if_moved_too_far(const Atoms& atoms) const;

 private:
  // Un échange de fantômes avec le voisin d'une face. Il y en a deux par
  // dimension (vers le bas, vers le haut), multipliés par `need` quand le
  // rayon dépasse la taille d'un sous-domaine.
  struct Swap {
    int dim;               // dimension de l'échange
    int sendproc;          // destinataire de mes atomes
    int recvproc;          // processus dont je reçois des fantômes
    double lo, hi;         // tranche des atomes à envoyer, le long de dim
    double shift;          // décalage périodique ajouté à leur coordonnée dim
    std::vector<int> sendlist;  // atomes envoyés
    int nrecv = 0;         // nombre de fantômes reçus
    int firstrecv = 0;     // indice du premier fantôme reçu
  };

  int procneigh_[3][2];  // voisins bas et haut dans chaque dimension
  int need_[3];          // nombre de sous-domaines à traverser par dimension
  std::vector<Swap> swaps_;
  std::vector<double> send_, recv_;  // tampons de communication
  std::vector<double> xold_;         // positions mémorisées (--check_exchange)

  void send_recv(int dest, int source);
  void forward(double* values, int width, bool with_shift);
};
