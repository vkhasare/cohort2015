#include "common.h"
#include "client_DS.h"
#include <math.h>

#define SAFE_WRITE(x) {\
         pthread_mutex_lock(&lock);\
         x \
         pthread_mutex_unlock(&lock);}

const int MAX_ALLOWED_KA_MISSES = 5;
pthread_mutex_t lock;

extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
extern debug_mode;
static int echo_req_rcvd_in_notify_rsp_pending = 0;   /*Counter for number of clients who
                                                        have responded with echo req when moderator
                                                        is in moderator notify rsp state*/
bool mask_modification_in_progress = false;

struct{
  unsigned int client_rsp_list[255];
  int client_rsp_cntr;
}clnt_notify_alive_list;

fptr client_func_handler(unsigned int);
static int handle_echo_req(const int, pdu_t *pdu, ...);
static int handle_echo_response(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_response(const int, pdu_t *pdu, ...);
static int handle_join_response(const int, pdu_t *pdu, ...);
static int handle_mod_notification(const int, pdu_t *pdu, ...);
static int handle_moderator_update(const int, pdu_t *pdu, ...);
static int handle_task_response(const int, pdu_t *pdu, ...);
static int handle_perform_task_req(const int, pdu_t *pdu, ...);
static int handle_new_server_notification(const int, pdu_t *pdu, ...);
static void send_join_group_req(client_information_t *, char *);
static void send_leave_group_req(client_information_t *, char *);
static void send_moderator_notify_response(client_information_t *client_info);
static void send_task_results_to_moderator(client_information_t *, client_grp_node_t * group, unsigned int,rsp_type_t, char*,unsigned int, bool);
static void moderator_send_task_response_to_server(client_information_t *, bool force_cleanup);
static void fetch_task_response_from_client(unsigned int client_id, char * file_path, char * group_name, unsigned int);
static void fill_thread_args(client_information_t *, perform_task_req_t *, thread_args *, int);
void* execute_task(void *t_args);
static char* find_prime_numbers(thread_args *, char *, rsp_type_t * );
static char* find_sum(thread_args *, char *, rsp_type_t * );

void handle_timeout(int signal, siginfo_t *si, void *uc);

void handle_timeout_real(bool init, int signal, siginfo_t *si,
                         client_information_t *client_info);

void mask_client_signals(bool flag);
#define MASK_CLIENT_SIGNALS(flag) \
    mask_modification_in_progress = true; \
    mask_client_signals(flag); \
    mask_modification_in_progress = false;

void mask_client_signals(bool flag)
{
    if(flag)
    {
        LOGGING_INFO("Signals have been masked off.");
        signal(MODERATOR_TIMEOUT, SIG_IGN);
        signal(CLIENT_TIMEOUT, SIG_IGN);
    }
    else
    {
        struct sigaction sa;

        LOGGING_INFO("Signals have been enabled.");

        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = handle_timeout;
        sigemptyset(&sa.sa_mask);
        
        if (sigaction(MODERATOR_TIMEOUT, &sa, NULL) == -1)
            errExit("sigaction (MODERATOR_TIMEOUT)");
        
        if (sigaction(CLIENT_TIMEOUT, &sa, NULL) == -1)
            errExit("sigaction (CLIENT_TIMEOUT)");
    }
    
    /* For now commenting out sigproc related handling and sticking to signal().
       sigprocmask needs more study and stands as TBD. With all probability,
       sigprocmask is not what we need. we just want to ignore signals for
       duration of handler. signal() will serve purpose. This is to be examined later.
       
       sigset_t mask;
       sigset_t orig_mask;
   
       sigemptyset (&mask);
       sigaddset (&mask, MODERATOR_TIMEOUT);
       sigaddset (&mask, CLIENT_TIMEOUT);
   
       if (flag)
       {
           if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) 
           {
               perror ("sigprocmask");
               return;
           }
       }
       else
       {
           if (sigprocmask(SIG_UNBLOCK, &mask, &orig_mask) < 0) 
           {
               perror ("sigprocmask");
               return;
           }
       }
        */
}

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
    case new_server_notify:
        func_name = handle_new_server_notification;
        break;
    default:
        PRINT("Invalid msg type of type - %d.", msgType);
        func_name = NULL;
  }

  LOGGING_INFO("Received message of type %d", msgType);

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
     unsigned int client_ids[254], *id_arr = NULL, iter =0;
     mod_client_node_t *mod_node = NULL;
     client_grp_node_t* moderated_group = client_info_local->active_group;
     moderator_information_t* moderator_info = client_info_local->moderator_info;
     mod_node = SN_LIST_MEMBER_HEAD(&(moderator_info->pending_client_list->client_grp_node),
                                      mod_client_node_t,
                                      list_element);

     /* Cycle through all listed clients with moderator. Determine ones that
      * have become unreachable and send forward their report. */
     while (mod_node)
     {
        if(mod_node->peer_client_id == client_info_local->client_id)
        {
            mod_node = SN_LIST_MEMBER_NEXT(mod_node, mod_client_node_t, list_element);
            continue;
        }

        /*If all heartbeats are expired, then client is definitely dead.*/
        if(mod_node->heartbeat_remaining-- == 0)
        {
             client_ids[iter] = mod_node->peer_client_id;
             iter++;

             /*Remove this client from pending list*/
             SN_LIST_MEMBER_REMOVE(&(moderator_info->pending_client_list->client_grp_node),
                                     mod_node,
                                     list_element);

             char ipaddr[INET6_ADDRSTRLEN];
             inet_ntop(AF_INET, get_in_addr(&mod_node->peer_client_addr), ipaddr, INET6_ADDRSTRLEN);
             
             PRINT("[WARNING] Client %s is down.", ipaddr);
             LOGGING_WARNING("Client %s went down.", ipaddr);
         }
         mod_node = SN_LIST_MEMBER_NEXT(mod_node, mod_client_node_t, list_element);
     }

     /* If we detect that some clients have gone down, then send forward
      * the dead client report towards server, piggybacked on the echo req. */
     if(iter)
     {
         id_arr = (unsigned int *) malloc(sizeof(unsigned int) * iter);
         memcpy(id_arr, client_ids, sizeof(unsigned int) * iter);
     }    

     pdu_t pdu;
     pdu.msg.id = echo_req;
     echo_req_t * echo_request = &pdu.msg.idv.echo_req;
     initialize_echo_request(echo_request);

     ALLOC_AND_COPY_STRING(moderated_group->group_name, echo_request->group_name);
     echo_request->num_clients = iter;
     echo_request->client_ids  = iter ? id_arr : NULL;
     
     char ipaddr[INET6_ADDRSTRLEN];
     inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(client_info_local->server)), 
             ipaddr, INET6_ADDRSTRLEN);
     //XXX GAUTAM XXX
     //PRINT("[Echo_Request: GRP - %s] Echo Request sent to :%s, "
     //        "dead client count: %u", echo_request->group_name, ipaddr, iter);
     LOGGING_INFO("[Echo_Request: GRP - %s] Echo Request sent to :%s,         \
               dead client count: %u", echo_request->group_name, ipaddr, iter);
     write_record(client_info_local->client_fd, (struct sockaddr *)&(client_info_local->server), &pdu);


     /* Send task results towards server if all active clients have responded. */
     //moderator_send_task_response_to_server(client_info_local, false);
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
        /* TODO add per group mask_modification progress*/
        if(mask_modification_in_progress == true)
            return; 
        
        if(signal == MODERATOR_TIMEOUT)
        {
            /* No group lookups required for moderators since at any given time
             * one client node can act as moderator node to only one group.
             * Reference to this would be stored under client_infp->active_group. */

            /* Validation for moderator_info. This timer should not fire for
             * normal non-moderator client node. */
            assert(client_info_local->moderator_info != NULL);
            assert(client_info_local->is_moderator == true);

            /* Assumption here is that since client is allowed to moderate at
             * max one group, we will not be performing lookup of group based on
             * timer_id of timer that got fired. */
            
            if (client_info_local->moderator_info->fsm_state == MODERATOR_NOTIFY_RSP_PENDING) 
            {
                /* If moderator is in mod notify rsp pending state and timeout happens on moderator
                 * for receiving the echo's from all clients. */
                send_moderator_notify_response(client_info_local);
            }
            else if (client_info_local->moderator_info->fsm_state == MODERATOR_TASK_RSP_PENDING) 
            {
                /* If moderator is in mod task rsp pending state, and timeout happens on moderator,
                 * then echo towards server is initiated with dead client list*/
              moderator_task_rsp_pending_timeout(client_info_local);
            } 
            else 
            {
              /*If moderator fsm state is other than MODERATOR_NOTIFY_RSP_PENDING or MODERATOR_TASK_RSP_PENDING,
               *then assert. */
              LOGGING_WARNING("moderator fsm state is other than MODERATOR_NOTIFY_RSP_PENDING or MODERATOR_TASK_RSP_PENDING. \
                               Current FSM state is %d", client_info_local->moderator_info->fsm_state);
              assert(0);
            }
        }
        else if(signal == CLIENT_TIMEOUT)
        {
            if(client_info_local->is_moderator)
            {
                /* Even though these validations are not of great consequence in
                 * case of single moderator client in group, asserting to make
                 * sure there are no violations to timer related assumptions in
                 * other parts of code. */
                moderator_information_t * moderator_info = client_info_local->moderator_info;
                pdu_t * rsp_pdu = (pdu_t *)moderator_info->moderator_resp_msg;
                assert(moderator_info != NULL);
                
                send_echo_request(client_info_local->client_fd, (struct sockaddr *)&client_info_local->server, 
                                  client_info_local->active_group->group_name);

                if(rsp_pdu && moderator_info->expected_responses == rsp_pdu->msg.idv.task_rsp.num_clients)
                    moderator_send_task_response_to_server(client_info_local, false);
            }
            else
            {
                client_grp_node_t* group_node = NULL;

                /* TODO traverse the list to find which group timer expired*/
                get_client_grp_node_by_timerid(&client_info_local, 
                                               si->si_value.sival_ptr, 
                                               &group_node);
                
                send_echo_request(client_info_local->client_fd, &group_node->moderator, 
                                  group_node->group_name);
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
    //mask off timer interrupts at this critical update.
    MASK_CLIENT_SIGNALS(true);
    
    client_information_t *client_info = NULL;
    char ipaddr[INET6_ADDRSTRLEN];
    int i = 0;

    comm_struct_t *rsp = &(pdu->msg);
    moderator_update_req_t mod_update_req = rsp->idv.moderator_update_req;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    client_grp_node_t* moderated_group_node = client_info->active_group;
    char* moderated_group_name = moderated_group_node->group_name;
    
    client_grp_node_t* group_node = NULL; //group for which update has come.  
    get_client_grp_node_by_group_name(&client_info, mod_update_req.group_name, &group_node);

    /* Validation ideally should not be required for group subscription since
     * message comes on a multicast port. If group is not present in node's LL
     * and message is received it means multicast_leave was missed/was unsuccessful. */
    assert(group_node != NULL);
    
    // for old moderators, if for some reason they receive this message late, do
    // mandatory cleanup. 
    if(client_info->is_moderator == true &&
            MATCH_STRING(moderated_group_name, group_node->group_name))
    {
        //cleanup required for old moderator node.
        timer_delete(client_info->moderator_info->timer_id);
        client_info->moderator_info->timer_id = 0;
        deallocate_moderator_list(&client_info);
        
        /* Rationale behind doing this is that, if and old moderator is
         * receiving a moderator update, it means that old mod has been declared
         * to be dead by server. And in good probability this will not be the
         * moderator node selected in next cycle of moderator selection. In this case,
         * old moderator timers should be stopped and client timers should be started. 
         * For clients that are not and were not moderators, nothing changes. They need to
         * only update their local mod_info. Timers continue as before. */
        start_recurring_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
        
        /* Release active_group reference after this stage. This should not be
         * used hereafter. */
        client_info->is_moderator = FALSE;
        client_info->active_group = NULL;
    }

    /*Storing new moderator IP in client info*/
    struct sockaddr_in mod;
    mod.sin_family = AF_INET;
    mod.sin_port = mod_update_req.moderator_port;
    mod.sin_addr.s_addr = mod_update_req.moderator_id;

    memcpy(&group_node->moderator, (struct sockaddr *) &mod, sizeof(group_node->moderator));

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(mod)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Update_Req: GRP - %s] New Moderator IP is %s", mod_update_req.group_name, ipaddr);

    LOGGING_INFO("New moderator has been selected for group %s and is %s", mod_update_req.group_name, ipaddr);

    if (client_info->client_id != mod_update_req.moderator_id)
    {
        /* If the client had already finished the task assigned for group, then
         * forward the results to new moderator again. */
        if (group_node->last_task_file_count)
        {
            LOGGING_INFO("Sending results (task_id - %u) to new moderator %s for group %s", group_node->last_task_id, ipaddr, mod_update_req.group_name);
            for( i = 0; i<group_node->last_task_file_count ; i++)
            {
                send_task_results_to_moderator(client_info, group_node,
                        group_node->last_task_id, TYPE_INT, group_node->last_task_result_path[i], client_info->client_id, TRUE);
            }
        }
        
        MASK_CLIENT_SIGNALS(false);
        return; //nothing more to be done for non mod client(s).
    }
    
    /*IT IS THE MODERATOR BLOCK.. UPDATE MODERATOR DATA STRUCTURES*/
    PRINT("THIS CLIENT IS SELECTED AS NEW MODERATOR FOR GROUP %s", mod_update_req.group_name);
    LOGGING_INFO("This client has been selected as moderator for group %s", mod_update_req.group_name);

    /*Allocate the moderator info*/
    allocate_moderator_info(&client_info);
    /*mark client node as moderator*/
    client_info->is_moderator = TRUE;
    client_info->active_group = group_node;

    moderator_information_t *mod_info = client_info->moderator_info;

    strcpy(mod_info->group_name, mod_update_req.group_name);

    /*Number of clients in group, to be monitored by moderator*/
    mod_info->active_client_count = mod_update_req.client_id_count;
    mod_info->expected_responses  = mod_update_req.expected_task_responses;
 
    LOGGING_INFO("Number of clients working in group %s on task are %d", mod_info->group_name, mod_info->active_client_count);

    /**Register the moderator fsm handler and set the fsm state*/
    mod_info->fsm = moderator_main_fsm;
    /* Since modertor reselection can happen only during task execution of task,
     * hence putting moderator in MODERATOR_TASK_RSP_PENDING state*/
    mod_info->fsm_state = MODERATOR_TASK_RSP_PENDING;

    i = 0;
    while (i < mod_update_req.client_id_count) 
    {
        /*allocate the client node for pending list, to add the moderator node into it.*/
        mod_client_node_t *mod_node = allocate_clnt_moderator_node(&(client_info->moderator_info));

        struct sockaddr_in ipAddr;
        char ipAddress[INET6_ADDRSTRLEN];

        mod_node->peer_client_id = mod_update_req.client_ids[i];
        
        ipAddr.sin_addr.s_addr = mod_update_req.client_ids[i];
        ipAddr.sin_family = AF_INET;
        ipAddr.sin_port = htons(atoi(PORT));
        
        memcpy(&mod_node->peer_client_addr, &ipAddr, sizeof(ipAddr));
        mod_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
        i++;
        
        inet_ntop(AF_INET, &(ipAddr.sin_addr), ipAddress, INET6_ADDRSTRLEN);
        PRINT("Moderator is working with clients - %s", ipAddress);
        LOGGING_INFO("Added client %s in moderator pending list", ipAddress);
    }
    
    PRINT("Group state is - %u", group_node->state);

    if(TASK_EXECUTION_IN_PROGRESS == group_node->state)
    {
        //delete older self timer.
        timer_delete(group_node->timer_id);
        group_node->timer_id = 0;
    }
/*
    //start new timer for maintaining keepalives.
    if (1 == mod_info->active_client_count) 
    {
        start_recurring_timer(&(client_info->moderator_info->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
    } 
    else if (1 < mod_info->active_client_count)
    {
        // Start the timer on Moderator to get echo requests from all its peer
        // working clients
        start_recurring_timer(&(client_info->moderator_info->timer_id), DEFAULT_TIMEOUT, MODERATOR_TIMEOUT);
    }
 */
    FREE_INCOMING_PDU(pdu->msg);
    MASK_CLIENT_SIGNALS(false);
}

