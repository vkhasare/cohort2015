#include "common.h"
#include "SLL/client_ll.h"

int cfd; /* Required for event handler */
extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination

struct keep_alive{
    unsigned count;
    char group_name[10][10];
};
struct keep_alive active_group;

static int handle_join_response(const int, const comm_struct_t, ...);
static void send_echo_req(const int);
static int handle_echo_response(const int, const comm_struct_t, ...);
static int handle_leave_response(const int, const comm_struct_t, ...);
fptr client_func_handler(unsigned int);
static void send_join_group_req(client_information_t *, char *);
static void send_leave_group_req(client_information_t *, char *);

/* <doc>
 * fptr client_func_handler(unsigned int msgType)
 * This function takes the msg type as input
 * and returns the respective function handler
 * name.
 *
 * </doc>
 */
fptr client_func_handler(unsigned int msgType)
{
  fptr func_name;

  switch(msgType)
  {
    case join_response:
        func_name = handle_join_response;
        break;
    case leave_response:
        func_name = handle_leave_response;
        break;
    case echo_response:
        func_name = handle_echo_response;
        break;
    default:
        PRINT("Invalid msg type of type - %d.", msgType);
        func_name = NULL;
  }

  return func_name;
}

/* <doc>
 * int handle_leave_response(const int sockfd, const comm_struct_t const resp, ...)
 * Group Leave Response handler. If cause in Response PDU is ACCEPTED, then leaves 
 * the multicast group. Else ignores for other causes.
 *  
 * </doc>
 */
static
int handle_leave_response(const int sockfd, const comm_struct_t const resp, ...)
{
        uint8_t cl_iter;
        msg_cause enum_cause = REJECTED;
        char *group_name;
        mcast_client_node_t *client_node = NULL;
        client_information_t *client_info = NULL;

        /* Extracting client_info from variadic args*/
        EXTRACT_ARG(resp, client_information_t*, client_info);

        leave_rsp_t leave_rsp = resp.idv.leave_rsp;

        for(cl_iter = 0; cl_iter < leave_rsp.num_groups; cl_iter++){
            group_name = leave_rsp.group_ids[cl_iter].str;
            enum_cause = leave_rsp.cause;

            PRINT("[Leave_Response: GRP - %s] Cause : %s.", group_name, enum_to_str(enum_cause));

            /* if cause other than ACCEPTED, ignore the response */
            if (enum_cause == ACCEPTED)
            {
                client_node = (mcast_client_node_t *) get_client_node_by_group_name(&client_info, group_name);

                /* Leave the multicast group if response cause is Accepted */
                if (TRUE == multicast_leave(client_node->mcast_fd, client_node->group_addr)) {
                    PRINT("Client has left multicast group %s.", group_name);
                } else {
                    PRINT("[Error] Error in leaving multicast group %s.", group_name);
                }

                /* Removing group association from list */
                deallocate_mcast_client_node(client_info, client_node);
            }

        }
}

/* <doc>
 * int handle_join_response(const int sockfd, const comm_struct_t const resp, ...)
 * Join Group Response Handler. Reads the join response PDU, and fetches the
 * IP for required multicast group. Joins the multicast group.
 *
 * </doc>
 */
static
int handle_join_response(const int sockfd, const comm_struct_t const resp, ...)
{
    struct sockaddr_in group_ip;
    int iter;
    char* group_name;
    unsigned int m_port;
    mcast_client_node_t node;
    int status;
    struct epoll_event *event;
    msg_cause enum_cause;

    client_information_t *client_info = NULL;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(resp, client_information_t*, client_info);

    join_rsp_t join_response = resp.idv.join_rsp; 

    /* Pointer to epoll event structure */
    event = client_info->epoll_evt;
 
    for(iter = 0; iter < join_response.num_groups; iter++){
        enum_cause = join_response.group_ips[iter].cause;

        group_name = join_response.group_ips[iter].group_name;

        if (enum_cause == REJECTED)
        {
            PRINT("[Join_Response: GRP - %s] Cause : %s. (Reason : Non-existent Group)", group_name, enum_to_str(enum_cause));
            continue;
        }

        PRINT("[Join_Response: GRP - %s] Cause : %s.", group_name, enum_to_str(enum_cause));

        group_ip.sin_family = join_response.group_ips[iter].sin_family;
        group_ip.sin_port = join_response.group_ips[iter].sin_port;
        group_ip.sin_addr.s_addr = join_response.group_ips[iter].s_addr;
        
        m_port = join_response.group_ips[iter].grp_port;

        /* Join the multicast group with the groupIP present in join_response msg*/
        int mcast_fd = multicast_join(group_ip,m_port);
        
        PRINT("Listening to group %s\n", group_name);
        
        if(mcast_fd > 0){ 
            memset(&node,0,sizeof(node));

            /* Register multicast FD with EPOLL for listening events.*/
            event->data.fd = mcast_fd;
            event->events = EPOLLIN|EPOLLET;

            status = epoll_ctl(client_info->epoll_fd, EPOLL_CTL_ADD, mcast_fd, event);

            if (status == -1)
            {
              perror("\nError while adding FD to epoll event.");
              exit(0);
            }
            
            /* Update the client-group association in list */
            strcpy(node.group_name,group_name);
            node.group_addr = group_ip;
            node.mcast_fd   = mcast_fd;
            node.group_port = m_port;

            /* If unable to add in list, then leave the multicast group. */
            if (ADD_CLIENT_IN_LL(&client_info, &node) == FALSE)
            {
              multicast_leave(mcast_fd, group_ip);
            }
        }
    }
}

