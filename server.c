#include "common.h"
#include "SLL/server_ll.h"
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

uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t **server_info)
{
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
        ADD_GROUP_IN_LL(server_info,(*mapping)[i].grname,(*mapping)[i].grp_ip);
    }
    fclose(fp);
  return count;
}

void process_join_request(server_information_t *server_info, int infd, char *grp_name, grname_ip_mapping_t *mapping, int num_groups)
{
  if (join_group(infd, grp_name, mapping, num_groups))
  {
    socklen_t addr_len;
    struct sockaddr_storage addr;

    char ipstr[INET6_ADDRSTRLEN];
    getpeername(infd, (struct sockaddr*) &addr, &addr_len);
    UPDATE_GRP_CLIENT_LL(&server_info,grp_name,(struct sockaddr*) &addr,infd);
  }
}

decode_join_request(char *buf, int msglen, int infd, grname_ip_mapping_t *mapping, int num_groups, server_information_t **server_info)
{
  char *ptr=buf;
  int len_join_req = strlen("JOIN:");
  int i = len_join_req;
  while (i <= msglen)
  {
    ptr += len_join_req;

    PRINT("Received request from client to join group\n");
    PRINT(ptr);
    process_join_request(*server_info, infd, ptr, mapping, num_groups);

    ptr = ptr + 3;
    i += len_join_req + 3;
  }
}

void accept_connections(int sfd,int efd,struct epoll_event *event)
{
    char remoteIP[INET6_ADDRSTRLEN];
    struct sockaddr_storage in_addr;
    socklen_t in_len;
    int infd, status;
    char buf[512];

    in_len = sizeof(in_addr);

    if ((infd = accept(sfd, (struct sockaddr *) &in_addr, &in_len)) == -1)
    {
       perror("\nError while accept.");
       exit(0);
    }

    inet_ntop(in_addr.ss_family, get_in_addr((struct sockaddr*)&in_addr), remoteIP, INET6_ADDRSTRLEN);
    sprintf(buf,"New connection came from %s and socket %d.\n",remoteIP,infd);
    PRINT(buf);

    status = make_socket_non_blocking(infd);

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    event->data.fd = infd;
    event->events = EPOLLIN|EPOLLET;
    status = epoll_ctl(efd, EPOLL_CTL_ADD, infd, event);
}


void display_group_info(server_information_t **server_info)
{
  display_mcast_group_node(server_info);
}


int main(int argc, char * argv[])
{
    int sfd, efd, status;
    int event_count, index;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char *addr,*port;
    char *ptr;
    int group_msg[1000] = {0};
    uint32_t num_groups;
   
    server_information_t *server_info = NULL;
    grname_ip_mapping_t * mapping = NULL;

    allocate_server_info(&server_info);

    num_groups = initialize_mapping("./ip_mappings.txt", &mapping, &server_info);
    
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

    
    //DEBUG("Started listening for connections..\n");
    PRINT("..WELCOME TO SERVER..");
    PRINT("\r   <Use \"show help\" to see all supported clis.>\n");

    PRINT_PROMPT("[server] ");
 
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

        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for accepting new connections on Server Socket*/
            if (sfd == events[index].data.fd)
            {
                accept_connections(sfd,efd,&event);
                continue;
            }
            /* Code Block for handling input from STDIN */
            else if (STDIN_FILENO == events[index].data.fd) {
                char read_buffer[100];
                char read_buffer_copy[100];
                char *ptr;
                int cnt=0, i = 0;
                cnt=read(events[index].data.fd, read_buffer, 99);

                if (cnt > 0)
                {
                  read_buffer[cnt-1] = '\0';
                  if (0 == strncmp(read_buffer,"show help",9))
                  {
                    display_server_clis();
                  }
                  else if(strncmp(read_buffer,"enable msg group",16) == 0)
                  {
                    strcpy(read_buffer_copy,read_buffer);
                    ptr = strtok(read_buffer_copy," ");
                    while(i < 3)
                    {
                      ptr = strtok(NULL," ");
                      i++;
                    }
                    if (!ptr)
                    {
                        PRINT("Error: Unrecognized Command.\n");
                        continue;
                    }
                    for(i = 0;i < num_groups; i++)
                    {
                      if(strcmp(ptr,mapping[i].grname) == 0)
                      {
                        group_msg[i] = 1;
                      }
                    }
                  }
                  else if(strncmp(read_buffer,"no msg group",12) == 0)
                  {
                    strcpy(read_buffer_copy,read_buffer);
                    ptr = strtok(read_buffer_copy," ");
                    while(i < 3)
                    {
                      ptr = strtok(NULL," ");
                      i++;
                    }
                    if (!ptr)
                    {
                        PRINT("Error: Unrecognized Command.\n");
                        continue;
                    }
                    for(i = 0;i < num_groups; i++)
                    {
                      if(strcmp(ptr,mapping[i].grname) == 0)
                        group_msg[i] = 0;
                    }
                  }
                  else if (0 == strcmp(read_buffer,"show groups\0"))
                  {
                    if(mapping)
                      display_mapping(mapping,num_groups);
                  }
                  else if (0 == strncmp(read_buffer,"show group info",15))
                  {
                     strcpy(read_buffer_copy,read_buffer);
                     ptr = strtok(read_buffer_copy," ");
                     while(i < 3)
                     {
                       ptr = strtok(NULL," ");
                       i++;
                     }
                     if (!ptr)
                     {
                        PRINT("Error: Unrecognized Command.\n");
                        continue;
                     }
                     if(strncmp(ptr,"all",3) == 0)
                     {
                       if(mapping)
                         display_group_info(&server_info);
                     }
                     else
                     {
                        display_mcast_group_node_by_name(&server_info, ptr);
                     }
                  }
                  else if (0 == strcmp(read_buffer,"cls\0"))
                  {
                    system("clear");
                  }
                  else
                  {
                    if (cnt != 1 && read_buffer[0] != '\n')
                       PRINT("Error: Unrecognized Command.\n");
                  }
                }

                PRINT_PROMPT("[server] ");
            }
            /* Code Block for handling events on connection sockets  */
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
                    sprintf(buf,"Client is dead having Socket no. %d.",events[index].data.fd);
                    PRINT(buf);
                    //Implicitly removed fd from epoll as well.
                    close(events[index].data.fd);
                    active_clients--;
                    break;
                }
                else
                {
                  if(strncmp(buf,"JOIN:",5 )){
                  int j =0;
                  for(j = 0; j< num_groups; j++)
                  {
                    if(group_msg[j]) 
                    {
                      strcpy(buf_copy,buf);
                      ptr = strtok(buf_copy,":");

                      if(strcmp(ptr,mapping[j].grname) == 0) {
                        PRINT(buf);
                      }
                    }
                  }

                  int numbytes;
                  if ((numbytes = send(events[index].data.fd,"EchoResponse.",15,0)) < 0)
                  {
                      PRINT("Error in sending.");
                  }
                 }else{
                   int infd=events[index].data.fd;
                   strcpy(buf_copy,buf);
                   decode_join_request(buf, count, infd, mapping, num_groups, &server_info);
                }
             }
        }
      }
    }    
    return 0;
}


