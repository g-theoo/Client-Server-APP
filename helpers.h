#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <set>
#include <netinet/tcp.h>
#include <math.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define SERVERBUFFLEN 1551
#define CLIENTBUFFLEN 1582

#define SUBSCRIBE 1
#define UNSUBSCRIBE 2

#define CONNECTED 1
#define DISCONNECED (-1)

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3


/*
 * structura mesajului trimis de catre clientul UDP la server
 * topic = numele topicului
 * data_type = tipul de date (INT, SHORT_REAL, FLOAT, STRING);
 * content = datele trimise
 */
struct __attribute__((packed)) msg_server {
    char topic[50];
    uint8_t data_type;
    char payload[1500];
};

/*
 * Structura mesajului trimis de catre clientul TCP la server (subscribe/unsubscribe)
 * action = SUBSCRIBE / UNSUBSCRIBE
 * topic = numele topicului
 * sf = store and forward (0 / 1)
 */
struct __attribute__((packed)) msg_action {
    int action;
    char topic[50];
    int sf;
};

/*
 * Structura mesajului trimis de catre server catre clientii TCP
 * ip = IP-ul clientului UDP
 * port = port-ul clientului UDP
 * topic = numele topicului pe care s-a primit mesajul
 * type = tipul de date
 * payload = datele transmise in mesaj
 */
struct __attribute__((packed)) msg_tcp {
    char ip[INET_ADDRSTRLEN];
    unsigned int port;
    char topic[51];
    char type[11];
    char payload[1500];
};

/*
 * Structura care defineste starea unui client
 * status = conectat / deconectat
 * fd = file descriptorul asociat client_ID-ului
 * old_fd = file descriptorul clientului cand s-a deconectat
 * topics = topic-urile la care este abonat clientul retinute sub forma de NUME / SF
 * old_messages = mesajele retinute pentru topicurile cu SF = 1 cand clientul este deconectat
 */
struct state {
    int8_t status;
    int fd;
    int old_fd;
    std::unordered_map<std::string, bool>  topics;
    std::unordered_map<std::string, std::vector<msg_tcp>> old_messages;
};

#endif
