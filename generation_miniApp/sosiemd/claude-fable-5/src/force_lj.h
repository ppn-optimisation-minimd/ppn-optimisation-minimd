// Potentiel de Lennard-Jones tronqué : e(r) = 4 eps [(sigma/r)^12 - (sigma/r)^6].
#pragma once

#include <vector>

#include "force.h"

class ForceLJ : public Force {
 public:
  // Tous les types d'atomes reçoivent les mêmes paramètres, comme dans miniMD.
  ForceLJ(int ntypes, double epsilon, double sigma, double cutforce);

  void compute(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy) override;

 private:
  int ntypes_;
  // Paramètres de la paire de types (a, b), rangés à l'indice a*ntypes + b.
  std::vector<double> cutforcesq_, epsilon_, sigma6_;

  void compute_half(Atoms& atoms, const Neighbor& neighbor, bool with_energy);
  void compute_full(Atoms& atoms, const Neighbor& neighbor, bool with_energy);
};
