#include "server_DS.h"
#include "print.h"

/* <doc>
 * void server_fsm_mod_pend_state(server_information_t *server_info,
 *                                server_event_t event,
 *                                void *fsm_msg)
 * This is the fsm handler when group is in moderator selection pending
 * state.
 * Events could be
 * - echo response from client comes for becoming moderator.
 *
 * </doc>
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

/* <doc>
 * void server_fsm_mod_selected (server_information_t *server_info,
 *                               server_event_t event,
 *                               void *fsm_msg)
 * This is the fsm handler when a moderator has been selected for
 * a group and any event comes.
 *
 * </doc>
 */
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

/* <doc>
 * void server_fsm_task_in_progress_mod_sel_pend(server_information_t *server_info,
 *                                               server_event_t event,
 *                                               void *fsm_msg)
 * This is the fsm handler when server group is in task in progress state
 * and moderator is detected as dead, then reselection of moderator should
 * happen.
 *
 * </doc>
 */
void server_fsm_task_in_progress_mod_sel_pend(server_information_t *server_info,
                                              server_event_t event,
                                              void *fsm_msg)
{
   switch (event) {
   case ECHO_RSP_RCVD_EVENT:
          send_new_moderator_info(server_info,
                                  fsm_msg);
          break;
   }

}

/* <doc>
 * void server_fsm_task_in_progress(server_information_t *server_info,
 *                                  server_event_t event,
 *                                  void *fsm_msg)
 * This is the fsm handler when server has delegated some task to
 * a group and group is now in task in progress state.
 *
 * </doc>
 */
void server_fsm_task_in_progress(server_information_t *server_info,
                                 server_event_t event,
                                 void *fsm_msg)
{
   switch (event) {
   case ECHO_REQ_RCVD_EVENT:
          server_echo_req_task_in_progress_state(server_info,
                                                 fsm_msg);
          break;
   case MOD_TASK_RSP_RCVD_EVENT:
          mcast_handle_task_response(server_info,
                                        fsm_msg);
          break;
   default:
          /*ignore case*/
          break;

   }
}

/* <doc>
 * bool server_main_fsm(server_information_t *server_info,
 *                      server_event_t event,
 *                      void *fsm_msg)
 * Main FSM Handler of Server
 *
 * </doc>
 */
bool server_main_fsm(server_information_t *server_info,
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
   case GROUP_TASK_IN_PROGRESS:
          server_fsm_task_in_progress(server_info,event,fsm_msg);
          break;
   case TASK_IN_PROGRESS_MOD_SEL_PEND:
          server_fsm_task_in_progress_mod_sel_pend(server_info,event,fsm_msg);
          break;
   default:
          /*ignore case*/
          break;
   }
}

