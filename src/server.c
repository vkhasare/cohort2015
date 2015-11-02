#include "common.h"
#include "server_DS.h"
#include "RBT.h"

grname_ip_mapping_t * mapping = NULL;
extern unsigned int echo_req_len;
extern unsigned int echo_resp_len; //includes nul termination
unsigned int num_groups = 0; // should be removed in future, when we remove mapping array and completely migrate on LL.

const int MAX_CLIENTS_TRIED_PER_ATTEMPT = 3;
const int MAX_ALLOWED_KA_MISSES = 5;

static int handle_echo_req(const int, pdu_t *pdu, ...);
static int handle_echo_response(const int sockfd, pdu_t *pdu, ...);
static int handle_join_req(const int sockfd, pdu_t *pdu, ...);
static int handle_leave_req(const int sockfd, pdu_t *pdu, ...);
static int handle_moderator_notify_response(const int sockfd, pdu_t *pdu, ...);
static int handle_moderator_task_response(const int sockfd, pdu_t *pdu, ...);
static void assign_task(server_information_t *, char *, int);
static void moderator_selection(server_information_t *, mcast_group_node_t *);

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
    comm_struct_t *req = &pdu->msg;
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause = REJECTED;
    char ipaddr[INET6_ADDRSTRLEN];
    server_information_t *server_info = NULL;
    unsigned int clientID = 0;
    RBT_tree *tree = NULL;
    RBT_node *rbNode = NULL;
    pdu_t rsp_pdu;
    comm_struct_t *resp = &rsp_pdu.msg;

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

    PRINT("[Leave_Response: GRP - %s, Client - %s] Cause: %s.",group_name, ipaddr, enum_to_str(cause));

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
    comm_struct_t *req = &(pdu->msg);
    uint8_t cl_iter, s_iter;
    char *group_name;
    msg_cause cause;
    server_information_t *server_info = NULL;
    unsigned int clientID = 0;
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

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);

    /* Initializing is must for xdr msg. */
    join_rsp->group_ips = (l_saddr_in_t *) calloc (join_req.num_groups, sizeof(l_saddr_in_t));
    
    for(cl_iter = 0; cl_iter < join_req.num_groups; cl_iter++){

        cause = REJECTED;
        group_name = join_req.group_ids[cl_iter].str;

        PRINT("[Join_Request: GRP - %s, Client - %s] Join Request Received.", group_name, ipaddr);

        /*search group_node in LL by group name*/
        get_group_node_by_name(&server_info, group_name, &group_node);

        if (group_node) {
             cause = ACCEPTED;

             clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

             /*Purge the client if already existing in the group list*/
             remove_client_from_mcast_group_node(&server_info, group_node, clientID);
             /*Add client in group link list*/
             ADD_CLIENT_IN_GROUP(&group_node, (struct sockaddr*) &pdu->peer_addr, clientID);

             /*Add in Client RBT*/
             tree = (RBT_tree*) server_info->client_RBT_head;

             if (clientID > 0) {
                 /*search the RB node by clientID*/
                 newNode = RBFindNodeByID(tree, clientID);
                 /* Add in RBT if it is a new client, else update the grp list of client */
                 if (!newNode) {
                     newNode = RBTreeInsert(tree, clientID, (struct sockaddr*) &pdu->peer_addr, 0, RB_FREE, group_node);
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
    }
    
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
    for(i = 0; i < count; i++){
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
      }
  }
  else
  {
      PRINT("[INFO] All clients are up and executing task.");
  }
}

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
        //traverse the list to find which group timer expired
        get_group_node_by_timerid(server_info_local, 
                                  si->si_value.sival_ptr, 
                                  &grp_node);
        
        if(signal == MOD_SEL_TIMEOUT)
        {
            //call handler logic here
            timer_delete(grp_node->timer_id);
            moderator_selection(*server_info_local, grp_node);
        }
        else if(signal == MOD_RSP_TIMEOUT)
        {
            //decrement counter and check if moderator has missed n successive heartbeat
            if(grp_node->heartbeat_remaining-- == 0)
            {
                char ipaddr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET, 
                          get_in_addr(&(grp_node->moderator_client->client_addr)), 
                          ipaddr, INET6_ADDRSTRLEN);
                PRINT("[UNREACHABLE_ALERT: GRP - %s] Moderator (%s) is unreachable. ", grp_node->group_name, ipaddr);
                timer_delete(grp_node->timer_id);
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

    get_group_node_by_name(&server_info, echo_req.group_name, &group_node);
    
    /*filling echo rsp pdu*/
    echo_response->status    = 11;
    echo_response->group_name = echo_req.group_name;

    write_record(sockfd, &pdu->peer_addr, &rsp_pdu);

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Response: GRP - %s] Echo Response sent to %s", echo_response->group_name, ipaddr);

    /*Check the fsm of group node and act accordingly.*/
    get_group_node_by_name(&server_info, echo_req.group_name, &group_node);

    fsm_msg.fsm_state = group_node->fsm_state;
    fsm_msg.grp_node_ptr = group_node;
    fsm_msg.pdu = pdu;

    /* Run the server fsm*/
    server_info->fsm(server_info, ECHO_REQ_RCVD_EVENT, &fsm_msg);

    return 0;
}

