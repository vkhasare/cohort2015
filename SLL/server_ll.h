
#include "header.h"
#include "sn_ll.h"

typedef struct {
  sn_list_t client_node;
} mcast_client_t;

typedef struct {
  int client_fd;
  struct sockaddr *client_addr;
  sn_list_element_t list_element;
} mcast_client_node_t;

typedef struct {
  sn_list_t group_node;
} mcast_group_t;

typedef struct {
  char *group_name;
  int number_of_clients;
  struct sockaddr_in group_addr;
  mcast_client_t *client_info;
  sn_list_element_t list_element;
} mcast_group_node_t;

typedef struct {
  mcast_group_t *server_list;
} server_information_t;

sn_list_element_t list_element;

void allocate_mcast_client_list(mcast_group_node_t **mcast_group_node)
{
   mcast_client_t *mcast_client = NULL;
   mcast_client = (mcast_client_t *) malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   (*mcast_group_node)->client_info = mcast_client;
}


mcast_client_node_t *allocate_mcast_client_node(mcast_group_node_t **mcast_group_node)
{
   mcast_client_node_t *new_client_node = NULL;

   if ((*mcast_group_node)->client_info == NULL)
   {
      allocate_mcast_client_list(mcast_group_node);
   }

   new_client_node = malloc(sizeof(mcast_client_node_t));
//  memset(&node,0,sizeof(node));
   (*mcast_group_node)->number_of_clients++;
   SN_LIST_MEMBER_INSERT_HEAD(&((*mcast_group_node)->client_info->client_node),
                               new_client_node,
                               list_element);

   return new_client_node;
}

void deallocate_mcast_client_node(mcast_group_node_t *mcast_group_node, mcast_client_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&(mcast_group_node->client_info->client_node),
                        node,
                        list_element);
   free(node);
}

/*
void deallocate_mcast_client_list(mcast_group_t *mcast_group)
{
   mcast_client_node_t *client_node = NULL;

   client_node =     SN_LIST_MEMBER_HEAD(&(mcast_group->client_info->client_node),
                                         mcast_client_node_t,
                                         list_element);

   while (client_node)
   {
      SN_LIST_MEMBER_REMOVE(&(mcast_group->mcast_client->client_node),
                           client_node,
                           list_element);
      free(client_node);

      client_node =  SN_LIST_MEMBER_HEAD(&(mcast_group->mcast_client->client_node),
                                         mcast_client_node_t,
                                         list_element);
   }

   mcast_group->client_info = NULL;
}
*/

void allocate_mcast_group_list(server_information_t **server_info)
{
   mcast_group_t  *mcast_group = NULL;
   mcast_group  = malloc(sizeof(mcast_group_t));
   SN_LIST_INIT(&(mcast_group->group_node));
   (*server_info)->server_list = mcast_group;
}

void allocate_server_info(server_information_t **server_info)
{
   *server_info = malloc(sizeof(server_information_t));

    allocate_mcast_group_list(server_info);
}

mcast_group_node_t *allocate_mcast_group_node(server_information_t **server_info)
{
   mcast_group_node_t *new_group_node = NULL;

   if ((*server_info)->server_list == NULL)
   {
      allocate_mcast_group_list(server_info);
   }

   new_group_node = malloc(sizeof(mcast_group_node_t));
   new_group_node->client_info = NULL;
   new_group_node->number_of_clients = 0;
   SN_LIST_MEMBER_INSERT_HEAD(&((*server_info)->server_list->group_node),
                             new_group_node,
                             list_element);
   return new_group_node;
}

void deallocate_mcast_group_node(server_information_t *server_info, mcast_group_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&(server_info->server_list->group_node),
                        node,
                        list_element);
   free(node);
}

/*
void deallocate_mcast_group_list(server_information_t *server_info)
{
   mcast_group_node_t *group_node = NULL;

   group_node =     SN_LIST_MEMBER_HEAD(&(server_info->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   while (group_node)
   {
      SN_LIST_MEMBER_REMOVE(&(server_info->server_list->group_node),
                           group_node,
                           list_element);
      free(group_node);

      group_node =   SN_LIST_MEMBER_HEAD(&(server_info->server_list->group_node),
                                         mcast_group_node_t,
                                         list_element);
   }

   server_info->server_list = NULL;

}
*/

