#include "common.h"
#include "SLL/server_ll.h"

grname_ip_mapping_t * mapping = NULL;
extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
unsigned int num_groups = 0; // should be removed in future, when we remove mapping array and completely migrate on LL.

static int handle_join_req(const int sockfd, const comm_struct_t const req, ...);
static int handle_echo_req(const int sockfd, const comm_struct_t const req, ...);
static int handle_leave_req(const int sockfd, const comm_struct_t const req, ...);

/* <doc>
 * fptr server_func_handler(unsigned int msgType)
 * This function takes the msg type as input
 * and returns the respective function handler
 * name.
 *
 * </doc>
 */
fptr server_func_handler(unsigned int msgType)
{
  fptr func_name;

  switch(msgType)
  {
    case join_request:
        func_name = handle_join_req;
        break;
    case leave_request:
        func_name = handle_leave_req;
        break;
    case echo_req:
        func_name = handle_echo_req;
        break;
    default:
        PRINT("Invalid msg type of type - %d.", msgType);
        func_name = NULL;
  }

  return func_name;
}

static
int handle_leave_req(const int sockfd, const comm_struct_t const req, ...)
{
    comm_struct_t resp;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause = REJECTED;
    bool found;
    int clientid;
    server_information_t *server_info = NULL;

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(req, server_information_t*, server_info);

    leave_req_t leave_req = req.idv.leave_req;
    leave_rsp_t *leave_rsp = &resp.idv.leave_rsp;

    resp.id = leave_response;
    leave_rsp->num_groups = 1;
    leave_rsp->group_ids = MALLOC_IE(1);
    
    clientid = leave_req.client_id;
    group_name = leave_req.group_ids[0].str;

    PRINT("[Leave_Request: GRP - %s, CL - %d] Leave Request Received.", group_name, clientid);

    if (remove_client_from_mcast_group_node(&server_info, group_name, clientid))
    {
       cause = ACCEPTED;
    }

    leave_rsp->group_ids[0].str = MALLOC_STR;
    strcpy(leave_rsp->group_ids[0].str,group_name);

    leave_rsp->cause = cause;

    PRINT("[Leave_Response: GRP - %s, CL - %d] Cause: %s.",group_name, clientid, enum_to_str(cause));

    write_record(sockfd, &resp);
    return 0;
}

static
int handle_join_req(const int sockfd, const comm_struct_t const req, ...){
    comm_struct_t resp;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause;
    server_information_t *server_info = NULL;

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(req, server_information_t*, server_info);

    join_req_t join_req = req.idv.join_req;   
    join_rsp_t *join_rsp = &resp.idv.join_rsp;

    resp.id = join_response;
    join_rsp->num_groups = join_req.num_groups;
    /* Initializing is must for xdr msg. */
    join_rsp->group_ips = (l_saddr_in_t *) calloc (join_req.num_groups, sizeof(l_saddr_in_t));
    
    for(cl_iter = 0; cl_iter < join_req.num_groups; cl_iter++){

        cause = REJECTED;
        group_name = join_req.group_ids[cl_iter].str;

        PRINT("[Join_Request: GRP - %s] Join Request Received.", group_name);

        for(s_iter = 0; s_iter < num_groups; s_iter++ ){
            if(strcmp(group_name, mapping[s_iter].grname) == 0){
                //update internal structures
                struct sockaddr_storage addr;
                socklen_t addr_len = sizeof addr;
                
                char ipstr[INET6_ADDRSTRLEN];
                int pname = getpeername(sockfd, (struct sockaddr*) &addr, &addr_len);

                cause = ACCEPTED;

                UPDATE_GRP_CLIENT_LL(&server_info, group_name, (struct sockaddr*) &addr, sockfd);

                /* Add ip addr as response and break to search for next group. */
                join_rsp->group_ips[cl_iter].sin_family =
                      mapping[s_iter].grp_ip.sin_family;
                join_rsp->group_ips[cl_iter].sin_port   =
                      mapping[s_iter].grp_ip.sin_port;
                join_rsp->group_ips[cl_iter].s_addr     =
                      mapping[s_iter].grp_ip.sin_addr.s_addr;
                join_rsp->group_ips[cl_iter].grp_port   =
                     mapping[s_iter].port_no;
                break;
            }
        }

         /* cause is ACCEPTED if group is valid, otherwise REJECTED. */
        join_rsp->group_ips[cl_iter].group_name = MALLOC_STR;
        strcpy(join_rsp->group_ips[cl_iter].group_name, group_name);

        join_rsp->group_ips[cl_iter].cause =  cause;

        PRINT("[Join_Response: GRP - %s] Cause: %s.", group_name, enum_to_str(cause));
    }
    
    write_record(sockfd, &resp);
    return 0;
}