unsigned int get_task_count(const char* filename,unsigned int** task_set)
{
    FILE *fp = NULL, *cmd_line = NULL;
    uint32_t count, i;
    char element[16], cmd[256];
    
    strcpy(cmd, "wc -l ");
    strcat(cmd, filename);
    cmd_line = popen (cmd, "r");
    fscanf(cmd_line, "%i", &count);
    pclose(cmd_line);

    *task_set = (unsigned int *) malloc(sizeof(unsigned int) * count);
    
    fp = fopen(filename, "r");
    for(i = 0; i < count; i++){
        fscanf(fp, "%s", element);
        (* task_set)[i] =strtoul(element,NULL,10);
    }
    fclose(fp);

  return count;
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
    int count = 0;
    comm_struct_t req;
    fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
    pdu_t req_pdu;
    unsigned int* task_set,task_count;

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

    /* Changing the fsm state as task has been started */
    group_node->fsm_state = GROUP_TASK_IN_PROGRESS;

    pdu_t *pdu = (pdu_t *) fsm_data->pdu;

    /* Get the task set for prime numbers */
    task_count = get_task_count("src/task_set.txt", &task_set);

    req.idv.perform_task_req.group_name= malloc(sizeof(char)*strlen(group_node->group_name));
    strcpy(req.idv.perform_task_req.group_name, group_node->group_name);
    req.idv.perform_task_req.task_id = server_info->task_id;
    (server_info->task_id)++;  
    
    req.id = perform_task_req;

    /* Fetch the client ID's and copy the task set */
    if (pdu->msg.idv.moderator_notify_rsp.client_id_count)
    {
        req.idv.perform_task_req.client_id_count = pdu->msg.idv.moderator_notify_rsp.client_id_count;
        req.idv.perform_task_req.client_ids = (unsigned int *) malloc(sizeof(unsigned int) * pdu->msg.idv.moderator_notify_rsp.client_id_count);
    }

    for(count = 0; count < pdu->msg.idv.moderator_notify_rsp.client_id_count; count++)
    {
        req.idv.perform_task_req.client_ids[count] = pdu->msg.idv.moderator_notify_rsp.client_ids[count];
    }

    req.idv.perform_task_req.task_count = task_count;
    req.idv.perform_task_req.task_set = (unsigned int *) malloc(sizeof(unsigned int) * task_count);

    for(count = 0; count < task_count; count++)
    {
        req.idv.perform_task_req.task_set[count] = task_set[count];
    }

    req.idv.perform_task_req.task_type = group_node->task_type;
    req_pdu.msg = req;

    /* Send multicast msg to group to perform task */
    write_record(server_info->server_fd,&(group_node->grp_mcast_addr), &req_pdu);

    PRINT("[Task_Request: GRP - %s] Task Request Sent.", group_node->group_name);
}

/* <doc>
 * void get_task_response_file_name(char * gname, uint8_t task_id, char * buffer)
 * This function returns the file name for result file
 *
 * </doc>
 */
void get_task_response_file_name(char * gname, uint8_t task_id, char * buffer){
     time_t rawtime;
     struct tm * timeinfo;
     char buffer2[40];
 
     time ( &rawtime );
     timeinfo = localtime ( &rawtime );
     sprintf(buffer,"task_result/Resp_%s_%d_", gname , task_id);
     strftime(buffer2,80,"%d%m%y_%H%M%S.txt",timeinfo);
     strcat(buffer,buffer2);
     PRINT("[INFO] The Response from %s for task %d is written in %s",gname, task_id,  buffer);
}


/* <doc>
 * write_task_response_file(task_rsp_t * task_rsp, char* fname)
 * After getting the task response, this function writes the
 * result in the result file.
 *
 * </doc>
 */ 
