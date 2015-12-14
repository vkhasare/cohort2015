#include "common.h"
#include "server_DS.h"
#include "RBT.h"
#include "logg.h"

grname_ip_mapping_t * mapping = NULL;
extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
unsigned int num_groups = 0; // should be removed in future, when we remove mapping array and completely migrate on LL.
extern debug_mode;

bool mask_modification_in_progress = false;

const int MAX_CLIENTS_TRIED_PER_ATTEMPT = 3;
const int MAX_ALLOWED_KA_MISSES = 5;

static int handle_echo_req(const int, pdu_t *pdu, ...);
static int handle_echo_response(const int sockfd, pdu_t *pdu, ...);
static int handle_join_req(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_req(const int sockfd, pdu_t *pdu, ...);
static int handle_moderator_notify_response(const int sockfd, pdu_t *pdu, ...);
static int handle_moderator_task_response(const int sockfd, pdu_t *pdu, ...);
static int handle_checkpoint_req(const int sockfd, pdu_t *pdu, ...);
static void assign_task(server_information_t *, char *, int, char *);
static void moderator_selection(server_information_t *, mcast_group_node_t *);

bool update_moderator_info(server_information_t *server_info,
                           mcast_group_node_t *group_node, 
                           unsigned int clientID);

void mask_server_signals(bool flag);

void handle_timeout(int signal, siginfo_t *si, void *uc);

void handle_timeout_real(bool init, int signal, siginfo_t *si,
                         server_information_t **server_info);

void retransmit_task_req_for_client(server_information_t *server_info,
                                    mcast_group_node_t *grp_node);

#define MASK_SERVER_SIGNALS(flag) \
    mask_modification_in_progress = true; \
    mask_server_signals(flag); \
    mask_modification_in_progress = false;

void mask_server_signals(bool flag)
{
    if(flag)
    {
        signal(MOD_SEL_TIMEOUT, SIG_IGN);
        signal(MOD_RSP_TIMEOUT, SIG_IGN);
    }
    else
    {
        struct sigaction sa;

        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = handle_timeout;
        sigemptyset(&sa.sa_mask);
        
        if (sigaction(MOD_SEL_TIMEOUT, &sa, NULL) == -1)
            errExit("sigaction (MOD_SEL_TIMEOUT)");
        
        if (sigaction(MOD_RSP_TIMEOUT, &sa, NULL) == -1)
            errExit("sigaction (MOD_RSP_TIMEOUT)");
    }
    
    /* For now commenting out sigproc related handling and sticking to signal().
       sigprocmask needs more study and stands as TBD. With all probability,
       sigprocmask is not what we need. we just want to ignore signals for
       duration of handler. signal() will serve purpose. This is to be examined later.

       sigset_t mask;
       sigset_t orig_mask;
       struct sigaction act;
   
       sigemptyset (&mask);
       sigaddset (&mask, MOD_SEL_TIMEOUT);
       sigaddset (&mask, MOD_RSP_TIMEOUT);
   
       if (flag)
       {
           if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
               perror ("sigprocmask");
               return;
           }
       }
       else
       {
           if (sigprocmask(SIG_UNBLOCK, &mask, &orig_mask) < 0) {
               perror ("sigprocmask");
               return;
           }
       }
    
     */
}

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
    case echo_response:
        func_name = handle_echo_response;
        break;
    case moderator_notify_rsp:
        func_name = handle_moderator_notify_response;
        break;
    case task_response:
        func_name = handle_moderator_task_response;
        break;
    case checkpoint_req:
        func_name = handle_checkpoint_req;
        break;
    default:
        PRINT("Invalid msg type of type - %d.", msgType);
        func_name = NULL;
  }

  LOGGING_INFO("Message of type %d is received.", msgType);
  return func_name;
}

/* <doc>
 * void send_new_server_info_to_all_groups(server_information_t *server_info)
 * This functions sends update to all the groups about new server IP.
 *
 * </doc>
 */
void send_new_server_info_to_all_groups(server_information_t *server_info)
{
    mcast_group_node_t *group_node = NULL;
    pdu_t pdu;
    comm_struct_t *msg = &(pdu.msg);
    msg->id = new_server_notify;
    new_server_notify_t *new_server_msg = &(msg->idv.new_server_notify);

    /*Fill New server ID in new_server_notify message*/
    new_server_msg->new_server_id = calc_key((struct sockaddr*) &server_info->secondary_server);

    group_node =     SN_LIST_MEMBER_HEAD(&(server_info->server_list->group_node),
                                           mcast_group_node_t,
                                           list_element);

    /*Send new_server_notify to all the multicast groups*/
    while (group_node)
    {
        PRINT("[New_Server_Notify] Sent new Server info to %s", group_node->group_name);
        LOGGING_INFO("New server info (server id - %u) sent to group %s", new_server_msg->new_server_id, group_node->group_name);
        write_record(server_info->server_fd, &group_node->grp_mcast_addr, &pdu);

        group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                             mcast_group_node_t,
                                             list_element);
    }
}

/* <doc>
 * void send_checkpoint(unsigned int chkpoint_type, server_information_t *server_info, struct sockaddr_in *clientAddr, char *group_name, int capability)
 * This functions takes arguments as
 *    - checkpoint type - ADD_CHECKPOINT/DEL_CHECKPOINT
 *    - client address
 *    - Client's group Name
 *    - Client's capability
 * This function sends above checkpointed information to backup server.
 * </doc>
 */
void send_checkpoint(unsigned int chkpoint_type, server_information_t *server_info, struct sockaddr_in *clientAddr, char *group_name, int capability)
{
             pdu_t chkpoint_pdu;
             comm_struct_t *cpmsg = &(chkpoint_pdu.msg);
             cpmsg->id = checkpoint_req;
             checkpoint_req_t *chkpoint_msg = &(cpmsg->idv.checkpoint_req);

             chkpoint_msg->chkpoint_type = chkpoint_type;
             chkpoint_msg->capability = capability;

             chkpoint_msg->group_ips = (l_saddr_in_t *) calloc (1, sizeof(l_saddr_in_t));
             chkpoint_msg->group_ips[0].sin_family =
                     clientAddr->sin_family;
             chkpoint_msg->group_ips[0].sin_port   =
                     clientAddr->sin_port;
             chkpoint_msg->group_ips[0].s_addr     =
                     clientAddr->sin_addr.s_addr;
            chkpoint_msg->group_ips[0].group_name = MALLOC_STR;
            strcpy(chkpoint_msg->group_ips[0].group_name, group_name);
            chkpoint_msg->num_groups = 1;

            LOGGING_INFO("Sent checkpoint of type %d to group %s for client id - %u", chkpoint_msg->chkpoint_type, group_name, clientAddr->sin_addr.s_addr);

            /*Send checkpoint msg to backup server*/
            write_record(server_info->server_fd, &server_info->secondary_server, &chkpoint_pdu);

}

/* <doc>
 * void insert_client_info_in_server_DS(server_information_t *server_info, mcast_group_node_t *group_node,
 *                                      struct sockaddr_in *clientAddr, int capability)
 * Function to add client information in group data structure of Server.
 *
 * </doc>
 */
void insert_client_info_in_server_DS(server_information_t *server_info, mcast_group_node_t *group_node,
                                     struct sockaddr_in *clientAddr, int capability)
{
        RBT_tree *tree = NULL;
        RBT_node *newNode = NULL;
        unsigned int clientID;

        if (group_node) {
             /*As client joins a group, re-adjust capability of group*/
             group_node->grp_capability += capability;

             clientID = calc_key((struct sockaddr*) clientAddr);

             /*Purge the client if already existing in the group list*/
             remove_client_from_mcast_group_node(&server_info, group_node, clientID);
             /*Add client in group link list*/
             ADD_CLIENT_IN_GROUP(&group_node, (struct sockaddr*) clientAddr, clientID);

             LOGGING_INFO("Adding client in group %s, client id - %u with capability %u", group_node->group_name, clientID , capability);
             LOGGING_INFO("Group %s total capability upgraded to %u", group_node->group_name, group_node->grp_capability);
             /*Add in Client RBT*/
             tree = (RBT_tree*) server_info->client_RBT_head;

             if (clientID > 0) {
                 /*search the RB node by clientID*/
                 newNode = RBFindNodeByID(tree, clientID);
                 /* Add in RBT if it is a new client, else update the grp list of client */
                 if (!newNode) {
                     newNode = RBTreeInsert(tree, clientID, (struct sockaddr*) clientAddr, 0, RB_FREE, group_node, capability);
                 } else {
                    /* Update group_list of the client node */
                    rb_info_t *rb_info_list = (rb_info_t*) newNode->client_grp_list;
                    rb_cl_grp_node_t *rb_grp_node = allocate_rb_cl_node(&rb_info_list);
                    /* storing grp node pointer in RB group list*/
                    rb_grp_node->grp_addr = group_node;

                    LOGGING_INFO("Client %u already present in RB Tree. Adding group node (group - %s) in list for it", clientID, group_node->group_name);

                    /* Uncomment to display rb group list for client
                     * display_rb_group_list(&rb_info_list);
                     */
                 }

                /*Uncomment for printing RBT node keys
                 *RBTreePrint(tree);
                 */

                 /*If backup server is available, then send checkpoint information*/
                 if (server_info->is_stdby_available) {
                    LOGGING_INFO("Sent add checkpoint to backup server for client %u and group %s", clientID, group_node->group_name);
                    send_checkpoint(ADD_CHECKPOINT, server_info, clientAddr, group_node->group_name, capability);
                 }
             }
        }
}

