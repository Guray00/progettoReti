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
#include <errno.h>

// ====================
#include "./dev_net.h"
#include "./dev_gui.h"
#include "../utils/costanti.h"
#include "../utils/connection.h"
#include "../API/logger.h"
// ====================

#include <libgen.h>  // utile per dirname e basename


// EXTERN ===================================================================
extern int to_child_fd[2];      // pipe NET->GUI
extern int to_parent_fd[2];     // pipe GUI->NET
extern int pid;                 // pid del processo
extern int DEVICE_PORT;
//char SCENE[MAX_USERNAME_SIZE];

// informazioni sulle connessioni attive
extern struct connection *con;   

// tiene traccia di tutti gli utenti presenti all'interno di una chat
struct connection *partecipants = NULL;
unsigned short int GROUPMODE = 0;
// ==========================================================================

// GLOBAL ===================================================================
int 
    ret,                // return value
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
    devaddr;                // indirizzo del device a cui si manda la richiesta
    //ndaddr;               // indirizzo del nuovo device


// LOCALHOST
const char ADDRESS[] = "127.0.0.1"; // indirizzo di connessione 
unsigned long my_hash;              // variabile contenete l'hash al momento del login
// ===========================================================================

// funzione per calcolare l'hash dell'utente
unsigned long hash(char *str){
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) )
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


void logout_device(int fd){
    char buffer[MAX_REQUEST_LEN];
    struct connection *tmp;
    
    sprintf(buffer, "%d %s", LOGOUT_CODE, "user");
    ret = send(fd, (void *) buffer, sizeof(buffer), 0);

    if(ret > 0){
        // se è andata a buon fine termino
        tmp = find_connection_by_socket(&con, fd);
        FD_CLR(fd, &master);
        close_connection(&tmp);
    }   
    else {
        perror("qualcosa è andato storto");
    }
}

// ATTENZIONE: questa funzione, come da nome, non effettua
// il logout dal server bensì si limita a contattare i peer 
// in modo da rimuoverne la connessione
void logout_devices(){
    struct connection *p, *next;
    char buffer[MAX_REQUEST_LEN];

    // la testa non deve essere eliminata perchè contiene le informazioni
    // de device
    // print_connection(&con);
    //sleep(1);
    for (p = con->next; p != NULL; p = next){

        // salvo il puntatore al prossimo elemento
        next = p->next;

        // genero la richiesta
        slog("===> STO CONTATTANDO: %s(%d)", p->username, p->socket);
        sprintf(buffer, "%d %s", LOGOUT_CODE, con->username);

        // invio la richiesta al device
        ret = send(p->socket, (void *) buffer, sizeof(buffer), 0);
        if(ret > 0){
            // se è andata a buon fine termino
            FD_CLR(p->socket, &master);
            close_connection(&p);
        }   
        else {
            perror("qualcosa è andato storto");
        }

    }

    slog("ho finito con i device");
}

void logout_server(){
    // creo il buffer per la richiesta
    char buffer[MAX_REQUEST_LEN];

    // creo il codice identificativo
    short int code = LOGOUT_CODE;

    // genero la richiesta di disconnessione dal server
    sprintf(buffer, "%d %s", code, con->username);
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if(ret > 0){
        // se è andata a buon fine termino
        close(sd);
        FD_CLR(sd, &master);
        //close_connection_by_socket(&con, sd); // non funziona perchè il server non è tra le connessioni
        slog("==> chiuso socket %d", sd);
    }
    
}

void send_request_to_gui(char* buffer){
    // NET->GUI, invio la richiesta
    write(to_child_fd[1], buffer, MAX_REQUEST_LEN);
}

// gestione chiusura
void intHandler() {

    // invia la richiesta al server di logout
    logout_server();
    
    // invia la richiesta a tutti i dispositivi di logout
    logout_devices();

    exit(0);
}

