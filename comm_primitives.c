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

#if 0
bool process_client_req();
bool process_client_init();
bool process_server_task();
bool process_client_answer();
bool process_client_leave();
#endif

const unsigned int max_groups = 1000;
const unsigned int max_gname_len = 25; //includes nul termination
const unsigned int echo_req_len = 256; //includes nul termination
const unsigned int echo_resp_len = 21; //includes nul termination

bool process_comm_struct(XDR* xdrs, comm_struct_t* m);

int rdata (char* fp, char* buf, int n)
{
    if (!(n = fread (buf, 1, n, (FILE *)fp)))
        return (-1);
    else
        return (n);
}

int wdata (char* fp, char* buf, int n)
{
    if (!(n = fwrite (buf, 1, n, (FILE *)fp)))
        return (-1);
    else
        return (n);
}

static
unsigned int get_index(uint8_t idx){
    return(max_gname_len * idx);
}

void print_my_struct(my_struct_t* m){
    char buf[512];
    sprintf(buf, 
            "\na=%d\nb=%f\nc=%u\nd=%lf\nc[0]=%d", 
            m->a,m->b,m->c_count,m->d,m->c[0]);
    PRINT(buf);
    return;
}

void print_echo_msg(char* echo_str){
    PRINT(echo_str);
    return;
}

void print_join_req(join_req_t* m){
    char buf[max_groups * max_gname_len * 8 + 14];
    int iter;
    sprintf(buf,"\nNumber of groups=%d", 
            m->num_groups);
    for(iter = 0; iter < m->num_groups; iter++){
//      sprintf(buf, "\n(%d): %s", iter, m->group_ids[get_index(iter)]);
        sprintf(buf, "\n(%d): %s", iter, m->group_ids[iter].str);
        PRINT(buf);
    }
    return;
}

void print_join_rsp(join_rsp_t* m){
    char buf[max_groups * 15 * 8 + 14];
    int iter;
    sprintf(buf,"\nNumber of groups=%d", 
            m->num_groups);
    for(iter = 0; iter < m->num_groups; iter++){
        sprintf(buf,"\n(%d): %d", iter, m->group_ips[iter]);
        PRINT(buf);
    }
    return;
}

void populate_my_struct(my_struct_t* m, int some_rand){
    m->a = 10 * some_rand;
    m->b = 13.9 * some_rand;
    m->d = 3.14156 * some_rand;
    m->c_count = 4; 
    m->c = (int *) malloc (sizeof(int)*m->c_count);
    m->c[0]=1;
    m->c[1]=10;
    m->c[2]=100;
}

void populate_join_req(comm_struct_t* m, char** grnames, int num_groups){
    int iter;
    m->idv.join_req.num_groups = num_groups; 
//    m->idv.join_req.group_ids = (string_t*) malloc (sizeof(string_t) * num_groups);
    m->idv.join_req.group_ids = MALLOC_IE(num_groups);
    for(iter = 0; iter < num_groups; iter++){
#define MALLOC_STRING(x)    m->idv.join_req.group_ids[(x)].str = MALLOC_STR
        MALLOC_STRING(iter);
#undef MALLOC_STRING

#define SUBS_TXT(x) (m->idv.join_req.group_ids[(x)].str)
        strcpy(SUBS_TXT(iter),grnames[iter]);
#undef SUBS_TXT     
    }
    
}

void populate_leave_req(comm_struct_t* m, char** grnames, int num_groups){
    int iter;
    m->idv.leave_req.num_groups = num_groups;
    m->idv.leave_req.group_ids = MALLOC_IE(num_groups);

    for(iter = 0; iter < num_groups; iter++){
#define MALLOC_STRING(x)    m->idv.leave_req.group_ids[(x)].str = MALLOC_STR
        MALLOC_STRING(iter);
#undef MALLOC_STRING

#define SUBS_TXT(x) (m->idv.leave_req.group_ids[(x)].str)
        strcpy(SUBS_TXT(iter),grnames[iter]);
#undef SUBS_TXT
    }
}

void populate_leave_rsp(comm_struct_t* m, char** grnames, char **cause, int num_groups){
    int iter;
    m->idv.leave_rsp.num_groups = num_groups;
    m->idv.leave_rsp.group_ids = MALLOC_IE(num_groups);

    for(iter = 0; iter < num_groups; iter++){
#define MALLOC_STRING(x)    \
m->idv.leave_rsp.group_ids[(x)].str = MALLOC_STR;  \
m->idv.leave_rsp.cause[(x)].str = MALLOC_STR

        MALLOC_STRING(iter);

#undef MALLOC_STRING

#define SUBS_TXT(x) (m->idv.leave_rsp.group_ids[(x)].str)
#define SUBS_CAUSE(x) (m->idv.leave_rsp.cause[(x)].str)


        strcpy(SUBS_TXT(iter),grnames[iter]);
        strcpy(SUBS_CAUSE(iter),cause[iter]);
#undef SUBS_TXT
    }
}

