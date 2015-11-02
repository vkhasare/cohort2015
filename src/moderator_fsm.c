#include "client_DS.h"
#include "print.h"

/* <doc>
 * void moderator_fsm_notify_rsp_pending_state(client_information_t *client_info,
 *                                             moderator_event_t event,
 *                                             void *fsm_msg)
 * This is the function handler when moderator has not dispatched the
 * moderator notify response to server.
 *
 * </doc>
 */
void moderator_fsm_notify_rsp_pending_state(client_information_t *client_info,
                                            moderator_event_t event,
                                            void *fsm_msg)
{
   switch (event) {
   case MOD_ECHO_REQ_RCVD_EVENT:
          moderator_echo_req_notify_rsp_pending_state(client_info,
                                                      fsm_msg);
          break;
   default:
          /*ignore case*/
          break;

   }
}

/* <doc>
 * void moderator_fsm_task_rsp_pending_state(client_information_t *client_info,
 *                                           moderator_event_t event,
 *                                           void *fsm_msg)
 * This is the function handler when moderator has not dispatched the task response
 * to Server and some event comes.
 *
 * </doc>
 */
void moderator_fsm_task_rsp_pending_state(client_information_t *client_info,
                                          moderator_event_t event,
                                          void *fsm_msg)
{
   switch (event) {
   case MOD_ECHO_REQ_RCVD_EVENT:
          moderator_echo_req_task_rsp_pending_state(client_info,
                                                   fsm_msg);
          break;
   default:
          /*ignore case*/
          break;

   }
}

/* <doc>
 * bool moderator_main_fsm(client_information_t *client_info,
 *                         moderator_event_t event,
 *                         void *fsm_msg)
 * Main FSM Handler of Moderator
 *
 * </doc>
 */
bool moderator_main_fsm(client_information_t *client_info,
                        moderator_event_t event,
                        void *fsm_msg)
{
   fsm_data_t *fsm_data = (fsm_data_t *) fsm_msg;

   switch (fsm_data->fsm_state) {
   case MODERATOR_NOTIFY_RSP_PENDING:
          moderator_fsm_notify_rsp_pending_state(client_info,event,fsm_msg);
          break;
   case MODERATOR_TASK_RSP_PENDING:
          moderator_fsm_task_rsp_pending_state(client_info,event,fsm_msg);
          break;
    default:
          /*ignore case*/
          break;
   }
}
