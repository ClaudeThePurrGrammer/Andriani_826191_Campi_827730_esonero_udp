/*
 * protocol.h (Server)
 *
 * Definizioni condivise lato server
 */

#ifndef PROTOCOL_H_SERVER
#define PROTOCOL_H_SERVER

#include <stdint.h>

#define SERVER_PORT 56700
#define BUFFER_SIZE 512

// Codici di stato della risposta
#define STATUS_SUCCESS            0
#define STATUS_CITY_NOT_AVAILABLE 2
#define STATUS_INVALID_REQUEST    1

// Struttura richiesta (Client → Server)
typedef struct {
    char type;        // 't'=temperatura, 'h'=umidità, 'w'=vento, 'p'=pressione
    char city[64];    // Nome città (null-terminated)
} weather_request_t;

// Struttura risposta (Server → Client)
typedef struct {
    unsigned int status;  // Codice di stato
    char type;            // Eco del tipo richiesto
    float value;          // Valore meteo
} weather_response_t;

// Funzioni di generazione dati meteo (server)
float get_temperature(void);   // -10.0 to 40.0 °C
float get_humidity(void);      // 20.0 to 100.0 %
float get_wind(void);          // 0.0 to 100.0 km/h
float get_pressure(void);      // 950.0 to 1050.0 hPa

// Funzione di validazione città
int citta_valida(const char *c);

#endif /* PROTOCOL_H_SERVER */
