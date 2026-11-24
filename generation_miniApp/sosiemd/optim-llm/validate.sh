#!/usr/bin/env bash
# Validation de sosieMD : compare ses resultats a ceux de miniMD (variante ref).
#
#   ./validate.sh            toutes les comparaisons (environ 3 minutes)
#   ./validate.sh --quick    sans le cas a 4 millions d'atomes (environ 2 minutes)
#
# Le script recompile sosieMD, lance les deux programmes avec les memes
# fichiers d'entree et les memes options, puis compare les lignes du tableau
# thermodynamique (pas, T, U, P) :
#   - au pas 0, les valeurs affichees doivent etre identiques ;
#   - aux pas suivants, l'ecart relatif doit rester sous la tolerance (1e-6).
# Il sort avec un code de retour non nul si une comparaison echoue.
#
# Tout est execute dans le sous-dossier validation/ (journaux dans
# validation/logs/). Variables d'environnement reconnues :
#   MINIMD   chemin de l'executable miniMD (defaut : baseline/ref/miniMD_openmpi)
#   MPIRUN   commande de lancement MPI (defaut : mpirun)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
MINIMD="${MINIMD:-$REPO/baseline/ref/miniMD_openmpi}"
MPIRUN="${MPIRUN:-mpirun}"
SOSIE="$HERE/sosieMD"
WORK="$HERE/validation"
LOGS="$WORK/logs"
TOLERANCE=1e-6

QUICK=0
for arg in "$@"; do
  case "$arg" in
    --quick) QUICK=1 ;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "option inconnue : $arg (voir --help)" >&2; exit 2 ;;
  esac
done

failures=0
comparisons=0

fail() { echo "  ECHEC : $*"; failures=$((failures + 1)); }

# ---------------------------------------------------------------------------
# Preparation
# ---------------------------------------------------------------------------

[ -x "$MINIMD" ] || { echo "miniMD introuvable : $MINIMD" >&2; exit 2; }

echo "== Compilation de sosieMD (make clean, make) =="
make -C "$HERE" clean > /dev/null
if ! make -C "$HERE" -j 8 > "$HERE/build.log" 2>&1; then
  cat "$HERE/build.log"; rm -f "$HERE/build.log"
  echo "ECHEC : la compilation a echoue"; exit 1
fi
comparisons=$((comparisons + 1))
if grep -qi "warning" "$HERE/build.log"; then
  grep -i -A3 "warning" "$HERE/build.log"
  fail "la compilation produit des avertissements"
else
  echo "  OK : compilation sans avertissement (-Wall -Wextra)"
fi
rm -f "$HERE/build.log"

rm -rf "$WORK"
mkdir -p "$LOGS"
ln -s "$REPO/baseline/ref/Cu_u6.eam" "$WORK/Cu_u6.eam"
cp "$REPO/launcher/in.param_analyse.miniMD" "$REPO/baseline/ref/in.eam.miniMD" "$WORK/"

# Petit fichier de donnees LAMMPS : reseau cfc de n^3 mailles, positions
# legerement perturbees, vitesses aleatoires. Le fichier 1m.data cite par les
# fichiers d'entree de miniMD n'est pas dans le depot.
#   make_data <mailles> <parametre de maille> <perturbation> <vitesse max> <masse>
make_data() {
  awk -v n="$1" -v a="$2" -v disp="$3" -v vel="$4" -v mass="$5" 'BEGIN {
    srand(12345); L = n * a; natoms = 4 * n * n * n
    print "LAMMPS data file: perturbed fcc lattice for the sosieMD validation\n"
    print natoms " atoms"; print "1 atom types\n"
    printf "0.0 %.10f xlo xhi\n0.0 %.10f ylo yhi\n0.0 %.10f zlo zhi\n\n", L, L, L
    print "Masses\n"; printf "1 %s\n\n", mass
    print "Atoms\n"
    split("0 0.5 0.5 0", bx, " "); split("0 0.5 0 0.5", by, " "); split("0 0 0.5 0.5", bz, " ")
    id = 0
    for (k = 0; k < n; k++) for (j = 0; j < n; j++) for (i = 0; i < n; i++) for (b = 1; b <= 4; b++) {
      id++
      printf "%d 1 %.10f %.10f %.10f\n", id, (i + bx[b]) * a + disp * rand(),
             (j + by[b]) * a + disp * rand(), (k + bz[b]) * a + disp * rand()
    }
    print "\nVelocities\n"
    for (id = 1; id <= natoms; id++)
      printf "%d %.10f %.10f %.10f\n", id, vel * (2 * rand() - 1), vel * (2 * rand() - 1), vel * (2 * rand() - 1)
  }'
}
make_data 8 1.6795962 0.1 1.5 1.0 > "$WORK/lj.data"      # Lennard-Jones, densite 0.8442
make_data 8 3.615 0.2 5.0 63.55 > "$WORK/cu.data"        # cuivre, unites metal
# Fichier d'entree qui nomme le fichier de donnees a sa ligne 4.
sed '4s/.*/lj.data        data file (none or filename)/' "$WORK/in.param_analyse.miniMD" > "$WORK/in.lj-data.miniMD"

