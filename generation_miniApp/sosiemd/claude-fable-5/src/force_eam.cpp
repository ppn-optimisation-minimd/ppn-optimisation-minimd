#include "force_eam.h"

#include <omp.h>

#include <cmath>
#include <fstream>
#include <sstream>

#include "util.h"

void SplineTable::build(const std::vector<double>& values, double delta)
{
  n = static_cast<int>(values.size());
  inv_delta = 1.0 / delta;
  coeff.assign(7 * (n + 1), 0.0);
  auto c = [&](int m, int k) -> double& { return coeff[7 * m + k]; };

  // c6 : valeur au point m.
  for (int m = 1; m <= n; m++) c(m, 6) = values[m - 1];

  // c5 : pente au point m (par pas de table), différences finies centrées
  // d'ordre 4 à l'intérieur, d'ordre inférieur près des bords.
  c(1, 5) = c(2, 6) - c(1, 6);
  c(2, 5) = 0.5 * (c(3, 6) - c(1, 6));
  c(n - 1, 5) = 0.5 * (c(n, 6) - c(n - 2, 6));
  c(n, 5) = c(n, 6) - c(n - 1, 6);
  for (int m = 3; m <= n - 2; m++)
    c(m, 5) = ((c(m - 2, 6) - c(m + 2, 6)) + 8.0 * (c(m + 1, 6) - c(m - 1, 6))) / 12.0;

  // c3, c4 : termes cubique et quadratique du polynôme qui raccorde valeurs
  // et pentes entre les points m et m + 1 :
  //   f(p) = ((c3 p + c4) p + c5) p + c6,  0 <= p <= 1.
  for (int m = 1; m <= n - 1; m++) {
    const double step = c(m + 1, 6) - c(m, 6);
    c(m, 4) = 3.0 * step - 2.0 * c(m, 5) - c(m + 1, 5);
    c(m, 3) = c(m, 5) + c(m + 1, 5) - 2.0 * step;
  }

  // c0, c1, c2 : dérivée par rapport à l'abscisse, f'(p) = (c0 p + c1) p + c2.
  for (int m = 1; m <= n; m++) {
    c(m, 2) = c(m, 5) / delta;
    c(m, 1) = 2.0 * c(m, 4) / delta;
    c(m, 0) = 3.0 * c(m, 3) / delta;
  }
}

ForceEAM::ForceEAM(const std::string& potential_file)
{
  std::ifstream file(potential_file);
  if (!file) fatal("Can't open EAM Potential file: " + potential_file);

  // Ligne 1 : commentaire. Ligne 2 : numéro atomique, masse, ... Ligne 3 :
  // nombre et pas des points en rho, nombre et pas des points en r, coupure.
  std::string line;
  int atomic_number = 0, nrho = 0, nr = 0;
  double drho = 0.0, dr = 0.0, cut = 0.0;
  std::getline(file, line);
  std::getline(file, line);
  std::istringstream(line) >> atomic_number >> mass;
  std::getline(file, line);
  std::istringstream(line) >> nrho >> drho >> nr >> dr >> cut;
  if (nrho < 6 || nr < 6 || drho <= 0.0 || dr <= 0.0 || cut <= 0.0)
    fatal(potential_file + " : en-tete de potentiel EAM invalide");

  // Puis trois tableaux à la suite : F(rho), z(r) et rho(r).
  auto read_values = [&](int count) {
    std::vector<double> values(count);
    for (double& value : values) file >> value;
    if (file.fail()) fatal(potential_file + " : tables de potentiel EAM incompletes");
    return values;
  };
  std::vector<double> frho = read_values(nrho);
  std::vector<double> zr = read_values(nr);
  std::vector<double> rhor = read_values(nr);

  // Comme miniMD (et LAMMPS pour ce format), les tables s'arrêtent à
  // l'avant-dernier point du fichier : le dernier n'est pas utilisé.
  frho.resize(nrho - 1);
  rhor.resize(nr - 1);
  zr.resize(nr - 1);

  // z(r) est une charge effective : z^2 / r en unités atomiques (Hartree,
  // Bohr), converti en eV et en Angström. On tabule z2 = r * phi.
  std::vector<double> z2r(zr.size());
  for (size_t m = 0; m < zr.size(); m++) z2r[m] = 27.2 * 0.529 * zr[m] * zr[m];

  frho_.build(frho, drho);
  rhor_.build(rhor, dr);
  z2r_.build(z2r, dr);
  cutforcesq_ = cut * cut;
}

void ForceEAM::compute(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy)
{
  fp_.resize(atoms.nlocal + atoms.nghost);
  if (neighbor.half) compute_half(atoms, neighbor, domain, with_energy);
  else compute_full(atoms, neighbor, domain, with_energy);
}

// Demi-listes, sans 3e loi sur les fantômes (imposé pour EAM) : une paire
// local-local est vue une fois et sert aux deux atomes ; une paire
// local-fantôme est vue par chacun des deux propriétaires, qui ne met à jour
// que son atome et compte la moitié de l'énergie.
void ForceEAM::compute_half(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy)
{
  const int nlocal = atoms.nlocal;
  const int nall = atoms.nlocal + atoms.nghost;
  const double* x = atoms.x.data();
  double* f = atoms.f.data();
  double* fp = fp_.data();
  const bool threaded = !serial && omp_get_max_threads() > 1;

  rho_.assign(nlocal, 0.0);
  double* rho = rho_.data();

  // 1. Densité électronique vue par chaque atome.
#pragma omp parallel for schedule(static) if (threaded)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    double rho_i = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;

      if (rsq < cutforcesq_) {
        int m;
        double p;
        rhor_.locate(std::sqrt(rsq), m, p);
        const double density = rhor_.value(m, p);
        rho_i += density;
        if (j < nlocal) add_to(rho[j], density, threaded);
      }
    }
    add_to(rho[i], rho_i, threaded);
  }

  // 2. Énergie d'immersion F(rho) et sa dérivée, qui sert aux forces.
  double energy_sum = 0.0, virial_sum = 0.0;

