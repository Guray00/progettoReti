#ifndef CHAT_H    /* This is an "include guard" */
#include <time.h>
#include "../utils/costanti.h"

#define CHAT_H       /* prevents the file from being included twice. */
                     /* Including a header file twice causes all kinds */
                     /* of interesting problems.*/

void identification();
void start_chat(struct device_info *info, char* dest, int port);
int wait_for_msg(struct device_info *info, char* dest, int sd);

// void chat_handler(char* dest);

#endif /* CHAT_H */