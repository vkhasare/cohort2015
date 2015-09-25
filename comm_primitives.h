#ifndef COMM_PRIMITIVES_H
#define COMM_PRIMITIVES_H

#include <rpc/rpc.h>
#include <stdbool.h>
#include <unistd.h>

typedef enum struct_id{
    client_req    = 1,
    client_init   = 2,
    server_task   = 3,
    client_answer = 4,
    client_leave  = 5,
    echo_req      = 6,
    echo_response = 7
}e_struct_id_t; 

typedef struct string{
    char* str;
}string_t;

typedef struct local_sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    unsigned long s_addr;
    char *group_name;
}l_saddr_in_t;

typedef struct client_req{
    unsigned int num_groups;
    string_t* group_ids; 
}client_req_t;

typedef struct client_init{
    unsigned int num_groups;
    l_saddr_in_t* group_ips; 
//  int* group_ips; 
}client_init_t;

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
        client_req_t cl_req;
        client_init_t cl_init;
        string_t echo_req;
        string_t echo_resp;
    }idv;
}comm_struct_t;

void print_structs(comm_struct_t* m);
void populate_my_struct(my_struct_t*, int);
bool process_my_struct(my_struct_t*, XDR*);
int rdata ();
int wdata ();
#endif
