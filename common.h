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

//#define port "3490"
#define CLIENT_MODE 101
#define SERVER_MODE 202
#define BACKLOG 30000
#define MAXEVENTS 30000
#define MAXDATASIZE 100

extern const unsigned int max_groups;
extern const unsigned int max_gname_len; //includes nul termination

#define PRINT_PROMPT(str)                  \
 do {                                      \
   write(STDOUT_FILENO,"\n",1);            \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

#define PRINT(str)                         \
 do {                                      \
   write(STDOUT_FILENO,"\n\n",2);          \
   write(STDOUT_FILENO,"\t",1);            \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

#define SIMPLE_PRINT(str)                  \
 do {                                      \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

typedef struct grname_ip_mapping{
    char grname[10];
    struct sockaddr_in grp_ip;
}grname_ip_mapping_t; 
  

int create_and_bind(char *machine_addr, char *machine_port, int oper_mode);
int make_socket_non_blocking (int sfd);
void *get_in_addr(struct sockaddr *sa);
int IS_SERVER(int oper);
int IS_CLIENT(int oper);
//uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t ** server_info);
void display_mapping(grname_ip_mapping_t * mapping, uint32_t count);
void display_clis();
