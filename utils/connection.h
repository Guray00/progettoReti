#ifndef CONNECTION_H
#define CONNECTION_H 

struct connection {
    char username[MAX_USERNAME_SIZE];
    int port;
    int socket;
    struct connection *next;
    struct connection *prev;
};

// cerca un utente all'interno della lista delle connessioni
struct connection* find_connection(struct connection* con, char dst[MAX_USERNAME_SIZE]);

// chiude una connessione attiva
struct connection* close_connection(struct connection *con);

struct connection* close_connection_by_socket(struct connection *con, int sock);

struct connection* new_connection(struct connection* con, int sock);

void set_connection(struct connection* head, char username[MAX_USERNAME_SIZE], int socket);

char* get_username_by_connection(struct connection* con, int sock);

#endif