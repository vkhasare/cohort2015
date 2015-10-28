#include <stdbool.h>
#include "server_DS.h"
#include "print.h"

/*
typedef enum{
  MODERATOR_SELECTION_PENDING = 1,
  MODERATOR_SELECTED          = 2,
  STATE_MAX
}server_state_t;

typedef enum{
  FIRST_ECHO_RSP_RCVD = 1,
  EVENT_MAX
}server_event_t;
*/

void server_fsm_mod_pend_state(server_information_t *server_info,
                               server_event_t event,
                               void *fsm_msg)
{

   switch (event) {
   case ECHO_RSP_RCVD_EVENT:
          mcast_send_chk_alive_msg(server_info,
                                   fsm_msg);
          break;
   default:
          /*ignore case*/
          break;

   }
}

void server_fsm_mod_selected (server_information_t *server_info,
                               server_event_t event,
                               void *fsm_msg)
{
   switch (event) {
   case MOD_NOTIFY_RSP_RCVD_EVENT:
          mcast_start_task_distribution(server_info,
                                        fsm_msg);
          break;
   default:
          /*ignore case*/
          break;

   }
}

bool server_callline_fsm(server_information_t *server_info,
      server_event_t event,
      void *fsm_msg)
{
   fsm_data_t *fsm_data = (fsm_data_t *) fsm_msg;
   
   switch (fsm_data->fsm_state) {
   case MODERATOR_SELECTION_PENDING:
          server_fsm_mod_pend_state(server_info,event,fsm_msg);
          break;
   case MODERATOR_SELECTED:
          server_fsm_mod_selected(server_info,event,fsm_msg);
          break;
    default:
          /*ignore case*/
          break;
   }
}
