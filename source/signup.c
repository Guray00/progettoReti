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
#include "../API/logger.h"
#include "../utils/costanti.h"
#include "./signup.h"

extern int ret;  /* Declaration of the variable */
extern int csd;

