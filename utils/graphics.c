#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include "./graphics.h"
#include "./costanti.h"

// stampa una riga divisoria
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

// stampa centralmente il testo ma con i bordi tratteggiati
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

// formatta il messaggio per la visualizzazione
void format_msg(char *formatted_msg, char *source, char* msg){
    char src [MAX_USERNAME_SIZE + 2] = "[";

    strcat(src, source);
    strcat(src, "]");

    sprintf(formatted_msg, "\033[0;35m%-10s\033[0m %s", src, msg);
}

// mostra l'header della chat di gruppo
void print_group_chat_header(){
    system("clear");
    print_separation_line(); // ***
    print_centered("CHAT DI GRUPPO");
    print_separation_line(); // ***
    printf("\n");
    fflush(stdout);
}
