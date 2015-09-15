#include "common.h"

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
    char *addr,*port;

    if (argc != 3)
    {
      printf("Usage: %s <server_IP> <server_port>\n", argv[0]);
//      exit(1);
//      Temporary code
      argv[1] = "127.0.0.1";
      argv[2] = "3490";
    }

    addr = argv[1];
    port = argv[2];

    sfd = create_and_bind(addr, port, SERVER_MODE);
    
    if (sfd == -1)
    {
        perror("\nError in sfd");
        exit(0);
    }
    
    s = make_socket_non_blocking(sfd);
    
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

                    printf("\nSending periodic Response.");
int numbytes;
                    if ((numbytes = send(events[i].data.fd,"EchoResponse.",15,0)) < 0)
                    {
                        printf("\nError in sending\n");
                    }
                }
                
            }
        }
    }
    
    return 0;
}


