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
#include "../utils/graphics.h"
// #include "./dev_gui.h"
#include "../utils/costanti.h"
#include "../utils/connection.h"
#include "../API/logger.h"
// ====================

#include <libgen.h>  // utile per dirname e basename


// EXTERN ===================================================================
extern int to_child_fd[2];      // pipe NET->GUI
extern int to_parent_fd[2];     // pipe GUI->NET
extern int pid;                 // pid del processo
extern int DEVICE_PORT;         // porta del device

// informazioni sulle connessioni attive
extern struct connection *con;   

// tiene traccia di tutti gli utenti presenti all'interno di una chat
struct connection *partecipants = NULL;
uint16_t GROUPMODE = 0;
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

// permette di eseguire il logout verso un solo device
void logout_device(int fd){
    char buffer[MAX_REQUEST_LEN];
    struct connection *tmp;
    
    sprintf(buffer, "%d %s", LOGOUT_CODE, "user");
    ret = send(fd, (void *) buffer, sizeof(buffer), 0);

    if(ret > 0){
        // se ?? andata a buon fine termino
        tmp = find_connection_by_socket(&con, fd);
        FD_CLR(fd, &master);
        close_connection(&tmp);
    }   
    else {
        perror("qualcosa ?? andato storto");
    }
}

// ATTENZIONE: questa funzione, come da nome, non effettua
// il logout dal server bens?? si limita a contattare i peer 
// in modo da rimuoverne la connessione
void logout_devices(){
    struct connection *p, *next;
    char buffer[MAX_REQUEST_LEN];

    // la testa non deve essere eliminata perch?? contiene le informazioni
    // de device
    for (p = con->next; p != NULL; p = next){

        // salvo il puntatore al prossimo elemento
        next = p->next;

        // genero la richiesta
        sprintf(buffer, "%d %s", LOGOUT_CODE, con->username);

        // invio la richiesta al device
        ret = send(p->socket, (void *) buffer, sizeof(buffer), 0);
        if(ret > 0){
            // se ?? andata a buon fine termino
            FD_CLR(p->socket, &master);
            close_connection(&p);
        }   
        else {
            perror("qualcosa ?? andato storto");
        }

    }

    slog("Tutti i device informati del logout");
}

