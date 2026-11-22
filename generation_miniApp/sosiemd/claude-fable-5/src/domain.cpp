#include "domain.h"

#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
const int EXCHANGE_WIDTH = 7;  // x, v et type d'un atome qui change de processus
const int BORDER_WIDTH = 4;    // x et type d'un fantôme
}

Domain::Domain()
{
  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
}

void Domain::setup(double cutneigh, Box& box)
{
  // Parmi les factorisations px*py*pz = nprocs, garder celle qui minimise la
  // surface d'un sous-domaine, donc le volume de fantômes à communiquer.
  const double area[3] = {box.prd[0] * box.prd[1], box.prd[0] * box.prd[2], box.prd[1] * box.prd[2]};
  double best = 2.0 * (area[0] + area[1] + area[2]);
  for (int px = 1; px <= nprocs; px++) {
    if (nprocs % px) continue;
    for (int py = 1; py <= nprocs / px; py++) {
      if ((nprocs / px) % py) continue;
      const int pz = nprocs / px / py;
      const double surface = area[0] / px / py + area[1] / px / pz + area[2] / py / pz;
      if (surface < best) {
        best = surface;
        procgrid[0] = px;
        procgrid[1] = py;
        procgrid[2] = pz;
      }
    }
  }

  // Grille cartésienne périodique : position de ce processus et voisins.
  int periods[3] = {1, 1, 1};
  int myloc[3];
  MPI_Comm cart;
  MPI_Cart_create(MPI_COMM_WORLD, 3, procgrid, periods, 0, &cart);
  MPI_Cart_get(cart, 3, procgrid, periods, myloc);
  for (int d = 0; d < 3; d++) MPI_Cart_shift(cart, d, 1, &procneigh_[d][0], &procneigh_[d][1]);
  MPI_Comm_free(&cart);

  for (int d = 0; d < 3; d++) {
    box.lo[d] = myloc[d] * box.prd[d] / procgrid[d];
    box.hi[d] = (myloc[d] + 1) * box.prd[d] / procgrid[d];
    need_[d] = static_cast<int>(cutneigh * procgrid[d] / box.prd[d] + 1);
  }

  // Dans chaque dimension, on envoie alternativement vers le bas la tranche
  // basse du sous-domaine, puis vers le haut la tranche haute. Si need > 1,
  // les échanges suivants font suivre les fantômes reçus d'un cran plus loin :
  // leur tranche est alors celle du sous-domaine d'origine (numéro `origin`).
  swaps_.clear();
  for (int d = 0; d < 3; d++) {
    for (int n = 0; n < 2 * need_[d]; n++) {
      Swap swap;
      swap.dim = d;
      swap.shift = 0.0;
      if (n % 2 == 0) {
        const int origin = myloc[d] + n / 2;
        swap.sendproc = procneigh_[d][0];
        swap.recvproc = procneigh_[d][1];
        swap.lo = origin * box.prd[d] / procgrid[d];
        swap.hi = std::min(box.lo[d] + cutneigh, (origin + 1) * box.prd[d] / procgrid[d]);
        if (myloc[d] == 0) swap.shift = box.prd[d];  // traverse le bord bas de la boîte
      } else {
        const int origin = myloc[d] - n / 2;
        swap.sendproc = procneigh_[d][1];
        swap.recvproc = procneigh_[d][0];
        swap.lo = std::max(box.hi[d] - cutneigh, origin * box.prd[d] / procgrid[d]);
        swap.hi = (origin + 1) * box.prd[d] / procgrid[d];
        if (myloc[d] == procgrid[d] - 1) swap.shift = -box.prd[d];  // traverse le bord haut
      }
      swaps_.push_back(swap);
    }
  }
}

