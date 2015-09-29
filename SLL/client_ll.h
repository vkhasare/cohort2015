
#include "header.h"
#include "sn_ll.h"

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

typedef struct {
  char group_name[10];
  struct sockaddr_in group_addr;
  int group_port;
  unsigned int mcast_fd;
  sn_list_element_t list_element;
} mcast_client_node_t;

typedef struct {
  unsigned int client_fd;
  mcast_client_t *client_list;
} client_information_t;

sn_list_element_t list_element;

void allocate_mcast_client_list(client_information_t *client_info)
{
   mcast_client_t  *mcast_client = NULL;
   mcast_client  = malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   client_info->client_list = mcast_client;
}

void allocate_client_list(client_information_t **client_info)
{
   mcast_client_t  *mcast_client = NULL;
   mcast_client  = malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   (*client_info)->client_list = mcast_client;
}

void allocate_client_info(client_information_t **client_info)
{
   *client_info = (client_information_t *) malloc(sizeof(client_information_t));
   (*client_info)->client_fd = -1;

   allocate_client_list(client_info);
}

mcast_client_node_t *allocate_mcast_client_node(client_information_t **client_info)
{
   mcast_client_node_t *new_client_node = NULL;

   if ((*client_info)->client_list == NULL)
   {
      allocate_client_list(client_info);
   }

   new_client_node = malloc(sizeof(mcast_client_node_t));
   SN_LIST_MEMBER_INSERT_HEAD(&((*client_info)->client_list->client_node),
                             new_client_node,
                             list_element);
   return new_client_node;
}

void deallocate_mcast_client_node(client_information_t *client_info, mcast_client_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&(client_info->client_list->client_node),
                        node,
                        list_element);
   free(node);
}


void display_mcast_client_node(client_information_t **client_info)
{
  mcast_client_node_t *client_node = NULL;
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];


  client_node =     SN_LIST_MEMBER_HEAD(&((*client_info)->client_list->client_node),
                                        mcast_client_node_t,
                                        list_element);

  if (client_node == NULL)
  {
    SIMPLE_PRINT("\t  Client has 0 groups.\n");
    return;
  }

  sprintf(buf,"Grp Name  \t Grp IP \t Grp Port \t Mcast_fd \t client_fd");
  PRINT(buf);
  sprintf(buf,"---------------------------------------------------------------------------");
  PRINT(buf);

  while (client_node)
  {
     inet_ntop(AF_INET, &(client_node->group_addr), groupIP, INET_ADDRSTRLEN);

     sprintf(buf,"\n\t%s \t\t %s \t\t %d \t\t %d \t\t %d",client_node->group_name, groupIP, client_node->group_port, client_node->mcast_fd, (*client_info)->client_fd);
     SIMPLE_PRINT(buf);

     client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                          mcast_client_node_t,
                                          list_element);
  }

}


mcast_client_node_t* get_client_node_by_group_name(client_information_t **client_info, char *grp_name)
{

  mcast_client_node_t *client_node = NULL;

  client_node =     SN_LIST_MEMBER_HEAD(&((*client_info)->client_list->client_node),
                                        mcast_client_node_t,
                                        list_element);
  while (client_node)
  {

     if (strcmp(client_node->group_name,grp_name) == 0)
     {
        break;
     }

     client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                          mcast_client_node_t,
                                          list_element);
  }

  return client_node;
}


void remove_group_from_client(client_information_t **client_info, mcast_client_node_t *node)
{
      SN_LIST_MEMBER_REMOVE(&((*client_info)->client_list->client_node), node, list_element); 
}


bool ADD_CLIENT_IN_LL(client_information_t **client_info, mcast_client_node_t *node)
{
  mcast_client_node_t *client_node = NULL;

  client_node = allocate_mcast_client_node(client_info);

  if (client_node)
  {
    strcpy(client_node->group_name,node->group_name);
    client_node->group_addr = node->group_addr;
    client_node->group_port = node->group_port;
    client_node->mcast_fd = node->mcast_fd;

    return TRUE;
  }

  return FALSE;
}

bool IS_GROUP_IN_CLIENT_LL(client_information_t **client_info, char *group_name)
{
  get_client_node_by_group_name(client_info, group_name);
}