void populate_client_init(comm_struct_t* m){
    m->id = join_response;
    m->idv.join_rsp.num_groups = 4; /*
    m->idv.cl_init.group_ips = (int *) malloc (sizeof(int) *
                                               m->idv.cl_init.num_groups);
    m->idv.cl_init.group_ips[0]=7945801;
    m->idv.cl_init.group_ips[1]=1825090;
    m->idv.cl_init.group_ips[2]=1583060;
    m->idv.cl_init.group_ips[3]=1342003;*/
}

void print_structs(comm_struct_t* m){
    switch(m->id){
        case join_request: 
            print_join_req(&m->idv.join_req);
            break;
        case join_response: 
            print_join_rsp(&m->idv.join_rsp);
            break;
        case echo_req:
            print_echo_msg(m->idv.echo_req.str);
            break;
        case echo_response:
//          print_echo_msg(m->idv.echo_resp.str);
            break;
        default:
//            PRINT("\nstructure unhandled as of now.");
            break;
    }
    return;
}

bool process_my_struct(my_struct_t* m, XDR* xdrs){
   return (xdr_int(xdrs, &(m->a))  && 
           xdr_float(xdrs, &(m->b)) && 
           xdr_u_int(xdrs, &(m->c_count)) &&
           xdr_double(xdrs, &(m->d)) &&
           xdr_array(xdrs, (char**)&(m->c), &(m->c_count), 100,
                      sizeof(int),(xdrproc_t )xdr_int ));
}

bool postprocess_struct_stream(comm_struct_t* m, XDR* xdrs, bool op_res){
//  char buf[512];
    bool ret_res = false;
    
    if (xdrs->x_op == XDR_ENCODE){
        ret_res = xdrrec_endofrecord(xdrs, TRUE);
//      sprintf(buf, "[I] Encode res: %d, Push res: %d, ", op_res ,ret_res);
//      PRINT(buf);
    }
    
    if (xdrs->x_op == XDR_DECODE){
        ret_res = xdrrec_skiprecord (xdrs);
//      sprintf(buf, "[I] Decode res: %d, Skip rec res: %d, ", op_res, ret_res);
//      PRINT(buf);
        op_res ? print_structs(m): NULL;
    }
    return ret_res;
}

bool postprocess_my_struct_stream(my_struct_t* m, XDR* xdrs, bool op_res){
    char buf[512];
    bool ret_res = false;
    
    if (xdrs->x_op == XDR_ENCODE){
        ret_res = xdrrec_endofrecord(xdrs, TRUE);
        sprintf(buf, "[I] Encode res: %d, Push res: %d, ", op_res ,ret_res);
        PRINT(buf);
    }
    
    if (xdrs->x_op == XDR_DECODE){
        ret_res = xdrrec_skiprecord (xdrs);
        sprintf(buf, "[I] Decode res: %d, Skip rec res: %d, ", op_res, ret_res);
        PRINT(buf);
        op_res ? print_my_struct(m): NULL;
    }
    return ret_res;
}

void write_record(int sockfd, comm_struct_t* m){
    FILE* fp = fdopen(sockfd, "wb");
//  my_struct_t m ;
    XDR xdrs ;
    int res;
    
//  populate_my_struct(&m, temp);
    xdrs.x_op = XDR_ENCODE;
    xdrrec_create(&xdrs, 0, 0, (char*)fp, rdata, wdata);

//  PRINT("Sending XDR local struct.");

//  res = process_my_struct(&m, &xdrs);
    res = process_comm_struct(&xdrs, m);
    if (!res)
    {
        PRINT("Error in sending msg.");
    }
    postprocess_struct_stream(m, &xdrs, res);
    xdr_destroy(&xdrs);
    fflush(fp);
}

bool read_my_record(int sockfd){
    FILE* fp = fdopen(sockfd, "rb");
    my_struct_t m ;
    XDR xdrs ;
    int res;

    m.c = NULL;
    xdrs.x_op = XDR_DECODE;

    xdrrec_create(&xdrs,0,0,(char*)fp,rdata,wdata);
    xdrrec_skiprecord(&xdrs);

    do{
        //count = process_comm_struct(&m , &xdrs);
        //postprocess_struct_stream(&m , &xdrs, count);
        res = process_my_struct(&m , &xdrs);
        postprocess_my_struct_stream(&m , &xdrs, res);
    }while(!xdrrec_eof(&xdrs));
    
    xdr_destroy(&xdrs);
}

int read_record(int sockfd, comm_struct_t* m){
    FILE* fp = fdopen(sockfd, "rb");
    XDR xdrs ;
    int res;

    xdrs.x_op = XDR_DECODE;

    xdrrec_create(&xdrs,0,0,(char*)fp,rdata,wdata);
    xdrrec_skiprecord(&xdrs);

    do{
        //count = process_comm_struct(&m , &xdrs);
        //postprocess_struct_stream(&m , &xdrs, count);
        res = process_comm_struct(&xdrs, m);
        postprocess_struct_stream(m , &xdrs, res);
    }while(!xdrrec_eof(&xdrs));
    
    xdr_destroy(&xdrs);
    return m->id;
}

bool xdr_echo_req(XDR* xdrs, string_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->str = NULL;
    }
    return (xdr_string(xdrs, &(m->str), echo_req_len));
}