/* <doc>
 * static void send_join_group_req(client_information_t *client_info, char *group_name)
 * This function creates Group Join Request message and sends to server. It accepts
 * group name as input and relays message on client fd. 
 * Request will not be relayed if client is already member of group.
 *
 * </doc>
 */
static void send_join_group_req(client_information_t *client_info, char *group_name)
{

       /* If client has already joined requested group, then no need to send join request*/
       if (IS_GROUP_IN_CLIENT_LL(&client_info,group_name))
       {
          PRINT("Error: Client is already member of group %s.",group_name);
       }
       else
       {
          comm_struct_t msg;

          msg.id = join_request;
          /* Sending join request for 1 group*/
          populate_join_req(&msg, &group_name, 1);
          write_record(client_info->client_fd, &msg);

          PRINT("[Join_Request: GRP - %s] Join Group Request sent to Server.", group_name);
       }

}

/* <doc>
 * static void send_leave_group_req(client_information_t *client_info, char *group_name)
 * This function creates Group Leave Request message and sends to server. It accepts
 * group name as input and relays message on client fd.
 * Request will not be relayed if client is not member of group.
 *
 * </doc>
 */
static void send_leave_group_req(client_information_t *client_info, char *group_name)
{

       /* If client is member of requested group, then only send leave request */
       if (IS_GROUP_IN_CLIENT_LL(&client_info, group_name))
       {
           comm_struct_t msg;

           msg.id = leave_request;
           /* Sending leave request for 1 group*/
           populate_leave_req(&msg, &group_name, 1);
           /*TODO - hardcoded for now.. needs to be done when client has its unique client id.*/
           msg.idv.leave_req.client_id = 5;
           write_record(client_info->client_fd, &msg);

           PRINT("[Leave_Request: GRP - %s] Leave Group Request sent to Server.", group_name);
       }
       else
       {
           /* client is not member of request group */
           PRINT("Error: Client is not member of group %s.", group_name);
       }

}




/*Commenting as not used currently 
void sendPeriodicMsg(int signal)
{
    int numbytes;
    char msg[] ="I am Alive";
    char send_msg[512];
    int i=active_group.count;

    while(i>0)
    {
      i--;
      PRINT(send_msg, "%s:%s\r\n",active_group.group_name[i],msg);
     
      PRINT("Sending periodic Request.");
      if ((numbytes = send(cfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
      {
        PRINT("Error in sending msg.");
      }
    }

    alarm(TIMEOUT_SECS);
}
*/

/*
void sendPeriodicMsg_XDR(int signal)
{
    my_struct_t m;
    XDR xdrs;
    int res;
    FILE* fp = fdopen(cfd, "wb");
    
    populate_my_struct(&m, 2);
    
    xdrs.x_op = XDR_ENCODE;
    xdrrec_create(&xdrs, 0, 0, (char*)fp, rdata, wdata);

//  PRINT("Sending XDR local struct.");
    res = process_my_struct(&m, &xdrs);
    if (!res)
    {
        PRINT("Error in sending msg.");
    }
    postprocess_struct_stream(&m, &xdrs, res);
    xdr_destroy(&xdrs);
    fflush(fp);

    alarm(TIMEOUT_SECS);
}
*/
static
void send_echo_req(int signal){
    int i=active_group.count;
    char msg[] =" I am Alive";
    comm_struct_t resp;
 
    resp.id = echo_req;
    resp.idv.echo_req.str = (char *) malloc (sizeof(char) * echo_req_len);
    resp.idv.echo_req.str[0]='\0';
    while(i-- > 0){
        strcat(resp.idv.echo_req.str, active_group.group_name[i]);
        strcat(resp.idv.echo_req.str,msg);
    }
    PRINT("Sending periodic Request.");
    write_record(cfd, &resp);
    alarm(TIMEOUT_SECS);
}

