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

#include "./dev_gui.h"
#include "../utils/costanti.h"
#include "../API/logger.h"

#define SIZE 10
#define ONLINE  1
#define OFFLINE 0

// EXTERN =================
extern int to_child_fd[2];
extern int to_parent_fd[2];
extern int pid;
extern struct connection con;
// ========================

// memorizza lo status di loggato o meno
short unsigned int STATUS = 0;

// menu per un utente non collegato
const char MENU[] = 
    "***************************\n"
    "           MENU            \n"
    "***************************\n"
    "1) signup\t[usr] [pw]\n"
    "2) in\t\t[usr] [pw]\n\n";

// menu per un utente collegato
const char MENU2[] = 
    "***************************\n"
    "*   Utente: %s:%d         \n"
    "***************************\n"
    "1) hanging\n"
    "2) show\n"
    "3) chat\n"
    "4) share\n"
    "5) out\n\n";


int command_to_code(char command[10]){
    if (strcmp(command, "signup") == 0)
        return SIGNUP_CODE;

    else if (strcmp(command, "in")      == 0)
        return LOGIN_CODE;
    
    else if (strcmp(command, "hanging") == 0)
        return 3;
    
    else if (strcmp(command, "show")    == 0)
        return 5;
    
    else if (strcmp(command, "chat")    == 0)
        return CHAT_CODE;
    
    else if (strcmp(command, "share")   == 0)
        return 6;
    
    else if (strcmp(command, "out")     == 0)
        return 7;
    
    return -1;
}


// MACRO  per inviare al network una richiesta
int send_request_to_net(char* buffer){
    char ret_code[4], result;

    // GUI -> NET, invio la richiesta
    write(to_parent_fd[1], buffer, strlen(buffer) + 1);

    // NET -> GUI, aspetto la risposta
    read(to_child_fd[0], ret_code, 4);
    result = atoi(ret_code);               // converto la risposta

    return result;
}

// effettua una richiesta di login
int send_login_request(char user[MAX_USERNAME_SIZE], char pw[MAX_PW_SIZE]){
    
    char buffer[MAX_REQUEST_LEN];
    int ret;

    // genero la richiesta
    sprintf(buffer, "%hd %s|%s", LOGIN_CODE, user, pw);
    ret = send_request_to_net(buffer);
    
    return ret;
}

// effettua una richiesta di login
int send_signup_request(char user[MAX_USERNAME_SIZE], char pw[MAX_PW_SIZE]){
    
    // instanzio le variabili
    char buffer[MAX_REQUEST_LEN];
    int ret;

    // genero la richiesta
    sprintf(buffer, "%hd %s|%s", SIGNUP_CODE, user, pw);
    ret = send_request_to_net(buffer);    
    return ret;
}

int login_limit(){
    if (STATUS != ONLINE) {
        printf("Questa funzione è disponibile solo se colllegati.\n\n");
        return 1;
    }

    return 0;
}

// main per la gui
void startGUI(){    
        short int ret;  

        // Stampa il menu delle scelte
        if (!STATUS)
            printf(MENU);

        
        do {
            char command[15], user[SIZE], pw[SIZE];
            char msg[MAX_REQUEST_LEN]; int msg_size;

            scanf("%s", command);

            switch(command_to_code(command)){

                case SIGNUP_CODE:
                    scanf("%s %s", user, pw);
                    ret = send_signup_request(user, pw);

                    if(ret == 1){
                        printf("Utente creato correttamente!\n\n");
                    } else {
                        printf("Esiste già un utente con questo nome, creazione annullata.\n\n");
                    }

                    break;

                case LOGIN_CODE:
                    scanf("%s %s", user, pw);
                    ret = send_login_request(user, pw);
                    
                    // se il login è andato a buon fine, salvo le
                    // informazioni e stampo il nuovo meno per le richieste
                    if (ret == 1) {
                        STATUS = ONLINE;
                        strcpy(con.username, user);                     // copio il nome
                        printf("Utente connesso correttamente!\n");     // mostro a schermo login riuscito
                        fflush(stdout);
                        sleep(1);                                       // aspetto un secondo
                        system("clear");                                // pulisco la shell
                        printf(MENU2, con.username, con.port);          // mostro il nuovo menu
                    }

                    else {
                        printf("Credenziali errate, login non effettuato.\n\n");
                    }

                    break;
                
                case CHAT_CODE:
                    // prendo in input l'utente a cui si vuole scrivere
                    scanf("%s", user);

                    // blocco il comando se non si è online
                    if(login_limit()) break;

                    // controllo che l'utente non tenti di parlare con se stesso
                    if (strcmp(user, con.username) == 0){
                        printf("Non puoi parlare con te stesso!\n\n");
                        break;
                    }

                    // TODO: controllo sulla validità del nome

                    // mostro a schermo con chi stiamo parlando
                    system("clear");
                    printf("**************************\n");
                    printf("chat con: %s\n", user);
                    printf("**************************\n\n");

                    /* TO-DO 
                        - download msgs
                        - show msgs
                        - scanf
                        - dopo che ho premuto invio, il messaggio passa al
                          backend che si occupa di:
                            - verificare se l'utente è online:
                                - se è online, lo recapita personalmente
                                - se offline, lo inoltra al server
                    */
                    
                    //check_usr_exists()
                    //download_msgs();

                    // prende l'input dell'utente
                    do  {

                        fgets(msg, 512, stdin);                        
                        msg_size = strcspn(msg, "\n");
                        msg[msg_size] = 0;

                        // se il contenuto non è vuoto mando
                        // il messaggio al mittente
                        if(strcmp(msg, "") != 0 && strcmp(msg, "\\q") != 0){
                            slog("[TO SEND] %s", msg);                        
                        }

                    } while(strcmp(msg, "\\q") != 0);

                    system("clear");
                    printf(MENU2, con.username, con.port);

                    break;

                default:
                    printf("Scelta scorretta, reinserire: ");
                    break;
            }

        } while (1); //  ciclo fino a quando non è corretta
        
        close(to_parent_fd[1]);
}

    /*
        char phrase[SIZE];

        while(1){
            printf("\n\nplease type a sentence: ");
            scanf("%s", phrase);
            write(to_parent_fd[1], phrase, strlen(phrase) + 1);

            // aspetto la risposta
            read(to_child_fd[0], phrase, SIZE);
            printf("GUI[%d] riceve: %s", getpid(), phrase);
            fflush(stdout);
        }*/