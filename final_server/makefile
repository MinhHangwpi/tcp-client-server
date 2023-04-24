CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
SRC = headers.h functions.c main.c
OBJ = functions.o main.o
EXEC = server

all: $(EXEC)
	echo "Successfully built the program $(EXEC)"

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

functions.o: functions.c headers.h
	$(CC) $(CFLAGS) -c functions.c

main.o: main.c headers.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f $(OBJ) $(EXEC)
	echo "Clean up completed"