#include "integrate.h"

#include <mpi.h>

namespace {

// Premier demi-pas : vitesses d'un demi-pas, puis positions d'un pas entier.
void initial_integrate(Atoms& atoms, double dt, double dtforce)
{
  double* x = atoms.x.data();
  double* v = atoms.v.data();
  const double* f = atoms.f.data();
  const int n = 3 * atoms.nlocal;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < n; i++) {
    v[i] += dtforce * f[i];
    x[i] += dt * v[i];
  }
}

// Second demi-pas : vitesses avec les nouvelles forces.
void final_integrate(Atoms& atoms, double dtforce)
{
  double* v = atoms.v.data();
  const double* f = atoms.f.data();
  const int n = 3 * atoms.nlocal;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < n; i++) v[i] += dtforce * f[i];
}

}  // namespace

void run(const Config& cfg, Atoms& atoms, Force& force, Neighbor& neighbor, Domain& domain, Thermo& thermo,
         Timer& timer)
{
  // Demi-pas de temps divisé par la masse, avec la conversion d'unités :
  // un demi-pas change la vitesse de dtforce * f.
  const double dtforce = 0.5 * cfg.dt / thermo.mvv2e / atoms.mass;
  const bool reverse_needed = neighbor.half && neighbor.ghost_newton;
  int next_sort = cfg.sort_every > 0 ? cfg.sort_every : cfg.nsteps + 1;

  MPI_Barrier(MPI_COMM_WORLD);  // tous les processus partent ensemble
  const double start = MPI_Wtime();

  for (int step = 1; step <= cfg.nsteps; step++) {
    initial_integrate(atoms, cfg.dt, dtforce);
    timer.start();

    if (step % neighbor.every != 0) {
      // Les listes de voisins restent valables : il suffit de déplacer les fantômes.
      domain.forward_positions(atoms);
      timer.stamp(TIME_COMM);
    } else {
      // Reconstruction : changement de propriétaire des atomes sortis, tri
      // éventuel, nouveaux fantômes, nouvelles listes.
      if (cfg.check_exchange) domain.warn_if_moved_too_far(atoms);
      const double extra_start = MPI_Wtime();
      domain.exchange(atoms);
      if (step >= next_sort) {
        atoms.reorder(neighbor.bin_atoms(atoms, atoms.nlocal));
        next_sort += cfg.sort_every;
      }
      domain.borders(atoms);
      timer.total[TIME_EXTRA] += MPI_Wtime() - extra_start;
      timer.stamp(TIME_COMM);
      if (cfg.check_exchange) domain.remember_positions(atoms);

      neighbor.build(atoms);
      timer.stamp(TIME_NEIGH);
    }

    const bool thermo_step = cfg.thermo_every > 0 && step % cfg.thermo_every == 0;
    force.compute(atoms, neighbor, domain, thermo_step);
    timer.stamp(TIME_FORCE);

    if (reverse_needed) {
      domain.reverse_forces(atoms);
      timer.stamp(TIME_COMM);
    }

    final_integrate(atoms, dtforce);

    if (thermo_step) thermo.report(step, atoms, force, MPI_Wtime() - start);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  timer.total[TIME_TOTAL] = MPI_Wtime() - start;
}
