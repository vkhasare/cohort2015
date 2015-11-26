#include "common.h"
#include "client_DS.h"
#include <pthread.h>

const int MAX_ALLOWED_KA_MISSES = 5;

extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
extern debug_mode;
static int echo_req_rcvd_in_notify_rsp_pending = 0;   /*Counter for number of clients who
                                                        have responded with echo req when moderator
                                                        is in moderator notify rsp state*/
struct clnt_notify_alive_list_struct{
  unsigned int client_rsp_list[255];
  int client_rsp_cntr;
};
struct clnt_notify_alive_list_struct clnt_notify_alive_list;

fptr client_func_handler(unsigned int);
static int handle_echo_req(const int, pdu_t *pdu, ...);
static int handle_echo_response(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_response(const int, pdu_t *pdu, ...);
static int handle_join_response(const int, pdu_t *pdu, ...);
static int handle_mod_notification(const int, pdu_t *pdu, ...);
static int handle_moderator_update(const int, pdu_t *pdu, ...);
static int handle_task_response(const int, pdu_t *pdu, ...);
static void send_join_group_req(client_information_t *, char *);
static void send_leave_group_req(client_information_t *, char *);
static void send_moderator_notify_response(client_information_t *client_info);
static int handle_perform_task_req(const int, pdu_t *pdu, ...);
static void send_task_results_to_moderator(client_information_t *, char*, unsigned int,rsp_type_t, result_t *,unsigned int);
void moderator_send_task_response_to_server(client_information_t *);
void* find_prime_numbers(void *args);

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
    case task_response:
        func_name = handle_task_response;
        break;
    case moderator_notify_req:
        func_name = handle_mod_notification;
        break;
    case moderator_update_req:
        func_name = handle_moderator_update;
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
 * void moderator_task_rsp_pending_timeout(client_information_t *client_info_local)
 * Handler when timeout happens on moderator when in task response pending state. 
 * Initiate task response towards server if required.
 *
 * </doc>
 */
void moderator_task_rsp_pending_timeout(client_information_t *client_info_local)
{
    /* Purpose of this block is to piggyback information about dead
     * clients detected on the moderator's keepalive messages towards server. */
     unsigned int client_ids[254], *id_arr, iter =0;
     mod_client_node_t *mod_node = NULL;
     mod_node = SN_LIST_MEMBER_HEAD(&(client_info_local->moderator_info->pending_client_list->client_grp_node),
                                      mod_client_node_t,
                                      list_element);

     while (mod_node){
        /*If all heartbeats are expired, then client is definitely dead.*/
        if(--mod_node->heartbeat_remaining == 0){
             client_ids[iter] = mod_node->peer_client_id;
             iter++;
             client_info_local->moderator_info->active_client_count--;

             /*Remove this client from pending list*/
             SN_LIST_MEMBER_REMOVE(&(client_info_local->moderator_info->pending_client_list->client_grp_node),
                                     mod_node,
                                     list_element);


             char ipaddr[INET6_ADDRSTRLEN];
             inet_ntop(AF_INET, get_in_addr(&mod_node->peer_client_addr), ipaddr, INET6_ADDRSTRLEN);
             PRINT("[WARNING] Client %s is down.", ipaddr);

             /*Send to Server if all clients, except this have responded with their task response*/
             moderator_send_task_response_to_server(client_info_local);
         }
         mod_node = SN_LIST_MEMBER_NEXT(mod_node, mod_client_node_t, list_element);
     }

     if(iter){
        id_arr = (unsigned int *) malloc(sizeof(unsigned int) * iter);
        memcpy(id_arr, client_ids, sizeof(unsigned int) * iter);
     }

     pdu_t pdu;
     pdu.msg.id = echo_req;
     echo_req_t * echo_request = &pdu.msg.idv.echo_req;
     initialize_echo_request(echo_request);

     echo_request->group_name  = client_info_local->active_group->group_name;
     echo_request->num_clients = iter;
     echo_request->client_ids  = iter ? id_arr : NULL;

     write_record(client_info_local->client_fd, (struct sockaddr *)&(client_info_local->server), &pdu);
}

/* <doc>
 * void handle_timeout_real(bool init, int signal, siginfo_t *si,
 *                          client_information_t *client_info)
 * This function manages keepalives in context of moderator-server and client-moderator link.
 * From client perspective there are 2 types of timers to be maintained. One
 * timer is when client is acting as moderator and needs to maintain/monitor
 * status of clients in the multicast group. Second type of timer is the one
 * maintained for sending periodic updates to moderators of the groups a client
 * is working for/is member of. 
 * 
 * Special case is moderator is the only node working in a multicast group. In
 * such a scenario, monitoring function is non existent and case falls back to
 * base case of mod-client keepalive. Timer member in DS's have been overloaded
 * keeping this in mind.
 *
 * </doc>
 */
void handle_timeout_real(bool init, int signal, siginfo_t *si,
                         client_information_t *client_info)
{
    static client_information_t *client_info_local;
    
    if(init)
    {
        /* Context restoration is possible in very limited context with signals. In order
         * to step around this problem we are calling this routine in two stages. In first
         * leg (init), we push/preserve address of DS that serves as lifeline throughout life
         * of application. In subsequent timer hits, where this info will be unavailable,
         * we use context stored at init time to perform the task. */
        
        client_info_local = client_info;
        return;
    }
    else
    {
        /*traverse the list to find which group timer expired*/
        
        /*Validation check for moderator_info*/
        if (client_info_local->moderator_info == NULL)
          return;
 
        if(signal == MODERATOR_TIMEOUT)
        {
            /* If moderator is in mod notify rsp pending state and timeout happens on moderator
             * for receiving the echo's from all clients. */
            if (client_info_local->moderator_info->fsm_state == MODERATOR_NOTIFY_RSP_PENDING) {
              send_moderator_notify_response(client_info_local);
            }
            /* If moderator is in mod task rsp pending state, and timeout happens on moderator,
             * then echo towards server is initiated with dead client list*/
            else if (client_info_local->moderator_info->fsm_state == MODERATOR_TASK_RSP_PENDING) {
              moderator_task_rsp_pending_timeout(client_info_local);
            } else {
              /*If moderator fsm state is other than MODERATOR_NOTIFY_RSP_PENDING or MODERATOR_TASK_RSP_PENDING,
               *then assert. */
//              assert(0);
              /*add logging warning mentioning fsm state of moderator*/
            }
        }
        else if(signal == CLIENT_TIMEOUT)
        {
            /* At present written assuming that there is only one active group at
             * any given time. Group lookup would be needed if client is capable
             * of executing multiple tasks at a time.*/
            if(client_info_local->is_moderator){
                send_echo_request(client_info_local->client_fd, (struct sockaddr *)&client_info_local->server, 
                                  client_info_local->active_group->group_name);
            }
            else{
                send_echo_request(client_info_local->client_fd, &client_info_local->active_group->moderator, 
                                  client_info_local->active_group->group_name);
            }
        }
    }
    
}

void handle_timeout(int signal, siginfo_t *si, void *uc)
{
    if(si == NULL)
        PRINT("***yikes!*** Signal context info found to be NULL.");
    handle_timeout_real(false, signal, si, NULL);
}

/* <doc>
 * static
 * int handle_moderator_update(const int sockfd, pdu_t *pdu, ...)
 * This functions receives message from Server, regarding some
 * update of moderator. It then updates the all clients/moderator
 * regarding update sent by server.
 *
 * </doc>
 */
static
int handle_moderator_update(const int sockfd, pdu_t *pdu, ...)
{
    client_information_t *client_info = NULL;
    struct sockaddr_in mod;
    char ipaddr[INET6_ADDRSTRLEN];
    moderator_information_t *mod_info = NULL;
    int i = 0;
    char str[100] = {0};

    comm_struct_t *rsp = &(pdu->msg);
    moderator_update_req_t mod_update_req = rsp->idv.moderator_update_req;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    /*Storing moderator IP in client info*/
    mod.sin_family = AF_INET;
    mod.sin_port = mod_update_req.moderator_port;
    mod.sin_addr.s_addr = mod_update_req.moderator_id;

    get_client_grp_node_by_group_name(&client_info, mod_update_req.group_name, &client_info->active_group);
    memcpy(&client_info->active_group->moderator, (struct sockaddr *) &mod, sizeof(client_info->active_group->moderator));

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(mod)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Update_Req: GRP - %s] New Moderator IP is %s", mod_update_req.group_name, ipaddr);

    /*Start the recurring timer*/
    if (client_info->client_id != mod_update_req.moderator_id)
    {
        start_recurring_timer(&(client_info->active_group->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
    }
    else
    {
       /*IT IS THE MODERATOR BLOCK.. UPDATE MODERATOR DATA STRUCTURES*/
       PRINT("THIS CLIENT IS RE-SELECTED AS MODERATOR FOR GROUP %s", mod_update_req.group_name);

       /*Allocate the moderator info*/
       allocate_moderator_info(&client_info);
       /*mark client node as moderator*/
       client_info->is_moderator = TRUE;

       mod_info = (client_info)->moderator_info;

       strcpy(mod_info->group_name, mod_update_req.group_name);

       /*Number of clients in group, to be monitored by moderator*/
       mod_info->active_client_count = mod_update_req.client_id_count;

       /**Register the moderator fsm handler and set the fsm state*/
       mod_info->fsm = moderator_main_fsm;
       /* Since modertor reselection can happen only during task execution of task,
        * hence putting moderator in MODERATOR_TASK_RSP_PENDING state*/
       mod_info->fsm_state = MODERATOR_TASK_RSP_PENDING;

       while (i < mod_update_req.client_id_count) {
          /*allocate the client node for pending list, to add the moderator node into it.*/
          mod_client_node_t *mod_node = allocate_clnt_moderator_node(&(client_info->moderator_info));

          struct sockaddr_in ipAddr;
          char ipAddress[INET6_ADDRSTRLEN];

          mod_node->peer_client_id = mod_update_req.client_ids[i];
          ipAddr.sin_addr.s_addr = mod_update_req.client_ids[i];
          ipAddr.sin_family = AF_INET;
          ipAddr.sin_port = htons(atoi(PORT));
          inet_ntop(AF_INET, &(ipAddr.sin_addr), ipAddress, INET6_ADDRSTRLEN);
          sprintf(str, "%s , %s", str, ipAddress);
          memcpy(&mod_node->peer_client_addr, &ipAddr, sizeof(ipAddr));
          mod_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
          i++;
       }
      PRINT("Moderator is working with clients - %s", str);
    }
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

    get_client_grp_node_by_group_name(&client_info, mod_notify_req.group_name, &client_info->active_group);
    memcpy(&client_info->active_group->moderator, (struct sockaddr *) &mod, sizeof(client_info->active_group->moderator));

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(mod)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Req: GRP - %s] Moderator IP is %s", mod_notify_req.group_name, ipaddr);

    /*After moderator information, inform moderator about the self presence by sending echo request msg.*/
    if (client_info->client_id != mod_notify_req.moderator_id && client_info->client_status == FREE) 
    {
        send_echo_request(client_info->client_fd, &client_info->active_group->moderator, mod_notify_req.group_name);
        start_recurring_timer(&(client_info->active_group->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
    } 
    else 
    {
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

       /*Updating list of clients who have responded during moderator notify duration*/
       clnt_notify_alive_list.client_rsp_list[(clnt_notify_alive_list.client_rsp_cntr)++] = client_info->client_id;

       /*If only 1 client in the group, then send moderator notify response for moderator right away.*/
       if (1 == mod_info->active_client_count) 
       {
           send_moderator_notify_response(client_info);
           start_recurring_timer(&(client_info->moderator_info->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
       } 
       else 
       {
          /* Start the timer on Moderator to get echo requests from all its peer clients */
           start_oneshot_timer(&(client_info->moderator_info->timer_id), DEFAULT_TIMEOUT, MODERATOR_TIMEOUT);
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
                if (TRUE == multicast_leave(client_grp_node->mcast_fd, client_grp_node->group_addr)) 
                {
                    PRINT("Client has left multicast group %s.", group_name);
                } 
                else 
                {
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
 
    for(iter = 0; iter < join_response.num_groups; iter++)
    {
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
        
        if(mcast_fd <= 0) return;

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
       unsigned int capability = 1;

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
          
          capability = generate_random_capability();
          
          /* Sending join request for 1 group*/
          populate_join_req(req, &group_name, 1, capability);
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

       if (client_info->client_status == BUSY) {
          PRINT("[Error] Client cannot leave group %s as it is working on a task.", group_name);
          return;
       }

       /* If client is member of requested group, then only send leave request */
       if (IS_GROUP_IN_CLIENT_LL(&client_info, group_name)) {
           pdu_t pdu;
           comm_struct_t *req = &(pdu.msg);

           req->id = leave_request;
           /* Sending leave request for 1 group*/
           populate_leave_req(req, &group_name, 1);
           write_record(client_info->client_fd, &client_info->server, &pdu);

           PRINT("[Leave_Request: GRP - %s] Leave Group Request sent to Server.", group_name);
       } else {
           /* client is not member of request group */
           PRINT("Error: Client is not member of group %s.", group_name);
       }

}

/* <doc>
 * static void send_moderator_notify_response(client_information_t *client_info)
 * After fetching all information about the clients in the group,
 * send the the information about active group clients to server.
 *
 * </doc>
 */
static void send_moderator_notify_response(client_information_t *client_info)
{
    /*change fsm state as soon as we enter this function*/
    client_info->moderator_info->fsm_state = MODERATOR_NOTIFY_RSP_SENT;
    /*Delete the timer running for mod notify req timeout*/
    if (client_info->moderator_info->timer_id) {
      timer_delete(client_info->moderator_info->timer_id);
    }

    pdu_t pdu;
    moderator_information_t *mod_info = (client_info)->moderator_info;
    comm_struct_t *rsp = &(pdu.msg);
    char ipaddr[INET6_ADDRSTRLEN];

    echo_req_rcvd_in_notify_rsp_pending = 0;

    rsp->id = moderator_notify_rsp;
    moderator_notify_rsp_t *moderator_notify_rsp = &(rsp->idv.moderator_notify_rsp);

    /*Filling group name and moderator id*/
    moderator_notify_rsp->group_name = mod_info->group_name;
    moderator_notify_rsp->moderator_id = client_info->client_id;    

    /*remove this line in future if you dont see any issues in this code*/
    //moderator_notify_rsp->client_id_count = mod_info->active_client_count;

    /*allocate memory for client IDs*/
    moderator_notify_rsp->client_ids = (unsigned int *) malloc(sizeof(unsigned int) * mod_info->active_client_count);

    /*Copy active client's id list into mod notify rsp msg*/
    memcpy(moderator_notify_rsp->client_ids,
           clnt_notify_alive_list.client_rsp_list, 
           (sizeof(clnt_notify_alive_list.client_rsp_list[0]) * clnt_notify_alive_list.client_rsp_cntr));

    /*Filling actual alive clients count*/
    moderator_notify_rsp->client_id_count = clnt_notify_alive_list.client_rsp_cntr;

    write_record(client_info->client_fd, &client_info->server, &pdu);

    /*Reset variables related to mod notify response*/
    clnt_notify_alive_list.client_rsp_cntr = 0;

    /* Start timer for maintaining client's keepalive*/
    if (1 < mod_info->active_client_count)
      start_recurring_timer(&(mod_info->timer_id), DEFAULT_TIMEOUT, MODERATOR_TIMEOUT);

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

  /*Increment the counter as request from client is received*/
  echo_req_rcvd_in_notify_rsp_pending = echo_req_rcvd_in_notify_rsp_pending + 1;

  /*Updating list of clients who have responded during moderator notify duration*/
  clnt_notify_alive_list.client_rsp_list[clnt_notify_alive_list.client_rsp_cntr++] = calc_key(&pdu->peer_addr);

  /* Send notification response to server if all the clients have responded with echo req or TIMEOUT happened
   * This is the case where all clients have responded.*/
  if (echo_req_rcvd_in_notify_rsp_pending == (client_info->moderator_info->active_client_count) - 1) 
  {
     send_moderator_notify_response(client_info);
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
  mod_client_node_t *client_node = NULL;

  get_client_from_moderator_pending_list(client_info, calc_key(&pdu->peer_addr), &client_node);

  if (client_node) {
     client_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
  }
}


/* <doc>
 * static
 * int handle_echo_response(const int sockfd, pdu_t *pdu, ...)
 * Handles the echo response from peer.
 *
 * </doc>
 */
static
int handle_echo_response(const int sockfd, pdu_t *pdu, ...)
{
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
 * void display_client_clis()
 * Displays all available Cli's.
 *
 * </doc>
 */
void display_client_clis()
{
   PRINT("show client groups                           --  displays list of groups joined by client");
   PRINT("join group <name>                            --  Joins a new group");
   PRINT("leave group <name>                           --  Leaves a group");
   PRINT("cls                                          --  Clears the screen");
   PRINT("show pending clients                         --  Moderator CLI. Shows list of working clients");
   PRINT("show done clients                            --  Moderator CLI. Shows list of done clients");
   PRINT("enable debug                                 --  Enables the debug mode");
   PRINT("disable debug                                --  Disables the debug mode");
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
        send_echo_request(client_info->client_fd, (struct sockaddr *)&client_info->server, "G2");
    }
    else if(strncmp(read_buffer,"show pending clients",19) == 0)
    {
        display_moderator_list(&client_info, SHOW_MOD_PENDING_CLIENTS);
    }
    else if(strncmp(read_buffer,"show done clients",13) == 0)
    {
        display_moderator_list(&client_info, SHOW_MOD_DONE_CLIENTS);
    }
    else if(strncmp(read_buffer,"enable debug\0",12) == 0)
    {
      debug_mode = TRUE;
      PRINT("<Debug Mode is ON>");
    }
    else if(strncmp(read_buffer,"disable debug\0",13) == 0)
    {
      debug_mode = FALSE;
      PRINT("<Debug Mode is OFF>");
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

    int event_count, index; 
    int status;
    int cfd, efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char clientAddress[INET6_ADDRSTRLEN];
    char port[6], client_port[6], *serverAddr;
    struct sockaddr myIp;
    struct sockaddr_in saa;
    client_information_t *client_info = NULL;
    unsigned int capability;

    /* Allocates client_info */
    allocate_client_info(&client_info);

    if (argc != 5)
    {
      PRINT("Usage: %s <server_IP> <server_port> <group_name>\n", argv[0]);
      /* server_IP/server_port not provided. Attempting talk with server on same machine
       * with default port. */
//      argv[1] = ipStr;
      argv[2] = "3490";
      argv[3] = "G1,G2";
      argv[4] = "eth0";
    }
    serverAddr = argv[1];
    strcpy(port, argv[2]);
    strcpy(group_name,argv[3]);
    strcpy(clientAddress, argv[4]);

    /* Binding to any random user space port for client.*/
    sprintf(client_port, "%d", htons(0)); 

    /*decide on client address depending upon user provided client address or eth0 address*/
    if (!strcmp(clientAddress, "eth0")) {
       /*Fetching eth0 IP for client*/
       get_my_ip(clientAddress, &myIp);
       inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(myIp)), clientAddress, INET6_ADDRSTRLEN);
       client_info->client_id = calc_key(&myIp);
    } else {
       inet_pton(AF_INET, clientAddress, &(saa.sin_addr));
       client_info->client_id = saa.sin_addr.s_addr;
    }

    /* Creating Client socket*/    
    cfd = create_and_bind(clientAddress, client_port, CLIENT_MODE);

    if (cfd == -1)
    {
        perror("\nError while creating and binding the socket.");
        exit(0);
    }

    /* Socket is made non-blocking */
    status = make_socket_non_blocking(cfd);

    /* Creating sockaddr_in struct for Server entry and keeping it in client_info */
    client_info->server.sin_family = AF_INET;
    client_info->server.sin_port = htons(atoi(port));
    client_info->server.sin_addr.s_addr = inet_addr(serverAddr); 

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    PRINT("..WELCOME TO CLIENT (%s)..", clientAddress);
    PRINT("\r   <Use \"show help\" to see all supported clis.>\n");

    PRINT_PROMPT("[client] ");

    //Initialize timeout handler
    handle_timeout_real(true, 0, NULL, client_info);
    
    struct sigaction sa;
    PRINT("Establishing handler for moderator-server echo req timeout(signal: %d)\n", MODERATOR_TIMEOUT);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_timeout;
    sigemptyset(&sa.sa_mask);
    if (sigaction(MODERATOR_TIMEOUT, &sa, NULL) == -1)
        errExit("sigaction");

    PRINT("Establishing handler for client-moderator echo req timeout(signal: %d)\n", CLIENT_TIMEOUT);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_timeout;
    sigemptyset(&sa.sa_mask);
    if (sigaction(CLIENT_TIMEOUT, &sa, NULL) == -1)
        errExit("sigaction");
    
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
  
    /* Client takes program argument as group_name, creates join request
     * and joins the multiple groups */ 
    char * gname=strtok(group_name,",");
    char * gr_list[max_groups];
    int iter = 0;

    while(gname!=NULL){ 
      gr_list[iter++] = gname;
      gname=strtok(NULL,",");
    }

    pdu_t pdu;
    
    capability = generate_random_capability();
          
    populate_join_req(&(pdu.msg), gr_list, iter, capability);
    write_record(client_info->client_fd, &client_info->server, &pdu);

    while (TRUE) {
        /* Listening for epoll events*/
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for handling input from STDIN */
            if (STDIN_FILENO == events[index].data.fd)
            {
                /* Invoking function to recognize the cli fired and call its appropriate handler */
                client_stdin_data(events[index].data.fd, client_info);

                if (client_info->moderator_info) {
                   PRINT_PROMPT("[moderator] ");
                } else {
                   PRINT_PROMPT("[client] ");
                }
            }
            /* Code Block for receiving data on Client Socket */
            else
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
static
void send_task_results_to_moderator(client_information_t *client_info, char* group_name, unsigned int task_id,  rsp_type_t rtype, result_t *result, unsigned int my_id)
{

     pdu_t pdu;
     /* Sending task response for 1 group*/
     populate_task_rsp(&(pdu.msg),task_id, group_name, rtype ,result, my_id);
     write_record(client_info->client_fd, &(client_info->active_group->moderator) , &pdu);
     
     /* Deactivate timer for this group on client node. Not deleting for
      * moderator since it makes no sense to run task collection timer for self*/
     if(client_info->is_moderator == false) {
         if (client_info->active_group->timer_id) {
             timer_delete(client_info->active_group->timer_id);
         }
         client_info->client_status = FREE;
     }
     PRINT("[Task_Response_Notify_Req: GRP - %s] Task Response Notify Req sent to Moderator. ", group_name);
}


/* <doc>
 * void moderator_send_task_response_to_server(client_information_t *client_info)
 * This is moderator function, which will send the collated task results to Server.
 *
 * </doc>
 */
void moderator_send_task_response_to_server(client_information_t *client_info) {

  /*client_info->moderator_info will always be valid here*/
  moderator_information_t * moderator_info = client_info->moderator_info;
  pdu_t * rsp_pdu = (pdu_t *)moderator_info->moderator_resp_msg;

  /*If response from all the clients is received, then send the collated result to Server.*/
  if(rsp_pdu->msg.idv.task_rsp.num_clients == moderator_info->active_client_count) {
      write_record(client_info->client_fd, &(client_info->server) ,rsp_pdu);

      if (moderator_info->timer_id) {
          timer_delete(moderator_info->timer_id);
      }

      PRINT("[Task_Response: GRP - %s] Task Response sent to Server.", moderator_info->group_name);

      /* Free the moderator, since moderator job is done now.
       * and mark the client free
       */
      deallocate_moderator_list(&client_info);
      client_info->is_moderator = FALSE;
      client_info->client_status = FREE;
   }
}


/* <doc>
 * int handle_task_response(const int sockfd, const comm_struct_t const resp, ...)
 * Join Group Response Handler. Reads the join response PDU, and fetches the
 * IP for required multicast group. Joins the multicast group.
 *
 * </doc>
 */
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

    /*client_info->moderator_info will always be valid here, in case result is not sent to server*/
    moderator_information_t * moderator_info = client_info->moderator_info;

    /*Response from peer clients should only be processed by the moderator*/
    if(moderator_info != NULL && MATCH_STRING(moderator_info->group_name, task_response->group_name)){
        mod_client_node_t *mod_node = NULL; 
        unsigned int peer_id = calc_key(&(pdu->peer_addr));
        char ipaddr[INET6_ADDRSTRLEN];

        inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN); 
        PRINT("[Task_Response_Notify_Req: GRP - %s] Task Response Notify Request received from client %s", task_response->group_name, ipaddr);

        /*Since response from client is received, fetch client node from pending list.*/
        get_client_from_moderator_pending_list(client_info, peer_id, &mod_node);

        /*Process task response notify req only if it is expected by moderator, else ignore.*/ 
        if (mod_node) {
            /*If response received from peer clients of group*/ 
            if(moderator_info->moderator_resp_msg != NULL) {
                update_task_rsp(&((pdu_t *)moderator_info->moderator_resp_msg)->msg, task_response->type, task_response->result, peer_id);
            } else {
                /*If response received from moderator*/
                moderator_info->moderator_resp_msg = populate_moderator_task_rsp(moderator_info->active_client_count, task_response, peer_id);
            }

            /*Move client from pending to done list.*/
            move_moderator_node_pending_to_done_list(client_info, mod_node);
        } else {
          /* Add logging warning, as some client who is not expected to work on this task
           * has responded to moderator with task rsp notify req
           */
        }

        /*Send to Server if all clients have responded with their task response*/
        moderator_send_task_response_to_server(client_info);
    } else{
        /*PRINT("Error case : Moderator received a wrong input");
          We should add logging warning here.
          This case is possible when
            - Result is received from a client, who was declared dead previously.
            - Result from moderator has already been sent to Server.
          In this case, we should ignore the result sent by client, which moderator
          is not expecting.
        */
    }
}

/* <doc>
 * result_t* copy_result_from_args(thread_args * args)
 * Miscellaneous function to copy result data set from 
 * thread_args to result_t.
 *
 * </doc>
 */
result_t* copy_result_from_args(thread_args * args)
{
    result_t * result=NULL;
    int i;
    result = malloc(sizeof(result_t));
    result->value=NULL; 
    result->size=0;

    if(args->result_count>0)
    {
        result->size=args->result_count;
        result->value=malloc(sizeof(int)*result->size);
        for(i=0;i<result->size;i++)
            result->value[i]=args->result[i];
    }
    return result;
}

/* <doc>
 * void send_task_results(thread_args *args)
 * This function is called when task is processed by the client and
 * results are ready to send to moderator.
 *
 * </doc>
 */
void send_task_results(thread_args *args){
        
    result_t *answer = copy_result_from_args(args); 

    if(answer !=NULL) {
          send_task_results_to_moderator(args->client_info,args->group_name, args->task_id, TYPE_INT, answer, args->client_info->client_id);
    }
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

    PRINT("[INFO] Started working on prime numbers for data set count : %d", t_args->data_count);

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
            /*PRINT("Prime number %d",t_args->result[t_args->result_count]);*/

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
    unsigned int client_task_set[MAX_TASK_COUNT] = {0};
    unsigned int start_index = 0, stop_index = 0, client_task_count = 0;
    client_information_t * client_info;
    pthread_t thread;

    perform_task_req_t perform_task = req->idv.perform_task_req;
    thread_args *args = malloc(sizeof(thread_args));
 
    PRINT("[Task_Request: GRP - %s] Task Request Received for group %s.", perform_task.group_name, perform_task.group_name);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    /*Client is busy in working on task now.*/
    client_info->client_status = BUSY;

    /*Update moderator_info with active number of clients working on task*/
    if (client_info->moderator_info) {
        client_info->moderator_info->fsm_state = MODERATOR_TASK_RSP_PENDING;
        /*Number of clients working on this task*/
        client_info->moderator_info->active_client_count = perform_task.client_id_count;

        /*allocate the moderator client node and store the information about the working active clients*/
        int i = 0;
        char str[100] = {0};
        struct sockaddr_in ipAddr;
        char ipAddress[INET_ADDRSTRLEN];

        while (i < perform_task.client_id_count) {
            mod_client_node_t *mod_node = (mod_client_node_t *) allocate_clnt_moderator_node(&client_info->moderator_info);
  
            mod_node->peer_client_id = perform_task.client_ids[i];

            ipAddr.sin_addr.s_addr = perform_task.client_ids[i];
            ipAddr.sin_family = AF_INET;
            ipAddr.sin_port = htons(atoi(PORT));
            inet_ntop(AF_INET, &(ipAddr.sin_addr), ipAddress, INET_ADDRSTRLEN);
            sprintf(str, "%s , %s", str, ipAddress);
            memcpy(&mod_node->peer_client_addr, &ipAddr, sizeof(ipAddr));
            i++;
        }
        PRINT("Working clients are - %s", str);
    }

    switch(perform_task.task_type)
    {
      case FIND_PRIME_NUMBERS:
        for(count = 0; count < perform_task.client_id_count; count++)
        {
          if(client_info->client_id == perform_task.client_ids[count]) /*This client needs to perform the task */
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
        }
      }
   }
}