int init_listen_socket(int port){
    listener = socket(DOMAIN, SOCK_STREAM, 0);
    if (listener < 0){
        perror("Errore apertura socket");
        exit(-1);
    }

    // imposto il socket di ascolto
    //con->socket = listener;

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

// MACRO che instaura una connessione con il server
int init_server_connection(int port){

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
    servaddr.sin_port = htons(port);
    inet_pton(DOMAIN, ADDRESS, &servaddr.sin_addr);

    // connessione al server
    ret = connect(sd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if(ret < 0){
        perror("Errore nella connect");
        return -1;
    }

    // ora che il server è connesso aggiungo all'ascolto
    FD_SET(sd, &master);                         
    fdmax = (sd > fdmax) ? sd : fdmax;

    // connessione confermata
    slog("[NET:%d] Connessione al server conclusa", DEVICE_PORT);
    return 1;
}

// MACRO per la ricezione di un codice di risposta da un device
short int recive_code_from_device(int fd){
    short int res_code;
    ret = recv(fd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione codice di risposta");
        exit(-1);
    }

    res_code = ntohs(res_code);
    return res_code;
}

// MACRO per la ricezione di un codice di risposta dal server
short int recive_code_from_server(){
    short int res_code;
    ret = recv(sd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione codice di risposta");
        exit(-1);
    }

    res_code = ntohs(res_code);
    //slog("network riceve: %hd", res_code);

    return res_code;
}

// Invia una richiesta a un device specificato
int send_device_request(int fd, char buffer[MAX_REQUEST_LEN]){
    int bytes_to_send, sent_bytes;
    char* buf = buffer;

    // if (!FD_ISSET(fd, &master)) return -1;

    bytes_to_send = MAX_REQUEST_LEN;
    while(bytes_to_send > 0) {
        sent_bytes = send(fd, (void*) buf, bytes_to_send, 0);
        if(sent_bytes == -1 || sent_bytes == 0){
            slog("ERRORE INVIO %d, inviati: %d", fd, sent_bytes);
            perror("errore invio richiesta device");
            return -1;
        } 
            
        bytes_to_send -= sent_bytes;
        buf += sent_bytes;
    }

    return sent_bytes;
}

// generalizzazione di una richiesta
int send_request(int code, int fd, char buffer[MAX_REQUEST_LEN]){
    //int bytes_to_send; , sent_bytes;
    char command [MAX_REQUEST_LEN];

    // controllo che effettivamente sia raggiungibile
    // if (!FD_ISSET(fd, &master)) return -1;

    sprintf(command, "%d %s", code, buffer);
    ret = send_device_request(fd, command);
    return ret;
}

// MACRO per inviare una richiesta a un server
int send_server_request(char buffer[MAX_REQUEST_LEN]){
    if(FD_ISSET(sd, &master)){
        return send_device_request(sd, buffer);
    }

    return -1;
}

// MACRO per la ricezione di richieste dal server
void recive_from_server(char buffer[MAX_REQUEST_LEN]){
    memset(buffer, 0, MAX_REQUEST_LEN); // resetto il contenuto del buffer
    
    int bytes_to_receive, received_bytes;
    char *buf = buffer;    


    bytes_to_receive = MAX_REQUEST_LEN;
    while(bytes_to_receive > 0) {
        received_bytes = recv(sd, buf, bytes_to_receive, 0);
        if(received_bytes == -1){
            perror("errore ricezione da server per network");
            exit(-1);
        }
                            
        bytes_to_receive -= received_bytes;
        buf += received_bytes;
    }
}

// MACRO per la ricezione di richieste dal server
int recive_from_device(char *buffer, int fd){
    memset(buffer, 0, MAX_REQUEST_LEN); // resetto il contenuto del buffer

    //if (!FD_ISSET(fd, &master)) return -1;
    
    int bytes_to_receive, received_bytes;
    char *buf = buffer;    


    bytes_to_receive = MAX_REQUEST_LEN;
    while(bytes_to_receive > 0) {
        received_bytes = recv(fd, buf, bytes_to_receive, 0);
        if(received_bytes == -1){
            perror("errore ricezione da device per network");
            return -1;
        }
                            
        bytes_to_receive -= received_bytes;
        buf += received_bytes;
    }

    return received_bytes;
}

// gestore notifiche a schermo
void notify(char* msg, char *COLOR){
    char formatted_msg[MAX_REQUEST_LEN];

    printf("\n");
    sprintf(formatted_msg,  "[ %s ]" , msg);
    printf(COLOR);
    print_centered_dotted(formatted_msg);
    printf(ANSI_COLOR_RESET);
    printf("\n");
}

// Gestisce le richieste arrivate dal server e dirette al device
void server_handler(){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata

    // ricevo dal buffer
    recive_from_server(buffer);
    sscanf(buffer, "%hd %s", &code, buffer);

    slog("[NET:%d] Ricevuto da server [%hd] e (%s)", DEVICE_PORT, code, buffer);

    switch(code){

        // DISCONNECT REQUEST
        case LOGOUT_CODE:
            slog("arrivata al net chiusura server");
            notify("SERVER SPENTO", ANSI_COLOR_RED);            

            // rimuovo il server dai socket ascoltati
            FD_CLR(sd, &master);
            close(sd);

            break;
    }
}

// gestisce l'arrivo di un messaggio P2P
void recive_p2p_msg(int device, char *buffer){

    char src[MAX_USERNAME_SIZE];
    char formatted_msg[MAX_REQUEST_LEN];

    // genero la richiesta per la gui
    strcpy(src, get_username_by_connection(&con, device));

    //if (strcmp(src, SCENE) == 0){
    // se trovo l'utente tra i partecipanti
    if (find_connection(&partecipants, src) != NULL){
        format_msg(formatted_msg, src, buffer);
        printf("%s\n", formatted_msg);
    }

    else {
        sprintf(formatted_msg, "Nuovo messaggio da %s: %s", src, buffer);
        notify(formatted_msg, ANSI_COLOR_BLUE);
    }
}

// aggiunge in fondo alla cronologia un messaggio p2p ricevuto
int append_received_msg_to_historic(char *mittente, char* msg){
    char formatted_msg[MAX_MSG_SIZE];

    char historic_path[100];
    FILE  *historic;

    sprintf(historic_path, "./devices_data/%s/%s.txt", con->username, mittente);
    historic = fopen(historic_path, "a");
    if(historic < 0){
        perror("errore apertura file");
        return -1;
    }

    // formatto il messaggio [UTENTE] [MESSAGGIO]
    format_msg(formatted_msg, mittente, msg);

    // aggiungo alla cronologia il messaggio
    fprintf(historic, formatted_msg);
    fprintf(historic, "\n");

    // chiudo il file
    fclose(historic);
    
    return 0;
}

// aggiunge alla cronologia un messaggio p2p inviato
int append_sent_msg_to_historic(char *dest, char* msg){
    char formatted_msg[MAX_MSG_SIZE];

    char historic_path[100];
    FILE  *historic;

    sprintf(historic_path, "./devices_data/%s/%s.txt", con->username, dest);
    historic = fopen(historic_path, "a");
    if(historic < 0){
        perror("errore apertura file");
        return -1;
    }

    // formatto il messaggio [UTENTE] [MESSAGGIO]
    format_msg(formatted_msg, con->username, msg);

    // aggiungo alla cronologia il messaggio
    fprintf(historic, formatted_msg);
    fprintf(historic, "\n");

    // chiudo il file
    fclose(historic);
    
    return 0;
}

// stabilisce una connessione con un destinatario se non presente
struct connection* create_connection(char dst[MAX_USERNAME_SIZE]){
    char buffer[MAX_REQUEST_LEN];   // buffer per le richieste
    int response;                   // salva il codice di risposta della richiesta
    char address[50];               // salva l'indirizzo in cui trovare il device
    int fd;                         // file descriptor
    struct sockaddr_in addr;        // indirizzo della futura connessione
    struct connection* newcon;      // puntatore alla connessione creata

    // TEST CONNECTION (già verificato prima!)
    // se l'utente è già connesso non devo interrogare il server,
    // bensì provare a comunicarci e vedere se il messaggio arriva.
    // Se arriva si procede normalente, in caso contrario è necessario
    // interrogare il server.
    // - per recuperare le informazione se la connessione non esiste e stabilirla
    // - inoltrare direttamente le informazioni


    // mando una richiesta di informazioni sullo stato dell'utente
    sprintf(buffer, "%d %s", CREATECON_CODE, dst);
    ret = send_server_request(buffer);
    if(ret < 0) return NULL;

    slog("[NET->SERVER]: %s", buffer);


    // FIRST CONNECTION - di seguito si esegue la prima connessione
    // recupero le informazioni sull'utente con cui voglio
    // stabilire una connessione e le traduco in:
    // [RISPOSTA INDIRIZZO PORTA], dove risposta stabilisce se l'utente 
    // non esiste(-1), offline(0),  online(1)
    recive_from_server(buffer);
    sscanf(buffer, "%d %s", &response, address);
    

    // TODO: testare la corretta ricezione dei dati

    // se l'utente è online la risposta sarà > 0
    if (response > 0){

        // inizializzo il socket per comunicare con il nuovo dispositivo
        fd  = socket(DOMAIN, SOCK_STREAM, 0);
        if(fd < 0){
            perror("Errore all'avvio del socket");
            exit(-1);
        }

        // preparazione indirizzi per raggiungere il nuovo device
        memset(&addr, 0, sizeof(addr)); // pulizia
        addr.sin_family = DOMAIN;
        addr.sin_port = htons(response);
        inet_pton(DOMAIN, address, &addr.sin_addr);

        // connessione al nuovo device
        ret = connect(fd, (struct sockaddr*) &addr, sizeof(addr));
        if(ret < 0){
            perror("Non è possibile connettersi con il nuovo device");
            slog("La porta di connessione era: %d", response);
            return NULL;
        }

        // il device che ha fatto la richiesta aggiunge
        FD_SET(fd, &master);
        if(fd > fdmax) fdmax = fd;

        // genero la richiesta di verifica hash e la invio al nuovo dispositivo
        sprintf(buffer, "%s %lu", con->username, my_hash);
        //slog("HO CONFEZIONATO: %s", buffer);
        send_device_request(fd, buffer);

        // aggiungo alla lista la nuova connessione
        newcon = new_connection(&con, fd);
        set_connection(&con, dst, fd);  // imposto il nome del nuovo socket
        newcon->port = response;        // imposto la porta

        // se sono in un gruppo, accetto subito!
        /*if (GROUPMODE == 1) {
            new_passive_connection(&partecipants, dst);
            slog("HO AGGIUNTO PASSIVO: %s", dst);
        }*/

        // la connessione tra i device è avvenuta,
        // aggiungo alla lista delle connessioni
        // slog("[NET:%d] connessione tra i device avvenuta", con->port);

        return newcon;
    }

    // il dispositivo non è online
    else if (response == 0){
        slog("utente attualmente offline, connessione non stabilita");
    }

    else {
        slog("Utente non esistente, connessione non stabilita");
    }
    
    return NULL;
}

// mostra i messaggi presenti in "show" come notifiche o messaggi p2p
void show_as_p2p(int fd){
    char path[100];

    FILE *file;
    char *line = NULL;
    ssize_t read;
    size_t l;


    // creo le cartelle e i file necessari (in locale) per show
    sprintf(path, "./devices_data/%s/%s", con->username, SHOW_FILE);


    // Il file è stato ricevuto, è compito allora del network aggiornare
    // in modo sensato i file locali. Devono essere aggiunti i nuovi messaggi
    // alla cronologia della chat caricata
    file     = fopen(path, "r");
    if(file < 0){
        perror("errore apertura file");
        return;
    }

    // mostro a schermo il parsing del contenuto del file
    while((read=getline(&line, &l, file)) != -1){
        line[strcspn(line, "\n")] = 0;
        recive_p2p_msg(fd, line);
    }

    // chiudo il file
    fclose(file);

    return;
}


// L'utente è stato trovato offline, è dunque necessario aggiungere il
// messaggio inviato come pendente al server, in modo che venga inviato
// quando l'untente farà l'accesso
int send_pendant_to_server(char dst[MAX_USERNAME_SIZE], char msg[MAX_MSG_SIZE]){
    //short int code = PENDANTMSG_CODE;

    char buffer[MAX_REQUEST_LEN];

    sprintf(buffer, "%d %s %s", PENDANTMSG_CODE, dst, msg);
    ret = send_server_request(buffer);
    
    
    return ret;
}

// consente l'invio di un messaggio
int send_msg(char* buffer){
    char dst[MAX_USERNAME_SIZE], original[MAX_USERNAME_SIZE];
    char msg[MAX_MSG_SIZE];
    struct connection* dst_connection, *p;

    // ricavo il messaggio e il destinatario
    sscanf(buffer, "%s %[^\n\t]", original, msg);
    //slog("[NET:%d] vuole scrivere a %s", con->port, dst);

    // per ogni partecipante alla chat, invio un messaggio
    for(p = partecipants; p != NULL; p = p->next){

        // recupero il nome del partecipante
        strcpy(dst, p->username);

        // verifico se esiste una connessione precedente con il device
        dst_connection = find_connection(&con, dst);

        
        // se non trovo nessuna connessione
        if(dst_connection == NULL){

            // chiedo di instaurare una connessione con l'utente
            // l'utente potrebbe però essere offline (prima o verificato
            // se la connessione esisteva, non se può essere creata) 
            slog("--- creo nuova connessione ---");
            dst_connection = create_connection(dst);

            // se ancora non è stato possibile realizzarla,
            // allora l'utente è offline
            if (dst_connection == NULL){

                // mando il messaggio come pendente al server
                ret = send_pendant_to_server(dst, msg);
            }

            // la connessione adesso è stata stabilita, dunque si 
            // manda il messaggio direttamente al socket
            else {
                // slog("la connessione è appena stata stabilita");
                sprintf(buffer, "%s è attualmente online", dst);
                notify(buffer, ANSI_COLOR_GREEN);
                ret = send_request(CHAT_CODE, dst_connection->socket, msg);
            }
        }

        // connessione trovata perchè creata precedentemente
        else {
            slog("questa è una connessione precedente");
            slog("--> in particolare con: %s (%d)", dst_connection->username, dst_connection->socket);
            ret = send_request(CHAT_CODE, dst_connection->socket, msg);
        }

    }

    // il messaggio è ora stato inviato, deve però essere salvato nella cronologia!
    // Ciò però è valido solamente se la connessione è singola
    // Nota: lo dobbiamo aggiungere solo se correttamente inviato
    if(connection_size(&partecipants) < 2 && ret > 0){
        append_sent_msg_to_historic(original, msg);
        // slog("=========================");
        // slog("AGGIUNGO ALLA CRONOLOGIA di %s il messaggio verso %s: %s", con->username, original, msg);
        // slog("=========================");
    }

    return ret;
}

// funzione utilizzata per la ricezione di file
int recive_file(int fd, char *path){
    FILE* file;
    char buffer[MAX_REQUEST_LEN];
    char dirpath[100];
    char cmd[100];
    int size;
    char c;

    //if (!FD_ISSET(fd, &master)) return -1;

    strcpy(dirpath, path);    
    dirname(dirpath);
    sprintf(cmd, "mkdir -p %s && touch %s", dirpath, path);   // mi assicuro che il file esista
    system(cmd);

    // fino al termine del file continuo a chiedere
    file = fopen(path, "w");

    // ricevo il numero di byte che mi verranno inviati
    recive_from_device(buffer, fd);
    sscanf(buffer, "%d", &size);

    // =========================
    while(size >= 0){
        ret = recv(sd, &c, 1, 0);
        if(ret < 0){
            perror("errore ricezione file");
            return -1;
        }

        size  = size - 1;
        if (size > -1) fputc(c, file);
    }
    // =========================

    // chiudo il file
    fclose(file);
    return 0;
}

int hanging_request_server(){
    char buffer[MAX_REQUEST_LEN];
    char path[100];


    // creo le cartelle e i file necessari (in locale) per l'hanging
    sprintf(path, "./devices_data/%s/%s", con->username, HANGING_FILE);


    // mando la richiesta di hanging
    sprintf(buffer, "%d", HANGING_CODE);
    ret = send_server_request(buffer);
    if (ret < 0) return ret;

    // aspetto di ricevere il file di hanging
    ret = recive_file(sd, path);
    if (ret < 0) return ret;

    return 0;
}


// invia la richiesta di show al server
// e aggiorna la cronologia dei messaggi
int show_request_server(char *mittente){
    char path[100];
    char historic_path[100];
    FILE  *historic;

    FILE *file;
    char *line = NULL;
    ssize_t read;
    size_t l;

    char msg[MAX_REQUEST_LEN] = "";


    // creo le cartelle e i file necessari (in locale) per show
    sprintf(path, "./devices_data/%s/%s", con->username, SHOW_FILE);

    // mando la richiesta di show [CODE] [USERNAME]
    ret = send_request(SHOW_CODE, sd, mittente);
    if (ret < 0) return ret;

    // aspetto di ricevere il file di per show
    ret = recive_file(sd, path);
    if (ret < 0) return ret;

    // Il file è stato ricevuto, è compito allora del network aggiornare
    // in modo sensato i file locali. Devono essere aggiunti i nuovi messaggi
    // alla cronologia della chat caricata
    sprintf(historic_path, "./devices_data/%s/%s.txt", con->username, mittente);
    historic = fopen(historic_path, "a");
    file     = fopen(path, "r");
    if(file < 0 || historic < 0){
        perror("errore apertura file");
        return -1;
    }

    // mostro a schermo il parsing del contenuto del file
    while((read=getline(&line, &l, file)) != -1){

        // formatto il messaggio [TIMESTAMP] [UTENTE] [MESSAGGI]
        format_msg(msg, mittente, line);

        // aggiungo alla cronologia il messaggio
        fprintf(historic, msg);
    }

    // chiudo il file
    fclose(file);
    fclose(historic);

    return 0;
}

// verifica chiedendo al server quali utenti presenti
// in rubrica siano effettivamente online
int available_request_server(){
    char available_path[100], contacts_path[100], dirpath[100], cmd[100], buffer[MAX_REQUEST_LEN];
    FILE *available, *contacts;

    char *line = NULL;
    ssize_t read;
    size_t l;

    // recupero il path relativo al file dei contatti
    sprintf(available_path, "./devices_data/%s/%s", con->username, AVAILABLE_FILE);
    sprintf(contacts_path,  "./devices_data/%s/%s", con->username, CONTACTS_FILE);
    strcpy(dirpath, contacts_path);    
    dirname(dirpath);

    // se i file non esistono li genero
    sprintf(cmd, "mkdir -p %s && touch %s && touch %s", dirpath, contacts_path, available_path); 
    system(cmd);

    //line[strcspn(line, "\n")] = 0;

    available = fopen(available_path, "w");
    contacts  = fopen(contacts_path,  "r");
    if(available < 0 || contacts < 0){
        perror("Non è stato possibile aprire i file");
        return -1;
    }

    // leggo ogni utente dal file dei contatti
    while((read=getline(&line, &l, contacts)) != -1){

        line[strcspn(line, "\n")] = 0;

        sprintf(buffer, "%d %s", ISONLINE_CODE, line);
        slog("richiedo per: %s", buffer);
        ret = send_server_request(buffer);
        if (ret == -1) break;

        ret = recive_code_from_server();
        if (ret == -1) break;

        // se l'utente risulta online, lo scrivo tra i disponibili
        if (ret > 0){
            fprintf(available, line);
            fprintf(available, "\n");

            // informo che tutto è andato correttamente
            ret = 0;
        }
    }

    // chiudo il file
    fclose(available);
    fclose(contacts);

    return ret;
}

int print_available(){
    char path[100];

    FILE *file;
    char *line = NULL;
    ssize_t read;
    size_t l;
    unsigned short int flag = 0;

    sprintf(path, "./devices_data/%s/%s", con->username, AVAILABLE_FILE);

    file     = fopen(path, "r");
    if(file < 0){
        perror("errore apertura file");
        return -1;
    }

    printf("\n");
    print_separation_line();
    printf("Gli utenti attualmente %sdisponibili%s sono:\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);

    // mostro a schermo il parsing del contenuto del file
    while((read=getline(&line, &l, file)) != -1){
        line[strcspn(line, "\n")] = 0;

        // mostro solo gli utenti online non partecipanti
        if(find_connection(&partecipants, line) == NULL){
            flag = 1;
            printf("- %s%s%s\n", ANSI_COLOR_GREEN, line, ANSI_COLOR_RESET);
        }
    }

    if(!flag){
        printf("Attenzione: %snessun utente%s attualmente disponibile.\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
    }

    print_separation_line();
    printf("\n");

    // chiudo il file
    fclose(file); 
    return 0;  
}


void switch_to_group(){
    char buffer[MAX_REQUEST_LEN];
    
    system("clear");
    print_group_chat_header();
    GROUPMODE = 1;

    // avverto la gui di avviare una chat per far avvenire l'inserimento
    sprintf(buffer, "%hd", STARTCHAT_CODE);
    send_request_to_gui(buffer);
}

int send_group_invite(struct connection *dst){

    ret = send_request(INVITEGROUP_CODE, dst->socket, "");
    if (ret < 0) return ret;

    // 0 se rifiuta la connessione, 1 se accetta, -1 se errore
    ret = recive_code_from_device(dst->socket);

    return ret;
}

// funzione che consente l'aggiunta di un utente a una chat
int add_user(char *dst){
    struct connection *p, *c;
    char buffer[MAX_REQUEST_LEN];

    // un utente da aggiungere non può essere un utente già partecipante
    if(find_connection(&partecipants, dst) != NULL) return -2;

    // aggiungo il nuovo partecipante alla lista dei partecipanti
    if(find_connection(&con, dst) == 0) 
        c = create_connection(dst);
    
    if(c == NULL) return -2;
    
    // mando l'invito all'utente
    ret = send_group_invite(c);
    if(ret <= 0) return ret;

    // passo graficamente alla chat di gruppo
    switch_to_group();

    // scorro tutti i partecipanti alla chat
    for(p = partecipants; p != NULL; p = p->next){

        if(strcmp(p->username, dst) == 0) continue;

        // se la connessione con un partecipanti non è ancora 
        // presente la forzo
        c = find_connection(&con, p->username);
        if(c == NULL){
            c = create_connection(dst);

            // se la connessione non può essere instaurata esco
            // nota: è il caso quando il server è offline
            if(c == NULL) return -1;
        }

        // la connessione tra l'host e il partecipante è ora assicurata
        // invio la richiesta di aggiunta utente al device p
        // nota: si usa il socket su c perchè i partecipanti non hanno socket aggiornati
        sprintf(buffer, "%d %s", ADDUSER_CODE, dst);
        ret = send_device_request(c->socket, buffer);
        if (ret == -1) return -1;

        sprintf(buffer, "%d", ENABLEGROUP_CODE);
        ret = send_device_request(c->socket, buffer);
        if (ret == -1) return -1;        

        slog("INOLTRATO RICHIESTA DI ADDUSER A: %s", c->username);
    }    

    new_passive_connection(&partecipants, dst);
    return 0;
}

// gestisce la ricezione di richiesta da altri devices
void device_handler(int device){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata
    struct connection *newp;            // per la richiesta di nuovo partecipante

    // ricevo dal buffer
    recive_from_device(buffer, device);
    sscanf(buffer, "%hd %[^\t\n]", &code, buffer);

    slog("[NET -> NET] %hd %s", code, buffer);

    switch(code){

        // DISCONNECT REQUEST
        case LOGOUT_CODE:
            //slog("arrivata al net chiusura device");
            sprintf(buffer, "%s", get_username_by_connection(&con, device));

            // solo se trovo il nome stampo la notifica a schermo
            if (strcmp(buffer, "(null)") != 0){
                sprintf(buffer, "Utente %s disconnesso", get_username_by_connection(&con, device));
                notify(buffer, ANSI_COLOR_RED);
            }


            close_connection_by_socket(&con, device);
            FD_CLR(device, &master);
            break;

        case CHAT_CODE:
            // NOTA: in buffer è presente il contenuto del messaggio

            // gestisco graficamente la ricezione del messaggio
            recive_p2p_msg(device, buffer);
            
            // il messaggio ricevuto deve anche essere scritto nella cronologia, 
            // solamente se si tratta di una chat singola
            if (connection_size(&partecipants) < 2)
                append_received_msg_to_historic(get_username_by_connection(&con, device), buffer);

            break;

        // ricezione richiesta di aggiunta a un gruppo
        case ADDUSER_CODE:

            // provo a instaurare una connessione con il nuovo utente
            newp = create_connection(buffer);

            // se la connessione non è possibile, esco
            if (newp == NULL) break;

            // altrimenti lo aggiungo ai partecipanti
            new_passive_connection(&partecipants, buffer);
            break;

        // risponde alla richiesta di invito
        case INVITEGROUP_CODE:

            // se non è già occupato in una chat accetta l'invito
            if (connection_size(&partecipants) == 0) {
                ret = 1;
                
                new_passive_connection(&partecipants, find_connection_by_socket(&con, device)->username);
                switch_to_group(device);
            }

            else {
                ret = 0;

            }

            // restitusco l'esito dell'invito: 1 se è stato accettato, 0 se invece è stato respinto
            code = htons(ret);
            send(device, (void*) &code, sizeof(uint16_t), 0);

            // il dispositivo dovrebbe fare logout
            if(ret == 0) {
                sprintf(buffer, "Chat di gruppo rifiutata, %s disconnesso", get_username_by_connection(&con, device));
                notify(buffer, ANSI_COLOR_RED);

                logout_device(device);
            }

            break;
        
        case ENABLEGROUP_CODE:
            new_passive_connection(&partecipants, find_connection_by_socket(&con, device)->username);
            switch_to_group();
            break;
    }
}


void gui_handler(){
    char read_buffer[MAX_REQUEST_LEN];  // richiesta ricevuta dalla GUI
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata
    char answer[4];                     // codice della risposta
    char* args;
    char hash_buffer[MAX_USERNAME_SIZE + MAX_PW_SIZE + 1];
    struct connection *p;

    // recupero il tipo di richiesta in base al codice
    read(to_parent_fd[0], read_buffer, MAX_REQUEST_LEN);
    sscanf(read_buffer, "%hd %[^\t\n]", &code, buffer);

    // verifico le richieste ricevute dalla gui
    switch (code){

        // SIGNUP REQUEST
        case SIGNUP_CODE:
            // la prima cosa da fare è creare la connessione con il server
            ret = init_server_connection(4242);
            // se non è stato possibile connettersi al server
            // chiedo di tornare indietro
            if (ret < 0) break;

            send_server_request(read_buffer);
            ret = recive_code_from_server();

            // TODO: la signup dovrebbe staccare dal server
            break;

        // LOGIN REQUEST
        case LOGIN_CODE:

            // la prima cosa da fare è creare la connessione con il server
            ret = init_server_connection(4242);

            // se non è stato possibile connettersi al server
            // chiedo di tornare indietro
            if (ret < 0) break;
            
            // recupero password e porta
            sprintf(read_buffer, "%s|%d", read_buffer, con->port);
            args = strtok(buffer, "|");
            strcpy(con->username, args);
            args = strtok(NULL, "|");

            // invio la richiesta di accesso al server
            ret = send_server_request(read_buffer);
            if (ret <= 0) break;


            // 1 se è connesso, 0 se non trovato, -2 se già acceduto
            ret = recive_code_from_server();

            // Se il risultato è 0 le credenziali erano errate, se il risultato era -2 era già online l'utente
            // In entrambi i casi la connessione deve essere eliminata
            if (ret == -2 || ret == 0) {
                close(sd);
                FD_CLR(sd, &master);
                break;
            }  
            // in tutti gli altri casi torno indietro
            else if( ret < 0) 
                break;

            // genero il mio hash al login
            sprintf(hash_buffer,"%s %s", con->username, args);
            my_hash = hash(hash_buffer);

            //slog("IL MIO HASH [%s][%s | %s]: %lu", read_buffer, con->username, args, my_hash);
            break;

        case CHAT_CODE:
            // si recuperano ii messaggi non ancora recapitati dal mittente stesso
            ret = show_request_server(buffer);
            if(ret < 0) break;

            // nota: nel buffer è presente il nome utente
            // TODO: controllare se il partecipante esiste realmente
            //strcpy(SCENE, buffer);

            // aggiungo il partecipante alla lista
            new_passive_connection(&partecipants, buffer);
            break;
        
        // CHECK ONLINE REQUEST
        case ISONLINE_CODE:
            /*ret = send_server_request(read_buffer);
            if(ret < 0) break;

            ret = recive_code_from_server();*/
            // nota: buffer contiene il nome utente
            /*ret = find_connection(&con, buffer) != NULL ? 1 :  0;*/

            // restituisco 1 se almeno un utente ha ricevuto il messaggio
            ret = 0;
            for(p = partecipants; p != NULL; p = p->next){
                
                if(find_connection(&con, p->username) != NULL){
                    ret = 1;
                    break;
                }
            }

            break;

        // SEND MESSAGE REQUEST
        case SENDMSG_CODE:
            ret = send_msg(buffer);
            break;

        // SEND HANGING REQUEST
        case HANGING_CODE:
            ret = hanging_request_server();
            break;

        // SEND SHOW REQUEST
        case SHOW_CODE:
            ret = show_request_server(buffer);
            break;

        // gestisce la richiesta di disconnessione
        case LOGOUT_CODE:
            slog("[GUI->NET:%d] ricevuto richiesta di disconnessione", con->port);
            logout_devices();
            logout_server();
            slog("ho fatto il logout");
            break;

        // gestisce l'uscita dalla chat eleminando gli utenti dalla chat
        case QUITCHAT_CODE:
            //strcpy(SCENE, "");
            // rimuovo tutti gli utenti partecipanti alla chat
            clear_connections(&partecipants);
            GROUPMODE = 0;
            break;

        // richiesta di verifica di quali utenti in rubrica sono presenti
        case AVAILABLE_CODE:
            ret = available_request_server();
            if (ret == -1) break;

            // mostro a schermo il risultato
            ret = print_available();
            break;

        // richiesta di aggiunta di un utente alla chat
        case ADDUSER_CODE:
            // buffer contiene il nome dell'utente da aggiungere
            add_user(buffer);
            break;

        // BAD REQUEST
        default:
            ret = -1;
            break;
    }

    // restituisco la risposta
    sprintf(answer, "%d", ret);
    write(to_child_fd[1], answer, sizeof(ret));
}


void startNET(){
    // handler per la disconnessione
    signal(SIGINT, intHandler);
    int i  = 0;

    // set per la verifica
    fd_set readers;

    // Inizializzo i socket
    init_listen_socket(DEVICE_PORT);

    // pulizia di master
    FD_ZERO(&master);    

    FD_SET(listener, &master);                   // aggiungo il socket in ascolto dei device
    fdmax = (listener > fdmax) ? listener : fdmax;
    
    FD_SET(to_parent_fd[0], &master);           // aggiungo le risposte dal server in ascolto
    fdmax = (to_parent_fd[0] > fdmax) ? to_parent_fd[0] : fdmax;
    
    FD_SET(to_child_fd[1], &master);            // aggiungo le risposte dal server in ascolto
    fdmax = (to_child_fd[1] > fdmax) ? to_child_fd[1] : fdmax;


    do{
        //  seleziono i socket che sono attivi
        FD_ZERO(&readers);  
        readers = master;  

        slog("------ [device %d in attesa] ------", DEVICE_PORT);
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
                slog("[NET:%d] intercettato socket %d", con->port, i);

                // [GUI-HANDLER] se la gui mi ha inviato un messaggio
                if(i == to_parent_fd[0]){      
                    gui_handler();
                }

                // [SERVER-HANDLER] ricezione di informazioni dal server
                else if ( i == sd){
                    server_handler();
                }

                // se è in arrivo una nuova richiesta di connessione da un device
                // [LISTENER-HANDLER]
                else if (i == listener){
                    int fd;
                    short int result;
                    socklen_t l;
                    char req[MAX_REQUEST_LEN];
                    char username[MAX_USERNAME_SIZE];
                    char formatted_msg[MAX_MSG_SIZE];

                    l =  sizeof(devaddr);

                    // accetto la richiesta di connessione dal nuovo device
                    memset(&devaddr, 0, sizeof(devaddr)); // pulizia
                    fd = accept(listener, (struct sockaddr*) &devaddr, &l);
                    if(fd < 0){
                        perror("Errore in accept");
                        slog("Errore in accept");
                        break;
                    }

                    // verification - aspetto di ricevere dal device l'hash che lo identifica
                    ret = recive_from_device(req, fd);
                    //if(ret < 0) break;

                    // controllo mediante server se l'utente è chi dice di essere
                    //slog("IL DEVICE MI HA PASSATO %s", req);
                    ret = send_request(WHOIS_CODE, sd, req);
                    //if(ret < 0) break;

                    // attendo la risposta dal server
                    result = recive_code_from_server();

                    // autentificazione confermata, instauro la connessione
                    if (result > 0){
                        
                        // aggiungo la connessione alla lista delle connessioni attive
                        new_connection(&con, fd);
                        sscanf(req, "%s", username);
                        set_connection(&con, username, fd); // salvo il nome
                        slog("connessione confermata con %s", username);

                        // ascolto le nuove richieste in arrivo
                        FD_SET(fd, &master);
                        if(fd > fdmax) fdmax = fd;

                        sprintf(formatted_msg, "%s si è appena collegato", get_username_by_connection(&con, fd));
                        notify(formatted_msg, ANSI_COLOR_GREEN);

                        // se siamo in un gruppo, la nuova connessione deve immediatamente essere considerata partecipante
                        if(GROUPMODE) new_passive_connection(&partecipants, get_username_by_connection(&con, fd));
                        
                        show_request_server(username);
                        show_as_p2p(fd);
                    }
                    
                    // autentificazione fallita, chido la comunicazione
                    else {
                        printf("TRADITO");
                        fflush(stdout);
                        slog("--- TRADITO ---");
                        close(fd);
                    }

                }
                

                // [DEVICES-HANDLER] gestione delle richieste di altri dispositivi
                else {
                    device_handler(i);
                }

                if (ret < 0) slog("qualche problema in net");
            }
        }
    } while(1);
}
