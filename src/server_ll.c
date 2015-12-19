#include <stdbool.h>
#include "server_DS.h"
#include "print.h"

extern int debug_mode;

void allocate_rb_cl_list(rb_info_t **rb_info)
{
   rb_client_t  *client = NULL;
   client  = (rb_client_t *) malloc(sizeof(rb_client_t));
   SN_LIST_INIT(&(client->group_node));
   (*rb_info)->cl_list = client;
}

/* <doc>
 * rb_cl_grp_node_t *allocate_rb_cl_node(rb_info_t **rb_info)
 * Allocates the node and saves it in rb_info.
 * Returns the allocated node, which can further be
 * initialized to LL group node pointer
 *
 * </doc>
 */
rb_cl_grp_node_t *allocate_rb_cl_node(rb_info_t **rb_info)
{
   rb_cl_grp_node_t *new_node = NULL;

   if ((*rb_info)->cl_list == NULL)
   {
      allocate_rb_cl_list(rb_info);
   }

   new_node = (rb_cl_grp_node_t *) malloc(sizeof(rb_cl_grp_node_t));
   new_node->grp_addr = NULL;

   SN_LIST_MEMBER_INSERT_HEAD(&((*rb_info)->cl_list->group_node),
                             new_node,
                             list_element);
   return new_node;
}

/* <doc>
 * void allocate_rb_info(rb_info_t **rb_info)
 * Allocates rb_info for group list
 *
 * </doc>
 */
void allocate_rb_info(rb_info_t **rb_info)
{
   *rb_info = malloc(sizeof(rb_info_t));

    allocate_rb_cl_list(rb_info);
}

/* <doc>
 * void display_rb_group_list(rb_info_t **rb_info) 
 * Prints group list from the RBT client node.
 * It accesses the group node pointer from group list
 * and prints the group name from group node.
 *
 * </doc>
 */
void display_rb_group_list(rb_info_t **rb_info)
{
   rb_cl_grp_node_t *group_node = NULL;

   group_node =     SN_LIST_MEMBER_HEAD(&((*rb_info)->cl_list->group_node),
                                        rb_cl_grp_node_t,
                                        list_element);

   while (group_node)
   {
     mcast_group_node_t *node = ((mcast_group_node_t *)group_node->grp_addr);

     PRINT("group-name = %s",(node)->group_name);
     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                          rb_cl_grp_node_t,
                                          list_element);
   }
}

/* <doc>
 * void deallocate_rb_cl_node(rb_info_t *rb_info, rb_cl_grp_node_t *node)
 * Deallocates group node and removes it from  rb info list, frees it.
 *
 * </doc>
 */
void deallocate_rb_cl_node(rb_info_t **rb_info, rb_cl_grp_node_t *node)
{
   SN_LIST_MEMBER_REMOVE(&((*rb_info)->cl_list->group_node),
                         node,
                         list_element);
   free(node);
}

/* <doc>
 * void remove_rb_grp_node(rb_info_t **rb_info, char *grpname)
 * searches the group node for a client against its grpname
 * and removes it.
 *
 * </doc>
 */
bool search_rb_grp_node(rb_info_t **rb_info, char *grpname, rb_cl_grp_node_t **rb_grp_node)
{
   rb_cl_grp_node_t *group_node = NULL;

   group_node =     SN_LIST_MEMBER_HEAD(&((*rb_info)->cl_list->group_node),
                                        rb_cl_grp_node_t,
                                        list_element);

   while (group_node)
   {
     mcast_group_node_t *node = ((mcast_group_node_t *)group_node->grp_addr);

     /* PRINT("group-name = %s",(node)->group_name); */
     if (!strcmp((node)->group_name, grpname)) {
         *rb_grp_node = group_node;
         return TRUE;
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                          rb_cl_grp_node_t,
                                          list_element);
   }
  return FALSE;
}

/* <doc>
 * void remove_rb_grp_node(rb_info_t **rb_info, char *grpname)
 * searches the group node for a client against its grpname
 * and removes it.
 *
 * </doc>
 */
