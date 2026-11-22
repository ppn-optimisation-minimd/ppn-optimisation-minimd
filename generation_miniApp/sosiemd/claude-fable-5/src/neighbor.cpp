#include "neighbor.h"

#include <algorithm>

#include "util.h"

namespace {
const double SMALL = 1.0e-6;   // marge relative sur l'étendue de la grille
const double FACTOR = 0.999;   // tolérance d'arrondi sur la portée du motif
}

void Neighbor::setup(const Box& box)
{
  int reach[3];  // nombre de cellules à parcourir de chaque côté
  for (int d = 0; d < 3; d++) {
    prd_[d] = box.prd[d];
    binsize_[d] = box.prd[d] / nbin[d];
    bininv_[d] = 1.0 / binsize_[d];

    // Cellules globales extrêmes où peut tomber un fantôme de ce processus,
    // plus une cellule de garde de chaque côté pour le motif.
    double coord = box.lo[d] - cutneigh - SMALL * box.prd[d];
    mbinlo_[d] = static_cast<int>(coord * bininv_[d]);
    if (coord < 0.0) mbinlo_[d] -= 1;
    coord = box.hi[d] + cutneigh + SMALL * box.prd[d];
    int mbinhi = static_cast<int>(coord * bininv_[d]);
    mbinlo_[d] -= 1;
    mbinhi += 1;
    mbin_[d] = mbinhi - mbinlo_[d] + 1;

    reach[d] = static_cast<int>(cutneigh * bininv_[d]);
    if (reach[d] * binsize_[d] < FACTOR * cutneigh) reach[d]++;
  }
  mbins_ = mbin_[0] * mbin_[1] * mbin_[2];

  // Motif : toutes les cellules dont le point le plus proche de la cellule
  // centrale est à moins de cutneigh. Avec les demi-listes et la 3e loi sur
  // les fantômes, on ne garde que la cellule centrale et la moitié
  // « supérieure » : chaque paire de cellules n'est alors vue qu'une fois.
  const bool upper_half_only = half && ghost_newton;
  const double cutneighsq = cutneigh * cutneigh;
  stencil_.clear();
  if (upper_half_only) stencil_.push_back(0);
  for (int k = upper_half_only ? 0 : -reach[2]; k <= reach[2]; k++)
    for (int j = -reach[1]; j <= reach[1]; j++)
      for (int i = -reach[0]; i <= reach[0]; i++) {
        if (upper_half_only && !(k > 0 || j > 0 || (j == 0 && i > 0))) continue;
        if (bin_distance_sq(i, j, k) < cutneighsq)
          stencil_.push_back((k * mbin_[1] + j) * mbin_[0] + i);
      }
}

// Carré de la plus petite distance entre la cellule centrale et la cellule
// décalée de (i, j, k).
double Neighbor::bin_distance_sq(int i, int j, int k) const
{
  const int offset[3] = {i, j, k};
  double sum = 0.0;
  for (int d = 0; d < 3; d++) {
    double gap = 0.0;
    if (offset[d] > 0) gap = (offset[d] - 1) * binsize_[d];
    else if (offset[d] < 0) gap = (offset[d] + 1) * binsize_[d];
    sum += gap * gap;
  }
  return sum;
}

// Indice local, dans la dimension d, de la cellule contenant la coordonnée x.
// Les fantômes peuvent avoir x < 0 ou x >= prd : les trois cas évitent
// qu'un arrondi ne les range dans une cellule de l'intérieur de la boîte.
inline int Neighbor::bin_coord(double x, int d) const
{
  if (x >= prd_[d]) return static_cast<int>((x - prd_[d]) * bininv_[d]) + nbin[d] - mbinlo_[d];
  if (x >= 0.0) return static_cast<int>(x * bininv_[d]) - mbinlo_[d];
  return static_cast<int>(x * bininv_[d]) - mbinlo_[d] - 1;
}