static
int handle_echo_req(const int sockfd, const comm_struct_t const req, ...){
    comm_struct_t resp;
        
    resp.id = echo_response;
    resp.idv.echo_resp.str = (char *) malloc (sizeof(char) * echo_resp_len);
    strcpy(resp.idv.echo_req.str, "EchoResponse.");
    write_record(sockfd, &resp);
    return 0;
}

uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t *server_info)
{
    FILE *fp = NULL, *cmd_line = NULL;
    char ip_str[16], cmd[256], port[16];
    uint32_t count, i;

    strcpy(cmd, "wc -l ");
    strcat(cmd, filename);
    cmd_line = popen (cmd, "r");
    fscanf(cmd_line, "%i", &count);
    pclose(cmd_line);

    *mapping = (grname_ip_mapping_t *) malloc(sizeof(grname_ip_mapping_t) * count);

    fp = fopen(filename, "r");
    for(i = 0; i < count; i++){
        fscanf(fp, "%s %s %s", (*mapping)[i].grname, ip_str,port);
        inet_pton(AF_INET, ip_str, &((*mapping)[i].grp_ip));
        (*mapping)[i].port_no = atoi(port);
        ADD_GROUP_IN_LL(&server_info,(*mapping)[i].grname,(*mapping)[i].grp_ip,(*mapping)[i].port_no);
    }
    fclose(fp);

  return count;
}


void accept_connections(int sfd,int efd,struct epoll_event *event)
{
    char remoteIP[INET6_ADDRSTRLEN];
    struct sockaddr_storage in_addr;
    socklen_t in_len;
    int infd, status;

    in_len = sizeof(in_addr);

    if ((infd = accept(sfd, (struct sockaddr *) &in_addr, &in_len)) == -1)
    {
       perror("\nError while accept.");
       exit(0);
    }

    inet_ntop(in_addr.ss_family, get_in_addr((struct sockaddr*)&in_addr), remoteIP, INET6_ADDRSTRLEN);
    PRINT("New connection came from %s and socket %d.\n",remoteIP,infd);

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


void display_group_info(server_information_t *server_info)
{
  display_mcast_group_node(&server_info);
}


int main(int argc, char * argv[])
{
    int sfd, efd, status, msfd;
    int event_count, index;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;
    server_information_t *server_info = NULL;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char *addr,*port;
    char *ptr;
    int group_msg[1000] = {0};
    //uint32_t num_groups;
    struct sockaddr_in maddr;
    char *message="Hello!!!";
    
    allocate_server_info(&server_info);

    num_groups = initialize_mapping("src/ip_mappings.txt", &mapping,server_info);
    
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
                         display_group_info(server_info);
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
                  else if( strncmp(read_buffer,"send msg",8) == 0)
                  {
                     char remoteIP[INET_ADDRSTRLEN];
                     unsigned int port;
                     strcpy(read_buffer_copy,read_buffer);
                     ptr = strtok(read_buffer_copy," ");
                     while(i < 2)
                     {
                       ptr = strtok(NULL," ");
                       i++;
                     }
                     for(i = 0;i < num_groups; i++)
                     {
                       if(strcmp(mapping[i].grname,ptr) == 0)
                       {
                         inet_ntop(AF_INET, &(mapping[i].grp_ip), remoteIP, INET_ADDRSTRLEN);
                         port = mapping[i].port_no;
                       }
                     }
                     if ((msfd=socket(AF_INET,SOCK_DGRAM,0)) < 0) 
                     {
                        PRINT("Error .. ");
                        exit(1);
                     }
                     memset(&maddr,0,sizeof(maddr));
                     maddr.sin_family=AF_INET;
                     maddr.sin_addr.s_addr=inet_addr(remoteIP);
                     maddr.sin_port=htons(port);

                     if (sendto(msfd,message,strlen(message),0,(struct sockaddr *) &maddr,sizeof(maddr)) < 0) 
                     {
                       PRINT("could not send multicast msg");
                       exit(1);
                     }
                     else
                     {
                       PRINT("Sent msg %s to group %s",message,ptr);
                     }
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
                comm_struct_t req;
                fptr func;

                memset(&req, 0 ,sizeof(req));

                if ((func = server_func_handler(read_record(events[index].data.fd, &req))))
                {
                    (func)(events[index].data.fd, req, server_info);
                }
            }
        }
    }
    return 0;
}


