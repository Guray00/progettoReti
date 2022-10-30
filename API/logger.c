#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
Questo sorgente consente di eseguire il debug su file in
modo semplificato. Il file è stato reperito e modificato
da fonti terze secondo la seguente licenza:

 * simple_logger
 * @license The MIT License (MIT)
 *   @copyright Copyright (c) 2015 EngineerOfLies
 *    Permission is hereby granted, free of charge, to any person obtaining a copy
 *    of this software and associated documentation files (the "Software"), to deal
 *    in the Software without restriction, including without limitation the rights
 *    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the Software is
 *    furnished to do so, subject to the following conditions:
 *    The above copyright notice and this permission notice shall be included in all
 *    copies or substantial portions of the Software.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *    SOFTWARE.
 */


static FILE * __log_file = NULL;
static int dirty = 0;

void close_logger()
{
    if (__log_file != NULL)
    {
        fclose(__log_file);
        __log_file = NULL;
    }
}

void init_logger(const char *log_file_path)
{
    if (log_file_path == NULL)
    {
        __log_file = fopen("output.log","a");
    }
    else
    {
        __log_file = fopen(log_file_path,"a");
    }
    atexit(close_logger);
}

void _slog(char *f,int l,char *msg,...)
{
    va_list ap;
    dirty = 1;

   char buff[100];
   time_t now = time (0);
   strftime (buff, 100, "%H:%M:%S", localtime (&now));

    //printf ("\033[0;37m[%s] ", buff);
    /*echo all logging to stdout*/
   
   /* 
    va_start(ap,msg);
    fprintf(stdout,"%s:%i: ",f,l);
    vfprintf(stdout,msg,ap);
    fprintf(stdout,"\n");
    va_end(ap);
    fprintf(stdout,"\n");
   */

    if (__log_file != NULL)
    {
        fprintf(__log_file, "\033[0;32m[%s]\033[0m ", buff);
        va_start(ap,msg);
        fprintf(__log_file,"(\033[0;35m%-20s\033[0m:\033[1;34m%i\033[0m):",f,l);
        vfprintf(__log_file,msg,ap);
        fprintf(__log_file,"\n");
        va_end(ap);

        slog_sync();
    }
}

void slog_sync()
{
    if ((__log_file == NULL)||(!dirty))return;
    dirty = 0;
    fflush(__log_file);
}
/*eol@eof*/
