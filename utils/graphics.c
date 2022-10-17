#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include "./graphics.h"

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