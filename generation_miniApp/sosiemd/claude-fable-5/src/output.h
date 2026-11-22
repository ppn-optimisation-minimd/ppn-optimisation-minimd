// Sorties : en-tête, résumé de performance et fichier YAML, aux formats de miniMD.
#pragma once

#include "atoms.h"
#include "config.h"
#include "domain.h"
#include "neighbor.h"
#include "thermo.h"
#include "timer.h"

// Rappel des paramètres de l'exécution, avant la simulation.
void print_header(const Config& cfg, const Atoms& atoms, const Neighbor& neighbor, const Domain& domain);

// Bloc « # Performance Summary: » et sa ligne PERF_SUMMARY.
// natoms est le nombre d'atomes compté à la fin de la boucle.
void print_performance(const Config& cfg, const Timer& timer, const Domain& domain, int natoms);

// Écrit sosieMD-<date>-<heure>.yaml dans le répertoire courant (et sur la
// sortie standard si cfg.yaml_screen) : paramètres, historique de T, U, P,
// temps, statistiques par processus.
void write_yaml(const Config& cfg, Atoms& atoms, const Neighbor& neighbor, const Domain& domain,
                const Thermo& thermo, const Timer& timer);
