#include "common.h"
#include "server_DS.h"
#include "RBT.h"

grname_ip_mapping_t * mapping = NULL;
extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
unsigned int num_groups = 0; // should be removed in future, when we remove mapping array and completely migrate on LL.

static int handle_join_req(const int sockfd, pdu_t *pdu, ...);
static int handle_echo_req(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_req(const int sockfd, pdu_t *pdu, ...);

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

/* <doc>
 * static
 * int handle_leave_req(const int sockfd, const comm_struct_t const req, ...)
 * Server processes Group Leave Request of client and responds back to
 * client with the appropriate cause.
 *
 * </doc>
 */ 
static
int handle_leave_req(const int sockfd, pdu_t *pdu, ...)
{
    comm_struct_t *req = pdu->msg;
    comm_struct_t resp;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause = REJECTED;
    bool found;
    server_information_t *server_info = NULL;
    unsigned int clientID = 0;
    RBT_tree *tree = NULL;
    RBT_node *rbNode = NULL;
    pdu_t rsp_pdu;

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    leave_req_t leave_req = req->idv.leave_req;
    leave_rsp_t *leave_rsp = &resp.idv.leave_rsp;

    resp.id = leave_response;
    leave_rsp->num_groups = 1;
    leave_rsp->group_ids = MALLOC_IE(1);
    
    group_name = leave_req.group_ids[0].str;

    clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    PRINT("[Leave_Request: GRP - %s, CL - %d] Leave Request Received.", group_name, clientID);

    tree = (RBT_tree*) server_info->client_RBT_head;

    if (clientID > 0) {
         rbNode = RBFindNodeByID(tree, clientID);

         if (rbNode) {
              mcast_group_node_t *ll_grp_node;
              rb_info_t *rb_info_list = (rb_info_t*) rbNode->client_grp_list;

              /*remove node from rbnode list.*/
              if (remove_rb_grp_node(&rb_info_list,group_name, &ll_grp_node)) {
                  cause = ACCEPTED;

                  /*remove node from ll list*/
                  remove_client_from_mcast_group_node(&server_info, ll_grp_node, clientID);

                  /*Remove RBnode from tree if group count is zero.*/
                  if (SN_LIST_LENGTH(&rb_info_list->cl_list->group_node) == 0) {
                    RBDelete(tree, rbNode);
                  }
              }
 
             /* Uncomment to display rb group list for client
              *   display_rb_group_list(&rb_info_list);
              */
               
         }
}

    leave_rsp->group_ids[0].str = MALLOC_STR;
    strcpy(leave_rsp->group_ids[0].str,group_name);

    leave_rsp->cause = cause;

    PRINT("[Leave_Response: GRP - %s, CL - %d] Cause: %s.",group_name, clientID, enum_to_str(cause));

    rsp_pdu.msg = &resp;
    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    return 0;
}

/* <doc>
 * static
 * int handle_join_req(const int sockfd, const comm_struct_t const req, ...)
 * Server processes Group Join Request of client and responds back to
 * client with the appropriate cause.
 *
 * </doc>
 */
static
int handle_join_req(const int sockfd, pdu_t *pdu, ...){
    comm_struct_t *req = pdu->msg;
    comm_struct_t resp;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause;
    server_information_t *server_info = NULL;
    unsigned int clientID = 0;
    RBT_tree *tree = NULL;
    RBT_node *newNode = NULL;
    mcast_group_node_t *group_node = NULL;
    pdu_t rsp_pdu;

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    join_req_t join_req = req->idv.join_req;   
    join_rsp_t *join_rsp = &resp.idv.join_rsp;

    resp.id = join_response;
    join_rsp->num_groups = join_req.num_groups;
    /* Initializing is must for xdr msg. */
    join_rsp->group_ips = (l_saddr_in_t *) calloc (join_req.num_groups, sizeof(l_saddr_in_t));
    
    for(cl_iter = 0; cl_iter < join_req.num_groups; cl_iter++){

        cause = REJECTED;
        group_name = join_req.group_ids[cl_iter].str;

        PRINT("[Join_Request: GRP - %s] Join Request Received.", group_name);

        /*search group_node in LL by group name*/
        get_group_node_by_name(&server_info, group_name, &group_node);

        if (group_node) {
             cause = ACCEPTED;

             clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

             /*Add in group linked list*/
             ADD_CLIENT_IN_GROUP(&group_node,(struct sockaddr*) &pdu->peer_addr, clientID);

             /*Add in Client RBT*/
             tree = (RBT_tree*) server_info->client_RBT_head;

             if (clientID > 0) {
                 /*search the RB node by clientID*/
                 newNode = RBFindNodeByID(tree, clientID);
                 /* Add in RBT if it is a new client, else update the grp list of client */
                 if (!newNode) {
                     newNode = RBTreeInsert(tree, clientID, (struct sockaddr*) &pdu->peer_addr, 0, FREE, group_node);
                 } else {
                    /* Update group_list of the client node */
                    rb_info_t *rb_info_list = (rb_info_t*) newNode->client_grp_list;
                    rb_cl_grp_node_t *rb_grp_node = allocate_rb_cl_node(&rb_info_list);
                    /* storing grp node pointer in RB group list*/
                    rb_grp_node->grp_addr = group_node;

                    /* Uncomment to display rb group list for client
                     * display_rb_group_list(&rb_info_list);
                     */                    
                 }
             }

             /*Uncomment for printing RBT node keys
              *RBTreePrint(tree);
              */

             /* Add ip addr as response and break to search for next group. */
             join_rsp->group_ips[cl_iter].sin_family =
                     group_node->group_addr.sin_family;
             join_rsp->group_ips[cl_iter].sin_port   =
                     group_node->group_addr.sin_port;
             join_rsp->group_ips[cl_iter].s_addr     =
                     group_node->group_addr.sin_addr.s_addr;
             join_rsp->group_ips[cl_iter].grp_port   =
                    group_node->group_port;
        }

         /* cause is ACCEPTED if group is valid, otherwise REJECTED. */
        join_rsp->group_ips[cl_iter].group_name = MALLOC_STR;
        strcpy(join_rsp->group_ips[cl_iter].group_name, group_name);

        join_rsp->group_ips[cl_iter].cause =  cause;

        PRINT("[Join_Response: GRP - %s] Cause: %s.", group_name, enum_to_str(cause));
    }
    
    rsp_pdu.msg = &resp;
    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);
    return 0;
}

