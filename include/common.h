#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "comm_primitives.h"
#include "print.h"
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/mman.h>

#define PORT "3490"
#define TIMEOUT_SECS 5
#define BACKLOG 30000
#define MAXEVENTS 30000
#define MAXDATASIZE 100
#define CLOCKID CLOCK_REALTIME

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

extern const unsigned int max_groups;
extern const unsigned int max_gname_len; //includes nul termination

/* <doc>
 * EXTRACT_ARG(lastArg, type, var)
 * Macro to extract variadic parameter
 * 
 * where, lastArg - name of last function argument
 *        type - data type of expected variable
 *        varName - name of destination variable
 * </doc>
 */
#define EXTRACT_ARG(lastArg, type, varName)     \
{                                              \
  va_list arguments;                            \
  va_start (arguments, lastArg);                \
  varName = va_arg (arguments, type);           \
  va_end (arguments);                           \
}

#define MATCH_STRING(str1,str2) strcmp(str1,str2)==0

/* <doc>
 * enum_mode_t
 * Consists of 
 * - CLIENT_MODE
 * - SERVER_MODE
 * </doc>
 */
typedef enum {
  CLIENT_MODE = 101,
  SERVER_MODE = 102
} enum_mode_t;

/* <doc>
 * enum msg_cause
 * Consists of Response message cause
 * - ACCEPTED
 * - REJECTED
 * - UNKNOWN
 * </doc>
 */
typedef enum {
 ACCEPTED = 10,
 REJECTED = 11,
 UNKNOWN  = 12
} msg_cause;

/* <doc>
 * enum chkpoint_type
 * Tells about the type of checkpoint
 *
 * </doc>
 */
typedef enum {
 ADD_CHECKPOINT = 67,
 DEL_CHECKPOINT = 68
} chkpoint_type;

/* <doc>
 * enum client_state
 * determines the state of client.
 *
 * </doc>
 */
typedef enum {
 FREE  = 55,
 BUSY  = 56,
 ALIVE = 57,
 DEAD  = 58
} client_state;

typedef struct grname_ip_mapping{
    char grname[10];
    struct sockaddr_in grp_ip;
    unsigned int port_no;
}grname_ip_mapping_t; 
  
typedef enum {
  INVALID_TASK_TYPE = 0,
  FIND_PRIME_NUMBERS = 1
} task_type_t;
 
typedef int (*fptr)(int, pdu_t *, ...);
int create_and_bind(char *machine_addr, char *machine_port, int oper_mode);
int make_socket_non_blocking (int sfd);
char *get_in_addr(struct sockaddr *sa);
int IS_SERVER(int oper);
int IS_CLIENT(int oper);
//uint32_t initialize_mapping(const char* filename, grname_ip_mapping_t ** mapping, server_information_t ** server_info);
void display_mapping(grname_ip_mapping_t * mapping, uint32_t count);
void display_clis();
void initialize_echo_request(echo_req_t *echo_req);
char* enum_to_str(msg_cause cause);
msg_cause str_to_enum(char *str);
inline unsigned int calc_key(struct sockaddr *sa);
void get_my_ip(const char *, struct sockaddr *);
int send_echo_request(const int sockfd, struct sockaddr *addr, char *grp_name);
void start_oneshot_timer(timer_t *t, uint8_t interval, uint32_t sigval);
void start_recurring_timer(timer_t *t, uint8_t interval, uint32_t sigval);
unsigned int generate_random_capability(void);
unsigned int get_task_count(const char* );
char * fetch_file(char *, char *);
void inline create_folder(char * path);
void inline check_and_create_folder(char * path);
float timedifference_msec(struct timeval t0, struct timeval t1);

#define LOGGING_WARNING(...)                \
logging_warning(SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

#define LOGGING_INFO(...)               \
logging_info(SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

#define LOGGING_ERROR(...)                  \
logging_info(SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

#define LOGGING_WARNING_IF(cond, ...)             \
logging_warning_if(cond, SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

#define LOGGING_INFO_IF(cond, ...)                  \
logging_info_if(cond, SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

#define LOGGING_ERROR_IF(cond, ...)                 \
logging_error_if(cond, SPRINTF(__VA_ARGS__), __FILE__, __LINE__);

