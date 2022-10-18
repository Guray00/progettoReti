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

// tiene traccia di tutte le connessioni attive
extern struct connection* con;
// ========================

// memorizza lo status di loggato o meno
short unsigned int STATUS = OFFLINE;
//char SCENE [MAX_USERNAME_SIZE];
int ret;

// menu per un utente non collegato
const char MENU[] = 
    "1) signup  [usr] [pw]" ANSI_COLOR_GREY " ⟶   Crea un nuovo account" ANSI_COLOR_RESET "\n"
    "2) in      [usr] [pw]" ANSI_COLOR_GREY " ⟶   Accedi al tuo account" ANSI_COLOR_RESET "\n\n"
    ANSI_COLOR_MAGENTA "[COMANDO]: " ANSI_COLOR_RESET;

// menu per un utente collegato
const char MENU2[] = 
    "1) hanging            " ANSI_COLOR_GREY " ⟶   Resoconto messaggi ricevuti mentre eri offline" ANSI_COLOR_RESET "\n"
    "2) show     [username]" ANSI_COLOR_GREY " ⟶   Mostra i messaggi riceuti mentre eri offline" ANSI_COLOR_RESET "\n"
    "3) chat     [username]" ANSI_COLOR_GREY " ⟶   Parla con qualcuno!" ANSI_COLOR_RESET "\n"
    "4) share              " ANSI_COLOR_GREY " ⟶   Condividi un documente con un utente" ANSI_COLOR_RESET "\n"
    "5) out                " ANSI_COLOR_GREY " ⟶   Esci dall'account" ANSI_COLOR_RESET "\n\n"
    ANSI_COLOR_MAGENTA "[COMANDO]: " ANSI_COLOR_RESET;

