#include <stdbool.h>
#include "client_DS.h"
#include "print.h"

void allocate_client_list(client_information_t **client_info)
{
   mcast_client_t  *mcast_client = NULL;
   mcast_client  = malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   (*client_info)->client_list = mcast_client;
}

/* <doc>
 * void allocate_client_info(client_information_t **client_info)
 * Allocates and initializes the main linked list - client_info
 * </doc>
 */
void allocate_client_info(client_information_t **client_info)
{
   *client_info = (client_information_t *) malloc(sizeof(client_information_t));
   (*client_info)->client_fd = INT_MAX;
   (*client_info)->epoll_fd = INT_MAX;
   (*client_info)->epoll_evt = NULL;
   allocate_client_list(client_info);
}

/* <doc>
 * mcast_client_node_t *allocate_client_node(client_information_t **client_info)
 * This function allocates the node, inserts it into client_info list and 
 * returns node to the caller.
 * 
 * </doc>
 */
mcast_client_node_t *allocate_client_node(client_information_t **client_info)
{
   mcast_client_node_t *new_client_node = NULL;

   if ((*client_info)->client_list == NULL)
   {
      allocate_client_list(client_info);
   }

   new_client_node = malloc(sizeof(mcast_client_node_t));

   new_client_node->group_port = -1;
   new_client_node->mcast_fd = INT_MAX;
   memset(&(new_client_node->group_name), '\0', sizeof(new_client_node->group_name));

   SN_LIST_MEMBER_INSERT_HEAD(&((*client_info)->client_list->client_node),
                             new_client_node,
                             list_element);

   return new_client_node;
}

/* <doc>
 * void deallocate_client_node(client_information_t *client_info, mcast_client_node_t *node)
 * This function removes the specied node from the client_info list.
 * 
 * </doc>
 */

void deallocate_client_node(client_information_t *client_info, mcast_client_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&(client_info->client_list->client_node),
                        node,
                        list_element);
   free(node);
}

/* <doc>
 * void display_client_node(client_information_t **client_info)
 * This function traverses and prints the client_info linked list.
 *
 * </doc>
 */
void display_client_node(client_information_t **client_info)
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

/* <doc>
 * mcast_client_node_t* get_client_node_by_group_name(client_information_t **client_info, char *grp_name)
 * This is the internal function, called by IS_GROUP_IN_CLIENT_LL. This function searches for the
 * node and return it.
 *
 * </doc>
 */
void get_client_node_by_group_name(client_information_t **client_info, char *grp_name, mcast_client_node_t **clnt_node)
{

  mcast_client_node_t *client_node = NULL;

  client_node =     SN_LIST_MEMBER_HEAD(&((*client_info)->client_list->client_node),
                                        mcast_client_node_t,
                                        list_element);
  while (client_node)
  {

     if (strncmp(client_node->group_name,grp_name,strlen(grp_name)) == 0)
     {
        *clnt_node = client_node;
        return;
     }

     client_node =     SN_LIST_MEMBER_NEXT(client_node,
                                          mcast_client_node_t,
                                          list_element);
  }

}


/* <doc>
 * bool ADD_CLIENT_IN_LL(client_information_t **client_info, mcast_client_node_t *node)
 * Add the client node in client_info list. client node contains information related
 * to multicast group IP/port and other related information.
 * Returns TRUE if added successfully, otherwise FALSE.
 *
 * </doc>
 */
bool ADD_CLIENT_IN_LL(client_information_t **client_info, mcast_client_node_t *node)
{
  mcast_client_node_t *client_node = NULL;

  client_node = allocate_client_node(client_info);

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

/* <doc>
 * bool IS_GROUP_IN_CLIENT_LL(client_information_t **client_info, char *group_name)
 * Traverses the client_info list and checks for node with group_name.
 * Returns TRUE if node is found, Otherwise FALSE.
 *
 * </doc>
 */
bool IS_GROUP_IN_CLIENT_LL(client_information_t **client_info, char *group_name)
{
  mcast_client_node_t *client_node = NULL;

  get_client_node_by_group_name(client_info, group_name, &client_node);

  if (client_node)
    return TRUE;

  return FALSE;
}
