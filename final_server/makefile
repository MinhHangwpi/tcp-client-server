CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
SRC = headers.h server.c
OBJ = server.o
EXEC = QRserver

all: $(EXEC)
	echo "Successfully built the program $(EXEC)"

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

server.o: server.c headers.h
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f $(OBJ) $(EXEC)
	echo "Clean up completed"