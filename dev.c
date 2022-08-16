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

#include "./utils/costanti.h"
#include "./API/logger.h"
#include "./source/chat.h"

// COSTANTI  ================================
const char MENU[] = 
    "***************************\n"
    "           MENU            \n"
    "***************************\n"
    "1) signup\n"
    "2) in\n\n";


const char MENU2[] = 
    "***************************\n"
    " Utente: %s:%d            \n"
    "***************************\n"
    "1) hanging\n"
    "2) show\n"
    "3) chat\n"
    "4) share\n"
    "5) out\n\n";
    //"Selezionare quale operazione compiere: ";

const char ADDRESS[] = "127.0.0.1";
// ==========================================

int 
    ret,      // return value
    len,      // lunghezza
    sd,       // socket di comunicazione con il server
    listener = -1, // socket di ascolto per messaggi e richieste
    fdmax = -1,    // id di socket massimo
    csd;    // socket di comunicazione

fd_set master;

struct connection DEVICE;

struct sockaddr_in 
    servaddr,               // indirizzo del server
    myaddr,                 // indirizzo del device
    conectaddr,             // indirizzo del device a cui si manda la richiesta
    ndaddr;                 // indirizzo del nuovo device


struct device_info info;

// gestisce il segnale di interruzione
void intHandler() {
    int i;

    slog("In chiusura  per SIGINT %s:%d, max socket: %d", DEVICE.username, DEVICE.port, fdmax);

    //close(sd);
    //slog("==> chiuso socket %d", sd);
  
    // vengono chiusi tutti i socket in uso dal device
    /*for (i = 0; i <= fdmax; i++){
        slog("==> chiuso socket %d", i);
        close(i);
    }*/

    exit(0);
}


// funzione per l'inizializzazione del server
int init(const char* addr, int port){

    sd  = socket(DOMAIN, SOCK_STREAM, 0);

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

    slog("Device (%d) ricevuto (socket non ancora attivo)", port);
}




// recupera la porta inserita in fase di avvio
int portCheck(int argc, char* argv[]){
    if(argc < 2){
        printf("Errore, porta non specificata\n");
        exit(-1);
    }

    return atoi(argv[1]);
}


// registrazione di un nuovo utente
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

// inizializzazione del socket di ascolto per ricevere richieste
// di connessione da altri device 
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


/*
// funzione di utility per aggiornare il valore massimo
void update_max_socket(int *max, int value){
    if(value  > *max)
        *max = value;
}
*/

/*
void wait_for_event(fd_set *m, int l, int *max, struct connection *connections){
    int i, ret, new_fd, addr_len;
    struct sockaddr_in req_addr;

    fd_set readers;
    FD_ZERO(&readers);  

    readers = (*m);  
    ret = select(*max+1, &readers, NULL, NULL, NULL);
    if(ret<0){
        perror("Errore nella select");
        exit(1);
    }

    // scorro tutti i socket cercando quelli che hanno avuto richieste
    for(i = 0; i < *max; i++){

        // se il socket i è settato
        if(FD_ISSET(i, &readers)){

            // LISTENER: verifica se ci sono nuove richieste di connessione
            if (i == l){

                // memorizzo il socket della richiesta in arrivo
                new_fd = accept(l, (struct sockaddr *)&req_addr, &addr_len);

                // richiedo l'identificazione
                // TODO: identify();

                // TODO: aggiungere una connessione al campo connessioni

                FD_SET(new_fd, m);
                slog("Nuova richiesta accettata correttamente su socket: %d", new_fd);

                update_max_socket(max, l);
            }

            if (i == fileno(stdin)){
                // gestione input utente
            }

            // è un messaggio ricevuto da un utente
            else {

            }
        }
    }
}
*/
/*
void wait_for_event(struct device_info *info){
    int i, ret, new_fd, addr_len;
    struct sockaddr_in req_addr;

    fd_set readers;
    FD_ZERO(&readers);  

    readers = *(info->master);  
    ret = select(*(info->fd_max)+1, &readers, NULL, NULL, NULL);
    if(ret<0){
        perror("Errore nella select");
        exit(1);
    }

    // scorro tutti i socket cercando quelli che hanno avuto richieste
    for(i = 0; i < *(info->fd_max); i++){

        // se il socket i è settato
        if(FD_ISSET(i, &readers)){

            // LISTENER: verifica se ci sono nuove richieste di connessione
            if (i == info->listener){

                // memorizzo il socket della richiesta in arrivo
                new_fd = accept(info->listener, (struct sockaddr *)&req_addr, &addr_len);

                // richiedo l'identificazione
                // TODO: identify();

                // TODO: aggiungere una connessione al campo connessioni

                FD_SET(new_fd, info->master);
                slog("Nuova richiesta accettata correttamente su socket: %d", new_fd);

                update_max_socket(info->fd_max, i);
            }
 

            if (i == fileno(stdin)){
                // gestione input utente
            }

            // è un messaggio ricevuto da un utente
            else {

            }
        }
    }
}
*/