void
write_task_response_file(task_rsp_t * task_rsp, char* fname){
  uint32_t i, j;
  FILE *fp=NULL;
  fp=fopen(fname,"w");
  fprintf(fp, "Client Id, Result Size, Result\n");
  for(i=0;i<task_rsp->num_clients;i++){
    fprintf(fp, "%u", task_rsp->client_ids[i]);
    fprintf(fp, ", %u", task_rsp->result[i].size);
    for(j=0;j<task_rsp->result[i].size;j++){
      fprintf(fp, ", %u", task_rsp->result[i].value[j]);
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
}

/* <doc>
 * void mcast_handle_task_response(server_information_t *server_info,
 *                                    void *fsm_msg)
 * Function to handle task response from the multicast group moderator. It takes 
 * input as response collated by the moderator of the group along with the client IDs 
 *  and the task type.
 *
 * </doc>
 */
void mcast_handle_task_response(server_information_t *server_info,
                                   void *fsm_msg)
{
  char ipaddr[INET6_ADDRSTRLEN], filename[80];
  
  int count = 0;
  comm_struct_t req;
  fsm_data_t *fsm_data = (fsm_data_t *)fsm_msg;
  pdu_t rsp_pdu;
  unsigned int* task_set,task_count;

  /* fetch the group node pointer from fsm_data */
  mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

  /* Changing the fsm state as task has been started */
  group_node->fsm_state = GROUP_TASK_COMPLETED;

  pdu_t *pdu = (pdu_t *) fsm_data->pdu;
  task_rsp_t *task_response= &(pdu->msg.idv.task_rsp);
  
  /* Get the file name based on current time stamp */ 
  get_task_response_file_name(group_node->group_name, task_response->task_id, filename); 
  
  /* Write the response  to file */  
  write_task_response_file(task_response, filename);
     
  /*Disarming the keepalive for this mod since task is complete.*/
  timer_delete(group_node->timer_id);
  
  /*Resetting mod_client pointer to avoid havoc in moderator_selection*/
  group_node->moderator_client = NULL;
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
        ll_grp_node->moderator_client->av_status = CLFREE;
        ll_grp_node->moderator_client = NULL;

        fsm_msg.fsm_state = ll_grp_node->fsm_state;
        fsm_msg.grp_node_ptr = (void *) ll_grp_node;
        fsm_msg.pdu = pdu;

        /*Run the server fsm*/
        server_info->fsm(server_info, MOD_TASK_RSP_RCVD_EVENT, &fsm_msg);
    }
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
}

bool activate_moderator_keepalive(server_information_t *server_info,
                                  mcast_group_node_t *group_node, 
                                  unsigned int clientID)
{
    RBT_tree *tree = (RBT_tree*) server_info->client_RBT_head;
    RBT_node *rbNode = RBFindNodeByID(tree, clientID);;
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

        /* Disarm initial MOD_SEL_TIMEOUT oneshot timer and start recurring timer
         * for moderator keepalive on this group */
        timer_delete(group_node->timer_id);

        /* Start recurring timeer for monitoring status of this moderator node.*/
        start_recurring_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, MOD_RSP_TIMEOUT);
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

    /* fetch the group node pointer from fsm_data */
    mcast_group_node_t *group_node = fsm_data->grp_node_ptr;

    pdu_t *pdu = (pdu_t *) fsm_data->pdu;

    /*Marking client node as Moderator*/
    unsigned int clientID = calc_key((struct sockaddr*) &pdu->peer_addr);

    if (clientID <= 0){
        //Peer addr 0 in PDU. Log error.
        return;
    }
    if(/*NOT*/ !activate_moderator_keepalive(server_info, group_node, clientID))
        return;

    /* Changing the fsm state as moderator has been selected */
    group_node->fsm_state = MODERATOR_SELECTED;
    group_node->heartbeat_remaining = MAX_ALLOWED_KA_MISSES;
    
    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(pdu->peer_addr)), ipaddr, INET6_ADDRSTRLEN);
    PRINT("-------------[NOTIFICATION] Client %s has been selected as MODERATOR for group %s  ---------------", ipaddr, group_node->group_name);

    /*Create the moderator notification request and inform the multicast group about the Group Moderator*/
    pdu_t notify_pdu;
    comm_struct_t *req = &(notify_pdu.msg);
    moderator_notify_req_t *mod_notify_req = &(req->idv.moderator_notify_req);

    struct sockaddr_in *addr_in = (struct sockaddr_in *) &pdu->peer_addr;

    req->id = moderator_notify_req;
    mod_notify_req->moderator_id = clientID;
    mod_notify_req->moderator_port = addr_in->sin_port;
    mod_notify_req->group_name = group_node->group_name;
    mod_notify_req->num_of_clients_in_grp = group_node->number_of_clients;

    /*Send to multicast group*/
    write_record(server_info->server_fd, &(group_node->grp_mcast_addr), &notify_pdu);

    PRINT("[Moderator_Notify_Req: GRP - %s] Moderator Notify Request sent to group %s.", mod_notify_req->group_name , mod_notify_req->group_name);
    //  group_node->fsm_state = MOD_NOTIFICATION_PENDING;
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

    /*Group is in moderator selection pending state*/
    group_node->fsm_state = MODERATOR_SELECTION_PENDING;

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

    /* Send the echo req to some n clients of group for moderator selection. The one who
     * who replies first will be selected as moderator.*/
    for(iter = 0; iter < MAX_CLIENTS_TRIED_PER_ATTEMPT && client_node; iter++)
