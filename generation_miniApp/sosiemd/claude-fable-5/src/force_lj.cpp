#include "force_lj.h"

#include <omp.h>

ForceLJ::ForceLJ(int ntypes, double epsilon, double sigma, double cutforce) : ntypes_(ntypes)
{
  const double sigma6 = sigma * sigma * sigma * sigma * sigma * sigma;
  cutforcesq_.assign(ntypes * ntypes, cutforce * cutforce);
  epsilon_.assign(ntypes * ntypes, epsilon);
  sigma6_.assign(ntypes * ntypes, sigma6);
}

void ForceLJ::compute(Atoms& atoms, const Neighbor& neighbor, Domain&, bool with_energy)
{
  if (neighbor.half) compute_half(atoms, neighbor, with_energy);
  else compute_full(atoms, neighbor, with_energy);
}

// Demi-listes : chaque paire (i, j) est vue une fois et sa force est appliquée
// aux deux atomes. Si la 3e loi ne s'applique pas aux fantômes (ghost_newton
// faux), une paire local-fantôme est vue par les deux processus concernés :
// chacun ne met à jour que son atome et compte la moitié de l'énergie.
void ForceLJ::compute_half(Atoms& atoms, const Neighbor& neighbor, bool with_energy)
{
  const int nlocal = atoms.nlocal;
  const int nall = atoms.nlocal + atoms.nghost;
  const int ntypes = ntypes_;
  const bool ghost_newton = neighbor.ghost_newton;
  const double* x = atoms.x.data();
  double* f = atoms.f.data();
  const int* type = atoms.type.data();
  const bool threaded = !serial && omp_get_max_threads() > 1;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < 3 * nall; i++) f[i] = 0.0;

  double energy_sum = 0.0, virial_sum = 0.0;

#pragma omp parallel for schedule(static) reduction(+ : energy_sum, virial_sum) if (threaded)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    const int type_i = type[i];
    double fix = 0.0, fiy = 0.0, fiz = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;
      const int pair = type_i * ntypes + type[j];

      if (rsq < cutforcesq_[pair]) {
        const double sr2 = 1.0 / rsq;
        const double sr6 = sr2 * sr2 * sr2 * sigma6_[pair];
        const double force = 48.0 * sr6 * (sr6 - 0.5) * sr2 * epsilon_[pair];  // -(de/dr) / r

        fix += dx * force;
        fiy += dy * force;
        fiz += dz * force;

        const bool update_j = ghost_newton || j < nlocal;
        if (update_j) {
          add_to(f[3 * j + 0], -dx * force, threaded);
          add_to(f[3 * j + 1], -dy * force, threaded);
          add_to(f[3 * j + 2], -dz * force, threaded);
        }
        if (with_energy) {
          const double weight = update_j ? 1.0 : 0.5;
          energy_sum += weight * (4.0 * sr6 * (sr6 - 1.0)) * epsilon_[pair];
          virial_sum += weight * rsq * force;
        }
      }
    }

    add_to(f[3 * i + 0], fix, threaded);
    add_to(f[3 * i + 1], fiy, threaded);
    add_to(f[3 * i + 2], fiz, threaded);
  }

  energy = energy_sum;
  virial = virial_sum;
}

// Listes complètes : chaque paire est vue deux fois, une fois par atome, et
// seule la force sur l'atome i est accumulée. Aucune écriture partagée entre
// threads.
void ForceLJ::compute_full(Atoms& atoms, const Neighbor& neighbor, bool with_energy)
{
  const int nlocal = atoms.nlocal;
  const int ntypes = ntypes_;
  const double* x = atoms.x.data();
  double* f = atoms.f.data();
  const int* type = atoms.type.data();

  double energy_sum = 0.0, virial_sum = 0.0;

#pragma omp parallel for schedule(static) reduction(+ : energy_sum, virial_sum)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    const int type_i = type[i];
    double fix = 0.0, fiy = 0.0, fiz = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;
      const int pair = type_i * ntypes + type[j];

      if (rsq < cutforcesq_[pair]) {
        const double sr2 = 1.0 / rsq;
        const double sr6 = sr2 * sr2 * sr2 * sigma6_[pair];
        const double force = 48.0 * sr6 * (sr6 - 0.5) * sr2 * epsilon_[pair];

        fix += dx * force;
        fiy += dy * force;
        fiz += dz * force;

        if (with_energy) {
          energy_sum += sr6 * (sr6 - 1.0) * epsilon_[pair];
          virial_sum += rsq * force;
        }
      }
    }

    f[3 * i + 0] = fix;
    f[3 * i + 1] = fiy;
    f[3 * i + 2] = fiz;
  }

  // Chaque paire a été comptée deux fois : facteur 4 du potentiel, divisé par 2.
  energy = 2.0 * energy_sum;
  virial = 0.5 * virial_sum;
}
