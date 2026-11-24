#include "setup.h"

#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "util.h"

namespace {

// Générateur de Park et Miller (16807 * graine mod 2^31-1), sans débordement
// grâce à la décomposition de Schrage. Renvoie un nombre dans ]0, 1[ et fait
// avancer la graine.
double park_miller(int& seed)
{
  const int IA = 16807, IM = 2147483647, IQ = 127773, IR = 2836;
  const int k = seed / IQ;
  seed = IA * (seed - k * IQ) - IR * k;
  if (seed < 0) seed += IM;
  return (1.0 / IM) * seed;
}

// Une composante de vitesse : le sixième tirage, les cinq premiers servant à
// décorréler des graines voisines.
double velocity_component(int& seed)
{
  for (int m = 0; m < 5; m++) park_miller(seed);
  return park_miller(seed);
}

bool inside(const Box& box, const double* pos)
{
  for (int d = 0; d < 3; d++)
    if (pos[d] < box.lo[d] || pos[d] >= box.hi[d]) return false;
  return true;
}

// Vérifie que tous les processus réunis ont créé exactement natoms atomes.
void check_atom_count(const Atoms& atoms)
{
  int total = 0;
  MPI_Allreduce(&atoms.nlocal, &total, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  if (total != atoms.natoms)
    fatal("Created incorrect # of atoms : " + std::to_string(total) + " au lieu de " +
          std::to_string(atoms.natoms) + " (les atomes doivent etre dans la boite [0, L[)");
}

}  // namespace

void create_box(Box& box, int nx, int ny, int nz, double rho)
{
  const double lattice = std::pow(4.0 / rho, 1.0 / 3.0);  // 4 atomes par maille
  box.prd[0] = nx * lattice;
  box.prd[1] = ny * lattice;
  box.prd[2] = nz * lattice;
}

void create_atoms(Atoms& atoms, int nx, int ny, int nz, double rho)
{
  atoms.natoms = 4 * nx * ny * nz;
  atoms.nlocal = 0;

  // Les sites sont repérés par des indices entiers (i, j, k) de pas un
  // demi-paramètre de maille ; ceux du réseau cfc ont i + j + k pair.
  const Box& box = atoms.box;
  const double half = 0.5 * std::pow(4.0 / rho, 1.0 / 3.0);
  const int nsites[3] = {2 * nx, 2 * ny, 2 * nz};
  int lo[3], hi[3];  // sites pouvant tomber dans mon sous-domaine
  for (int d = 0; d < 3; d++) {
    lo[d] = std::max(static_cast<int>(box.lo[d] / half - 1), 0);
    hi[d] = std::min(static_cast<int>(box.hi[d] / half + 1), nsites[d] - 1);
  }

  // Ordre de parcours de miniMD : par blocs de 8 x 8 x 8 sites, z puis y puis
  // x à l'extérieur comme à l'intérieur des blocs. Cet ordre fixe la
  // numérotation initiale des atomes, donc l'ordre de toutes les sommes.
  const int BLOCK = 8;
  for (int bz = 0; bz * BLOCK <= hi[2]; bz++)
    for (int by = 0; by * BLOCK <= hi[1]; by++)
      for (int bx = 0; bx * BLOCK <= hi[0]; bx++)
        for (int sz = 0; sz < BLOCK; sz++)
          for (int sy = 0; sy < BLOCK; sy++)
            for (int sx = 0; sx < BLOCK; sx++) {
              const int i = bx * BLOCK + sx;
              const int j = by * BLOCK + sy;
              const int k = bz * BLOCK + sz;
              if ((i + j + k) % 2 != 0) continue;
              if (i < lo[0] || i > hi[0] || j < lo[1] || j > hi[1] || k < lo[2] || k > hi[2]) continue;

              const double pos[3] = {half * i, half * j, half * k};
              if (!inside(box, pos)) continue;

              // La graine ne dépend que du site : les vitesses sont les
              // mêmes quel que soit le nombre de processus.
              int seed = k * nsites[1] * nsites[0] + j * nsites[0] + i + 1;
              const double vx = velocity_component(seed);
              const double vy = velocity_component(seed);
              const double vz = velocity_component(seed);
              atoms.add_local(pos[0], pos[1], pos[2], vx, vy, vz);
            }

  check_atom_count(atoms);
}

void create_velocity(double t_request, Atoms& atoms, const Thermo& thermo)
{
  double sum[3] = {0.0, 0.0, 0.0};
  for (int i = 0; i < atoms.nlocal; i++)
    for (int d = 0; d < 3; d++) sum[d] += atoms.v[3 * i + d];

  double mean[3];
  for (int d = 0; d < 3; d++) {
    MPI_Allreduce(&sum[d], &mean[d], 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    mean[d] /= atoms.natoms;
  }
  for (int i = 0; i < atoms.nlocal; i++)
    for (int d = 0; d < 3; d++) atoms.v[3 * i + d] -= mean[d];

  const double factor = std::sqrt(t_request / thermo.temperature(atoms));
  for (int i = 0; i < 3 * atoms.nlocal; i++) atoms.v[i] *= factor;
}

DataFile read_data_file(const std::string& path)
{
  std::ifstream file(path);
  if (!file) fatal("Cannot open data file " + path);

  DataFile data;
  std::string line;
  auto strip = [](std::string text) {  // sans commentaire ni blancs aux extrémités
    text = text.substr(0, text.find('#'));
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
  };

  // En-tête : titre, puis des lignes « valeur(s) mot-clé » jusqu'au nom de la
  // première section.
  std::getline(file, line);
  std::string section;
  while (std::getline(file, line)) {
    line = strip(line);
    if (line.empty()) continue;
    std::istringstream fields(line);
    double lo = 0.0, hi = 0.0;
    if (line.find("atoms") != std::string::npos) fields >> data.natoms;
    else if (line.find("atom types") != std::string::npos) continue;  // types tirés au hasard, comme miniMD
    else if (line.find("xlo xhi") != std::string::npos) { fields >> lo >> hi; data.prd[0] = hi - lo; }
    else if (line.find("ylo yhi") != std::string::npos) { fields >> lo >> hi; data.prd[1] = hi - lo; }
    else if (line.find("zlo zhi") != std::string::npos) { fields >> lo >> hi; data.prd[2] = hi - lo; }
    else { section = line; break; }
  }
  if (data.natoms <= 0 || data.prd[0] <= 0.0 || data.prd[1] <= 0.0 || data.prd[2] <= 0.0)
    fatal(path + " : en-tete incomplet (nombre d'atomes et bornes xlo xhi, ylo yhi, zlo zhi attendus)");

  data.x.assign(3 * static_cast<size_t>(data.natoms), 0.0);
  data.v.assign(3 * static_cast<size_t>(data.natoms), 0.0);

  // Lit natoms lignes « identifiant [type] a b c » dans le tableau `values`.
  auto read_per_atom = [&](std::vector<double>& values, bool has_type) {
    for (int n = 0; n < data.natoms; n++) {
      int id = 0, type = 0;
      double a, b, c;
      if (!std::getline(file, line)) fatal(path + " : section " + section + " incomplete");
      std::istringstream fields(line);
      fields >> id;
      if (has_type) fields >> type;
      fields >> a >> b >> c;
      if (fields.fail() || id < 1 || id > data.natoms)
        fatal(path + " : ligne invalide dans la section " + section + " : '" + line + "'");
      values[3 * static_cast<size_t>(id - 1) + 0] = a;
      values[3 * static_cast<size_t>(id - 1) + 1] = b;
      values[3 * static_cast<size_t>(id - 1) + 2] = c;
    }
  };

  // Sections : un nom, une ligne vide, puis le contenu.
  bool have_atoms = false;
  while (!section.empty()) {
    std::getline(file, line);  // ligne vide après le nom
    if (section == "Atoms") {
      read_per_atom(data.x, true);
      have_atoms = true;
    } else if (section == "Velocities") {
      read_per_atom(data.v, false);
    } else if (section == "Masses") {
      // Une masse par type dans le fichier ; comme miniMD, on ne retient que
      // la première : tous les atomes ont la même masse.
      bool first = true;
      while (std::getline(file, line) && !strip(line).empty()) {
        int type;
        double mass;
        std::istringstream fields(line);
        fields >> type >> mass;
        if (fields.fail()) fatal(path + " : ligne invalide dans la section Masses : '" + line + "'");
        if (first) data.mass = mass;
        first = false;
      }
    } else {
      fatal(path + " : section inconnue '" + section + "' (Atoms, Velocities et Masses sont reconnues)");
    }

    section.clear();
    while (section.empty() && std::getline(file, line)) section = strip(line);
  }
  if (!have_atoms) fatal(path + " : section Atoms absente");
  return data;
}

void add_atoms_from_data(const DataFile& data, Atoms& atoms)
{
  atoms.nlocal = 0;
  for (int i = 0; i < data.natoms; i++) {
    const double* pos = &data.x[3 * static_cast<size_t>(i)];
    const double* vel = &data.v[3 * static_cast<size_t>(i)];
    if (inside(atoms.box, pos)) atoms.add_local(pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]);
  }
  check_atom_count(atoms);
}
