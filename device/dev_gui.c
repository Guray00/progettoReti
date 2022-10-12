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
#include <sys/ioctl.h>

#include "./dev_gui.h"
#include "../utils/costanti.h"
#include "../utils/connection.h"
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
short unsigned int STATUS = OFFLINE;

// menu per un utente non collegato
const char MENU[] = 
    "1) signup\t[usr] [pw]\n"
    "2) in\t\t[usr] [pw]\n\n";

// menu per un utente collegato
const char MENU2[] = 
    "1) hanging\n"
    "2) show\n"
    "3) chat\n"
    "4) share\n"
    "5) out\n\n";

// stampa una riga di asterischi
void print_separation_line(){
    struct winsize w;
    int i;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    for(i = 0; i < w.ws_col; i++)
        printf("*");
    
    printf("\n");
}

// stampa centralmente il testo
void print_centered(char* txt){
    int size, len, i;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    len = strlen(txt);
    size = (w.ws_col - len)/2;
    
    for (i = 0; i < size; i++) printf(" ");
    printf("%s", txt);
    for (i = 0; i < size; i++) printf(" ");
    printf("\n");
}

void print_menu(){
    print_separation_line();
    print_centered("MENU PRINCIPALE");
    print_separation_line();
    printf("\n");
    printf(MENU);
    fflush(stdout);
}

void print_logged_menu(char* username, int port){
    char buffer[100];

    print_separation_line();
    sprintf(buffer, "Bentornato utente %s:%d", username, port);
    print_centered(buffer);
    print_separation_line();
    printf("\n");
    printf(MENU2);
    fflush(stdout);
}

int command_to_code(char command[10]){
    if (strcmp(command, "signup") == 0)
        return SIGNUP_CODE;

    else if (strcmp(command, "in")      == 0)
        return LOGIN_CODE;
    
    else if (strcmp(command, "hanging") == 0)
        return HANGING_CODE;
    
    else if (strcmp(command, "show")    == 0)
        return 5;
    
    else if (strcmp(command, "chat")    == 0)
        return CHAT_CODE;
    
    else if (strcmp(command, "share")   == 0)
        return 6;
    
    else if (strcmp(command, "out")     == 0)
        return LOGOUT_CODE;
    
    return -1;
}

// pulisce lo standard input
void fstdin(){
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
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

// effettua una richiesta di logout
int send_logout_request(){
    
    char buffer[MAX_REQUEST_LEN];
    int ret;

    // genero la richiesta
    sprintf(buffer, "%hd", LOGOUT_CODE);
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
        return 1;
    }

    return 0;
}


// verifica se un utente è online
int checkUserOnline(char usr[MAX_USERNAME_SIZE]){
    char buffer[MAX_REQUEST_LEN];
    int ret;

    // genero la richiesta
    sprintf(buffer, "%hd %s", ISONLINE_CODE, usr);
    ret = send_request_to_net(buffer);
    return ret;
}


// stampa la cronologia di un utente
int print_historic(char src[MAX_USERNAME_SIZE], char dst[MAX_USERNAME_SIZE]){
    FILE *file;
    char c;
    char path[500];
    char command[500];

    // creo i file se non esistono
    sprintf(path, "./devices_data/%s/%s.txt", src, dst);
    sprintf(command, "mkdir -p ./devices_data/%s/ && touch %s", src, path);
    system(command);
    
    file = fopen(path, "r");
    if(!file) {
        perror("Errore caricamento cronologia");
        return -1;
    }

    // stampo tutta la cronologia
    c = fgetc(file);
    while(c != EOF){
        printf("%c", c);
        c = fgetc(file);
    }
    
    // chiudo il file e termino
    fclose(file);
    return 0;
}

void printChatHeader(char *dest){
    char header[100];

    system("clear");
    print_separation_line(); // ***
    sprintf(header, "Chat con %s", dest);
    print_centered(header);
    print_separation_line(); // ***
    printf("\n");
}

void send_msg_to_net(char *dst, char *msg){
    char buffer[MAX_REQUEST_LEN];
    
    sprintf(buffer, "%d %s %s", SENDMSG_CODE, dst, msg);
    slog("[GUI->NET]: %s", buffer);
    send_request_to_net(buffer);
}


// gestisce la richiesta di hanging
void hanging(){
    char buffer[MAX_REQUEST_LEN];
    char path[100];
    FILE* file;

    char username[MAX_USERNAME_SIZE];
    unsigned long timestamp;
    int n;
    
    time_t rawtime;
    struct tm ts;


    // GUI -> NET, mando la richiesta di hanging
    sprintf(buffer, "%d", HANGING_CODE);
    send_request_to_net(buffer);

    system("clear");
    print_separation_line();
    print_centered("HANGING");
    print_separation_line();
    printf("\n");

    // stampo il file
    sprintf(path, "./devices_data/%s/%s", con.username, HANGING_FILE);
    file = fopen(path, "r");

    while(fscanf(file, "%lu %s %d", &timestamp, username, &n) != EOF){
        rawtime = timestamp;
        ts = *localtime(&rawtime);

        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ts);
        printf("[%s] %-*s %d messaggi\n", buffer, MAX_USERNAME_SIZE,username, n);
    }
    fclose(file);

    printf("\n\nPremi [INVIO] per tornare indietro: ");
    
    fstdin();
    getchar();

    system("clear");
    print_logged_menu(con.username, con.port);
    fstdin();
}