/* <doc>
 * bool remove_client_info_from_server_DS(server_information_t *server_info, struct sockaddr_in *clientAddr, char *group_name)
 * Function to remove client information from group data structure of Server.
 *
 * </doc>
 */
bool remove_client_info_from_server_DS(server_information_t *server_info, struct sockaddr_in *clientAddr, char *group_name)
{
      RBT_tree *tree = NULL;
      RBT_node *rbNode = NULL;
      unsigned int clientID;


      clientID = calc_key((struct sockaddr*) clientAddr);

      if (clientID > 0) {
        tree = (RBT_tree*) server_info->client_RBT_head;
        rbNode = RBFindNodeByID(tree, clientID);

        if (rbNode) {
            mcast_group_node_t *ll_grp_node;
            rb_info_t *rb_info_list = (rb_info_t*) rbNode->client_grp_list;

            /*remove node from rbnode list.*/
            if (remove_rb_grp_node(&rb_info_list,group_name, &ll_grp_node)) {
                /*As client leaves a group, re-adjust capability of group*/
                ll_grp_node->grp_capability -= rbNode->capability;

                LOGGING_INFO("Group %s total capability downgraded to %u", group_name, ll_grp_node->grp_capability);
                /*remove node from ll list*/
                remove_client_from_mcast_group_node(&server_info, ll_grp_node, clientID);

                LOGGING_INFO("Removed client %u from group node %s, having client capability %u", clientID, group_name, rbNode->capability);

                /*Remove RBnode from tree if group count is zero.*/
                if (SN_LIST_LENGTH(&rb_info_list->cl_list->group_node) == 0) {
                  RBDelete(tree, rbNode);
                  LOGGING_INFO("Removed the client node %u from RB Tree as it is part of no group",clientID);
                }

               /* Uncomment to display rb group list for client
                *   display_rb_group_list(&rb_info_list);
                */
               /*If backup server is available, then send checkpoint information*/
               if (server_info->is_stdby_available) {
                  LOGGING_INFO("Sent del checkpoint to backup server for client %u and group %s", clientID, group_name);
                  send_checkpoint(DEL_CHECKPOINT, server_info, clientAddr, group_name, rbNode->capability);
               }

                return true;
             }
         }
      }

 return false;
}


/* <doc>
 * static
 * int handle_checkpoint_req(const int sockfd, pdu_t *pdu, ...)
 * This function is for backup server where it handles the 
 * checkpoint request sent by primary server.
 * Depending upon checkpoint type - Add/Del, it updates its 
 * data structures.
 *
 * </doc>
 */
static
int handle_checkpoint_req(const int sockfd, pdu_t *pdu, ...)
{
    struct sockaddr_in client_ip;
    int iter;
    char* group_name;
    chkpoint_type chkpoint_type;
    unsigned int capability;
    mcast_group_node_t *group_node = NULL;

    comm_struct_t *chkpoint_msg = &(pdu->msg);

    server_information_t *server_info = NULL;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    checkpoint_req_t chkpoint_req = chkpoint_msg->idv.checkpoint_req;

    chkpoint_type = chkpoint_req.chkpoint_type;
    capability = chkpoint_req.capability;

    group_name = chkpoint_req.group_ips[0].group_name;
    client_ip.sin_family = chkpoint_req.group_ips[0].sin_family;
    client_ip.sin_port = chkpoint_req.group_ips[0].sin_port;
    client_ip.sin_addr.s_addr = chkpoint_req.group_ips[0].s_addr;
    memset(&client_ip.sin_zero,0,sizeof(client_ip.sin_zero));
 
    if (chkpoint_type == ADD_CHECKPOINT)
    {
        /*Add into the data structure*/
        get_group_node_by_name(&server_info, group_name, &group_node);
        insert_client_info_in_server_DS(server_info, group_node, (struct sockaddr_in *) &client_ip, capability);
        LOGGING_INFO("Handling add checkpoint for group - %s from active server", group_name);
     }
     else if (chkpoint_type == DEL_CHECKPOINT)
     {
        /*Remove from data structure*/
        remove_client_info_from_server_DS(server_info, (struct sockaddr_in *) &client_ip, group_name);
        LOGGING_INFO("Handling del checkpoint for group - %s from active server", group_name);
     }

     /*If backup server also have back server configured, then send checkpointed information to it as well.*/
     if (server_info->is_stdby_available) {
       send_checkpoint(chkpoint_type, server_info, (struct sockaddr_in*) &client_ip, group_name, capability);
     }

     FREE_INCOMING_PDU(pdu->msg);
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
    comm_struct_t *req = &pdu->msg;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause = REJECTED;
    char ipaddr[INET6_ADDRSTRLEN];
    server_information_t *server_info = NULL;
    unsigned int clientID = 0;
    pdu_t rsp_pdu;
    comm_struct_t *resp = &rsp_pdu.msg;
    bool delete_status = false;

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    leave_req_t leave_req = req->idv.leave_req;
    leave_rsp_t *leave_rsp = &resp->idv.leave_rsp;

    resp->id = leave_response;
    leave_rsp->num_groups = 1;
    leave_rsp->group_ids = MALLOC_STR_IE(1);
    
    group_name = leave_req.group_ids[0].str;

    clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);

    PRINT("[Leave_Request: GRP - %s, Client - %s] Leave Request Received.", group_name, ipaddr);

    delete_status = remove_client_info_from_server_DS(server_info, (struct sockaddr_in *) &pdu->peer_addr, group_name);

    if (delete_status)
    {
       cause = ACCEPTED;
    }
 
    leave_rsp->group_ids[0].str = MALLOC_STR;
    strcpy(leave_rsp->group_ids[0].str,group_name);

    leave_rsp->cause = cause;

    PRINT("[Leave_Response: GRP - %s, Client - %s] Cause: %s.",group_name, ipaddr, enum_to_str(cause));
    LOGGING_INFO("Sending leave response for group %s to client %s with cause %s", group_name, ipaddr, enum_to_str(cause));

    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    FREE_INCOMING_PDU(pdu->msg);
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
    comm_struct_t *req = &(pdu->msg);
    uint8_t cl_iter, s_iter, i = 0;
    char *group_name;
    msg_cause cause;
    server_information_t *server_info = NULL;
    unsigned int clientID = 0, capability = 1;
    char ipaddr[INET6_ADDRSTRLEN];
    RBT_tree *tree = NULL;
    RBT_node *newNode = NULL;
    mcast_group_node_t *group_node = NULL;
    pdu_t rsp_pdu;
    comm_struct_t *resp = &(rsp_pdu.msg);

    /* Extracting server_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    join_req_t join_req = req->idv.join_req;   
    join_rsp_t *join_rsp = &(resp->idv.join_rsp);

    resp->id = join_response;
    join_rsp->num_groups = join_req.num_groups;

    capability = join_req.capability;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);

    /* Initializing is must for xdr msg. */
    join_rsp->group_ips = (l_saddr_in_t *) calloc (join_req.num_groups, sizeof(l_saddr_in_t));
    
    for(cl_iter = 0; cl_iter < join_req.num_groups; cl_iter++)
    {
        cause = REJECTED;
        group_name = join_req.group_ids[cl_iter].str;

        PRINT("[Join_Request: GRP - %s, Client - %s] Join Request Received.", group_name, ipaddr);

        /*search group_node in LL by group name*/
        get_group_node_by_name(&server_info, group_name, &group_node);

        if (group_node) 
        {
             cause = ACCEPTED;

             /*Add into Data Structure*/
             insert_client_info_in_server_DS(server_info, group_node, (struct sockaddr_in*) &pdu->peer_addr, capability);

             /* Add ip addr as response and break to search for next group. */
             join_rsp->group_ips[cl_iter].sin_family =
                     group_node->grp_mcast_addr.sin_family;
             join_rsp->group_ips[cl_iter].sin_port   =
                     group_node->grp_mcast_addr.sin_port;
             join_rsp->group_ips[cl_iter].s_addr     =
                     group_node->grp_mcast_addr.sin_addr.s_addr;
             join_rsp->group_ips[cl_iter].grp_port   =
                    group_node->group_port;
        }

         /* cause is ACCEPTED if group is valid, otherwise REJECTED. */
        join_rsp->group_ips[cl_iter].group_name = MALLOC_STR;
        strcpy(join_rsp->group_ips[cl_iter].group_name, group_name);

        join_rsp->group_ips[cl_iter].cause =  cause;

        PRINT("[Join_Response: GRP - %s, Client: %s] Cause: %s.", group_name, ipaddr, enum_to_str(cause));
        LOGGING_INFO("Sending join response for group %s to client %s with cause %s", group_name, ipaddr, enum_to_str(cause));
    }

    FREE_INCOMING_PDU(pdu->msg);
 
    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

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
    for(i = 0; i < count; i++)
    {
        fscanf(fp, "%s %s %s", (*mapping)[i].grname, ip_str,port);
        /* Storing group IP in sockaddr_in struct */
        (*mapping)[i].grp_ip.sin_family = AF_INET;
        (*mapping)[i].grp_ip.sin_port = htons(atoi(port));
        (*mapping)[i].grp_ip.sin_addr.s_addr = inet_addr(ip_str);
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
  display_mcast_group_node(&server_info, SHOW_ALL);
}

