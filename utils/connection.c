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
struct connection* find_connection(struct connection **head, char dst[MAX_USERNAME_SIZE]){
    struct connection *p;

    for (p = *head; p != NULL; p = p->next){

        // se trovo un utente con lo stesso nome 
        // mi sono già connesso in passato, verifico
        // se posso correttamente comunicarci
        if(strcmp(dst, p->username) == 0){
            return p;
        }
    }

    return NULL;
}

// trova una connessione a partire dal socket
struct connection* find_connection_by_socket(struct connection **head, int fd){
    struct connection *p;

    for (p = *head; p != NULL; p = p->next){

        // se trovo un utente con lo stesso nome 
        // mi sono già connesso in passato, verifico
        // se posso correttamente comunicarci
        if(fd == p->socket){
            return p;
        }
    }

    return NULL;
}

// Rimuove una connessione dalla lista, senza chiuderla
struct connection* remove_connection(struct connection **con){
    
    struct connection* next = NULL;
    struct connection* prev = NULL;

    // se l'lemento è NULL, ritorno NULL
    if ( (*con) == NULL) return NULL;

    // elimino la connessione dalla lista

    // salvo un puntatore al prossimo elemento
    next = (*con)->next;
    prev = (*con)->prev;
    

    // se il prossimo elemento esiste, imposto che punti al mio precedente
    if(next) next->prev = prev;   

    // se il precedente esiste, imposto che punti al mio successivo   
    if(prev){
        prev->next = next;
    }
    else {
        // se entro significa che il precedente era NULL
        // L'unico caso in cui il precedente è NULL è che si stia
        // eseguendo un estrazione in testa. Dunque, se viene passata la testa,
        // questa dovrà essere estratta è sarà necessario puntare al successivo
        //con = &next;
        // printf("ho sistemato la nuova testa\n");
    } 


    // restituisco un puntatore al prossimo elemento
    return next;

}

// consente la chiusura di una connessione paddando il puntatore
// alla connessione attuale
struct connection* close_connection(struct connection **con){
    struct connection* next;

    if ((*con) == NULL) return NULL;

    // chiudo la connessione
    close((*con)->socket);

    // elimino la connessione dalla lista
    next = remove_connection(con);

    // ritorno un puntatore all'elemento che segue
    return next;
}


// passato un indice di un socket, chiude la connessione
struct connection* close_connection_by_socket(struct connection **head, int sock){
    struct connection *next, *tmp;

    tmp = *head;
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


struct connection* new_connection(struct connection** head, int sock){
    struct connection *tmp, *tail;

    // controllo prima di inserire se esiste già un socket connesso
    // nota: se il socket passato è -1, non serve cercare perchè significa
    // che non vi è una connessione attiva ma si ha comunque l'utente nella lista
    if (sock != -1 && find_connection_by_socket(head, sock) != NULL) {
        return find_connection_by_socket(head, sock);
    }

    tmp = (void*) malloc(sizeof(struct connection));
    tmp->socket = sock;
    tmp->next = NULL;
    tmp->prev = NULL;
    strcpy(tmp->username, "undefined");

    tail = *head;

    // se la testa è NULL, sostituisco in testa
    if (tail == NULL){
        *head = tmp;
    } 
    
    // altrimenti se la testa è presente inserisco in coda
    else {
        while(tail->next != NULL) tail = tail->next;

        tail->next = tmp;
        tmp->prev = tail;
    }

    
    return tmp;
}


// assegno un username all'utente con socket "socket"
void set_connection(struct connection** head, char username[MAX_USERNAME_SIZE], int socket){
    struct connection *tmp;

    tmp = *head;
    while(tmp!=NULL){
        if (tmp->socket == socket){
            strcpy(tmp->username, username);
            break;
        }

        tmp = tmp->next;
    }

}

char* get_username_by_connection(struct connection** head, int sock){
    struct connection *tmp;

    tmp = *head;

    while(tmp!=NULL){
        if (tmp->socket == sock){
            return tmp->username;
            break;
        }

        tmp = tmp->next;
    }

    return NULL;
}

void print_connection(struct connection** head){

    struct connection* tmp;
    for(tmp = *head; tmp != NULL; tmp = tmp->next){
        printf("[%s(%d)]->", tmp->username, tmp->socket);
    }

    printf("[NULL]\n\n");
    fflush(stdout);
}

// cancella (senza chiudere) tutte le connessioni dalla lista
void clear_connections(struct connection** head){
    struct connection* next = NULL;
    //int i;

    while(*head != NULL){
        next = remove_connection(head);
        *head = next;
    }
    

   /*
    for(i = 0; *head != NULL && i < 10; (*head) = next)  {
        print_connection(head);
        printf("i = %d\n", i);
        i++;
        sleep(1);

        next = remove_connection(head);
    }

    */
}

// dato il nome di un utente, si aggiorna il suo socket
struct connection* new_passive_connection(struct connection** head, char *name){
    struct connection *tmp, *tail;

    // evito i duplicati
    if (find_connection(head, name)) {
        return find_connection(head, name);
    }

    tmp = (void*) malloc(sizeof(struct connection));
    tmp->socket = -1;
    tmp->next = NULL;
    tmp->prev = NULL;
    strcpy(tmp->username, name);

    tail = *head;

    // se la testa è NULL, sostituisco in testa
    if ( (*head) == NULL){
        *head = tmp;
    } 
    
    // altrimenti se la testa è presente inserisco in coda
    else {
        while(tail->next != NULL) tail = tail->next;

        tail->next = tmp;
        tmp->prev = tail;
    }

    
    return tmp;
}


int connection_size(struct connection **head){
    struct connection* p;
    int counter = 0;

    for(p = *head; p != NULL; p = p->next){
        counter++;
    }

    return counter;
}

int remove_connection_by_socket(struct connection **head, int fd){

    struct connection *p, *next;


    for (p = (*head); p != NULL; p = p->next){

        if(p->socket == fd){
            next  = remove_connection(&p);
            if(*head == p) (*head) = next;
            return 1; 
        }

    }

    return 0;
}


int remove_connection_by_username(struct connection **head, char *username){

    struct connection *p = NULL, *next = NULL;


    for (p = (*head); p != NULL; p = p->next){

        if(strcmp(p->username, username) == 0){
            //printf("sto rimuovendo adesso %s\n", p->username);
            next = remove_connection(&p);

            // se ho estratto in testa, devo aggiornare dove puntare perchè il risultato di
            // &p non altererà il valore di head
            if (*head == p) *head = next;
            //printf("dovrei averlo rimosso...\n");
            return 1;
        }

    }

    return 0;
}