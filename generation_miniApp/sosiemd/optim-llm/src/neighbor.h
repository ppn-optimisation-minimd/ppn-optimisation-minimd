// Listes de voisins : pour chaque atome local, les atomes situés à moins de
// `cutneigh`. Elles sont construites à l'aide d'une grille de cellules et
// réutilisées pendant `every` pas de temps.
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "atoms.h"

class Neighbor {
 public:
  // --- Réglages, à fixer avant setup() ---
  int every = 20;             // reconstruction tous les `every` pas
  double cutneigh = 0.0;      // rayon des listes (coupure + peau)
  int nbin[3] = {1, 1, 1};    // nombre de cellules dans la boîte entière
  bool half = true;           // demi-listes : chaque paire n'est rangée qu'une fois
  bool ghost_newton = true;   // demi-listes : une paire local-fantôme n'est rangée
                              // que par un seul des deux processus concernés

  // --- Listes ---
  // Les voisins de l'atome i sont neighbors[i*maxneighs + k], k < numneigh[i].
  int maxneighs = 100;
  std::vector<int> numneigh;
  std::unique_ptr<int[]> neighbors;

  const int* neighbors_of(int i) const { return &neighbors[static_cast<size_t>(i) * maxneighs]; }

  // Calcule la grille de cellules de ce processus et le motif de cellules à
  // visiter autour d'un atome.
  void setup(const Box& box);

  // Reconstruit les listes des atomes locaux.
  void build(const Atoms& atoms);

  // Range les `count` premiers atomes dans les cellules et renvoie leur liste
  // triée par cellule (par indice croissant dans une même cellule). Appelée
  // avec count = nlocal, cette liste est l'ordre utilisé pour trier les atomes.
  const std::vector<int>& bin_atoms(const Atoms& atoms, int count);

 private:
  // Grille : nbin cellules de taille binsize couvrent la boîte entière ; ce
  // processus en garde mbin par dimension, à partir de la cellule globale
  // mbinlo, de quoi contenir son sous-domaine et ses fantômes.
  double prd_[3];
  double binsize_[3], bininv_[3];
  int mbin_[3], mbinlo_[3];
  int mbins_ = 0;
  std::vector<int> stencil_;    // décalages d'indice de cellule à visiter

  std::vector<int> atom_bin_;   // cellule de chaque atome
  std::vector<int> bin_start_;  // début de chaque cellule dans bin_list_
  std::vector<int> bin_list_;   // atomes triés par cellule
  size_t neighbors_capacity_ = 0;

  int bin_coord(double x, int d) const;
  int bin_of(const double* pos) const;
  double bin_distance_sq(int i, int j, int k) const;
};