// Indice de la cellule contenant la position pos, ou -1 si elle est en dehors
// de la grille de ce processus.
inline int Neighbor::bin_of(const double* pos) const
{
  int b[3];
  for (int d = 0; d < 3; d++) {
    b[d] = bin_coord(pos[d], d);
    if (b[d] < 0 || b[d] >= mbin_[d]) return -1;
  }
  return (b[2] * mbin_[1] + b[1]) * mbin_[0] + b[0];
}

const std::vector<int>& Neighbor::bin_atoms(const Atoms& atoms, int count)
{
  const double* x = atoms.x.data();

  atom_bin_.resize(count);
#pragma omp parallel for schedule(static)
  for (int i = 0; i < count; i++) atom_bin_[i] = bin_of(&x[3 * i]);

  // Tri par comptage : effectif de chaque cellule, puis position de départ,
  // puis rangement des atomes dans l'ordre de leurs indices.
  bin_start_.assign(mbins_ + 1, 0);
  for (int i = 0; i < count; i++) {
    if (atom_bin_[i] < 0)
      abort_run("un atome est sorti de la grille de cellules : simulation instable (pas de temps trop grand ?)");
    bin_start_[atom_bin_[i] + 1]++;
  }
  for (int b = 0; b < mbins_; b++) bin_start_[b + 1] += bin_start_[b];

  bin_list_.resize(count);
  std::vector<int> next(bin_start_.begin(), bin_start_.end() - 1);
  for (int i = 0; i < count; i++) bin_list_[next[atom_bin_[i]]++] = i;

  return bin_list_;
}

void Neighbor::build(const Atoms& atoms)
{
  const int nlocal = atoms.nlocal;
  const double* x = atoms.x.data();
  const double cutneighsq = cutneigh * cutneigh;
  const int nstencil = static_cast<int>(stencil_.size());

  bin_atoms(atoms, nlocal + atoms.nghost);
  numneigh.resize(nlocal);

  // Si un atome a plus de maxneighs voisins, on agrandit et on recommence.
  for (bool complete = false; !complete;) {
    const size_t needed = static_cast<size_t>(nlocal) * maxneighs;
    if (needed > neighbors_capacity_) {
      // Un peu de marge : le nombre d'atomes locaux fluctue avec MPI.
      neighbors_capacity_ = needed + needed / 20;
      neighbors.reset();
      neighbors.reset(new int[neighbors_capacity_]);
    }
    int most = 0;  // plus grand nombre de voisins rencontré

#pragma omp parallel for schedule(static) reduction(max : most)
    for (int i = 0; i < nlocal; i++) {
      int* list = &neighbors[static_cast<size_t>(i) * maxneighs];
      const double xi = x[3 * i + 0];
      const double yi = x[3 * i + 1];
      const double zi = x[3 * i + 2];
      const int ibin = atom_bin_[i];
      int n = 0;

      for (int s = 0; s < nstencil; s++) {
        const int jbin = ibin + stencil_[s];
        for (int m = bin_start_[jbin]; m < bin_start_[jbin + 1]; m++) {
          const int j = bin_list_[m];

          if (half && !ghost_newton && j < i) continue;  // paire rangée chez j
          if (jbin == ibin) {
            if (j == i) continue;
            if (half && ghost_newton) {
              if (j < i) continue;
              // Fantôme de la même cellule : la paire revient à celui des
              // deux atomes qui est « en dessous » (z, puis y, puis x).
              if (j >= nlocal) {
                const double xj = x[3 * j + 0], yj = x[3 * j + 1], zj = x[3 * j + 2];
                if (zj < zi || (zj == zi && (yj < yi || (yj == yi && xj < xi)))) continue;
              }
            }
          }

          const double dx = xi - x[3 * j + 0];
          const double dy = yi - x[3 * j + 1];
          const double dz = zi - x[3 * j + 2];
          if (dx * dx + dy * dy + dz * dz <= cutneighsq) {
            if (n < maxneighs) list[n] = j;
            n++;
          }
        }
      }
      numneigh[i] = n;
      most = std::max(most, n);
    }

    complete = most <= maxneighs;
    if (!complete) maxneighs = static_cast<int>(most * 1.2);
  }
}