/* <doc>
 * static
 * int handle_new_server_notification(const int sockfd, pdu_t *pdu, ...)
 * This function handles the new_server_notify request from server and
 * updates its server address.
 *
 * </doc>
 */
static
int handle_new_server_notification(const int sockfd, pdu_t *pdu, ...)
{
    client_information_t *client_info;
    comm_struct_t *req = &(pdu->msg);
    new_server_notify_t new_server_update = req->idv.new_server_notify;

    PRINT("[new_server_notify] New Server Notify Request Received.");

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    /*Update with new server address*/
    client_info->server.sin_addr.s_addr = new_server_update.new_server_id;

    LOGGING_WARNING("New server is working and Server ID is %u", client_info->server.sin_addr.s_addr);

    FREE_INCOMING_PDU(pdu->msg);
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
    comm_struct_t *rsp = &(pdu->msg);
    moderator_notify_req_t mod_notify_req = rsp->idv.moderator_notify_req;
    
    client_information_t *client_info = NULL;
    /* Extracting client_info from variadic args. */
    EXTRACT_ARG(pdu, client_information_t*, client_info);
    
    /* Extract and set active group info here. */
    client_grp_node_t *group_node = NULL;
    get_client_grp_node_by_group_name(&client_info, mod_notify_req.group_name, &group_node);
    
    assert(group_node != NULL);

    struct sockaddr_in mod;
    char ipaddr[INET6_ADDRSTRLEN];

    /*Storing moderator IP in client info*/
    mod.sin_family = AF_INET;
    mod.sin_port = mod_notify_req.moderator_port;
    mod.sin_addr.s_addr = mod_notify_req.moderator_id;

    /* XXX Add handling for spurious message (unexpected group name on monitored multicast sock etc).*/
    memcpy(&group_node->moderator, (struct sockaddr *) &mod, sizeof(group_node->moderator));

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(mod)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Req: GRP - %s] Moderator IP is %s", mod_notify_req.group_name, ipaddr);
    
    /* Moderator known. This group now waits on TASK. Update state to reflect that. */
    group_node->state = TASK_AWAITED;

    LOGGING_INFO("Moderator Notification received for group %s. Moderator IP is %s", mod_notify_req.group_name, ipaddr);

    /* After moderator information, inform moderator about the self presence by
     * sending echo request msg. */
    if (client_info->client_id != mod_notify_req.moderator_id &&
            group_node->pending_task_count == 0) //XXX pending task count check can be modified to be on client_capability exhausted
    {
        send_echo_request(client_info->client_fd, &group_node->moderator, mod_notify_req.group_name);
    } 
    else 
    {
        /*IT IS THE MODERATOR BLOCK.. UPDATE MODERATOR DATA STRUCTURES*/
        PRINT("THIS CLIENT IS SELECTED AS MODERATOR FOR GROUP %s", mod_notify_req.group_name);

        LOGGING_INFO("This client has been selected as moderator for group %s", mod_notify_req.group_name);

        /*Allocate the moderator info*/
        allocate_moderator_info(&client_info);
        /*mark client node as moderator*/
        client_info->is_moderator = TRUE;
        client_info->active_group = group_node; 

        moderator_information_t *mod_info = client_info->moderator_info;

        strcpy(mod_info->group_name, mod_notify_req.group_name);

        /*Number of clients in group, to be monitored by moderator*/
        mod_info->active_client_count = mod_notify_req.num_of_clients_in_grp;
        mod_info->expected_responses = mod_notify_req.num_of_clients_in_grp;

        LOGGING_INFO("Waiting for echo request from %d peer clients", mod_info->active_client_count);

        /**Register the moderator fsm handler and set the fsm state*/
        mod_info->fsm = moderator_main_fsm;
        mod_info->fsm_state = MODERATOR_NOTIFY_RSP_PENDING;

        /*Updating list of clients who have responded during moderator notify duration*/
        clnt_notify_alive_list.client_rsp_list[(clnt_notify_alive_list.client_rsp_cntr)++] = client_info->client_id;

        /*If only 1 client in the group, then send moderator notify response for moderator right away.*/
        if (1 == mod_info->active_client_count) 
        {
            send_moderator_notify_response(client_info);

            /* Moderator node, if it is the only node in group, it has to start sending keepalives
             * right away since this would help server to know if moderator has immediatelygone down and 
             * there is no client in cluster to work on task.*/

            //start_recurring_timer(&(client_info->moderator_info->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
        } 
        else 
        {
            /* Start the timer on Moderator to get echo requests from all its peer clients */
            // Heuristic approach for duration looks slightly hacky. But helps with unexpected timeouts.
            start_oneshot_timer(&(client_info->moderator_info->timer_id), (5 * DEFAULT_TIMEOUT), MODERATOR_TIMEOUT);
        }
    }

    FREE_INCOMING_PDU(pdu->msg);
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

    for(cl_iter = 0; cl_iter < leave_rsp.num_groups; cl_iter++)
    {
        group_name = leave_rsp.group_ids[cl_iter].str;
        enum_cause = leave_rsp.cause;
        
        PRINT("[Leave_Response: GRP - %s] Cause : %s.", group_name, enum_to_str(enum_cause));
        LOGGING_INFO("Leave response for group %s received with cause %s", group_name, enum_to_str(enum_cause));

        /* if cause other than ACCEPTED, ignore the response */
        if (enum_cause != ACCEPTED) continue;
        
        get_client_grp_node_by_group_name(&client_info, group_name, &client_grp_node);
        assert(client_grp_node != NULL);

        /* Leave the multicast group if response cause is Accepted */
        if (TRUE == multicast_leave(client_grp_node->mcast_fd, client_grp_node->group_addr)) 
        {
            PRINT("Client has left multicast group %s.", group_name);
            LOGGING_INFO("Client has left multicast group %s.", group_name);
        } 
        else 
        {
            PRINT("[Error] Error in leaving multicast group %s.", group_name);
            LOGGING_ERROR("Client couldnot leave the multicast group %s.", group_name);
        }

        /* Delete kernel side timer if it allocated. (might not be allocated
         * if client never received any task for this group.) */
        if (client_grp_node->timer_id != 0) 
        {
            timer_delete(client_grp_node->timer_id);
            client_grp_node->timer_id = 0;
        }
        
        /* Removing group association from list */
        deallocate_client_grp_node(client_info, client_grp_node);
    }
    
    FREE_INCOMING_PDU(pdu->msg);
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
    client_grp_node_t node, *node_ref;
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

        LOGGING_INFO("Join Response received for group %s with cause %s", group_name, enum_to_str(enum_cause));

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
        
        /* Update the client-group association in list */
        strcpy(node.group_name,group_name);
        node.group_addr = group_ip;
        node.group_port = m_port;

        /* Join the multicast group with the groupIP present in join_response msg*/
        int mcast_fd = multicast_join(group_ip,m_port);

        if(mcast_fd <= 0) return;

        PRINT("Listening to group %s\n", group_name);
        LOGGING_INFO("Started listening on group %s", group_name);

        /* XXX revert if distastrous side effects observed. */
        //memset(&node,0,sizeof(node));

        /* Register multicast FD with EPOLL for listening events.*/
        event->data.fd = mcast_fd;
        event->events = EPOLLIN;

        status = epoll_ctl(client_info->epoll_fd, EPOLL_CTL_ADD, mcast_fd, event);

        if (status == -1)
        {
            perror("\nError while adding FD to epoll event.");
            LOGGING_ERROR("Error in registering multicast fd in epoll");
            exit(0);
        }

        /* If unable to add in list, then clean up epoll and return. */
        if (node_ref = ADD_CLIENT_IN_LL(&client_info, &node))
        {
            node_ref->mcast_fd = mcast_fd;
            node_ref->state    = MOD_INFO_AWAITED;
        }
        else
        {
            epoll_ctl(client_info->epoll_fd, EPOLL_CTL_DEL, mcast_fd, event);
        }
    }
    FREE_INCOMING_PDU(pdu->msg);
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
        LOGGING_WARNING("Client is already member of group %s.",group_name);
    }
    else
    {
        pdu_t pdu;
        comm_struct_t *req = &(pdu.msg);

        req->id = join_request;

        /* Sending join request for 1 group*/
        populate_join_req(req, &group_name, 1, client_info->client_capability);

        PRINT("[Join_Request: GRP - %s] Join Group Request sent to Server.", group_name);
        LOGGING_INFO("Sending Join request for group %s, having capability %u", group_name, client_info->client_capability);
        write_record(client_info->client_fd, &client_info->server, &pdu);
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
    client_grp_node_t *client_grp_node = NULL;
    get_client_grp_node_by_group_name(&client_info, group_name, &client_grp_node);

    /* TODO Add lookup and reorder for multiple groups. */
    if (client_grp_node && client_grp_node->state == TASK_EXECUTION_IN_PROGRESS) 
    {
        PRINT("[Error] Client cannot leave group %s as it is working on a task.", group_name);
        LOGGING_WARNING("Client is busy. So, cannot leave the group %s", group_name);
        return;
    }

    /* If client is member of requested group, then only send leave request */
    if (IS_GROUP_IN_CLIENT_LL(&client_info, group_name)) 
    {
        pdu_t pdu;
        comm_struct_t *req = &(pdu.msg);

        req->id = leave_request;
        /* Sending leave request for 1 group*/
        populate_leave_req(req, &group_name, 1);

        PRINT("[Leave_Request: GRP - %s] Leave Group Request being sent to Server.", group_name);
        LOGGING_INFO("Leave Group Request being sent to Server.", group_name);
        write_record(client_info->client_fd, &client_info->server, &pdu);
    } 
    else 
    {
        /* client is not member of request group */
        PRINT("Error: Client is not member of group %s.", group_name);
        LOGGING_WARNING("Client is not member of group %s.", group_name);
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
    //ideally this should not be required as that was a one shot timer.
    //commenting out for now.
    //stop_timer(&client_info->moderator_info->timer_id);

    pdu_t pdu;
    moderator_information_t *mod_info = client_info->moderator_info;
    comm_struct_t *rsp = &(pdu.msg);
    char ipaddr[INET6_ADDRSTRLEN];

    echo_req_rcvd_in_notify_rsp_pending = 0;

    rsp->id = moderator_notify_rsp;
    moderator_notify_rsp_t *moderator_notify_rsp = &(rsp->idv.moderator_notify_rsp);

    /*Filling group name and moderator id*/
    /* TODO try and replace this with ALLOC_AND_COPY_STR. TBD is restrict group
     * name length at file parsing and cli. */
    moderator_notify_rsp->group_name = MALLOC_STR;
    strcpy(moderator_notify_rsp->group_name, mod_info->group_name);
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

    LOGGING_INFO("Sending mod notify response to server for group %s, with client's count as %d",
                  moderator_notify_rsp->group_name, clnt_notify_alive_list.client_rsp_cntr);

    
    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(client_info->server)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Rsp: GRP - %s] Moderator Notify Response sent to server %s", moderator_notify_rsp->group_name, ipaddr);

    write_record(client_info->client_fd, &client_info->server, &pdu);

    /*Reset variables related to mod notify response*/
    clnt_notify_alive_list.client_rsp_cntr = 0;

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
    echo_req_rcvd_in_notify_rsp_pending++ ;

    /*Updating list of clients who have responded during moderator notify duration*/
    clnt_notify_alive_list.client_rsp_list[clnt_notify_alive_list.client_rsp_cntr++] = calc_key(&pdu->peer_addr);

    LOGGING_INFO("Client %u has responded echo request in moderator notify rsp pending state.", calc_key(&pdu->peer_addr));

    /* Send notification response to server if all the clients have responded with echo req or TIMEOUT happened
     * This is the case where all clients have responded.*/
    if (echo_req_rcvd_in_notify_rsp_pending == (client_info->moderator_info->active_client_count) - 1) 
    {
        /* Timer would not stopped in this case. Disarm explicitly.*/
        //stop_timer(&client_info->moderator_info->timer_id);
        timer_delete(client_info->moderator_info->timer_id);
        client_info->moderator_info->timer_id = 0;
        
        LOGGING_INFO("All the clients have responded with echo req in moderator notify rsp pending state. Sending notify rsp to server");
        send_moderator_notify_response(client_info);
    }
}

