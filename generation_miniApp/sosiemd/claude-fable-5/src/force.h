// Interface commune aux potentiels.
#pragma once

#include "atoms.h"
#include "domain.h"
#include "neighbor.h"

class Force {
 public:
  // Résultats du dernier compute(..., with_energy = true), pour ce processus.
  // Une paire à cheval sur deux processus compte pour moitié chez chacun.
  double energy = 0.0;  // énergie potentielle
  double virial = 0.0;  // somme sur les paires de r . f

  double mass = 1.0;    // masse atomique fixée par le potentiel (EAM)

  // Vrai avec --half_neigh -1 : les forces sont calculées sans threads.
  bool serial = false;

  virtual ~Force() = default;

  // Calcule les forces sur les atomes locaux (et sur les fantômes avec les
  // demi-listes et la 3e loi sur les fantômes) ; l'énergie et le viriel ne
  // sont calculés que si with_energy est vrai.
  virtual void compute(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy) = 0;
};

// Ajoute `value` à `target`. Quand `shared` est vrai, un autre thread peut
// modifier la même case au même moment : l'ajout est alors atomique.
inline void add_to(double& target, double value, bool shared)
{
  if (shared) {
#pragma omp atomic
    target += value;
  } else {
    target += value;
  }
}
