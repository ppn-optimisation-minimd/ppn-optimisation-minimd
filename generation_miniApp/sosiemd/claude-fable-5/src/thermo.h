// Grandeurs thermodynamiques affichées : température T, énergie potentielle
// par atome U et pression P, avec les conventions et les unités de miniMD.
#pragma once

#include <vector>

#include "atoms.h"
#include "config.h"
#include "force.h"

class Thermo {
 public:
  // Constantes de conversion, fixées par setup() selon le système d'unités.
  double mvv2e = 1.0;         // m v^2 -> énergie
  double dof_boltz = 1.0;     // (3N - 3) degrés de liberté, fois k_B
  double t_scale = 1.0;       // somme des m v^2 -> température
  double p_scale = 1.0;       // énergie -> pression
  double energy_scale = 1.0;  // énergie potentielle -> valeur affichée

  // Historique des valeurs affichées, repris dans le fichier YAML.
  std::vector<int> steps;
  std::vector<double> temperatures, energies, pressures;

  void setup(Units units, const Atoms& atoms);

  // Température instantanée de l'ensemble du système.
  double temperature(const Atoms& atoms) const;

  // Calcule T, U et P au pas `step`, les mémorise et les affiche. Les forces
  // doivent venir d'être calculées avec with_energy = true.
  void report(int step, const Atoms& atoms, const Force& force, double elapsed);
};