bool remove_rb_grp_node(rb_info_t **rb_info, char *grpname, mcast_group_node_t **ll_grp_node)
{
   rb_cl_grp_node_t *group_node = NULL;

   group_node =     SN_LIST_MEMBER_HEAD(&((*rb_info)->cl_list->group_node),
                                        rb_cl_grp_node_t,
                                        list_element);

   while (group_node)
   {
     mcast_group_node_t *node = ((mcast_group_node_t *)group_node->grp_addr);

     /* PRINT("group-name = %s",(node)->group_name); */
     if (!strcmp((node)->group_name, grpname)) {
         *ll_grp_node = node;
         deallocate_rb_cl_node(rb_info, group_node);
         return TRUE;
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                          rb_cl_grp_node_t,
                                          list_element);
   } 
  return FALSE;
}

void allocate_mcast_client_list(mcast_group_node_t **mcast_group_node)
{
   mcast_client_t *mcast_client = NULL;
   mcast_client = (mcast_client_t *) malloc(sizeof(mcast_client_t));
   SN_LIST_INIT(&(mcast_client->client_node));
   (*mcast_group_node)->client_info = mcast_client;
}

/* <doc>
 * mcast_client_node_t *allocate_mcast_client_node(mcast_group_node_t **mcast_group_node)
 * Allocates the client node.
 *
 * </doc>
 */
mcast_client_node_t *allocate_mcast_client_node(mcast_group_node_t **mcast_group_node)
{
   mcast_client_node_t *new_client_node = NULL;

   if ((*mcast_group_node)->client_info == NULL)
   {
      allocate_mcast_client_list(mcast_group_node);
   }

   new_client_node = malloc(sizeof(mcast_client_node_t));

   (*mcast_group_node)->number_of_clients++;
   SN_LIST_MEMBER_INSERT_HEAD(&((*mcast_group_node)->client_info->client_node),
                               new_client_node,
                               list_element);

   return new_client_node;
}

/* <doc>
 * void deallocate_mcast_client_node(mcast_group_node_t *mcast_group_node, mcast_client_node_t *node)
 * Removes the client association from group node and frees it.
 *
 * </doc>
 */
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
   (*server_info)->task_id =1;
   (*server_info)->is_stdby_available = false;
}

/* <doc>
 * void allocate_server_info(server_information_t **server_info)
 * Allocates the server_info.
 *
 * </doc>
 */
void allocate_server_info(server_information_t **server_info)
{
   *server_info = malloc(sizeof(server_information_t));

    allocate_mcast_group_list(server_info);
}

/* <doc>
 * mcast_group_node_t *allocate_mcast_group_node(server_information_t **server_info)
 * Allocates the group node and links it with server info.
 * Returns group node if allocated, otherwise returns NULL. 
 *
 * </doc>
 */
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
   new_group_node->group_port = INT_MAX;
   new_group_node->fsm_state = STATE_NONE;
   new_group_node->grp_capability = 0;
   new_group_node->server_metrics = NULL;
   new_group_node->is_metrics_valid = false;
   /* No issues observed on server side so far. Doing intialization as a precaution. */
   new_group_node->timer_id = 0;
   
   new_group_node->group_reachability_index = GROUP_REACHABLE;
   SN_LIST_MEMBER_INSERT_HEAD(&((*server_info)->server_list->group_node),
                             new_group_node,
                             list_element);
   return new_group_node;
}

/* <doc>
 * void deallocate_mcast_group_node(server_information_t *server_info, mcast_group_node_t *node)
 * Deallocates group node and removes it from server info list, frees it.
 *
 * </doc>
 */
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


/* <doc>
 * void  get_group_node_by_best_fit(server_information_t **server_info, unsigned int filesize, mcast_group_node_t **grp_node)
 * Traverses server_info list and returns the group node pointer which is best fit for a task as per its capability.
 * Best fit logic -
 * Select group which has high processing clients or large number of clients with good processing power.
 *
 * </doc>
 */
void get_group_node_by_best_fit(server_information_t **server_info, unsigned int task_size, mcast_group_node_t **grp_node)
{
   mcast_group_node_t *group_node;
   char groupIP[INET_ADDRSTRLEN];
   int min_subtask_size = INT_MAX, subtask_size = INT_MAX;

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   /*Scan over all groups which are free. if any group has moderator_client, that means it is busy.*/
   while (group_node && !group_node->moderator_client)
   {
       /*
       Choosing whichever group gives minimum subtask_size.
       Assuming parallel nature of tasks i.e. no recomputations required.
       */
       if (group_node->grp_capability)
          subtask_size = task_size / group_node->grp_capability;

       if (subtask_size < min_subtask_size )
       {
          *grp_node = group_node;
          min_subtask_size = subtask_size;
       }

       group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                            mcast_group_node_t,
                                            list_element);
   }
}

