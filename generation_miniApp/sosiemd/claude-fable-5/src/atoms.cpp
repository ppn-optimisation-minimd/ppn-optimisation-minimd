#include "atoms.h"

#include <cstdlib>

void Atoms::reserve(int n)
{
  if (n <= static_cast<int>(type.size())) return;
  const int capacity = n + n / 8 + 1024;  // marge pour limiter les réallocations
  x.resize(3 * static_cast<size_t>(capacity));
  v.resize(3 * static_cast<size_t>(capacity));
  f.resize(3 * static_cast<size_t>(capacity));
  type.resize(capacity);
}

void Atoms::add_local(double px, double py, double pz, double vx, double vy, double vz)
{
  reserve(nlocal + 1);
  x[3 * nlocal + 0] = px;
  x[3 * nlocal + 1] = py;
  x[3 * nlocal + 2] = pz;
  v[3 * nlocal + 0] = vx;
  v[3 * nlocal + 1] = vy;
  v[3 * nlocal + 2] = vz;
  // Même tirage que miniMD : rand() de la bibliothèque C, initialisé par
  // srand(5413) dans main(). Tous les types ont les mêmes paramètres.
  type[nlocal] = std::rand() % ntypes;
  nlocal++;
}

void Atoms::copy(int from, int to)
{
  for (int d = 0; d < 3; d++) {
    x[3 * to + d] = x[3 * from + d];
    v[3 * to + d] = v[3 * from + d];
  }
  type[to] = type[from];
}

void Atoms::pbc()
{
#pragma omp parallel for schedule(static)
  for (int i = 0; i < nlocal; i++) {
    for (int d = 0; d < 3; d++) {
      // L'ordre des deux tests garantit 0 <= x < prd même avec les arrondis.
      if (x[3 * i + d] < 0.0) x[3 * i + d] += box.prd[d];
      if (x[3 * i + d] >= box.prd[d]) x[3 * i + d] -= box.prd[d];
    }
  }
}

void Atoms::reorder(const std::vector<int>& order)
{
  x_tmp_.resize(x.size());
  v_tmp_.resize(v.size());
  type_tmp_.resize(type.size());

#pragma omp parallel for schedule(static)
  for (int k = 0; k < nlocal; k++) {
    const int old = order[k];
    for (int d = 0; d < 3; d++) {
      x_tmp_[3 * k + d] = x[3 * old + d];
      v_tmp_[3 * k + d] = v[3 * old + d];
    }
    type_tmp_[k] = type[old];
  }

  x.swap(x_tmp_);
  v.swap(v_tmp_);
  type.swap(type_tmp_);
}
