#ifndef COSTANTI_H
#define COSTANTI_H 

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


// CODES ===========================
#define SIGNUP_CODE 1
#define LOGIN_CODE  2
#define CHAT_CODE   4
// =================================

#define DEBUG               1

#define DOMAIN              AF_INET
#define MAX_REQUEST_LEN     255

#define MAX_USERNAME_SIZE   16
#define MAX_PW_SIZE         16
#define FILE_REGISTER       "registro.txt"
#define FILE_USERS          "utenti.txt"
#define TMP_FILE_REGISTER   "tmp.txt"

#define PORT_NOT_KNOWN       0000

struct user {
    char username[MAX_USERNAME_SIZE];
    char pw[MAX_PW_SIZE];
    int  port; // vecchia porta
    long unsigned int login_timestamp;
    long unsigned int logout_timestamp;
};

struct connection {
    char username[MAX_USERNAME_SIZE];
    int port;
    int socket;
};

struct device_info{
    char* username;
    int port;
    fd_set *master; 
    int *fd_max;
    int listener;  
    struct connection *connection;
};

#endif /* CHAT_H */