/* <doc>
 * void moderator_echo_req_task_rsp_pending_state(client_information_t *client_info,
 *                                               void *fsm_msg)
 * Function will get hit when all clients are working on the task and moderator is in
 * task response pending state (waiting for execution to complete so that it can
 * collate resutls and send it to server) and echo request from client comes.
 * All periodic echo requests from clients in cluster shall hit this function.
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

    get_client_from_moderator_pending_list(client_info->moderator_info, calc_key(&pdu->peer_addr), &client_node);

    if (client_node) 
    {
        client_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
        LOGGING_INFO("Echo req from client %u in task rsp pending state", client_node->peer_client_id);
    }
    else
    {
        LOGGING_WARNING("[UNREGISTERED CLIENT ECHO ALERT] Echo req from %u in task rsp pending state", calc_key(&pdu->peer_addr));
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
     //XXX GAUTAM XXX
//    PRINT("[Echo_Response: GRP - %s] Echo Response received from %s", echo_rsp->group_name, ipaddr);
    LOGGING_INFO("[Echo_Response: GRP - %s] Echo Response received from %s", echo_rsp->group_name, ipaddr);
    FREE_INCOMING_PDU(pdu->msg);

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
int handle_echo_req(const int sockfd, pdu_t *pdu, ...)
{
    comm_struct_t *req = &(pdu->msg);
    pdu_t rsp_pdu;
    comm_struct_t *rsp = &(rsp_pdu.msg);
    char ipaddr[INET6_ADDRSTRLEN];
    client_information_t *client_info = NULL;
    echo_req_t echo_req = req->idv.echo_req;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
     //XXX GAUTAM XXX
//    PRINT("[Echo_Request: GRP - %s] Echo Request received from %s", echo_req.group_name, ipaddr);
    LOGGING_INFO("[Echo_Request: GRP - %s] Echo Request received from %s", echo_req.group_name, ipaddr);
    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);
    
    /* Unmask timers. It is more important to send own echo req towards server. */
    if(client_info->is_moderator == true)
    {
        /* Not validating source group info here since immaterial of source of
         * echo request, if a client is a moderator, sending of its own echo req
         * towards server is of highest proirity. Unconditionally unmasking. */
        MASK_CLIENT_SIGNALS(false);
    }
        
    rsp->id = echo_response;
    echo_rsp_t *echo_response = &(rsp->idv.echo_resp);

    /*filling echo rsp pdu*/
    echo_response->status    = client_info->client_status;
    echo_response->group_name = MALLOC_STR;
    strcpy(echo_response->group_name, echo_req.group_name);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    //XXX GAUTAM XXX
    //    PRINT("[Echo_Response: GRP - %s] Echo Response sent to %s", echo_response->group_name, ipaddr);
    LOGGING_INFO("[Echo_Response: GRP - %s] Echo Response sent to %s", echo_response->group_name, ipaddr);
    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    /* If moderator, then run the moderator fsm. Validate source of echo req here. */
    if (client_info->is_moderator &&
            MATCH_STRING(client_info->active_group->group_name, echo_req.group_name)) 
    {
        fsm_data_t fsm_msg;
        fsm_msg.fsm_state = client_info->moderator_info->fsm_state;
        fsm_msg.pdu = pdu;

        /*run the fsm*/
        client_info->moderator_info->fsm(client_info, MOD_ECHO_REQ_RCVD_EVENT, &fsm_msg);
    }

    FREE_INCOMING_PDU(pdu->msg);

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
    PRINT("force cleanup                                --  Moderator CLI. Forcefully cleans up the moderator and pushes the task result towards Server");
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
    else if (0 == strcmp(read_buffer,"force cleanup\0"))
    {
        if (client_info->moderator_info) {
           moderator_send_task_response_to_server(client_info, true);
        } else {
           PRINT("Error: This CLI is applicable only for moderator.");
        }
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

    /*Start logging on client*/
    enable_logging(argv[0]);

    /* Allocates client_info */
    allocate_client_info(&client_info);
    
    /* Assigning some numeric capability index to this client. In heterogeneous
     * environment this could be a function of resources available with client
     * e.g. processing power, memory, client process priority on host. */
    client_info->client_capability = generate_random_capability();

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
    if (!strcmp(clientAddress, "eth0")) 
    {
       /*Fetching eth0 IP for client*/
       get_my_ip(clientAddress, &myIp);
       inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(myIp)), clientAddress, INET6_ADDRSTRLEN);
       client_info->client_id = calc_key(&myIp);
    } 
    else 
    {
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

    LOGGING_INFO("Client started on %s", clientAddress);

    /* Socket is made non-blocking */
    status = make_socket_non_blocking(cfd);

    /* Creating sockaddr_in struct for Server entry and keeping it in client_info */
    client_info->server.sin_family = AF_INET;
    client_info->server.sin_port = htons(atoi(port));
    client_info->server.sin_addr.s_addr = inet_addr(serverAddr); 
    memset(&client_info->server.sin_zero,0,sizeof(client_info->server.sin_zero));

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

    PRINT("Establishing handler for moderator-server echo req timeout(signal: %d)\n", MODERATOR_TIMEOUT);
    PRINT("Establishing handler for client-moderator echo req timeout(signal: %d)\n", CLIENT_TIMEOUT);
    mask_client_signals(false);

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
    event.events = EPOLLIN;

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

    while(gname!=NULL)
    {
        LOGGING_INFO("Sending join request for %s", gname); 
        gr_list[iter++] = gname;
        gname=strtok(NULL,",");
    }

    pdu_t pdu;
    
    LOGGING_INFO("Client has capability as %u", client_info->client_capability);

    populate_join_req(&(pdu.msg), gr_list, iter, client_info->client_capability);
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
                MASK_CLIENT_SIGNALS(true);
                fptr func;

                /* read_record reads data on events[index].data.fd and fills req struct.
                 * client_func_handler then returns function to handle the req depending
                 * upon req msg type. Once function handler name is received, it is invoked.
                 */
                if ((func = client_func_handler(read_record(events[index].data.fd, &pdu))))
                {
                    (func)(events[index].data.fd, &pdu, client_info);
                }
                MASK_CLIENT_SIGNALS(false);
            }
        }
    }

    /* close the client socket*/    
    close(cfd);

    return 0;
}
#endif

