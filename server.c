#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// MAX_CLIENTS attend un nombre pair
#define MAX_CLIENTS 4  
#define BUFFER_SIZE 1024
#define PORT 8080

// Structure pour les threads clients
typedef struct {
    int sockfd;
    pthread_t thread_id;
    int other_client_id;
    int client_id;
} client_thread;

// Structure pour les paires de clients
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int active_client;
    int clients[2];
} client_pair;

client_thread clients[MAX_CLIENTS];
client_pair pairs[MAX_CLIENTS / 2];
int server_running = 1;
int num_clients = 0;

void sigusr1_handler(int sig) {
    printf("Fermeture des toutes les connexions et fermeture du serveur\n");
    server_running = 0;
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_cond_broadcast(&pairs[i].cond);
    }
}

void sigusr2_handler(int sig) {
    printf("Nombre de clients connectés: %d\n", num_clients);
}

void signal_handler() {
    struct sigaction sa1, sa2;
    sa1.sa_handler = sigusr1_handler;
    sa2.sa_handler = sigusr2_handler;
    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGUSR2, &sa2, NULL);
    sigaction(SIGINT, &sa1, NULL);
    sigaction(SIGTSTP, &sa1, NULL);
}

// Fonction de gestion des clients
void *client_handler(void *arg) {
    int client_num = *(int *)arg;
    int sockfd = clients[client_num].sockfd;
    int other_client = clients[client_num].other_client_id;
    int pair_id = client_num / 2;
    free(arg);  // Libérer la mémoire allouée pour le numéro du client
    char buffer[BUFFER_SIZE];

    while (server_running) {
        pthread_mutex_lock(&pairs[pair_id].mutex);
        while (pairs[pair_id].active_client != client_num && server_running) { 
            pthread_cond_wait(&pairs[pair_id].cond, &pairs[pair_id].mutex);
        }
        if (!server_running) {
            pthread_mutex_unlock(&pairs[pair_id].mutex);
            break;
        }

        // Envoyer un signal "WRITE" au client pour qu'il écrive un message
        send(sockfd, "WRITE", 5, 0);

        // Réception du message du client
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            pthread_mutex_unlock(&pairs[pair_id].mutex);
            break;
        }

        // Envoyer le message à l'autre client
        send(clients[other_client].sockfd, buffer, bytes_received, 0);

        // Passer le tour à l'autre client
        pairs[pair_id].active_client = other_client;
        pthread_cond_signal(&pairs[pair_id].cond);
        pthread_mutex_unlock(&pairs[pair_id].mutex);
    }

    close(sockfd);
    return NULL;
}

int main() {
    // Installation des handlers pour SIGUSR1 et SIGUSR2
    signal_handler();

    // Initialisation des mutex et des variables de condition pour chaque paire de clients
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_mutex_init(&pairs[i].mutex, NULL);
        pthread_cond_init(&pairs[i].cond, NULL);
        pairs[i].active_client = 2 * i;
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Création de la socket serveur
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Définition de l'adresse et du port du serveur
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Liaison de la socket à l'adresse et au port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Ecoute des connexions entrantes
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Boucle principale pour accepter les connexions des clients
    while (num_clients < MAX_CLIENTS) {
        // Accepter une nouvelle connexion
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // Initialisation des informations du client
        clients[num_clients].sockfd = new_socket;
        clients[num_clients].client_id = num_clients;
        clients[num_clients].other_client_id = (num_clients % 2 == 0) ? num_clients + 1 : num_clients - 1;
        pairs[num_clients / 2].clients[num_clients % 2] = num_clients;

        // Création d'un thread pour gérer le client
        int *client_num = malloc(sizeof(int));
        *client_num = num_clients;
        pthread_create(&clients[num_clients].thread_id, NULL, client_handler, (void *)client_num);

        num_clients++;
    }

    // Attendre la fin de tous les threads clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(clients[i].thread_id, NULL);
    }

    // Fermeture de la socket serveur
    close(server_fd);

    // Destruction des mutex et des variables de condition
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_mutex_destroy(&pairs[i].mutex);
        pthread_cond_destroy(&pairs[i].cond);
    }

    return 0;
}
