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

// definizioni =========================
#define DOMAIN              AF_INET
#define MAX_REQUEST_LEN      16

// error codes
#define EXIT_IF_ERROR        -1
#define EXIT_IF_SUCCESS      0
#define CONTINUE             1
#define BAD_REQUEST          "BAD_REQUEST"
// =====================================

// dichiarazione variabili =============

    int 
        i,          // per gli scorrimenti
        len,        // per lunghezza
        ret,        // return value
        sd,         // socket di ascolto
        client_len, // lunghezza indirizzo client 
        csd;

    struct sockaddr_in 
        servaddr,       // indirizzo server
        clientaddr;     // indirizzo client

    pid_t pid;  // pid del processo

    short unsigned int request;

// =====================================

// definizione costanti ================
const char ADDRESS[] = "127.0.0.1";

const char MENU[] = 
    "***************************"
    "           MENU            "
    "***************************"
    "1) signup"
    "1) in"
    "1) "
    "1) ";

// =====================================


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
    servaddr.sin_port = htons(port);
    inet_pton(DOMAIN, ADDRESS, &servaddr.sin_addr);


    ret = bind(sd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if(ret < 0){
        perror("Errore nella bind");
        exit(-1);
    }


    // avvio ascolto
    ret = listen(sd, 10);
    if(ret < 0){
        perror("Errore nella listen");
        exit(-1);
    }

    printf("* Avviato socket di ascolto su: %s:%d\n", addr, port);
}

// trova la porta in cui deve avvenire la comunicazione
int findPort(int argc, char* argv[]){
    int port = 4242;
    if(argc >= 2){
        port = atoi(argv[1]);
    }

    return port;
}
/*
int checkError(int check, const char* msg, int cmd){
    if (check < 0){
        perror(msg);
        
        if (cmd == -1){
            exit(-1);
        }
    }

    if (cmd == 0){
        exit(0);
    }

    return check;
}*/

// gestisce il segnale di interruzione
void intHandler() {
    close(sd);
    perror("Disconnesso");
    exit(0);
}

int main(int argc, char* argv[]){

    //  assegna la gestione del segnale di int
    signal(SIGINT, intHandler);

    int bad_cmd; // identifica se un comando è stato dato correttamente

    // individua la porta da utilizzare
    int port = findPort(argc, argv);    

    // impostazioni inziali del socket
    init(ADDRESS, port);

    while(1){

        // accetto richiesta
        csd = accept(sd, (struct sockaddr*) &clientaddr, &len);
        if(csd < 0){
            perror("Errore in accept");
            continue;
        }

        // richiesta accettata
        pid = fork();   

        if (pid == 0){

            // non serve rimanere in ascolto
            close(sd);

            do {
                
                len = recv(csd, &request, MAX_REQUEST_LEN, 0);
                if(len < 0){
                    perror("Errore ricezione richiesta");
                    
                    // chiudo il socket di comunicazione
                    close(csd);
                    exit(-1);
                }

                switch(request){

                    case 1:         // signup
                        bad_cmd = 0;
                        break;

                    case 2:         // in
                        bad_cmd = 0;
                        break;
                }

            } while(bad_cmd);

            exit(0);
        }

        else{
            // sono nel padre, csd non mi serve
            close(csd);
        }

    }

    return 0;
}