/* <doc>
 * void update_moderator_info_with_task_response(client_information_t *client_info, 
 *                                               task_rsp_t *task_response, unsigned int peer_id)
 * Update moderator information with task response. Reads the task response PDU, and appends the moderator
 * Response message with the new received response.
 *
 * </doc>
 */
void update_moderator_info_with_task_response(client_information_t *client_info, 
                                              task_rsp_t *task_response, unsigned int peer_id)
{
    moderator_information_t *moderator_info = client_info->moderator_info;
    mod_client_node_t *mod_node = NULL; 

    /*Since response from client is received, fetch client node from pending list.*/
    get_client_from_moderator_pending_list(moderator_info, peer_id, &mod_node);

    /* Process task response notify req only if it is expected by moderator, else ignore. */ 
    if (mod_node) 
    {
        mod_node->ref_count--;
        /* If moderator response msg has been allocated for the group. */ 
        if(moderator_info->moderator_resp_msg != NULL) 
        {
            LOGGING_INFO("Task response notify req for group %s is received from %u", 
                    task_response->group_name, peer_id);
            update_task_rsp(&((pdu_t *)moderator_info->moderator_resp_msg)->msg, 
                    task_response->type, task_response->result->str, peer_id);
        } 
        else 
        {
            /* Allocate and populate moderator side response structure. */
            LOGGING_INFO("Moderator has received the first task response for group %s from client id %u", 
                    task_response->group_name, peer_id);
            
            moderator_info->moderator_resp_msg = 
                populate_moderator_task_rsp(moderator_info->expected_responses, task_response, peer_id);
        }
        
        /* Move client from pending to done list. */
        if (mod_node->ref_count == 0)
        {
           move_moderator_node_pending_to_done_list(client_info, mod_node);
        }

        /*Send to Server if all clients have responded with their task response*/
        moderator_send_task_response_to_server(client_info, false);
    } 
    else 
    {
        /* Some client who is not expected to work on this task
         * has responded to moderator with task rsp notify req
         */
        LOGGING_WARNING("Client %u who is not expected to work on group %s task,"
                "has responded to moderator with task rsp notify req",
                peer_id, task_response->group_name);
    }
}

