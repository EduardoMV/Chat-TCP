#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include "security.h"

#define PORT 8080
#define MAXLINE 1024
#define KEY 10

#define CONTROLLER_PORT 3000
#define CONTROLLER_IP "127.0.0.1"
#define CONTROLLER_BUFFER_SIZE 1024

int main()
{

    // -------------------- TCP CLIENT <-> CONTROLLER ----------------------------
    int controllerfd;
    struct sockaddr_in controller_addr;
    char controller_buffer[CONTROLLER_BUFFER_SIZE];
    ssize_t controller_n;

    // ---------------------------------------------------------------------------

    // ---------------------  UDP SERVER <-> Client ------------------------------
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
    int listenfd, len, n;

    char clientIP[INET_ADDRSTRLEN];
    int clientPORT;
    // ---------------------------------------------------------------------------

    printf("Listening in port number: %d\n", PORT);
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (listenfd == -1)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    len = sizeof(cliaddr);

    pid_t child_pid;

    struct sockaddr_in connectedClients[100];
    int clientCount = 0;

    while (1)
    {
        n = recvfrom(listenfd, buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);

        int prevConnected = 0;
        for (int cc = 0; cc < clientCount; cc++)
        {
            if (connectedClients[cc].sin_addr.s_addr != cliaddr.sin_addr.s_addr)
                continue;
            if (connectedClients[cc].sin_port != cliaddr.sin_port)
                continue;
            prevConnected = 1;
            break;
        }
        if (prevConnected == 0)
        {
            connectedClients[clientCount] = cliaddr;
            clientCount += 1;
            printf("new connection\n");
        }

        child_pid = fork();
        if (child_pid == 0)
        {
            break;
        }
    }
    if (child_pid == 0)
    {
        if (n < 0)
        {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
            close(listenfd);
            return 1;
        }
        buffer[n] = '\0';
        char *decoded = decrypt(buffer, KEY);

        if (strcmp(decoded, "ping") == 0)
        {
            char *pong = encrypt("pong", KEY);
            sendto(listenfd, pong, strlen(pong), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            close(listenfd);

            printf("==== NEW PING RECIEVED ====\n");
            printf("from: %d:%d\n", cliaddr.sin_addr.s_addr, cliaddr.sin_port);
            printf("===========================\n");

            return 0;
        }

        // Print the buffer content
        printf("Buffer content: %s\n", buffer);

        printf("\n==== NEW UDP REQ RECIEVED ====\n");
        printf("from: %d:%d\n", cliaddr.sin_addr.s_addr, cliaddr.sin_port);
        printf("rcv:  %s\n", buffer);
        printf("data: %s\n", decoded);

        // ############### CONECTION WITH THE CONTROLLER ################
        controllerfd = socket(AF_INET, SOCK_STREAM, 0);
        if (controllerfd < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Set up server address
        memset(&controller_addr, 0, sizeof(controller_addr));
        controller_addr.sin_family = AF_INET;
        controller_addr.sin_port = htons(CONTROLLER_PORT);
        if (inet_pton(AF_INET, CONTROLLER_IP, &controller_addr.sin_addr) <= 0)
        {
            perror("invalid address/ Address not supported");
            close(controllerfd);
            exit(EXIT_FAILURE);
        }

        if (connect(controllerfd, (struct sockaddr *)&controller_addr, sizeof(controller_addr)) < 0)
        {
            perror("connection failed");
            close(controllerfd);
            exit(EXIT_FAILURE);
        }

        send(controllerfd, decoded, strlen(decoded), 0);
        controller_n = recv(controllerfd, controller_buffer, CONTROLLER_BUFFER_SIZE - 1, 0);

        if (controller_n < 0)
        {
            perror("recv failed");
            char *error_msg = encrypt("Error in controller", KEY);
            sendto(listenfd, error_msg, strlen(error_msg), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            close(listenfd);
            return 1;
        }
        // ####################################################################3

        controller_buffer[controller_n] = '\0';
        char *encoded = encrypt(controller_buffer, KEY);

        for (int i = 0; i < clientCount; i++)
        {
            sendto(listenfd, encoded, strlen(encoded), 0, (struct sockaddr *)&connectedClients[i], sizeof(cliaddr));
        }

        printf("=========== RESPONSE ===========\n");
        printf("to:    %d:%d\n", cliaddr.sin_addr.s_addr, cliaddr.sin_port);
        printf("data:  %s\n", controller_buffer);
        printf("send:  %s\n", encoded);
        printf("================================\n");

        close(listenfd);
        close(controllerfd);
    }
    else
    {
        close(listenfd);
    }
    return 0;
}