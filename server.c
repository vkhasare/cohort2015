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

int main(int argc, char * argv[])
{
    int sfd, efd, status;
    int event_count, index;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];
    char *addr,*port;
    char *ptr;
    int group_msg[1000] = {0};
    uint32_t num_groups;
    
    grname_ip_mapping_t * mapping;
    
    num_groups = initialize_mapping("./ip_mappings.txt", &mapping);
    
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
        perror("\nError while creating and binding the socket.");
        exit(0);
    }
    
    status = make_socket_non_blocking(sfd);

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }
    
    listen(sfd, BACKLOG);
    
    printf("\nStarted listening for connections..");
 
    efd = epoll_create(MAXEVENTS);

    if (efd == -1)
    {
        perror("\nError while creating the epoll.");
        exit(0);
    }
    
    event.data.fd = sfd;
    event.events = EPOLLIN|EPOLLET;

    status = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
   
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
        //printf("\nWaiting for something to happen.. (ACTIVE_CLIENTS - %d)",active_clients);

        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            if (sfd == events[index].data.fd)
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

                status = make_socket_non_blocking(infd);
                
                event.data.fd = infd;
                event.events = EPOLLIN|EPOLLET;
                
                status = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                printf("\n[I] Accepted.");
                active_clients++;
                continue;
            }
            else if (STDIN_FILENO == events[index].data.fd) {
                char read_buffer[100];
                char read_buffer_copy[100];
                char *ptr;
                int cnt=0, i = 0;
                cnt=read(events[index].data.fd, read_buffer, 99);

                if (cnt > 0)
                {
                  read_buffer[cnt-1] = '\0';
                  printf("\nYou typed - %s", read_buffer);
                  if (0 == strcmp(read_buffer,"show date\0"))
                  {
                    printf("\nDate: %s %s\n",__DATE__,__TIME__);
                  }
                  if(strncmp(read_buffer,"show msg group",14) == 0)
                  {
                    printf("\n%s",read_buffer);
                    strcpy(read_buffer_copy,read_buffer);
                    ptr = strtok(read_buffer_copy," ");
                    while(i < 2)
                    {
                      ptr = strtok(NULL," ");
                      i++;
                    }
                    printf("\n grp name: %s",ptr);
                    for(i = 0;i < num_groups; i++)
                    {
                      if(strcmp(ptr,mapping.grname) == 0)
                        group_msg[i] = 1;
                    }
                  }
                  if(strncmp(read_buffer,"no msg group",12) == 0)
                  {
                    printf("\n%s",read_buffer);
                    strcpy(read_buffer_copy,read_buffer);
                    ptr = strtok(read_buffer_copy," ");
                    while(i < 2)
                    {
                      ptr = strtok(NULL," ");
                      i++;
                    }
                    printf("\n grp name: %s",ptr);
                    for(i = 0;i < num_groups; i++)
                    {
                      if(strcmp(ptr,mapping.grname) == 0)
                        group_msg[i] = 0;
                    }
                  }
                  else if (0 == strcmp(read_buffer,"show groups\0"))
                  {
                    printf("\nshow groups");
                    if(mapping)
                      display_mapping(mapping,num_groups);
                  }
                }
            }
            else
            {
                ssize_t count;
                char buf[512];
                char buf_copy[512];
               
                count = read(events[index].data.fd, buf, sizeof(buf));
                
                if (count == -1)
                {
                    break;
                }
                else if (count == 0)
                {
                    printf("\nClient is dead having Socket no. %d.",events[index].data.fd);
                    close(events[index].data.fd);
                    active_clients--;
                    break;
                }
                else
                {
                  int j =0;
                  for(j = 0; j< num_groups; j++)
                  {
                    if(group_msg[j]) 
                    {
                      strcpy(buf_copy,buf);
                      ptr = strtok(buf_copy,":");

                      if(strcmp(ptr,mapping.grname[j]) == 0) {
                        printf("\n\n[I] Message from client - %d",count);
                        printf("\n%s",buf);
                      }
                    }
int numbytes;
                    if ((numbytes = send(events[index].data.fd,"EchoResponse.",15,0)) < 0)
                    {
                        printf("\nError in sending\n");
                    }
                  }
                }
                
            }
        }
    }
    
    return 0;
}


