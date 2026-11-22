// Potentiel EAM (embedded atom method) pour un métal pur, lu dans un fichier
// au format « funcfl » de DYNAMO / LAMMPS (ici Cu_u6.eam).
//
// Énergie : E = somme_i F(rho_i) + somme_paires phi(r_ij), où
//   rho_i = somme_j rho(r_ij) est la densité électronique vue par l'atome i,
//   F est l'énergie d'immersion et phi(r) = z2(r) / r la répulsion de paire.
#pragma once

#include <string>
#include <vector>

#include "force.h"

// Fonction tabulée à pas constant et interpolée par morceaux cubiques.
// Le point m (1 <= m <= n) a pour abscisse (m - 1) * delta.
struct SplineTable {
  int n = 0;
  double inv_delta = 0.0;
  std::vector<double> coeff;  // 7 coefficients par point, voir build()

  // values[m - 1] est la valeur au point m.
  void build(const std::vector<double>& values, double delta);

  // Repère l'abscisse x : intervalle m et fraction p (entre 0 et 1) dans cet
  // intervalle. Hors de la table, on reste sur le premier ou le dernier.
  void locate(double x, int& m, double& p) const
  {
    p = x * inv_delta + 1.0;
    m = static_cast<int>(p);
    if (m > n - 1) m = n - 1;
    if (m < 1) m = 1;
    p -= m;
    if (p > 1.0) p = 1.0;
  }

  double value(int m, double p) const
  {
    const double* c = &coeff[7 * m];
    return ((c[3] * p + c[4]) * p + c[5]) * p + c[6];
  }

  double derivative(int m, double p) const
  {
    const double* c = &coeff[7 * m];
    return (c[0] * p + c[1]) * p + c[2];
  }
};

class ForceEAM : public Force {
 public:
  // Lit le potentiel et prépare les tables. Fixe `mass`.
  explicit ForceEAM(const std::string& potential_file);

  void compute(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy) override;

 private:
  double cutforcesq_ = 0.0;  // carré du rayon de coupure, lu dans le fichier
  SplineTable frho_;         // F(rho)
  SplineTable rhor_;         // rho(r)
  SplineTable z2r_;          // z2(r) = r * phi(r)

  std::vector<double> rho_;  // densité vue par chaque atome local
  std::vector<double> fp_;   // F'(rho) de chaque atome, fantômes compris

  void compute_half(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy);
  void compute_full(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy);
};
