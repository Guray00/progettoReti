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
#define SIGNUP_CODE         1
#define LOGIN_CODE          2
#define CHAT_CODE           4
#define HANGING_CODE        5
#define SHOW_CODE           6
#define SHARE_CODE          7

#define LOGOUT_CODE        10

#define SENDMSG_CODE      100
#define CREATECON_CODE    101
#define PENDANTMSG_CODE   102
#define WHOIS_CODE        103
#define QUITCHAT_CODE     104


#define ISONLINE_CODE     200
// =================================

#define DEBUG               1

#define DOMAIN              AF_INET
#define MAX_REQUEST_LEN     255

#define MAX_USERNAME_SIZE   16
#define MAX_PW_SIZE         16
                                                       //        - SPACING - INT (CODE)
#define MAX_MSG_SIZE        MAX_REQUEST_LEN - MAX_USERNAME_SIZE  - 2       - 4
 

#define FILE_REGISTER       "registro.txt"
#define FILE_USERS          "utenti.txt"
#define TMP_FILE_REGISTER   "tmp.txt"
#define TMP_FILE            "tmp.txt"
#define HANGING_FILE        "hanging"
#define SHOW_FILE           "show"

#define PORT_NOT_KNOWN       0000

#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

struct user {
    char username[MAX_USERNAME_SIZE];
    char pw[MAX_PW_SIZE];
    int  port; // vecchia porta
    long unsigned int login_timestamp;
    long unsigned int logout_timestamp;
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
