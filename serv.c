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
#include <libgen.h>  // utile per dirname e basename

#include "./API/logger.h"
#include "./utils/costanti.h"
#include "./utils/connection.h"
#include "./utils/graphics.h"


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

    uint16_t request;

    fd_set 
        master,         // tutti i socket 
        readers;        // socket disponibili


// utile per mantenere le informazioni
// sulle connessioni in corso
struct connection *con;

// ==============================================

// definizione costanti =========================
const char ADDRESS[] = "127.0.0.1";


const char MENU[] = 
    "1) help " ANSI_COLOR_GREY " ⟶   Mostra questa pagina di informazioni" ANSI_COLOR_RESET "\n"
    "2) list " ANSI_COLOR_GREY " ⟶   Mostra quali utenti sono attualmente online" ANSI_COLOR_RESET "\n"
    "3) esc  " ANSI_COLOR_GREY " ⟶   Termina il server" ANSI_COLOR_RESET "\n\n";

const char expanded_help[] = 
    ANSI_COLOR_BLUE "help: \n" ANSI_COLOR_RESET
    "Mostra a schermo la seguente schermata. "
    "\n\n"
    ANSI_COLOR_BLUE"list:\n" ANSI_COLOR_RESET
    "Mostra un elenco in cui vengono elencati tutti gli utenti attualmente online. "
    "Per ciascuno di essi viene mostrato: nome utente, timestamp di login, porta di accesso."
    "\n\n"
    ANSI_COLOR_BLUE "esc:\n" ANSI_COLOR_RESET
    "Permette la chiusura sicura del server. Una volta fatto ciò i device non saranno "
    "in grado di effettuare l'accesso o interagire con il server.\n\n";


// funzione per l'inizializzazione del server
int init(const char* addr, int port){
    const int enable = 1;

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

    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(int))< 0)
        perror("setsockopt fallito");

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
    return ret;
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
    int16_t code = LOGOUT_CODE;
    struct connection *p;


    // genero la richiesta
    sprintf(buffer, "%d %s", code, "server");

    
    // per ogni device connesso notifico la disconnesione
    for (p = con->next; p!=NULL; ){

        // invio la richiesta di disconnessione
        slog("mando richiesta disconnesione su socket %d", p->socket);
        ret = send_request(p->socket, buffer);

        if(ret > 0){

            slog("==> chiuso socket %d", p->socket);

            // se è andata a buon fine termino l'iesimo socket
            // e rimuovo dalla struttura dati la connessione
            p = close_connection(&p);
        }
    }

    exit(0);
}

void pipeHandler() {
    slog("[PIPE ERROR]");
    exit(0);
}

// calcola l'hash di una stringa ricevuta
// viene utilizzato per la verifica dell'hash
// per la connessione tra dispositivi
unsigned long hash(char *str){
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) )
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

// ritorna l'hash di un utente
unsigned long generate_user_hash(char* username){
    FILE *file;
    char usr[MAX_USERNAME_SIZE], pw[MAX_PW_SIZE], hash_str[MAX_PW_SIZE + MAX_USERNAME_SIZE + 1];
    unsigned long res;

    file = fopen(FILE_USERS, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return -1;
    }

    res = 0;
    while(fscanf(file, "%s | %s", usr, pw) != EOF) {

        // se ho trovato l'utente
        if(strcmp(usr, username) == 0) {
            sprintf(hash_str, "%s %s", usr, pw);
            res = hash(hash_str);
            break;
        }
    }

    fclose(file);
    return res;
}

// **********************************************************************
// * aggiunge un utente all'elenco generale dei registri                *
// * utilizzata durente la procedura di registrazione                   *
// * - username:  nome utente da aggiungere (verifica già compiuta)     *
// * - port:      porta di comunicazione utilizzata dall'utente         *
// **********************************************************************
int add_empty_entry_register(char* username, int port){
    FILE *file;
    //time_t t;

    file = fopen(FILE_REGISTER, "a");
    if(!file) {
        perror("Errore aggiunta entry");
        return 0;
    }

    // essendo la prima connessione, login e logout coincidono (utente non connesso)
    ret = (fprintf(file, "%s | %d | %lu | %lu\n", username, port,  (unsigned long)time(NULL), (unsigned long)time(NULL)) > 0) ? 1 : 0;
    fclose(file);

    return ret;
}

