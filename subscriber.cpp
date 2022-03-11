#include "helpers.h"

void usage(char *file)
{
    fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER> \n", file);
    exit(0);
}

int main(int argc, char *argv[]) {

    if(argc != 4) {
        usage (argv[0]);
    }

    fflush(stdout);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd, n, ret, flag = 1;
    char buffer[CLIENTBUFFLEN];
    sockaddr_in serv_addr;
    msg_action msg;
    fd_set read_set, tmp_set;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Can't create the socket!");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "Invalid IP address!");

    ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Can't connect to server!");

    ret = send(sockfd, argv[1], strlen(argv[1]), 0);
    DIE( ret < 0, "Can't send the Client ID to the server!");

    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(ret < 0, "Can't disable Nagle's algorithm!");

    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    FD_SET(sockfd, &read_set);

    while(1) {
        tmp_set = read_set;
        memset(buffer, 0, CLIENTBUFFLEN);

        ret = select(sockfd + 1, &tmp_set, nullptr, nullptr, nullptr);
        DIE( ret < 0, "Can't select a FD!");

        if(FD_ISSET(STDIN_FILENO, &tmp_set)) {

            memset(&msg, 0, sizeof(msg_action));
            msg.sf = -1;

            fgets(buffer, CLIENTBUFFLEN - 1, stdin);
            // s-a primit comanda de exit -> inchidere client
            if(strcmp(buffer, "exit\n") == 0) {
                break;
            }

            // s-a primit alt input fata de 'exit'
            char *tok = strtok(buffer, " ");
            if(strcmp(tok, "subscribe") == 0) {
                // s-a primit o comanda de subscribe
                char *topic = strtok(nullptr, " ");
                if(topic == nullptr) {
                    perror("Available commands: 'exit' 'subscribe <TOPIC> <SF>' 'unsubscribe <TOPIC>'!");
                    continue;
                }
                if(strlen(topic) > 50) {
                    perror("Topic length must be maximum 50 characters long!");
                    continue;
                }

                char *string_sf = strtok(nullptr, "\n");
                if(string_sf == nullptr) {
                    perror("Available commands: 'exit' 'subscribe <TOPIC> <SF>' 'unsubscribe <TOPIC>'!");
                    continue;
                }

                if(strcmp(string_sf, "0") == 0 || strcmp(string_sf, "1") == 0) {
                    msg.action = SUBSCRIBE;
                    strcpy(msg.topic, topic);
                    msg.sf = atoi(string_sf);

                    ret = send(sockfd, (char *) &msg, sizeof(msg), 0);
                    DIE(ret < 0, "Can't send command to server!");

                    printf("Subscribed to topic.\n");
                } else {
                    perror("Invalid SF flag!");
                    continue;
                }
            } else if(strcmp(tok, "unsubscribe") == 0) {
                // s-a primit o comanda de unsubscribe
                char *topic = strtok(nullptr, "\n");
                if(topic == nullptr) {
                    perror("Available commands: 'exit' 'subscribe <TOPIC> <SF>' 'unsubscribe <TOPIC>'!");
                    continue;
                }

                if(strlen(topic) > 50) {
                    perror("Topic length must be maximum 50 characters long!");
                    continue;
                }
                msg.action = UNSUBSCRIBE;
                strcpy(msg.topic, topic);

                ret = send(sockfd, (char *) &msg, sizeof(msg), 0);
                DIE(ret < 0, "Can't send command to server!");

                printf("Unsubscribed from %s \n", msg.topic);
            } else {
                perror("Available commands: 'exit' 'subscribe <TOPIC> <SF>' 'unsubscribe <TOPIC>'!");
                continue;
            }
        }

        if(FD_ISSET(sockfd, &tmp_set)) {
            // s-a primit mesaj de la server
            n = recv(sockfd, buffer, CLIENTBUFFLEN, 0);
            DIE(n < 0, "Can't receive from server!");

            if(n == 0) {
                break;
            }

            auto *msg = (msg_tcp *)buffer;
            printf("%s:%u - %s - %s - %s\n", msg->ip, msg->port, msg->topic, msg->type, msg->payload);
        }
    }

    close(sockfd);
}
