// Paramètres d'une exécution : fichier d'entrée au format miniMD, puis
// options de la ligne de commande qui le complètent ou le remplacent.
#pragma once

#include <string>

#define SOSIEMD_VERSION "sosieMD 1.0 (MPI+OpenMP)"

enum class Units { LJ, Metal };
enum class ForceStyle { LJ, EAM };

struct Config {
  // --- Fichier d'entrée (une valeur par ligne, voir in.lj.miniMD) ---
  std::string input_file = "in.lj.miniMD";
  std::string data_file;                 // vide : réseau cfc généré
  Units units = Units::LJ;
  ForceStyle force_style = ForceStyle::LJ;
  double epsilon = 1.0, sigma = 1.0;     // paramètres de Lennard-Jones
  int nx = 0, ny = 0, nz = 0;            // taille en mailles du réseau
  int nsteps = 0;                        // nombre de pas de temps
  double dt = 0.0;                       // pas de temps
  double t_request = 0.0;                // température initiale
  double rho = 0.0;                      // densité
  int neigh_every = 0;                   // période de reconstruction des listes
  double force_cut = 0.0;                // rayon de coupure des forces
  double neigh_cut = 0.0;                // rayon des listes = coupure + peau
  int thermo_every = 0;                  // période d'affichage (0 : début et fin)

  // --- Ligne de commande ---
  int num_threads = 1;
  int ntypes = 4;                        // nombre de types d'atomes
  int half_neigh = 1;                    // 1 demi-listes, 0 listes complètes, -1 demi-listes sans threads
  int ghost_newton = 1;                  // 3e loi de Newton sur les atomes fantômes
  int neigh_bins = -1;                   // cellules par dimension (-1 : automatique)
  int sort_every = 0;                    // période du tri des atomes (0 : jamais)
  bool sort_at_start = false;            // tri avant le pas 0 (seulement si --sort N > 0)
  bool check_exchange = false;           // avertir si un atome traverse plus d'un sous-domaine
  int yaml_output = 0;                   // non nul : écrire le fichier YAML
  bool yaml_screen = false;              // YAML aussi sur la sortie standard
};

// Lit le fichier d'entrée et la ligne de commande. Affiche l'aide et quitte
// si -h est présent ; quitte avec un message en cas d'erreur.
Config read_config(int argc, char** argv);