// Envoie le tampon send_ à `dest` et reçoit dans recv_ ce qu'envoie `source`,
// quelle qu'en soit la taille.
void Domain::send_recv(int dest, int source)
{
  int nsend = static_cast<int>(send_.size());
  int nrecv = 0;
  MPI_Sendrecv(&nsend, 1, MPI_INT, dest, 0, &nrecv, 1, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  recv_.resize(nrecv);
  MPI_Sendrecv(send_.data(), nsend, MPI_DOUBLE, dest, 0, recv_.data(), nrecv, MPI_DOUBLE, source, 0,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void Domain::exchange(Atoms& atoms)
{
  atoms.pbc();

  std::vector<int> leaving;
  std::vector<char> is_leaving;

  for (int d = 0; d < 3; d++) {
    if (procgrid[d] == 1) continue;  // un seul processus : pbc() a suffi
    const double lo = atoms.box.lo[d];
    const double hi = atoms.box.hi[d];
    const int nlocal = atoms.nlocal;

    // Atomes sortis du sous-domaine dans cette dimension.
    leaving.clear();
    is_leaving.assign(nlocal, 0);
    for (int i = 0; i < nlocal; i++) {
      const double coord = atoms.x[3 * i + d];
      if (coord < lo || coord >= hi) {
        leaving.push_back(i);
        is_leaving[i] = 1;
      }
    }

    // Les copier dans le tampon et boucher les trous qu'ils laissent parmi
    // les `nkeep` premières cases avec les atomes restants de la fin du
    // tableau, pris dans l'ordre.
    const int nkeep = nlocal - static_cast<int>(leaving.size());
    send_.resize(EXCHANGE_WIDTH * leaving.size());
    int tail = nkeep;
    for (size_t k = 0; k < leaving.size(); k++) {
      const int i = leaving[k];
      double* buf = &send_[EXCHANGE_WIDTH * k];
      for (int c = 0; c < 3; c++) {
        buf[c] = atoms.x[3 * i + c];
        buf[3 + c] = atoms.v[3 * i + c];
      }
      buf[6] = atoms.type[i];
      if (i < nkeep) {
        while (is_leaving[tail]) tail++;
        atoms.copy(tail++, i);
      }
    }
    atoms.nlocal = nkeep;

    // Le même tampon part chez les deux voisins : chacun n'en retient que les
    // atomes qui tombent chez lui. Avec deux processus dans la dimension, les
    // deux voisins sont le même processus et un seul envoi suffit.
    const int nsides = procgrid[d] > 2 ? 2 : 1;
    for (int side = 0; side < nsides; side++) {
      send_recv(procneigh_[d][side], procneigh_[d][1 - side]);
      const int nrecv = static_cast<int>(recv_.size()) / EXCHANGE_WIDTH;
      for (int k = 0; k < nrecv; k++) {
        const double* buf = &recv_[EXCHANGE_WIDTH * k];
        if (buf[d] < lo || buf[d] >= hi) continue;
        const int i = atoms.nlocal++;
        atoms.reserve(atoms.nlocal);
        for (int c = 0; c < 3; c++) {
          atoms.x[3 * i + c] = buf[c];
          atoms.v[3 * i + c] = buf[3 + c];
        }
        atoms.type[i] = static_cast<int>(buf[6]);
      }
    }
  }
}

void Domain::borders(Atoms& atoms)
{
  atoms.nghost = 0;

  int iswap = 0;
  for (int d = 0; d < 3; d++) {
    int first = 0, last = 0;  // atomes candidats à l'envoi : [first, last)
    for (int n = 0; n < 2 * need_[d]; n++) {
      Swap& swap = swaps_[iswap++];

      // Premier aller-retour dans une dimension : les candidats sont les
      // atomes locaux et les fantômes des dimensions déjà traitées. Aux
      // suivants (need > 1) : seulement les fantômes arrivés entre-temps,
      // qu'il faut faire suivre un sous-domaine plus loin.
      if (n % 2 == 0) {
        first = last;
        last = atoms.nlocal + atoms.nghost;
      }

      swap.sendlist.clear();
      for (int i = first; i < last; i++) {
        const double coord = atoms.x[3 * i + d];
        if (coord >= swap.lo && coord <= swap.hi) swap.sendlist.push_back(i);
      }

      const int nsend = static_cast<int>(swap.sendlist.size());
      send_.resize(BORDER_WIDTH * static_cast<size_t>(nsend));
      for (int k = 0; k < nsend; k++) {
        const int i = swap.sendlist[k];
        double* buf = &send_[BORDER_WIDTH * k];
        for (int c = 0; c < 3; c++) buf[c] = atoms.x[3 * i + c];
        buf[d] += swap.shift;
        buf[3] = atoms.type[i];
      }

      // Vers soi-même (image périodique) : pas de message, le tampon d'envoi
      // sert directement de tampon de réception.
      const bool self = swap.sendproc == me;
      if (!self) send_recv(swap.sendproc, swap.recvproc);
      const std::vector<double>& in = self ? send_ : recv_;

      swap.nrecv = static_cast<int>(in.size()) / BORDER_WIDTH;
      swap.firstrecv = atoms.nlocal + atoms.nghost;
      atoms.reserve(swap.firstrecv + swap.nrecv);
      for (int k = 0; k < swap.nrecv; k++) {
        const int i = swap.firstrecv + k;
        for (int c = 0; c < 3; c++) atoms.x[3 * i + c] = in[BORDER_WIDTH * k + c];
        atoms.type[i] = static_cast<int>(in[BORDER_WIDTH * k + 3]);
      }
      atoms.nghost += swap.nrecv;
    }
  }
}

// Envoie aux fantômes `width` valeurs par atome prises dans `values`.
void Domain::forward(double* values, int width, bool with_shift)
{
  for (const Swap& swap : swaps_) {
    const int nsend = static_cast<int>(swap.sendlist.size());
    send_.resize(static_cast<size_t>(width) * nsend);
    for (int k = 0; k < nsend; k++) {
      const int i = swap.sendlist[k];
      for (int c = 0; c < width; c++) send_[width * k + c] = values[width * i + c];
      if (with_shift) send_[width * k + swap.dim] += swap.shift;
    }

    const bool self = swap.sendproc == me;
    if (!self) {
      recv_.resize(static_cast<size_t>(width) * swap.nrecv);
      MPI_Sendrecv(send_.data(), width * nsend, MPI_DOUBLE, swap.sendproc, 0, recv_.data(), width * swap.nrecv,
                   MPI_DOUBLE, swap.recvproc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    const std::vector<double>& in = self ? send_ : recv_;
    std::copy(in.begin(), in.begin() + static_cast<size_t>(width) * swap.nrecv, values + width * swap.firstrecv);
  }
}

void Domain::forward_positions(Atoms& atoms)
{
  forward(atoms.x.data(), 3, true);
}

void Domain::forward_per_atom(std::vector<double>& values)
{
  forward(values.data(), 1, false);
}

void Domain::reverse_forces(Atoms& atoms)
{
  // Ordre inverse de borders() : un fantôme de fantôme rend d'abord sa force
  // au fantôme intermédiaire, qui la transmet ensuite à l'atome d'origine.
  for (auto swap = swaps_.rbegin(); swap != swaps_.rend(); ++swap) {
    const int nsend = static_cast<int>(swap->sendlist.size());
    const double* ghost_forces = &atoms.f[3 * static_cast<size_t>(swap->firstrecv)];
    send_.assign(ghost_forces, ghost_forces + 3 * static_cast<size_t>(swap->nrecv));

    const bool self = swap->sendproc == me;
    if (!self) {
      recv_.resize(3 * static_cast<size_t>(nsend));
      MPI_Sendrecv(send_.data(), 3 * swap->nrecv, MPI_DOUBLE, swap->recvproc, 0, recv_.data(), 3 * nsend,
                   MPI_DOUBLE, swap->sendproc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    const std::vector<double>& in = self ? send_ : recv_;
    for (int k = 0; k < nsend; k++) {
      const int i = swap->sendlist[k];
      for (int c = 0; c < 3; c++) atoms.f[3 * i + c] += in[3 * k + c];
    }
  }
}

int Domain::total_sent() const
{
  int total = 0;
  for (const Swap& swap : swaps_) total += static_cast<int>(swap.sendlist.size());
  return total;
}

void Domain::remember_positions(const Atoms& atoms)
{
  xold_.assign(atoms.x.begin(), atoms.x.begin() + 3 * static_cast<size_t>(atoms.nlocal));
}

void Domain::warn_if_moved_too_far(const Atoms& atoms) const
{
  const Box& box = atoms.box;
  double dmax_sq = 0.0;
  for (int i = 0; i < atoms.nlocal; i++) {
    double dist_sq = 0.0;
    for (int d = 0; d < 3; d++) {
      double delta = atoms.x[3 * i + d] - xold_[3 * i + d];
      if (delta > box.prd[d]) delta -= box.prd[d];
      if (delta < -box.prd[d]) delta += box.prd[d];
      dist_sq += delta * delta;
    }
    dmax_sq = std::max(dmax_sq, dist_sq);
  }
  const double dmax = std::sqrt(dmax_sq);
  const double size[3] = {box.hi[0] - box.lo[0], box.hi[1] - box.lo[1], box.hi[2] - box.lo[2]};
  if (dmax > size[0] || dmax > size[1] || dmax > size[2])
    std::printf("Warning: Atoms move further than your subdomain size, which will eventually cause lost atoms.\n"
                "Increase reneighboring frequency or choose a different processor grid\n"
                "Maximum move distance: %lf; Subdomain dimensions: %lf %lf %lf\n",
                dmax, size[0], size[1], size[2]);
}
