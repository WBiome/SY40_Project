#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 4
#define BUFFER_SIZE 1024
#define PORT 8080

typedef struct {
    int sockfd;
    pthread_t thread_id;
    int other_client_id;
    int client_id;
} client_thread;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int active_client;
    int clients[2];
} client_pair;

client_thread clients[MAX_CLIENTS];
client_pair pairs[MAX_CLIENTS / 2];
int server_running = 1;

void sigusr1_handler(int sig) {
    printf("Server shutting down...\n");
    server_running = 0;
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_cond_broadcast(&pairs[i].cond);
    }
}

void sigusr2_handler(int sig) {
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        printf("Active client in pair %d: %d\n", i + 1, pairs[i].active_client + 1);
    }
}

void *client_handler(void *arg) {
    int client_num = *(int *)arg;
    int sockfd = clients[client_num].sockfd;
    int other_client = clients[client_num].other_client_id;
    int pair_id = client_num / 2; // Each pair has a unique id
    free(arg);
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

        // Signal au client d'écrire
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

        pairs[pair_id].active_client = other_client;
        pthread_cond_signal(&pairs[pair_id].cond);
        pthread_mutex_unlock(&pairs[pair_id].mutex);
    }

    close(sockfd);
    return NULL;
}

int main() {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_mutex_init(&pairs[i].mutex, NULL);
        pthread_cond_init(&pairs[i].cond, NULL);
        pairs[i].active_client = 2 * i; // Initial active client is the first in each pair
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int num_clients = 0;

    while (num_clients < MAX_CLIENTS) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        clients[num_clients].sockfd = new_socket;
        clients[num_clients].client_id = num_clients;
        clients[num_clients].other_client_id = (num_clients % 2 == 0) ? num_clients + 1 : num_clients - 1;
        pairs[num_clients / 2].clients[num_clients % 2] = num_clients;

        int *client_num = malloc(sizeof(int));
        *client_num = num_clients;
        pthread_create(&clients[num_clients].thread_id, NULL, client_handler, (void *)client_num);

        num_clients++;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(clients[i].thread_id, NULL);
    }

    close(server_fd);
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_mutex_destroy(&pairs[i].mutex);
        pthread_cond_destroy(&pairs[i].cond);
    }

    return 0;
}
