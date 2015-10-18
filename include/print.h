#ifndef PRINT_H
#define PRINT_H

#include <stdlib.h>
#include <stdarg.h>

#define MAX_LOGBUFF_SIZE 512

static char* SPRINTF(char *fmt, ...);

/* <doc>
 * PRINT_PROMPT(str)
 * Macro to Print cli prompt on STDOUT
 *
 * where str - text to be printed for prompt
 * </doc>
 */
#define PRINT_PROMPT(str)                  \
 do {                                      \
   write(STDOUT_FILENO,"\n",1);            \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

/* <doc>
 * PRINT(str)
 * Macro to Print text with pre-decided newlines/tabs on STDOUT
 *
 * where str - text to be printed on STDOUT
 * </doc>
 */
#define PRINT(...)                              \
 do {                                               \
   char *out_str = SPRINTF(__VA_ARGS__);                    \
   write(STDOUT_FILENO,"\n\n",2);                   \
   write(STDOUT_FILENO,"\t",1);                     \
   write(STDOUT_FILENO,out_str,strlen(out_str));    \
 } while(0)

#define SIMPLE_PRINT(str)                  \
 do {                                      \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)


static char* SPRINTF(char *fmt, ...)
{
    va_list ap;
    char *retstr;
    static char str[MAX_LOGBUFF_SIZE];

    va_start(ap, fmt);

    if(vsnprintf(str, MAX_LOGBUFF_SIZE, fmt, ap) < 0)
    {
      *str = '\0';
    }

    va_end(ap);

    return str;
}

#endif
