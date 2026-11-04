CC = gcc
EXEC = Ceflo
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
HEADERS = $(wildcard src/include/*.h)
CFLAGS = -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto
LDFLAGS = -rdynamic -lm -ldl
SANITIZE = -std=c11 -D_GNU_SOURCE -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -DCEFLO_NO_POOL
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --main-stacksize=67108864 --error-exitcode=1
VAL_FILE = test.cf

all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

# Les bibliothèques natives : `from @"lib/math/libmath.so" import [mathPi]`
libs : lib/math/libmath.so lib/file/libfile.so

lib/math/libmath.so : lib/math/math.c $(HEADERS)
	$(CC) $(CFLAGS) -fPIC -shared -Isrc/include -o $@ $< -lm

lib/file/libfile.so : lib/file/file.c $(HEADERS)
	$(CC) $(CFLAGS) -fPIC -shared -Isrc/include -o $@ $<

test : $(EXEC) libs
	tests/run.sh ./$(EXEC)

# Les tests sous AddressSanitizer, puis avec un GC qui se déclenche à chaque allocation
sanitize : libs
	$(CC) $(SANITIZE) -o $(EXEC)_asan $(SRC) $(LDFLAGS)
	tests/run.sh ./$(EXEC)_asan
	$(CC) $(SANITIZE) -DCEFLO_GC_STRESS -o $(EXEC)_stress $(SRC) $(LDFLAGS)
	CEFLO_TEST_SKIP="memoire recursion map_grand" tests/run.sh ./$(EXEC)_stress

# Cherche les fuites de mémoire. Pour un autre script : make val VAL_FILE=exemple/4_listes.cf
# La pile de valgrind doit avoir la même taille que celle demandée par ceflo_stack_init.
# Sans réservoir (CEFLO_NO_POOL), valgrind voit chaque objet et chaque libération.
val : $(SRC) $(HEADERS)
	$(CC) -std=c11 -D_GNU_SOURCE -g -O1 -DCEFLO_NO_POOL -o $(EXEC)_val $(SRC) $(LDFLAGS)
	$(VALGRIND) ./$(EXEC)_val $(VAL_FILE)

# Installe la coloration et l'autocomplétion dans Visual Studio Code.
editor :
	python3 editor/vscode/construire.py
	code --install-extension editor/vscode/ceflo.vsix --force

editor-test :
	node --test editor/vscode/test/analyse.test.js
	editor/vscode/test/lancer.sh

clean :
	-rm -f src/*.o lib/*/*.so $(EXEC) $(EXEC)_asan $(EXEC)_stress $(EXEC)_val editor/vscode/ceflo.vsix

.PHONY : all libs test sanitize val editor editor-test clean
