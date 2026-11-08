CC = gcc
EXEC = Ceflo

# Le lexer, le parser et l'AST : communs aux deux interpréteurs.
FRONT = $(addprefix src/,lexer.c parser.c ast.c token.c error.c str.c list.c common.c)
# La machine virtuelle : le bytecode, le compilateur et les natives.
VM_SRC = $(wildcard src/vm/*.c src/vm/natives/*.c)
# L'ancien interpréteur, qui parcourt l'AST. Il sert de référence pendant la
# réécriture : `make diff` compare ses sorties à celles de la VM.
REF_SRC = $(filter-out $(FRONT),$(wildcard src/*.c))

HEADERS = $(wildcard src/include/*.h src/vm/*.h)
WARNINGS = -std=c11 -D_GNU_SOURCE -Wall -Wextra
CFLAGS = $(WARNINGS) -O3 -g -flto=auto
LDFLAGS = -rdynamic -lm -ldl
SANITIZE = $(WARNINGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -DCEFLO_NO_POOL
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --main-stacksize=67108864 --error-exitcode=1
VAL_FILE = test.cf

# src/vm passe avant src/include : sur macOS, value.h et Value.h sont le même nom.
VM_INCLUDES = -Isrc/vm -Isrc/include
REF_INCLUDES = -Isrc/include

# Les fichiers .o vont dans build/ : `make BUILD=build/moi` permet à plusieurs
# personnes de compiler en même temps dans le même dossier.
BUILD = build
VM_OBJ = $(patsubst %.c,$(BUILD)/vm/%.o,$(FRONT) $(VM_SRC))
REF_OBJ = $(patsubst %.c,$(BUILD)/ref/%.o,$(FRONT) $(REF_SRC))

all : $(EXEC) $(EXEC)_ref

$(EXEC) : $(VM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EXEC)_ref : $(REF_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/vm/%.o : %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VM_INCLUDES) -o $@ -c $<

$(BUILD)/ref/%.o : %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REF_INCLUDES) -o $@ -c $<

# Les bibliothèques natives : `from @"lib/math/libmath.so" import [mathPi]`
libs : lib/math/libmath.so lib/file/libfile.so

lib/math/libmath.so : lib/math/math.c $(HEADERS)
	$(CC) $(CFLAGS) -fPIC -shared $(VM_INCLUDES) -o $@ $< -lm

lib/file/libfile.so : lib/file/file.c $(HEADERS)
	$(CC) $(CFLAGS) -fPIC -shared $(VM_INCLUDES) -o $@ $<

test : $(EXEC) libs
	tests/run.sh ./$(EXEC)

test-ref : $(EXEC)_ref
	tests/run.sh ./$(EXEC)_ref

# Chaque test et chaque exemple doit donner la même chose avec la VM et avec l'ancien interpréteur.
diff : $(EXEC) $(EXEC)_ref libs
	tests/diff.sh ./$(EXEC) ./$(EXEC)_ref

# La vitesse de la VM comparée à l'ancien interpréteur (et à Python s'il est installé).
bench : $(EXEC) $(EXEC)_ref
	bench/run.sh ./$(EXEC) ./$(EXEC)_ref

# Les tests sous AddressSanitizer, puis avec un GC qui se déclenche à chaque point sûr.
# Avec ce GC, les récursions très profondes sont trop lentes : chaque GC reparcourt toute la pile.
sanitize : libs
	$(CC) $(SANITIZE) $(VM_INCLUDES) -o $(EXEC)_asan $(FRONT) $(VM_SRC) $(LDFLAGS)
	tests/run.sh ./$(EXEC)_asan
	$(CC) $(SANITIZE) $(VM_INCLUDES) -DCEFLO_GC_STRESS -o $(EXEC)_stress $(FRONT) $(VM_SRC) $(LDFLAGS)
	CEFLO_TEST_SKIP="memoire recursion map_grand recursion_profonde recursion_closure recursion_methode" tests/run.sh ./$(EXEC)_stress

# Cherche les fuites de mémoire. Pour un autre script : make val VAL_FILE=exemple/4_listes.cf
# La pile de valgrind doit avoir la même taille que celle demandée par ceflo_stack_init.
# Sans réservoir (CEFLO_NO_POOL), valgrind voit chaque objet et chaque libération.
val : $(FRONT) $(VM_SRC) $(HEADERS)
	$(CC) $(WARNINGS) -g -O1 -DCEFLO_NO_POOL $(VM_INCLUDES) -o $(EXEC)_val $(FRONT) $(VM_SRC) $(LDFLAGS)
	$(VALGRIND) ./$(EXEC)_val $(VAL_FILE)

# Installe la coloration et l'autocomplétion dans Visual Studio Code.
editor :
	python3 editor/vscode/construire.py
	code --install-extension editor/vscode/ceflo.vsix --force

editor-test :
	node --test editor/vscode/test/analyse.test.js
	editor/vscode/test/lancer.sh

clean :
	-rm -rf build src/*.o lib/*/*.so $(EXEC) $(EXEC)_ref $(EXEC)_asan $(EXEC)_stress $(EXEC)_val editor/vscode/ceflo.vsix

.PHONY : all libs test test-ref diff bench sanitize val editor editor-test clean
