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
#include "../utils/connection.h"
#include "../API/logger.h"
// ====================


// EXTERN ====================
extern int to_child_fd[2];      // pipe NET->GUI
extern int to_parent_fd[2];     // pipe GUI->NET
extern int pid;                 // pid del processo
extern struct connection con;   // informazioni sulla connessione
// ===========================

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
    myaddr;                 // indirizzo del device
    //connectaddr,             // indirizzo del device a cui si manda la richiesta
    //ndaddr;                 // indirizzo del nuovo device


// LOCALHOST
const char ADDRESS[] = "127.0.0.1";
// ===========================================================================

void logout_devices(){
    struct connection *p, *next;
    char buffer[MAX_REQUEST_LEN];

    // la testa non deve essere eliminata
    for (p = con.next; p != NULL; p = next){

        // salvo il puntatore al prossimo elemento
        next = p->next;

        // genero la richiesta
        sprintf(buffer, "%d %s", LOGOUT_CODE, con.username);

        // invio la richiesta al device
        ret = send(p->socket, (void *) buffer, sizeof(buffer), 0);
        if(ret > 0){

            // se è andata a buon fine termino
            close_connection(&con);
        }   
    }
}

// gestione chiusura
void intHandler() {
    //slog("In chiusura  per SIGINT %s:%d, max socket: %d", DEVICE.username, DEVICE.port, fdmax);

    // creo il buffer per la richiesta
    char buffer[MAX_REQUEST_LEN];

    // creo il codice identificativo
    short int code = LOGOUT_CODE;

    // genero la richiesta di disconnessione dal server
    sprintf(buffer, "%d %s", code, con.username);
    ret = send(sd, (void *) buffer, sizeof(buffer), 0);
    if(ret > 0){
        // se è andata a buon fine termino
        //close(sd);
        close_connection_by_socket(&con, sd);
        slog("==> chiuso socket %d", sd);
        exit(0);
    }

    logout_devices();
}

int init_listen_socket(int port){
    listener = socket(DOMAIN, SOCK_STREAM, 0);
    if (listener < 0){
        perror("Errore apertura socket");
        exit(-1);
    }

    // imposto il socket di ascolto
    con.socket = listener;

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
            if(sent_bytes == -1){
                perror("errore invio");
                exit(-1);
            } 
            
            bytes_to_send -= sent_bytes;
            buf += sent_bytes;
    }

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
void recive_from_device(char buffer[MAX_REQUEST_LEN], int fd){
    memset(buffer, 0, MAX_REQUEST_LEN); // resetto il contenuto del buffer
    
    int bytes_to_receive, received_bytes;
    char *buf = buffer;    


    bytes_to_receive = MAX_REQUEST_LEN;
    while(bytes_to_receive > 0) {
        received_bytes = recv(fd, buf, bytes_to_receive, 0);
        if(received_bytes == -1){
            perror("errore ricezione da server per network");
            exit(-1);
        }
                            
        bytes_to_receive -= received_bytes;
        buf += received_bytes;
    }
}


void server_handler(){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata

    // ricevo dal buffer
    recive_from_server(buffer);
    sscanf(buffer, "%hd %s", &code, buffer);

    slog("ricevuto per net %hd e %s", code, buffer);

    switch(code){

        // DISCONNECT REQUEST
        case LOGOUT_CODE:
            slog("arrivata al net chiusura server");
            printf("\n************* SERVER OFFLINE **************\n\n");
            fflush(stdout);

            FD_CLR(sd, &master);
            break;
    }
}

void device_handler(int device){
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata

    // ricevo dal buffer
    recive_from_device(buffer, device);
    sscanf(buffer, "%hd %s", &code, buffer);

    slog("ricevuto per net DA DEVICE %hd e %s", code, buffer);

    switch(code){

        // DISCONNECT REQUEST
        case LOGOUT_CODE:
            slog("arrivata al net chiusura device");
            fflush(stdout);

            close_connection_by_socket(&con, device);
            FD_CLR(device, &master);
            break;
    }
}


// stabilisce una connessione con un destinatario se non presente
struct connection* create_connection(char dst[MAX_USERNAME_SIZE]){
    char buffer[MAX_REQUEST_LEN];   // buffer per le richieste
    int response;                   // salva il codice di risposta della richiesta
    char address[50];               // salva l'indirizzo in cui trovare il device
    int dest_port;                  // porta in cui contattare il device
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
    send_server_request(buffer);
    slog("[NET->SERVER]: %s", buffer);


