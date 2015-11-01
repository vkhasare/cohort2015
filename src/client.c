#include "common.h"
#include "client_DS.h"
#include <pthread.h>

extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination

struct keep_alive{
    unsigned count;
    char group_name[10][10];
};
struct keep_alive active_group;

static int send_echo_request(const int sockfd, struct sockaddr *,char *);
static int handle_echo_req(const int, pdu_t *pdu, ...);
static int handle_echo_response(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_response(const int, pdu_t *pdu, ...);
static int handle_join_response(const int, pdu_t *pdu, ...);
static int handle_mod_notification(const int, pdu_t *pdu, ...);
static int handle_task_response(const int, pdu_t *pdu, ...);
fptr client_func_handler(unsigned int);
static void send_join_group_req(client_information_t *, char *);
static void send_leave_group_req(client_information_t *, char *);
static int handle_perform_task_req(const int, pdu_t *pdu, ...);
void* find_prime_numbers(void *args);
static void send_task_results_to_moderator(client_information_t *, char*, unsigned int,rsp_type_t, result_t *,unsigned int);

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
    case echo_req:
        func_name = handle_echo_req;
        break;
    case echo_response:
        func_name = handle_echo_response;
        break;
    case TASK_RESPONSE:
        func_name = handle_task_response;
        break;
    case moderator_notify_req:
        func_name = handle_mod_notification;
        break;
    case perform_task_req:
        func_name = handle_perform_task_req;
        break;
    default:
        PRINT("Invalid msg type of type - %d.", msgType);
        func_name = NULL;
  }

  return func_name;
}

/* <doc>
 * static
 * int handle_mod_notification(const int sockfd, pdu_t *pdu, ...)
 * This functions receives message from Server, regarding the
 * information of moderator. It then informs its presence to moderator.
 *
 * </doc>
 */
static
int handle_mod_notification(const int sockfd, pdu_t *pdu, ...)
{
    client_information_t *client_info = NULL;
    struct sockaddr_in mod;
    char ipaddr[INET6_ADDRSTRLEN];
    moderator_information_t *mod_info = NULL;

    comm_struct_t *rsp = &(pdu->msg);
    moderator_notify_req_t mod_notify_req = rsp->idv.moderator_notify_req;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    /*Storing moderator IP in client info*/
    mod.sin_family = AF_INET;
    mod.sin_port = mod_notify_req.moderator_port;
    mod.sin_addr.s_addr = mod_notify_req.moderator_id;

    memcpy(&client_info->moderator, (struct sockaddr *) &mod, sizeof(client_info->moderator));

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(mod)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Req: GRP - %s] Moderator IP is %s", mod_notify_req.group_name, ipaddr);

    /*After moderator information, inform moderator about the self presence by sending echo request msg.*/
    if (client_info->client_id != mod_notify_req.moderator_id) {
       send_echo_request(client_info->client_fd, &client_info->moderator, mod_notify_req.group_name);
    } else {
       /*IT IS THE MODERATOR BLOCK.. UPDATE MODERATOR DATA STRUCTURES*/
       PRINT("THIS CLIENT IS SELECTED AS MODERATOR FOR GROUP %s", mod_notify_req.group_name);
       /*Allocate the moderator info*/
       allocate_moderator_info(&client_info);
       /*mark client node as moderator*/
       client_info->is_moderator = TRUE;

       mod_info = (client_info)->moderator_info;

       strcpy(mod_info->group_name, mod_notify_req.group_name);
       /*Number of clients in group, to be monitored by moderator*/
       mod_info->active_client_count = mod_notify_req.num_of_clients_in_grp;

       /**Register the moderator fsm handler and set the fsm state*/
       mod_info->fsm = moderator_main_fsm;
       mod_info->fsm_state = MODERATOR_NOTIFY_RSP_PENDING;

       /*allocate the client node for pending list, to add the moderator node into it.*/
       mod_client_node_t *mod_node = (mod_client_node_t *) allocate_clnt_moderator_node(&client_info->moderator_info);

       struct sockaddr moderatorIP;
       get_my_ip("eth0", &moderatorIP);

       mod_node->peer_client_id = client_info->client_id;
       mod_node->peer_client_addr.sa_family = moderatorIP.sa_family;
       strcpy(mod_node->peer_client_addr.sa_data, moderatorIP.sa_data);

       /*If only 1 client in the group, then send moderator notify response for moderator right away.*/
       if (1 == mod_info->active_client_count) {
          send_moderator_notify_response(client_info);
       } else {
          /* Start the timer on Moderator to get echo requests from all its peer clients
           * 
           *
           *
           * On Time out or receiving all 
           */
       }
    }
}

