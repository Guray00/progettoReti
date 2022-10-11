#include <stdio.h>
#include <stdlib.h>
#include "costanti.h"
#include "connection.h"


struct connection* find_connection(struct connection *con, char dst[MAX_USERNAME_SIZE]){
    struct connection *p;

    for (p = con; p != NULL; p = p->next){

        // se trovo un utente con lo stesso nome 
        // mi sono già connesso in passato, verifico
        // se posso correttamente comunicarci
        if(strcmp(dst, p->username) == 0){
            return p;
        }
    }

    return NULL;
}

struct connection* close_connection(struct connection *con){
    struct connection* next;

    // elimino la connessione dalla lista
    next = con->next;
    con->prev->next = con->next;

    // chiudo la connessione
    close(con->socket);

    // libero la memoria
    free(con);

    return next;
}


// passato un indice di un socket, chiude la connessione
struct connection* close_connection_by_socket(struct connection *head, int sock){
    struct connection *next, *tmp;

    tmp = head;
    while(tmp!=NULL){

        // quando trovo la connessione con il socket desiderato
        if (tmp->socket == sock){
            // elimino la connessione dalla lista
            next = tmp->next;
            tmp->prev->next = tmp->next;

            // chiudo la connessione
            close(tmp->socket);

            // libero la memoria
            free(tmp);
            return next;
        }

        tmp = tmp->next;
    }

    
    // se non trovo la connessione restituisco NULL
    return NULL;
}


struct connection* new_connection(struct connection* con, int sock){
    struct connection *tmp, *tail;

    tmp = (void*) malloc(sizeof(struct connection));
    //strcpy(tmp->username, dst);
    tmp->socket = sock;

    tail = con;
    while(tail->next != NULL) tail = tail->next;

    tail->next = tmp;
    tmp->prev = tail;

    return tmp;
}


// assegno un username all'utente con socket "socket"
void set_connection(struct connection* head, char username[MAX_USERNAME_SIZE], int socket){
    struct connection *tmp;

    tmp = head;
    while(tmp!=NULL){
        printf("scorro %d", tmp->socket);
        if (tmp->socket == socket){
            strcpy(tmp->username, username);
            break;
        }

        tmp = tmp->next;
    }

}

char* get_username_by_connection(struct connection* head, int sock){
    struct connection *tmp;

    tmp = head;

    while(tmp!=NULL){
        if (tmp->socket == sock){
            return tmp->username;
            break;
        }

        tmp = tmp->next;
    }

    return NULL;
}