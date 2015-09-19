
#include "header.h"
#include "sn_ll.h"

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

typedef struct {
  char group_name[10];
  struct sockaddr_in group_addr;
  unsigned int fd_id;
  sn_list_element_t list_element;
} mcast_client_node_t;

typedef struct {
  mcast_client_t *server_list;
} client_information_t;

sn_list_element_t list_element;

void allocate_mcast_client_list(client_information_t *client_info)
{
   mcast_client_t  *mcast_client = NULL;
   mcast_client  = malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   client_info->server_list = mcast_client;
}

static int j = 0;

void allocate_client_list(client_information_t **client_info)
{
   mcast_client_t  *mcast_client = NULL;
   mcast_client  = malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   (*client_info)->server_list = mcast_client;
}

void allocate_client_info(client_information_t **client_info)
{
   *client_info = (client_information_t *) malloc(sizeof(client_information_t));

   allocate_client_list(client_info);
}

mcast_client_node_t *allocate_mcast_client_node(client_information_t **client_info)
{
   mcast_client_node_t *new_client_node = NULL;

   if ((*client_info)->server_list == NULL)
   {
      allocate_client_list(client_info);
   }

   new_client_node = malloc(sizeof(mcast_client_node_t));
   SN_LIST_MEMBER_INSERT_HEAD(&((*client_info)->server_list->client_node),
                             new_client_node,
                             list_element);
   return new_client_node;
}

void deallocate_mcast_client_node(client_information_t *client_info, mcast_client_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&(client_info->server_list->client_node),
                        node,
                        list_element);
   free(node);
}


void display_mcast_client_node(client_information_t **client_info)
{
  mcast_client_node_t *client_node = NULL;
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];


  client_node =     SN_LIST_MEMBER_HEAD(&((*client_info)->server_list->client_node),
                                        mcast_client_node_t,
                                        list_element);
  while (client_node)
  {
     inet_ntop(AF_INET, &(client_node->group_addr), groupIP, INET_ADDRSTRLEN);
     sprintf(buf,"%s  \t %s",client_node->group_name, groupIP);
     PRINT(buf);
     client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                          mcast_client_node_t,
                                          list_element);
  }

}

void ADD_CLIENT_IN_LL(client_information_t **client_info, char *group_name, struct sockaddr_in group_addrIP, int fd_id)
{
  mcast_client_node_t *client_node = NULL;

  client_node = allocate_mcast_client_node(client_info);
  strcpy(client_node->group_name,group_name);
  client_node->group_addr = group_addrIP;
  client_node->fd_id=fd_id;
}
