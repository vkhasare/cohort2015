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
uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping);
void display_mapping(grname_ip_mapping_t * mapping, uint32_t count);
void display_clis();

uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping){
    FILE *fp = NULL, *cmd_line = NULL;
    char ip_str[16], cmd[256];
    uint32_t count, i;
   
    strcpy(cmd, "wc -l ");
    strcat(cmd, filename);
    cmd_line = popen (cmd, "r");
    fscanf(cmd_line, "%i", &count);
    pclose(cmd_line);

    *mapping = (grname_ip_mapping_t *) malloc(sizeof(grname_ip_mapping_t) * count); 
    
    fp = fopen(filename, "r");
    for(i = 0; i < count; i++){
        fscanf(fp, "%s %s", (*mapping)[i].grname, ip_str);
        inet_pton(AF_INET, ip_str, &((*mapping)[i].grp_ip));
        //printf("\nGroup name: %s IP addr: %u", (*mapping)[i].grname, (*mapping)[i].grp_ip);
    }
    fclose(fp);

    return count;
}

void display_mapping(grname_ip_mapping_t * mapping, uint32_t count)
{
  uint32_t i;
  char buf[512];
  char remoteIP[INET_ADDRSTRLEN];

  for(i = 0;i < count; i++)
  {
    inet_ntop(AF_INET, &(mapping[i].grp_ip), remoteIP, INET_ADDRSTRLEN);
    sprintf(buf,"Group name: %s \t\tIP addr: %s", mapping[i].grname,remoteIP);
    PRINT(buf);
  }
}

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

void print_my_struct(my_struct_t* m){
    char buf[512];
    sprintf(buf, 
            "\na=%d\nb=%f\nc=%u\nd=%lf\nc[0]=%d", 
            m->a,m->b,m->c_count,m->d,m->c[0]);
    PRINT(buf);
    return;
}
void populate_my_struct(my_struct_t* m){
    m->a = 10;
    m->b = 13.9;
    m->d = 3.14156;
    m->c_count = 4; 
    m->c = (int *) malloc (sizeof(int)*m->c_count);
    m->c[0]=1;
    m->c[1]=10;
    m->c[2]=100;
}

bool process_my_struct(my_struct_t* m, XDR* xdrs){
    char buf[512];
    bool ret_res = (xdr_int(xdrs, &(m->a))  && 
           xdr_float(xdrs, &(m->b)) && 
           xdr_u_int(xdrs, &(m->c_count)) &&
           xdr_double(xdrs, &(m->d)) &&
           xdr_array(xdrs, (char**)&(m->c), &(m->c_count), 100,
                      sizeof(int),(xdrproc_t )xdr_int ));

    if(ret_res){
        if (xdrs->x_op == XDR_ENCODE){
            sprintf(buf, "[I] Push res: %d, Encode res: %d", 
                    xdrrec_endofrecord(xdrs, TRUE), ret_res);
            PRINT(buf);
        }
        if (xdrs->x_op == XDR_DECODE){
            print_my_struct(m);
            sprintf(buf, "[I] Skip rec res: %d, Decode res: %d", 
                    xdrrec_skiprecord (xdrs), ret_res);
            PRINT(buf);
        }
    }

    return ret_res;
}

int rdata (fp, buf, n)
    FILE    *fp;
    char    *buf;
    int     n;
{
    if (n = fread (buf, 1, n, fp))
        return (n);
    else
        return (-1);
}

int wdata (fp, buf, n)
    FILE    *fp;
    char    *buf;
    int     n;
{
    if (n = fwrite (buf, 1, n, fp))
        return (n);
    else
        return (-1);
}

int IS_SERVER(int oper)
{
  if (SERVER_MODE == oper)
     return 1;

  return 0;
}

int IS_CLIENT(int oper)
{
  if (CLIENT_MODE == oper)
     return 1;

  return 0;
}

int create_and_bind(char *machine_addr, char *machine_port, int oper_mode)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    int yes=1;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if (getaddrinfo(machine_addr, machine_port, &hints, &result) != 0)
    {
        PRINT("Failure in getaddrinfo");
        return -1;
    }

    for (rp = result; rp != NULL; rp=rp->ai_next)
    {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
        {
            continue;
        }

        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (IS_SERVER(oper_mode))
        {
           if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
           {
              continue;
           }
        }
        else if (IS_CLIENT(oper_mode))
        {
           if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
           {
              continue;
           }
        }  
        break;
    }

    if (!rp)
    {
      perror("\nFailure in binding socket.");
      exit(1);
    }

    freeaddrinfo(result);

//    DEBUG("Socket is created.");

    return sfd;
}


int make_socket_non_blocking (int sfd)
{
    int flags;

    flags = fcntl(sfd, F_GETFL,0);

    if (flags == -1) {
        perror("Error while fcntl F_GETFL");
        return -1;
    }

    fcntl(sfd, F_SETOWN, getpid());
    if (fcntl(sfd, F_SETFL,flags|O_NONBLOCK) == -1) {
        perror("Error while fcntl F_SETFL");
        return -1;
    }

//  DEBUG("Socket is non-blocking now.");

  return 1;
}


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
}

void display_server_clis()
{
  PRINT("show groups                  -- Displays list of groups");
  PRINT("show msg group <group_name>  --  Enables display of messages for a specific group.");
  PRINT("no msg group <group_name>    --  Disables display of messages for a specific group."); 
}


