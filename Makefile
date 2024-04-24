
CC = gcc
CCC = i686-w64-mingw32-gcc
EXEC = Ceflo
FILE = test.cf
DEBUG = -DDEBUG_CEFLO
LINUX = -DLINUX
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
CFLAGS= -Wextra -std=c11 -pedantic -ggdb $(LINUX)

TEST=/home/prog/Bureau/PROG/ceflo_ast/bin

all : $(EXEC)
%.o : %.c include/*.h
	$(CC) $(CFLAGS) -o $@ -c $< -lm
$(EXEC):$(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

cls :
	-rm src/*.o
	-rm $(EXEC)

publish :
	cp ./$(EXEC)  $(TEST)/Linux
	cp ./$(EXEC).exe  $(TEST)/Win


test :
	-make
	./$(EXEC) $(FILE)
val :
	-make
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXEC) $(FILE)