bool xdr_echo_resp(XDR* xdrs, string_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->str = NULL;
    }
    return (xdr_string(xdrs, &(m->str), echo_resp_len));
}

bool xdr_gname_string(XDR* xdrs, string_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->str = NULL;
    }
    return (xdr_string(xdrs, &(m->str), max_gname_len));
}

bool process_join_req(XDR* xdrs, join_req_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_ids = NULL;
    }
    
    return (xdr_u_int(xdrs, &(m->num_groups)) &&
            xdr_array(xdrs, (char **)&(m->group_ids), &(m->num_groups), max_groups,
                             (sizeof(string_t)),
                             (xdrproc_t )xdr_gname_string));
//          xdr_string(xdrs, &(m->group_ids), max_groups * max_gname_len));
}

bool process_leave_req(XDR* xdrs, leave_req_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_ids = NULL;
    }

    return (xdr_u_int(xdrs, &(m->num_groups)) &&
            xdr_u_int(xdrs, &(m->client_id))  &&
            xdr_array(xdrs, (char **)&(m->group_ids), &(m->num_groups), max_groups,
                             (sizeof(string_t)),
                             (xdrproc_t )xdr_gname_string));
}


bool process_leave_resp(XDR* xdrs, leave_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_ids = NULL;
        m->cause = NULL;
    }
/*
    return (xdr_u_int(xdrs, &(m->num_groups)) &&
            xdr_array(xdrs, (char **)&(m->group_ids), &(m->num_groups), max_groups,
                             (sizeof(string_t)),
                             (xdrproc_t )xdr_gname_string) &&
            xdr_array(xdrs, (char **)&(m->cause), &(m->num_groups), max_groups,
                             (sizeof(string_t)),
                             (xdrproc_t )xdr_gname_string));*/
    int uint_res = xdr_u_int(xdrs, &(m->num_groups));
    int gid_res = xdr_array(xdrs, (char **)&(m->group_ids), &(m->num_groups), max_groups,
                                  (sizeof(string_t)),
                                  (xdrproc_t )xdr_gname_string);
    int c_res = xdr_array(xdrs, (char **)&(m->cause), &(m->num_groups), max_groups,
                                (sizeof(string_t)),
                                (xdrproc_t )xdr_gname_string);
    return uint_res && gid_res && c_res;
//  return uint_res && c_res;
}

bool xdr_l_saddr_in(XDR* xdrs, l_saddr_in_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
    }
    
/*  return (xdr_short(xdrs, &(m->sin_family)) &&
            xdr_u_short(xdrs, &(m->sin_port)) &&
            xdr_u_long(xdrs, &(m->s_addr)) &&
            xdr_string(xdrs, &(m->group_name), max_gname_len));*/
    int short_res = xdr_short(xdrs, &(m->sin_family));
    int ushort_res = xdr_u_short(xdrs, &(m->sin_port));
    int ulong_res = xdr_u_long(xdrs, &(m->s_addr));
    int string_res = xdr_string(xdrs, &(m->group_name), max_gname_len);
    int uint_res = xdr_u_int(xdrs, &(m->grp_port));
    return short_res && ushort_res && ulong_res && string_res && uint_res;
}

bool process_join_resp(XDR* xdrs, join_rsp_t* m){
/* return (xdr_u_int(xdrs, &(m->num_groups)) &&
           xdr_array(xdrs, (char**) &(m->group_ips), &(m->num_groups), max_groups,
                     sizeof(char),(xdrproc_t )xdr_int));*/
    if(xdrs->x_op == XDR_DECODE){
        m->group_ips = NULL;
    }
    
//  return (xdr_u_int(xdrs, &(m->num_groups)) &&
//          xdr_array(xdrs, (char**) &(m->group_ips),
//          &(m->num_groups), max_groups, sizeof(l_saddr_in_t),(xdrproc_t )xdr_l_saddr_in));
    int int_res = xdr_u_int(xdrs, &(m->num_groups));
    int arr_res = xdr_array(xdrs, (char**) &(m->group_ips),
            &(m->num_groups), max_groups, sizeof(l_saddr_in_t),(xdrproc_t )xdr_l_saddr_in);
    return int_res && arr_res;
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

bool process_comm_struct(XDR* xdrs, comm_struct_t* m){
    struct xdr_discrim comm_discrim[] = {
        {join_request,    (xdrproc_t)process_join_req},
        {join_response,   (xdrproc_t)process_join_resp},
//      {server_task, NULL},
//      {client_answer, NULL},
//      {client_leave, NULL},
        {echo_req,      (xdrproc_t)xdr_echo_req},
        {echo_response, (xdrproc_t)xdr_echo_resp},
        {leave_request, (xdrproc_t)process_leave_req},
        {leave_response, (xdrproc_t)process_leave_resp},
        { __dontcare__, NULL }
    };
    int enum_res = xdr_enum(xdrs, (enum_t *)(&(m->id)));
    int union_res =  xdr_union(xdrs,(enum_t *)(&(m->id)), (char *)&m->idv, comm_discrim, NULL);

    return enum_res && union_res;
}
