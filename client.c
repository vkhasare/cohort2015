#include "common.h"
#define TIMEOUT_SECS 5

int cfd;
char group_name[512];

void sendPeriodicMsg(int signal)
{
    int numbytes;
    char msg[] ="I am Alive";
    char send_msg[512];
    strcpy(send_msg,group_name);
    strcat(send_msg,":");
    strcat(send_msg,msg);

    PRINT("Sending periodic Request.");
    if ((numbytes = send(cfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending msg.");
    }

    alarm(TIMEOUT_SECS);
}

int main(int argc, char * argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int count = 0;
   
    int event_count, index; 
    int /*cfd,*/ status;
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

    cfd = create_and_bind(addr, port, CLIENT_MODE);

    if (cfd == -1)
    {
        perror("\nError while creating and binding the socket.");
        exit(0);
    }

    status = make_socket_non_blocking(cfd);

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    PRINT("..WELCOME TO CLIENT..\n");
    PRINT_PROMPT("[client] ");
 
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
        perror("\nError while creating the epoll.");
        exit(0);
    }

    event.data.fd = cfd;
    event.events = EPOLLIN|EPOLLET;

    status = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event);

    if ( status == -1)
    {
        perror("\nError while adding FD to epoll event.");
        exit(0);
    }

    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN|EPOLLET;

    status = epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &event);

    if ( status == -1)
    {
        perror("\nError while adding STDIN FD to epoll event.");
        exit(0);
    }

    events = calloc(MAXEVENTS, sizeof(event));

    while (1) {
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for accepting new connections on Server Socket */
            if ((cfd == events[index].data.fd) && (events[index].events & EPOLLIN))
            {
          		  char buf[512];
          	  	read(events[index].data.fd, buf, sizeof(buf));
//                buf[strlen(buf)] = '\0';
                PRINT(buf);
            }
            /* Code Block for handling input from STDIN */
            else if (STDIN_FILENO == events[index].data.fd)
            {
                char read_buffer[100];
                char read_buffer_copy[100];
                char *ptr;
                int cnt=0, i = 0;
                cnt=read(events[index].data.fd, read_buffer, 99);

                PRINT_PROMPT("[client] ");
            }
        }
    }
    
    close(sockfd);
    
    return 0;
}




