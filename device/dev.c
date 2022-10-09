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

#include "dev_gui.h"
#include "dev_net.h"
#include "../API/logger.h"
#include "../utils/costanti.h"

#define SIZE 15

int to_child_fd[2];
int to_parent_fd[2];
int pid;
struct connection con;


// recupera la porta inserita in fase di avvio
int portCheck(int argc, char* argv[]){
    if(argc < 2){
        printf("Errore, porta non specificata\n");
        exit(-1);
    }

    return atoi(argv[1]);
}

int main(int argc, char* argv[])
{   
    // recupera la porta sulla quale si comunica
    int port = portCheck(argc, argv);
    con.port = port;
    con.prev = NULL;
    con.next = NULL;

    // inizializzo il logger
    init_logger("./.log");


    // realizzo la pipe in modo
    // che i processi possano
    // comunicare tra di loro
    pipe(to_child_fd);
    pipe(to_parent_fd);

    // divido il processo in 
    // frontend e backend
    pid = fork();

    // frontend (child)
    if (pid == 0) {
        // chiusura fd inutilizzati
        close(to_child_fd[1]); 
        close(to_parent_fd[0]);  
        
        // avvio il frontend del 
        // device per la gestione
        // dell'interfaccia
        startGUI();
    } 
    
    // backend (father)
    else {
        // chiusura fd inutilizzati
        close(to_parent_fd[1]); 
        close(to_child_fd[0]);

        // avvio il backend del device
        // per la gestione delle richieste
        // di rete
        startNET(port);
    }

    return 0;
}