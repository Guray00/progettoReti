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
#include "./utils/connection.h"
#include "./source/register.h"

// dichiarazione variabili ======================

    int 
        i,          // per gli scorrimenti
        len,        // per lunghezza
        ret,        // return value
        sd,         // socket di ascolto
        client_len, // lunghezza indirizzo client 
        tmp,
        csd,
        fd_max = -1;     // massimo numero di socket attivi

    struct sockaddr_in 
        servaddr,       // indirizzo server
        clientaddr;     // indirizzo client

    pid_t pid;  // pid del processo

    short unsigned int request;

    fd_set 
        master,         // tutti i socket 
        readers;        // socket disponibili


// utile per mantenere le informazioni
// sulle connessioni in corso
struct connection con;

// ==============================================

// definizione costanti =========================
const char ADDRESS[] = "127.0.0.1";


// funzione per l'inizializzazione del server
int init(const char* addr, int port){

    // socket su cui ricevere le richieste
    sd = socket(DOMAIN, SOCK_STREAM, 0);

    // verifico errore socket
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


// manda una richiesta e risponde con il numero di byte inviati
int send_request(int dfd, char buffer[MAX_REQUEST_LEN]){
    int bytes_to_send, sent_bytes;
    char* buf = buffer;

    bytes_to_send = MAX_REQUEST_LEN;
    while(bytes_to_send > 0) {
        sent_bytes = send(dfd, (void*) buf, bytes_to_send, 0);
        if(sent_bytes == -1) {
            perror("errore invio richiesta server");
            return -1;
        }
            
        bytes_to_send -= sent_bytes;
        buf += sent_bytes;
    }
    
    return sent_bytes;
}

// gestisce il segnale di interruzione
void intHandler() {
    slog("**************************************");
    slog("*         server in chiusura         *");
    slog("**************************************");

    // creo il buffer per la richiesta
    char buffer[MAX_REQUEST_LEN];

    // creo il codice identificativo
    short int code = -1;
    struct connection *p;

    // genero la richiesta
    sprintf(buffer, "%d %s", htons(code), "server");

    // notifico tutti i dispositivi della disconnessione
    /*
    for(i = 0; i <= fd_max; i++){

        // se il file descriptor è attivo
        if (FD_ISSET(i, &master) && i != sd){

            // invio la richiesta
            ret = send_request(i, buffer);
            if(ret > 0){

                // se è andata a buon fine termino l'iesimo socket
                close(i);
                slog("==> chiuso socket %d", i);
            }
        }
    }*/

    
    for (p = con.next; p!=NULL; ){

        // invio la richiesta
        slog("mando richiesta disconnesione su socket %d", p->socket);
        ret = send_request(p->socket, buffer);
        if(ret > 0){
            slog("==> chiuso socket %d", p->socket);

             // se è andata a buon fine termino l'iesimo socket
            p = close_connection(p);
            }
    }

    exit(0);
}

void pipeHandler() {
    slog("[PIPE ERROR]");
   // exit(0);
}

// **********************************************************************
// * aggiunge un utente all'elenco generale dei registri                *
// * utilizzata durente la procedura di registrazione                   *
// * - username:  nome utente da aggiungere (verifica già compiuta)     *
// * - port:      porta di comunicazione utilizzata dall'utente         *
// **********************************************************************
int add_entry_register(char* username){
    FILE *file;
    //time_t t;

    file = fopen(FILE_REGISTER, "a");
    if(!file) {
        perror("Erroe aggiunta entry");
        return 0;
    }

    // recupero data e ora
    //time(&t);

    // essendo la prima connessione, login e logout coincidono (utente non connesso)
    ret = (fprintf(file, "%s | %d | %lu | %lu\n", username, PORT_NOT_KNOWN,  (unsigned long)time(NULL), (unsigned long)time(NULL)) > 0) ? 1 : 0;
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

int find_entry_register(struct user u) {
    FILE *file;
    char usr[MAX_USERNAME_SIZE];

    file = fopen(FILE_REGISTER, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return 0;
    }

    while(fscanf(file, "%s | %d | %lu | %lu", &usr, NULL, NULL, NULL) != EOF) {
        if(strcmp(usr, u.username) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int find_entry_users(char* username) {
    FILE *file;
    char usr[MAX_USERNAME_SIZE];

    file = fopen(FILE_USERS, "r");

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    while(fscanf(file, "%s | %s", &usr, NULL) != EOF) {
        if(strcmp(usr, username) == 0) {
            return 1;
        }
    }

    fclose(file);
    return 0;
}


/* verifica se l'utente esiste, restituisce:
    - utente trovato: 1
    - utente non trovato: 0
*/
int auth(struct user usr) {
    FILE *file;
    char username[MAX_USERNAME_SIZE];
    char password[MAX_PW_SIZE];

    file = fopen(FILE_USERS, "r");
    slog("Richiesta accesso utente per: %s | %s", usr.username, usr.pw);

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    while(fscanf(file, "%s | %s", &username, &password) != EOF) {
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
    slog("ret vale: %d", ret);
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


void updateRegister(char username[MAX_USERNAME_SIZE], int port, unsigned long lin, unsigned long lon){
    FILE *file, *tmp;               // file utilizzati
    char usr[MAX_USERNAME_SIZE];    // nome letto dal registro
    int p;                          // porta letta
    unsigned long li;               // login timestamp letto
    unsigned long lo;               // logout timestamp letto
    char command[50];               // comando costruito per rinominare il file

    // apro i file
    file = fopen(FILE_REGISTER, "r");
    tmp  = fopen(TMP_FILE_REGISTER, "w");

    // se uno dei due file non si apre, termino
    if(!file || !tmp) {
        perror("Errore apertura file registro");
        return;
    }

    // per ogni riga letta dal registro la copio nel file temporaneo
    while(fscanf(file, "%s | %d | %lu | %lu", &usr, &p, &li, &lo) != EOF) {
        
        // se leggo l'utente attuale scrivo nel file una nuova versione con login e logout aggiornato
        if(strcmp(usr, username) == 0) {
            if(lin == 0) lin = li;
            fprintf(tmp, "%s | %d | %lu | %lu\n", username, port, lin, lon);
        }

        // tutte le altre righe vengono copiate normalmente
        else {
            fprintf(tmp, "%s | %d | %lu | %lu\n", usr, p, li, lo);
        }
    }

    // chiudo i file
    fclose(file);
    fclose(tmp);

    // costruisco il comando per il renaming
    sprintf(command, "rm %s && mv %s %s", FILE_REGISTER, TMP_FILE_REGISTER, FILE_REGISTER);
    system(command);
}


int login(int sock, struct user usr, int port){
    int flag = auth(usr);

    if (flag){
        struct connection *tmp, *tail;
        // aggiornare il registro degli accessi con il timestamp attuale di login
        // e 0 per segnalare che siamo ancora connessi
        updateRegister(usr.username, port, (unsigned long) time(NULL), 0);

        // imposta l'username alla connessione
        set_connection(&con, usr.username, sock);        
    }

    return flag;
}

// gestisce la richiesta di nuova connessione di un dispositivo
void newConnection(){

    // accetto richiesta e salvo l'informazione nel socket csd
    csd = accept(sd, (struct sockaddr*) &clientaddr, &len);
    if(csd < 0){
        perror("Errore in accept");
        return;
    }


    slog("Il server ha accettato la connessione");
    FD_SET(csd, &master);
    if(csd > fd_max) fd_max = csd;  // avanzo il socket di id massimo, fd_max contiene il max fd
    
    // aggiungo la connessione alla lista
    new_connection(&con, csd);
}



/*  1: utente online
    0: utente offline
   -1: utente non trovato   */
short int isOnline(char username[MAX_USERNAME_SIZE]){
    FILE *file;
    char usr[MAX_USERNAME_SIZE];
    int res = -1;
    
    char logout[255];
    char p[255];

    file = fopen(FILE_REGISTER, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return -1;
    }

    while(fscanf(file, "%s | %s | %s | %s", &usr, &p, NULL, &logout) != EOF) {

        if(strcmp(usr, username) == 0) {
            // se è != 0 significa che non è online
            if (logout != 0)
                res = 0;
            else
                res = atoi(p);

            break;
        }
    }

    fclose(file);
    return res;
}


int main(int argc, char* argv[]){

    // variabili di utility
    char buffer[MAX_REQUEST_LEN];
    struct user usr;

    
    //  assegna la gestione del segnale di int
    signal(SIGINT, intHandler);
    signal(SIGPIPE, pipeHandler);


    // individua la porta da utilizzare
    int port = findPort(argc, argv);    

    // utilizzato per debug
    init_logger("./.log");

    slog("*********************************");
    slog("> server started on port: %d", port);
    slog("*********************************");    

    // impostazioni inziali del socket
    init(ADDRESS, port);

    // imposto di default alla richiesta errata
    request = -1;

    FD_ZERO(&master);                   // pulisco il master
    FD_ZERO(&readers);                  // pulisco i readers
    FD_SET(sd, &master);                // aggiungo il socket di ricezione tra quelli monitorati
    if(sd > fd_max) fd_max = sd;

    // inizializzo la lista di connessioni
    con.socket = sd;
    con.prev = NULL;
    con.next = NULL;
    strcpy(con.username, "server");
    con.port = port;

    while(1){

        FD_ZERO(&readers);  // pulisco i lettori
        readers = master;   // copio i socket nei readers

        slog("--- [server in attesa, sd=%d ed fd_max=%d] ---", sd, fd_max);
        ret = select(fd_max+1, &readers, NULL, NULL, NULL);   // seleziono i socket effettivamente attivi
        if(ret < 0){
            perror("Errore nella select");
            exit(1);
        } 

        // per ogni socket aggiunto, verifico quali sono attivi
        for(i = 0; i <= fd_max; i++){

            if(FD_ISSET(i, &readers)){
                slog("entro per %d", i);

                // verifico se la richiesta è sul socket di accettazione,
                // dunque se è pendente una nuova richiesta di connessione
                if(i == sd){
                    newConnection();
                }

                // in tutti gli altri casi sono i devices che effettuano le richieste
                else {
                    short int code;
                    int bytes_to_receive, received_bytes;
                    char *args;       
                    short int response;
                    char *buf = buffer;    


                    bytes_to_receive = sizeof(buffer);
                    while(bytes_to_receive > 0) {
                        received_bytes = recv(i, buf, bytes_to_receive, 0);
                        if(received_bytes == -1){
                            perror("errore ricezione richiesta per server");
                            exit(-1);
                        }
                            
                        bytes_to_receive -= received_bytes;
                        buf += received_bytes;
                    }


                    sscanf(buffer, "%hd %s", &code, &buffer);
                    slog("server ricevuto richiesta [%hd]", code);

                    switch(code){

                        // DISCONNECT
                        case -1:
                            slog("[SOCKET %d] Device chiuso", i);
                            updateRegister(usr.username, port, 0, (unsigned long) time(NULL)); // segno il logout del device
                            //close(i);
                            //close_connection(&con);
                            close_connection_by_socket(&con, i);

                            FD_CLR(i, &master);
                            break;

                        // SIGNUP
                        case SIGNUP_CODE:     
                            args = strtok(buffer, "|");
                            strcpy(usr.username, args);
                            args = strtok(NULL, "|");
                            strcpy(usr.pw, args);

                            ret = signup(usr);
                            response = htons(ret);
                            send(i, (void*) &response, sizeof(uint16_t), 0);
                            break;
                        
                        // LOGIN
                        case LOGIN_CODE:     
                            // scompongo il buffer in [username | password | port]
                            args = strtok(buffer, "|");
                            strcpy(usr.username, args);
                            args = strtok(NULL, "|");
                            strcpy(usr.pw, args);
                            args = strtok(NULL, "|");
                            port = atoi(args);

                            ret = login(i, usr, port);

                            response = htons(ret);
                            send(i, (void*) &response, sizeof(uint16_t), 0);
                            break;

                        case ISONLINE_CODE:
                            ret = isOnline(buffer);
                            response = htons(ret);
                            send(i, (void*) &response, sizeof(uint16_t), 0);
                            break;

                        case CREATECON_CODE:
                            /* devo rispondere con le informazioni
                                -1: utente non esistente
                                 0: utente non attualmente raggiungibile
                                 1: utente raggiunto e invio di credenziali 
                            */

                            
                            ret = 0;//isOnline(buffer);
                            memset(buffer, 0, sizeof(buffer));

                            // se il dispositivo è raggiungibile
                            if (ret > 0){
                                sprintf(buffer, "%d %s %d", ret, ADDRESS, port);
                            }
                            // se non è raggiungibile
                            else {
                                sprintf(buffer, "%d", ret);
                            }



                            ret = send_request(i, buffer);

                            //strcpy(buffer, get_username_by_connection(&con, i));
                            slog("[SERVER->NET (%d)]: mandato %d", i, ret);
                            break;
                    }   
                }
            }
        }
    }

    return 0;
}