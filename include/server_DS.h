#include "header.h"
#include "sn_ll.h"

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

/*Client Node*/
typedef struct {
  unsigned int client_fd;
  struct sockaddr *client_addr;
  sn_list_element_t list_element;
} mcast_client_node_t;

typedef struct {
  sn_list_t group_node;
} mcast_group_t;

/*Group Node*/
typedef struct {
  char *group_name;
  int number_of_clients;
  struct sockaddr_in group_addr;
  unsigned int group_port;
  mcast_client_t *client_info;
  sn_list_element_t list_element;
} mcast_group_node_t;

/*Server Info*/

typedef struct {
  mcast_group_t *server_list;
  void *client_RBT_head;
} server_information_t;

typedef struct {
  void *grp_addr;
  sn_list_element_t list_element;
} rb_cl_grp_node_t;

typedef struct {
  sn_list_t group_node;
} rb_client_t;

typedef struct {
  rb_client_t *cl_list;
} rb_info_t;

sn_list_element_t list_element;