/* <doc>
 * static void send_task_results_to_moderator(client_information_t *client_info, rsp_type_t, result_t *)
 * This function creates Group Task Response message and sends to moderator. It accepts
 * results as input and relays message on client fd.
 * Request will not be relayed if client is not member of group.
 *
 * </doc>
 */
static
void send_task_results_to_moderator(client_information_t *client_info, client_grp_node_t * group, unsigned int task_id,  rsp_type_t rtype,
                                    char* file_path, unsigned int my_id, bool is_task_reassigned)
{
    pdu_t pdu;
    char * group_name = group->group_name;
    
    /* Sending task response for 1 group*/
    populate_task_rsp(&(pdu.msg),task_id, group_name, rtype ,file_path, my_id);
    moderator_information_t * moderator_info = client_info->moderator_info;
    
    /* If this client is a non-mod client, write result onto socket. Otherwise
     * just update internal data structure. */
    if((client_info->is_moderator == false) || (!MATCH_STRING(moderator_info->group_name, group_name)))
    { 
        write_record(client_info->client_fd, &group->moderator, &pdu);
    } 
    else 
    {
        //This client is the moderator for the group. no need to send it via network. 
        task_rsp_t *task_response=&(pdu.msg.idv.task_rsp);
        PRINT("[Task_Response_Notify_Req: GRP - %s] Moderator finished executing own task.", group_name);
        SAFE_WRITE(update_moderator_info_with_task_response(client_info, task_response, client_info->client_id););
        FREE_INCOMING_PDU(pdu.msg);
    } 
    
    // Storing task_result_file_path
    if(!is_task_reassigned)
    {
      SAFE_WRITE(
           group->last_task_result_path[group->last_task_file_count++]=file_path;
           group->last_task_id = task_id;
           group->last_task_rsp_type = pdu.msg.idv.task_rsp.type;
      );
    }
}