/* <doc>
 * int handle_leave_response(const int sockfd, const comm_struct_t const resp, ...)
 * Group Leave Response handler. If cause in Response PDU is ACCEPTED, then leaves 
 * the multicast group. Else ignores for other causes.
 *  
 * </doc>
 */
static
int handle_leave_response(const int sockfd, pdu_t *pdu, ...)
{
        uint8_t cl_iter;
        msg_cause enum_cause = REJECTED;
        char *group_name;
        client_grp_node_t *client_grp_node = NULL;
        client_information_t *client_info = NULL;

        comm_struct_t *resp = &(pdu->msg);
        /* Extracting client_info from variadic args*/
        EXTRACT_ARG(pdu, client_information_t*, client_info);

        leave_rsp_t leave_rsp = resp->idv.leave_rsp;

        for(cl_iter = 0; cl_iter < leave_rsp.num_groups; cl_iter++){
            group_name = leave_rsp.group_ids[cl_iter].str;
            enum_cause = leave_rsp.cause;

            PRINT("[Leave_Response: GRP - %s] Cause : %s.", group_name, enum_to_str(enum_cause));

            /* if cause other than ACCEPTED, ignore the response */
            if (enum_cause == ACCEPTED)
            {
                get_client_grp_node_by_group_name(&client_info, group_name, &client_grp_node);

                /* Leave the multicast group if response cause is Accepted */
                if (TRUE == multicast_leave(client_grp_node->mcast_fd, client_grp_node->group_addr)) {
                    PRINT("Client has left multicast group %s.", group_name);
                } else {
                    PRINT("[Error] Error in leaving multicast group %s.", group_name);
                }

                /* Removing group association from list */
                deallocate_client_grp_node(client_info, client_grp_node);
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
int handle_join_response(const int sockfd, pdu_t *pdu, ...)
{
    struct sockaddr_in group_ip;
    int iter;
    char* group_name;
    unsigned int m_port;
    client_grp_node_t node;
    int status;
    struct epoll_event *event;
    msg_cause enum_cause;

    comm_struct_t *resp = &(pdu->msg);

    client_information_t *client_info = NULL;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    join_rsp_t join_response = resp->idv.join_rsp;

    /* Pointer to epoll event structure */
    event = client_info->epoll_evt;
 
    for(iter = 0; iter < join_response.num_groups; iter++){
        /*Reading the join response pdu*/
        enum_cause = join_response.group_ips[iter].cause;
        group_name = join_response.group_ips[iter].group_name;

        if (enum_cause == REJECTED)
        {
            PRINT("[Join_Response: GRP - %s] Cause : %s. (Reason : Non-existent Group)", group_name, enum_to_str(enum_cause));
            continue;
        }

        /*If join response has case as ACCEPTED*/
        PRINT("[Join_Response: GRP - %s] Cause : %s.", group_name, enum_to_str(enum_cause));

        group_ip.sin_family = join_response.group_ips[iter].sin_family;
        group_ip.sin_port = join_response.group_ips[iter].sin_port;
        group_ip.sin_addr.s_addr = join_response.group_ips[iter].s_addr;
        memset(&group_ip.sin_zero,0,sizeof(group_ip.sin_zero));

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
          pdu_t pdu;
          comm_struct_t *req = &(pdu.msg);

          req->id = join_request;
          /* Sending join request for 1 group*/
          populate_join_req(req, &group_name, 1);
          write_record(client_info->client_fd, &client_info->server, &pdu);

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
           pdu_t pdu;
           comm_struct_t *req = &(pdu.msg);

           req->id = leave_request;
           /* Sending leave request for 1 group*/
           populate_leave_req(req, &group_name, 1);
           write_record(client_info->client_fd, &client_info->server, &pdu);

           PRINT("[Leave_Request: GRP - %s] Leave Group Request sent to Server.", group_name);
       }
       else
       {
           /* client is not member of request group */
           PRINT("Error: Client is not member of group %s.", group_name);
       }

}

/* <doc>
 * void send_moderator_notify_response(client_information_t *client_info)
 * After fetching all information about the clients in the group,
 * send the the information about active group clients to server.
 *
 * </doc>
 */
void send_moderator_notify_response(client_information_t *client_info)
{
    pdu_t pdu;
    moderator_information_t *mod_info = (client_info)->moderator_info;
    comm_struct_t *rsp = &(pdu.msg);
    char ipaddr[INET6_ADDRSTRLEN];
    int iter = 0;

    rsp->id = moderator_notify_rsp;
    moderator_notify_rsp_t *moderator_notify_rsp = &(rsp->idv.moderator_notify_rsp);

    /*Filling group name and moderator id*/
    moderator_notify_rsp->group_name = mod_info->group_name;
    moderator_notify_rsp->moderator_id = client_info->client_id;    

    moderator_notify_rsp->client_id_count = mod_info->active_client_count;

    /*allocate memory for client IDs*/
    moderator_notify_rsp->client_ids = MALLOC_UARRAY_IE(mod_info->active_client_count);

    moderator_notify_rsp->client_ids[iter] = (unsigned int *) malloc(sizeof(unsigned int) * mod_info->active_client_count);

    mod_client_node_t *mod_node = NULL;

    mod_node =     SN_LIST_MEMBER_HEAD(&(mod_info->pending_client_list->client_grp_node),
                                         mod_client_node_t,
                                         list_element);

    while (mod_node)
    {
      /*Fill client ID of all the peer clients*/
      moderator_notify_rsp->client_ids[iter] = mod_node->peer_client_id;

      mod_node   =   SN_LIST_MEMBER_NEXT(mod_node,
                                         mod_client_node_t,
                                         list_element);
      iter++;
    }

    write_record(client_info->client_fd, &client_info->server, &pdu);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(client_info->server)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Rsp: GRP - %s] Moderator Notify Response sent to server %s", moderator_notify_rsp->group_name, ipaddr);

}

/* <doc>
 * void moderator_echo_req_notify_rsp_pending_state(client_information_t *client_info,
 *                                                  void *fsm_msg)
 * When moderator has not sent moderator notification response to server and 
 * receives echo req from its peer clients, then it should store the information
 * about all the clients.
 *
 * </doc>
 */
void moderator_echo_req_notify_rsp_pending_state(client_information_t *client_info,
                                                 void *fsm_msg)
{
  fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
  pdu_t *pdu = (pdu_t *) fsm_data->pdu;
  comm_struct_t *req = &(pdu->msg);
  echo_req_t echo_req = req->idv.echo_req;
  static int client_echo_req_rcvd = 0;

  /*Increment the counter as request from client is received*/
  client_echo_req_rcvd = client_echo_req_rcvd + 1;

  /*allocate the moderator client node and store the information about the group active clients*/
  mod_client_node_t *mod_node = (mod_client_node_t *) allocate_clnt_moderator_node(&client_info->moderator_info);

  mod_node->peer_client_id = calc_key(&pdu->peer_addr); 
  mod_node->peer_client_addr.sa_family = pdu->peer_addr.sa_family;
  strcpy(mod_node->peer_client_addr.sa_data, pdu->peer_addr.sa_data);

  /* Send notification response to server if all the clients have responded with echo req or TIMEOUT happened
   * This is the case where all clients have responded.*/
  if (client_echo_req_rcvd == (client_info->moderator_info->active_client_count) - 1) {
     send_moderator_notify_response(client_info);
     client_info->moderator_info->fsm_state = MODERATOR_NOTIFY_RSP_SENT; 
  }
}

/* <doc>
 * void moderator_echo_req_task_rsp_pending_state(client_information_t *client_info,
 *                                               void *fsm_msg)
 * Function will get hit when all clients are working on the task and client is in
 * task response pending state to server and echo request from client comes.
 * All periodic echo's will hit this function.
 *
 * </doc>
 */
void moderator_echo_req_task_rsp_pending_state(client_information_t *client_info,
                                               void *fsm_msg)
{
  fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
  pdu_t *pdu = (pdu_t *) fsm_data->pdu;
  comm_struct_t *req = &(pdu->msg);
  echo_req_t echo_req = req->idv.echo_req;


//commented for now.
//  check_keepalive_working_clients(&client_info);
//UPDATE THE DS HERE
}


/* <doc>
 * static
 * int handle_echo_response(const int sockfd, pdu_t *pdu, ...)
 * Handles the echo response from peer.
 *
 * </doc>
 */
static
int handle_echo_response(const int sockfd, pdu_t *pdu, ...){
    char ipaddr[INET6_ADDRSTRLEN];
    comm_struct_t *rsp = &(pdu->msg);
    echo_rsp_t *echo_rsp = &(rsp->idv.echo_resp);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Response: GRP - %s] Echo Response received from %s", echo_rsp->group_name, ipaddr);

    return 0;
}

/* <doc>
 * static
 * int handle_echo_req(const int sockfd, pdu_t *pdu, ...)
 * Handles the echo request msg from peer node and
 * responds back.
 *
 * </doc>
 */
static
int handle_echo_req(const int sockfd, pdu_t *pdu, ...){

    comm_struct_t *req = &(pdu->msg);
    pdu_t rsp_pdu;
    comm_struct_t *rsp = &(rsp_pdu.msg);
    char ipaddr[INET6_ADDRSTRLEN];
    client_information_t *client_info = NULL;
    echo_req_t echo_req = req->idv.echo_req;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Request: GRP - %s] Echo Request received from %s", echo_req.group_name, ipaddr);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    rsp->id = echo_response;
    echo_rsp_t *echo_response = &(rsp->idv.echo_resp);

    /*filling echo rsp pdu*/
    echo_response->status    = client_info->client_status;
    echo_response->group_name = echo_req.group_name;
    
    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Response: GRP - %s] Echo Response sent to %s", echo_response->group_name, ipaddr);

    /*If moderator, then run the moderator fsm*/
    if (client_info->moderator_info) {
        fsm_data_t fsm_msg;
        fsm_msg.fsm_state = client_info->moderator_info->fsm_state;
        fsm_msg.pdu = pdu;

        /*run the fsm*/
        client_info->moderator_info->fsm(client_info, MOD_ECHO_REQ_RCVD_EVENT, &fsm_msg);
    }

    return 0;
}

