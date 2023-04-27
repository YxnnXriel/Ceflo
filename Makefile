
CC = gcc
CCC = i686-w64-mingw32-gcc
EXEC = Ceflo
FILE = test.cf
DEBUG = -DDEBUG_CEFLO
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
CFLAGS= -Wextra -std=c11 -pedantic -ggdb 

all : $(EXEC)
%.o : %.c include/*.h
	$(CC) $(CFLAGS) -o $@ -c $< -lm
$(EXEC):$(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

cls :
	-rm src/*.o
	-rm $(EXEC)

publish :
	cp ./$(EXEC)  CefloV2/Linux

test :
	-make
	./$(EXEC) $(FILE)
val :
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXEC) $(FILE)