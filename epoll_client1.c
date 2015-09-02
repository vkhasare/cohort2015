#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define PORT "3490"
#define MAXDATASIZE 100
#define MAXEVENTS 600

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
}

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


    if (getaddrinfo("localhost", port, &hints, &result) != 0)
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

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
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

    return 0;
}

int main(int argc, const char * argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int count = 0;
    
    int sfd, s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    int active_clients = 0;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];


    while (active_clients < MAXEVENTS) {
    sfd = create_and_bind(PORT);

    if (sfd == -1)
    {
        perror("\nError in sfd");
        exit(0);
    }

    s= make_socket_non_blocking(sfd);    
 
    efd = epoll_create(MAXEVENTS);

    if (efd == -1)
    {
        perror("\nError in epoll_create1");
        exit(0);
    }

    event.data.fd = sfd;
    event.events = EPOLLIN|EPOLLET;

    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    printf("Connection %d", ++active_clients);

    if ( s == -1)
    {
        exit(0);
    }
sleep(1);
    }
    events = calloc(MAXEVENTS, sizeof(event));

    while (1) {
        int n, i;
        printf("\nWaiting for something to happen.. (ACTIVE_CLIENTS - %d)",active_clients);
        n = epoll_wait(efd, events, MAXEVENTS, -1);


        for (i = 0; i < n; i++) {
            if (sfd == events[i].data.fd)
            {


                printf("\n[I] New connection came from %s and socket %d.","hh",1);

                printf("\n[I] Accepted.");
                continue;
            }
            else
            {
                    printf("\nSomething on socket to read - from socket");

            }
        }
    }

   // inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    
    //printf("client: connecting to %s\n", s);
    
   /* 
    if ((numbytes = send(sockfd,"hello rohit",13,0)) < 0)
    {
        printf("\nError in sending\n");
    }
    */
//   while(1); 
    
    close(sockfd);
    
    return 0;
}




