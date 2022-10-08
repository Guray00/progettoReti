#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>

// ====================
#include "./dev_net.h"
#include "../utils/costanti.h"
#include "../API/logger.h"
// ====================


// EXTERN ================
extern int to_child_fd[2];
extern int to_parent_fd[2];
extern int pid;
extern struct connection con;
// =======================

// GLOBAL ===================================================================
int 
    ret,                // return value
    len,                // lunghezza
    sd,                 // socket di comunicazione con il server
    listener = -1,      // socket di ascolto per messaggi e richieste
    fdmax = -1,         // id di socket massimo
    csd;                // socket di comunicazione

// gestione delle richieste
fd_set master;

// indirizzi
struct sockaddr_in 
    servaddr,               // indirizzo del server
    myaddr,                 // indirizzo del device
    conectaddr,             // indirizzo del device a cui si manda la richiesta
    ndaddr;                 // indirizzo del nuovo device


// LOCALHOST
const char ADDRESS[] = "127.0.0.1";
// ===========================================================================


// gestione chiusura
void intHandler() {
    //slog("In chiusura  per SIGINT %s:%d, max socket: %d", DEVICE.username, DEVICE.port, fdmax);

    // creo il buffer per la richiesta
    char buffer[MAX_REQUEST_LEN];

    // creo il codice identificativo
    short int code = -1;

    // genero la richiesta
    sprintf(buffer, "%d %s", htons(code), con.username);

    // invio la richiesta
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if(ret > 0){
        // se è andata a buon fine termino
        close(sd);
        slog("==> chiuso socket %d", sd);
        exit(0);
    }
}

int init_listen_socket(int port){
    listener = socket(DOMAIN, SOCK_STREAM, 0);
    if (listener < 0){
        perror("Errore apertura socket");
        exit(-1);
    }

    // salvo il mio indirizzo per ricevere le richieste
    memset(&myaddr, 0, sizeof(myaddr)); // pulizia
    myaddr.sin_port = htons(port);
    myaddr.sin_family = DOMAIN;
    inet_pton(DOMAIN, ADDRESS, &myaddr.sin_addr);

    bind(listener, (struct sockaddr*) &myaddr, sizeof(myaddr));
    listen(listener, 10);

    slog("socket di ascolto (%d) attivo su porta: %d", listener, port);
    fdmax = listener;

    return listener;
}


// instaura una connessione con il server
void init_server_connection(int port){

    // inizializzo il socket di ascolto
    sd  = socket(DOMAIN, SOCK_STREAM, 0);

    // se non è possibile avviare il device, chiudo
    if(sd < 0){
        perror("Errore all'avvio del socket");
        exit(-1);
    }

    // preparazione indirizzi per raggiungere il server
    memset(&servaddr, 0, sizeof(servaddr)); // pulizia
    servaddr.sin_family = DOMAIN;
    servaddr.sin_port = htons(4242);
    inet_pton(DOMAIN, ADDRESS, &servaddr.sin_addr);

    // connessione al server
    ret = connect(sd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if(ret < 0){
        perror("Errore nella connect");
        exit(-1);
    }

    // connessione confermata
    slog("Device (%d) connesso al server", port);
}


// MACRO per la ricezione di un codice di risposta dal server
int recive_code_from_server(){
    short int res_code;
    ret = recv(sd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione codice di risposta");
        exit(-1);
    }

    res_code = ntohs(res_code);
    slog("network riceve: %hd", res_code);

    return res_code;
}

// MACRO per inviare una richiesta a un server
void send_server_request(char buffer[MAX_REQUEST_LEN]){
    int bytes_to_send, sent_bytes;
    char* buf = buffer;

    bytes_to_send = MAX_REQUEST_LEN;
    while(bytes_to_send > 0) {
        sent_bytes = send(sd, (void*) buf, bytes_to_send, 0);
            if(sent_bytes == -1) 
                exit(-1);
            
            bytes_to_send -= sent_bytes;
            buf += sent_bytes;
    }

    slog("network invia: %s", buffer);
}



void gui_handler(){
    char read_buffer[MAX_REQUEST_LEN];  // richiesta ricevuta dalla GUI
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata
    char answer[4];                     // codice della risposta

    // recupero il tipo di richiesta in base al codice
    len = read(to_parent_fd[0], read_buffer, MAX_REQUEST_LEN);
    sscanf(read_buffer, "%hd %s", &code, buffer);

    // verifico le richieste ricevute dalla gui
    switch (code){

        // SIGNUP REQUEST
        case SIGNUP_CODE:
            send_server_request(read_buffer);
            ret = recive_code_from_server();
            break;

        // LOGIN REQUEST
        case LOGIN_CODE:
            sprintf(read_buffer, "%s|%d", read_buffer, con.port);
            send_server_request(read_buffer);
            ret = recive_code_from_server();
            break;
        
        // BAD REQUEST
        default:
            ret = -1;
            break;
    }

    // restituisco la risposta
    sprintf(answer, "%d", ret);
    write(to_child_fd[1], answer, strlen(answer) + 1);
}


void startNET(int port){
    // handler per la disconnessione
    signal(SIGINT, intHandler);
    int i  = 0;

    // set per la verifica
    fd_set readers;

    // Inizializzo i socket
    init_server_connection(port);
    init_listen_socket(port);
    

    // pulizia di master
    FD_ZERO(&master);    

    FD_SET(listener, &master);                   // aggiungo il socket in ascolto dei device
    fdmax = (listener > fdmax) ? listener : fdmax;
    FD_SET(sd, &master);                         // aggiungo le risposte dal server in ascolto
    fdmax = (sd > fdmax) ? sd : fdmax;
    
    FD_SET(to_parent_fd[0], &master);           // aggiungo le risposte dal server in ascolto
    fdmax = (to_parent_fd[0] > fdmax) ? to_parent_fd[0] : fdmax;
    
    FD_SET(to_child_fd[1], &master);            // aggiungo le risposte dal server in ascolto
    fdmax = (to_child_fd[1] > fdmax) ? to_child_fd[1] : fdmax;


    do{
        //  seleziono i socket che sono attivi
        FD_ZERO(&readers);  
        readers = master;  

        slog("------ [device %d in attesa] ------", port);
        ret = select(fdmax+1, &readers, NULL, NULL, NULL);
        if(ret<=0){
            perror("Errore nella select");
            exit(1);
        }

        // per ogni richiesta pendente
        for(i = 0; i <= fdmax; i++){

            // se il socket i è settato,
            // analizzo la richiesta
            if(FD_ISSET(i, &readers)){

                // [GUI-HANDLER] se la gui mi ha inviato un messaggio
                if(i == to_parent_fd[0]){      
                    gui_handler();
                }

                // [SERVER-HANDLER] ricezione di informazioni dal server
                else if ( i == sd){
                    
                }

                // [DEVICES-HANDLER] gestione delle richieste di altri dispositivi
                else {

                }
            }
        }
    } while(1);
}


/*length = read(to_parent_fd[0], message, SIZE);
                    slog("network riceve: %s", message);

                    for (j = 0; j < length; j++) {
                        message[j] = toupper(message[j]);
                    }

                    slog("network invia: %s", message);
                    
                    write(to_child_fd[1], message, strlen(message) + 1);
                    // close(to_parent_fd[1]);
                    kill(pid, SIGUSR1);*/