void mark_dead_clients_in_group(int* ref_array, int* ref_count,
        int* dead_client_arr, int dead_client_count)
{
    //Attempt to find all dead clients first and mark them.
    int i, j, num_clients_marked = 0;
    for(i = 0; i < *ref_count && 
               num_clients_marked < dead_client_count; i++)
        for(j = 0; j < dead_client_count; j++)
        {
            if(ref_array[i] == dead_client_arr[j])
            {
                ref_array[i] = 0;
                num_clients_marked++;
                break;
            }
        }
    //compress array in second step;
    i = 0; //serves as current index in ref_array
    j = *ref_count - 1; //runs from end of array to find non zero entry.
    
    while(num_clients_marked-- > 0 && (i < (*ref_count - num_clients_marked)))
    {
        //move i to find first 0 entry.
        while((ref_array[i] != 0) && (i < *ref_count)) i++;
        
        //move j backwards till we cross i or we get a non zero entry in array.
        while(ref_array[j] == 0)
        {
            if(j == i) break;
            j--;
            continue;
        }
        ref_array[i] = ref_array[j];
        ref_array[j] = 0;
    }

    //modify ref count to reflect number of clients presently working;
    *ref_count -= dead_client_count;
}

/* <doc>
 * void retransmit_task_req_for_client(server_information_t *server_info,
 *                                     mcast_group_node_t *grp_node)
 * This functions resends the task request with information of dead
 * clients and instructs group to work on those task sets.
 *
 * </doc>
 */
void retransmit_task_req_for_client(server_information_t *server_info,
                                    mcast_group_node_t *grp_node)
{
    comm_struct_t req;
    pdu_t req_pdu;
    int j = 0, index;

    req.idv.perform_task_req.group_name= MALLOC_CHAR(strlen(grp_node->group_name)+1);
    strcpy(req.idv.perform_task_req.group_name, grp_node->group_name);
    req.idv.perform_task_req.task_id = server_info->task_id;

    PRINT("Re-sending task request for client - %u , group %s, task id - %d", grp_node->dead_clients[0], grp_node->group_name, server_info->task_id);
    LOGGING_INFO("Re-sending task request for client - %u , group %s, task id - %d", grp_node->dead_clients[0], grp_node->group_name, server_info->task_id);

    req.id = perform_task_req;

    req.idv.perform_task_req.client_id_count = grp_node->dead_client_count;
    req.idv.perform_task_req.client_ids = MALLOC_UINT(1);

    req.idv.perform_task_req.client_ids[0] = grp_node->task_set_details.working_clients[grp_node->task_set_details.number_of_working_clients - 1];

    for ( j = 0; j < grp_node->task_set_details.number_of_working_clients; j++)
    {
       if (grp_node->task_set_details.working_clients[j] == grp_node->dead_clients[0])
       {
          break;
       }
    }

    PRINT("SENDING %s", grp_node->task_set_details.task_filename[j]);

    req.idv.perform_task_req.task_filename = malloc(sizeof(string_t *)*grp_node->dead_client_count);

    for(index = 0;index < grp_node->dead_client_count; index++){
       req.idv.perform_task_req.task_filename[index].str = MALLOC_CHAR(strlen(grp_node->task_set_details.task_filename[j])+1);
       strcpy(req.idv.perform_task_req.task_filename[index].str, grp_node->task_set_details.task_filename[j]);
    }
    
    req.idv.perform_task_req.task_folder_path = MALLOC_CHAR(strlen(grp_node->task_set_details.task_folder_path)+1);
    strcpy(req.idv.perform_task_req.task_folder_path , grp_node->task_set_details.task_folder_path);
    
    req.idv.perform_task_req.task_type = grp_node->task_type;
    req.idv.perform_task_req.retransmitted = 1;

    req_pdu.msg = req;

    /* Send multicast msg to group to perform task */
    write_record(server_info->server_fd,&(grp_node->grp_mcast_addr), &req_pdu);

    grp_node->task_set_details.number_of_working_clients += 1;

    //remove it from groups' active working list
    for (index = 0; index < grp_node->dead_client_count; index++)
    {
      mark_dead_clients_in_group(grp_node->task_set_details.working_clients,
                                 &grp_node->task_set_details.number_of_working_clients, &grp_node->dead_clients[index], 1);
    }

    free(grp_node->dead_clients);
}


/* <doc>
 * void server_echo_req_task_in_progress_state(server_information_t *server_info,
 *                                             void *fsm_msg)
 * When a multicast group is busy in performing a task and it gets periodic echo
 * requests from the moderators (client), so as to make sure that moderator is
 * alive.
 *
 * </doc>
 */
void server_echo_req_task_in_progress_state(server_information_t *server_info,
                                            void *fsm_msg)
{
  mcast_client_node_t *client_node = NULL;
  RBT_tree *tree = NULL;
  RBT_node *rbNode = NULL;
  fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;

  /* fetch the group node pointer from fsm_data */
  mcast_group_node_t *group_node = fsm_data->grp_node_ptr;
  
  pdu_t *pdu = (pdu_t *) fsm_data->pdu;

  group_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
  //PRINT("num: %d name:%s", group_node->heartbeat_remaining, group_node->group_name);
  echo_req_t *echo_req = &pdu->msg.idv.echo_req;
  if(echo_req->num_clients)
  {
      uint8_t iter;
      for(iter = 0; iter < echo_req->num_clients; iter++)
      {
          char ipaddr[INET6_ADDRSTRLEN];
          struct sockaddr_in cl_addr;
          cl_addr.sin_addr.s_addr = echo_req->client_ids[iter]; 
          inet_ntop(AF_INET, 
                  get_in_addr((struct sockaddr *)&cl_addr), 
                  ipaddr, INET6_ADDRSTRLEN);
          PRINT("[UNREACHABLE_ALERT: GRP - %s] Client (%s) is unreachable. ", group_node->group_name, ipaddr);
          LOGGING_WARNING("Group %s: Client %s with client id %u is down.", group_node->group_name, ipaddr , echo_req->client_ids[iter]);

          /*As client went down, re-adjust the group capability*/
          tree = (RBT_tree*) server_info->client_RBT_head;
          rbNode = RBFindNodeByID(tree, echo_req->client_ids[iter]);
          group_node->grp_capability -= rbNode->capability;
      }
      mark_dead_clients_in_group(group_node->task_set_details.working_clients, 
                                 &group_node->task_set_details.number_of_working_clients, 
                                 echo_req->client_ids, echo_req->num_clients);
  }
  else
  {
      PRINT("[INFO] All clients are up and executing task.");
      LOGGING_WARNING("All clients are up and executing task");
  }
}

/* <doc>
 * void handle_timeout_real(bool init, int signal, siginfo_t *si,
 *                          server_information_t **server_info)
 * This is the timeout handler function on server when the
 * timer expires.
 *
 * </doc>
 */
