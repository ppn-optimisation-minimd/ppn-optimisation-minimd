#include "config.h"

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "util.h"

namespace {

bool is_root()
{
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank == 0;
}

void print_help()
{
  std::printf(
      "\n" SOSIEMD_VERSION "\n"
      "Mini-application de dynamique moleculaire equivalente a miniMD (variante ref).\n\n"
      "Options (memes noms, memes sens et memes valeurs par defaut que miniMD) :\n\n"
      "  Execution :\n"
      "\t-t / --num_threads <n>:       nombre de threads OpenMP par processus (defaut 1)\n"
      "\t--half_neigh <int>:           type de listes de voisins (defaut 1)\n"
      "\t                                0: listes completes\n"
      "\t                                1: demi-listes\n"
      "\t                               -1: demi-listes, forces calculees sans threads\n"
      "\t-gn / --ghost_newton <int>:   3e loi de Newton sur les atomes fantomes (defaut 1,\n"
      "\t                                demi-listes seulement ; force a 0 avec EAM)\n"
      "\t--teams <n>:                  accepte et ignore, comme dans miniMD ref\n"
      "\t-sse <n>, -d / --device <n>, -dm / --device_map, -ng / --num_gpus <n>,\n"
      "\t--skip_gpu <n>:               sans objet ici : acceptes sans effet, avec un avertissement\n"
      "\n  Simulation :\n"
      "\t-i / --input_file <fichier>:  fichier d'entree (defaut : in.lj.miniMD)\n"
      "\t--ntypes <n>:                 nombre de types d'atomes (defaut 4)\n"
      "\t-n / --nsteps <n>:            nombre de pas de temps\n"
      "\t-s / --size <n>:              taille du reseau dans les trois directions\n"
      "\t-nx/-ny/-nz <n>:              taille du reseau par direction (-ny et -nz exigent -nx)\n"
      "\t-b / --neigh_bins <n>:        nombre de cellules de voisinage par direction\n"
      "\t-u / --units <lj|metal>:      systeme d'unites\n"
      "\t-p / --force <lj|eam>:        potentiel\n"
      "\t-f / --data_file <fichier>:   configuration lue dans un fichier de donnees LAMMPS\n"
      "\n  Divers :\n"
      "\t--check_exchange:             avertir si un atome parcourt plus d'un sous-domaine\n"
      "\t                                entre deux echanges\n"
      "\t--safe_exchange:              accepte sans effet, comme dans miniMD ref\n"
      "\t--sort <n>:                   trier les atomes tous les <n> pas (defaut : periode\n"
      "\t                                des listes de voisins ; 0 = jamais)\n"
      "\t-o / --yaml_output <n>:       ecrire le fichier YAML si <n> est non nul (defaut 0)\n"
      "\t--yaml_screen:                ecrire aussi le YAML sur la sortie standard\n"
      "\t-h / --help:                  afficher cette aide\n\n");
}

int to_int(const std::string& text, const std::string& option)
{
  char* end = nullptr;
  const long value = std::strtol(text.c_str(), &end, 10);
  if (text.empty() || *end != '\0')
    fatal("valeur entiere attendue apres " + option + ", recu '" + text + "'");
  return static_cast<int>(value);
}

// Les 14 lignes du fichier d'entrée : titre, ligne vide, puis une valeur (ou
// un groupe de valeurs) par ligne, suivie d'un commentaire libre.
void read_input_file(Config& cfg)
{
  std::ifstream file(cfg.input_file);
  if (!file) fatal("Cannot open " + cfg.input_file);

  int line_number = 0;
  auto next_line = [&]() {
    std::string line;
    if (!std::getline(file, line))
      fatal("fichier d'entree " + cfg.input_file + " incomplet (14 lignes attendues)");
    line_number++;
    return std::istringstream(line);
  };
  auto check = [&](const std::istringstream& fields, const char* expected) {
    if (fields.fail())
      fatal(cfg.input_file + ", ligne " + std::to_string(line_number) + " : " + expected + " attendu");
  };

  next_line();  // titre
  next_line();  // ligne vide

  std::string word;
  std::istringstream fields = next_line();
  fields >> word;
  if (word == "lj") cfg.units = Units::LJ;
  else if (word == "metal") cfg.units = Units::Metal;
  else fatal(cfg.input_file + ", ligne 3 : unites 'lj' ou 'metal' attendues, recu '" + word + "'");

  fields = next_line();
  fields >> word;
  check(fields, "nom de fichier de donnees ou 'none'");
  cfg.data_file = (word == "none") ? "" : word;

  fields = next_line();
  word.clear();
  fields >> word;
  if (word == "lj") cfg.force_style = ForceStyle::LJ;
  else if (word == "eam") cfg.force_style = ForceStyle::EAM;
  else fatal(cfg.input_file + ", ligne 5 : potentiel 'lj' ou 'eam' attendu, recu '" + word + "'");

  fields = next_line();
  fields >> cfg.epsilon >> cfg.sigma;
  check(fields, "epsilon et sigma");
  fields = next_line();
  fields >> cfg.nx >> cfg.ny >> cfg.nz;
  check(fields, "trois tailles entieres");
  fields = next_line();
  fields >> cfg.nsteps;
  check(fields, "nombre de pas");
  fields = next_line();
  fields >> cfg.dt;
  check(fields, "pas de temps");
  fields = next_line();
  fields >> cfg.t_request;
  check(fields, "temperature initiale");
  fields = next_line();
  fields >> cfg.rho;
  check(fields, "densite");
  fields = next_line();
  fields >> cfg.neigh_every;
  check(fields, "periode des listes de voisins");
  double skin = 0.0;
  fields = next_line();
  fields >> cfg.force_cut >> skin;
  check(fields, "rayon de coupure et peau");
  cfg.neigh_cut = cfg.force_cut + skin;
  fields = next_line();
  fields >> cfg.thermo_every;
  check(fields, "periode d'affichage");
}

}  // namespace

