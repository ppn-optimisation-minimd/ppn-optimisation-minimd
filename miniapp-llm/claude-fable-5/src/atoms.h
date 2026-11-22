// Les atomes d'un processus : positions, vitesses, forces et types.
#pragma once

#include <vector>

// Boîte de simulation périodique. Elle part de l'origine dans les trois
// directions ; chaque processus en possède un pavé [lo, hi).
struct Box {
  double prd[3] = {0.0, 0.0, 0.0};  // longueurs de la boîte entière
  double lo[3] = {0.0, 0.0, 0.0};   // bornes du sous-domaine de ce processus
  double hi[3] = {0.0, 0.0, 0.0};
};

// Tableaux plats : l'atome i occupe x[3*i], x[3*i+1], x[3*i+2] (idem v et f).
// Les nlocal premiers atomes appartiennent à ce processus. Les nghost
// suivants sont des « fantômes » : copies d'atomes voisins (d'un autre
// processus ou de l'autre côté de la boîte périodique), dont seules la
// position, la force et le type sont tenus à jour.
class Atoms {
 public:
  int natoms = 0;  // nombre total d'atomes, tous processus confondus
  int nlocal = 0;
  int nghost = 0;
  int ntypes;
  double mass = 1.0;  // masse commune à tous les atomes
  Box box;

  std::vector<double> x, v, f;
  std::vector<int> type;

  explicit Atoms(int ntypes_) : ntypes(ntypes_) {}

  // Garantit la place pour n atomes (locaux et fantômes).
  void reserve(int n);

  // Ajoute un atome local ; son type est tiré au hasard comme dans miniMD.
  void add_local(double px, double py, double pz, double vx, double vy, double vz);

  // Recopie position, vitesse et type de l'atome `from` dans la case `to`.
  void copy(int from, int to);

  // Ramène les atomes locaux dans la boîte périodique.
  void pbc();

  // Réordonne les atomes locaux : le nouvel atome k est l'ancien order[k].
  // Les forces ne sont pas déplacées : elles sont recalculées juste après.
  void reorder(const std::vector<int>& order);

 private:
  std::vector<double> x_tmp_, v_tmp_;  // tampons de reorder(), conservés entre deux tris
  std::vector<int> type_tmp_;
};