static
int handle_echo_response(const int sockfd, const comm_struct_t const req, ...){
    PRINT(req.idv.echo_resp.str);
    return 0;
}

int is_gname_already_present(char *grp_name){

   if(active_group.count == 0)
     return 0;  

   int i=0 , count=active_group.count;

   while(i<count)
   {
     if(strcmp(grp_name,active_group.group_name[i])==0) return i+1;
     i++;
   }

   return 0;
}

void insert_gname(char *gname){
   strcpy(active_group.group_name[active_group.count], gname);
   active_group.count++;
}

/* <doc>
 * void startKeepAlive(char * gname)
 * Function to start periodic timer for sending messages to server.
 * Takes the group name as argument.
 *
 * </doc>
 */
void startKeepAlive(char * gname)
{
    if(is_gname_already_present(gname) == 0)
    {
       insert_gname(gname);
    
      if(active_group.count == 1)
      {
        struct sigaction myaction;
        myaction.sa_handler = send_echo_req;
        sigfillset(&myaction.sa_mask);
        myaction.sa_flags = 0;

        if (sigaction(SIGALRM, &myaction, 0) < 0)
        {
          perror("\nError in sigaction.");
          exit(0);
        }
    
        alarm(TIMEOUT_SECS);
      }
   }
}


/* <doc>
 * void stopKeepAlive()
 * Function to stop periodic timer
 *
 * </doc>
 */
void stopKeepAlive()
{
    int i = 0;

    while(i < active_group.count)
    {
      strcpy(active_group.group_name[i] ,"IV");
      i++;
    }

    active_group.count = 0;
    alarm(0);
}

/* <doc>
 * void display_client_clis()
 * Displays all available Cli's.
 *
 * </doc>
 */
void display_client_clis()
{
   PRINT("show client groups                           --  displays list of groups joined by client");
   PRINT("enable keepalive group <group_name>          --  Sends periodic messages to Server");
   PRINT("disable keepalive                            --  Stops periodic messages to Server");
   PRINT("join group <name>                            --  Joins a new group");
   PRINT("leave group <name>                           --  Leaves a group");
   PRINT("cls                                          --  Clears the screen");
}

/* <doc>
 * void display_client_groups(client_information_t *client_info)
 * Prints list of all groups which client has joined.
 * 
 * </doc>
 */
void display_client_groups(client_information_t *client_info)
{
  display_mcast_client_node(&client_info);
}

/* <doc>
 * void client_stdin_data(int fd, client_information_t *client_info)
 * Function for handling input from STDIN. Input can be any cli
 * command and respective handlers are then invoked.
 *
 * </doc>
 */
void client_stdin_data(int fd, client_information_t *client_info)
{
    char read_buffer[MAXDATASIZE];
    int cnt = 0;

    cnt = read(fd, read_buffer, MAXDATASIZE-1);
    read_buffer[cnt-1] = '\0';

    if (0 == strncmp(read_buffer,"show help",9))
    {
       display_client_clis();
    }
    else if (strncmp(read_buffer,"show client groups",22) == 0)
    {
       display_client_groups(client_info);
    }
    else if (strncmp(read_buffer,"enable keepalive group ",23) == 0)
    {
       startKeepAlive(read_buffer+23);
    }
    else if (strncmp(read_buffer,"disable keepalive",18) == 0)
    {
       stopKeepAlive();
    }
    else if (strncmp(read_buffer,"join group ",11) == 0)
    {
       send_join_group_req(client_info, read_buffer+11);
    }
    else if (strncmp(read_buffer,"leave group ",12) == 0)
    {
       send_leave_group_req(client_info, read_buffer+12);
    }
    else if (0 == strcmp(read_buffer,"cls\0"))
    {
       system("clear");
    }
    else
    {
      /* If not a valid cli */
      if (cnt != 1 && read_buffer[0] != '\n')
        PRINT("Error: Unrecognized Command.\n");
    }
}

#ifndef TEST_CLIENT_C

