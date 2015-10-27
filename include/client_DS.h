#include "header.h"
#include "sn_ll.h"

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

/* client node */
typedef struct {
  char group_name[10];
  struct sockaddr_in group_addr;
  int group_port;
  unsigned int mcast_fd;
  sn_list_element_t list_element;
} mcast_client_node_t;

/* client info  */
typedef struct {
  unsigned int client_status;
  unsigned int client_fd;
  unsigned int epoll_fd;
  struct sockaddr_in server;
  struct epoll_event *epoll_evt;
  mcast_client_t *client_list;
  struct common_struct * moderator_resp_msg;
} client_information_t;

sn_list_element_t list_element;

