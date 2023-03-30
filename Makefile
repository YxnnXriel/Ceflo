
CC = gcc
EXEC = Ceflo
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

test :
	-make
	./$(EXEC) text.cf
val :
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXEC) text.cf