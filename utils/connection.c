#include <stdio.h>
#include <stdlib.h>
#include "costanti.h"
#include "connection.h"

/*
CONNECTION - Struttura dati

La seguente è una struttura dati utilizzata dal server e dai devices
per tenere traccia di tutte le connessione attualmente attive.

La testa della lista è sempre il dispositivo stesso con le proprie informazioni.
Tale scelta è stata effettuata per mantenere in modo consistente le informazioni
prime dei devices e semplificare i casi limiti di estrazione in testa (che in questa 
modalità non avvengono, essendo il device sempre attivo fin quando funziona).
*/

// restituisce un puntatore relativo a un utente passato mediante username
struct connection* find_connection(struct connection *head, char dst[MAX_USERNAME_SIZE]){
    struct connection *p;

    for (p = head; p != NULL; p = p->next){

        // se trovo un utente con lo stesso nome 
        // mi sono già connesso in passato, verifico
        // se posso correttamente comunicarci
        if(strcmp(dst, p->username) == 0){
            return p;
        }
    }

    return NULL;
}

// consente la chiusura di una connessione paddando il puntatore
// alla connessione attuale
struct connection* close_connection(struct connection *con){
    struct connection* next;

    // chiudo la connessione
    close(con->socket);

    // elimino la connessione dalla lista
    next = con->next;
    if(next) next->prev = con->prev;       
    (con->prev)->next = con->next;

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

            // chiudo la connessione
            close(tmp->socket);

            // elimino la connessione dalla lista
            next = tmp->next;
            if(next) next->prev = tmp->prev;       
            (tmp->prev)->next = tmp->next;

            // libero la memoria
            free(tmp);
            return next;
        }

        tmp  = tmp->next;
    }

    // se non trovo la connessione restituisco NULL
    return NULL;
}


struct connection* new_connection(struct connection* head, int sock){
    struct connection *tmp, *tail;

    tmp = (void*) malloc(sizeof(struct connection));
    tmp->socket = sock;

    tail = head;
    while(tail->next != NULL) tail = tail->next;

    tail->next = tmp;
    tmp->prev = tail;
    tmp->next = NULL;

    strcpy(tmp->username, "undefined");
    return tmp;
}


// assegno un username all'utente con socket "socket"
void set_connection(struct connection* head, char username[MAX_USERNAME_SIZE], int socket){
    struct connection *tmp;

    tmp = head;
    while(tmp!=NULL){
        //printf("scorro %d", tmp->socket);
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

void print_connection(struct connection* head){

    struct connection* tmp;
    for(tmp = head->next; tmp != NULL; tmp = tmp->next){
        printf("[%s(%d)]->", tmp->username, tmp->socket);
    }

    printf("[NULL]\n\n");
    fflush(stdout);
}
