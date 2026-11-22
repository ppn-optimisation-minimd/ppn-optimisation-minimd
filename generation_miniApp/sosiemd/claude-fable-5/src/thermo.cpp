#include "thermo.h"

#include <mpi.h>

#include <cstdio>

void Thermo::setup(Units units, const Atoms& atoms)
{
  const Box& box = atoms.box;
  if (units == Units::LJ) {
    mvv2e = 1.0;
    dof_boltz = atoms.natoms * 3 - 3;
    p_scale = 1.0 / 3 / box.prd[0] / box.prd[1] / box.prd[2];
    energy_scale = 1.0;
  } else {
    mvv2e = 1.036427e-04;                                 // (g/mol)(A/ps)^2 -> eV
    dof_boltz = (atoms.natoms * 3 - 3) * 8.617343e-05;    // k_B en eV/K
    p_scale = 1.602176e+06 / 3 / box.prd[0] / box.prd[1] / box.prd[2];  // eV/A^3 -> bar
    // Facteur de miniMD en unités metal (2 x 524287.985533). Il n'a pas de
    // sens physique : l'énergie EAM affichée n'est pas en eV par atome. Il est
    // repris tel quel pour afficher les mêmes nombres que miniMD.
    energy_scale = 2.0 * 524287.985533;
  }
  t_scale = mvv2e / dof_boltz;
}

double Thermo::temperature(const Atoms& atoms) const
{
  const double* v = atoms.v.data();
  const int nlocal = atoms.nlocal;
  double sum = 0.0;  // somme des m v^2 de mes atomes

#pragma omp parallel for schedule(static) reduction(+ : sum)
  for (int i = 0; i < nlocal; i++)
    sum += (v[3 * i] * v[3 * i] + v[3 * i + 1] * v[3 * i + 1] + v[3 * i + 2] * v[3 * i + 2]) * atoms.mass;

  double total = 0.0;
  MPI_Allreduce(&sum, &total, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  return total * t_scale;
}

void Thermo::report(int step, const Atoms& atoms, const Force& force, double elapsed)
{
  const double t = temperature(atoms);

  double local[2] = {force.energy, force.virial};
  double total[2];
  MPI_Allreduce(local, total, 2, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  const double u = total[0] * energy_scale / atoms.natoms;
  // Pression du viriel : (N k_B T + somme des r.f / 3) / V, les degrés de
  // liberté étant comptés comme pour la température.
  const double p = (t * dof_boltz + total[1]) * p_scale;

  if (step == 0) {
    steps.clear();
    temperatures.clear();
    energies.clear();
    pressures.clear();
  }
  steps.push_back(step);
  temperatures.push_back(t);
  energies.push_back(u);
  pressures.push_back(p);

  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) std::printf("%i %e %e %e %6.3lf\n", step, t, u, p, elapsed);
}
