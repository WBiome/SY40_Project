#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> 

#define BUFFER_SIZE 1024
#define PORT 8080

void *receive_handler(void *arg) {
    int sockfd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        // initialisation du buffer avant la reception
        memset(buffer, 0, BUFFER_SIZE);
        // reception du message
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Erreur de réception du message");
            break;
        }
        if (strcmp(buffer, "WRITE") == 0) {
            // Si le signal "WRITE" est reçu
            printf("Entrez votre message: ");
            fflush(stdout);
            // initialisation du buffer avant l'envoi
            memset(buffer, 0, BUFFER_SIZE); 
            // lecture du message
            fgets(buffer, BUFFER_SIZE, stdin);
            // envoi du message
            send(sockfd, buffer, strlen(buffer), 0);
        } else {
            // Message de l'autre client
            printf("Message reçu: %s", buffer);
            printf("En attente de votre tour...\n");
        }
    }
    
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    pthread_t receive_thread;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nErreur de création de socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nAdresse invalide / Adresse non supportée \n");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nÉchec de la connexion \n");
        return -1;
    }

    pthread_create(&receive_thread, NULL, receive_handler, &sockfd);
    pthread_join(receive_thread, NULL);
    close(sockfd);

    return 0;
}