mcast_group_node_t* get_group_node_by_name(server_information_t **server_info, char *exp_grp_name)
{
  mcast_group_node_t *group_node = NULL;
  char groupIP[INET_ADDRSTRLEN];

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   while (group_node)
   {
     if (strncmp(group_node->group_name,exp_grp_name,2) == 0)
     {
        return group_node;
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                        mcast_group_node_t,
                                        list_element);
   }

   return NULL;
}


void display_mcast_client_node(mcast_group_node_t **group_node)
{
   mcast_client_node_t *client_node = NULL;
   char buf[100];
   char ipaddr[INET6_ADDRSTRLEN];

   client_node = SN_LIST_MEMBER_HEAD(&((*group_node)->client_info->client_node),
                                       mcast_client_node_t,
                                       list_element);

   while (client_node)
   {
      inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(client_node->client_addr)), ipaddr, INET6_ADDRSTRLEN);

      sprintf(buf,
      "\n\tClient Address: %s \t Client FD: %d",
      ipaddr, client_node->client_fd);

      SIMPLE_PRINT(buf);

      client_node = SN_LIST_MEMBER_NEXT(client_node,
                                        mcast_client_node_t,
                                        list_element);
   }
}

void display_mcast_group_node(server_information_t **server_info)
{
  mcast_group_node_t *group_node = NULL;
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);


   while (group_node)
   {

     inet_ntop(AF_INET, &(group_node->group_addr), groupIP, INET_ADDRSTRLEN);

     sprintf(buf,
     "\n\n\rGroup Name: %s    Group IP: %s    Client Count: %d",
     group_node->group_name, groupIP, group_node->number_of_clients);

     SIMPLE_PRINT(buf);

     if (group_node->client_info)
     {
       display_mcast_client_node(&group_node);
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                        mcast_group_node_t,
                                        list_element);
   }
}


void display_mcast_group_node_by_name(server_information_t **server_info, char *grp_name)
{
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];

  mcast_group_node_t *group_node = get_group_node_by_name(server_info,grp_name);

   if (group_node)
   {
      inet_ntop(AF_INET, &(group_node->group_addr), groupIP, INET_ADDRSTRLEN);

      sprintf(buf,
      "\n\n\rGroup Name: %s \t Group IP: %s   Client Count: %d",
      group_node->group_name, groupIP, group_node->number_of_clients);

      SIMPLE_PRINT(buf);

      if (group_node->client_info)
      {
        display_mcast_client_node(&group_node);
      }
   }
   else
   {
     PRINT("Error: Couldnot find group.");
   }

}


bool remove_client_from_mcast_group_node(server_information_t **server_info, char *grp_name, int target_fd)
{
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];

  mcast_group_node_t *group_node = get_group_node_by_name(server_info,grp_name);

   if (group_node)
   {
      if (group_node->client_info)
      {
           mcast_client_node_t *client_node = NULL, *temp_node = NULL;

           client_node = SN_LIST_MEMBER_HEAD(&(group_node->client_info->client_node),
                                              mcast_client_node_t,
                                              list_element);

            while(client_node)
            {
               if (client_node->client_fd == target_fd)
               {
                    SN_LIST_MEMBER_REMOVE(&(group_node->client_info->client_node), client_node, list_element);
                    group_node->number_of_clients--;
                    return TRUE;
               }
                 client_node = SN_LIST_MEMBER_NEXT(client_node,
                                                  mcast_client_node_t,
                                                  list_element);
            }
      }
   }
   else
   {
     PRINT("Error: Couldnot find group.");
   }

  return FALSE;
}


void ADD_GROUP_IN_LL(server_information_t **server_info, char *group_name, struct sockaddr_in group_addrIP)
{
  mcast_group_node_t *group_node = NULL;

  group_node = allocate_mcast_group_node(server_info);

  group_node->group_name = group_name;
  group_node->group_addr = group_addrIP;
}

void ADD_CLIENT_IN_GROUP(mcast_group_node_t **group_node, struct sockaddr *addr, int infd)
{
  mcast_client_node_t *client_node = NULL;
  client_node = allocate_mcast_client_node(group_node);
  memcpy(&(client_node->client_addr),addr,sizeof(addr));
  client_node->client_fd = infd;
}

void UPDATE_GRP_CLIENT_LL(server_information_t **server_info, char *grp_name, struct sockaddr *addr, int infd)
{
   mcast_group_node_t *group_node = get_group_node_by_name(server_info,grp_name);

   if (group_node)
   {
     ADD_CLIENT_IN_GROUP(&group_node, addr, infd);
   }
}