/* <doc>
 * int main(int argc, char * argv[])
 * Main function of Client. Socket connection is created and 
 * registered with the epoll. Whenever any event comes, appropriate
 * action is taken. Events can be of 3 types -
 * 1. Event on client socket
 * 2. Cli command Event
 * 3. Event meant for client listening on multicast socket
 * 
 * </doc>
 */
int main(int argc, char * argv[])
{
    char group_name[50];
    struct addrinfo hints;
    int count = 0;

    active_group.count = 0;
 
    int event_count, index; 
    int status;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];
    char *addr,*port;

    client_information_t *client_info = NULL;

    /* Allocates client_info */
    allocate_client_info(&client_info);

    if (argc != 4)
    {
      PRINT("Usage: %s <client_IP> <client_port> <group_name>\n", argv[0]);
//      exit(1);
//      Temporary code
      argv[1] = "127.0.0.1";
      argv[2] = "3490";
      argv[3] = "G1";
    }

    addr = argv[1];
    port = argv[2];
    strcpy(group_name,argv[3]);

    /* Creating Client socket*/
    cfd = create_and_bind(addr, port, CLIENT_MODE);

    if (cfd == -1)
    {
        perror("\nError while creating and binding the socket.");
        exit(0);
    }

    /* Socket is made non-blocking */
    status = make_socket_non_blocking(cfd);

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    PRINT("..WELCOME TO CLIENT..");
    PRINT("\r   <Use \"show help\" to see all supported clis.>\n");

    PRINT_PROMPT("[client] ");

    /* epoll socket is created */
    efd = epoll_create(MAXEVENTS);

    /* Update client_info with client/epoll FD and epoll event structure */
    client_info->client_fd = cfd; 
    client_info->epoll_fd = efd;
    client_info->epoll_evt = &event;

    if (efd == -1)
    {
        perror("\nError while creating the epoll.");
        exit(0);
    }

    event.data.fd = cfd;
    event.events = EPOLLIN|EPOLLET;

    /* Register client FD with epoll*/
    status = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event);

    if ( status == -1)
    {
        perror("\nError while adding FD to epoll event.");
        exit(0);
    }

    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN|EPOLLET;

    /* Register STDIN with epoll */
    status = epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &event);

    if ( status == -1)
    {
        perror("\nError while adding STDIN FD to epoll event.");
        exit(0);
    }

    events = calloc(MAXEVENTS, sizeof(event));
  
    /* CLient takes program argument as group_name, creates join request
     * and joins the multiple groups */ 
    char * gname=strtok(group_name,",");
    char * gr_list[max_groups];
    comm_struct_t m; int iter = 0;

    m.id = join_request;

    while(gname!=NULL){ 
      gr_list[iter++] = gname;
      gname=strtok(NULL,",");
    }
    populate_join_req(&m, gr_list, iter);
    write_record(cfd, &m);

    while (TRUE) {
        /* Listening for epoll events*/
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for receiving data on Client Socket */
            if ((cfd == events[index].data.fd) && (events[index].events & EPOLLIN))
            {
                comm_struct_t req;
                fptr func;

                memset(&req, 0 ,sizeof(req));

                /* read_record reads data on events[index].data.fd and fills req struct.
                 * client_func_handler then returns function to handle the req depending
                 * upon req msg type. Once function handler name is received, it is invoked.
                 */
                if ((func = client_func_handler(read_record(events[index].data.fd, &req))))
                {
                    (func)(events[index].data.fd, req, client_info);
                }
                
            }
            /* Code Block for handling input from STDIN */
            else if (STDIN_FILENO == events[index].data.fd)
            {
                /* Invoking function to recognize the cli fired and call its appropriate handler */
                client_stdin_data(events[index].data.fd, client_info);

                PRINT_PROMPT("[client] ");
            }
            /* Data for multicast grp */
            else
            {
              mcast_client_node_t *client_node = NULL;
              char message[512];

              client_node =     SN_LIST_MEMBER_HEAD(&((client_info)->client_list->client_node),                                                              
                                                   mcast_client_node_t,            
                                                   list_element);
              while (client_node)
              {
                if(client_node->mcast_fd == events[index].data.fd)
                {
                  read(events[index].data.fd, message, sizeof(message));
                  PRINT("This Message is intended for Group %s: %s",client_node->group_name, message);
                  PRINT_PROMPT("[client] ");
                }
                client_node =     SN_LIST_MEMBER_NEXT(client_node,                      
                                                      mcast_client_node_t,         
                                                      list_element);
              }
            }
        }
    }

    /* close the client socket*/    
    close(cfd);
    
    return 0;
}
#endif
