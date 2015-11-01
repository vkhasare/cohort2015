#include "header.h"
#include "sn_ll.h"
  
#define MAX_TASK_COUNT 100000

/*Enum for pending list and done list of moderator*/
typedef enum{
  SHOW_MOD_PENDING_CLIENTS = 1,
  SHOW_MOD_DONE_CLIENTS
}moderator_show_type_t;
 
/*Enum of FSM states on Moderator*/
typedef enum{
  MODERATOR_NOTIFY_RSP_PENDING = 1,
  MODERATOR_NOTIFY_RSP_SENT,
  MODERATOR_TASK_RSP_PENDING,
  STATE_NONE,
  STATE_MAX
}moderator_state_t;

/*Enum of FSM events on Moderator*/
typedef enum{
  MOD_ECHO_REQ_RCVD_EVENT = 1,
  EVENT_NONE,
  EVENT_MAX
}moderator_event_t;
 
typedef struct {
  sn_list_t client_grp_node;
} client_grp_t;
 
/* Moderator client node - This is the moderator view of client node*/
typedef struct {
  unsigned int peer_client_id;                    /*Client ID of peer client node*/
  struct sockaddr peer_client_addr;               /*IP addr of peer client node*/
  uint8_t heartbeats_missed;                      /*Number of heartbeats missed for this client*/
  sn_list_element_t list_element;
} mod_client_node_t;

struct client_information_t;

/* Moderator info - This contains all the information about moderator*/
typedef struct {
  char group_name[10];                            /*Name of group moderator belongs to.*/
  client_grp_t *pending_client_list;              /*List of clients who are working on task and currently being checked for keepalive*/
  client_grp_t *done_client_list;                 /*List of clients who have completed the task*/
  moderator_state_t fsm_state;                    /*Moderator FSM state*/
  uint8_t task_id;                                /*Task Identifier */
  uint8_t active_client_count;                      /*Number of active clients in the multicast group*/
  bool (* fsm)(struct client_information_t *client_info, moderator_event_t event, void *fsm_msg);    /*Moderator FSM function pointer*/
  void * moderator_resp_msg;
} moderator_information_t;
  
/* client group node - This node comprises of multicast group information, of which client is member of.*/
typedef struct {
  char group_name[10];                /*Name of multicast group*/
  struct sockaddr_in group_addr;      /*Address of multicast group*/
  int group_port;                     /*Multicast group port*/
  unsigned int mcast_fd;              /*Multicast FD associated with every group*/
  sn_list_element_t list_element;
} client_grp_node_t;
  
/* client info - This contains all the information about client.*/
typedef struct {
  uint8_t is_moderator:1;                           /*Is this is a moderator?*/
  moderator_information_t* moderator_info;          /*If moderator, then it must have list of clients*/
  unsigned int client_id;                           /*Client ID*/
  unsigned int client_fd;                           /*Client FD*/
  unsigned int epoll_fd;                            /*EPoll FD*/
  unsigned int client_status;                       /*Busy or Free*/
  struct sockaddr_in server;                        /*Address of server*/
  struct sockaddr moderator;                        /*Address of moderator*/
  struct epoll_event *epoll_evt;                    /*Pointer to epoll event structure, to register the new FD's*/
  client_grp_t *client_grp_list;                    /*List of client group nodes, each node will have info. abt the joined group.*/
} client_information_t;
  
sn_list_element_t list_element;
  
/*fsm data structure*/
typedef struct
{
  moderator_event_t fsm_state;
  void *pdu;
}fsm_data_t;

bool moderator_main_fsm(client_information_t *client_info,
                        moderator_event_t event,
                        void *fsm_msg);

void moderator_echo_req_notify_rsp_pending_state(client_information_t *client_info,
                                                 void *fsm_msg);

void moderator_echo_req_task_rsp_pending_state(client_information_t *client_info,
                                               void *fsm_msg);

typedef struct {
  unsigned int data_count;
  unsigned int data[MAX_TASK_COUNT];
  unsigned int result_count;
  unsigned int result[MAX_TASK_COUNT];
} thread_args;
