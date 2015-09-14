#include "common.h"

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
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];
    char *addr,*port;

    if (argc != 3)
    {
      printf("Usage: %s <client_IP> <client_port>\n", argv[0]);
//      exit(1);
//      Temporary code
      argv[1] = "127.0.0.1";
      argv[2] = "3490";
    }

    addr = argv[1];
    port = argv[2];

    sfd = create_and_bind(addr, port, CLIENT_MODE);

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

    if ( s == -1)
    {
        exit(0);
    }

    events = calloc(MAXEVENTS, sizeof(event));

    while (1) {
        int n, i;
        printf("\nWaiting for something to happen on client..");
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




