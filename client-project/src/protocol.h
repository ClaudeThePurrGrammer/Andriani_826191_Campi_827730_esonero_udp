/*
 * protocol.h (Client)
 *
 * Definizioni condivise lato client
 */

#ifndef PROTOCOL_H_CLIENT
#define PROTOCOL_H_CLIENT

#include <stdint.h>

#define SERVER_PORT 56700
#define BUFFER_SIZE 512

// Codici di stato della risposta
#define STATUS_SUCCESS            0
#define STATUS_CITY_NOT_AVAILABLE 1
#define STATUS_INVALID_REQUEST    2

// Struttura richiesta (Client → Server)
typedef struct {
    char type;        // 't'=temperatura, 'h'=umidità, 'w'=vento, 'p'=pressione
    char city[64];    // Nome città (null-terminated)
} weather_request_t;

// Struttura risposta (Server → Client)
typedef struct {
    unsigned int status;  // Codice di stato (network byte order gestito manualmente)
    char type;            // Eco del tipo richiesto
    float value;          // Valore meteo (serializzato come uint32_t)
} weather_response_t;

#endif /* PROTOCOL_H_CLIENT */
