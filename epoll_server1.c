

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

#define PORT "3490"
#define BACKLOG 30000
#define MAXEVENTS 30000

/*
typedef union epoll_data {
    void        *ptr;
    int          fd;
    uint32_t     u32;
    uint64_t     u64;
} epoll_data_t;

typedef struct epoll_event
{
    __uint32_t event;
    epoll_data_t data;
};
*/

int create_and_bind(char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    int yes=1;
    
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    
    if (getaddrinfo(NULL, port, &hints, &result) != 0)
    {
        printf("\nFailure in getaddrinfo");
        return -1;
    }
    
    for (rp = result; rp != NULL; rp=rp->ai_next) {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
        {
            continue;
        }
        
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
        {
            continue;
        }
        
        break;
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
   
//    printf("\nsocket is non blocking now.");
 
    return 0;
}


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
}


int main(int argc, const char * argv[])
{
    int sfd, s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];

    sfd = create_and_bind(PORT);
    
    if (sfd == -1)
    {
        perror("\nError in sfd");
        exit(0);
    }
    
    s= make_socket_non_blocking(sfd);
    
    if (s == -1)
    {
        perror("\nError in non_blocking");
        exit(0);
    }
    
    listen(sfd, BACKLOG);
    
    printf("\nStarted listening for connections..");
 
    efd = epoll_create(MAXEVENTS);

    if (efd == -1)
    {
        perror("\nError in epoll_create1");
        exit(0);
    }
    
    event.data.fd = sfd;
    event.events = EPOLLIN|EPOLLET;

    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
   
    if ( s == -1)
    {
        exit(0);
    }
   
    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN|EPOLLET;

    s = epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &event);
 
    events = calloc(MAXEVENTS, sizeof(event));

    while (1) {
        int n, i;
        printf("\nWaiting for something to happen.. (ACTIVE_CLIENTS - %d)",active_clients); 
        n = epoll_wait(efd, events, MAXEVENTS, -1);

        for (i = 0; i < n; i++) {
            if (sfd == events[i].data.fd)
            {

                struct sockaddr_storage in_addr;
                socklen_t in_len;
                int infd;
                
                in_len = sizeof(in_addr);
                
                if ((infd = accept(sfd, (struct sockaddr *) &in_addr, &in_len)) == -1)
                {
                    perror("\nError while accept.");
                    exit(0);
                }
              
                inet_ntop(in_addr.ss_family, get_in_addr((struct sockaddr*)&in_addr), remoteIP, INET6_ADDRSTRLEN);
 
                printf("\n[I] New connection came from %s and socket %d.",remoteIP,infd);

                s = make_socket_non_blocking(infd);
                
                event.data.fd = infd;
                event.events = EPOLLIN|EPOLLET;
                
                s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                printf("\n[I] Accepted.");
                active_clients++;
                continue;
            }
            else if (STDIN_FILENO == events[i].data.fd) {
                char read_buffer[100];
                int cnt=0;
                cnt=read(events[i].data.fd, read_buffer, 99);

                if (cnt > 0)
                {
                  read_buffer[cnt-1] = '\0';
                  printf("\nYou typed - %s", read_buffer);
                  if (0 == strcmp(read_buffer,"show date\0"))
                  {
                    printf("\nDate: %s %s\n",__DATE__,__TIME__);
                  }
                }
            }
            else
            {
                ssize_t count;
                char buf[512];
               
                count = read(events[i].data.fd, buf, sizeof(buf));
                
                if (count == -1)
                {
                    break;
                }
                else if (count == 0)
                {
                    printf("\nClient is dead having Socket no. %d.",events[i].data.fd);
                    close(events[i].data.fd);
                    active_clients--;
                    break;
                }
                else
                {
                    printf("\nSomething on socket to read - from socket %d.", events[i].data.fd);
                    printf("\n[I] Message from client - %d",count);
                    //s = write(1, buf, count);
                    printf("\n%s",buf);
                   //while (1) {printf("\nRohit"); sleep(5);} 
                    if (s == -1)
                    {
                        perror ("write");
                        exit(0);
                    }
                }
                
            }
        }
    }
    
    return 0;
}


