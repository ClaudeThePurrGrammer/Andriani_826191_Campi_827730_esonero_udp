/*
 * main.c
 *
 * UDP Client - Servizio Meteo
 * Portabile su Windows, Linux e macOS
 */

#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "protocol.h"

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

int set_Socket() {
    int socket_value = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_value < 0) {
        perror("Creazione del socket fallita");
        clearwinsock();
        exit(EXIT_FAILURE);
    }
    return socket_value;
}

struct sockaddr_in set_Sockaddr_in(const char* Ip_addr, int port) {
    struct sockaddr_in x;
    memset(&x, 0, sizeof(x));
    x.sin_family = AF_INET;

    if (strcmp(Ip_addr, "localhost") == 0) {
        x.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        x.sin_addr.s_addr = inet_addr(Ip_addr);
    }

    x.sin_port = htons(port);
    return x;
}

void normalize_city(char *city) {
    if (city == NULL || city[0] == '\0') return;
    city[0] = toupper((unsigned char)city[0]);
    for (int i = 1; city[i] != '\0'; i++) {
        city[i] = tolower((unsigned char)city[i]);
    }
}

/* --- Serializzazione richiesta --- */
void Request_Sending(int sock, struct sockaddr_in *sad, weather_request_t *req) {
    char buffer[1 + 64];
    int offset = 0;

    memcpy(buffer + offset, &req->type, sizeof(char));
    offset += sizeof(char);

    memcpy(buffer + offset, req->city, strlen(req->city) + 1);
    offset += strlen(req->city) + 1;

    if (sendto(sock, buffer, offset, 0, (struct sockaddr*)sad, sizeof(*sad)) < 0) {
#if defined WIN32
        fprintf(stderr, "Invio fallito, errore Winsock: %d\n", WSAGetLastError());
#else
        perror("Invio fallito");
#endif
        exit(EXIT_FAILURE);
    }
}

/* --- Deserializzazione risposta --- */
void Data_Received(int sock, weather_response_t *resp, struct sockaddr_in *fromAddr) {
    char buffer[sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t)];
    socklen_t fromSize = sizeof(*fromAddr);

    int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                  (struct sockaddr*)fromAddr, &fromSize);
    if (bytes_received <= 0) {
#if defined WIN32
        fprintf(stderr, "Ricezione fallita, errore Winsock: %d\n", WSAGetLastError());
#else
        perror("Ricezione fallita");
#endif
        exit(EXIT_FAILURE);
    }

    int offset = 0;
    uint32_t net_status;
    memcpy(&net_status, buffer + offset, sizeof(uint32_t));
    resp->status = ntohl(net_status);
    offset += sizeof(uint32_t);

    memcpy(&resp->type, buffer + offset, sizeof(char));
    offset += sizeof(char);

    uint32_t temp;
    memcpy(&temp, buffer + offset, sizeof(uint32_t));
    temp = ntohl(temp);
    memcpy(&resp->value, &temp, sizeof(float));
}

/* --- Risoluzione DNS per output --- */
void resolve_server_name(const struct sockaddr_in *addr, char *host, size_t hostlen, char *ipstr, size_t iplen) {
#if defined WIN32
    strncpy(ipstr, inet_ntoa(addr->sin_addr), iplen);
    struct hostent *h = gethostbyaddr((const char*)&addr->sin_addr, sizeof(addr->sin_addr), AF_INET);
    const char *resolved = h ? h->h_name : "localhost";
    if (strcmp(ipstr,"127.0.0.1")==0) {
        resolved = "localhost";
    }
    strncpy(host, resolved, hostlen);
#else
    inet_ntop(AF_INET, &(addr->sin_addr), ipstr, iplen);
    if (strcmp(ipstr,"127.0.0.1")==0) {
        strncpy(host,"localhost",hostlen);
    } else {
        getnameinfo((struct sockaddr*)addr, sizeof(*addr),
                    host, hostlen,
                    NULL, 0, NI_NAMEREQD);
    }
#endif
}

int main(int argc, char *argv[]) {
#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
        printf("Errore a WSAStartup()\n");
        exit(EXIT_FAILURE);
    }
#endif

    const char* IP_address = "127.0.0.1";
    int port = SERVER_PORT;
    weather_request_t weather_request;
    weather_response_t weather_response;
    memset(&weather_request, 0, sizeof(weather_request));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            IP_address = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            char *req = argv[++i];
            if (strlen(req) < 2 || req[1] != ' ') {
                printf("Errore: richiesta non valida (formato -r \"type city\")\n");
                exit(EXIT_FAILURE);
            }
            weather_request.type = req[0];
            strncpy(weather_request.city, req+2, sizeof(weather_request.city)-1);
            weather_request.city[sizeof(weather_request.city)-1] = '\0';
            if (strlen(weather_request.city) >= 64) {
                printf("Errore: nome città troppo lungo\n");
                exit(EXIT_FAILURE);
            }
            normalize_city(weather_request.city);
        }
    }

    int sock = set_Socket();
    struct sockaddr_in sad = set_Sockaddr_in(IP_address, port);
    struct sockaddr_in fromAddr;

    Request_Sending(sock, &sad, &weather_request);
    Data_Received(sock, &weather_response, &fromAddr);

    char host[256], ipstr[256];
    resolve_server_name(&sad, host, sizeof(host), ipstr, sizeof(ipstr));



    if (weather_response.status == STATUS_SUCCESS) {
        if (weather_response.type == 't')
        {
        	printf("Ricevuto risultato dal server %s (ip %s). ", host, ipstr);
            printf("%s: Temperatura = %.1f°C\n", weather_request.city, weather_response.value);
        }
        else if (weather_response.type == 'h')
        {
        	printf("Ricevuto risultato dal server %s (ip %s). ", host, ipstr);
            printf("%s: Umidità = %.1f%%\n", weather_request.city, weather_response.value);
        }
        else if (weather_response.type == 'w')
        {
        	printf("Ricevuto risultato dal server %s (ip %s). ", host, ipstr);
            printf("%s: Vento = %.1f km/h\n", weather_request.city, weather_response.value);
        }
        else if (weather_response.type == 'p')
        {
        	printf("Ricevuto risultato dal server %s (ip %s). ", host, ipstr);
            printf("%s: Pressione = %.1f hPa\n", weather_request.city, weather_response.value);
        }
    } else if (weather_response.status == STATUS_CITY_NOT_AVAILABLE) {
        printf("Città non disponibile\n");
    } else if (weather_response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    }

    closesocket(sock);
    clearwinsock();
    return 0;
}