/* <doc>
 * static
 * void moderator_send_task_response_to_server(client_information_t *client_info, bool force_cleanup)
 * This is moderator function, which will send the collated task results to Server.
 *
 * </doc>
 */
static
void moderator_send_task_response_to_server(client_information_t *client_info, bool force_cleanup)
{
    /* Critical section. We dont want timer to interrupt the moderator when it
     * is attempting to send response to server. mod_info structure and timers
     * undergo modifications and these should not be interrupted. So, */
    MASK_CLIENT_SIGNALS(true);
    
    /*client_info->moderator_info will always be valid here*/
    moderator_information_t * moderator_info = client_info->moderator_info;
    pdu_t * rsp_pdu = (pdu_t *)moderator_info->moderator_resp_msg;
    int i;
    unsigned long num, result=0;
 
    if(rsp_pdu == NULL)
    {
       MASK_CLIENT_SIGNALS(false);
       return;
    }
    
    int num_clients=rsp_pdu->msg.idv.task_rsp.num_clients;

    /* Send response iff all active clients have responded.
     * For force cleanup mode, ignore the below condition and push
     * the results towards server. */
    if(force_cleanup == false &&
      (num_clients != moderator_info->expected_responses))
    {
      MASK_CLIENT_SIGNALS(false);
      return;
    }

    /* Strictly speaking, this assert is not required. Adding to to catch
     * scenarios when these are out of sync. TODO Remove later. */
    if(force_cleanup == false)
      assert(moderator_info->pending_client_list->client_grp_node.length == 0);
    else
      LOGGING_INFO("Forcefully pushing the moderator results towards Server for group %s", moderator_info->group_name); 

    task_rsp_t *task_resp= &(rsp_pdu->msg.idv.task_rsp);
    switch(task_resp->type){
    case TYPE_FILE:
      for(i = 0; i < num_clients; i++)
      {
          /* TODO Investigate if thread should be spawned for this operation. */
          fetch_task_response_from_client(task_resp->client_ids[i], task_resp->result[i].str, task_resp->group_name, client_info->client_id);
      }
      break;
    case TYPE_LONG:
      for(i=0; i < num_clients;i++){
        PRINT("Response from client %u is %s",task_resp->client_ids[i], task_resp->result[i].str);
        num=atol(task_resp->result[i].str);    
        result += num;
      }
      free(task_resp->final_resp);
      task_resp->final_resp=malloc(23);
      sprintf(task_resp->final_resp, "%lu", result);
      break;
    }
    write_record(client_info->client_fd, &(client_info->server), rsp_pdu);
    free(rsp_pdu);

    timer_delete(moderator_info->timer_id);
    moderator_info->timer_id = 0;

    PRINT("[Task_Response: GRP - %s] Task Response sent to Server.", moderator_info->group_name);
    LOGGING_INFO("Task response for group %s is sent to server, client worked - %d", moderator_info->group_name, moderator_info->active_client_count);

    /* Free the moderator, since moderator job is done now.
     * Mark the client free */
    deallocate_moderator_list(&client_info);
    client_info->is_moderator = FALSE;
    client_info->client_status = FREE; /* XXX Questionable. */
    //client_info->active_group->state = TASK_RES_SENT;
    client_info->active_group = NULL;
    //should get taken care of via worker thread.
    //client_info->active_group->pending_task_count = 0;

    MASK_CLIENT_SIGNALS(false);
}
/* <doc>
 * int handle_task_response(const int sockfd, const comm_struct_t const resp, ...)
 * Handles the task response from peer clients.
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
    char ipaddr[INET6_ADDRSTRLEN];

    comm_struct_t *resp = &(pdu->msg);
    client_information_t *client_info = NULL;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);

    task_rsp_t * task_response = &resp->idv.task_rsp;

    /*client_info->moderator_info will always be valid here, in case result is not sent to server*/
    moderator_information_t * moderator_info = client_info->moderator_info;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Task_Response_Notify_Req: GRP - %s] Task Response Notify Request received from client %s", task_response->group_name, ipaddr);

    /*Response from peer clients should only be processed by the moderator*/
    if(moderator_info != NULL && MATCH_STRING(moderator_info->group_name, task_response->group_name))
    {
        unsigned int peer_id = calc_key(&(pdu->peer_addr));
       
        SAFE_WRITE(update_moderator_info_with_task_response(client_info, task_response, peer_id););
    } 
    else
    {
        LOGGING_WARNING("Task response notify request received for case, either\n"
                        "- Result is received from a client, who was declared dead previously.\n"
                        "- Result from moderator has already been sent to Server.");
          /* In this case, we ignore the result sent by client, which moderator
           * is not expecting. */
    }
    FREE_INCOMING_PDU(pdu->msg);
}

/* <doc>
 * fetch_file_from_server(client_information_t * client_info, char * file_path)
 * fetch file from server to the client.
 *
 * </doc>
 */

char * fetch_file_from_server(client_information_t * client_info, char * file_folder, char * file_name, char * group_name){
   
   char src[100]={0};
   char *dest=malloc(sizeof(char)*80);
   char ipaddr[INET6_ADDRSTRLEN];

   inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(client_info->server)), 
             ipaddr, INET6_ADDRSTRLEN);
   sprintf(src, "%s:%s/%s", ipaddr, file_folder, file_name);

   sprintf(dest, "/tmp/client_vsk/%s/", group_name);
   check_and_create_folder(dest);
   strcat(dest, file_name);

   return fetch_file(src, dest); 
}

void copy_file(char *src, char *dest){
   char cmd[200];
   sprintf(cmd,"cp %s %s", src, dest);
   system(cmd);
}


/* <doc>
 * void fetch_task_response_from_client(client_information_t * client_info, char * file_path)
 * fetch file from client to the group moderator
 *
 * </doc>
 */
void fetch_task_response_from_client(unsigned int client_id, 
        char * file_path, char * group_name, unsigned int mod_id)
{

   struct in_addr ip_addr;
   ip_addr.s_addr = client_id;
   char src[100];
   char dest[100];

   sprintf(dest, "/tmp/client_vsk/moderator/%s/", group_name);

   check_and_create_folder(dest);

   strcat(dest, (char *)basename(file_path));

   if(mod_id != client_id)
   {
     sprintf(src, "%s:%s", inet_ntoa(ip_addr),  file_path);
     fetch_file(src, dest);
   }
   else
   {
     copy_file(file_path, dest);
   }
}