    // FIRST CONNECTION - di seguito si esegue la prima connessione
    // recupero le informazioni sull'utente con cui voglio
    // stabilire una connessione e le traduco in:
    // [RISPOSTA INDIRIZZO PORTA], dove risposta stabilisce se l'utente 
    // non esiste(-1), offline(0),  online(1)
    recive_from_server(buffer);
    sscanf(buffer, "%d %s %d", &response, address, &dest_port);
    
    // TODO: testare la corretta ricezione dei dati
    if (response > 0){
        slog("risposta positiva: %d, %s, %d", response, address, dest_port);

        // inizializzo il socket per comunicare con il nuovo dispositivo
        fd  = socket(DOMAIN, SOCK_STREAM, 0);
        if(fd < 0){
            perror("Errore all'avvio del socket");
            exit(-1);
        }

        // aggiungo alla lista la nuova connessione
        new_connection(&con, fd);

        // preparazione indirizzi per raggiungere il server
        memset(&addr, 0, sizeof(addr)); // pulizia
        addr.sin_family = DOMAIN;
        addr.sin_port = htons(4242);
        inet_pton(DOMAIN, address, &addr.sin_addr);

        // connessione al server
        ret = connect(fd, (struct sockaddr*) &addr, sizeof(addr));
        if(ret < 0){
            perror("Errore nella connect");
            exit(-1);
        }

        // la connessione tra i device è avvenuta,
        // aggiungo alla lista delle connessioni
        slog("connessione tra i device avvenuta");
        newcon = new_connection(&con, fd);
        strcpy(newcon->username, dst);  // necessario, new_connection non salva il nome per compatibilità

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


// L'utente è stato trovato offline, è dunque necessario aggiungere il
// messaggio inviato come pendente al server, in modo che venga inviato
// quando l'untente farà l'accesso
void send_pendant_to_server(char dst[MAX_USERNAME_SIZE], char msg[MAX_MSG_SIZE]){
    //short int code = PENDANTMSG_CODE;

    char buffer[MAX_REQUEST_LEN];

    sprintf(buffer, "%d %s %s", PENDANTMSG_CODE, dst, msg);
    send_server_request(buffer);
}


// consente l'invio di un messaggio
void send_msg(char* buffer){
    char dst[MAX_USERNAME_SIZE];
    char msg[MAX_MSG_SIZE];
    struct connection* dst_connection;

    // ricavo il messaggio e il destinatario
    sscanf(buffer, "%s %[^\n\t]", dst, msg);
    
    // verifico se esiste una connessione precedente con il device
    dst_connection = find_connection(&con, dst);
    
    // se non trovo nessuna connessione
    if(dst_connection == NULL){

        // chiedo di instaurare una connessione con l'utente
        // l'utente potrebbe però essere offline (prima o verificato
        // se la connessione esisteva, non se può essere creata) 
        dst_connection = create_connection(dst);

        // se ancora non è stato possibile realizzarla,
        // allora l'utente è offline
        if (dst_connection == NULL){
            send_pendant_to_server(dst, msg);
        }
    }
    
}




void gui_handler(){
    char read_buffer[MAX_REQUEST_LEN];  // richiesta ricevuta dalla GUI
    char buffer[MAX_REQUEST_LEN];       // buffer con i parametri passati
    short int code;                     // codice della richiesta effettuata
    char answer[4];                     // codice della risposta

    // recupero il tipo di richiesta in base al codice
    len = read(to_parent_fd[0], read_buffer, MAX_REQUEST_LEN);
    sscanf(read_buffer, "%hd %[^\t\n]", &code, buffer);

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
        
        // CHECK ONLINE REQUEST
        case ISONLINE_CODE:
            send_server_request(read_buffer);
            ret = recive_code_from_server();
            break;

        // SEND MESSAGE REQUEST
        case SENDMSG_CODE:
            send_msg(buffer);
            ret = 0; // non significativo attualmente

            // cerca tra le connessioni attive se è 
            // già presente il destinatario
            

            /*
                - se ho una connessione con il destinatario:
                    invio il messaggio al suo socket
                
                - altrimenti:
                    verifico se posso avere una connessione
                    - se posso averla invio un messaggio direttamente
                    - altrimenti invio un messaggio al server
            */
            break;

        case LOGOUT_CODE:
            slog("[NET] ricevuto richiesta di disconnessione");
            logout_devices();
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
                    server_handler();
                }

                // [DEVICES-HANDLER] gestione delle richieste di altri dispositivi
                else {
                    device_handler(i);
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