#ifndef DEVGUI_H    /* This is an "include guard" */
#include <time.h>

#define DEVGUI_H    /* prevents the file from being included twice. */
                     /* Including a header file twice causes all kinds */
                     /* of interesting problems.*/

void startGUI();
void format_msg(char *formatted_msg, char *source, char* msg);
void print_centered(char*);

#endif /* DEVGUI_H */