/* <doc>
 * void  get_group_node_by_name(server_information_t **server_info, char *exp_grp_name,
 *                              mcast_group_node_t **grp_node)
 * Traverses server_info list and returns the group node pointer which is required.
 * exp_grp_name - is group name expected.
 *
 * </doc>
 */
void  get_group_node_by_name(server_information_t **server_info, char *exp_grp_name,
                             mcast_group_node_t **grp_node)
{
   mcast_group_node_t *group_node;
   char groupIP[INET_ADDRSTRLEN];

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   while (group_node)
   {
     if (strncmp(group_node->group_name, exp_grp_name, 2) == 0)
     {
        /* Return on finding the node */
        *grp_node = group_node;
        return;
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                        mcast_group_node_t,
                                        list_element);
   }

}

/* <doc>
 * mcast_group_node_t* get_group_node_by_timerid(server_information_t **server_info, char *exp_grp_name)
 * Traverses server_info list and returns the group node pointer which is required.
 * timer_ptr - is address of the timer we are looking for.
 *
 * </doc>
 */
void  get_group_node_by_timerid(server_information_t **server_info, timer_t *timer_ptr,
                             mcast_group_node_t **grp_node)
{
   mcast_group_node_t *group_node;
   char groupIP[INET_ADDRSTRLEN];

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   while (group_node)
   {
     if (*timer_ptr == group_node->timer_id)
     {
        /* Return on finding the node */
        *grp_node = group_node;
        return;
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                        mcast_group_node_t,
                                        list_element);
   }

}

/* Invoked internally by display_mcast_client_node */
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

      if(debug_mode) {
          sprintf(buf,
          "\n\tClient Address: %s \t Client FD: %u",
          ipaddr, client_node->client_id);
      } else {
          sprintf(buf, "\n\tClient Address: %s", ipaddr);
      }

      SIMPLE_PRINT(buf);

      client_node = SN_LIST_MEMBER_NEXT(client_node,
                                        mcast_client_node_t,
                                        list_element);
   }
}

/* <doc>
 * void display_mcast_group_node(server_information_t **server_info)
 * Traverses server_info list and displays all the information regarding
 * groups and their associated clients
 *
 * </doc>
 */
void display_mcast_group_node(server_information_t **server_info, display_show_type_t show_type)
{
  mcast_group_node_t *group_node = NULL;
  char buf[100];
  char groupIP[INET6_ADDRSTRLEN], moderatorIP[INET6_ADDRSTRLEN];

   group_node =     SN_LIST_MEMBER_HEAD(&((*server_info)->server_list->group_node),
                                        mcast_group_node_t,
                                        list_element);

   while (group_node)
   {
     inet_ntop(AF_INET, get_in_addr((struct sockaddr *)(&group_node->grp_mcast_addr)), groupIP, INET6_ADDRSTRLEN);

     if (group_node->moderator_client) {
        inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(group_node->moderator_client->client_addr)), moderatorIP, INET6_ADDRSTRLEN);
     } else {
        sprintf(moderatorIP, "None");
     }

     if(debug_mode) {
        sprintf(buf,
        "\n\n\rGroup Name: %s    Group IP: %s    Group Port: %d   Client Count: %d   Capability: %d",
        group_node->group_name, groupIP, group_node->group_port, group_node->number_of_clients, group_node->grp_capability);
     } else {
        sprintf(buf,
        "\n\n\rGroup Name: %s   Client Count: %d   Moderator: %s",
        group_node->group_name, group_node->number_of_clients, moderatorIP);
     }

     SIMPLE_PRINT(buf);

     if ((show_type == SHOW_ALL) &&
         (group_node->client_info))
     {
       display_mcast_client_node(&group_node);
     }

     group_node =     SN_LIST_MEMBER_NEXT(group_node,
                                        mcast_group_node_t,
                                        list_element);
   }
}

/* <doc>
 * void display_mcast_group_node_by_name(server_information_t **server_info, char *grp_name)
 * Display server_info list output for a particular group name
 *
 * </doc>
 */