# ---------------------------------------------------------------------------
# Outils
# ---------------------------------------------------------------------------

# run_md <journal> <processus> <threads> <executable> <options...>
# Avec plusieurs threads, --bind-to none evite que mpirun ne les lie tous au
# meme coeur.
run_md() {
  local log=$1 np=$2 nt=$3 exe=$4
  shift 4
  local bind=()
  [ "$nt" -gt 1 ] && bind=(--bind-to none)
  (cd "$WORK" && "$MPIRUN" "${bind[@]}" -np "$np" "$exe" "$@" -t "$nt") > "$log" 2>&1
}

# Nom de journal propre a (programme, processus, threads, options).
log_name() {
  local prog=$1 np=$2 nt=$3
  shift 3
  echo "$LOGS/${prog}_np${np}_t${nt}_$(echo "$*" | md5sum | cut -c1-10).log"
}

# Lignes « pas T U P » du tableau thermodynamique d'un journal.
thermo_lines() {
  awk '/^# Timestep T U P Time/ { on = 1; next }
       on && NF == 0 { on = 0 }
       on && NF == 5 && $1 ~ /^[0-9]+$/ { print $1, $2, $3, $4 }' "$1"
}

table_header() {
  printf "\n== %s ==\n" "$1"
  printf "%-36s %5s  %13s %13s  %13s %13s  %13s %13s  %9s\n" \
    "cas" "pas" "T miniMD" "T sosieMD" "U miniMD" "U sosieMD" "P miniMD" "P sosieMD" "ecart rel"
}

# compare <nom> <np miniMD> <threads miniMD> <np sosieMD> <threads sosieMD> <options...>
# Lance miniMD (une seule fois par jeu d'options) et sosieMD, puis affiche une
# ligne par pas avec le plus grand ecart relatif sur T, U et P.
compare() {
  local name=$1 rnp=$2 rnt=$3 snp=$4 snt=$5
  shift 5
  local ref_log sos_log
  ref_log=$(log_name miniMD "$rnp" "$rnt" "$@")
  sos_log=$(log_name sosieMD "$snp" "$snt" "$@")
  [ -f "$ref_log" ] || run_md "$ref_log" "$rnp" "$rnt" "$MINIMD" "$@"
  run_md "$sos_log" "$snp" "$snt" "$SOSIE" "$@"

  comparisons=$((comparisons + 1))
  if ! paste -d' ' <(thermo_lines "$ref_log") <(thermo_lines "$sos_log") |
    awk -v name="$name" -v tol="$TOLERANCE" '
      function reldiff(a, b,   m) {
        m = (a < 0 ? -a : a); if ((b < 0 ? -b : b) > m) m = (b < 0 ? -b : b)
        return m == 0 ? 0 : (a > b ? a - b : b - a) / m
      }
      NF != 8 || $1 != $5 { printf "%-36s les pas affiches different : \"%s\"\n", name, $0; bad = 1; next }
      {
        worst = reldiff($2, $6)
        if (reldiff($3, $7) > worst) worst = reldiff($3, $7)
        if (reldiff($4, $8) > worst) worst = reldiff($4, $8)
        if ($1 == 0) {
          # Au pas 0 : egalite de tous les chiffres affiches.
          ok = ($2 == $6 && $3 == $7 && $4 == $8); note = ok ? "OK (identique)" : "ECHEC (pas 0 different)"
        } else {
          ok = (worst <= tol); note = ok ? "OK" : "ECHEC (> " tol ")"
        }
        if (!ok) bad = 1
        printf "%-36s %5d  %13s %13s  %13s %13s  %13s %13s  %9.1e  %s\n", name, $1, $2, $6, $3, $7, $4, $8, worst, note
        lines++
      }
      END { if (lines == 0) { printf "%-36s aucune ligne thermodynamique\n", name; bad = 1 } exit bad }'; then
    failures=$((failures + 1))
    echo "  ECHEC : $name (journaux : $ref_log $sos_log)"
  fi
}

