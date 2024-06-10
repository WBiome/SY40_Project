#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PORT 8080

typedef struct {
    int sockfd;
    pthread_t thread_id; 
} client_thread;

pthread_mutex_t mutex;
pthread_cond_t cond;
client_thread clients[2];
int active_client = 0;
int server_running = 1;

void sigusr1_handler(int sig) {
    printf("Server shutting down...\n");
    server_running = 0;
    pthread_cond_broadcast(&cond);
}

void sigusr2_handler(int sig) {
    printf("Active client: %d\n", active_client + 1);
}

void *client_handler(void *arg) {
    int client_num = *(int *)arg;
    int sockfd = clients[client_num].sockfd;
    free(arg);
    char buffer[BUFFER_SIZE];

    while (server_running) {
        pthread_mutex_lock(&mutex);
        while (active_client != client_num && server_running) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (!server_running) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Signal the client to write
        send(sockfd, "WRITE", 5, 0);

        // Receive the client's message
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Send the message to the other client
        int other_client = (client_num + 1) % 2;
        send(clients[other_client].sockfd, buffer, bytes_received, 0);

        active_client = other_client;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    close(sockfd);
    return NULL;
}

int main() {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

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

    if (listen(server_fd, 2) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 2; i++) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        clients[i].sockfd = new_socket;
        int *client_num = malloc(sizeof(int));
        *client_num = i;
        pthread_create(&clients[i].thread_id, NULL, client_handler, (void *)client_num);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(clients[i].thread_id, NULL);
    }

    close(server_fd);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}
