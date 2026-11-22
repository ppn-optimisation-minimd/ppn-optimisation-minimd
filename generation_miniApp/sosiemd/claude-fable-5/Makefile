# sosieMD -- compilation avec le compilateur MPI (mpicxx).
#
#   make                 produit ./sosieMD
#   make clean           supprime le binaire et les fichiers objets
#   make CXXFLAGS="-O2 -g -fopenmp"     remplace les options de compilation
#   make CXX=mpiicpx                    change de compilateur
#
# Les options par defaut sont celles de miniMD dans ce projet : -g, pas
# d'inlining automatique et pointeur de cadre conserve, pour que MAQAO
# attribue le temps aux bonnes fonctions et aux bonnes boucles.

CXX      = mpicxx
CXXFLAGS = -Ofast -g -march=native -fno-inline-functions -fno-omit-frame-pointer -fopenmp
WARNINGS = -Wall -Wextra
LDFLAGS  =

EXE      = sosieMD
BUILD    = build
SOURCES  = $(wildcard src/*.cpp)
OBJECTS  = $(patsubst src/%.cpp,$(BUILD)/%.o,$(SOURCES))

$(EXE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

# -MMD -MP : dependances vers les en-tetes generees par le compilateur.
$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARNINGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(EXE)

.PHONY: clean

-include $(OBJECTS:.o=.d)
