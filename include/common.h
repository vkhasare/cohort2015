#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <limits.h>
#include "comm_primitives.h"
#include "print.h"

//#define port "3490"
#define TIMEOUT_SECS 5
#define CLIENT_MODE 101
#define SERVER_MODE 202
#define BACKLOG 30000
#define MAXEVENTS 30000
#define MAXDATASIZE 100

extern const unsigned int max_groups;
extern const unsigned int max_gname_len; //includes nul termination

/* <doc>
 * EXTRACT_ARG(lastArg, type, var)
 * Macro to extract variadic parameter
 * 
 * where, lastArg - name of last function argument
 *        type - data type of expected variable
 *        varName - name of destination variable
 * </doc>
 */
#define EXTRACT_ARG(lastArg, type, varName)     \
{                                              \
  va_list arguments;                            \
  va_start (arguments, lastArg);                \
  varName = va_arg (arguments, type);           \
  va_end (arguments);                           \
}

/* <doc>
 * enum msg_cause
 * Consists of Response message cause
 * - ACCEPTED
 * - REJECTED
 * - UNKNOWN
 * </doc>
 */
typedef enum {
 ACCEPTED = 10,
 REJECTED = 11,
 UNKNOWN  = 12
} msg_cause;

typedef struct grname_ip_mapping{
    char grname[10];
    struct sockaddr_in grp_ip;
    unsigned int port_no;
}grname_ip_mapping_t; 
  
typedef int (*fptr)(int, comm_struct_t, ...);
int create_and_bind(char *machine_addr, char *machine_port, int oper_mode);
int make_socket_non_blocking (int sfd);
void *get_in_addr(struct sockaddr *sa);
int IS_SERVER(int oper);
int IS_CLIENT(int oper);
//uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t ** server_info);
void display_mapping(grname_ip_mapping_t * mapping, uint32_t count);
void display_clis();
char* enum_to_str(msg_cause cause);
msg_cause str_to_enum(char *str);