#ifndef COMM_PRIMITIVES_H
#define COMM_PRIMITIVES_H

#include <rpc/rpc.h>
#include <stdbool.h>
#include <unistd.h>

#define MALLOC_IE(count)                            \
 ({                                                 \
   (string_t*) malloc (sizeof(string_t) * count);   \
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
    leave_response  = 39
}e_struct_id_t; 

typedef struct string{
    char* str;
}string_t;

typedef struct local_sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    unsigned long s_addr;
    char *group_name;
    unsigned int grp_port;
}l_saddr_in_t;

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
    unsigned int client_id;
    string_t* group_ids;
}leave_req_t;

typedef struct leave_response{
    unsigned int num_groups;
    string_t* group_ids;
    string_t* cause;
}leave_rsp_t;

typedef struct my_struct{
    int a;
    float b;
    unsigned int c_count;
    double d;
    int *c;
}my_struct_t;

typedef struct common_struct{
    e_struct_id_t id;
    union{
        join_req_t join_req;
        join_rsp_t join_rsp;
        string_t echo_req;
        string_t echo_resp;
        leave_req_t leave_req;
        leave_rsp_t leave_rsp;
    }idv;
}comm_struct_t;

void print_structs(comm_struct_t* m);
void populate_my_struct(my_struct_t*, int);
bool process_my_struct(my_struct_t*, XDR*);
int rdata ();
int wdata ();
#endif
