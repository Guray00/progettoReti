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

#include "./chat.h"
#include "../API/logger.h"
#include "../utils/costanti.h"

// pulisce il terminale
void clear_terminal(){
        printf("\e[1;1H\e[2J");  // cancella il terminale
        fflush(stdout);
}

// funzione di utility per aggiornare il valore massimo
void update_max(int *max, int value){
    if(value  > *max)
        *max = value;
}

// protocollo di identificazione tra device
void identification(){

}

// info -> contiene le informazioni generiche sul device
// dest -> se disponibile, contiene il nome del destinatario a cui rispondere
// sd   -> se disponibile, contiene il numero del socket su cui rispondere
int wait_for_msg(struct device_info *info, char* dest, int sd){
    int i, len, ret, new_fd;
    struct sockaddr_in req_addr;
    socklen_t adl;

    char buffer[512];        // buffer di messaggi da leggere
    char msg[512];           // contenuto del messaggio
    int msg_size = 0;       // dimensione del messaggio


    int ret_value = -1;

    fd_set readers;
    FD_ZERO(&readers);  

    slog("------ [%s in attesa] ------", info->username);

    //  seleziono i socket che sono attivi
    readers = *(info->master);  
    ret = select(*(info->fd_max)+1, &readers, NULL, NULL, NULL);
    if(ret<=0){
        perror("Errore nella select");
        exit(1);
    }

   
    // scorro tutti i socket cercando quelli che hanno avuto richieste
    for(i = 0; i <= *(info->fd_max); i++){

        // se il socket i è settato
        if(FD_ISSET(i, &readers)){

            // LISTENER: verifica se ci sono nuove richieste di connessione da devices
            if (i == info->listener){
                slog("%s ha individuato una richiesta di connessione su: %d", info->username, i);

                // memorizzo il socket della richiesta in arrivo
                new_fd = accept(info->listener, (struct sockaddr *)&req_addr, &adl);
                if(new_fd <= 0){
                    perror("Impossibile accettare la richiesta");
                    continue;
                }

                // richiedo l'identificazione
                // TODO: identify();

                // TODO: aggiungere una connessione al campo connessioni

                FD_SET(new_fd, info->master);
                slog("%s ha accettato correttamente la richiesta, nuovo socket: %d", info->username, new_fd);

                update_max((info->fd_max), new_fd);
                ret_value = 0;
            }
  
            // verifico se c'è stata una richiesta di input,
            // nel caso l'utente ha intenzione di mandare un messaggio
            else if (i == fileno(stdin) && dest != NULL && sd != -1){
                // se scrivo un messaggio

                slog("%s ha scritto un messaggio, invierà su: %d", info->username, sd);
                
                fgets(msg, 512, stdin);
                msg_size = strcspn(msg, "\n");
                msg[msg_size] = 0;

                sprintf(buffer, "%d|%s", msg_size, msg);
                slog("buffer contiene: %s", buffer);
                fflush(stdin);

                // lo invio
                len = send(sd, &buffer, sizeof(buffer), 0);
                if(len <= 0){
                    perror("Errore invio messaggio");
                    return ret_value;
                }

                slog("%s ha inviato un messaggio", info->username);
                ret_value = 0;
            }

            else if(i==fileno(stdin) && dest == NULL && sd == -1){
                // attesa di input, ma per selezione

                // scanf("%d", &ret_value);
                // FD_CLR(i, info->master);
                // fflush(stdin);

            }
            
            /*
            // è un messaggio ricevuto dall'utente con cui sono in chat
            else if (sd != -1 && i == sd && i != fileno(stdin)){

                len = recv(sd, &buffer, sizeof(buffer), 0);
                if(len < 0){
                    perror("errore ricezione");
                    continue;
                }

                sprintf(buffer, "%d %s", msg_size, msg);
                msg[msg_size] = '\0';

                printf("[%-10s] %s", dest, msg);
                fflush(stdout);
            }
            */

            else{
                slog("%s ha ricevuto un messaggio", info->username);

                // notifica
                len = recv(i, &buffer, sizeof(buffer), 0);
                if(len <= 0){
                    perror("errore ricezione");
                    return ret_value;
                }

                slog("ricevuto: %s", buffer);
                sscanf(buffer, "%d|%[^\t\n]", &msg_size, msg);
                msg[msg_size] = '\0';

                slog("msg_size: %d", msg_size);
                slog(msg);

                printf("\n******************************************\n");
                printf("[%s] %s", "undefined", msg);
                printf("\n******************************************\n");
                fflush(stdout);


                ret_value = 0;
            }
        
        }
    }

   return ret_value;
}



void chatting(struct device_info *info, struct connection *con){

    int quit = 0;

    do {
        slog("hehehehh sono nel chat");
        //scanf("%d", &quit);
        wait_for_msg(info, con->username, con->socket);

    } while(quit == 0);

}


// manda una richiesta di inizio chat a un device
void start_chat(struct device_info *info, char* dest, int dest_port){

    struct sockaddr_in destaddr;            // indirizzo del dispositivo da contattare
    int new_socket;                         // socket di connessione con il nuovo dispositivo
    int ret;                                // variabile di utilità
    struct connection new_con;              // struttura per il salvataggio della connessione tra devices

    // costruisco l'indirizzo del device destinatario
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = DOMAIN;
    destaddr.sin_port   = htons(dest_port);
    inet_pton(DOMAIN, "127.0.0.1", &destaddr.sin_addr);

    // creo il socket necessario per instaurare la comunicazione
    new_socket = socket(DOMAIN, SOCK_STREAM, 0);

    // eseguo una connect sulla porta "port"
    ret = connect(new_socket, (struct sockaddr *) &destaddr, sizeof(destaddr));
    if(ret < 0){
        perror("Errore connessione");
        slog("connessione rifiutata");
        return;
    }

    // aggiungo il nuovo socket a master
    FD_SET(new_socket, (info->master));
    update_max((info->fd_max), new_socket); // aggiorno il massimo indice di socket

    
    slog("Connessione da %s:%d a %s:%d stabilita", info->username, info->port, dest, dest_port);
    // identification() -> gli utenti si identificano secondo il protocollo descritto in identification
    
    slog("Utente %s ha aggiunto il socket %d in attesa di messaggi (maxfd = %d)", info->username, new_socket, *(info->fd_max));

    // creo le nuove informazioni per la connessione
    new_con.port = dest_port;
    strcpy(new_con.username, dest);
    new_con.socket = new_socket;

    //clear_terminal();
    chatting(info, &new_con);
}