int add_entry_register(char* username, int port, unsigned long lin, unsigned long lon){
    FILE *file;

    file = fopen(FILE_REGISTER, "a");
    if(!file) {
        perror("Errore aggiunta entry");
        return 0;
    }

    // essendo la prima connessione, login e logout coincidono (utente non connesso)
    ret = (fprintf(file, "%s | %d | %lu | %lu\n", username, port,  lin, lon) > 0) ? 1 : 0;
    fclose(file);

    return ret;
}

// macro per l'aggiunta di una login entry nel registro
int add_login_entry(char* username, int port){
    return add_entry_register(username, port, (unsigned long) time(NULL), 0);
}


// aggiunge all'elenco degli utenti un utente
// - usr: struttura contentente nome utente e password
int add_entry_users(struct user usr){
    FILE *file;

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
    unsigned long lon, lin;         // login e logout timestamp
    int port;                       // la porta letta

    file = fopen(FILE_REGISTER, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return 0;
    }

    while(fscanf(file, "%s | %d | %lu | %lu", usr, &port, &lin, &lon) != EOF) {
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
    char usr[MAX_USERNAME_SIZE], pw[MAX_PW_SIZE];

    file = fopen(FILE_USERS, "r");

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    while(fscanf(file, "%s | %s", usr, pw) != EOF) {
        if(strcmp(usr, username) == 0) {
            return 1;
        }
    }

    fclose(file);
    return 0;
}

/* port: utente online
      0: utente offline
     -2: utente non trovato   */
int16_t isOnline(char *username){
    FILE *file;
    char usr[MAX_USERNAME_SIZE];
    int res = -2;
    
    int p;
    unsigned long lout, lin;

    // apro il registro per consultare gli accessi
    file = fopen(FILE_REGISTER, "r");
    if(!file) {
        perror("Errore apertura file registro");
        return -1;
    }

    // analizzo ogni riga del registro per scoprire se l'utente è online
    while(fscanf(file, "%s | %d | %lu | %lu", usr, &p, &lin, &lout) != EOF) {

        // se ho trovato l'utente nel registro
        if(strcmp(usr, username) == 0) {

            // se è != 0 significa che non è online
            if (lout != 0)
                res = 0;

            // in ogni altro caso l'utente è invece online, restituisco la porta
            else{
                res = p;
                break;
            }

        }
    }

    // nel caso in cui fosse assente l'utente
    if(res == -2){
        add_empty_entry_register(username, PORT_NOT_KNOWN);
        ret = 1;
    }

    // chiudo il file e restituisco il risultato
    fclose(file);
    return res;
}



/* verifica se l'utente esiste, restituisce:
    - utente trovato: 1
    - utente non trovato: 0
    - utente già acceduto: -2
*/
int auth(struct user usr) {
    FILE *file;
    char username[MAX_USERNAME_SIZE];
    char password[MAX_PW_SIZE];
    int16_t result;

    file = fopen(FILE_USERS, "r");
    slog("[SERVER] Richiesta accesso utente per: %s | %s", usr.username, usr.pw);

    if(!file) {
        perror("Errore apertura file utenti");
        return 0;
    }

    // di default considero con l'utente come non trovato
    result = 0;
    while(fscanf(file, "%s | %s", username, password) != EOF) {
        if(strcmp(usr.username, username) == 0 && strcmp(usr.pw, password) == 0) {

            // se l'utente è offline
            if (isOnline(username) == 0)
                result = 1;     // 1 se lo trovo e non è già stato fatto l'accesso
            else
                result = -2;    // -2 se lo trovo ma è già stato fatto l'accesso
        }
    }

    fclose(file);
    return result;
}

int signup(struct user usr, int port){
    int ret1, ret2;

    // se non esiste già un utente registrato lo registriamo
    ret = find_entry_users(usr.username);
    if (ret == 0){

        // aggiungo all'elenco degli utenti
        ret1 = add_entry_users(usr);
        ret2 = add_empty_entry_register(usr.username, port);

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

// aggiorna una riga del file di registro, inserendovi il nuovo valore
void updateRegister(char username[MAX_USERNAME_SIZE], int port, unsigned long lin, unsigned long lon){
    FILE *file, *tmp;               // file utilizzati
    char usr[MAX_USERNAME_SIZE];    // nome letto dal registro
    int p;                          // porta letta
    unsigned long li;               // login timestamp letto
    unsigned long lo;               // logout timestamp letto
    char command[50];               // comando costruito per rinominare il file
    uint16_t missing = 1; // controlla se, per incongruenza, l'informazione è assente nel registro

    // apro i file
    file = fopen(FILE_REGISTER, "r");
    tmp  = fopen(TMP_FILE_REGISTER, "w");

    // se uno dei due file non si apre, termino
    if(!file || !tmp) {
        perror("Errore apertura file registro");
        return;
    }

    // slog("la porta vale: %d", port);


    // per ogni riga letta dal registro la copio nel file temporaneo
    while(fscanf(file, "%s | %d | %lu | %lu", usr, &p, &li, &lo) != EOF) {
        
        // se leggo l'utente attuale scrivo nel file una nuova versione con login e logout aggiornato
        if(strcmp(usr, username) == 0 && lo == 0) {
            if(lin == 0) lin = li;  // se viene fornito 0 al login significa che non è significativo
            if(port == 0) port = p; // se viene fornito 0 alla porta significa che non è significativa
            fprintf(tmp, "%s | %d | %lu | %lu\n", username, port, lin, lon);

            missing = 0;
        }

        // tutte le altre righe vengono copiate normalmente
        else {
            fprintf(tmp, "%s | %d | %lu | %lu\n", usr, p, li, lo);
        }
    }

    // se l'informazione viene trovata, per qualche motivo, assente dal registro
    // nota: essendo l'informazione assente, si considera come login time (anche se fornito)
    // pari a quello attuale perchè ritenuto non significativo quello passato dall'utente
    if (missing) fprintf(tmp, "%s | %d | %lu | %lu\n", username, port, (unsigned long) time(NULL), lon);

    // chiudo i file
    fclose(file);
    fclose(tmp);

    // costruisco il comando per il renaming
    sprintf(command, "rm %s && mv %s %s", FILE_REGISTER, TMP_FILE_REGISTER, FILE_REGISTER);
    system(command);
}

void update_logout_entry(char *username,  unsigned long lon){
    updateRegister(username, 0, 0, lon);
}

int login(int sock, struct user usr, int port, unsigned long hardclose){

    int flag = auth(usr);

    if (flag == 1){
        add_login_entry(usr.username, port);

        // imposta l'username alla connessione
        set_connection(&con, usr.username, sock);        
    }
    else if(flag == -2 && hardclose != 0){
        update_logout_entry(usr.username, hardclose);
        add_login_entry(usr.username, port);
        set_connection(&con, usr.username, sock);    
        flag = 1;    
    }

    return flag;
}

// gestisce la richiesta di nuova connessione di un dispositivo
void newConnection(){
    socklen_t clen = sizeof(clientaddr);
    int fd;

    // accetto richiesta e salvo l'informazione nel socket csd
    fd = accept(sd, (struct sockaddr*) &clientaddr, &clen);
    if(fd < 0){
        perror("Errore in accept");
        return;
    }


    slog("Il server ha accettato la connessione");
    FD_SET(fd, &master);
    if(fd > fd_max) fd_max = fd;  // avanzo il socket di id massimo, fd_max contiene il max fd
    
    // aggiungo la connessione alla lista
    new_connection(&con, fd);
}




int update_hanging_file(char* dst, char* src){
    FILE *file, *tmp;                   // file utilizzati
    char command[500];                  // comando costruito per rinominare il file
    char cmd2[500];                     // comando costruito per rinominare il file
    char path[PATH_SIZE], tmp_path[PATH_SIZE]; // per memorizzare i path dei file

    char usr[MAX_USERNAME_SIZE];        // username letto dal file
    unsigned long timestamp;            // timestamp letto dal file
    int  n, nuser;                      // numero di messaggi letti
    int total;                          // numer totale di messaggi letti

    
    sprintf(path,     "./server_data/%s/%s",       dst, HANGING_FILE);
    sprintf(tmp_path, "./server_data/%s/%s",       dst, TMP_FILE);
    sprintf(cmd2,     "touch ./server_data/%s/%s", dst, HANGING_FILE);
    
    // apro i file
    system(cmd2);
    file = fopen(path,     "r");                // apro il file
    tmp  = fopen(tmp_path, "w");

    // se uno dei due file non si apre, termino
    if(!file || !tmp) {
        perror("Errore apertura file per hanging");
        return -1;
    }

    // numero di messaggi precedenti pari a zero
    nuser = 0;
    total = 0;
    
    // per ogni riga letta dal registro la copio nel file temporaneo
    while(fscanf(file, "%lu %s %d", &timestamp, usr, &n) != EOF) {
        
        // se non è la riga che cerco la riscrivo
        if(strcmp(usr, src) != 0) {
            fprintf(tmp, "%lu %s %d\n", timestamp, usr, n);
        } else {
            // salvo il numero di messaggi precedenti
            nuser = n;
        }

        total++;
    }

    // nuovo messaggio, per cui incremento il contatore
    nuser++;

    // scrivo sempre l'ultimo utente  alla fine
    fprintf(tmp, "%lu %s %d\n", (unsigned long) time(NULL), src, nuser);
    
    // chiudo i file
    fclose(file);
    fclose(tmp);

    // costruisco il comando per il renaming
    sprintf(command, "rm %s && mv %s %s", path, tmp_path, path);
    system(command);

    return total;
}

// rimuove da dest l'hanging information di source
int remove_from_hanging_file(char* dst, char* src){
    FILE *file, *tmp;                   // file utilizzati
    char command[500];                  // comando costruito per rinominare il file
    char cmd2[500];                     // comando costruito per rinominare il file
    char path[PATH_SIZE], tmp_path[PATH_SIZE];      // per memorizzare i path dei file

    char usr[MAX_USERNAME_SIZE];        // username letto dal file
    unsigned long timestamp;            // timestamp letto dal file
    int  n;                             // numero di messaggi letti
    int total;                          // numer totale di messaggi letti

    
    sprintf(path,     "./server_data/%s/%s",       dst, HANGING_FILE);
    sprintf(tmp_path, "./server_data/%s/%s",       dst, TMP_FILE);
    sprintf(cmd2,     "touch ./server_data/%s/%s", dst, HANGING_FILE);
    
    // apro i file
    system(cmd2);
    file = fopen(path,     "r");                // apro il file
    tmp  = fopen(tmp_path, "w");

    // se uno dei due file non si apre, termino
    if(!file || !tmp) {
        perror("Errore apertura file per hanging");
        return -1;
    }

    // numero di messaggi precedenti pari a zero
    total = 0;
    
    // per ogni riga letta dal registro la copio nel file temporaneo
    while(fscanf(file, "%lu %s %d", &timestamp, usr, &n) != EOF) {
        
        // se non è la riga che cerco la riscrivo
        if(strcmp(usr, src) != 0) {
            fprintf(tmp, "%lu %s %d\n", timestamp, usr, n);
        }

        total++;
    }
    
    // chiudo i file
    fclose(file);
    fclose(tmp);

    // costruisco il comando per il renaming
    sprintf(command, "rm %s && mv %s %s", path, tmp_path, path);
    system(command);

    return total;
}

// scrive un messaggio pendente per un destinatario
void write_pendant(char* src, char* dst, char* msg){
    FILE *file;
    char path[PATH_SIZE];
    char command[500];

    // creo i file se non esistono
    // msg from A to B pending in: server_data/B/A.txt
    sprintf(path, "./server_data/%s/%s.txt", dst, src);
    sprintf(command, "mkdir -p ./server_data/%s/ && touch %s", dst, path);
    system(command);
    
    file = fopen(path, "a");
    if(!file) {
        perror("Errore caricamento cronologia");
        return;
    }

    fprintf(file, "%s\n", msg);
    
    // chiudo il file e termino
    fclose(file);

    // aggiorno il file per hanging
    update_hanging_file(dst, src);
}


// Consente l'invio di un file verso un device destinatario
void send_file(char* path, int device){
    FILE *file;
    char cmd[100];
    char buffer[MAX_REQUEST_LEN];
    char c;
    int size;

    // recupero il path della cartella
    char dirpath[100];
    strcpy(dirpath, path);    
    dirname(dirpath);

    slog("[SERVER] richiesto file: %s", path);

    // mi assicuro dell'esistenza del file generandolo su bisogno
    sprintf(cmd, "mkdir -p %s && touch %s", dirpath, path);   // mi assicuro che il file esista
    system(cmd);
    
    // apro il file
    file = fopen(path, "r");
    if(!file) {
        perror("Errore apertura file per l'invio");
        return;
    }

    // calcolo dimensione file
    fseek(file, 0L, SEEK_END);
    size = ftell(file);

    // invia il numero di byte al network
    rewind(file);
    sprintf(buffer, "%d", size);
    send_request(device, buffer); 

    
    do{
        c   = fgetc(file);
        ret = send(device, &c, 1, 0);
        if(ret < 0){
            perror("errore invio file");
            exit(-1);
        }    
    } while(c != EOF);
    
        
    // chiudo il file
    fclose(file);
    slog("[SERVER] file inviato");
}


// invia il file di hanging
void send_hanging(int device){ 
    //FILE *file;
    char usr[MAX_USERNAME_SIZE];
    char path[100];
    
    strcpy(usr, get_username_by_connection(&con, device));
    sprintf(path, "./server_data/%s/%s", usr, HANGING_FILE);
    send_file(path, device);
}

// invia il file di show
void send_show(int device, char* mittente){ 
    char usr[MAX_USERNAME_SIZE];
    char path[100];
    char cmd[100];
    char buffer[MAX_REQUEST_LEN];
    struct connection* p;
    FILE *file;
    
    // recupero il nome del file che mi interessa
    strcpy(usr, get_username_by_connection(&con, device));
    sprintf(path, "./server_data/%s/%s.txt", usr, mittente);

    // la notifica di lettura deve essere inviata solo se il file non è vuoto
    if((file = fopen(path, "r"))){
        fclose(file);

        // informo colui che ha inviato i messaggi dell'avvenuta ricezione
        p = find_connection(&con, mittente);
        if(p!=NULL){
            sprintf(buffer, "%hd %s", READ_CODE, usr);
            send_request(p->socket, buffer);
        }
    }

    // mando il file contenente le informazioni 
    // sui messaggi pendenenti per l'utente
    send_file(path, device);

    // ora i messaggi pendenti sono stati recapitati
    // motivo per cui devono essere rimossi
    sprintf(cmd, "rm %s", path);
    system(cmd);
    remove_from_hanging_file(usr, mittente);   
}




/*  Quando un utente si connette invia un hash del proprio 
    nome utente e password per consentire al ricevitore di
    accertarne l'autenticità. Questa funzione riceve un buffer
    contenente il nome utente e l'hash e verifica se corrisponde
    con quello calcolato dal server */
void send_whois(int device, char* buf){
    unsigned long hash_value;           // il valore dell'hash da verificare
    char username[MAX_USERNAME_SIZE];   // l'username passato a cui corrisponde l'hash
    unsigned long hash_correct;         // l'hash calcolato dal server per l'utente
    int16_t response;                 // risposta del server al device

    // recupero dal buffer i dati: [USERNAME] [HASH]
    sscanf(buf, "%s %lu", username, &hash_value);

    // calcolo l'hash per l'utente che ha scritto
    hash_correct = generate_user_hash(username);

    // se l'hash corrisponde, ritorno 1
    if (hash_correct == hash_value){
        response = htons(1);
    }
    
    // se non combacia ritorno -1
    else {
        response = htons(-1);
    }

    // invio la risposta al device
    send(i, (void*) &response, sizeof(uint16_t), 0);
}


void print_menu(){
    print_separation_line();
    print_centered("MENU SERVER");
    print_separation_line();
    printf(MENU);
    fflush(stdout);
}

// ripulisce lo standard input
void fstdin(){
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void print_online_header(){
    print_separation_line();
    print_centered("UTENTI ONLINE");
    print_separation_line();
    printf("\n");
    fflush(stdout);
}

// permette di stampare timestamp e porta di un utente
// questa funzione viene utilizzata per stampare la lista degli utenti online
void  print_timestamp_and_port(char* username){
    FILE *file;
    char usr[MAX_USERNAME_SIZE];
    unsigned long lon, lin;         // login e logout timestamp
    int port;                       // la porta letta
    time_t rawtime;
    struct tm ts;
    char buffer[200];

    file = fopen(FILE_REGISTER, "r");

    if(!file) {
        perror("Errore apertura file registro");
        return;
    }

    while(fscanf(file, "%s | %d | %lu | %lu", usr, &port, &lin, &lon) != EOF) {
        if(strcmp(usr, username) == 0 && lon == 0){
            rawtime = lin;
            ts = *localtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ts);

            printf(" * ");
            printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET, buffer);
            printf(" * ");
            printf(ANSI_COLOR_MAGENTA "%d" ANSI_COLOR_RESET, port);
            printf("\n");
            break;
        }
    }

    fclose(file);
}

// mostra a schermo quali utenti sono attualmente online
int print_online(){
    FILE *file;
    char usr[MAX_USERNAME_SIZE], simb, pw[MAX_PW_SIZE];

    char *line = NULL;
    ssize_t read;
    size_t l;

    system("clear");
    print_online_header();

    file = fopen(FILE_USERS, "r");
    if(!file) {
        perror("Errore apertura file utenti");
        return -1;
    }

    while((read=getline(&line, &l, file)) != -1){
        line[strcspn(line, "\n")] = 0;

        sscanf(line, "%s %c %s", usr, &simb, pw);
        if(isOnline(usr)){
            printf("• %s%-16s%s", ANSI_COLOR_BLUE, usr, ANSI_COLOR_RESET);
            print_timestamp_and_port(usr); // stampa a schermo timestamp e porta dell'utente trovato
        }
    }

    printf("\n\nPremi [INVIO] per tornare indietro: ");
    fflush(stdout);
    getchar();  // aspetto la pressione di "invio"

    fclose(file);
    return 0;
}

void print_help_header(){
    print_separation_line();
    print_centered("HELP");
    print_separation_line();
    printf("\n");
    fflush(stdout);
}

void help(){
    system("clear");
    
    print_help_header();
    printf(expanded_help);

    printf("\n\nPremi [INVIO] per tornare indietro: ");
    fflush(stdout);
    getchar();  // aspetto la pressione di "invio"
}

void gui_int_handler(){
    kill(getppid(), SIGINT);
    exit(0);
}

void gui_server_handler(){
    char command[MAX_REQUEST_LEN];

    fscanf(stdin, "%s", command);
    fstdin();

    if(strcmp(command, "esc") == 0){
        gui_int_handler();
    }

    else if (strcmp(command, "help") == 0){
        help();
        system("clear");
        print_menu();
    }

    else if(strcmp(command, "list") == 0){
        print_online();
        system("clear");
        print_menu();
    }

    else {
        printf(ANSI_COLOR_RED "Comando errato, riprovare." ANSI_COLOR_RESET);
        printf("\n\n");
        fflush(stdout);
    }
}



int main(int argc, char* argv[]){

    // variabili di utility
    char buffer[MAX_REQUEST_LEN];
    struct user usr;
    
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

    // inizializzo la lista di connessioni
    con = (void*) malloc(sizeof(struct connection));
    con->socket = sd;
    con->prev = NULL;
    con->next = NULL;
    strcpy(con->username, "server");
    con->port = port;

    pid = fork();

    if(pid == 0){
        signal(SIGINT, gui_int_handler);
        print_menu();

        while(1){
            gui_server_handler();
        }
    }

    else {
        //  assegna la gestione del segnale di int
        signal(SIGINT, intHandler);
        signal(SIGSEGV, intHandler);
        signal(SIGPIPE, pipeHandler);
        
        FD_ZERO(&master);                   // pulisco il master
        FD_ZERO(&readers);                  // pulisco i readers
        FD_SET(sd, &master);                // aggiungo il socket di ricezione tra quelli monitorati
        if(sd > fd_max) fd_max = sd;

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
                // verifico se la richiesta è sul socket di accettazione,
                // dunque se è pendente una nuova richiesta di connessione
                if(i == sd){
                    newConnection();
                }
            
               
                // in tutti gli altri casi sono i devices che effettuano le richieste
                else {
                    int16_t code;                             // codice della richiesta
                    int bytes_to_receive, received_bytes;       // byte da ricevere e ricevuti
                    int16_t response;                         // codice di risposta
                    char *buf = buffer;                         // buffer per le richieste
                    char msg[MAX_MSG_SIZE];                     // messaggio
                    char dst[MAX_USERNAME_SIZE], src[MAX_USERNAME_SIZE]; // utente e source
                    unsigned long hardclose;                    // campo per lo spegnimento a server spento

                    slog("[SERVER] servendo socket: %d (%s)", i, get_username_by_connection(&con, i));

                    bytes_to_receive = sizeof(buffer);
                    while(bytes_to_receive > 0) {
                        received_bytes = recv(i, buf, bytes_to_receive, 0);
                        if(received_bytes == -1){
                            perror("errore ricezione richiesta per server");
                            exit(-1);
                        }

                        // se non ricevo dati l'utente con cui parlavo si è disconnesso. Devo dunque segnarlo 
                        // come offline e chiudere la connessione.
                        else if(received_bytes == 0){
                            // aggiorno il campo del registro
                            update_logout_entry(get_username_by_connection(&con, i), (unsigned long)  time(NULL));

                            // disconnetto
                            close_connection_by_socket(&con, i);
                            FD_CLR(i, &master);
                        }
                            
                        bytes_to_receive -= received_bytes;
                        buf += received_bytes;
                    }


                    sscanf(buffer, "%hd %[^\n\t]", &code, buffer);
                    slog("server ricevuto richiesta [%hd]", code);

                    switch(code){

                        // DISCONNECT
                        case LOGOUT_CODE:
                            slog("[SERVER] Sto scollegando %s(%d)", get_username_by_connection(&con, i), i);
                            update_logout_entry(get_username_by_connection(&con, i), (unsigned long) time(NULL));

                            // rimuovo dalla lista la connessione i-esima
                            close_connection_by_socket(&con, i);

                            // rimuovo la connessione dai socket ascoltati
                            FD_CLR(i, &master);

                            break;

                        // SIGNUP
                        case SIGNUP_CODE:     
                            sscanf(buffer, "%s %s %d", usr.username, usr.pw, &port);


                            ret = signup(usr, port);
                            response = htons(ret);
                            send(i, (void*) &response, sizeof(uint16_t), 0);

                            // dopo una signup, avviene sempre la rimossione della connessione
                            close_connection_by_socket(&con, i);
                            FD_CLR(i, &master);
                            break;
                        
                        // LOGIN
                        case LOGIN_CODE:     
                            // scompongo il buffer in [username | password | port]
                            sscanf(buffer, "%s %s %d %lu", usr.username, usr.pw, &port, &hardclose);
                            ret = login(i, usr, port, hardclose);

                            response = htons(ret);
                            send(i, (void*) &response, sizeof(uint16_t), 0);

                            // Se l'utente ha sbagliato le credenziali (0) oppure era già online (-2)
                            // è necessario eliminare la connessione in essendo questa stata stabilita
                            if (ret == -2 || ret == 0){
                                close_connection_by_socket(&con, i);
                                FD_CLR(i, &master);
                            }

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

                            // verifica se l'utente è online
                            ret = isOnline(buffer);

                            // se il dispositivo è raggiungibile
                            if (ret > 0){
                                // restituisco i dati a chi ha chiesto
                                sprintf(buffer, "%d %s", ret, ADDRESS);
                            }
                            // se non è raggiungibile
                            else {
                                sprintf(buffer, "%d", ret);
                            }

                            ret = send_request(i, buffer);
                            
                            break;
                        
                        case PENDANTMSG_CODE:
                            // ricavo il destinatario e il messaggio inviato
                            sscanf(buffer, "%s %[^\n\t]", dst, msg);

                            // recupero il mittente del messaggio analizzando le connessioni
                            strcpy(src, get_username_by_connection(&con, i));

                            slog("pendant da %s a %s", src, dst);
                            write_pendant(src, dst, msg);
                            break;

                        case HANGING_CODE:
                            send_hanging(i);
                            break;

                        case SHOW_CODE:
                            sscanf(buffer, "%s", dst);
                            send_show(i, dst);
                            break;

                        case WHOIS_CODE:
                            send_whois(i, buffer);
                            break;
                    }   
                }
            }
        }
    }
    } // ELSE

    return 0;
}