/* <doc>
 * static
 * int send_echo_request(unsigned int sockfd, struct sockaddr *addr, char *grp_name)
 * Sends echo request on mentioned sockfd to the addr passed.
 * grp_name is filled in echo req pdu.
 *
 * </doc>
 */
static
int send_echo_request(const int sockfd, struct sockaddr *addr, char *grp_name)
{
  pdu_t pdu;
  char ipaddr[INET6_ADDRSTRLEN];
  comm_struct_t *req = &(pdu.msg);
  echo_req_t *echo_request = &(req->idv.echo_req);

  req->id = echo_req;

  /*Creating the echo request pdu*/
  echo_request->group_name = grp_name;
  write_record(sockfd, addr, &pdu);

  inet_ntop(AF_INET, get_in_addr(addr), ipaddr, INET6_ADDRSTRLEN);
  PRINT("[Echo_Request: GRP - %s] Echo Request sent to %s", echo_request->group_name, ipaddr);

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
//        myaction.sa_handler = send_echo_req;
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
   PRINT("show pending clients                         --  Moderator CLI. Shows list of working clients");
   PRINT("show done clients                            --  Moderator CLI. Shows list of done clients");
   PRINT("test echo                                    --  Echo debug CLI");
}

/* <doc>
 * void display_client_groups(client_information_t *client_info)
 * Prints list of all groups which client has joined.
 * 
 * </doc>
 */
