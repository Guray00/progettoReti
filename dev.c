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
#include "./utils/costanti.h"

// DEFINIZIONI =============================

// =========================================


// COSTANTI  ================================
const char MENU[] = 
    "***************************\n"
    "           MENU            \n"
    "***************************\n"
    "1) signup\n"
    "2) in\n\n"
    "Selezionare quale operazione compiere: ";

const char ADDRESS[] = "127.0.0.1";
// ==========================================

int 
    ret,    // return value
    len,    // lunghezza
    sd,     // socket di ascolto
    csd;    // socket di comunicazione

struct sockaddr_in servaddr;


// gestisce il segnale di interruzione
void intHandler() {
    close(sd);
    perror("Disconnesso");
    exit(0);
}


// funzione per l'inizializzazione del server
int init(const char* addr, int port){

    sd = socket(DOMAIN, SOCK_STREAM, 0);

    if(sd < 0){
        perror("Errore all'avvio del socket");
        exit(-1);
    }

    // preparazione indirizzi
    memset(&servaddr, 0, sizeof(servaddr)); // pulizia
    servaddr.sin_family = DOMAIN;
    servaddr.sin_port = htons(4242);
    inet_pton(DOMAIN, ADDRESS, &servaddr.sin_addr);

    ret = connect(sd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if(ret < 0){
        perror("Errore nella connect");
        exit(-1);
    }

    printf("* Socket connesso da: %s:%d\n", addr, port);
}


// si occupa di recuperare la porta
int portCheck(int argc, char* argv[]){
    if(argc < 2){
        printf("Errore, porta non specificata\n");
        exit(-1);
    }

    return atoi(argv[1]);
}


// si occupa di far avvenire la registrazione per gli utenti
void signup(){

    // dichiaro username e password
    struct user usr;
    char buffer[32];

    printf("Inserire il nome utente: ");
    scanf("%s", &usr.username);

    printf("Inserire la password: ");
    scanf("%s", &usr.pw);

    sprintf(buffer, "%s %s", usr.username, usr.pw);

    //printf(buffer);
    
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if (ret < 0){
        perror("Errore invio dati per signup");
        exit(-1);
    }
}

int main(int argc, char* argv[]){

    int answer;
    uint16_t code;
    
    //  assegna la gestione del segnale di int
    signal(SIGINT, intHandler);

    // avvio il socket
    int port = portCheck(argc, argv);
    init(ADDRESS, port);
    printf(MENU);

    do {
        scanf("%d", &answer);

        switch(answer){
            case 1:
                code = htons(0); 
                
                ret = send(sd, (void*) &code, sizeof(code), 0);
                if(ret < 0){
                    perror("Errore send request");
                    exit(-1);
                }

                signup();       
                break;

            case 2:
                break;

            default:
                answer = -1; // segno che è scorretta
                printf("Scelta scorretta, reinserire: ");
                break;
        }

    } while (answer == -1); //  ciclo fino a quando non è corretta
    

    return 0;
}