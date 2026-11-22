// Création du système : réseau cubique à faces centrées et vitesses
// initiales, ou lecture d'un fichier de données LAMMPS.
#pragma once

#include <string>
#include <vector>

#include "atoms.h"
#include "thermo.h"

// Dimensions de la boîte contenant nx*ny*nz mailles cfc à la densité rho.
void create_box(Box& box, int nx, int ny, int nz, double rho);

// Place sur le réseau les atomes qui tombent dans le sous-domaine de ce
// processus, avec une vitesse pseudo-aléatoire propre à chaque site.
void create_atoms(Atoms& atoms, int nx, int ny, int nz, double rho);

// Annule la vitesse du centre de masse puis met les vitesses à l'échelle
// pour obtenir la température t_request.
void create_velocity(double t_request, Atoms& atoms, const Thermo& thermo);

// Contenu utile d'un fichier de données LAMMPS (style atomique).
struct DataFile {
  int natoms = 0;
  double prd[3] = {0.0, 0.0, 0.0};  // longueurs de la boîte
  double mass = 0.0;                // 0 : pas de section Masses
  std::vector<double> x, v;         // 3 valeurs par atome, rangées par identifiant
};

DataFile read_data_file(const std::string& path);

// Ajoute les atomes du fichier qui tombent dans le sous-domaine de ce processus.
void add_atoms_from_data(const DataFile& data, Atoms& atoms);
