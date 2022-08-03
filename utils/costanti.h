#define DEBUG               1

#define DOMAIN              AF_INET
#define MAX_REQUEST_LEN     40

#define MAX_USERNAME_SIZE   16
#define MAX_PW_SIZE         16
#define FILE_REGISTER       "registro.txt"
#define FILE_USERS          "utenti.txt"
#define PORT_NOT_KNOWN       0000

struct user {
    char username[MAX_USERNAME_SIZE];
    char pw[MAX_PW_SIZE];
};