Config read_config(int argc, char** argv)
{
  Config cfg;

  // Le fichier d'entrée est lu d'abord : la ligne de commande a le dernier mot.
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      if (is_root()) print_help();
      MPI_Finalize();
      std::exit(0);
    }
    if ((arg == "-i" || arg == "--input_file") && i + 1 < argc) cfg.input_file = argv[i + 1];
  }
  read_input_file(cfg);

  int nsteps = -1, size = -1, nx = -1, ny = -1, nz = -1, sort = -1;

  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) fatal("valeur manquante apres " + arg);
      return argv[++i];
    };
    auto int_value = [&]() { return to_int(value(), arg); };
    auto ignored = [&](const char* reason) {
      if (is_root()) std::printf("# Warning: option %s %s\n", arg.c_str(), reason);
    };

    if (arg == "-i" || arg == "--input_file") value();
    else if (arg == "-t" || arg == "--num_threads") cfg.num_threads = int_value();
    else if (arg == "-n" || arg == "--nsteps") nsteps = int_value();
    else if (arg == "-s" || arg == "--size") size = int_value();
    else if (arg == "-nx") nx = int_value();
    else if (arg == "-ny") ny = int_value();
    else if (arg == "-nz") nz = int_value();
    else if (arg == "--ntypes") cfg.ntypes = int_value();
    else if (arg == "-b" || arg == "--neigh_bins") cfg.neigh_bins = int_value();
    else if (arg == "--half_neigh") cfg.half_neigh = int_value();
    else if (arg == "-gn" || arg == "--ghost_newton") cfg.ghost_newton = int_value();
    else if (arg == "--sort") sort = int_value();
    else if (arg == "--check_exchange") cfg.check_exchange = true;
    else if (arg == "-o" || arg == "--yaml_output") cfg.yaml_output = int_value();
    else if (arg == "--yaml_screen") cfg.yaml_screen = true;
    else if (arg == "-f" || arg == "--data_file") cfg.data_file = value();
    else if (arg == "-u" || arg == "--units") cfg.units = (value() == "metal") ? Units::Metal : Units::LJ;
    else if (arg == "-p" || arg == "--force") cfg.force_style = (value() == "eam") ? ForceStyle::EAM : ForceStyle::LJ;
    else if (arg == "--teams") {
      if (int_value() != 1) ignored("ignoree, comme dans miniMD ref : une seule equipe de threads");
    } else if (arg == "-sse") {
      if (int_value() != 0) ignored("sans objet : pas de variante a intrinseques, calcul standard");
    } else if (arg == "-d" || arg == "--device" || arg == "-ng" || arg == "--num_gpus" || arg == "--skip_gpu") {
      value();
      ignored("sans objet : pas d'execution sur GPU");
    } else if (arg == "-dm" || arg == "--device_map") {
      ignored("sans objet : pas d'execution sur GPU");
    } else if (arg == "--safe_exchange") {
      ignored("sans effet, comme dans miniMD ref qui ne lit pas cette option");
    } else {
      fatal("option inconnue '" + arg + "' (voir -h)");
    }
  }

  // Mêmes règles de priorité que miniMD.
  if (nsteps > 0) cfg.nsteps = nsteps;
  if (size > 0) cfg.nx = cfg.ny = cfg.nz = size;
  if (nx > 0) {
    cfg.nx = nx;
    if (ny > 0) cfg.ny = ny;
    else if (size < 0) cfg.ny = nx;
    if (nz > 0) cfg.nz = nz;
    else if (size < 0) cfg.nz = nx;
  } else if ((ny > 0 || nz > 0) && is_root()) {
    std::printf("# Warning: -ny et -nz sont ignores sans -nx, comme dans miniMD\n");
  }

  // --sort absent : tri à chaque reconstruction des listes ; --sort 0 : jamais.
  cfg.sort_every = sort > 0 ? sort : (sort < 0 ? cfg.neigh_every : 0);
  cfg.sort_at_start = sort > 0;

  if (cfg.force_style == ForceStyle::EAM && cfg.ghost_newton == 1) {
    if (is_root()) std::printf("# EAM currently requires '--ghost_newton 0'; Changing setting now.\n");
    cfg.ghost_newton = 0;
  }

  if (cfg.num_threads < 1) fatal("le nombre de threads (-t) doit etre au moins 1");
  if (cfg.ntypes < 1) fatal("le nombre de types (--ntypes) doit etre au moins 1");
  if (cfg.half_neigh < -1 || cfg.half_neigh > 1) fatal("--half_neigh attend -1, 0 ou 1");
  if (cfg.ghost_newton != 0 && cfg.ghost_newton != 1) fatal("--ghost_newton attend 0 ou 1");
  if (cfg.neigh_every < 1) fatal("la periode des listes de voisins doit etre au moins 1");
  if (cfg.thermo_every < 0) fatal("la periode d'affichage ne peut pas etre negative");
  if (cfg.data_file.empty() && (cfg.nx < 1 || cfg.ny < 1 || cfg.nz < 1))
    fatal("la taille du reseau doit etre au moins 1 dans chaque direction");
  if (cfg.neigh_cut <= 0.0) fatal("le rayon des listes de voisins doit etre positif");

  return cfg;
}