// while(client_node)
    {
      /*Send echo to only free clients for moderator selection*/
        if (client_node->av_status == CLFREE)
            send_echo_request(server_info->server_fd, &client_node->client_addr, group_node->group_name);

        client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                              mcast_client_node_t,
                                              list_element);
    }
    group_node->moderator_client = client_node;

    /*Start a one time timer and wait for first client to respond. If no
     *response within timeout frame, retry. */
    start_oneshot_timer(&(group_node->timer_id), DEFAULT_TIMEOUT, MOD_SEL_TIMEOUT);
}

/* <doc>
 * static
 * void assign_task(server_information_t *server_info, char *grp_name, int task_type)
 * This function takes parameter as multicast group name and type
 * of task to be done. It then multicasts the task request.
 *
 * </doc>
 */
static
void assign_task(server_information_t *server_info, char *grp_name, int task_type)
{
    mcast_group_node_t *group_node = NULL;

    /*fetch the group information whose clients are supposed to work on task*/
    get_group_node_by_name(&server_info, grp_name, &group_node); 

   /*If group node has moderator, that means group is currently busy working on a task*/
   if (group_node->moderator_client) 
   {
      PRINT("Group %s is currently Busy.", grp_name);
      return;
   } 
   else if (group_node->client_info == NULL) 
   {
      PRINT("No clients present in the group.");
      return;
   }
   
   group_node->task_type = task_type;
   /*Select the moderator for the multicast group*/
   moderator_selection(server_info, group_node);
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
    
    int cnt=0, i = 0, msfd;
    int group_msg[1000] = {0};
    
    struct sockaddr_in maddr;
    
    cnt=read(fd, read_buffer, 99);

    /*If input buffer is empty return. Ideally this should not happen.*/
    if (cnt <= 0) return;

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
            return;
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
            return;
        }
        for(i = 0;i < num_groups; i++)
        {
            if(strcmp(ptr,mapping[i].grname) == 0)
                group_msg[i] = 0;
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
        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        while(i < 1)
        {
            ptr = strtok(NULL," ");
            i++;
        }

        if(ptr)
          task_type = atoi(ptr);

        strcpy(read_buffer_copy,read_buffer);
        ptr = strtok(read_buffer_copy," ");
        i = 0;
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
        for(i = 0;i < num_groups; i++)
        {
            if(strcmp(ptr,mapping[i].grname) == 0)
              assign_task(server_info, mapping[i].grname, task_type);
        }
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

    server_info->server_fd = sfd;

    //Initialize timeout handler
    handle_timeout_real(true, 0, NULL, &server_info);

    struct sigaction sa;
    
    PRINT("Establishing handler for moderator selection timeout(signal: %d)\n", MOD_SEL_TIMEOUT);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_timeout;
    sigemptyset(&sa.sa_mask);
    if (sigaction(MOD_SEL_TIMEOUT, &sa, NULL) == -1)
        errExit("sigaction");
    
    PRINT("Establishing handler for moderator resp timeout(signal: %d)\n", MOD_RSP_TIMEOUT);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_timeout;
    sigemptyset(&sa.sa_mask);
    if (sigaction(MOD_RSP_TIMEOUT, &sa, NULL) == -1)
        errExit("sigaction");
    
    events = calloc(MAXEVENTS, sizeof(event));

    while (1) 
    {
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) 
        {
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


