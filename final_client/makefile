CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
SRC = headers.h qr_client.c
OBJ = qr_client.o
EXEC = QRclient

all: $(EXEC)
	echo "Successfully built the program $(EXEC)"

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

qr_client.o: qr_client.c headers.h
	$(CC) $(CFLAGS) -c qr_client.c

clean:
	rm -f $(OBJ) $(EXEC)
	echo "Clean up completed"