void display_client_groups(client_information_t *client_info)
{
  display_client_grp_node(&client_info);
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
    else if (strncmp(read_buffer,"test echo",9) == 0)
    {
        send_echo_request(client_info->client_fd, &client_info->server, "G2");
    }
    else if(strncmp(read_buffer,"show pending clients",19) == 0)
    {
        display_moderator_pending_list(&client_info, SHOW_MOD_PENDING_CLIENTS);
    }
    else if(strncmp(read_buffer,"show done clients",13) == 0)
    {
        display_moderator_pending_list(&client_info, SHOW_MOD_DONE_CLIENTS);
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
    int cfd, efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char ipStr[INET6_ADDRSTRLEN];
    char *port, *serverAddr;
    struct sockaddr myIp;

    client_information_t *client_info = NULL;

    /* Allocates client_info */
    allocate_client_info(&client_info);

    /* Binding to any random user space port for client.*/
    port = htons(0); 

    /*Fetching eth0 IP for client*/
    get_my_ip("eth0", &myIp);
    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(myIp)), ipStr, INET6_ADDRSTRLEN);

    client_info->client_id = calc_key(&myIp);

    /* Creating Client socket*/    
    cfd = create_and_bind(ipStr, port, CLIENT_MODE);

    if (cfd == -1)
    {
        perror("\nError while creating and binding the socket.");
        exit(0);
    }

    /* Socket is made non-blocking */
    status = make_socket_non_blocking(cfd);


    if (argc != 4)
    {
      PRINT("Usage: %s <server_IP> <server_port> <group_name>\n", argv[0]);
      /* server_IP/server_port not provided. Attempting talk with server on same machine
       * with default port. */
      argv[1] = ipStr;
      argv[2] = "3490";
      argv[3] = "G1";
    }
    serverAddr = argv[1];
    port = argv[2];
    strcpy(group_name,argv[3]);

    /* Creating sockaddr_in struct for Server entry and keeping it in client_info */
    client_info->server.sin_family = AF_INET;
    client_info->server.sin_port = htons(atoi(port));
    client_info->server.sin_addr.s_addr = inet_addr(serverAddr); 

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    PRINT("..WELCOME TO CLIENT (%s)..", ipStr);
    PRINT("\r   <Use \"show help\" to see all supported clis.>\n");

    PRINT_PROMPT("[client] ");

    /* epoll socket is created */
    efd = epoll_create(MAXEVENTS);

    /* Update client_info with client/epoll FD and epoll event structure */
    client_info->client_status = FREE;
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
    int iter = 0;

    //m.id = join_request;

    while(gname!=NULL){ 
      gr_list[iter++] = gname;
      gname=strtok(NULL,",");
    }

    pdu_t pdu;
    populate_join_req(&(pdu.msg), gr_list, iter);
    write_record(client_info->client_fd, &client_info->server, &pdu);

    while (TRUE) {
        /* Listening for epoll events*/
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for receiving data on Client Socket */
            if ((cfd == events[index].data.fd) && (events[index].events & EPOLLIN))
            {
                fptr func;

                /* read_record reads data on events[index].data.fd and fills req struct.
                 * client_func_handler then returns function to handle the req depending
                 * upon req msg type. Once function handler name is received, it is invoked.
                 */
                if ((func = client_func_handler(read_record(events[index].data.fd, &pdu))))
                {
                    (func)(events[index].data.fd, &pdu, client_info);
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
                fptr func;
                if ((func = client_func_handler(read_record(events[index].data.fd, &pdu))))
                {
                    (func)(events[index].data.fd, &pdu, client_info);
                }
/*
              client_grp_node_t *client_grp_node = NULL;
              char message[512];

              client_grp_node =     SN_LIST_MEMBER_HEAD(&((client_info)->client_grp_list->client_grp_node),                                                              
                                                   client_grp_node_t,            
                                                   list_element);
              while (client_grp_node)
              {
                if(client_grp_node->mcast_fd == events[index].data.fd)
                {
                  read(events[index].data.fd, message, sizeof(message));
                  PRINT("This Message is intended for Group %s: %s",client_grp_node->group_name, message);
                  PRINT_PROMPT("[client] ");
                }
                client_grp_node =     SN_LIST_MEMBER_NEXT(client_grp_node,                      
                                                      client_grp_node_t,         
                                                      list_element);
              }
*/
            }
        }
    }

    /* close the client socket*/    
    close(cfd);
    
    return 0;
}
#endif