// MAIN DELLA GUI
void startGUI(){    
        short int ret;  


        // Stampa il menu delle scelte
        print_menu();

        
        do {
            char command[15], user[SIZE], pw[SIZE];
            char msg[MAX_REQUEST_LEN]; int msg_size;
            int status;
            FILE *historic;
            char path[500];

            // chiede l'inserimento di un comando
            scanf("%s", command);

            slog("comando: %s", command);
            switch(command_to_code(command)){

                case SIGNUP_CODE:
                    scanf("%s %s", user, pw);

                    // per iscriversi bisogna non essere collegati!
                    if(!login_limit()) {
                        printf("Questa funzione è disponibile solo se non collegati.\n\n");
                        break;
                    }

                    ret = send_signup_request(user, pw);

                    if(ret == 1){
                        printf("Utente creato correttamente!\n\n");
                    } else {
                        printf("Esiste già un utente con questo nome, creazione annullata.\n\n");
                    }

                    break;

                case LOGIN_CODE:
                    scanf("%s %s", user, pw);

                    // per fare il login bisogna non essere collegati!
                    if(!login_limit()) {
                        printf("Questa funzione è disponibile solo se non collegati.\n\n");
                        break;
                    }

                    // invio della richiesta di login al server
                    ret = send_login_request(user, pw);
                    
                    // se il login è andato a buon fine, salvo le
                    // informazioni e stampo il nuovo meno per le richieste
                    if (ret == 1) {
                        STATUS = ONLINE;

                        // setup della connessione
                        strcpy(con.username, user);                     // copio il nome

                        printf("Utente connesso correttamente!\n");     // mostro a schermo login riuscito
                        fflush(stdout);
                        
                        sleep(1);                                       // aspetto un secondo
                        system("clear");                                // pulisco la shell
                        print_logged_menu(con.username, con.port);
                    }
                    else 
                        printf("Credenziali errate, login non effettuato.\n\n");
                    

                    break;
                
                case CHAT_CODE:
                    // prendo in input l'utente a cui si vuole scrivere
                    scanf("%s", user);

                    // blocco il comando se non si è online
                    if(login_limit()) {
                        printf("Questa funzione è disponibile solo se collegati.\n\n");
                        break;
                    }   

                    // controllo che l'utente non tenti di parlare con se stesso
                    if (strcmp(user, con.username) == 0){
                        printf("Non puoi parlare con te stesso!\n\n");
                        break;
                    }

                    // TODO: controllare se il nome è in rubrica e corretto
                    status = checkUserOnline(user);        

                    // mostra la parte superiore della chat
                    printChatHeader(user);

                    // mostra a schermo la cronologia e ne consente l'aggiornamento
                    print_historic(con.username, user);
                    sprintf(path, "./devices_data/%s/%s.txt", con.username, user);
                    historic = fopen(path, "a");

                    // prende l'input dell'utente
                    do  {

                        // crea l'inzio dell'output di ogni messaggio
                        char formatted_msg[MAX_MSG_SIZE] = "";
                        char source[MAX_USERNAME_SIZE + 2] = "[";        
                        strcat(source, con.username);
                        strcat(source, "]");

                        // prende in input il messaggio
                        fgets(msg, MAX_MSG_SIZE, stdin);                        
                        msg_size = strcspn(msg, "\n");
                        msg[msg_size] = 0;


                        // se il contenuto non è vuoto mando
                        // il messaggio al mittente
                        if(strcmp(msg, "") != 0 && strcmp(msg, "\\q") != 0){

                            // cancello la riga precendente (messaggio scritto dall'utente)
                            printf("\033[A\r\33[2K");   

                            // costtruisco il messaggio formattato
                            sprintf(formatted_msg, "\033[0;35m%-10s\033[0m %s\033[0;32m [*", source, msg);
                            if (status == 1) 
                                strcat(formatted_msg, "*]\033[0m\n");
                            else 
                                strcat(formatted_msg, " ]\033[0m\n");

                            send_msg_to_net(user, msg);     // invia al network la richiesta di invio messaggio, 
                                                            // e risponde segnalando se è stato recapitato direttamente o al server

                            // mostro il messaggio formattato
                            printf(formatted_msg);                  // stampa a schermo
                            fprintf(historic, formatted_msg);       // salva nella cronologia
                            fflush(stdout);                         // forzo l'output
                        }

                    } while(strcmp(msg, "\\q") != 0);

                    // riporta al menu principale
                    fclose(historic);   // chiudo il file della cronologia
                    system("clear");    // pulisco la schermata
                     print_logged_menu(con.username, con.port);
                    break;

                case HANGING_CODE:
                    hanging();
                    break;

                case LOGOUT_CODE:
                    STATUS = OFFLINE;

                    // TODO: verificare il corretto funzionamento
                    send_logout_request();

                    // stampo il menu iniziale
                    system("clear");
                    print_menu();
                    break;

                // scelta errata
                default:
                    fstdin();
                    printf("Scelta scorretta, reinserire: ");
                    break;
            }

        } while (1);
        
        // programma terminato, chiusura della pipe di comunicazione
        close(to_parent_fd[1]);
}