void handle_timeout_real(bool init, int signal, siginfo_t *si,
                         server_information_t **server_info)
{
    static server_information_t **server_info_local;
    mcast_group_node_t *grp_node = NULL;

    if(init)
    {
        /* Context restoration is possible in very limited context with signals. In order
         * to step around this problem we are calling this routine in two stages. In first
         * leg (init), we push/preserve address of DS that serves as lifeline throughout life
         * of application. In subsequent timer hits, where this info will be unavailable,
         * we use context stored at init time to perform the task. */
        
        server_info_local = server_info;
        return;
    }
    else
    {
        if(mask_modification_in_progress == true)
            return; 

        //traverse the list to find which group timer expired
        get_group_node_by_timerid(server_info_local, 
                                  si->si_value.sival_ptr, 
                                  &grp_node);
        if(grp_node == NULL)
        {
            /* A group was removed but timer is still running. Log unusual error.
             * DELETE. Not stop this timer. We want to release such timers. */

            timer_delete(*(timer_t*)(si->si_value.sival_ptr));
            return;
        }
        
        if(signal == MOD_SEL_TIMEOUT)
        {
            if(grp_node->fsm_state == MODERATOR_SELECTED)
                /* XXX This should not happen now since we are attempting to
                 * ignore all signals when incoming packets come and are being
                 * processed. This should be a very very rare scenario that
                 * mask_modification_in_progress flag did not get a chance to
                 * get set. Asserting to determine frequency of these incidents. */
                assert(0);
            // XXX XXX XXX XXX revisit. fired when all clients died on handle_mod_notification.

            //new addition
            /* timer delete not required here. Nor is stop_timer. Because if it
             * came here it most certainly came via one-shot timer expiration.
             * Just rearming of timer required. */
            ////timer_delete(grp_node->timer_id);
            
            moderator_selection(*server_info_local, grp_node);
        }
        else if(signal == MOD_RSP_TIMEOUT)
        {
            //decrement counter and check if moderator has missed n successive heartbeat
            if(grp_node->heartbeat_remaining-- > 0) 
                return;

            if (grp_node->fsm_state == MODERATOR_SELECTED)
            {
                /* This means we were waiting for response from moderator
                 * about active clients in the group but moderator node went
                 * down in between. In this case we should re-trigger
                 * moderator selection. */

                //new addition
                timer_delete(grp_node->timer_id);
                grp_node->timer_id = 0;

                moderator_selection(*server_info_local, grp_node);
            }
            else if(grp_node->fsm_state == GROUP_TASK_IN_PROGRESS)
            {
                char ipaddr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET, 
                        get_in_addr(&(grp_node->moderator_client->client_addr)), 
                        ipaddr, INET6_ADDRSTRLEN);
                PRINT("[UNREACHABLE_ALERT: GRP - %s] Moderator (%s) is unreachable. ", grp_node->group_name, ipaddr);

                //stop_timer(&grp_node->timer_id);
                timer_delete(grp_node->timer_id);
                grp_node->timer_id = 0;

                /*Resetting old moderator/client info*/
                unsigned int clientID = grp_node->moderator_client->client_id;
                RBT_tree *tree = (RBT_tree*) (*server_info_local)->client_RBT_head;
                RBT_node *rbNode = RBFindNodeByID(tree, clientID);
                rbNode->av_status = RB_FREE;
                rbNode->is_moderator = FALSE;

                LOGGING_WARNING("Moderator %s went down for group %s. Initiating Moderator re-selection." , ipaddr, grp_node->group_name);

                /*As client went down, re-adjust the group capability*/
                grp_node->grp_capability -= rbNode->capability;

                /*Initiate new moderator selection process*/ 
                grp_node->fsm_state = TASK_IN_PROGRESS_MOD_SEL_PEND;
                moderator_selection(*server_info_local, grp_node);

                /*Copying dead clients, so that task assigned to dead clients can be re-assigned once we have new moderator information*/
                grp_node->dead_client_count = 1;
                grp_node->dead_clients = malloc(sizeof(unsigned int) * 1);
//                memcpy(*grp_node->dead_clients, &clientID, sizeof(unsigned int) * grp_node->dead_client_count);
                grp_node->dead_clients[0] = clientID;
            }
            else
            {
                /* XXX This timer is not expected to hit in any other state. Investigate. */
                assert(0);
            }
            //PRINT("***came here for decrementing: %d***", grp_node->heartbeat_remaining);
        }
    }
    
}

void handle_timeout(int signal, siginfo_t *si, void *uc)
{
    if(si == NULL)
        PRINT("***yikes!***");
    handle_timeout_real(false, signal, si, NULL);
}


/* <doc>
 * void send_new_moderator_info(server_information_t *server_info,
 *                              void *fsm_msg)
 * This sends the moderator update req to the multicast group to
 * update them about the new moderator selected for the group.
 *
 * </doc>
 */
void send_new_moderator_info(server_information_t *server_info,
                             void *fsm_msg)
{
    char ipaddr[INET6_ADDRSTRLEN];
    fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;
    
    /* Disarm initial MOD_SEL_TIMEOUT oneshot at this stage since hitting this
     * path means at least one client has responded in mod reselection phase. */
    //stop_timer(&group_node->timer_id);
    /* XXX XXX XXX start task resp related timer. chk if it is missing. */

    if(group_node->timer_id != 0){ 
      timer_delete(group_node->timer_id);
      group_node->timer_id = 0;}

    /* Changing the fsm state as moderator has been selected */
    group_node->fsm_state = GROUP_TASK_IN_PROGRESS;
    
    pdu_t *pdu = (pdu_t *) fsm_data->pdu;

    /*Marking client node as Moderator*/
    unsigned int clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    if (clientID <= 0){
        return;
    }
    
    if(/*NOT*/ !update_moderator_info(server_info, group_node, clientID))
    {
        /* Some problem in lookup. Log error and return*/
        return;
    }

    /* Changing the fsm state as moderator has been selected */
    group_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("-------------[NOTIFICATION] NEW MODERATOR - Client %s is selected for group %s  ---------------", ipaddr, group_node->group_name);

    LOGGING_INFO("New moderator - Client %s is selected for group %s ", ipaddr, group_node->group_name);

    /*Create the moderator notification request and inform the multicast group about the Group Moderator*/
    pdu_t mod_update_pdu;
    comm_struct_t *req = &(mod_update_pdu.msg);
    moderator_update_req_t *mod_update_req = &(req->idv.moderator_update_req);

    struct sockaddr_in *addr_in = (struct sockaddr_in *) &pdu->peer_addr;

    req->id = moderator_update_req;
    mod_update_req->moderator_id = clientID;
    mod_update_req->moderator_port = addr_in->sin_port;
    mod_update_req->group_name = MALLOC_CHAR(strlen(group_node->group_name)+1);
    strcpy(mod_update_req->group_name, group_node->group_name);

    mod_update_req->client_id_count = group_node->task_set_details.number_of_working_clients;
    mod_update_req->client_ids = MALLOC_UINT(mod_update_req->client_id_count);

    memcpy(mod_update_req->client_ids, group_node->task_set_details.working_clients, 
           group_node->task_set_details.number_of_working_clients * sizeof(group_node->task_set_details.working_clients[0]));

    PRINT("[Moderator_Update_Req: GRP - %s] Moderator Update Request sent to group %s.", mod_update_req->group_name , mod_update_req->group_name);

    /*Send to multicast group*/
    write_record(server_info->server_fd, &(group_node->grp_mcast_addr), &mod_update_pdu);
 
    /* Start recurring timer for monitoring status of this moderator node.*/
    start_recurring_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, MOD_RSP_TIMEOUT);

    /*Once the new moderator is select, distribute moderator's task to some other client*/
    retransmit_task_req_for_client(server_info, group_node);
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
    server_information_t *server_info = NULL;
    mcast_group_node_t *group_node = NULL;
    fsm_data_t fsm_msg;
    echo_req_t echo_req = req->idv.echo_req;

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    rsp->id = echo_response;
    echo_rsp_t *echo_response = &(rsp->idv.echo_resp);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Request: GRP - %s] Echo Request received from %s", echo_req.group_name, ipaddr);

    LOGGING_INFO("Echo Request received from %s for group %s", ipaddr, echo_req.group_name);

    get_group_node_by_name(&server_info, echo_req.group_name, &group_node);
    
    /*filling echo rsp pdu*/
    echo_response->status    = 11;
    echo_response->group_name = MALLOC_STR;
    strcpy(echo_response->group_name, echo_req.group_name);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Response: GRP - %s] Echo Response sent to %s", echo_response->group_name, ipaddr);

    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    /*Check the fsm of group node and act accordingly.*/
    get_group_node_by_name(&server_info, echo_req.group_name, &group_node);

    fsm_msg.fsm_state = group_node->fsm_state;
    fsm_msg.grp_node_ptr = group_node;
    fsm_msg.pdu = pdu;

    /* Run the server fsm*/
    server_info->fsm(server_info, ECHO_REQ_RCVD_EVENT, &fsm_msg);

    FREE_INCOMING_PDU(pdu->msg);

    return 0;
}


/* <doc>
 * void create_task_sets_per_client(mcast_group_node_t *group_node,unsigned int *client_ids,mcast_task_set_t *task_set,char *memblock)
 * This function creates files per client
 */