/* <doc>
 * static void send_task_results_to_moderator(client_information_t *client_info, rsp_type_t, result_t *)
 * This function creates Group Task Response message and sends to moderator. It accepts
 * results as input and relays message on client fd.
 * Request will not be relayed if client is not member of group.
 *
 * </doc>
 */
static void send_task_results_to_moderator(client_information_t *client_info, char* group_name, unsigned int task_id,  rsp_type_t rtype, result_t *result, unsigned int my_id)
{

     pdu_t pdu;
     /* Sending task response for 1 group*/
     populate_task_rsp(&(pdu.msg),task_id, group_name, rtype ,result, my_id);
     write_record(client_info->client_fd, &(client_info->moderator) , &pdu);
     // PRINT("[Task_Response: GRP - %s] Task Response sent to Moderator. ", group_name);
}

/*#define MAX_CLIENT 10
void * populate_moderator_task_rsp( uint8_t num_clients, task_rsp_t *resp, unsigned int client_id ){
    pdu_t *rsp_pdu= malloc(sizeof(pdu_t)); 
    comm_struct_t* m = &(rsp_pdu->msg);
    memset(&(m->idv),0,sizeof(task_rsp_t));
    m->id=TASK_RESPONSE;
    task_rsp_t * task_rsp = &(m->idv.task_rsp);
    task_rsp->type = resp->type;
    task_rsp->group_name = malloc(sizeof(char)*strlen(resp->group_name));
    strcpy(task_rsp->group_name, resp->group_name); 
    task_rsp->result = (result_t **)malloc(sizeof(result_t*)*num_clients);
    task_rsp->client_ids = (unsigned int *)malloc(sizeof(unsigned int *)*num_clients);
    update_task_rsp(m, task_rsp->type, resp->result, client_id);
    return rsp_pdu;
}*/

