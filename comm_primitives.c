#include "comm_primitives.h"

#define PRINT_PROMPT(str)                  \
 do {                                      \
   write(STDOUT_FILENO,"\n",1);            \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

#define PRINT(str)                         \
 do {                                      \
   write(STDOUT_FILENO,"\n\n",2);          \
   write(STDOUT_FILENO,"\t",1);            \
   write(STDOUT_FILENO,str,strlen(str));   \
 } while(0)

const unsigned int max_groups = 1000;
const unsigned int max_gname_len = 5; //includes nul termination

struct xdr_discrim comm_discrim[] = {
    {client_req, NULL},
    {client_init, NULL},
//  {server_task, NULL},
//  {client_answer, NULL},
//  {client_leave, NULL},
    { __dontcare__, NULL }
};

int rdata (FILE *fp, char* buf, int n)
{
    if (!(n = fread (buf, 1, n, fp)))
        return (-1);
    else
        return (n);
}

int wdata (FILE *fp, char* buf, int n)
{
    if (!(n = fwrite (buf, 1, n, fp)))
        return (-1);
    else
        return (n);
}

void print_my_struct(my_struct_t* m){
    char buf[512];
    sprintf(buf, 
            "\na=%d\nb=%f\nc=%u\nd=%lf\nc[0]=%d", 
            m->a,m->b,m->c_count,m->d,m->c[0]);
    PRINT(buf);
    return;
}

void print_client_req(client_req_t* m){
    char buf[512];
    int iter;
    sprintf(buf,"\nNumber of groups=%d", 
            m->num_groups);
    for(iter = 0; iter < m->num_groups; iter++){
        sprintf(buf,"\n(%d): %s", iter, m->group_ids[iter]);
    }
    PRINT(buf);
    return;
}


void populate_my_struct(my_struct_t* m){
    m->a = 10;
    m->b = 13.9;
    m->d = 3.14156;
    m->c_count = 4; 
    m->c = (int *) malloc (sizeof(int)*m->c_count);
    m->c[0]=1;
    m->c[1]=10;
    m->c[2]=100;
}

bool process_my_struct(my_struct_t* m, XDR* xdrs){
    char buf[512];
    bool ret_res = (xdr_int(xdrs, &(m->a))  && 
           xdr_float(xdrs, &(m->b)) && 
           xdr_u_int(xdrs, &(m->c_count)) &&
           xdr_double(xdrs, &(m->d)) &&
           xdr_array(xdrs, (char**)&(m->c), &(m->c_count), 100,
                      sizeof(int),(xdrproc_t )xdr_int ));

    if(ret_res){
        if (xdrs->x_op == XDR_ENCODE){
            sprintf(buf, "[I] Push res: %d, Encode res: %d", 
                    xdrrec_endofrecord(xdrs, TRUE), ret_res);
            PRINT(buf);
            xdrrec_endofrecord(xdrs, TRUE);
        }
        if (xdrs->x_op == XDR_DECODE){
            print_my_struct(m);
            sprintf(buf, "[I] Skip rec res: %d, Decode res: %d", 
                    xdrrec_skiprecord (xdrs), ret_res);
            PRINT(buf);
            xdrrec_skiprecord (xdrs);
        }
    }
    return ret_res;
}

bool process_client_req(client_req_t* m, XDR* xdrs){
    char buf[max_groups * max_gname_len + 100];
    bool ret_res = (xdr_u_int(xdrs, &(m->num_groups)) &&
           xdr_array(xdrs, (char**)&(m->group_ids), &(m->num_groups), max_groups,
                      sizeof(char),(xdrproc_t )xdr_int ));
    if(ret_res){
        if (xdrs->x_op == XDR_ENCODE){
            sprintf(buf, "[I] Push res: %d, Encode res: %d", 
                    xdrrec_endofrecord(xdrs, TRUE), ret_res);
            PRINT(buf);
            xdrrec_endofrecord(xdrs, TRUE);
        }
        if (xdrs->x_op == XDR_DECODE){
            print_client_req(m);
            sprintf(buf, "[I] Skip rec res: %d, Decode res: %d", 
                    xdrrec_skiprecord (xdrs), ret_res);
            PRINT(buf);
            xdrrec_skiprecord (xdrs);
        }
    }
}

bool process_client_init(my_struct_t* m, XDR* xdrs){
}

bool process_server_task(my_struct_t* m, XDR* xdrs){
    return false;
}

bool process_client_answer(my_struct_t* m, XDR* xdrs){
    return false;
}

bool process_client_leave(my_struct_t* m, XDR* xdrs){
    return false;
}