#pragma omp parallel for schedule(static) reduction(+ : energy_sum) if (threaded)
  for (int i = 0; i < nlocal; i++) {
    int m;
    double p;
    frho_.locate(rho[i], m, p);
    fp[i] = frho_.derivative(m, p);
    if (with_energy) energy_sum += frho_.value(m, p);
  }

  // 3. Les forces sur mes atomes dépendent de F' chez leurs voisins fantômes.
  domain.forward_per_atom(fp_);

  // 4. Forces. Pour la paire (i, j) :
  //    dE/dr = (F'_i + F'_j) rho'(r) + phi'(r),  avec phi = z2 / r.
#pragma omp parallel for schedule(static)
  for (int i = 0; i < 3 * nall; i++) f[i] = 0.0;

#pragma omp parallel for schedule(static) reduction(+ : energy_sum, virial_sum) if (threaded)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    double fix = 0.0, fiy = 0.0, fiz = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;

      if (rsq < cutforcesq_) {
        const double r = std::sqrt(rsq);
        int m;
        double p;
        rhor_.locate(r, m, p);
        const double rho_prime = rhor_.derivative(m, p);
        const double z2_prime = z2r_.derivative(m, p);
        const double z2 = z2r_.value(m, p);

        const double recip = 1.0 / r;
        const double phi = z2 * recip;
        const double phi_prime = z2_prime * recip - phi * recip;
        const double energy_prime = fp[i] * rho_prime + fp[j] * rho_prime + phi_prime;
        const double force = -energy_prime * recip;  // -(dE/dr) / r

        fix += dx * force;
        fiy += dy * force;
        fiz += dz * force;

        const bool local_j = j < nlocal;
        if (local_j) {
          add_to(f[3 * j + 0], -dx * force, threaded);
          add_to(f[3 * j + 1], -dy * force, threaded);
          add_to(f[3 * j + 2], -dz * force, threaded);
        }
        if (with_energy) {
          const double weight = local_j ? 1.0 : 0.5;
          energy_sum += weight * phi;
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

// Listes complètes : chaque atome parcourt tous ses voisins et ne met à jour
// que lui-même ; chaque paire compte pour moitié à chaque passage.
void ForceEAM::compute_full(Atoms& atoms, const Neighbor& neighbor, Domain& domain, bool with_energy)
{
  const int nlocal = atoms.nlocal;
  const double* x = atoms.x.data();
  double* f = atoms.f.data();
  double* fp = fp_.data();

  double energy_sum = 0.0, virial_sum = 0.0;

  // 1 et 2. Densité vue par chaque atome, énergie d'immersion et dérivée.
#pragma omp parallel for schedule(static) reduction(+ : energy_sum)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    double rho_i = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;

      if (rsq < cutforcesq_) {
        int m;
        double p;
        rhor_.locate(std::sqrt(rsq), m, p);
        rho_i += rhor_.value(m, p);
      }
    }

    int m;
    double p;
    frho_.locate(rho_i, m, p);
    fp[i] = frho_.derivative(m, p);
    if (with_energy) energy_sum += frho_.value(m, p);
  }

  // 3. F' des voisins fantômes.
  domain.forward_per_atom(fp_);

  // 4. Forces.
#pragma omp parallel for schedule(static) reduction(+ : energy_sum, virial_sum)
  for (int i = 0; i < nlocal; i++) {
    const int* neighs = neighbor.neighbors_of(i);
    const int numneigh = neighbor.numneigh[i];
    const double xi = x[3 * i + 0];
    const double yi = x[3 * i + 1];
    const double zi = x[3 * i + 2];
    double fix = 0.0, fiy = 0.0, fiz = 0.0;

    for (int k = 0; k < numneigh; k++) {
      const int j = neighs[k];
      const double dx = xi - x[3 * j + 0];
      const double dy = yi - x[3 * j + 1];
      const double dz = zi - x[3 * j + 2];
      const double rsq = dx * dx + dy * dy + dz * dz;

      if (rsq < cutforcesq_) {
        const double r = std::sqrt(rsq);
        int m;
        double p;
        rhor_.locate(r, m, p);
        const double rho_prime = rhor_.derivative(m, p);
        const double z2_prime = z2r_.derivative(m, p);
        const double z2 = z2r_.value(m, p);

        const double recip = 1.0 / r;
        const double phi = z2 * recip;
        const double phi_prime = z2_prime * recip - phi * recip;
        const double energy_prime = fp[i] * rho_prime + fp[j] * rho_prime + phi_prime;
        const double force = -energy_prime * recip;

        fix += dx * force;
        fiy += dy * force;
        fiz += dz * force;

        if (with_energy) {
          energy_sum += 0.5 * phi;
          virial_sum += 0.5 * rsq * force;
        }
      }
    }

    f[3 * i + 0] = fix;
    f[3 * i + 1] = fiy;
    f[3 * i + 2] = fiz;
  }

  energy = energy_sum;
  virial = virial_sum;
}
