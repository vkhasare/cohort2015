#ifndef COMM_PRIMITIVES_H
#define COMM_PRIMITIVES_H

#include <rpc/rpc.h>
#include <stdbool.h>
#include <unistd.h>

#define MALLOC_STR_IE(count)                        \
 ({                                                 \
   (string_t*) malloc (sizeof(string_t) * count);   \
 })

#define MALLOC_UARRAY_IE(count)                       \
 ({                                                 \
   (uarray_t*) malloc (sizeof(uarray_t) * count);   \
 })

#define MALLOC_STR                                  \
 ({                                                 \
   (char*) malloc(sizeof(char) * max_gname_len);    \
 })

typedef enum struct_id{
    join_request    = 32,
    join_response   = 33,
    server_task     = 34,
    client_answer   = 35,
    echo_req        = 36,
    echo_response   = 37,
    leave_request   = 38,
    leave_response  = 39,
    moderator_notify_req = 40,
    moderator_notify_rsp = 41,
    TASK_RESPONSE
}e_struct_id_t; 

typedef struct string{
    char* str;
}string_t;

typedef struct uarray{
    unsigned int* val;
}uarray_t;

typedef struct local_sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    unsigned long s_addr;
    char *group_name;
    unsigned int cause;
    unsigned int grp_port;
}l_saddr_in_t;

typedef struct echo_req{
    char* group_name;
} echo_req_t;

typedef struct echo_response{
    unsigned int status;
    char* group_name;
} echo_rsp_t;

typedef struct join_request{
    unsigned int num_groups;
    string_t* group_ids; 
}join_req_t;

typedef struct join_response{
    unsigned int num_groups;
    l_saddr_in_t* group_ips; 
//  int* group_ips; 
}join_rsp_t;

typedef struct leave_request{
    unsigned int num_groups;
    string_t* group_ids;
}leave_req_t;

typedef struct leave_response{
    unsigned int num_groups;
    string_t* group_ids;
    unsigned int cause;
}leave_rsp_t;

typedef struct moderator_notify_req {
    unsigned int moderator_id;
    unsigned int moderator_port;
    int num_of_clients_in_grp;
    char* group_name;
}moderator_notify_req_t;

typedef struct moderator_notify_rsp {
    char* group_name;
    unsigned int moderator_id;
    int client_id_count;
    unsigned int* client_ids;
}moderator_notify_rsp_t;

typedef struct my_struct{
    int a;
    float b;
    unsigned int c_count;
    double d;
    int *c;
}my_struct_t;

typedef enum response_type{
   TYPE_CHAR,
   TYPE_INT,
   TYPE_LONG
}rsp_type_t;

typedef struct result{
   unsigned int size;
   int * value;
}result_t;

typedef struct task_response{
   char* group_name;
//   unsigned int task_id;
   rsp_type_t type;
   unsigned int num_clients;
   unsigned int * client_ids;
   result_t ** result;
}task_rsp_t;

typedef struct common_struct{
    e_struct_id_t id;
    union{
        join_req_t join_req;
        join_rsp_t join_rsp;
        echo_req_t echo_req;
        echo_rsp_t echo_resp;
        leave_req_t leave_req;
        leave_rsp_t leave_rsp;
        moderator_notify_req_t moderator_notify_req;
        moderator_notify_rsp_t moderator_notify_rsp;
        task_rsp_t task_rsp;
    }idv;
}comm_struct_t;



typedef struct pdu{
  struct sockaddr peer_addr;
  comm_struct_t msg;
}pdu_t;

void print_structs(comm_struct_t* m);
void populate_my_struct(my_struct_t*, int);
bool process_my_struct(my_struct_t*, XDR*);
int rdata ();
int wdata ();


    
#endif
