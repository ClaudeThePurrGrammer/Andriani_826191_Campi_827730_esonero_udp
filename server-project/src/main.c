/*
 * main.c
 *
 * UDP Server - Servizio Meteo
 * Portabile su Windows, Linux e macOS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // strcmp, memset, memcpy
#include <ctype.h>    // isalnum

#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <strings.h>  // strcasecmp
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include "protocol.h"

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

// Lista città supportate
static const char *citta[] = {
    "Bari","Roma","Milano","Napoli","Torino",
    "Palermo","Genova","Bologna","Firenze","Venezia"
};
static const int num_cities = sizeof(citta)/sizeof(citta[0]);

int citta_valida(const char *c) {
    for (int i = 0; i < num_cities; i++) {
#if defined WIN32
        if (_stricmp(c, citta[i]) == 0)
#else
        if (strcasecmp(c, citta[i]) == 0)
#endif
            return 1;
    }
    return 0;
}

// Funzioni di generazione dati meteo
float get_temperature(void) {
    return ((float)rand() / RAND_MAX) * (40.0 - (-10.0)) + (-10.0);
}
float get_humidity(void) {
    return ((float)rand() / RAND_MAX) * (100.0 - 20.0) + 20.0;
}
float get_wind(void) {
    return ((float)rand() / RAND_MAX) * (100.0 - 0.0) + 0.0;
}
float get_pressure(void) {
    return ((float)rand() / RAND_MAX) * (1050.0 - 950.0) + 950.0;
}

int main(int argc, char *argv[]) {
#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("Errore a WSAStartup()\n");
        return 0;
    }
#endif

    int port = SERVER_PORT;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    int my_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0) {
        perror("Creazione socket fallita");
        clearwinsock();
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() fallita");
        closesocket(my_socket);
        clearwinsock();
        exit(EXIT_FAILURE);
    }

    printf("Server in ascolto sulla porta %d...\n", port);

    while (1) {
        char recv_buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
#if defined WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif

        weather_request_t req;
        weather_response_t res;

        int bytesReceived = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0,
                                     (struct sockaddr*)&client_addr, &client_len);
        if (bytesReceived <= 0) {
            perror("recvfrom fallita");
            continue;
        }

        int offset = 0;
        if (bytesReceived < (int)sizeof(char)) {
            res.status = STATUS_INVALID_REQUEST;
            res.type = '\0';
            res.value = 0.0f;
            goto SEND_RESPONSE;
        }

        req.type = recv_buffer[offset];
        offset += sizeof(char);

        int city_len = bytesReceived - offset;
        if (city_len < 0) city_len = 0;
        if (city_len >= (int)sizeof(req.city)) city_len = (int)sizeof(req.city) - 1;
        memcpy(req.city, recv_buffer + offset, city_len);
        req.city[city_len] = '\0';

        char client_ip[INET_ADDRSTRLEN];
#if defined WIN32
        strncpy(client_ip, inet_ntoa(client_addr.sin_addr), INET_ADDRSTRLEN);
#else
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
#endif

        const char *hostname;
        if (strcmp(client_ip, "127.0.0.1") == 0) {
            hostname = "localhost";
        } else {
#if defined WIN32
            struct hostent *host = gethostbyaddr((const char*)&client_addr.sin_addr,
                                                 sizeof(client_addr.sin_addr), AF_INET);
#else
            struct hostent *host = gethostbyaddr(&(client_addr.sin_addr),
                                                 sizeof(client_addr.sin_addr), AF_INET);
#endif
            hostname = host ? host->h_name : "?";
        }

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               hostname, client_ip, req.type, req.city);

        int invalid_char = 0;
        for (int i = 0; req.city[i] != '\0'; i++) {
            if (req.city[i] == '\t' || (!isalnum((unsigned char)req.city[i]) && req.city[i] != ' ')) {
                invalid_char = 1;
                break;
            }
        }

        if (req.type != 't' && req.type != 'h' && req.type != 'w' && req.type != 'p') {
            res.status = STATUS_INVALID_REQUEST;
            res.type = '\0';
            res.value = 0.0f;
        } else if (invalid_char) {
            res.status = STATUS_INVALID_REQUEST;
            res.type = '\0';
            res.value = 0.0f;
        } else if (!citta_valida(req.city)) {
            res.status = STATUS_CITY_NOT_AVAILABLE;
            res.type = '\0';
            res.value = 0.0f;
        } else {
            res.status = STATUS_SUCCESS;
            res.type = req.type;
            switch (req.type) {
                case 't': res.value = get_temperature(); break;
                case 'h': res.value = get_humidity(); break;
                case 'w': res.value = get_wind(); break;
                case 'p': res.value = get_pressure(); break;
            }
        }

SEND_RESPONSE:;
        char send_buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
        offset = 0;

        uint32_t net_status = htonl(res.status);
        memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(send_buffer + offset, &res.type, sizeof(char));
        offset += sizeof(char);

        uint32_t temp_u32;
        memcpy(&temp_u32, &res.value, sizeof(float));
        temp_u32 = htonl(temp_u32);
        memcpy(send_buffer + offset, &temp_u32, sizeof(float));
        offset += sizeof(float);

        sendto(my_socket, send_buffer, offset, 0,
               (struct sockaddr*)&client_addr, client_len);
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