void display_mcast_group_node_by_name(server_information_t **server_info, char *grp_name)
{
  char buf[100];
  char groupIP[INET_ADDRSTRLEN], moderatorIP[INET6_ADDRSTRLEN];

  mcast_group_node_t *group_node = NULL;
  get_group_node_by_name(server_info,grp_name,&group_node);

   if (group_node)
   {
      inet_ntop(AF_INET, get_in_addr((struct sockaddr *)(&group_node->grp_mcast_addr)), groupIP, INET6_ADDRSTRLEN);

      if (group_node->moderator_client) {
         inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(group_node->moderator_client->client_addr)), moderatorIP, INET6_ADDRSTRLEN);
      } else {
         sprintf(moderatorIP, "None");
      }

      if(debug_mode) {
         sprintf(buf,
         "\n\n\rGroup Name: %s    Group IP: %s    Group Port: %d   Client Count: %d   Capability: %d",
         group_node->group_name, groupIP, group_node->group_port, group_node->number_of_clients, group_node->grp_capability);
      } else {
         sprintf(buf,
         "\n\n\rGroup Name: %s   Client Count: %d   Moderator: %s",
         group_node->group_name, group_node->number_of_clients, moderatorIP);
      }

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


bool search_client_in_group_node(mcast_group_node_t *group_node, unsigned int target_id, mcast_client_node_t **clnt_node)
{
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];

      if (group_node->client_info)
      {
           mcast_client_node_t *client_node = NULL, *temp_node = NULL;

           client_node = SN_LIST_MEMBER_HEAD(&(group_node->client_info->client_node),
                                              mcast_client_node_t,
                                              list_element);

            while(client_node)
            {
               if (client_node->client_id == target_id)
               {
                    *clnt_node = client_node;
                    return TRUE;
               }
                 client_node = SN_LIST_MEMBER_NEXT(client_node,
                                                  mcast_client_node_t,
                                                  list_element);
            }
      }
  return FALSE;
}

/* <doc>
 * bool remove_client_from_mcast_group_node(server_information_t **server_info, char *grp_name, int target_id)
 * This functions removes a client from group node. It first searches for the group node and 
 * removes the request client from list.
 * Return TRUE if successful, otherwise FALSE.
 * </doc>
 */
bool remove_client_from_mcast_group_node(server_information_t **server_info, mcast_group_node_t *group_node, unsigned int target_id)
{
  char buf[100];
  char groupIP[INET_ADDRSTRLEN];

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
               if (client_node->client_id == target_id)
               {
                    deallocate_mcast_client_node(group_node, client_node);

                    group_node->number_of_clients--;

                    /*deallocate client_info if there are no clients present for the group*/
                    if (SN_LIST_LENGTH(&group_node->client_info->client_node) == 0)
                    {
                        free(group_node->client_info);
                        group_node->client_info = NULL;
                    }
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

/* <doc>
 * void ADD_GROUP_IN_LL(server_information_t **server_info, char *group_name, struct sockaddr_in group_addrIP, unsigned int port)
 * Adds the group node in server_info list.
 *
 * </doc>
 */
void ADD_GROUP_IN_LL(server_information_t **server_info, char *group_name, struct sockaddr_in group_addrIP, unsigned int port)
{
  mcast_group_node_t *group_node = NULL;

  group_node = allocate_mcast_group_node(server_info);

  group_node->group_name = group_name;
  group_node->grp_mcast_addr = group_addrIP;
  group_node->group_port = port;
  group_node->moderator_client = NULL;
}

/* <doc>
 * void ADD_CLIENT_IN_GROUP(mcast_group_node_t **group_node, struct sockaddr *addr, unsigned int client_id)
 * Add the client node in group_node list. client node contains information related
 * to socket address and socket FD.
 *
 * </doc>
 */
void ADD_CLIENT_IN_GROUP(mcast_group_node_t **group_node, struct sockaddr *addr, unsigned int client_id)
{
  mcast_client_node_t *client_node = NULL;
  client_node = allocate_mcast_client_node(group_node);
  memcpy(&(client_node->client_addr),addr,sizeof(*addr));
  client_node->client_id = client_id;
  /*client will be free initially*/
  client_node->av_status = CLFREE;
}
