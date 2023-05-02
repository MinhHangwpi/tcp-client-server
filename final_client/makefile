CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
SRC = headers.h client.c
OBJ = client.o
EXEC = QRclient

all: $(EXEC)
	echo "Successfully built the program $(EXEC)"

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

client.o: client.c headers.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f $(OBJ) $(EXEC)
	echo "Clean up completed"