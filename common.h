#include <stdio.h>
#include <stdlib.h>
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

typedef struct grname_ip_mapping{
    char grname[10];
    uint32_t grp_ip;
}grname_ip_mapping_t; 

int create_and_bind(char *machine_addr, char *machine_port, int oper_mode);
int make_socket_non_blocking (int sfd);
void *get_in_addr(struct sockaddr *sa);
int IS_SERVER(int oper);
int IS_CLIENT(int oper);
uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping);

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
        (*mapping)[i].grp_ip = htonl(inet_addr(ip_str));
        printf("\nGroup name: %s IP addr: %u", (*mapping)[i].grname, (*mapping)[i].grp_ip);
    }
    fclose(fp);

    return count;
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
        printf("\nFailure in getaddrinfo");
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

    printf("\nSocket is created.");

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

  printf("\nSocket is non-blocking now.");

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