void create_task_sets_per_client(mcast_group_node_t *group_node,unsigned int *client_ids,mcast_task_set_t *task_set,char *src,
                                 unsigned int num_of_clients, unsigned int task_count, unsigned int capability_total, char * folder_path)
{
   time_t rawtime;
   struct tm * timeinfo;
   unsigned int i;
   char file_name[100], file_path[200], buffer2[40];
   unsigned int start_index=0, counter = 0, data_count = 0;
   FILE *fptr, *fp;
   char * line[10];
   size_t len = 0;
   ssize_t read;
   char temp[30];
   char *dst;
   int dst_fd;

   for(i = 0; i < num_of_clients; i++)
   {
     /* generate the file name for the client */
     time ( &rawtime );
     timeinfo = localtime ( &rawtime );

     strftime(buffer2,80,"%d%m%y_%H%M%S.txt",timeinfo);
     sprintf(file_name,"%u_%s",client_ids[i], buffer2);
     sprintf(file_path, "%s/%s", folder_path, file_name);
   
     task_set->task_filename[i] = MALLOC_CHAR(strlen(file_name) + 1);
     strcpy(task_set->task_filename[i],file_name);
     
     /* open the file in write mode */
     dst_fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
     if (dst_fd == -1)
       errExit("open");

     
     /* Find the data count to be written to the file for a client based on the task_count and total capability */
     data_count = (task_set->capability[i] * task_count)/ capability_total;
     if( i == num_of_clients - 1 )
       data_count = task_count - start_index;
 
     PRINT("The data count for client %u is %u", i+1, data_count);
     /*write data count into the file. Once written, break from loop.*/
     off_t fsize =data_count*11;

     if (ftruncate(dst_fd, fsize) == -1)
       errExit("ftruncate"); 

     dst = mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, dst_fd, 0);

     if (dst == MAP_FAILED)
       errExit("mmap");

     memcpy(dst, src+start_index, fsize);

     if (msync(dst, fsize, MS_SYNC) == -1)
       errExit("msync");
     //update start_index for next client.
     start_index += data_count;

     munmap(dst, fsize);
   }
}

/* <doc>
 * void get_data_set_for_client_based_on_capability(server_information_t *server_info,
 *                                                  mcast_group_node_t *group_node,
 *                                                  int num_of_clients,
 *                                                  unsigned int *client_ids,
 *                                                  unsigned int task_count)
 * This function divides the data set into multiple files based on client capability
 * </doc>
 */
void get_data_set_for_client_based_on_capability(server_information_t *server_info,
                                                 mcast_group_node_t *group_node,
                                                 int num_of_clients,
                                                 unsigned int *client_ids,
                                                 unsigned int task_count )
{
   RBT_tree *tree = NULL;
   RBT_node *rbNode = NULL;
   struct stat sb; 
   FILE * fp, *cmd_line=NULL;
   char * line = NULL;
   size_t len = 0;
   ssize_t read;
   int num_of_clients_modified, fd, i;
   bool client_list_changed = FALSE;
   char element[16], cmd[256], *memblock;
   unsigned int capability_total = 0;
   char folder_path[100];
   mcast_task_set_t *task_set = &group_node->task_set_details;

   tree = (RBT_tree*) server_info->client_RBT_head;

   if(num_of_clients >= task_count)
   {
     /*In case where task set is smaller as compared to the number of clients, choose few clients from the list */
     client_list_changed = TRUE;
     num_of_clients_modified = task_count/2;

     LOGGING_INFO("Task count is less than number of clients for group %s.", group_node->group_name);
     LOGGING_INFO("New number of working clients will be %d", num_of_clients_modified);
     
     /*Initialise the group node with client list */
     task_set->capability = MALLOC_UINT(num_of_clients_modified);
     task_set->task_filename = (char **) malloc(sizeof(char*) * num_of_clients_modified);
   }
   else
   {
     /*Initialise the group node with client list */
     task_set->capability = MALLOC_UINT(num_of_clients);
     task_set->task_filename = (char **) malloc(sizeof(char*) * num_of_clients);
   }
   
   /* If client list is shorter */
   if(client_list_changed)
     num_of_clients = num_of_clients_modified;
   
   for(i = 0; i < num_of_clients; i++)
   {
     rbNode = RBFindNodeByID(tree, client_ids[i]);

     if(rbNode)
     {
       /* Find the addition ofcapabilities of all the applicable client nodes, to find the percentage */
       capability_total = capability_total + rbNode->capability;
       task_set->capability[i] = rbNode->capability;
     }
   }

   PRINT("Creating the task sets . .. ");
   /* Open the data set file to read */
   fd = open(group_node->task_set_filename, O_RDONLY);
   fstat(fd, &sb);
   
   memblock = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
   if (memblock == MAP_FAILED) printf("mmap"); 

   sprintf(folder_path,"/tmp/server/task_set/%s", group_node->group_name);
   task_set->task_folder_path = MALLOC_CHAR(strlen(folder_path) + 1);
   strcpy(task_set->task_folder_path,folder_path);
   check_and_create_folder(folder_path);
  
 
   /*Create tas set file for each client on which it is required to work.*/
   create_task_sets_per_client( group_node, client_ids, task_set, memblock, num_of_clients, task_count, capability_total, folder_path);

   munmap(memblock, sb.st_size);
}

/* <doc>
 * void mcast_start_task_distribution(server_information_t *server_info,
 *                                    void *fsm_msg)
 * Function to send task request to the multicast group. It takes 
 * input as list of active client IDs (obtained in moderator notify
 * response) and the task type.
 *
 * </doc>
 */
void mcast_start_task_distribution(server_information_t *server_info,
                                   void *fsm_msg)
{
    char ipaddr[INET6_ADDRSTRLEN];
    int count = 0, index;
    comm_struct_t req;
    fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
    pdu_t req_pdu;
    unsigned int task_count;

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

    pdu_t *pdu = (pdu_t *) fsm_data->pdu;

    /* Get the task set for prime numbers */
    task_count = get_task_count(group_node->task_set_filename);
    PRINT("THe task file is %s",group_node->task_set_filename); 

    if(!task_count)
      return;

    req.idv.perform_task_req.group_name= MALLOC_CHAR(strlen(group_node->group_name)+1);
    strcpy(req.idv.perform_task_req.group_name, group_node->group_name);
    req.idv.perform_task_req.task_id = server_info->task_id;

    LOGGING_INFO("Sending task request for group %s, task id - %d", group_node->group_name, server_info->task_id);

    (server_info->task_id)++;  
    
    req.id = perform_task_req;

    /* Fetch the client ID's and copy the task set */
    if (pdu->msg.idv.moderator_notify_rsp.client_id_count)
    {
        req.idv.perform_task_req.client_id_count = pdu->msg.idv.moderator_notify_rsp.client_id_count;
        req.idv.perform_task_req.client_ids = MALLOC_UINT(pdu->msg.idv.moderator_notify_rsp.client_id_count);
    
        memcpy(req.idv.perform_task_req.client_ids,
                pdu->msg.idv.moderator_notify_rsp.client_ids,
                sizeof(req.idv.perform_task_req.client_ids[0]) * pdu->msg.idv.moderator_notify_rsp.client_id_count);
    }
    
    /* Fetch the data set for each client based on capability */
    get_data_set_for_client_based_on_capability(server_info, group_node,req.idv.perform_task_req.client_id_count,req.idv.perform_task_req.client_ids, task_count);

    /*Copy list of working clients in group node*/
    group_node->task_set_details.number_of_working_clients = pdu->msg.idv.moderator_notify_rsp.client_id_count;

    group_node->task_set_details.working_clients = MALLOC_UINT(group_node->task_set_details.number_of_working_clients);
    memcpy(group_node->task_set_details.working_clients,
           pdu->msg.idv.moderator_notify_rsp.client_ids,
           group_node->task_set_details.number_of_working_clients * (sizeof(pdu->msg.idv.moderator_notify_rsp.client_ids[0])));

    req.idv.perform_task_req.task_filename = malloc(sizeof(string_t *)*group_node->task_set_details.number_of_working_clients);
    for(index=0;index < group_node->task_set_details.number_of_working_clients; index++){
       req.idv.perform_task_req.task_filename[index].str = MALLOC_CHAR(strlen(group_node->task_set_details.task_filename[index])+1);
       strcpy(req.idv.perform_task_req.task_filename[index].str, group_node->task_set_details.task_filename[index]);
    }
    req.idv.perform_task_req.task_folder_path = MALLOC_CHAR(strlen(group_node->task_set_details.task_folder_path)+1);
    strcpy(req.idv.perform_task_req.task_folder_path , group_node->task_set_details.task_folder_path);
//    req.idv.perform_task_req.task_count = task_count;
//    req.idv.perform_task_req.task_set = MALLOC_UINT(task_count);

/*    for(count = 0; count < task_count; count++)
    {
        req.idv.perform_task_req.task_set[count] = task_set[count];
    }
    // USE THIS INSTEAD OF FOR
    memcpy(req.idv.perform_task_req.task_set, task_set,
           sizeof(req.idv.perform_task_req.task_set[0])* task_count); */

    req.idv.perform_task_req.retransmitted = 0;

    req.idv.perform_task_req.task_type = group_node->task_type;
    req_pdu.msg = req;

    /* Send multicast msg to group to perform task */
    write_record(server_info->server_fd,&(group_node->grp_mcast_addr), &req_pdu);

    /* Changing the fsm state as task has been started */
    group_node->fsm_state = GROUP_TASK_IN_PROGRESS;

    PRINT("[Task_Request: GRP - %s] Task Request Sent.", group_node->group_name);

}

