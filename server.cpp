#include <iostream>
#include "helpers.h"

void usage(char *file)
{
    fprintf(stderr, "Usage: %s <PORT> \n", file);
    exit(0);
}

int main(int argc, char *argv[]) {

    if(argc != 2) {
        usage(argv[0]);
    }

    fflush(stdout);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int udp_socket, tcp_socket, portno, newsockfd, i, n, ret, fdmax, flag = 1;
    bool exit_flag = false;
    char buffer[SERVERBUFFLEN];
    sockaddr_in serv_addr, cli_addr;
    fd_set read_set, tmp_set;
    socklen_t clilen;
    std::unordered_map<std::string, state> history;
    std::unordered_map<int, std::string> clients;
    std::unordered_map<std::string, std::unordered_set<int>> topics;

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket < 0, "Can't create UDP socket!");

    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket < 0, "Can't create TCP socket!");

    portno = atoi(argv[1]);
    DIE(portno < 1024, "Invalid PORT!");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(udp_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Can't bind UDP socket!");

    ret = bind(tcp_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Can't bind TCP socket!");

    ret = listen(tcp_socket, INT32_MAX);
    DIE(ret < 0, "Can't listen on TCP socket!");

    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    FD_SET(udp_socket, &read_set);
    FD_SET(tcp_socket, &read_set);

    fdmax = tcp_socket;

    while(!exit_flag) {
        tmp_set = read_set;

        memset(buffer, 0, sizeof(buffer));

        ret = select(fdmax + 1, &tmp_set, nullptr, nullptr, nullptr);
        DIE(ret < 0, "Can't select a FD!");

        for(i = 0; i <= fdmax; i ++) {
            if(FD_ISSET(i, &tmp_set)) {
                memset(buffer, 0, sizeof(buffer));
                if(i == STDIN_FILENO) {
                    // citire de la tastatura
                    fgets(buffer, SERVERBUFFLEN - 1, stdin);

                    if(strcmp(buffer, "exit\n") == 0) {
                        exit_flag = true;
                        break;
                    } else {
                        perror("Available commands: 'exit' !");
                    }
                } else if(i == tcp_socket) {
                    // cerere de conexiune de la un client TCP
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(tcp_socket, (sockaddr *) &cli_addr, &clilen);
                    DIE(newsockfd < 0, "Can't accept new TCP client!");

                    ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
                    DIE(ret < 0, "Can't disable Nagle's algorithm!");

                    FD_SET(newsockfd, &read_set);
                    if(newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }

                    // identificare cilient dupa CLIENT ID
                    n = recv(newsockfd, buffer, SERVERBUFFLEN, 0);
                    DIE(n <= 0, "Client ID was not received!");

                    // este prima conectare a clientului cu ID-ul respectiv
                    if(history.find(buffer) == history.end()) {

                        std::string client_ID;
                        client_ID.append(buffer);

                        state new_connection;
                        new_connection.status = CONNECTED;
                        new_connection.fd = newsockfd;
                        new_connection.topics.clear();
                        new_connection.old_messages.clear();

                        history.insert({client_ID, new_connection});
                        clients.insert({newsockfd, client_ID});

                        printf("New client %s connected from %s:%hu.\n", client_ID.c_str(), inet_ntoa(cli_addr.sin_addr),
                            ntohs(cli_addr.sin_port));

                    } else {
                        // clientul a mai fost conectat in trecut cu ID-ul respectiv
                        state client_status = history.find(buffer)->second;
                        std::string client_ID = history.find(buffer)->first;

                        if(client_status.status == CONNECTED) {
                            // este deja un client cu ID-ul respectiv conectat
                            FD_CLR(newsockfd, &read_set);

                            printf("Client %s already connected.\n", buffer);

                            close(newsockfd);

                        } else if(client_status.status == DISCONNECED) {
                            // clinetul se reconecteaza cu ID-ul respectiv
                            client_status.status = CONNECTED;
                            client_status.old_fd = client_status.fd;
                            client_status.fd = newsockfd;

                            auto topics_temp = client_status.topics;
                            auto it = topics_temp.begin();

                            // se trimit mesajele primite pe topicurile cu SF = 1 cat timp clientul
                            // a fost deconectat
                            while(it != topics_temp.end()) {
                                bool SF = it->second;
                                if(SF) {
                                    std::string name = it->first;
                                    if(client_status.old_messages.find(name) == client_status.old_messages.end()) {
                                        // nu au fost primite mesaje pe topicul respectiv cat timp clientul
                                        // a fost deconectat

                                        it++;
                                        continue;
                                    } else {
                                        // se trimit mesajele primite toate mesajele primite pe topicul respectiv

                                        auto old_messages = client_status.old_messages.find(name)->second;
                                        int size = old_messages.size();
                                        for(int j = 0; j < size; j ++) {
                                            ret = send(newsockfd, (char *) &old_messages[j], sizeof(msg_tcp), 0);
                                            DIE(ret < 0, "Can't send the message to TCP Client!");
                                        }
                                    }
                                }
                                it++;
                            }

                            // actualizare history
                            client_status.old_messages.clear();
                            auto current = history.find(client_ID);
                            current->second = client_status;

                            // actualizare clienti
                            clients.erase(client_status.old_fd);
                            clients.insert({newsockfd, client_ID});

                            // actualizare FD in topics
                            auto update_topics = topics.begin();
                            if(update_topics != topics.end()) {
                                while(update_topics != topics.end()) {
                                    auto temp_set = update_topics->second;
                                    if(temp_set.find(client_status.old_fd) != temp_set.end()) {
                                        temp_set.erase(client_status.old_fd);
                                        temp_set.insert(newsockfd);
                                        update_topics->second = temp_set;
                                    }
                                    update_topics++;
                                }
                            }
                            printf("New client %s connected from %s:%hu.\n", client_ID.c_str(), inet_ntoa(cli_addr.sin_addr),
                                   ntohs(cli_addr.sin_port));
                        }
                    }

                } else if(i == udp_socket) {
                    // mesaj de la un client UDP
                    clilen = sizeof(cli_addr);
                    ret = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (sockaddr*) &cli_addr, &clilen);
                    DIE(ret < 0, "Can't receive from UDP Client!");

                    msg_server *info = (msg_server *) buffer;

                    msg_tcp for_subscribers;
                    memset(&for_subscribers, 0, sizeof(msg_tcp));

                    strcpy(for_subscribers.ip, inet_ntoa(cli_addr.sin_addr));
                    for_subscribers.port = ntohs(cli_addr.sin_port);

                    // procesare mesaj primit de la clientul UDP
                    if(info->data_type == INT) {

                        uint8_t sign_byte = info->payload[0];
                        long value = ntohl(*(uint32_t*)(info->payload + 1));

                        if(sign_byte == 1) {
                            value = value * (-1);
                        }
                        strcpy(for_subscribers.topic, info->topic);
                        strcpy(for_subscribers.type, "INT");
                        sprintf(for_subscribers.payload, "%ld", value);

                    } else if(info->data_type == SHORT_REAL) {

                        float value = ntohs(*(uint16_t*)(info->payload));
                        value = abs(value / 100);
                        strcpy(for_subscribers.topic, info->topic);
                        strcpy(for_subscribers.type, "SHORT_REAL");
                        sprintf(for_subscribers.payload, "%.2f", value);

                    } else if(info->data_type == FLOAT) {

                        uint8_t sign_byte = info->payload[0];
                        uint8_t exp = info->payload[5];

                        double value = ntohl(*(uint32_t*)(info->payload + 1));
                        value = value * pow(10, (-exp));

                        if(sign_byte == 1) {
                            value = value * (-1);
                        }

                        strcpy(for_subscribers.topic, info->topic);
                        strcpy(for_subscribers.type, "FLOAT");
                        sprintf(for_subscribers.payload, "%f", value);

                    } else if(info->data_type == STRING) {

                        strcpy(for_subscribers.topic, info->topic);
                        strcpy(for_subscribers.type, "STRING");
                        strcpy(for_subscribers.payload, info->payload);

                    } else {
                        perror("Invalid type of data!");
                    }

                    for_subscribers.topic[50] = '\0';
                    for_subscribers.type[10] = '\0';

                    if(topics.find(info->topic) == topics.end()) {
                        // topicul pe care s-a trimis mesaj nu exista -> se creeaza topicul
                        std::unordered_set<int> temp;
                        topics.insert({info->topic, temp});

                    } else {
                        // topicul pe care s-a trimis mesaj exista

                        auto subscribers = topics.find(info->topic)->second;
                        // se extrage lista cu FD-urile clientilor care sunt abonati la topicul respectiv
                        if(!subscribers.empty()) {

                            auto sub_it = subscribers.begin();
                            while (sub_it != subscribers.end()) {

                                std::string client_ID = clients.find((*sub_it))->second;
                                auto client_info = history.find(client_ID);
                                auto client_state = client_info->second;

                                if(client_state.status == DISCONNECED) {
                                    //clientul respectiv este deconectat
                                    if(client_state.topics.find(info->topic)->second == true) {
                                        // daca este abonat la topic cu SF = 1, se adauga mesajul la
                                        // lista de mesaje care trebuie sa fie trimise la reconectrae
                                        auto old_messages = client_state.old_messages.find(info->topic);
                                        if(old_messages == client_state.old_messages.end()) {
                                            std::vector<msg_tcp> messages;
                                            messages.push_back(for_subscribers);
                                            client_state.old_messages.insert({info->topic, messages});
                                            client_info->second = client_state;
                                        } else {
                                            auto messages = old_messages->second;
                                            messages.push_back(for_subscribers);
                                            old_messages->second = messages;
                                            client_info->second = client_state;
                                        }
                                    }
                                } else if(client_state.status == CONNECTED) {
                                    // clientul este conectat
                                    auto current_topic = client_state.topics.find(info->topic);
                                    if(current_topic != client_state.topics.end()) {
                                        //clientul este abonat la topicul respectiv -> i se trimite mesajul
                                        ret = send((*sub_it), (char *)&for_subscribers, sizeof(for_subscribers), 0);
                                        DIE(ret < 0, "Can't send message to TCP client!");
                                    }
                                }
                                sub_it++;
                            }
                        }
                    }
                } else {
                    // mesaj de la TCP (sub / unsub) sau deconectare
                    n = recv(i, buffer, sizeof(buffer), 0);
                    DIE(n < 0, "Can't receive from TCP Client!");

                    if(n == 0) {
                        // s-a deconectat
                        std::string client_ID = clients.find(i)->second;
                        printf("Client %s disconnected.\n", client_ID.c_str());

                        //actualizare client ca fiind deconectat
                        auto client_status = history.find(client_ID)->second;
                        client_status.status = DISCONNECED;

                        auto current = history.find(client_ID);
                        current->second = client_status;
                        FD_CLR(i, &read_set);

                    } else {
                        // s-a primit o comanda de subscribe/unsubscribe
                        msg_action *command = (msg_action *) buffer;

                        std::string client_ID = clients.find(i)->second;
                        state client_data = history.find(client_ID)->second;

                        auto command_topic = client_data.topics.find(command->topic);

                        if(command_topic == client_data.topics.end()) {
                            //topicul nu exista in istoricul clientului
                            if(command->action == SUBSCRIBE) {
                                // se adauga topicul in istoricul clientului cu SF-ul dorit
                                if (command->sf == 1) {
                                    client_data.topics.insert({command->topic, true});
                                } else {
                                    client_data.topics.insert({command->topic, false});
                                }
                                // actualizare history
                                auto current = history.find(client_ID);
                                current->second = client_data;

                                // se verifica daca topicul respectiv exista deja
                                if(topics.find(command->topic) == topics.end()) {
                                    // topicul nu exista -> se creeaza si se retine FD-ul clientului respectiv
                                    std::unordered_set<int> temp;
                                    temp.insert(i);
                                    topics.insert({command->topic, temp});
                                } else{
                                    // topicul exista
                                    auto temp = topics.find(command->topic);
                                    auto subscribed_clients = temp->second;
                                    if(subscribed_clients.find(i) == subscribed_clients.end()) {
                                        // clientul nu este abnoat la topic -> se aboneaza
                                        subscribed_clients.insert(i);
                                        // se actualizeaza topicul respectiv
                                        temp->second = subscribed_clients;
                                    }
                                }
                            }
                        } else {
                            // topicul exista deja in istoricul clientului
                            if(command->action == SUBSCRIBE) {
                                // se actualizeaza abonamentul clientului pe topicul respectiv
                                if(command->sf == 1) {
                                    command_topic->second = true;
                                } else {
                                    command_topic->second = false;
                                }
                                // actualizare history
                                auto current = history.find(client_ID);
                                current->second = client_data;

                            } else if(command->action == UNSUBSCRIBE) {
                                // unsubscribe de la topicul rescpectiv
                                client_data.topics.erase(command->topic);

                                // actualizare topic
                                auto to_unsubscribe_topic = topics.find(command->topic);
                                auto current_clients = to_unsubscribe_topic->second;
                                if(current_clients.size() == 1) {
                                    topics.erase(command->topic);
                                } else {
                                    current_clients.erase(i);
                                    to_unsubscribe_topic->second = current_clients;
                                }
                                // actualizare history
                                auto current = history.find(client_ID);
                                current->second = client_data;
                            }
                        }
                    }
                }
            }
        }
    }
    // inchidere clienti
    for(i = 0; i <= fdmax; i ++) {
        if(FD_ISSET(i, &read_set)) {
            close(i);
        }
    }
    FD_ZERO(&read_set);
}