/* <doc>
 * void free_thread_args(thread_argsyy *args)
 * free thread arguments memory on client
 *
 * </doc>
 */

void free_thread_args(thread_args *args){

   args->group = NULL;
   free(args->task_filename);
   free(args->task_folder_path);
   free(args);

}

/* <doc>
 * void* execute_task(void *args)
 * Execute Task on client.
 *
 * </doc>
 */

void* execute_task(void *args)
{
    unsigned int task_count;

    mask_signal(CLIENT_TIMEOUT, true);
    mask_signal(MODERATOR_TIMEOUT, true);

    thread_args *t_args = (thread_args *)args;
    client_information_t *client_info = t_args->client_info;
    client_grp_node_t * group = t_args->group;
    /*Making thread detached.*/
    pthread_t self = pthread_self();
    pthread_detach(self);

    char * dest=fetch_file_from_server(client_info, t_args->task_folder_path, 
            t_args->task_filename, group->group_name);

    char * result=NULL;
    rsp_type_t result_type;
    if(dest !=NULL)
    {
       switch(t_args->task_type) {
         case FIND_PRIME_NUMBERS:
           result = find_prime_numbers(t_args, dest, &result_type);
         break;
         case FIND_SUM:
           result = find_sum(t_args, dest, &result_type);
         break;
       } 

       PRINT(" Task id %u has been successfully executed on client", t_args->task_id);
       if(result !=NULL)
       {
           group->pending_task_count--;
           assert((int)group->pending_task_count >= 0);
           
           if((client_info->is_moderator == false ||
                       (client_info->is_moderator == true && 
                        !MATCH_STRING(client_info->active_group->group_name,group->group_name))) 
                   && group->pending_task_count == 0)
           {
               timer_delete(group->timer_id);
               group->timer_id = 0;
               /*TODO add state change to TASK_INFO_AWAITED on server res ack.*/
               group->state = TASK_RES_SENT;
           }
           
           /* Lot of clean up will happen on this path. Timers will be
            * deactivated post successful send towards server. */
           send_task_results_to_moderator(t_args->client_info, t_args->group, t_args->task_id,
                   result_type, result, t_args->client_info->client_id, FALSE);

           if(group->pending_task_count == 0)
           {
               LOGGING_INFO("Client is done with its task for group %s and is now free."
                       "Sending task rsp notify to moderator", group->group_name);
               PRINT("[Task_Response_Notify_Req: GRP - %s] Task Response Notify Req "
                       "being sent to Moderator. ", group->group_name);
           }
           else
           {
               LOGGING_INFO("Number of pending tasks with client: %u", group->pending_task_count);
               PRINT("[Task_Response_Notify_Req: GRP - %s] Number of pending tasks with client: %u", 
                       group->group_name, group->pending_task_count);
           }

           /* Deactivate timer for this group on client node. 
           if(group->pending_task_count == 0
                   && (client_info->is_moderator == FALSE ||
                       !MATCH_STRING(client_info->active_group->group_name,group->group_name)))
           {
               /// Check designed on thread init time parameters since client_info level
                // information is subject to change via other threads of
                // execution. checking if client is/was mod for this group. /
           }*/

       }
       free(dest);
    }
    else
    {
      PRINT("failed to open the task set");
      LOGGING_ERROR("failed to open the task set");
    } 
    
    //free thread args
    free_thread_args(t_args);
    return NULL;
}

/* <doc>
 * void* find_prime_numbers(thread_args *args)
 * Reads the subset of data and prints prime numbers
 *
 * </doc>
 */
char * find_prime_numbers(thread_args *t_args, char * file_path, rsp_type_t *rtype)
{
    unsigned int i,j;
    bool flag;
    unsigned long number;
    unsigned int result_count=0;
    int fdSrc;
    struct stat sb;
    char *num, *src;
    int len=0;
    int task_count = get_task_count(file_path);

    fdSrc = open(file_path, O_RDONLY);

    if (fdSrc == -1)
    {
        LOGGING_ERROR("Error in opening file %s", file_path);
        errExit("open");
    }
    /* Use fstat() to obtain size of file: we use this to specify the
       size of the two mappings */

    if (fstat(fdSrc, &sb) == -1)
    {
        LOGGING_ERROR("Error in fstat of file %s", file_path);
        errExit("fstat");
    }
    /* Handle zero-length file specially, since specifying a size of
       zero to mmap() will fail with the error EINVAL */

    if (sb.st_size == 0)
        LOGGING_ERROR("%s is zero-size file", file_path);
    assert(sb.st_size != 0);

    src = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fdSrc, 0);
    if (src == MAP_FAILED)
    {
        LOGGING_ERROR("Error in mmap of file %s", file_path);
        errExit("mmap");
    }

    PRINT("[INFO] Started working on prime numbers for data set count : %lu", task_count);
    LOGGING_INFO("Started working on prime numbers for data set count : %lu", task_count);

    char folder_path[120];

    assert(t_args->group != NULL);

    sprintf(folder_path, "/tmp/client_vsk/task_result/%s", t_args->group->group_name);

    check_and_create_folder(folder_path);

    char *dst_file_path=malloc(sizeof(char)*180);

    sprintf(dst_file_path,"%s/%s", folder_path, t_args->task_filename);

    FILE*fptr=(fopen(dst_file_path,"w"));

    if(fptr==NULL)
    {
       LOGGING_ERROR("Error in opening destination file %s", dst_file_path); 
       free(dst_file_path);
       return NULL;
    }
    int x=task_count/10, u=0;
    /* Loop for prime number's in given data set */
    for(i = 0; i < task_count; i++)
    { 
        num=src+len; //starting position of the current number
        //calculating length of the number
        char * end = src+len;
        while(*end != '\n')  {end++;len++;}
        len++; //incrementing for '\n' character

        number=atol(num);//converting string to long 
        //PRINT("The number is %l" , number);
        if(number < 2) continue; 

        flag = FALSE;
        unsigned long root = (unsigned long)sqrt(number);

        //checking if number is a prime no
        for (j = 2; j <= root; j++)
        {
            if ((number % j) == 0)
            {
                flag = TRUE;
                break;
            }
        }
        if(!flag)
        {
            
            fprintf(fptr, "%10u\n", number);
//            PRINT("Prime number %u",number);

            /* Total count of prime numbers */
            result_count++;
        }
        if(i%x==0) {
           PRINT("%d0 % Task successfully executed", u);
           LOGGING_INFO("%d0 % Task successfully executed", u);
           u++;
        }
    }

    if(result_count == 0)
    {
        PRINT("No prime numbers found..");
        LOGGING_INFO("No prime numbers found..");
    }

    fclose(fptr);
    munmap(src, sb.st_size);
    *rtype = TYPE_FILE; 
    return dst_file_path;
}



/* <doc>
 * void* find_sum(thread_args *args)
 * Reads the subset of data and calculates the sum of all numbers
 *
 * </doc>
 */
