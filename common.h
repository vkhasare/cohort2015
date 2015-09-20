#include <stdio.h>
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
#include <rpc/rpc.h>

//#define port "3490"
#define CLIENT_MODE 101
#define SERVER_MODE 202
#define BACKLOG 30000
#define MAXEVENTS 30000
#define MAXDATASIZE 100


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
  
typedef enum struct_id{
    client_req    = 2001,
    client_init   = 2002,
    server_task   = 2003,
    client_answer = 2004,
    client_leave  = 2005
}e_struct_id_t; 

typedef struct client_req{
    int num_groups;
    char* group_ids; 
}client_req_t;

typedef struct client_init{
    int num_groups;
    int* group_ips; 
}client_init_t;

int create_and_bind(char *machine_addr, char *machine_port, int oper_mode);
int make_socket_non_blocking (int sfd);
void *get_in_addr(struct sockaddr *sa);
int IS_SERVER(int oper);
int IS_CLIENT(int oper);
//uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t ** server_info);
void display_mapping(grname_ip_mapping_t * mapping, uint32_t count);
void display_clis();
typedef struct my_struct{
    int a;
    float b;
    unsigned int c_count;
    double d;
    int *c;
}my_struct_t;

typedef struct common_struct{
    e_struct_id_t id;
    union{
        client_req_t cl_req;
        client_init_t cl_init;
    }recv_struct;
}comm_struct_t;

void print_my_struct(my_struct_t* m);
void populate_my_struct(my_struct_t* m);
int rdata (FILE *fp,char *  buf,int n);
int wdata (FILE *fp,char *  buf,int n);


