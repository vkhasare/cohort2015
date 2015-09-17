#include "common.h"
#define TIMEOUT_SECS 5

int sfd;
char group_name[512];

void sendPeriodicMsg(int signal)
{
    int numbytes;
    char msg[] ="I am Alive";
    char send_msg[512];
    strcpy(send_msg,group_name);
    strcat(send_msg,":");
    strcat(send_msg,msg);

    printf("\nSending periodic Request.\n");
    if ((numbytes = send(sfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        printf("\nError in sending\n");
    }

    alarm(TIMEOUT_SECS);
}

int main(int argc, char * argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int count = 0;
    
    int /*sfd,*/ s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];
    char *addr,*port;

    struct sigaction myaction;


    if (argc != 4)
    {
      printf("Usage: %s <client_IP> <client_port> <group_name>\n", argv[0]);
//      exit(1);
//      Temporary code
      argv[1] = "127.0.0.1";
      argv[2] = "3490";
      argv[3] = "G1";
    }

    addr = argv[1];
    port = argv[2];
    strcpy(group_name,argv[3]);

    sfd = create_and_bind(addr, port, CLIENT_MODE);

    if (sfd == -1)
    {
        perror("\nError in sfd");
        exit(0);
    }

    s= make_socket_non_blocking(sfd);    
 
    myaction.sa_handler = sendPeriodicMsg;
    sigfillset(&myaction.sa_mask);
    myaction.sa_flags = 0;
    
    if (sigaction(SIGALRM, &myaction, 0) < 0)
    {
        perror("\nError in sigaction.");
        exit(0);
    }

    alarm(TIMEOUT_SECS);

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
        //printf("\nWaiting for something to happen on client..");
        n = epoll_wait(efd, events, MAXEVENTS, -1);

        for (i = 0; i < n; i++) {
            if ((sfd == events[i].data.fd) && (events[i].events & EPOLLIN))
            {
          		  char buf[512];
          	  	read(events[i].data.fd, buf, sizeof(buf));
//            		printf("%s",buf);
                buf[strlen(buf)] = '\0';
            		write(1, buf, strlen(buf));
                continue;
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