static
int handle_echo_req(const int sockfd, pdu_t *pdu, ...){
    comm_struct_t *req = pdu->msg;
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

/* <doc>
 * void display_group_info(server_information_t *server_info)
 * Shows information for all the groups maintained
 * on server.
 *
 * </doc>
 */
void display_group_info(server_information_t *server_info)
{
  display_mcast_group_node(&server_info);
}


int IntComp(unsigned a,unsigned int b) {
  if( a > b) return(1);
  if( a < b) return(-1);
  return(0);
}

void IntPrint(unsigned int a) {
  PRINT("number - %u", a);
}


int main(int argc, char * argv[])
{
    int sfd, efd, status, msfd;
    int event_count, index;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;
    server_information_t *server_info = NULL;
    RBT_tree* tree = NULL;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char *addr,*port;
    char *ptr;
    int group_msg[1000] = {0};
    //uint32_t num_groups;
    struct sockaddr_in maddr;
    char *message="Hello!!!",ip_addr[16];
   
    /* allocating server info for LL */ 
    allocate_server_info(&server_info);

    /* allocating RBT root for faster client lookup on server */
    tree = RBTreeCreate(IntComp, NullFunction, IntPrint);
    server_info->client_RBT_head = tree;

    num_groups = initialize_mapping("src/ip_mappings.txt", &mapping,server_info);
    
    if (argc != 3)
    {
      printf("Usage: %s <server_IP> <server_port>\n", argv[0]);
      struct sockaddr myIp;
      argv[1] = &ip_addr;
      get_my_ip("eth0", &myIp);
      inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(myIp)), argv[1], INET6_ADDRSTRLEN);
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
                fptr func;

                /* Allocating PDU for incoming message */
                pdu_t pdu;
                if ((func = server_func_handler(read_record(events[index].data.fd, &pdu))))
                {
                    (func)(events[index].data.fd, &pdu, server_info);
                }

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

            }
        }
    }
    return 0;
}


