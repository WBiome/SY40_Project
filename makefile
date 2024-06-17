# Variables
CC = gcc
CFLAGS = -Wall -pthread
TARGETS = server client

# Cibles par défaut
all: clean $(TARGETS)

# Compilation du serveur
server: server.o
	$(CC) $(CFLAGS) -o server server.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

# Compilation du client
client: client.o
	$(CC) $(CFLAGS) -o client client.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

# Nettoyage des fichiers objets et exécutables
clean:
	rm -f *.o $(TARGETS)