void logout_server(){
    // creo il buffer per la richiesta
    char buffer[MAX_REQUEST_LEN];
    FILE *file;
    char path[PATH_SIZE];

    // creo il codice identificativo
    int16_t code = LOGOUT_CODE;

    // genero la richiesta di disconnessione dal server
    sprintf(buffer, "%d %s", code, con->username);
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if(ret > 0){
        // se ?? andata a buon fine termino
        close(sd);
        FD_CLR(sd, &master);
        slog("==> chiuso socket %d", sd);
    }

    // se il logout non va a buon fine significa che il server non ?? attivo
    // devo allora memorizzare in locale il timestamp in modo da aggiornarlo di nuovo
    // al prossimo login
    else {
        // se non sono loggato non ?? importante salvare il timestamp
        if(strcmp(con->username, "") == 0) return;

        sprintf(path, "./devices_data/%s/%s", con->username, LOGOUT_FILE);
        slog("salvo timestamp del logout in locale: %s", path);
        file = fopen(path, "w");
        if(file < 0){
            perror("errore salvataggio timestamp");
            exit(-1);
        }
        
        // scrivo nel file il timestamp del logout
        fprintf(file, "%lu", (unsigned long) time(NULL));
        fclose(file);
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

    // se non ?? possibile avviare il device, chiudo
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

    // ora che il server ?? connesso aggiungo all'ascolto
    FD_SET(sd, &master);                         
    fdmax = (sd > fdmax) ? sd : fdmax;

    // connessione confermata
    slog("[NET:%d] Connessione al server conclusa", DEVICE_PORT);
    return 1;
}

// MACRO per la ricezione di un codice di risposta da un device
int16_t receive_code_from_device(int fd){
    int16_t res_code;
    ret = recv(fd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione codice di risposta");
        exit(-1);
    }

    res_code = ntohs(res_code);
    return res_code;
}

// MACRO per la ricezione di un codice di risposta dal server
int16_t receive_code_from_server(){
    int16_t res_code;
    ret = recv(sd, (void*) &res_code, sizeof(res_code), 0);
    if(ret < 0){
        perror("Errore ricezione codice di risposta");
        exit(-1);
    }

    res_code = ntohs(res_code);

    return res_code;
}

// Invia una richiesta a un device specificato
int send_device_request(int fd, char buffer[MAX_REQUEST_LEN]){
    int bytes_to_send, sent_bytes;
    char* buf = buffer;

    bytes_to_send = MAX_REQUEST_LEN;
    while(bytes_to_send > 0) {
        sent_bytes = send(fd, (void*) buf, bytes_to_send, 0);
        if(sent_bytes == -1 || sent_bytes == 0){
            slog("Errore invio %d, inviati: %d", fd, sent_bytes);
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
    char command [MAX_REQUEST_LEN];

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
void receive_from_server(char buffer[MAX_REQUEST_LEN]){
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
int receive_from_device(char *buffer, int fd){
    memset(buffer, 0, MAX_REQUEST_LEN); // resetto il contenuto del buffer
    
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

// Gestisce le richieste arrivate dal server e dirette al device
void server_handler(){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    int16_t code;                     // codice della richiesta effettuata

    // ricevo dal buffer
    receive_from_server(buffer);
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

        // conferma di avvenuta lettura
        case READ_CODE:
            sprintf(buffer, "%s ha letto i messaggi", buffer);
            notify(buffer, ANSI_COLOR_CYAN);
            break;
    }
}

// verifica la presenza di un utente all'interno dei contatti
int check_user_in_contacts(char *user){
    FILE *file;
    char buffer[PATH_SIZE];
    char path[PATH_SIZE];
    char *line = NULL;
    ssize_t read;
    size_t l;
 
    // genero i file se non presenti
    sprintf(path, "./devices_data/%s/%s", con->username, CONTACTS_FILE);
    sprintf(buffer, "mkdir -p ./devices_data/%s/ && touch %s",con->username, path);
    system(buffer);

    // provo ad aprire il file dei contatti
    file = fopen(path, "r");
    if(file < 0){
        perror("Non ?? stato possibile aprire i file");
        return -1;
    }

    ret  = -2;
    // leggo ogni utente dal file dei contatti
    while((read=getline(&line, &l, file)) != -1){
        line[strcspn(line, "\n")] = 0;

        if(strcmp(line, user) == 0){
            ret = 1;
            break;
        }
    }

    // chiudo il file
    fclose(file);    
    return ret;
}

// gestisce l'arrivo di un messaggio P2P
void receive_p2p_msg(int device, char *buffer){

    char src[MAX_USERNAME_SIZE];
    char formatted_msg[MAX_REQUEST_LEN];

    // genero la richiesta per la gui
    strcpy(src, get_username_by_connection(&con, device));

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

    // TEST CONNECTION (gi?? verificato prima!)
    // se l'utente ?? gi?? connesso non devo interrogare il server,
    // bens?? provare a comunicarci e vedere se il messaggio arriva.
    // Se arriva si procede normalente, in caso contrario ?? necessario
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
    receive_from_server(buffer);
    sscanf(buffer, "%d %s", &response, address);
    

    // se l'utente ?? online la risposta sar?? > 0
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
            perror("Non ?? possibile connettersi con il nuovo device");
            slog("La porta di connessione era: %d", response);
            return NULL;
        }

        // il device che ha fatto la richiesta aggiunge
        FD_SET(fd, &master);
        if(fd > fdmax) fdmax = fd;

        // genero la richiesta di verifica hash e la invio al nuovo dispositivo
        sprintf(buffer, "%s %lu", con->username, my_hash);
        send_device_request(fd, buffer);

        // aggiungo alla lista la nuova connessione
        newcon = new_connection(&con, fd);
        set_connection(&con, dst, fd);  // imposto il nome del nuovo socket
        newcon->port = response;        // imposto la porta

        return newcon;
    }

    // il dispositivo non ?? online
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


    // Il file ?? stato ricevuto, ?? compito allora del network aggiornare
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
        receive_p2p_msg(fd, line);
    }

    // chiudo il file
    fclose(file);

    return;
}


// L'utente ?? stato trovato offline, ?? dunque necessario aggiungere il
// messaggio inviato come pendente al server, in modo che venga inviato
// quando l'untente far?? l'accesso
int send_pendant_to_server(char dst[MAX_USERNAME_SIZE], char msg[MAX_MSG_SIZE]){
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

    // per ogni partecipante alla chat, invio un messaggio
    for(p = partecipants; p != NULL; p = p->next){

        // recupero il nome del partecipante
        strcpy(dst, p->username);

        // verifico se esiste una connessione precedente con il device
        dst_connection = find_connection(&con, dst);

        
        // se non trovo nessuna connessione
        if(dst_connection == NULL){

            // chiedo di instaurare una connessione con l'utente
            // l'utente potrebbe per?? essere offline (prima o verificato
            // se la connessione esisteva, non se pu?? essere creata) 
            slog("--- creo nuova connessione ---");
            dst_connection = create_connection(dst);

            // se ancora non ?? stato possibile realizzarla,
            // allora l'utente ?? offline
            if (dst_connection == NULL){

                // mando il messaggio come pendente al server
                ret = send_pendant_to_server(dst, msg);
            }

            // la connessione adesso ?? stata stabilita, dunque si 
            // manda il messaggio direttamente al socket
            else {
                sprintf(buffer, "%s ?? attualmente online", dst);
                notify(buffer, ANSI_COLOR_GREEN);
                ret = send_request(CHAT_CODE, dst_connection->socket, msg);
            }
        }

        // connessione trovata perch?? creata precedentemente
        else {
            ret = send_request(CHAT_CODE, dst_connection->socket, msg);
        }

    }

    // il messaggio ?? ora stato inviato, deve per?? essere salvato nella cronologia!
    // Ci?? per?? ?? valido solamente se la connessione ?? singola
    // Nota: lo dobbiamo aggiungere solo se correttamente inviato
    if(connection_size(&partecipants) < 2 && ret > 0){
        append_sent_msg_to_historic(original, msg);
    }

    if(ret > 0) ret = 1;
    return ret;
}

// Consente l'invio di un file verso un device destinatario
int send_file(int device, char* path){
    FILE *file;
    char cmd[100];
    char buffer[MAX_REQUEST_LEN];
    char real_path[PATH_SIZE];
    char c;
    int size;

    // recupero il path della cartella
    sprintf(real_path, "./devices_data/%s/%s", con->username, path);

    // mi assicuro dell'esistenza del file generandolo su bisogno
    sprintf(cmd, "mkdir -p ./devices_data/%s/ && touch %s", con->username, real_path);   // mi assicuro che il file esista
    system(cmd);
    
    // apro il file
    file = fopen(real_path, "r");
    if(!file) {
        perror("Errore apertura file per l'invio");
        return -1;
    }

    // calcolo dimensione file
    fseek(file, 0L, SEEK_END);
    size = ftell(file);

    slog("Invio del file %s con dimensione %d", real_path, size);


    // invia il numero di byte da inviare all'altro dispositio
    rewind(file);
    sprintf(buffer, "%d", size);
    send_device_request(device, buffer); 
    
    do{
        c   = fgetc(file);
        ret = send(device, &c, 1, 0);
        if(ret < 0){
            perror("errore invio file");
            return -1;
        }    
    } while(c != EOF);
    
        
    // chiudo il file
    fclose(file);
    return 1;
}

// funzione utilizzata per la ricezione di file
int receive_file(int fd, char *path){
    FILE* file;
    char buffer[MAX_REQUEST_LEN];
    char dirpath[100];
    char cmd[100];
    int size;
    char c;


    strcpy(dirpath, path);    
    dirname(dirpath);
    sprintf(cmd, "mkdir -p %s && touch %s", dirpath, path);   // mi assicuro che il file esista
    system(cmd);

    // fino al termine del file continuo a chiedere
    file = fopen(path, "w");

    // ricevo il numero di byte che mi verranno inviati
    receive_from_device(buffer, fd);
    sscanf(buffer, "%d", &size);
    slog("buffer=> %s, sto per ricevere %d byte", buffer, size);

    // =========================
    while(size >= 0){
        ret = recv(fd, &c, 1, 0);
        if(ret < 0){
            perror("errore ricezione file");
            slog("errore ricezione file");
            return -1;
        }

        size  = size - 1;
        if (size > -1) fputc(c, file);
    }
    // =========================
    slog("ho finito di ricevere");

    // chiudo il file
    fclose(file);
    return 0;
}

// riceve un file da un altro device
int receive_file_from_device(int fd, char* path){
    char realpath[PATH_SIZE];
    
    // converto la richiesta
    sprintf(realpath, "./devices_data/%s/%s", con->username, path);
    slog("salver?? in: %s", realpath);

    ret = receive_file(fd, realpath);
    slog("receive_file_from_device riceve %d", ret);
    return ret;
}

// invial la richiesta di hanging verso il server
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
    ret = receive_file(sd, path);
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
    ret = receive_file(sd, path);
    if (ret < 0) return ret;

    // Il file ?? stato ricevuto, ?? compito allora del network aggiornare
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
        perror("Non ?? stato possibile aprire i file");
        return -1;
    }

    // leggo ogni utente dal file dei contatti
    while((read=getline(&line, &l, contacts)) != -1){

        line[strcspn(line, "\n")] = 0;

        sprintf(buffer, "%d %s", ISONLINE_CODE, line);
        slog("richiedo per: %s", buffer);
        ret = send_server_request(buffer);
        if (ret == -1) break;

        ret = receive_code_from_server();
        if (ret == -1) break;

        // se l'utente risulta online, lo scrivo tra i disponibili
        if (ret > 0){
            fprintf(available, line);
            fprintf(available, "\n");

            // informo che tutto ?? andato correttamente
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
    uint16_t flag = 0;

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
    ret = receive_code_from_device(dst->socket);

    return ret;
}

// funzione che consente l'aggiunta di un utente a una chat
int add_user(char *dst){
    struct connection *p, *c;
    char buffer[MAX_REQUEST_LEN];

    // un utente da aggiungere non pu?? essere un utente gi?? partecipante
    if(find_connection(&partecipants, dst) != NULL) return -2;

    // aggiungo il nuovo partecipante alla lista dei partecipanti
    c = find_connection(&con, dst); 
    if(c== NULL){
        c = create_connection(dst);

        // se l'utente non esiste, usciamo
        if(c == NULL) return -2;
    }
    
    // mando l'invito all'utente
    ret = send_group_invite(c);
    if(ret <= 0) return ret;

    notify("Nuova chat di gruppo avviata", ANSI_COLOR_GREY);
    GROUPMODE = 1;

    // scorro tutti i partecipanti alla chat
    for(p = partecipants; p != NULL; p = p->next){

        // la richiesta non deve essere inoltrata all'utente invitato
        if(strcmp(p->username, dst) == 0) continue;

        // se la connessione con un partecipanti non ?? ancora 
        // presente la forzo
        c = find_connection(&con, p->username);
        if(c == NULL){
            c = create_connection(p->username);
            set_connection(&con, p->username, c->socket);

            // se la connessione non pu?? essere instaurata esco
            // nota: ?? il caso quando il server ?? offline
            if(c == NULL) return -1;
        }

        // la connessione tra l'host e il partecipante ?? ora assicurata
        // invio la richiesta di aggiunta utente al device p
        // nota: si usa il socket su c perch?? i partecipanti non hanno socket aggiornati
        sprintf(buffer, "%d %s", ADDUSER_CODE, dst);
        ret = send_device_request(c->socket, buffer);
        if (ret == -1) return -1;

        sprintf(buffer, "%d", ENABLEGROUP_CODE);
        ret = send_device_request(c->socket, buffer);
        if (ret == -1) return -1;        

    }   

    // aggiungiamo la nuova connession
    new_passive_connection(&partecipants, dst);
    return 0;
}

// gestisce la ricezione di richiesta da altri devices
void device_handler(int device){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    char tmp[MAX_REQUEST_LEN];          // buffer temporaneo
    int16_t code;                     // codice della richiesta effettuata
    struct connection *newp;            // per la richiesta di nuovo partecipante

    // ricevo dal buffer la richiesta
    receive_from_device(buffer, device);
    sscanf(buffer, "%hd %[^\t\n]", &code, buffer);

    slog("[NET -> NET] %hd %s", code, buffer);

    switch(code){

        // DISCONNECT REQUEST - un dispositivo invia una richiesta di disconnessione
        case LOGOUT_CODE:
            sprintf(buffer, "%s", get_username_by_connection(&con, device));

            // solo se trovo il nome stampo la notifica a schermo
            if (strcmp(buffer, "(null)") != 0){
                sprintf(buffer, "Utente %s disconnesso", get_username_by_connection(&con, device));
                notify(buffer, ANSI_COLOR_RED);
            }

            // se l'utente che esce aveva qualche chat attiva, si rimuovono i partecipanti
            newp = find_connection(&partecipants, get_username_by_connection(&con, device));
            if(newp != NULL) remove_connection(&newp);

            close_connection_by_socket(&con, device);
            FD_CLR(device, &master);
            break;

        case CHAT_CODE:
            // NOTA: in buffer ?? presente il contenuto del messaggio

            // gestisco graficamente la ricezione del messaggio
            receive_p2p_msg(device, buffer);
            
            // il messaggio ricevuto deve anche essere scritto nella cronologia, 
            // solamente se si tratta di una chat singola
            if (connection_size(&partecipants) < 2)
                append_received_msg_to_historic(get_username_by_connection(&con, device), buffer);

            break;

        // ricezione richiesta di aggiunta a un gruppo
        case ADDUSER_CODE:

            // provo a instaurare una connessione con il nuovo utente
            newp = create_connection(buffer);

            // se la connessione non ?? possibile, esco
            if (newp == NULL) break;

            // altrimenti lo aggiungo ai partecipanti
            new_passive_connection(&partecipants, buffer);
            break;

        // risponde alla richiesta di invito
        case INVITEGROUP_CODE:

            // se non ?? gi?? occupato in una chat accetta l'invito
            if (connection_size(&partecipants) == 0) {
                ret = 1;
                
                new_passive_connection(&partecipants, find_connection_by_socket(&con, device)->username);
                switch_to_group(device);
            }

            else {
                ret = 0;
            }

            // restitusco l'esito dell'invito: 1 se ?? stato accettato, 0 se invece ?? stato respinto
            code = htons(ret);
            send(device, (void*) &code, sizeof(uint16_t), 0);

            // il dispositivo dovrebbe fare logout
            if(ret == 0) {
                sprintf(buffer, "Chat di gruppo rifiutata, %s disconnesso", get_username_by_connection(&con, device));
                notify(buffer, ANSI_COLOR_RED);

                logout_device(device);
            }

            break;
        
        // abilita la visulaizzazione della chat di gruppo
        case ENABLEGROUP_CODE:

            // costringo l'interfaccia grafica a far partire la chat solo
            // se non gi?? attiva
            if (connection_size(&partecipants) < 2){
                switch_to_group();
            }
            else {
                notify("E' stata avviata una chat di gruppo", ANSI_COLOR_GREY);
                GROUPMODE = 1;
            }

            new_passive_connection(&partecipants, find_connection_by_socket(&con, device)->username);
            break;

        // invio richiesta di uscita dalla chat
        case QUITCHAT_CODE:
            // se sono ancora in chat con l'utente che mi ha scritto
            if(find_connection(&partecipants, get_username_by_connection(&con, device)) != NULL){
                sprintf(buffer, "%s ?? uscito dalla chat", get_username_by_connection(&con, device));
                notify(buffer, ANSI_COLOR_MAGENTA);
            }

            // rimuovo che il partecipante solamente nelle chat di gruppo. Questo ?? necessario
            // per consentire l'invio di messaggi pendenti nel server quando l'utente va offline
            // oppure esce dalla chat
            if(GROUPMODE == 1) remove_connection_by_username(&partecipants, get_username_by_connection(&con, device));
            
            if(connection_size(&partecipants) < 1 && GROUPMODE == 1) {
                notify("Nessun utente attualmente in chat", ANSI_COLOR_CYAN);   
            }

            break;

        // ricevuta richiesta di ricezione file
        case SHARE_CODE:
            // nota: buffer contiene il path in cui salvare il file
            slog("sto per ricevere %s byte", buffer);
            ret = receive_file_from_device(device, buffer);

            // se il file ?? stato correttamente ricevuto
            if(ret >= 0) {
                sprintf(tmp, "Ricevuto file \"%s\"", buffer);
                notify(tmp, ANSI_COLOR_GREEN);
            }
            // altrimenti notifico che c'?? stato qualche problema
            else {
                sprintf(tmp, "Errore ricezione file \"%s\"", buffer);
                notify(tmp, ANSI_COLOR_RED);
            }

            break;
    }
}

// informo gli utenti l'uscita dalla chat
int send_quitchat_to_device(int fd){
    char buffer[MAX_REQUEST_LEN];

    sprintf(buffer, "%d %s", QUITCHAT_CODE, con->username);
    ret = send_device_request(fd, buffer);

    return ret;
}

// permette di effettuare lo share di un file
// la richiesta contiene i campi: [CODE] [USER] [PATH]
int share_to_device(char* buffer){
    char user[MAX_USERNAME_SIZE];
    char path[PATH_SIZE], full_path[PATH_SIZE];
    struct connection *dst_connection;

    sscanf(buffer, "%s %[^\t\n]", user, full_path);

    // impedisce l'invio di file presenti in altre cartelle
    strcpy(path, basename(full_path));

    // controllo che l'utente sia nella rubrica
    ret = check_user_in_contacts(user);
    if(ret < 0) return ret;    

    // verifico se esiste una connessione precedente con il device
    dst_connection = find_connection(&con, user);
        
    // se non trovo nessuna connessione
    if(dst_connection == NULL){

        // chiedo di instaurare una connessione con l'utente
        // l'utente potrebbe per?? essere offline (prima o verificato
        // se la connessione esisteva, non se pu?? essere creata) 
        slog("--- creo nuova connessione ---");
        dst_connection = create_connection(user);

        // se ancora non ?? stato possibile realizzarla,
        // allora l'utente ?? offline
        if (dst_connection == NULL){
            ret = -2;
        }

        // la connessione adesso ?? stata stabilita
        else {
            // invio il file
            ret = send_request(SHARE_CODE, dst_connection->socket, path);
            if(ret < 0) return ret;

            ret = send_file(dst_connection->socket, path);
        }
    }

    // connessione trovata perch?? creata precedentemente
    else {
        // invio il file
        ret = send_request(SHARE_CODE, dst_connection->socket, path);
        if(ret < 0) return ret;
        
        ret = send_file(dst_connection->socket, path);
    }
    
    return ret;
}

// gestisce le richieste provenineti dalla gui
void gui_handler(){
    char read_buffer[MAX_REQUEST_LEN];  // richiesta ricevuta dalla GUI
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    int16_t code;                     // codice della richiesta effettuata
    int server_port;                    // porta su cui contattare il server
    char username[MAX_USERNAME_SIZE], password[MAX_USERNAME_SIZE];
    char hash_buffer[MAX_USERNAME_SIZE + MAX_PW_SIZE + 1];
    struct connection *p, *q;
    FILE* file;
    unsigned long hardclose;

    // recupero il tipo di richiesta in base al codice
    read(to_parent_fd[0], read_buffer, MAX_REQUEST_LEN);
    sscanf(read_buffer, "%hd %[^\t\n]", &code, buffer);

    // verifico le richieste ricevute dalla gui
    switch (code){

        // SIGNUP REQUEST
        case SIGNUP_CODE:
            // la prima cosa da fare ?? creare la connessione con il server
            sscanf(buffer, "%d %s %s", &server_port, username, password);
            ret = init_server_connection(server_port);
            // se non ?? stato possibile connettersi al server
            // chiedo di tornare indietro
            if (ret < 0) break;

            sprintf(buffer, "%hd %s %s %d", SIGNUP_CODE, username, password, DEVICE_PORT);
            send_server_request(buffer);

            // 1 creato, 0 se non creato, -1 errore
            ret = receive_code_from_server();

            // disconnettiamo dal server
            FD_CLR(sd, &master);
            close(sd);
            break;

        // LOGIN REQUEST
        case LOGIN_CODE:
            sscanf(buffer, "%d %s %s", &server_port, con->username, password);
            // la prima cosa da fare ?? creare la connessione con il server
            ret = init_server_connection(server_port);

            // se non ?? stato possibile connettersi al server
            // chiedo di tornare indietro
            if (ret < 0) break;


            sprintf(read_buffer, "./devices_data/%s/%s", con->username, LOGOUT_FILE);
            if((file = fopen(read_buffer, "r"))){
                fscanf(file, "%lu", &hardclose);                // leggo il timestamp scritto 
                fclose(file);                                   // chiudo il file
                sprintf(buffer, "rm %s", read_buffer);          // elimino il file
                system(buffer);                                 // eseguo il comando di delete
            }
            else {
                hardclose = 0;                                  // non bisogna forzarlo
            }

            // invio la richiesta di accesso al server
            sprintf(buffer, "%hd %s %s %d %lu", LOGIN_CODE, con->username, password, con->port, hardclose);
            ret = send_server_request(buffer);
            if (ret <= 0) break;

            // 1 se ?? connesso, 0 se non trovato, -2 se gi?? acceduto
            ret = receive_code_from_server();

            // Se il risultato ?? 0 le credenziali erano errate, se il risultato era -2 era gi?? online l'utente
            // In entrambi i casi la connessione deve essere eliminata
            if (ret == -2 || ret == 0) {
                close(sd);
                FD_CLR(sd, &master);
                break;
            }  
            // in tutti gli altri (negativi) casi torno indietro
            else if( ret < 0) 
                break;

            // genero il mio hash al login
            sprintf(hash_buffer,"%s %s", con->username, password);
            my_hash = hash(hash_buffer);

            break;

        // gestione richiesta di una nuova chat
        case CHAT_CODE:
            // Verifico che l'utente con il quale si vuole parlare sia effettivamente
            // nella lista dei contatti
            ret = check_user_in_contacts(buffer);
            if (ret <= 0) break;

            // si recuperano ii messaggi non ancora recapitati dal mittente stesso
            ret = show_request_server(buffer);
            if(ret < 0) break;
            
            // aggiungo il partecipante alla lista
            new_passive_connection(&partecipants, buffer);
            break;
        
        // CHECK ONLINE REQUEST
        case ISONLINE_CODE:
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
            logout_devices();
            logout_server();
            slog("[GUI->NET:%d] ricevuto richiesta di disconnessione", con->port);
            break;

        // gestisce l'uscita dalla chat eleminando gli utenti dalla chat
        case QUITCHAT_CODE:
            for(p = partecipants; p != NULL; p = p->next){
                // ogni utente con cui stavo parlando deve essere notificato del mio logout
                q = find_connection(&con, p->username);
                if(q != NULL) {
                    ret = send_quitchat_to_device(q->socket);
                    if (ret < 0) break;
                }
            }

            // rimuovo tutti gli utenti partecipanti alla chat
            clear_connections(&partecipants);
            GROUPMODE = 0;
            ret = 1; // segnalo che ?? tutto a posto
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
            // nota: buffer contiene il nome dell'utente da aggiungere

            // controlla se l'utente appartiene ai propri contatti
            ret = check_user_in_contacts(buffer);
            if(ret < 0) break;

            // aggiunge l'utente alla chat
            ret = add_user(buffer);
            break;

        case SHARE_CODE:
            // nota: buffer contiene il nome dell'utente a cui mandare il file
            ret = share_to_device(buffer);

            if(ret > 0){
                printf(ANSI_COLOR_GREEN "File inviato correttamente" ANSI_COLOR_RESET);
                printf("\n\n");
                fflush(stdout);
            }
            else {
                printf(ANSI_COLOR_RED "C'?? stato un errore con l'invio del file" ANSI_COLOR_RESET);
                printf("\n\n");
                fflush(stdout);
            }
            break;

        case SHAREGROUP_CODE:
            // nota: buffer contiene il nome del file da inviare

            if (connection_size(&partecipants) < 1){
                printf(ANSI_COLOR_RED "Nessun destinatario a cui inviare il file!" ANSI_COLOR_RESET);
                printf("\n\n");
                fflush(stdout);
                break;
            }

            for(p = partecipants; p != NULL; p = p->next){
                sprintf(read_buffer, "%s %s", p->username, buffer);
                ret = share_to_device(read_buffer);

                if(ret < 0) break;
            }

            if(ret > 0){
                printf(ANSI_COLOR_GREEN "File inviato correttamente" ANSI_COLOR_RESET);
                printf("\n\n");
                fflush(stdout);
            }

            else {
                printf(ANSI_COLOR_RED "C'?? stato un errore con l'invio del file" ANSI_COLOR_RESET);
                printf("\n\n");
                fflush(stdout);
            }
            
            break;

        // BAD REQUEST
        default:
            ret = -1;
            slog("BAD REQUEST");
            break;
    }

    // restituisco la risposta
    write(to_child_fd[1], &ret, sizeof(ret));
}


void startNET(){
    // handler per la disconnessione
    signal(SIGINT, intHandler);
    int i  = 0;
    ret = 0;

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

            // se il socket i ?? settato,
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

                // se ?? in arrivo una nuova richiesta di connessione da un device
                // [LISTENER-HANDLER]
                else if (i == listener){
                    int fd;
                    int16_t result;
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
                    ret = receive_from_device(req, fd);

                    // controllo mediante server se l'utente ?? chi dice di essere
                    ret = send_request(WHOIS_CODE, sd, req);

                    // attendo la risposta dal server
                    result = receive_code_from_server();

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

                        // notifica al collegamento con un device
                        sprintf(formatted_msg, "%s si ?? appena collegato", get_username_by_connection(&con, fd));
                        notify(formatted_msg, ANSI_COLOR_GREEN);

                        // se siamo in un gruppo, la nuova connessione deve immediatamente essere considerata partecipante
                        if(GROUPMODE) new_passive_connection(&partecipants, get_username_by_connection(&con, fd));
                        
                        // invio la richiesta di notifica
                        show_request_server(username);
                        show_as_p2p(fd);
                    }
                    
                    // autentificazione fallita, chiudo la comunicazione
                    else {
                        printf("\nconnessione malevola individuata\n");
                        fflush(stdout);
                        slog("connessione malevola individuata");
                        close(fd);
                    }

                }
                

                // [DEVICES-HANDLER] gestione delle richieste di altri dispositivi
                else {
                    device_handler(i);
                }

                if (ret < 0) slog("Problema sconosciuto verificato nel network");
            }
        }
    } while(1);
}
