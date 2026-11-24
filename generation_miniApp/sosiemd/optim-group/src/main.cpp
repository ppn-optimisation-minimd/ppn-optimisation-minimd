// sosieMD : mini-application de dynamique moléculaire équivalente à miniMD
// (variante ref). Ce fichier donne le déroulement d'une exécution ; chaque
// étape est détaillée dans le fichier du même nom.
#include <mpi.h>
#include <omp.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "atoms.h"
#include "config.h"
#include "domain.h"
#include "force.h"
#include "force_eam.h"
#include "force_lj.h"
#include "integrate.h"
#include "neighbor.h"
#include "output.h"
#include "setup.h"
#include "thermo.h"
#include "timer.h"

namespace {

// Nombre de cellules de voisinage par dimension, avec les règles de miniMD.
void choose_bins(const Config& cfg, const Atoms& atoms, int nbin[3])
{
  const Box& box = atoms.box;
  if (cfg.neigh_bins > 0) {
    nbin[0] = nbin[1] = nbin[2] = cfg.neigh_bins;
  } else if (cfg.data_file.empty()) {
    // Réseau généré : 5 cellules pour 6 mailles.
    const double scale = 5.0 / 6.0;
    nbin[0] = static_cast<int>(scale * cfg.nx);
    nbin[1] = static_cast<int>(scale * cfg.ny);
    nbin[2] = static_cast<int>(scale * cfg.nz);
  } else {
    // Fichier de données : taille de cellule déduite de la densité.
    const double density = atoms.natoms / (box.prd[0] * box.prd[1] * box.prd[2]);
    const double bin_size = std::pow(density * 16, 1.0 / 3.0);
    for (int d = 0; d < 3; d++) nbin[d] = static_cast<int>(box.prd[d] / bin_size);
  }
  for (int d = 0; d < 3; d++)
    if (nbin[d] == 0) nbin[d] = 1;
}

}  // namespace

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  Config cfg = read_config(argc, argv);
  omp_set_num_threads(cfg.num_threads);
  std::srand(5413);  // graine du tirage des types d'atomes, celle de miniMD

  Atoms atoms(cfg.ntypes);
  Domain domain;
  Neighbor neighbor;
  Thermo thermo;
  Timer timer;

  std::unique_ptr<Force> force;
  if (cfg.force_style == ForceStyle::LJ)
    force.reset(new ForceLJ(cfg.ntypes, cfg.epsilon, cfg.sigma, cfg.force_cut));
  else
    force.reset(new ForceEAM("Cu_u6.eam"));  // cherché dans le répertoire courant, comme miniMD
  force->serial = cfg.half_neigh == -1;

  // --- Création du système ---
  if (domain.me == 0) std::printf("# Create System:\n");

  DataFile data;
  const bool from_file = !cfg.data_file.empty();
  if (from_file) {
    data = read_data_file(cfg.data_file);
    atoms.natoms = data.natoms;
    for (int d = 0; d < 3; d++) atoms.box.prd[d] = data.prd[d];
    if (data.mass > 0.0) atoms.mass = data.mass;
    cfg.rho = atoms.natoms / (data.prd[0] * data.prd[1] * data.prd[2]);
  } else {
    create_box(atoms.box, cfg.nx, cfg.ny, cfg.nz, cfg.rho);
  }

  domain.setup(cfg.neigh_cut, atoms.box);

  neighbor.every = cfg.neigh_every;
  neighbor.cutneigh = cfg.neigh_cut;
  neighbor.half = cfg.half_neigh != 0;
  neighbor.ghost_newton = cfg.ghost_newton != 0;
  choose_bins(cfg, atoms, neighbor.nbin);
  neighbor.setup(atoms.box);

  if (cfg.force_style == ForceStyle::EAM) atoms.mass = force->mass;

  if (from_file) {
    add_atoms_from_data(data, atoms);
    data = DataFile();  // libère la copie du fichier
    thermo.setup(cfg.units, atoms);
  } else {
    create_atoms(atoms, cfg.nx, cfg.ny, cfg.nz, cfg.rho);
    thermo.setup(cfg.units, atoms);
    create_velocity(cfg.t_request, atoms, thermo);
  }

  if (domain.me == 0) std::printf("# Done .... \n");
  print_header(cfg, atoms, neighbor, domain);

  // --- Pas 0 : fantômes, listes de voisins, forces ---
  const bool reverse_needed = neighbor.half && neighbor.ghost_newton;
  domain.exchange(atoms);
  if (cfg.sort_at_start) atoms.reorder(neighbor.bin_atoms(atoms, atoms.nlocal));
  domain.borders(atoms);
  if (cfg.check_exchange) domain.remember_positions(atoms);
  neighbor.build(atoms);
  force->compute(atoms, neighbor, domain, true);
  if (reverse_needed) domain.reverse_forces(atoms);

  if (domain.me == 0) {
    std::printf("# Starting dynamics ...\n");
    std::printf("# Timestep T U P Time\n");
  }
  thermo.report(0, atoms, *force, 0.0);

  // --- Boucle en temps ---
  run(cfg, atoms, *force, neighbor, domain, thermo, timer);

  int natoms = 0;  // recompté : un atome perdu se verrait ici
  MPI_Allreduce(&atoms.nlocal, &natoms, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  // --- État final, s'il n'a pas déjà été affiché par la boucle ---
  if (cfg.thermo_every == 0 || cfg.nsteps % cfg.thermo_every != 0) {
    force->compute(atoms, neighbor, domain, true);
    if (reverse_needed) domain.reverse_forces(atoms);
    thermo.report(cfg.nsteps, atoms, *force, timer.total[TIME_TOTAL]);
  }

  print_performance(cfg, timer, domain, natoms);
  if (cfg.yaml_output) write_yaml(cfg, atoms, neighbor, domain, thermo, timer);

  MPI_Finalize();
  return 0;
}