# Temps t_total, t_force, t_neigh de la ligne PERF_SUMMARY d'un journal.
perf_times() { awk '$12 == "PERF_SUMMARY" { printf "%9.3f %9.3f %9.3f", $5, $6, $7 }' "$1"; }

# ---------------------------------------------------------------------------
# 1. Les trois cas de validation, un processus et un thread
# ---------------------------------------------------------------------------

SHORT=(-i in.param_analyse.miniMD -s 20 -n 200)
GROUP=(-i in.param_analyse.miniMD -s 100)
EAM=(-i in.eam.miniMD -s 20 -n 100)

table_header "Cas de validation (1 processus, 1 thread)"
compare "court (LJ, 32 000 atomes)" 1 1 1 1 "${SHORT[@]}"
compare "EAM (32 000 atomes)" 1 1 1 1 "${EAM[@]}"
if [ "$QUICK" -eq 0 ]; then
  compare "groupe (LJ, 4 000 000 atomes)" 1 1 1 1 "${GROUP[@]}"
else
  echo "groupe (LJ, 4 000 000 atomes)        saute (--quick)"
fi

# ---------------------------------------------------------------------------
# 2. Independance au nombre de threads et de processus
#    Reference : miniMD avec 1 processus et 1 thread. (miniMD ref ne calcule
#    pas les forces EAM en demi-listes avec plusieurs threads.)
# ---------------------------------------------------------------------------

table_header "Threads et processus : sosieMD (np, t) contre miniMD (1, 1)"
for config in "1 4" "4 1" "2 2"; do
  set -- $config
  compare "court  sosieMD -np $1 -t $2" 1 1 "$1" "$2" "${SHORT[@]}"
done
for config in "1 4" "4 1" "2 2"; do
  set -- $config
  compare "EAM    sosieMD -np $1 -t $2" 1 1 "$1" "$2" "${EAM[@]}"
done

# ---------------------------------------------------------------------------
# 3. Options qui changent le chemin de calcul (memes options des deux cotes)
# ---------------------------------------------------------------------------

LJ100=(-i in.param_analyse.miniMD -s 20 -n 100)
table_header "Options (memes options, meme nombre de processus et de threads)"
compare "listes completes" 1 1 1 1 "${LJ100[@]}" --half_neigh 0
compare "listes completes, -np 4" 4 1 4 1 "${LJ100[@]}" --half_neigh 0
compare "listes completes, -t 4" 1 4 1 4 "${LJ100[@]}" --half_neigh 0
compare "listes completes, EAM" 1 1 1 1 "${EAM[@]}" --half_neigh 0
compare "listes completes, EAM, -np 2 -t 2" 2 2 2 2 "${EAM[@]}" --half_neigh 0
compare "sans Newton sur les fantomes" 1 1 1 1 "${LJ100[@]}" -gn 0
compare "sans Newton fantomes, -np 4" 4 1 4 1 "${LJ100[@]}" -gn 0
compare "demi-listes d'origine (-1)" 1 1 1 1 "${LJ100[@]}" --half_neigh -1
compare "sans tri (--sort 0)" 1 1 1 1 "${LJ100[@]}" --sort 0
compare "tri tous les 50 pas, -np 4" 4 1 4 1 "${LJ100[@]}" --sort 50
compare "grille imposee (-b 8)" 1 1 1 1 "${LJ100[@]}" -b 8
compare "grille imposee (-b 30), -np 2" 2 1 2 1 "${LJ100[@]}" -b 30
compare "-nx 10 -ny 20 -nz 30" 1 1 1 1 -i in.param_analyse.miniMD -nx 10 -ny 20 -nz 30 -n 100
compare "-nx 30 -ny 8 -nz 8, -np 4" 4 1 4 1 -i in.param_analyse.miniMD -nx 30 -ny 8 -nz 8 -n 100
compare "un seul type (--ntypes 1)" 1 1 1 1 "${LJ100[@]}" --ntypes 1
compare "petite boite (-s 2)" 1 1 1 1 -i in.param_analyse.miniMD -s 2 -n 100
compare "-np 3" 3 1 3 1 "${LJ100[@]}"
compare "-np 8" 8 1 8 1 "${LJ100[@]}"
compare "LJ force par -p lj -u lj" 1 1 1 1 -i in.eam.miniMD -s 12 -n 60 -p lj -u lj

# ---------------------------------------------------------------------------
# 4. Lecture d'un fichier de donnees LAMMPS
# ---------------------------------------------------------------------------