// stampa una riga di asterischi
void print_separation_line(){
    struct winsize w;
    int i;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    for(i = 0; i < w.ws_col; i++)
        printf("─");
    
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

void print_centered_dotted(char* txt){
    int size, len, i;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    len = strlen(txt);
    size = (w.ws_col - len)/2;
    
    for (i = 0; i < size/2; i++) {printf(" ─");}
    printf("%s", txt);
    for (i = 0; i < size/2; i++) {printf("─ ");}
    
    if (w.ws_col != (size*2+len)) printf("─");
    printf("\n");
}

// mostra l'HEADER per una nuova schermata
void print_header(char* header){
    system("clear");
    print_separation_line();
    print_centered(header);
    print_separation_line();
    printf("\n");
}

void print_menu(){
    print_header("MENU PRINCIPALE");
    printf(MENU);
    fflush(stdout);
}

void print_logged_menu(char* username, int port){
    char buffer[100];

    sprintf(buffer, "Bentornato utente %s:%d", username, port);
    print_header(buffer);
    printf(MENU2);
    fflush(stdout);
}

// pulisce lo standard input
void fstdin(){
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// converte un comando ricevuto in un codice
int command_to_code(char *buffer){
    char command[20];
    // mi assicuro di prendere solo la prima parola, ovvero il comando
    sscanf(buffer, "%s %[^\n]", command, buffer);
    slog("COMANDO ORA VALE: %s", command);

    if (strcmp(command, "signup") == 0)
        return SIGNUP_CODE;

    else if (strcmp(command, "in")      == 0)
        return LOGIN_CODE;
    
    else if (strcmp(command, "hanging") == 0)
        return HANGING_CODE;
    
    else if (strcmp(command, "show")    == 0)
        return SHOW_CODE;
    
    else if (strcmp(command, "chat")    == 0)
        return CHAT_CODE;
    
    else if (strcmp(command, "share")   == 0)
        return SHARE_CODE;
    
    else if (strcmp(command, "out")     == 0)
        return LOGOUT_CODE;
    
    else {
        // pulisco l'output
        return -1;
    }
}


// MACRO  per inviare al network una richiesta
int send_request_to_net(char* buffer){
    //char ret_code[4];//, result;
    int result = 0;

    // GUI -> NET, invio la richiesta
    slog("SONO SEND->net invio %s", buffer);
    write(to_parent_fd[1], buffer, MAX_REQUEST_LEN);

    // NET -> GUI, aspetto la risposta
    read(to_child_fd[0], &result, sizeof(result));
    slog("SONO SEND->net HO RICEVUTO: %hd", result);
    //slog("mannaggia sono send_request e ricevo: %d", result);
    //result = atoi(ret_code);               // converto la risposta

    return result;
}

void send_request_to_net_without_response(char* buffer){

    // GUI -> NET, invio la richiesta senza attendere una risposta
    slog("SONO SEND->net SENZA RISPOSTA invio %s", buffer);
    write(to_parent_fd[1], buffer, MAX_REQUEST_LEN);

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


// verifica se un utente è già parte dei partecipanti
short int checkUserOnline(char usr[MAX_USERNAME_SIZE]){
    char buffer[MAX_REQUEST_LEN];
    int ret;

    // genero la richiesta
    sprintf(buffer, "%hd %s", ISONLINE_CODE, usr);
    ret = send_request_to_net(buffer);

    return ret;
}


// stampa la cronologia della chat di un utente
int print_historic(char src[MAX_USERNAME_SIZE], char dst[MAX_USERNAME_SIZE]){
    FILE *file;             // apre il file da cui leggere la cronologia
    char c;                 // utilizzato per leggere un byte dalla chat
    char path[500];         // utilizzato per salvare il path del file
    char command[500];      // utilizzato per inviare al sistema il comando

    // creo i file se non esistono
    sprintf(path, "./devices_data/%s/%s.txt", src, dst);
    sprintf(command, "mkdir -p ./devices_data/%s/ && touch %s", src, path);
    system(command);
    
    // apro il file in lettura
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
    fflush(stdout);
    return 0;
}


void printChatHeader(char *dest){
    char header[100];

    system("clear");
    print_separation_line(); // ***
    sprintf(header, "%s ⟶  %s", con->username, dest);
    print_centered(header);
    print_separation_line(); // ***
    printf("\n");
}

void print_group_chat_header(){
    system("clear");
    print_separation_line(); // ***
    print_centered("CHAT DI GRUPPO");
    print_separation_line(); // ***
    printf("\n");
    fflush(stdout);
}

int send_msg_to_net(char *dst, char *msg){
    char buffer[MAX_REQUEST_LEN];
    
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d %s %s", SENDMSG_CODE, dst, msg);
    slog("[GUI->NET] sei passato da send_msg_to_net: %s", buffer);
    ret = send_request_to_net(buffer);
    slog("[GUI->NET] sei tornato da send_msg_to_net: %d", ret);
    return ret;
}


// gestisce la richiesta di hanging
int hanging(){
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
    ret = send_request_to_net(buffer);

    if(ret < 0) return ret;

    // mostra a schermo l'header del hanging
    print_header("HANGING");

    // recupero il path del file
    sprintf(path, "./devices_data/%s/%s", con->username, HANGING_FILE);
    file = fopen(path, "r");

    // mostro a schermo il parsing del contenuto del file
    while(fscanf(file, "%lu %s %d", &timestamp, username, &n) != EOF){

        // ricavo la data e l'ora dal timestamp
        rawtime = timestamp;
        ts = *localtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ts);

        // mostro a schermo [TIMESTAMP] [UTENTE] [MESSAGGI]
        printf("[%s] %-*s %d messaggi", buffer, MAX_USERNAME_SIZE,username, n);

        // stampo la "o" se è arrivato solo un messaggio
        if(n == 1) 
            printf("o\n");
        else 
            printf("\n");
    }

    // chiudo il file
    fclose(file);

    printf("\n\nPremi [INVIO] per tornare indietro: ");
    //fstdin();   // pulisco da eventuali residui
    getchar();  // aspetto la pressione di "invio"

    // mostro nuovamente a schermo il vecchio menu
    system("clear");
    print_logged_menu(con->username, con->port);
    //fstdin();   // pulisco l'input

    return 0;
}


// consente di formattare un messaggio secondo un determinato
// standard, in modo da renderlo più gradevole alla vista
void format_msg(char *formatted_msg, char *source, char* msg){
    char src [MAX_USERNAME_SIZE + 2] = "[";

    strcat(src, source);
    strcat(src, "]");

    sprintf(formatted_msg, "\033[0;35m%-10s\033[0m %s", src, msg);
}


// invia una richiesta "show" al network
int show_request_to_net(char* mittente){
    char buffer[MAX_USERNAME_SIZE];
    sprintf(buffer, "%d %s", SHOW_CODE, mittente);

    return send_request_to_net(buffer);
}

int chat_request_to_net(char* mittente){
    char buffer[MAX_USERNAME_SIZE];
    sprintf(buffer, "%d %s", CHAT_CODE, mittente);

    return send_request_to_net(buffer);
}


// Esecuzione del comando di show della gui
int show(char *mittente){
    FILE *file;
    char header[100];
    char path[100];
    char *line = NULL;
    ssize_t read;
    size_t l;

    char msg[MAX_REQUEST_LEN] = "";

    // [GUI->NET] genero la richiesta
    ret = show_request_to_net(mittente);
    if (ret < 0) return ret;

    // [GUI]
    sprintf(header, "NUOVI MESSAGGI da %s", mittente);
    print_header(header);

    // creo il path del file
    sprintf(path, "./devices_data/%s/%s",     con->username, SHOW_FILE);
    file = fopen(path, "r");
    if(file < 0){
        perror("errore apertura file");
        return  -1;
    }

    // mostro a schermo il parsing del contenuto del file
    while((read=getline(&line, &l, file)) != -1){

        // mostro a schermo [TIMESTAMP] [UTENTE] [MESSAGGI]
        format_msg(msg, mittente, line);
        printf(msg);
    }

    // chiudo il file
    fclose(file);

    // aspetto l'input dell'utente per andare avanti
    printf("\n\nPremi [INVIO] per tornare indietro: ");
    //fstdin();   // pulisco da eventuali residui
    getchar();  // aspetto la pressione di "invio"

    // mostro nuovamente a schermo il vecchio menu
    system("clear");
    print_logged_menu(con->username, con->port);
    return 0;
}

// stampa il visualizzato in base allo stato che gli viene passato
void print_view_mark(int status){
        // messaggio recapitato
        if (status > 0) 
            printf(ANSI_COLOR_GREEN " [**]" ANSI_COLOR_RESET);

        // messaggio non inviato perchè server offline
        else if (status == -2)
            printf(ANSI_COLOR_RED " [X]" ANSI_COLOR_RESET);

        // messaggio non recapitato perchè utente offline
        else
            printf(ANSI_COLOR_GREEN " [*]" ANSI_COLOR_RESET);

        printf("\n");
}

// invia al network una richiesta di verifica di quali utenti
// sono attualmente raggiungibili tra quelli presenti in rubrica
int available_request_to_net(){
    char buffer[MAX_REQUEST_LEN];

    sprintf(buffer, "%d", AVAILABLE_CODE);
    ret = send_request_to_net(buffer);
    return ret;
}


void start_chat(char *dst){
    char msg[MAX_MSG_SIZE]; 
    int msg_size;
    short int status;
    char buffer[MAX_REQUEST_LEN];
    char user[MAX_USERNAME_SIZE];

    
    // funzione di utility per liberare lo standard input
    //fstdin();

    // prende l'input dell'utente
    do  {
        char formatted_msg[MAX_MSG_SIZE];
        memset(msg, 0, MAX_MSG_SIZE);

        // prende in input il messaggio
        fgets(msg, MAX_MSG_SIZE, stdin);                        
        msg_size = strcspn(msg, "\n");
        msg[msg_size] = 0;

        // cancello la riga precendente (messaggio scritto dall'utente)
        printf("\033[A\r\33[2K");  
        fflush(stdout);     // necessario per cancellare subito la riga  del comando digitato

        // se il contenuto non è vuoto mando
        // il messaggio al mittente
        if(strcmp(msg, "") != 0 && strcmp(msg, "\\q") != 0 && strcmp(msg, "\\u") != 0 && !strstr(msg, "\\a")){

            // costtruisco il messaggio formattato con gli abbellimenti grafici
            // e lo mostro a schermo
            format_msg(formatted_msg, con->username, msg);
            printf(formatted_msg);             

            // invia al network la richiesta di invio messaggio,
            // e risponde segnalando se è stato recapitato direttamente o al server
            ret = send_msg_to_net(dst, msg);  
                                                                   
            // se il messaggio è stato inviato correttamente
            // mostro a schermo le spunte di successo
            if(ret > 0){
                status = checkUserOnline(dst);        
                print_view_mark(status);
            }

            // altrimenti segno la spunta rossa per informare che il messaggio
            // non è stato mandato ne al server ne al device, verrà dunque perso
            else {
                print_view_mark(-2);
            }
                            

            // forzo l'output del messaggio formattato
            fflush(stdout);
        }

        // se viene chiesto di mostrare gli utenti in rubrica online
        else if(strcmp(msg, "\\u") == 0){
            ret = available_request_to_net();
            if (ret == -1){
                printf(ANSI_COLOR_RED "Comando non disponibile: impossibile contattare il server al momento\n\n" ANSI_COLOR_RESET);
            }
        }

            // se viene richiesto di aggiungere un utente
        else if(strstr(msg, "\\a")){
            // recupero il nome dell'utente da aggiungere
            sscanf(msg, "%s %s", buffer, user);

            // costruisco la richiesta per net di aggiunta utente
            sprintf(buffer, "%d %s", ADDUSER_CODE, user);

            // invio la richiesta al network
            send_request_to_net_without_response(buffer);
        }

    } while(strcmp(msg, "\\q") != 0);
    slog("RICHIESTA DI USCIRE PER: %s", con->username);
    
    // riporta al menu principale
    memset(buffer, 0, MAX_REQUEST_LEN);
    sprintf(buffer, "%d", QUITCHAT_CODE);

    send_request_to_net(buffer);
    system("clear");    // pulisco la schermata
    print_logged_menu(con->username, con->port);
}


void handle_message(){
    char buffer[MAX_REQUEST_LEN];
    short int code;

    // leggo la richiesta
    read(to_child_fd[0], buffer, MAX_REQUEST_LEN);
    sscanf(buffer, "%hd %[^\n]", &code, buffer);

    switch (code){
    case STARTCHAT_CODE:

        // quando parte la chat potrebbe essere interrotta una vecchia richiesta
        // di inserimento di uno comando. Per questo motivo, nel caso in cui lo stdin
        // fosse vuoto vi si scrive un carattere e si cancella la risposta a schermo.
        slog("[NET:%d]passato da qua", con->port);
        start_chat("inutile");
        break;
    
    default:
        break;
    }

}


// MAIN DELLA GUI
void startGUI(){    
        short int ret;  

        char command[15], user[MAX_USERNAME_SIZE], pw[MAX_PW_SIZE];
      
        //char buffer[MAX_REQUEST_LEN];
        int i;

        // Stampa il menu delle scelte
        print_menu();

        int fdmax = -1;
        fd_set readers, master;

        FD_ZERO(&master);                   // pulisco il master
        FD_ZERO(&readers);                  // pulisco i readers

        FD_SET(fileno(stdin), &master);
        fdmax = (fileno(stdin) > fdmax) ? fileno(stdin) : fdmax;

        FD_SET(to_child_fd[0], &master);           // aggiungo le risposte dal server in ascolto
        fdmax = (to_child_fd[0] > fdmax) ? to_child_fd[0] : fdmax;
        
        do {
            //  seleziono i socket che sono attivi   
            FD_ZERO(&readers);
            readers = master;  
            ret = select((fdmax)+1, &readers, NULL, NULL, NULL);
            if(ret<=0){
                perror("Errore nella select");
                exit(1);
            }

            // gestisco l'input e le richieste del net
            for(i = 0; i <= fdmax; i++){

            // se nessuno è settato
            if(!FD_ISSET(i, &readers))
                continue;

            // se la richiesta arriva dal net
            if (i == to_child_fd[0]){
                handle_message();
            }

            else if (i == fileno(stdin)){
            

            // chiede l'inserimento di un comando
            
            fscanf(stdin, "%[^\n]", command);
            slog("STO CICLANDO(%d): %s", con->port, command);
            fstdin();
            switch(command_to_code(command)){

                case SIGNUP_CODE:
                    sscanf(command, "%s %s", user, pw);

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
                    sscanf(command, "%s %s", user, pw);

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
                        strcpy(con->username, user);                     // copio il nome

                        printf("Utente connesso correttamente!\n");     // mostro a schermo login riuscito
                        fflush(stdout);
                        
                        sleep(1);                                       // aspetto un secondo
                        system("clear");                                // pulisco la shell
                        print_logged_menu(con->username, con->port);
                    }

                    else if(ret == 0)
                        printf("Credenziali errate, riprovare\n\n");
                    
                    else if (ret == -2)
                        printf("Utente già connesso, non è possibile accedere contemporaneamente da più dispositivi.\n\n");
                    
                    break;
                
                case CHAT_CODE:
                    // prendo in input l'utente a cui si vuole scrivere
                    sscanf(command, "%s", user);

                    // blocco il comando se non si è online
                    if(login_limit()) {
                        printf("Questa funzione è disponibile solo se collegati.\n\n");
                        break;
                    }   

                    // controllo che l'utente non tenti di parlare con se stesso
                    if (strcmp(user, con->username) == 0){
                        printf("Non puoi parlare con te stesso!\n\n");
                        break;
                    }

                    /*  [GUI->NET] invio al network la richiesta di realizzare una nuova chat
                        In questa parte del programma si verifica tramite network se è possibile
                        instaurare una connessione con il destinatario, se non è già stata creata.
                        In base a tale risposta, tutti i messaggi inviati successivamente considereranno
                        l'utente connesso o meno in base a ciò.*/
                    chat_request_to_net(user);


                    // TODO: controllare se il nome è in rubrica e corretto


                    // mostra la parte superiore della chat
                    printChatHeader(user);

                    // mostra a schermo la cronologia e ne consente l'aggiornamento
                    print_historic(con->username, user);

                    // porta l'utente alla schermata di inserimento messaggi
                    start_chat(user);
                    slog("SONO USCITO DALLA CHAT AVVIATA: %s", con->username);
                    break;

                case HANGING_CODE:
                    if(login_limit()) {
                        printf("Questa funzione è disponibile solo se collegati.\n\n");
                        break;
                    }

                    ret = hanging();
                    if (ret < 0) break;
                    break;

                case SHOW_CODE:
                    // prendo in input il nome dell'utente
                    sscanf(command, "%s", user);

                    if(login_limit()) {
                        printf("Questa funzione è disponibile solo se collegati.\n\n");
                        break;
                    }   

                    // controllo che l'utente non tenti di parlare con se stesso
                    if (strcmp(user, con->username) == 0){
                        printf("Non puoi vedere i nuovi messaggi con te stesso!\n\n");
                        break;
                    }

                    ret = show(user);
                    break;

                // l'utente ha fatto "out" da shell
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
                    printf("Scelta scorretta, reinserire: ");
                    break;
            }

        if (ret == -1) printf("Non è stato possibile inviare la richiesta.\n");
        } // chiude if
        } // chiude for

        } while (1);

        
        // programma terminato, chiusura della pipe di comunicazione
        close(to_parent_fd[1]);
}