unsigned long int conv(char ipadr[])
{
    unsigned long int num=0,val;
    char *tok,*ptr;
    tok=strtok(ipadr,".");
    while( tok != NULL)
    {
        val=strtoul(tok,&ptr,0);
        num=(num << 8) + val;
        tok=strtok(NULL,".");
    }
    return(num);
}

/* <doc>
 * int handle_task_response(const int sockfd, const comm_struct_t const resp, ...)
 * Join Group Response Handler. Reads the join response PDU, and fetches the
 * IP for required multicast group. Joins the multicast group.
 *
 * </doc>
 */
#define TOTAL_COUNT 9
static
int handle_task_response(const int sockfd, pdu_t *pdu, ...)
{
 
    struct sockaddr_in group_ip;
    int iter;
    char* group_name;
    unsigned int m_port;
    client_grp_node_t node;
    int status;

    comm_struct_t *resp = &(pdu->msg);

    client_information_t *client_info = NULL;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    task_rsp_t * task_response = &resp->idv.task_rsp;
    moderator_information_t * moderator_info = client_info->moderator_info;
    if(moderator_info != NULL && MATCH_STRING(moderator_info->group_name, task_response->group_name)){ 
    unsigned int peer_id = calc_key(&(pdu->peer_addr));
    
     
    if(moderator_info->moderator_resp_msg != NULL){
      update_task_rsp(&((pdu_t *)moderator_info->moderator_resp_msg)->msg, task_response->type, task_response->result, peer_id);
    } else{
      moderator_info->moderator_resp_msg = populate_moderator_task_rsp(moderator_info->active_client_count, task_response, peer_id);
    }
//    move_moderator_node_pending_to_done_list(
    pdu_t * rsp_pdu = (pdu_t *)moderator_info->moderator_resp_msg;
    if(rsp_pdu->msg.idv.task_rsp.num_clients == moderator_info->active_client_count){
      write_record(client_info->client_fd, &(client_info->server) ,rsp_pdu);
    }
    } else{
      PRINT("Error case : Moderator received a wrong input");
    }
}

