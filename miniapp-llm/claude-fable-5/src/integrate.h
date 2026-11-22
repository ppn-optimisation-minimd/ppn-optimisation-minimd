// Boucle en temps : intégration de Verlet (forme vitesse).
#pragma once

#include "atoms.h"
#include "config.h"
#include "domain.h"
#include "force.h"
#include "neighbor.h"
#include "thermo.h"
#include "timer.h"

// Avance le système de cfg.nsteps pas et remplit les chronomètres, dont le
// temps total de la boucle. Les forces doivent être à jour à l'entrée ; elles
// le sont à la sortie.
void run(const Config& cfg, Atoms& atoms, Force& force, Neighbor& neighbor, Domain& domain, Thermo& thermo,
         Timer& timer);
