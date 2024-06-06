#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

typedef struct {
    pthread_t thread_id;
    char message[BUFFER_SIZE];
    int has_message;
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
    free(arg);

    while (server_running) {
        pthread_mutex_lock(&mutex);
        while (active_client != client_num && server_running) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (!server_running) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        printf("Client %d, enter your message: ", client_num + 1);
        fgets(clients[client_num].message, BUFFER_SIZE, stdin);
        clients[client_num].has_message = 1;
        active_client = (active_client + 1) % 2;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
        sleep(2); // Attente de 2 secondes pour permettre à l'autre client de recevoir le message
    }

    return NULL;
}

int main() {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    for (int i = 0; i < 2; i++) {
        clients[i].has_message = 0;
        int *client_num = malloc(sizeof(int));
        *client_num = i;
        pthread_create(&clients[i].thread_id, NULL, client_handler, (void *)client_num);
    }

    while (server_running) {
        pthread_mutex_lock(&mutex);
        if (clients[0].has_message) {
            printf("Client 1 says: %s", clients[0].message);
            clients[0].has_message = 0;
        }
        if (clients[1].has_message) {
            printf("Client 2 says: %s", clients[1].message);
            clients[1].has_message = 0;
        }
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
        sleep(1);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(clients[i].thread_id, NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}