result_t* copy_result_from_args(thread_args * args){
   result_t * result=NULL;
   int i;
      result = malloc(sizeof(result_t));
      result->value=NULL; 
      result->size=0;
  if(args->result_count>0){
      result->size=args->result_count;
      result->value=malloc(sizeof(int)*result->size);
      for(i=0;i<result->size;i++){
        result->value[i]=args->result[i];
      }
   }
   return result;
}


void send_task_results(thread_args *args){
        
        result_t *answer = copy_result_from_args(args); 
        if(answer !=NULL) 
          send_task_results_to_moderator(args->client_info,args->group_name, args->task_id, TYPE_INT, answer, args->client_info->client_id); 
}

/* <doc>
 * void* find_prime_numbers(void *args)
 * Read's the sub set of data and prints prime numbers
 *
 * </doc>
 */
void* find_prime_numbers(void *args)
{
  unsigned int i,j,flag;

  thread_args *t_args = (thread_args *)args;
  
  /* Loop for prime number's in given data set */
  for(i = 0; i < t_args->data_count; i++)
  {
    flag = 0;
    for (j = 2; j < t_args->data[i]; j++)
    {
      if ((t_args->data[i] % j) == 0)
      {
        flag = 1;
        break;
      }
    }
    if(!flag)
    {
      t_args->result[t_args->result_count] = t_args->data[i];
      //PRINT("Prime number %d",t_args->result[t_args->result_count]);
      
      /* Total count of prime numbers */
      t_args->result_count++;
    }
  }
  if(t_args->result_count == 0)
    PRINT("No prime numbers found..");

  send_task_results(args);

  return NULL;
}

/* <doc>
 * static
 * int handle_perform_task_req(const int sockfd, pdu_t *pdu, ...)
 * Perform task Request Handler. Reads the PDU and spawns a thread to perform the task
 *
 * </doc>
 */
static
int handle_perform_task_req(const int sockfd, pdu_t *pdu, ...)
{
    comm_struct_t *req = &(pdu->msg);
    struct sockaddr myIp;
    int count = 0, i = 0, result;
    unsigned int client_task_set[MAX_TASK_COUNT] = {0}, clientID;
    unsigned int start_index = 0, stop_index = 0, client_task_count = 0;
    pthread_t thread;

    get_my_ip("eth0", &myIp);
    clientID = calc_key(&myIp);
    
    perform_task_req_t perform_task = req->idv.perform_task_req;
    thread_args *args = malloc(sizeof(thread_args));
    client_information_t * client_info; 
    PRINT("[Task Set Received]");
    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    for(count = 0; count < perform_task.client_id_count; count++)
    {
      if(clientID == perform_task.client_ids[count]) /*This client needs to perform the task */
      {
        /* assumption client_id_count < num of task count 
         * start_index : the index of the original task set from where this clients starts performing task
         * stop_index : the index of the original task set to which this client perform's the task
         */
        start_index = ((perform_task.task_count/perform_task.client_id_count) * count);
        /* client_task_count is the count of total number's which client has to work upon */
        client_task_count = (perform_task.task_count/perform_task.client_id_count);

        if((count +1) == perform_task.client_id_count) /* last client */
        {
          /* Consider rest all elements of task set */
          stop_index = perform_task.task_count - 1;
          client_task_count = stop_index - start_index + 1;
        }
        else
          stop_index = start_index + client_task_count;

        while(i != client_task_count)
        {
            /* Derive the task set for client */
            client_task_set[i] = perform_task.task_set[(start_index + i)];
            args->data[i] = client_task_set[i];
            i++;
        }

        args->data_count = client_task_count;

        args->client_info=client_info;
        args->task_id = perform_task.task_id;
        args->group_name=malloc(sizeof(char)*strlen(perform_task.group_name));
        strcpy(args->group_name, perform_task.group_name);
        /* Create a thread to perform the task */
        result = pthread_create(&thread, NULL, find_prime_numbers ,args);
        
        if(result)
        {
          PRINT("Could not create thread to perform task");
        }
        pthread_join(thread,NULL);
//        result_t *answer = copy_result_from_args(args ); 
//        if(answer !=NULL) 
//          send_task_results_to_moderator(client_info, TYPE_INT, result, clientID); 
      }
   }
}

