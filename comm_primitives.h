#ifndef COMM_PRIMITIVES_H
#define COMM_PRIMITIVES_H

#include <rpc/rpc.h>
#include <stdbool.h>
#include <unistd.h>

typedef enum struct_id{
    client_req    = 2001,
    client_init   = 2002,
    server_task   = 2003,
    client_answer = 2004,
    client_leave  = 2005
}e_struct_id_t; 

typedef struct client_req{
    unsigned int num_groups;
    char* group_ids; 
}client_req_t;

typedef struct client_init{
    int num_groups;
    int* group_ips; 
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
    }idv;
}comm_struct_t;

extern struct xdr_discrim comm_discrim[];

void print_my_struct(my_struct_t* m);
void populate_my_struct(my_struct_t* m);
bool process_my_struct(my_struct_t* m, XDR* xdrs);
int rdata ();
int wdata ();
#endif
