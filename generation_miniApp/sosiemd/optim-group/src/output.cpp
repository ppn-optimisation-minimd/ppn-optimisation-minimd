#include "output.h"

#include <mpi.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace {

const char* units_name(const Config& cfg) { return cfg.units == Units::LJ ? "LJ" : "METAL"; }
const char* force_name(const Config& cfg) { return cfg.force_style == ForceStyle::LJ ? "LJ" : "EAM"; }
const char* data_name(const Config& cfg) { return cfg.data_file.empty() ? "None" : cfg.data_file.c_str(); }

// Destination du YAML : le fichier, et aussi l'écran si demandé. Seul le
// processus 0 a un fichier ouvert ; les autres n'écrivent rien.
struct YamlSink {
  FILE* file = nullptr;
  bool screen = false;

  void print(const char* format, ...) const
  {
    if (!file) return;
    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    if (screen) {
      va_start(args, format);
      std::vprintf(format, args);
      va_end(args);
    }
  }
};

// Moyenne, extrêmes et histogramme (10 classes) d'une valeur par processus.
struct ProcStats {
  double ave, max, min;
  int histogram[10];
};

ProcStats stats_over_procs(double value, int nprocs)
{
  ProcStats s;
  MPI_Allreduce(&value, &s.ave, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(&value, &s.min, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&value, &s.max, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  s.ave /= nprocs;

  int mine[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const double range = s.max - s.min;
  int slot = range == 0.0 ? 0 : static_cast<int>((value - s.min) / range * 10);
  if (slot > 9) slot = 9;
  mine[slot] = 1;
  MPI_Allreduce(mine, s.histogram, 10, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  return s;
}

void print_stats(const YamlSink& out, const char* label, double value, int nprocs)
{
  const ProcStats s = stats_over_procs(value, nprocs);
  out.print("# %s %g ave %g max %g min\n", label, s.ave, s.max, s.min);
  out.print("# Histogram:");
  for (int count : s.histogram) out.print(" %d", count);
  out.print("\n");
}

double average_over_procs(double value, int nprocs)
{
  double sum = 0.0;
  MPI_Allreduce(&value, &sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  return sum / nprocs;
}

}  // namespace

void print_header(const Config& cfg, const Atoms& atoms, const Neighbor& neighbor, const Domain& domain)
{
  if (domain.me != 0) return;
  std::printf("# " SOSIEMD_VERSION " output ...\n");
  std::printf("# Run Settings: \n");
  std::printf("\t# MPI processes: %i\n", domain.nprocs);
  std::printf("\t# OpenMP threads: %i\n", cfg.num_threads);
  std::printf("\t# Inputfile: %s\n", cfg.input_file.c_str());
  std::printf("\t# Datafile: %s\n", data_name(cfg));
  std::printf("# Physics Settings: \n");
  std::printf("\t# ForceStyle: %s\n", force_name(cfg));
  std::printf("\t# Force Parameters: %2.2lf %2.2lf\n", cfg.epsilon, cfg.sigma);
  std::printf("\t# Units: %s\n", units_name(cfg));
  std::printf("\t# Atoms: %i\n", atoms.natoms);
  std::printf("\t# Atom types: %i\n", atoms.ntypes);
  std::printf("\t# System size: %2.2lf %2.2lf %2.2lf (unit cells: %i %i %i)\n", atoms.box.prd[0],
              atoms.box.prd[1], atoms.box.prd[2], cfg.nx, cfg.ny, cfg.nz);
  std::printf("\t# Density: %lf\n", cfg.rho);
  std::printf("\t# Force cutoff: %lf\n", cfg.force_cut);
  std::printf("\t# Timestep size: %lf\n", cfg.dt);
  std::printf("# Technical Settings: \n");
  std::printf("\t# Neigh cutoff: %lf\n", cfg.neigh_cut);
  std::printf("\t# Half neighborlists: %i\n", cfg.half_neigh);
  std::printf("\t# Neighbor bins: %i %i %i\n", neighbor.nbin[0], neighbor.nbin[1], neighbor.nbin[2]);
  std::printf("\t# Neighbor frequency: %i\n", cfg.neigh_every);
  std::printf("\t# Sorting frequency: %i\n", cfg.sort_every);
  std::printf("\t# Thermo frequency: %i\n", cfg.thermo_every);
  std::printf("\t# Ghost Newton: %i\n", cfg.ghost_newton);
  std::printf("\t# Use intrinsics: %i\n", 0);
  std::printf("\t# Do safe exchange: %i\n", 0);
  std::printf("\t# Size of float: %i\n\n", static_cast<int>(sizeof(double)));
}

void print_performance(const Config& cfg, const Timer& timer, const Domain& domain, int natoms)
{
  if (domain.me != 0) return;
  const double* t = timer.total;
  const double other = t[TIME_TOTAL] - t[TIME_FORCE] - t[TIME_NEIGH] - t[TIME_COMM];
  const double performance = 1.0 * natoms * cfg.nsteps / t[TIME_TOTAL];  // atomes x pas par seconde
  std::printf("\n\n");
  std::printf("# Performance Summary:\n");
  std::printf("# MPI_proc OMP_threads nsteps natoms t_total t_force t_neigh t_comm t_other performance "
              "perf/thread grep_string t_extra\n");
  std::printf("%i %i %i %i %lf %lf %lf %lf %lf %lf %lf PERF_SUMMARY %lf\n\n\n", domain.nprocs, cfg.num_threads,
              cfg.nsteps, natoms, t[TIME_TOTAL], t[TIME_FORCE], t[TIME_NEIGH], t[TIME_COMM], other, performance,
              performance / domain.nprocs / cfg.num_threads, t[TIME_EXTRA]);
}

void write_yaml(const Config& cfg, Atoms& atoms, const Neighbor& neighbor, const Domain& domain,
                const Thermo& thermo, const Timer& timer)
{
  const int nprocs = domain.nprocs;
  const bool root = domain.me == 0;
  const Box& box = atoms.box;

  // Vérification préalable, comme miniMD : aucun atome perdu ni hors de la boîte.
  atoms.pbc();
  int nlost = 0;
  for (int i = 0; i < atoms.nlocal; i++)
    for (int d = 0; d < 3; d++)
      if (atoms.x[3 * i + d] < 0.0 || atoms.x[3 * i + d] >= box.prd[d]) {
        nlost++;
        break;
      }
  int counts[2] = {atoms.nlocal, nlost};
  int totals[2];
  MPI_Allreduce(counts, totals, 2, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  const int natoms = totals[0];
  if (natoms != atoms.natoms || totals[1] > 0) {
    if (root) {
      std::printf("Atom counts = %d %d %d\n", totals[1], natoms, atoms.natoms);
      std::printf("ERROR: Incorrect number of atoms\n");
    }
    return;
  }

  YamlSink out;
  out.screen = cfg.yaml_screen;
  if (root) {
    const std::time_t now = std::time(nullptr);
    const std::tm local = *std::localtime(&now);
    char filename[64];
    std::snprintf(filename, sizeof(filename), "sosieMD-%4d-%02d-%02d-%02d-%02d-%02d.yaml", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    out.file = std::fopen(filename, "w");
    if (!out.file) std::printf("# Warning: impossible d'ecrire %s\n", filename);
  }

  out.print("run_configuration: \n");
  out.print("  variant: " SOSIEMD_VERSION "\n");
  out.print("  mpi_processes: %i\n", nprocs);
  out.print("  thread_teams: %i\n", 1);
  out.print("  threads: %i\n", cfg.num_threads);
  out.print("  datafile: %s\n", data_name(cfg));
  out.print("  units: %s\n", units_name(cfg));
  out.print("  atoms: %i\n", atoms.natoms);
  out.print("  atom_types: %i\n", atoms.ntypes);
  out.print("  system_size: %2.2lf %2.2lf %2.2lf\n", box.prd[0], box.prd[1], box.prd[2]);
  out.print("  unit_cells: %i %i %i\n", cfg.nx, cfg.ny, cfg.nz);
  out.print("  density: %lf\n", cfg.rho);
  out.print("  force_type: %s\n", force_name(cfg));
  out.print("  force_cutoff: %lf\n", cfg.force_cut);
  out.print("  force_params: %2.2lf %2.2lf\n", cfg.epsilon, cfg.sigma);
  out.print("  neighbor_cutoff: %lf\n", cfg.neigh_cut);
  out.print("  neighbor_type: %i\n", cfg.half_neigh);
  out.print("  neighbor_bins: %i %i %i\n", neighbor.nbin[0], neighbor.nbin[1], neighbor.nbin[2]);
  out.print("  neighbor_frequency: %i\n", cfg.neigh_every);
  out.print("  sort_frequency: %i\n", cfg.sort_every);
  out.print("  timestep_size: %lf\n", cfg.dt);
  out.print("  thermo_frequency: %i\n", cfg.thermo_every);
  out.print("  ghost_newton: %i\n", cfg.ghost_newton);
  out.print("  use_intrinsics: %i\n", 0);
  out.print("  safe_exchange: %i\n", 0);
  out.print("  float_size: %i\n\n", static_cast<int>(sizeof(double)));

  // Historique thermodynamique. « Conservation » rapporte 1.5 T + U à sa
  // valeur initiale.
  out.print("\n\nthermodynamic_output:\n");
  for (size_t i = 0; i < thermo.steps.size(); i++) {
    const double conserve = (1.5 * thermo.temperatures[i] + thermo.energies[i]) /
                            (1.5 * thermo.temperatures[0] + thermo.energies[0]);
    out.print("  timestep: %d \n", thermo.steps[i]);
    out.print("      T*:           %15.10g \n", thermo.temperatures[i]);
    out.print("      U*:           %15.10g \n", thermo.energies[i]);
    out.print("      P*:           %15.10g \n", thermo.pressures[i]);
    out.print("      Conservation: %15.10g \n", conserve);
  }

  // Comme miniMD, ces lignes vides vont toujours sur la sortie standard.
  if (root) {
    std::printf("\n\n");
    if (out.file) std::fprintf(out.file, "\n\n");
  }

  // Temps, moyennés sur les processus.
  const double* t = timer.total;
  double time_total = average_over_procs(t[TIME_TOTAL], nprocs);
  const double performance = 1.0 * natoms * cfg.nsteps / time_total;
  out.print("time:\n");
  out.print("  total:\n");
  out.print("    time: %g \n", time_total);
  out.print("    performance: %10.5e \n", performance);
  out.print("    performance_proc: %10.5e \n", performance / nprocs / cfg.num_threads);
  if (time_total == 0.0) time_total = 1.0;
  const double time_other = time_total - (t[TIME_FORCE] + t[TIME_NEIGH] + t[TIME_COMM]);
  out.print("  force: %g\n", average_over_procs(t[TIME_FORCE], nprocs));
  out.print("  neigh: %g\n", average_over_procs(t[TIME_NEIGH], nprocs));
  out.print("  comm:  %g\n", average_over_procs(t[TIME_COMM], nprocs));
  out.print("  other: %g\n", average_over_procs(time_other, nprocs));
  out.print("\n");

  // Répartition entre processus.
  out.print("# Timing histograms \n");
  print_stats(out, "Force time:", t[TIME_FORCE], nprocs);
  print_stats(out, "Neigh time:", t[TIME_NEIGH], nprocs);
  print_stats(out, "Comm  time:", t[TIME_COMM], nprocs);
  print_stats(out, "Other time:", time_other, nprocs);
  if (root) {
    std::printf("\n");
    if (out.file) std::fprintf(out.file, "\n");
  }

  double nneighs = 0.0;  // voisins rangés dans les listes de ce processus
  for (int i = 0; i < atoms.nlocal; i++) nneighs += neighbor.numneigh[i];
  print_stats(out, "Nlocal:    ", atoms.nlocal, nprocs);
  print_stats(out, "Nghost:    ", atoms.nghost, nprocs);
  print_stats(out, "Nswaps:    ", domain.total_sent(), nprocs);
  print_stats(out, "Neighs:    ", nneighs, nprocs);

  double total_neighs = 0.0;
  MPI_Allreduce(&nneighs, &total_neighs, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  out.print("# Total # of neighbors = %g\n", total_neighs);
  out.print("\n");

  if (out.file) std::fclose(out.file);
}
