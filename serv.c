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

#include "./API/logger.h"
#include "./utils/costanti.h"
#include "./source/register.h"

// dichiarazione variabili ======================

    int 
        i,          // per gli scorrimenti
        len,        // per lunghezza
        ret,        // return value
        sd,         // socket di ascolto
        client_len, // lunghezza indirizzo client 
        tmp,
        csd;

    struct sockaddr_in 
        servaddr,       // indirizzo server
        clientaddr;     // indirizzo client

    pid_t pid;  // pid del processo

    short unsigned int request;

// ==============================================

// definizione costanti =========================
const char ADDRESS[] = "127.0.0.1";

const char MENU[] = 
    "***************************\n"
    "           MENU            \n"
    "***************************\n"
    "1) help\n"
    "2) list\n"
    "3) esc\n";
// ==============================================


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

    slog("server in ascolto su: %d", port); 
}

// trova la porta in cui deve avvenire la comunicazione
int findPort(int argc, char* argv[]){
    int port = 4242;
    if(argc >= 2){
        port = atoi(argv[1]);
    }

    return port;
}

// gestisce il segnale di interruzione
void intHandler() {
    slog("********** server in chiusura ********");
    close(sd);
    slog("==> %d chiuso", sd);
    close(csd);
    slog("==> %d chiuso", csd);
    exit(0);
}

// **********************************************************************
// * aggiunge un utente all'elenco generale dei registri                *
// * utilizzata durente la procedura di registrazione                   *
// * - username:  nome utente da aggiungere (verifica già compiuta)     *
// * - port:      porta di comunicazione utilizzata dall'utente         *
// **********************************************************************
int add_entry_register(char* username){
    FILE *file;
    time_t t;

    file = fopen(FILE_REGISTER, "a");
    if(!file) {
        perror("Erroe aggiunta entry");
        return 0;
    }

    // recupero data e ora
    time(&t);

    // essendo la prima connessione, login e logout coincidono (utente non connesso)
    ret = (fprintf(file, "%s | %d | %lu | %lu\n", username, PORT_NOT_KNOWN,  (long*)&t,  (long*)&t) > 0) ? 1 : 0;
    fclose(file);
    return ret;
}

// aggiunge all'elenco degli utenti un utente
// - usr: struttura contentente nome utente e password
int add_entry_users(struct user usr){
    FILE *file;
    time_t t;

    file = fopen(FILE_USERS, "a");
    if(!file) {
        perror("Errore aggiunta entry");
        return 0;
    }

    ret = (fprintf(file, "%s | %s\n", usr.username, usr.pw) > 0) ? 1 : 0;

    fclose(file);
    return ret;
}


int find_entry_register(char* username) {
    FILE *file;
    char usr[32];

    file = fopen(FILE_REGISTER, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return 0;
    }

    while(fscanf(file, "%s | %s | %s | %s", &usr, NULL, NULL, NULL) != EOF) {
        //printf("analizzo: %s (%d), paragono con %s (%d)", usr.username, strlen(usr.username), username, strlen(username));
        //fflush(stdout);

        if(strcmp(usr, username) == 0) {
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int find_entry_users(char* username) {
    FILE *file;
    char usr[32];

    file = fopen(FILE_USERS, "r");

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    while(fscanf(file, "%s | %s", &usr, NULL) != EOF) {
        //printf("analizzo: %s (%d), paragono con %s (%d)", usr.username, strlen(usr.username), username, strlen(username));
        //fflush(stdout);

        if(strcmp(usr, username) == 0) {
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int auth(struct user usr) {
    FILE *file;
    char username[32];
    char password[32];

    file = fopen(FILE_USERS, "r");

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    while(fscanf(file, "%s | %s", &username, &password) != EOF) {
        //printf("analizzo: %s (%d), paragono con %s (%d)", usr.username, strlen(usr.username), username, strlen(username));
        //fflush(stdout);

        if(strcmp(usr.username, username) == 0 && strcmp(usr.pw, password) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int signup(struct user usr){
    int ret1, ret2;

    // se non esiste già un utente registrato lo registriamo
    ret = find_entry_users(usr.username);
    if (ret == 0){

        // aggiungo all'elenco degli utenti
        ret1 = add_entry_users(usr);
        ret2 = add_entry_register(usr.username);

        if (ret1 && ret2){
            slog("Utente \"%s\" aggiunto correttamente", &usr.username);
            return 1;
        }
        else {
            perror("Errore aggiunta utente");
        }
    }
    
    // utente non aggiunto
    return 0;
}




int login(struct user usr){
    int flag = auth(usr);

    // aggiornare il registro degli accessi
    if (flag){
        //updateRegister(usr);
    }

    return flag;
}

void sendResponse(int value){
    short int response;

    if(value == 0){
        // registrazione non andata a buon fine
        response = htons(-1);
        send(csd, (void*) &response, sizeof(uint16_t), 0);
    }
    else {
        response = htons(1);
        send(csd, (void*) &response, sizeof(uint16_t), 0);
    }
}

int main(int argc, char* argv[]){

    char buffer[MAX_REQUEST_LEN];
    char param1[16], param2[16], param3[16];
    struct user usr;

    //  assegna la gestione del segnale di int
    signal(SIGINT, intHandler);

    // individua la porta da utilizzare
    int port = findPort(argc, argv);    

    // utilizzato per debug
    init_logger("./.log");

    // impostazioni inziali del socket
    init(ADDRESS, port);

    // imposto di default alla richiesta errata
    request = -1;

    while(1){

        // accetto richiesta e salvo l'informazione nel socket csd
        csd = accept(sd, (struct sockaddr*) &clientaddr, &len);
        if(csd < 0){
            perror("Errore in accept");
            continue; 
        }

        slog("connessione accettata");

        // creo un processo per gestire la richiesta
        pid = fork();   

        // se sono nel figlio, devo gestire la richiesta
        if (pid == 0){

            // non serve rimanere in ascolto
            close(sd);

            // per ogni richiesta cre un processo che 
            // la possa gestire
            do {

                // attendo di ricevere il codice della richiesta
                len = recv(csd, &buffer, MAX_REQUEST_LEN, 0);
                if(len < 0){
                    perror("Errore ricezione richiesta");
                    close(csd);
                    exit(-1);
                }

                if(len == 0){
                    continue;
                }

                // ricevo nel formato: codice parametro1, parametro2, parametro3
                sscanf(buffer, "%hd %s %s %s", &request, &param1, &param2, &param3);
                request = ntohs(request);


                slog("request: %d | %s | %s | %s", request, param1, param2, param3);

                // in base alla richiesta compio azioni differenti
                switch(request){
                    case 0: // signup
                        strcpy(usr.username, param1); 
                        strcpy(usr.pw, param2);

                        ret = signup(usr);
                        sendResponse(ret);
                        break;

                    case 1: // in
                        strcpy(usr.username, param1); 
                        strcpy(usr.pw, param2);

                        ret = login(usr);
                        sendResponse(ret);
                        slog("Risposta inviata");
                        break;

                    case 2: // device port
                        //ret = get_device_port(param1);
                        //slog("porta di %s disponibile a: %d", param1, port);

                        break;

                    default:
                        //request = -1:
                        slog("bad request");
                        break;
                }

            } while(1);

            exit(0);
        }

        else{
            // sono nel padre, csd non mi serve
            close(csd);

            // mostro le scelte messe a disposizione
            printf(MENU);
        }

    }

    return 0;
}