#include "header.h"
//#include "common.h"
#include "sn_ll.h"
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MOD_SEL_TIMEOUT SIGUSR1
#define MOD_RSP_TIMEOUT SIGUSR2
#define DEFAULT_TIMEOUT 3

/*Enum for cli 'show groups' and 'show group info G1' */
typedef enum{
  SHOW_GROUP_ONLY,
  SHOW_ALL
}display_show_type_t;

/*Enum of FSM states on Server*/
typedef enum{
  MODERATOR_SELECTION_PENDING = 1,
  MODERATOR_SELECTED,
  GROUP_TASK_IN_PROGRESS,
  GROUP_TASK_COMPLETED,
  TASK_IN_PROGRESS_MOD_SEL_PEND,
  STATE_NONE,
  STATE_MAX
}server_state_t;

/*Enum of FSM events on Server*/
typedef enum{
  ECHO_RSP_RCVD_EVENT = 1,
  SEND_KEEPALIVE_EVENT,
  MOD_NOTIFY_RSP_RCVD_EVENT,
  MOD_TASK_RSP_RCVD_EVENT,
  ECHO_REQ_RCVD_EVENT,
  EVENT_NONE,
  EVENT_MAX
}server_event_t;

/*Enum for client status on Server*/
typedef enum {
 CLFREE  = 55,
 CLBUSY  = 56,
 CLALIVE = 57,
 CLDEAD  = 58
} svr_client_state;

/*Enum for operations on client list*/
typedef enum{
    DISPLAY = 1,
    UPDATE_NODE = 2
}ops;

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

/*Client Node - This node maintains information about the client, which is part of a group*/
typedef struct {
  unsigned int client_id;              /*Client ID*/
  struct sockaddr client_addr;         /*Address of Client*/
  svr_client_state av_status;          /*Availibility status of client - busy or free*/
  sn_list_element_t list_element;
} mcast_client_node_t;

typedef struct {
  sn_list_t group_node;
} mcast_group_t;

typedef struct {
  int number_of_working_clients;            /*Number of working clients on a task*/
  unsigned int *working_clients;            /*List of client ids working on task*/
  unsigned int *capability;                 /* capabilities of the clients working on task */
  char **task_filename;                     /* filenames of the data set's for the clients */
} mcast_task_set_t;

/*Group Node - This node maintains information related to multicast group.*/
typedef struct {
  char *group_name;                         /*Multicast group name*/
  unsigned int grp_capability;              /*Capability of group = capabilties of all clients in that group*/
  int number_of_clients;                    /*No. of clients under the group*/
  struct sockaddr_in grp_mcast_addr;        /*Group Multicast Address*/
  unsigned int group_port;                  /*Group Multicast Port*/
  mcast_client_t *client_info;              /*List of clients, joined to multicast group*/
  mcast_client_node_t *moderator_client;    /*Points to client which is moderator*/
  timer_t timer_id;                         /*Timer for monitoring moderator when task is in progress*/
  uint8_t heartbeat_remaining;              /*Non-zero count represents active moderator */
  server_state_t fsm_state;                 /*Current state of group in FSM, to be used by server*/
  sn_list_element_t list_element;
  int task_type;                            /* A group can perform only one task at a time. Maintaining the task type */
  char *task_set_filename;                  /* A group can perform only one task at a time. Maintaining the task set filename */
  mcast_task_set_t task_set_details;        /* List of clients and corresponding data set */
} mcast_group_node_t;

/* Declaration of Server Info - Main Data structure on Server*/
typedef struct server_information_t server_information_t;

struct server_information_t{
  unsigned int server_fd;               /*Server FD*/
  unsigned int task_id;                 /*Next task Id*/
  mcast_group_t *server_list;           /*Server List having group nodes for all the multicast groups.*/
  void *client_RBT_head;                /*Pointer to RBTree head, which maintains global list of all clients.*/
  bool (* fsm)(server_information_t* server_info, server_event_t event, void* fsm_msg);    /*Server FSM function pointer*/
};

/* RBT - This is client group node, which is part of client list of RBT tree node.
 * This node maintains pointer to LL group node.*/
typedef struct {
  void *grp_addr;                       /*Pointer to LL group node*/
  sn_list_element_t list_element;
} rb_cl_grp_node_t;

typedef struct {
  sn_list_t group_node;
} rb_client_t;

/*RB info is the list maintained in RBT node.*/
typedef struct {
  rb_client_t *cl_list;
} rb_info_t;

/*fsm data structure*/
typedef struct
{
  server_event_t fsm_state;
  mcast_group_node_t *grp_node_ptr;
  void *pdu;
}fsm_data_t;

/*Declarations*/
rb_cl_grp_node_t *allocate_rb_cl_node(rb_info_t **rb_info);

bool server_main_fsm(server_information_t *server_info,
                     server_event_t event,
                     void *fsm_msg);

void mcast_send_chk_alive_msg(server_information_t *server_info,
                             void *fsm_msg);

void mcast_start_task_distribution(server_information_t *server_info,
                                   void *fsm_msg);

void mcast_handle_task_response(server_information_t *server_info,
                                   void *fsm_msg);
void server_echo_req_task_in_progress_state(server_information_t *server_info,
                                            void *fsm_msg);