table_header "Fichier de donnees LAMMPS (2 048 atomes)"
compare "LJ, option -f" 1 1 1 1 -i in.param_analyse.miniMD -f lj.data -n 100
compare "LJ, nomme dans le fichier d'entree" 1 1 1 1 -i in.lj-data.miniMD -n 100
compare "LJ, -f, -np 4 -t 2" 4 2 4 2 -i in.param_analyse.miniMD -f lj.data -n 100
compare "EAM, -f" 1 1 1 1 -i in.eam.miniMD -f cu.data -n 100
compare "EAM, -f, -np 4" 4 1 4 1 -i in.eam.miniMD -f cu.data -n 100

# ---------------------------------------------------------------------------
# 5. Sortie YAML (-o 1 --yaml_screen), 4 processus
#    Les deux fichiers doivent avoir les memes lignes, hors temps mesures et
#    hors deux differences connues : le nom du programme (variant) et
#    force_params, que miniMD imprime a partir de pointeurs.
# ---------------------------------------------------------------------------

printf "\n== Sortie YAML (-np 4, -o 1 --yaml_screen) ==\n"
normalize_yaml() {
  awk '/^  variant:/ || /^  force_params:/ { next }
       /^    time:/ || /^    performance/ || /^  (force|neigh|comm|other):/ { sub(/:.*/, ": <temps>"); print; next }
       /^# (Force|Neigh|Comm |Other) time:/ { sub(/:.*/, ": <temps>"); print; getline; print "# Histogram: <temps>"; next }
       { print }' "$@"
}
for prog in miniMD sosieMD; do
  exe=$MINIMD
  [ "$prog" = sosieMD ] && exe=$SOSIE
  mkdir -p "$WORK/yaml_$prog"
  (cd "$WORK/yaml_$prog" && "$MPIRUN" -np 4 "$exe" -i ../in.param_analyse.miniMD -s 20 -n 150 -o 1 --yaml_screen) \
    > "$LOGS/yaml_$prog.log" 2>&1
  normalize_yaml "$WORK/yaml_$prog"/*.yaml > "$LOGS/yaml_$prog.file" 2> /dev/null
  sed -n '/^run_configuration:/,$p' "$LOGS/yaml_$prog.log" | normalize_yaml > "$LOGS/yaml_$prog.screen"
done
comparisons=$((comparisons + 2))
ls "$WORK"/yaml_sosieMD/sosieMD-*.yaml > /dev/null 2>&1 || fail "sosieMD n'a pas ecrit de fichier sosieMD-<date>.yaml"
for part in file screen; do
  label="fichier"
  [ "$part" = screen ] && label="sortie standard"
  if [ -s "$LOGS/yaml_miniMD.$part" ] && diff "$LOGS/yaml_miniMD.$part" "$LOGS/yaml_sosieMD.$part" > "$LOGS/yaml_$part.diff"; then
    echo "  OK : YAML ($label) identique a celui de miniMD, hors temps, variant et force_params" \
      "($(wc -l < "$LOGS/yaml_sosieMD.$part") lignes comparees)"
  else
    cat "$LOGS/yaml_$part.diff" 2> /dev/null
    fail "YAML ($label) different de celui de miniMD"
  fi
done
echo "  Valeurs a 10 chiffres du dernier pas (miniMD puis sosieMD) :"
for prog in miniMD sosieMD; do
  printf "    %-8s %s\n" "$prog" "$(grep -A3 'timestep: 150' "$LOGS/yaml_$prog.file" | awk '{ printf "%s ", $NF }')"
done

# ---------------------------------------------------------------------------
# 6. Temps (indicatifs) des trois cas, 1 processus et 1 thread
# ---------------------------------------------------------------------------

printf "\n== Temps en secondes (1 processus, 1 thread, une seule execution) ==\n"
printf "%-32s %-8s %9s %9s %9s\n" "cas" "" "t_total" "t_force" "t_neigh"
timing_row() {
  local name=$1
  shift
  printf "%-32s %-8s %s\n" "$name" "miniMD" "$(perf_times "$(log_name miniMD 1 1 "$@")")"
  printf "%-32s %-8s %s\n" "" "sosieMD" "$(perf_times "$(log_name sosieMD 1 1 "$@")")"
}
timing_row "court (LJ, 32 000 atomes)" "${SHORT[@]}"
timing_row "EAM (32 000 atomes)" "${EAM[@]}"
[ "$QUICK" -eq 0 ] && timing_row "groupe (LJ, 4 000 000 atomes)" "${GROUP[@]}"

# ---------------------------------------------------------------------------
# Bilan
# ---------------------------------------------------------------------------

echo
if [ "$failures" -eq 0 ]; then
  echo "VALIDATION REUSSIE : $comparisons verifications, aucun echec."
  exit 0
fi
echo "VALIDATION EN ECHEC : $failures echec(s) sur $comparisons verifications."
exit 1
