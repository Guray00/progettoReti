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
#include "./API/logger.h"
#include <termios.h>

// COSTANTI  ================================
const char MENU[] = 
    "***************************\n"
    "           MENU            \n"
    "***************************\n"
    "1) signup\n"
    "2) in\n\n"
    "Selezionare quale operazione compiere: ";


const char MENU2[] = 
    "***************************\n"
    " Utente: %s            \n"
    "***************************\n"
    "1) hanging\n"
    "2) show\n"
    "3) chat\n"
    "4) share\n"
    "5) out\n\n"
    "Selezionare quale operazione compiere: ";

const char ADDRESS[] = "127.0.0.1";
// ==========================================

int 
    ret,    // return value
    len,    // lunghezza
    sd,     // socket di ascolto
    csd;    // socket di comunicazione

struct sockaddr_in servaddr, myaddr;


// gestisce il segnale di interruzione
void intHandler() {
    close(sd);
    perror("Disconnesso");
    exit(0);
}


// funzione per l'inizializzazione del server
int init(const char* addr, int port){

    sd  = socket(DOMAIN, SOCK_STREAM, 0);
    //csd = socket(DOMAIN, SOCK_STREAM, 0);

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

    slog("Socket di ascolto attivo per device su: %d", port);
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
    char buffer[MAX_REQUEST_LEN];
    short int res_code;
    uint16_t code = htons(0);

    printf("Inserire il nome utente: ");
    scanf("%s", &usr.username);

    printf("Inserire la password: ");
    scanf("%s", &usr.pw);

    // creo il buffer per la richiesta
    sprintf(buffer, "%hd %s %s", code, &usr.username, &usr.pw);

    // invio la richiesta
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if (ret < 0){
        perror("Errore invio dati per signup");
        exit(-1);
    }
    else if (ret == 0){
        perror("disconnessione server");
        exit(-1);
    }

    ret = recv(sd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione per signup");
        exit(-1);
    }

    res_code = ntohs(res_code);
    slog("ricevuto codice di risposta: %d", res_code);

    if(res_code < 0){
        printf("Utente già utilizzato, selezionare un nome differente.\n\n");
        fflush(stdout);
    } else {
        printf("Registrazione avvenuta con successo!\n\n");
        fflush(stdout);
    }
    
}


void logged(char* username){

    int answer = -1;

    printf("\e[1;1H\e[2J");
    printf(MENU2, username);

    do{

        scanf("%d", &answer);

        switch (answer){
        
        
        default:
            break;
        }

    }while (answer == -1);
}

int login(){

    // pulisce lo schermo
    //printf("\e[1;1H\e[2J");
    //printf(MENU2, );

    struct user usr;
    short int res_code;
    int i;
    uint16_t code = htons(1);
    char buffer[MAX_REQUEST_LEN];


    printf("Username: ________________\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    scanf("%s", &usr.username);

    printf("Password: ________________\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    scanf("%s", &usr.pw);

    sprintf(buffer, "%hd %s %s", code, &usr.username, &usr.pw);

    ret = send(sd, (void*) buffer, sizeof(buffer), 0);
    if(ret < 0){
        perror("Errore send request");
        exit(-1);
    } 
    
    ret = recv(sd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione per signup");
        exit(-1);
    }

    res_code = ntohs(res_code);
    slog("ricevuto codice di risposta: %d", res_code);

    if(res_code < 0){
        printf("Connessione rifiutata.\n\n");
        fflush(stdout);
    } else {
        logged(usr.username);        
    }
    
}


int main(int argc, char* argv[]){

    int answer;
    uint16_t code;

    init_logger("./.log"); 
    
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
                signup();   
                printf(MENU);    
                break;

            case 2:
                login();
                break;

            default:
                answer = -1; // segno che è scorretta
                printf("Scelta scorretta, reinserire: ");
                break;
        }

    } while (1); //  ciclo fino a quando non è corretta
    

    return 0;
}