void free_thread_args(thread_args * t_args){
   int i;

   free(t_args->source_folder);
   for(i=0;i<t_args->file_count; i++)
     free(t_args->task_filename[i]);
   free(t_args->task_filename);
   free(t_args->dest_folder);
   free(t_args);

}

void* fetch_task_response_files_from_moderator(void *args)
{

    thread_args *t_args = (thread_args *)args;
    /*Making thread detached.*/
    pthread_t self = pthread_self();
    pthread_detach(self);

    unsigned int file_count = t_args->file_count;
    char path[200];
    unsigned int i;
    for(i=0; i<file_count;i++){
       sprintf(path, "%s/%s", t_args->source_folder, t_args->task_filename[i]);
       fetch_file(path, t_args->dest_folder);
    }
    free_thread_args(t_args);
    return NULL;
}



void collect_task_results(task_rsp_t * task_resp, char *result_folder, unsigned int client_id){

   char *src_folder=malloc(sizeof(char)*100);
   struct in_addr ip_addr;
   ip_addr.s_addr = client_id;
   sprintf(src_folder, "%s:/tmp/client/moderator/%s", inet_ntoa(ip_addr), task_resp->group_name);
   int i, pthread_status;
   char * filename;
   int num_clients=task_resp->num_clients;
   thread_args * t_args = malloc(sizeof(thread_args));
   t_args->file_count=num_clients;
   t_args->source_folder = src_folder;
   t_args->task_filename = malloc(sizeof(char *)*num_clients);
   t_args->dest_folder = result_folder;

   for(i=0;i<num_clients;i++){
     filename = basename(task_resp->result[i].str);
     t_args->task_filename[i] = MALLOC_CHAR(strlen(filename)+1);
     strcpy(t_args->task_filename[i], filename);
   }
  
   pthread_t thread; 
   pthread_status = pthread_create(&thread, NULL, fetch_task_response_files_from_moderator ,t_args);
   if(pthread_status)
   {
     PRINT("Could not create thread to perform task");
   }
  
}



/* <doc>
 * void get_task_result_folder_path(char * gname, uint8_t task_id)
 * This function returns the folder where task results are stored
 *
 * </doc>
 */
char * get_task_result_folder_path(char * gname, uint8_t task_id){
     time_t rawtime;
     struct tm * timeinfo;
     char buffer2[40];
     char * buffer = malloc(sizeof(char)*140);

     time ( &rawtime );
     timeinfo = localtime ( &rawtime );
     strftime(buffer2,80,"%d%m%y_%H%M%S",timeinfo);

     sprintf(buffer,"/tmp/server/task_result/%s/", gname);
     strcat(buffer,buffer2);

     create_folder(buffer);

     PRINT("[INFO] The Response from %s for task %d is written in %s",gname, task_id,  buffer);
     LOGGING_INFO("Final response output for group %s , task %d is written in %s", gname, task_id,  buffer);

     return buffer;
}


/* <doc>
 * write_task_response_file(task_rsp_t * task_rsp, char* fname)
 * After getting the task response, this function writes the
 * result in the result file.
 *
 * </doc>
 */ 
/*void
write_task_response_file(task_rsp_t * task_rsp, char* fname)
{
  uint32_t i, j;
  FILE *fp=NULL;
  fp=fopen(fname,"w");
  fprintf(fp, "Client Id, Result Size, Result\n");
  for(i=0;i<task_rsp->num_clients;i++)
  {
    fprintf(fp, "%u", task_rsp->client_ids[i]);
    fprintf(fp, ", %u", task_rsp->result[i].size);
    for(j=0;j<task_rsp->result[i].size;j++)
    {
      fprintf(fp, ", %u", task_rsp->result[i].value[j]);
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
}*/

/* <doc>
 * void mcast_handle_task_response(server_information_t *server_info, void *fsm_msg)
 * Function to handle task response from the multicast group moderator. It takes 
 * input as response collated by the moderator of the group along with the client IDs 
 *  and the task type.
 *
 * </doc>
 */
void mcast_handle_task_response(server_information_t *server_info, void *fsm_msg)
{
    char ipaddr[INET6_ADDRSTRLEN], * result_folder;
    int i = 0;

    int count = 0;
    comm_struct_t req;
    fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
    pdu_t rsp_pdu;
    unsigned int* task_set,task_count;

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

    pdu_t *pdu = (pdu_t *) fsm_data->pdu;
    task_rsp_t *task_response= &(pdu->msg.idv.task_rsp);

    LOGGING_INFO("Task response received from moderator for group %s for task id %d", group_node->group_name, task_response->task_id);

    /* Get the file name based on current time stamp */ 
    result_folder = get_task_result_folder_path(group_node->group_name, task_response->task_id); 

    /* Write the response  to file */  
    collect_task_results(task_response, result_folder, calc_key(&(pdu->peer_addr))); 

    /*Disarming the keepalive for this mod since task is complete.*/
    //stop_timer(&group_node->timer_id);
    timer_delete(group_node->timer_id);
    group_node->timer_id = 0;

    /*Resetting mod_client pointer to avoid havoc in moderator_selection*/
    group_node->moderator_client = NULL;

    mcast_task_set_t * task_set_details = &group_node->task_set_details;
    /*Free if group node has maintained array list of working clients*/
    if (task_set_details->working_clients) {
        free(task_set_details->working_clients);
    }

    /* Free the filename */
    free(group_node->task_set_filename);

    /*Free task related details*/
    free(task_set_details->capability);

    for(i = 0; i < task_set_details->number_of_working_clients; i++)
    {
        free(task_set_details->task_filename[i]);
    }
    free(task_set_details->task_filename);

    task_set_details->number_of_working_clients = 0;
    free(task_set_details->task_folder_path);

    group_node->task_type = INVALID_TASK_TYPE;

    /*Come back to initial state where the cycle started*/
    group_node->fsm_state = STATE_NONE;

  /*Remove the task set file for the group once task response has been received by Server*/
/*  strcpy(cmd,"rm -rf task_set/");
  strcat(cmd,group_node->group_name);
  cmd_line = popen (cmd, "w");
  pclose(cmd_line); */

}

/* <doc>
 * static
 * int handle_moderator_task_response(const int sockfd, pdu_t *pdu, ...)
 * This is the handler function for moderator task response. It
 * reads the responses from all clients and store it in a file.
 * </doc>
 */
static
int handle_moderator_task_response(const int sockfd, pdu_t *pdu, ...)
{
    comm_struct_t *rsp = &(pdu->msg);
    char ipaddr[INET6_ADDRSTRLEN];
    server_information_t *server_info = NULL;
    RBT_tree *tree = NULL;
    RBT_node *rbNode = NULL;
    unsigned int clientID;
    fsm_data_t fsm_msg;

    task_rsp_t *moderator_task_rsp = &(rsp->idv.task_rsp);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Task_Response: GRP - %s] Task Response received from moderator %s", moderator_task_rsp->group_name, ipaddr);

    /* Search for client moderator node by moderator ID in RBTree. Through RBnode, traverse the rb group list
     * to identify the LL group node.
     * Traversing this way is faster than searching the group node in LL. */
    clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    tree = (RBT_tree*) server_info->client_RBT_head;

    if (clientID <= 0) return; 
         
    /*Lookup in RBTree for moderator*/
    rbNode = RBFindNodeByID(tree, clientID);

    if (rbNode == NULL) return;
    
    rb_cl_grp_node_t *rb_grp_node;
    rb_info_t *rb_info_list = (rb_info_t*) rbNode->client_grp_list;

    /*Search for group node in group list of RBnode*/
    if (search_rb_grp_node(&rb_info_list, moderator_task_rsp->group_name, &rb_grp_node)) 
    {
        mcast_group_node_t *ll_grp_node = ((mcast_group_node_t *)rb_grp_node->grp_addr);

        /*Marking the client free again*/
        rbNode->av_status = RB_FREE;
        rbNode->is_moderator = FALSE;

        if (ll_grp_node->moderator_client)
        {
           ll_grp_node->moderator_client->av_status = CLFREE;
           ll_grp_node->moderator_client = NULL;
        }
        else
        {
           mcast_client_node_t *client_node;
           search_client_in_group_node(ll_grp_node, clientID, &client_node);
           if (client_node)
           {
              client_node->av_status = CLFREE;
           }
        }

        fsm_msg.fsm_state = ll_grp_node->fsm_state;
        fsm_msg.grp_node_ptr = (void *) ll_grp_node;
        fsm_msg.pdu = pdu;

        /*Run the server fsm*/
        server_info->fsm(server_info, MOD_TASK_RSP_RCVD_EVENT, &fsm_msg);
    }
    FREE_INCOMING_PDU(pdu->msg);
}