char * find_sum(thread_args *t_args, char * file_path, rsp_type_t *rtype)
{
    unsigned int i,j;
    bool flag;
    unsigned long number, sum=0;
    unsigned int result_count=0;
    int fdSrc;
    struct stat sb;
    int len=0;
    char *num, *src;

    int task_count = get_task_count(file_path);

    fdSrc = open(file_path, O_RDONLY);
    if (fdSrc == -1)
        errExit("open");

    /* Use fstat() to obtain size of file: we use this to specify the
       size of the two mappings */

    if (fstat(fdSrc, &sb) == -1)
        errExit("fstat");

    /* Handle zero-length file specially, since specifying a size of
       zero to mmap() will fail with the error EINVAL */

    if (sb.st_size == 0)
        exit(EXIT_SUCCESS);

    src = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fdSrc, 0);
    if (src == MAP_FAILED)
        errExit("mmap");

    PRINT("[INFO] Started working on prime numbers for data set count : %lu", task_count);
    LOGGING_INFO("Started working on prime numbers for data set count : %lu", task_count);

    char *result_str=malloc(sizeof(char)*23);

    int x=task_count/10, u=0;

    /* Loop for prime number's in given data set */
    for(i = 0; i < task_count; i++)
    {
        num=src+len; //starting position of the current number
        //calculating length of the number
        char * end = src+len;
        while(*end != '\n')  {end++;len++;}
        len++; //incrementing for '\n' character

        number=atol(num);//converting string to long

        sum += number;
        
        if(i%x==0){
         PRINT("%d0 % Task successfully executed", u);
         u++;}
    }
    sprintf(result_str, "%lu", sum);
    PRINT("The final sum is %s", result_str); 
    munmap(src, sb.st_size);
    *rtype = TYPE_LONG; 
    return result_str;
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
    int count[5] = {0} , i = 0, result;
    client_information_t * client_info = NULL;
    moderator_information_t * mod_info = NULL;
    client_grp_node_t * requested_group = NULL;
    pthread_t thread;

    perform_task_req_t * perform_task = &(req->idv.perform_task_req);

    int current_working_clients = perform_task->client_id_count;
 
    PRINT("[Task_Request: GRP - %s] Task Request Received for group %s.", perform_task->group_name, perform_task->group_name);

    LOGGING_INFO("Task request is received for group %s", perform_task->group_name);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, client_information_t*, client_info);
    mod_info = client_info->moderator_info;

    get_client_grp_node_by_group_name(&client_info, perform_task->group_name, &requested_group);
    assert(requested_group != NULL);

    /*Reset the cached results when new task request is received for the group*/
    if (!perform_task->task_reassigned)
    {
        for(i=0;i < requested_group->last_task_file_count; i++)
        {
          free(requested_group->last_task_result_path[i]);  
          requested_group->last_task_result_path[i]=NULL;
        }
        requested_group->last_task_result_path[0] = '\0';
        requested_group->last_task_id = -1;
        requested_group->last_task_file_count = 0;
        requested_group->state = STATE_NONE;
    }

    /* Check whether this particular client is required to work on this piece of
     * task. If not, just return. */
    bool found = false;
    int number_of_occurence = 0;
    for(i = 0; i < current_working_clients; i++)
    {
        if(client_info->client_id ==  perform_task->client_ids[i])
        {
            count[number_of_occurence++] = i;
            found = true;
        }
    }

    /*Update moderator_info with active number of clients working on task*/
    if (mod_info && MATCH_STRING(mod_info->group_name, requested_group->group_name))
    {
        mod_info->fsm_state = MODERATOR_TASK_RSP_PENDING;

        /*allocate the moderator client node and store the information about the working active clients*/
        struct sockaddr_in ipAddr;
        char ipAddress[INET_ADDRSTRLEN];

        if (perform_task->task_reassigned)
        {
            LOGGING_INFO("Retransmitted task request for clients (group %s)", perform_task->group_name);
            /*Currently hardcoded for moderator down scenario. Here, 1 is number of response expected due to dead clients*/
//            mod_info->active_client_count += 1;
        }
        else
        {
            /*Number of clients working on this task*/
            mod_info->active_client_count = perform_task->client_id_count;
        }

        i = 0;
        while (i < current_working_clients) 
        {
            mod_client_node_t *mod_node = NULL;
            get_client_from_moderator_pending_list(mod_info, perform_task->client_ids[i], &mod_node);

            /* XXX TODO Examine if we really need to allocate a new node. If we find
             * node in done_list, we should be able to use that. */
            if (mod_node == NULL) 
            {
                mod_node = (mod_client_node_t *) allocate_clnt_moderator_node(&mod_info);
            } 
            else 
            {
                mod_node->ref_count++;
            }

            mod_node->peer_client_id = perform_task->client_ids[i];

            ipAddr.sin_addr.s_addr = perform_task->client_ids[i];
            ipAddr.sin_family = AF_INET;
            ipAddr.sin_port = htons(atoi(PORT));
            inet_ntop(AF_INET, &(ipAddr.sin_addr), ipAddress, INET_ADDRSTRLEN);

            LOGGING_INFO("Client %s is working on task in group %s", ipAddress, perform_task->group_name);
            PRINT("Client %s is working on task in group %s", ipAddress, perform_task->group_name);
	    
            memcpy(&mod_node->peer_client_addr, (struct sockaddr *) &ipAddr, sizeof(struct sockaddr));
            i++;
        }
        
        if(mod_info->timer_id == 0)
        {
            /* Start timer for maintaining client's keepalive*/
            if (1 == mod_info->active_client_count && found)
            {
                start_recurring_timer(&(mod_info->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
            }
            else
                start_recurring_timer(&(mod_info->timer_id), DEFAULT_TIMEOUT, MODERATOR_TIMEOUT);
        }
    }

    if(found == false)
    {
        requested_group->state = IDLE; 
        PRINT("[INFO] Task Request for group %s is not intended for this client.", perform_task->group_name);
        FREE_INCOMING_PDU(pdu->msg);
        return;
    }
 
    /* assumption client_id_count < num of task count */

    LOGGING_INFO("Client started working on group %s task - prime numbers", perform_task->group_name);
    i = 0;
    while (i < number_of_occurence)
    {
        thread_args *args = malloc(sizeof(thread_args));
        fill_thread_args(client_info, perform_task, args, count[i]);
        /* Create a thread to perform the task */
        result = pthread_create(&thread, NULL, execute_task ,args);
        i++;

        if(result)
        {
            PRINT("Could not create thread to perform task");
            LOGGING_ERROR("Failure in creating thread to perform task");
        }
        else 
        {
            /* Fire timer for maintaining keepalives on behalf of this group
             * task from main thread itself for non mod threads. */
            if((client_info->is_moderator == false) || 
                    (/*NOT*/ !MATCH_STRING(requested_group->group_name, client_info->active_group->group_name)))
            {
                if(requested_group->pending_task_count == 0)
                    start_recurring_timer(&(requested_group->timer_id), DEFAULT_TIMEOUT, CLIENT_TIMEOUT);
            }

            /*Increment the busy count by 1 as client is working on one of the task set*/    
            requested_group->pending_task_count++;

            /* Moderator state and moderator's client view of task are mutually
             * exclusive concepts. Update state of all clients performing task. */
            requested_group->state = TASK_EXECUTION_IN_PROGRESS;
        }
    }
    FREE_INCOMING_PDU(pdu->msg);
}

void fill_thread_args(client_information_t * client_info, 
                      perform_task_req_t * perform_task, 
                      thread_args * args, int index)
{
        args->client_info=client_info;
        args->task_id = perform_task->task_id;

        get_client_grp_node_by_group_name(&client_info, perform_task->group_name, &args->group);

        ALLOC_AND_COPY_STRING(perform_task->task_filename[index].str, args->task_filename);
        ALLOC_AND_COPY_STRING(perform_task->task_folder_path, args->task_folder_path);
 
        args->task_type = perform_task->task_type;
}