void chat_handler(struct device_info *info){

    int dest_port = 1235;               // porta del dispositivo da raggiungere
    char dest[16];                      // nome del dispositivo da raggiungere

    // nome utente da contattare
    printf("Inserire il nome utente con cui comunicare: ");
    fflush(stdout);

    // aspetto un input
    wait_for_msg(info, NULL, -1);
    scanf("%s", dest);                  // recupero il nome  del destinatario
    fflush(stdin);

    // TODO: chiedo al server se l'utente è online
        // il server mi risponde con la porta, -1 se offline

    if(dest_port == -1){
        // mando una richiesta di "save_history" al server per ogni messaggio digitato
    }
    else {
        start_chat(info, dest, dest_port); 
        // gestisco l'invio di messaggi
    }
    
}


// gestisce il menu utente dopo che un utente si è loggato
void logged(char* username, int port){
    int answer = 1;
    int pid;
    int i;
    int ret = -1;

    struct user usr;

    init_listen_socket(port);                   // il socket è aperto solo dopo il login

    // create user info
    strcpy(usr.username, username);
    strcpy(DEVICE.username, username);
    //DEVICE.username = usr.username;
    DEVICE.port = port;
    DEVICE.socket = listener;

    FD_ZERO(&master);    
    FD_SET(listener, &master);                  // aggiungo il socket in ascolto dei device
    FD_SET(sd, &master);                        // aggiungo le risposte dal server in ascolto
    FD_SET(fileno(stdin), &master);             // aggiungo lo stdin in ascolto

    // imposto le informazioni del device
    info.listener = listener;
    info.fd_max = &fdmax;
    info.master = &master;
    info.username = username;
    info.port = port;

    printf("\e[1;1H\e[2J");                 // cancella il terminale
    printf(MENU2, username, port);          // mostra il menu delle scelte
    fflush(stdout);                         // mostra in uscita tutto l'ouput
    
    do{
        
        ret = wait_for_msg(&info, NULL, -1);          // verifico tutte le richieste
        if(ret != -1) continue;

        scanf("%d", &answer);                   // prendo in input la scelta del comando
        fflush(stdin);                          // pulisco l'input
        
        switch (answer){

            case 0:
                answer = 0;
                break;

            case 1:
                slog("scelta 1 individuata");
                break;

            case 2:
                break;

            case 3:
                // CHAT
            
                chat_handler(&info);
                break;
        
        default:
            break;
        }

    }while (answer != 0);


    // se sono arrivato qua significa che 
    // è stata richiesta una disconnessione,
    // si chiudono dunque tutti i socket aperti
    // dopo il login per le comunicazioni
    for (i = 0; i < fdmax; i++){
        close(i);
        perror("chiuso socket");
        FD_CLR(i, &master);
    }

    // TODO: logout
}

int login(int port){

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
        logged(usr.username, port);        
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
        printf("Selezionare quale operazione compiere: ");
        scanf("%d", &answer);

        switch(answer){
            case 1:            
                signup();   
                break;

            case 2:
                login(port);
                break;

            default:
                answer = -1; // segno che è scorretta
                printf("Scelta scorretta, reinserire: ");
                break;
        }

    } while (1); //  ciclo fino a quando non è corretta
    

    return 0;
} 