/* <doc>
 * static
 * int handle_moderator_notify_response(const int sockfd, pdu_t *pdu, ...)
 * This is the handler function for moderator notify response. It
 * updates its data structures for all group client related information.
 * </doc>
 */
static
int handle_moderator_notify_response(const int sockfd, pdu_t *pdu, ...)
{
    pdu_t rsp_pdu;
    comm_struct_t *rsp = &(pdu->msg);
    char ipaddr[INET6_ADDRSTRLEN];
    server_information_t *server_info = NULL;
    RBT_tree *tree = NULL;
    RBT_node *rbNode = NULL;
    unsigned int clientID;
    fsm_data_t fsm_msg;

    moderator_notify_rsp_t *moderator_notify_rsp = &(rsp->idv.moderator_notify_rsp);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Moderator_Notify_Rsp: GRP - %s] Moderator Notify Response received from %s", moderator_notify_rsp->group_name, ipaddr);

    LOGGING_INFO("Moderator Notify Response received from %s for group %s", ipaddr , moderator_notify_rsp->group_name);

    /* Search for client moderator node by moderator ID in RBTree. Through RBnode, traverse the rb group list
     * to identify the LL group node.
     * Traversing this way is faster than searching the group node in LL. */
    clientID = moderator_notify_rsp->moderator_id;

    tree = (RBT_tree*) server_info->client_RBT_head;

    if (clientID <= 0) return;
    /*Lookup in RBTree for moderator*/
    rbNode = RBFindNodeByID(tree, clientID);

    if (rbNode == NULL) return;
    rb_cl_grp_node_t *rb_grp_node;
    rb_info_t *rb_info_list = (rb_info_t*) rbNode->client_grp_list;

    /*Search for group node in group list of RBnode*/
    if (search_rb_grp_node(&rb_info_list, moderator_notify_rsp->group_name, &rb_grp_node)) 
    {
        mcast_group_node_t *ll_grp_node = ((mcast_group_node_t *)rb_grp_node->grp_addr);
        fsm_msg.fsm_state = ll_grp_node->fsm_state;
        fsm_msg.grp_node_ptr = (void *) ll_grp_node;
        fsm_msg.pdu = pdu;

        /*Run the server fsm*/
        server_info->fsm(server_info, MOD_NOTIFY_RSP_RCVD_EVENT, &fsm_msg);
    }

    FREE_INCOMING_PDU(pdu->msg);
}

/* <doc>
 * bool update_moderator_info(server_information_t *server_info,
 *                            mcast_group_node_t *group_node,
 *                            unsigned int clientID)
 * This function activates the monitoring of periodic keepalive of moderator 
 * towards Server.
 *
 * </doc>
 */
bool update_moderator_info(server_information_t *server_info,
                                  mcast_group_node_t *group_node, 
                                  unsigned int clientID)
{
    RBT_tree *tree = (RBT_tree*) server_info->client_RBT_head;
    RBT_node *rbNode = RBFindNodeByID(tree, clientID);
    mcast_client_node_t *client_node = NULL;

    if(!rbNode)
    {
        //something seriously wrong. ID not registered with sever. Log fatal error.
        return false;
    }
    
    /*Mark RBT node as moderator and busy*/
    rbNode->av_status = RB_BUSY;
    rbNode->is_moderator = TRUE;

    /*In group node LL, store pointer for moderator client node and also mark client in LL as busy.*/
    if(search_client_in_group_node(group_node, clientID, &client_node))
    {
        group_node->moderator_client = client_node;
        client_node->av_status = CLBUSY;
        return true;
    }
    return false;
}

/* <doc
 * void mcast_send_chk_alive_msg(server_information_t *server_info, void *fsm_msg)
 * This functions makes the client moderator on server and multicasts this information
 * to the group (all clients of that multicast group)
 *
 * </doc>
 */
void mcast_send_chk_alive_msg(server_information_t *server_info,
                              void *fsm_msg)
{
    char ipaddr[INET6_ADDRSTRLEN];
    fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
    pdu_t *pdu = (pdu_t *) fsm_data->pdu;

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

    /* Delete initial MOD_SEL_TIMEOUT oneshot at this stage since hitting this
     * path means at least one client has responded in mod selection phase. */
    //stop_timer(&group_node->timer_id);
    timer_delete(group_node->timer_id);
    group_node->timer_id = 0;

    /*Restoring the index*/
    group_node->group_reachability_index = GROUP_REACHABLE; 

    /* Marking client node as Moderator */
    unsigned int clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    if (clientID <= 0)
    {
        //Peer addr 0 in PDU. Log error.
        return;
    }
    
    if(/*NOT*/ !update_moderator_info(server_info, group_node, clientID))
    {
        /* Some problem in lookup. Log error and return*/
        return;
    }
    
    /* Changing the fsm state as moderator has been selected */
    group_node->fsm_state = MODERATOR_SELECTED;
    group_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
    
    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("-------------[NOTIFICATION] Client %s has been selected as MODERATOR for group %s  ---------------", ipaddr, group_node->group_name);

    LOGGING_INFO("Sending moderator (%s) notify req to multicast group %s", ipaddr, group_node->group_name);

    /*Create the moderator notification request and inform the multicast group about the Group Moderator*/
    pdu_t notify_pdu;
    comm_struct_t *req = &(notify_pdu.msg);
    moderator_notify_req_t *mod_notify_req = &(req->idv.moderator_notify_req);

    struct sockaddr_in *addr_in = (struct sockaddr_in *) &pdu->peer_addr;

    req->id = moderator_notify_req;
    mod_notify_req->moderator_id = clientID;
    mod_notify_req->moderator_port = addr_in->sin_port;
    mod_notify_req->group_name = MALLOC_STR;
    strcpy(mod_notify_req->group_name, group_node->group_name);
    mod_notify_req->num_of_clients_in_grp = group_node->number_of_clients;

    PRINT("[Moderator_Notify_Req: GRP - %s] Moderator Notify Request sent to group %s.", mod_notify_req->group_name , mod_notify_req->group_name);
    
    /*Send to multicast group*/
    write_record(server_info->server_fd, &(group_node->grp_mcast_addr), &notify_pdu);

    /* Start recurring timer for monitoring status of this moderator node.*/
    /* XXX XXX check if this timer needs to be stopped for mod reseletion. */
    ////////
    start_recurring_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, MOD_RSP_TIMEOUT);
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
    server_information_t *server_info = NULL;
    comm_struct_t *rsp = &(pdu->msg); 
    echo_rsp_t echo_rsp = rsp->idv.echo_resp;
    fsm_data_t fsm_msg;

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Response: GRP - %s] Echo Response received from %s", echo_rsp.group_name, ipaddr);

    /* Extracting client_info from variadic args*/
    EXTRACT_ARG(pdu, server_information_t*, server_info);

    /*Check the fsm of group node and act accordingly.*/
    mcast_group_node_t *group_node = NULL;
    get_group_node_by_name(&server_info, echo_rsp.group_name, &group_node);

    fsm_msg.fsm_state = group_node->fsm_state;
    fsm_msg.grp_node_ptr = group_node;
    fsm_msg.pdu = pdu;

    /* Run the server fsm*/
    server_info->fsm(server_info, ECHO_RSP_RCVD_EVENT, &fsm_msg);

    FREE_INCOMING_PDU(pdu->msg);

    return 0;
}

/* <doc>
 * static
 * void moderator_selection(server_information_t *server_info, mcast_group_node_t *group_node)
 * Functionality of this is to choose moderator amongst the client nodes of a multicast group.
 * It takes parameter as multicast group node and group name.
 *
 * </doc>
 */
static
void moderator_selection(server_information_t *server_info, mcast_group_node_t *group_node)
{
    size_t iter;
    mcast_client_node_t *client_node = NULL;
    int max_clients = MAX_CLIENTS_TRIED_PER_ATTEMPT;

    if(group_node->moderator_client == NULL)
    {
        /* This check/design mandates that when task has been completed and moderator 
         * is no longer required, the pointer should appropriately reinitialized to NULL.*/
        client_node = SN_LIST_MEMBER_HEAD(&((group_node)->client_info->client_node),
                                          mcast_client_node_t,
                                          list_element);
    }
    else
    {
        client_node = group_node->moderator_client;
    }

    if (SN_LIST_LENGTH(&group_node->client_info->client_node) < MAX_CLIENTS_TRIED_PER_ATTEMPT)
    {
        max_clients = SN_LIST_LENGTH(&group_node->client_info->client_node);
    }

    /* Send the echo req to some n clients of group for moderator selection. The one who
     * who replies first will be selected as moderator.*/
    for(iter = 0; iter < max_clients && client_node; iter++)
    {
      /*Send echo to only free clients for moderator selection*/
        if (client_node->av_status == CLFREE) 
        {
            send_echo_request(server_info->server_fd, &client_node->client_addr, group_node->group_name);

            LOGGING_INFO("Sending initial echo for moderator selection of group %s to client %d", group_node->group_name, client_node->client_id);
        }

        client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                              mcast_client_node_t,
                                              list_element);
    }

    group_node->moderator_client = client_node;
    if(client_node == NULL) 
    {
        /* Mark end of one iteration. */
        group_node->group_reachability_index--;
        
        if(group_node->group_reachability_index == 0)
        {
            /* We have failed to contact group members in designated number of
             * attempts. Do not rearm timers. Do not retrigger mod_selection.
             * Return from this point. */
            PRINT("[UNREACHABLE_ALERT: GRP - %s] Group is unreachable. ", group_node->group_name);
            return;
        }
    }

    /*Start a one time timer and wait for first client to respond. If no
     *response within timeout frame, retry. */
    start_oneshot_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, MOD_SEL_TIMEOUT);
}

