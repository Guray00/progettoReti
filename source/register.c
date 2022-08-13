#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../utils/costanti.h"
#include "./register.h"

int update_register_login(char* username) {

    int found = 0;
    /*int port;
    FILE *registro, *tmp;

    char usr[MAX_USERNAME_SIZE];
    time_t logout, login;
    char replace[500];
 
    
    registro = fopen(FILE_REGISTER, "r");
    tmp      = fopen(TMP_FILE_REGISTER, "w+");

    if (registro == NULL || tmp == NULL) {
        perror("Errore apertura file");
        exit(0);
    }*/
 
  // salvo il tempo di logout

  // scansione del file di registro  
    //sprintf(word, "%s | %d | %lu | %lu\n", username, port, login, (long)0);
    //sprintf(replace, "%s | %d | %lu | %lu\n", username, port, login, logout);

   /* while (fgets(read, 500, registro) != NULL) {
        if (strcmp(read, word) == 0) {
      found = 1;
            strcpy(read, replace);
        }

        fputs(read, tmp);
    }*/

    /*
    while(fscanf(registro, "%s | %d | %lu | %lu", &usr, &port, &login, &logout) != EOF) {

        if(strcmp(usr, username) == 0) {
            found = 1;
            time(&login);
            logout = 0;
        }
        
        sprintf(replace, "%s | %d | %lu | %lu\n", username, port, login, logout);
        fputs(replace, tmp);        
    }
 
  // chiudo i file
    fclose(registro);
    fclose(tmp);

    tmp = fopen(FILE_REGISTER, "w+");
    registro = fopen("tmp.txt", "r");

    if (registro == NULL || tmp == NULL) {
        perror("Errore apertura file");
        exit(0);
    }

    while (fgets(read, 1024, registro) != NULL) {
        fputs(read, tmp);
    }

    fclose(registro);
    fclose(tmp);
    remove(TMP_FILE_REGISTER);
    */
    return found;
}


int get_device_port(char* username){
    FILE *file;
    int port;
    time_t login, logout;
    char usr[32];

    file = fopen(FILE_REGISTER, "r");
    if(!file) {
        perror("Errore apertura file registro");
        return 0;
    }

    while(fscanf(file, "%s | %d | %lu | %lu", usr, &port, &login, &logout) != EOF) {
        //printf("analizzo: %s (%d), paragono con %s (%d)", usr.username, strlen(usr.username), username, strlen(username));
        //fflush(stdout);

        if(strcmp(usr, username) == 0) {
            return 1;
        }
    }

    fclose(file);
    return 0;
}