/* <doc>
 * static
 * void assign_task(server_information_t *server_info, char *grp_name, int task_type, char *filename)
 * This function takes parameter as multicast group name and type
 * of task to be done. It then multicasts the task request.
 *
 * </doc>
 */
    static
void assign_task(server_information_t *server_info, char *grp_name, int task_type, char *filename)
{
    mcast_group_node_t *group_node = NULL;
    struct stat st;

    /*Validate file existence*/
    if (stat(filename, &st))
    {
        PRINT("%s is Non-existent file.", filename);
        LOGGING_ERROR("Task file %s doesnot exist.", filename);
        return;
    }

    /*fetch the group information whose clients are supposed to work on task*/
    if (grp_name)
    {
        get_group_node_by_name(&server_info, grp_name, &group_node); 
    }
    else
    {
        /*If no group is given, then find the group to work on this task, as per best-fit policy*/
        get_group_node_by_best_fit(&server_info, st.st_size, &group_node);

        if (group_node)
        {
            PRINT("-------------[NOTIFICATION] Group %s has been auto-selected for task  ---------------", group_node->group_name);
        }
    }
    /*If group node has moderator, that means group is currently busy working on a task*/
    if (!group_node)
    {
        PRINT("Error: This is a non-existent group.");
        LOGGING_ERROR("Task Assignment: Group %s is non-existent", grp_name);
        return;
    }
    if (group_node->moderator_client) 
    {
        PRINT("Group %s is currently Busy.", grp_name);
        LOGGING_ERROR("Task Assignment: Group %s is Busy.", group_node->group_name);
        return;
    } 
    else if (group_node->client_info == NULL)
    {
        PRINT("No clients present in the group.");
        LOGGING_ERROR("Task Assignment: There are no clients in the group %s", group_node->group_name);
        return;
    }

    group_node->task_type = task_type;

    LOGGING_INFO("Group %s has been assigned task of type %d", group_node->group_name, task_type);

    group_node->task_set_filename = MALLOC_CHAR(strlen(filename)+1); 
    memcpy(group_node->task_set_filename, filename, strlen(filename)+1);

    /*Group is in moderator selection pending state*/
    group_node->fsm_state = MODERATOR_SELECTION_PENDING;

    /* If group was unreachable earlier, we have to make an attempt to reach the
     * group again when prompted by user from CLI. Mark group to be rechable and
     * proceed with moderator selection. */
    group_node->group_reachability_index = GROUP_REACHABLE;
    
    /*Select the moderator for the multicast group*/
    moderator_selection(server_info, group_node);

    /* This is the first time timer is getting engaged in any way. If earlier
     * clean-up was proper then we should not be required delete timers or do
     * any timer related processing here. */
}


/* RBT comparision function */
int IntComp(unsigned a,unsigned int b) 
{
    if( a > b) return(1);
    if( a < b) return(-1);
    return(0);
}

/* RBT Printing function */
void IntPrint(unsigned int a) {
    PRINT("number - %u", a);
}

/* <doc>
 * void server_stdin_data(int fd, server_information_t *server_info)
 * Function for handling input from STDIN. Input can be any cli
 * command and respective handlers are then invoked.
 *
 * </doc>
 */
void server_stdin_data(int fd, server_information_t *server_info)
{
    char read_buffer[100];
    char read_buffer_copy[100];
    char *ptr;
    
    int cnt = 0, i = 0;
    
    cnt=read(fd, read_buffer, 99);

    /*If input buffer is empty return. Ideally this should not happen.*/
    if (cnt <= 0) return;

    read_buffer[cnt-1] = '\0';
    if (0 == strncmp(read_buffer,"show help",9))
    {
        display_server_clis();
    }
    else if(strncmp(read_buffer,"server backup",13) == 0)
    {
        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        while(i < 2)
        {
            ptr = strtok(NULL," ");
            i++;
        }
        if (!ptr)
        {
            PRINT("Error: Unrecognized Command.\n");
            return;
        }

        server_info->secondary_server.sin_family = AF_INET;
        server_info->secondary_server.sin_port = htons(atoi(PORT));
        server_info->secondary_server.sin_addr.s_addr = inet_addr(ptr);

        server_info->is_stdby_available = true;
    }
    else if(strncmp(read_buffer,"switch",5) == 0)
    {
        if (server_info->is_stdby_available)
        {
           send_new_server_info_to_all_groups(server_info); 
        }
        else
        {
           PRINT("No Backup Server available.");
        }
    }
    else if (0 == strcmp(read_buffer,"show groups"))
    {
        display_mcast_group_node(&server_info, SHOW_GROUP_ONLY);
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
            return;
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
    else if(strncmp(read_buffer,"task",4) == 0)
    {
        int task_type = 0;
        char filename[100];
        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        while(i < 1)
        {
            ptr = strtok(NULL," ");
            i++;
        }

        if(ptr)
        {
          if(strcmp(ptr,"prime") == 0)
            task_type = FIND_PRIME_NUMBERS;
          else
            task_type = INVALID_TASK_TYPE;
        }

        /* Fetch the filename */
        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        i = 0;
        while(i < 5)
        {
            ptr = strtok(NULL," ");
            i++;
        }
        if(ptr)
        {
          strcpy(filename,ptr);
        }
        else
        {
          strcpy(filename,"task_set/prime_set1.txt");        
        }
        /* Fetch the group name */
        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        i = 0;
        while(i < 3)
        {
            ptr = strtok(NULL," ");
            i++;
        }
        if(task_type) {
           assign_task(server_info, ptr, task_type, filename);
        }
        else
          PRINT("Please enter valid task type.\n");
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
    else
    {
        if (cnt != 1 && read_buffer[0] != '\n')
            PRINT("Error: Unrecognized Command.\n");
    }
}

int main(int argc, char * argv[])
{
    int sfd, efd, status;
    int event_count, index;
    struct epoll_event event;
    struct epoll_event *events;

    int active_clients = 0;
    server_information_t *server_info = NULL;
    RBT_tree* tree = NULL;

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char *addr,*port;
    char ip_addr[16];

    /*Start logging on server*/
    enable_logging(argv[0]);

    /* allocating server info for LL */ 
    allocate_server_info(&server_info);

    /* allocating RBT root for faster client lookup on server */
    tree = RBTreeCreate(IntComp, NullFunction, IntPrint);
    server_info->client_RBT_head = tree;

    /* Registering the fsm handler */
    server_info->fsm = server_main_fsm;

    num_groups = initialize_mapping("src/ip_mappings.txt", &mapping,server_info);

    if (argc != 3)
    {
        printf("Usage: %s <server_IP> <server_port>\n", argv[0]);
        struct sockaddr myIp;
        argv[1] = ip_addr;
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

    LOGGING_INFO("Server %s started on port %s", addr, port);

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
    event.events = EPOLLIN;

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

    server_info->server_fd = sfd;

    //Initialize timeout handler
    handle_timeout_real(true, 0, NULL, &server_info);

    PRINT("Establishing handler for moderator selection timeout(signal: %d)\n", MOD_SEL_TIMEOUT);
    PRINT("Establishing handler for moderator resp timeout(signal: %d)\n", MOD_RSP_TIMEOUT);
    mask_server_signals(false);
    
    events = calloc(MAXEVENTS, sizeof(event));

    while (1) 
    {
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) 
        {
            /* Code Block for accepting new connections on Server Socket*/
            if (sfd == events[index].data.fd)
            {
                MASK_SERVER_SIGNALS(true);
                fptr func;

                /* Allocating PDU for incoming message */
                pdu_t pdu;
                if ((func = server_func_handler(read_record(events[index].data.fd, &pdu))))
                {
                    (func)(events[index].data.fd, &pdu, server_info);
                }
                MASK_SERVER_SIGNALS(false);
            }
            /* Code Block for handling input from STDIN */
            else if (STDIN_FILENO == events[index].data.fd) 
            {
                /* Invoking function to recognize the cli fired and call its appropriate handler */
                server_stdin_data(events[index].data.fd, server_info);

                PRINT_PROMPT("[server] ");
            }
        }
    }
    return 0;
}


