#include "comm_primitives.h"
#include "print.h"
#define MAXMSGSIZE 5000
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
    PRINT("\na=%d\nb=%f\nc=%u\nd=%lf\nc[0]=%d", 
            m->a,m->b,m->c_count,m->d,m->c[0]);
    return;
}

void print_echo_msg(char* echo_str){
    PRINT(echo_str);
    return;
}

void print_join_req(join_req_t* m){
    int iter;
    PRINT("\nNumber of groups=%d", 
            m->num_groups);
    for(iter = 0; iter < m->num_groups; iter++){
//      sprintf(buf, "\n(%d): %s", iter, m->group_ids[get_index(iter)]);
        PRINT("\n(%d): %s", iter, m->group_ids[iter].str);
    }
    return;
}

void print_join_rsp(join_rsp_t* m){
    int iter;
    PRINT("\nNumber of groups=%d", 
            m->num_groups);
    for(iter = 0; iter < m->num_groups; iter++){
        PRINT("\n(%d): %d", iter, m->group_ips[iter]);
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
    m->id=join_request;
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

void populate_leave_rsp(comm_struct_t* m, char** grnames, unsigned int cause, int num_groups){
    int iter;
    m->idv.leave_rsp.num_groups = num_groups;
    m->idv.leave_rsp.group_ids = MALLOC_IE(num_groups);
    m->idv.leave_rsp.cause = cause;

    for(iter = 0; iter < num_groups; iter++){
#define MALLOC_STRING(x)    \
m->idv.leave_rsp.group_ids[(x)].str = MALLOC_STR;

        MALLOC_STRING(iter);

#undef MALLOC_STRING

#define SUBS_TXT(x) (m->idv.leave_rsp.group_ids[(x)].str)


        strcpy(SUBS_TXT(iter),grnames[iter]);
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
//           print_echo_msg(m->idv.echo_req.str);
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
//        op_res ? print_structs(m): NULL;
    }
    return ret_res;
}

bool postprocess_my_struct_stream(my_struct_t* m, XDR* xdrs, bool op_res){
    char buf[512];
    bool ret_res = false;
    
    if (xdrs->x_op == XDR_ENCODE){
        ret_res = xdrrec_endofrecord(xdrs, TRUE);
        PRINT("[I] Encode res: %d, Push res: %d, ", op_res ,ret_res);
    }
    
    if (xdrs->x_op == XDR_DECODE){
        ret_res = xdrrec_skiprecord (xdrs);
        PRINT("[I] Decode res: %d, Skip rec res: %d, ", op_res, ret_res);
        op_res ? print_my_struct(m): NULL;
    }
    return ret_res;
}

/* <doc>
 * void write_record(struct sockaddr_in *destAddr, int sockfd, pdu_t *pdu)
 * This function receives pdu from app, destined for a remote address.
 * It decapsulates the pdu and sends the actual msg on wire.
 * </doc>
 */
void write_record(int sockfd, struct sockaddr_in *destAddr, pdu_t *pdu){
    XDR           xdrs ;
    int           res;
    char          msgbuf[MAXMSGSIZE];
    comm_struct_t *m;
    ssize_t msglen;
 
    xdrs.x_op = XDR_ENCODE;

    /*pdu received from App. Decapsulating it.*/
    m = &(pdu->msg);

    msglen = xdr_sizeof(process_comm_struct,m);

    //memset(msgbuf, 0, sizeof(msgbuf));

    /* msglen will ensure only pdu of req. length goes on wire, not whole MAXMSGSIZE btyes.*/
    xdrmem_create(&xdrs, msgbuf, (unsigned int)msglen, XDR_ENCODE);

    res = process_comm_struct(&xdrs, &(pdu->msg));

    if (!res) {
        PRINT("Error in sending msg.");
    }

    /*
    //Only for debugging
    int size;
    size = xdr_getpos(&xdrs);
    PRINT("Size of encoded data: %i\n", size);
    xdr_destroy(&xdrs); 
    */
    sendto(sockfd, msgbuf, msglen, MSG_DONTWAIT,
                     (struct sockaddr *) destAddr, sizeof(*destAddr));

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

/* <doc>
 * int read_record(int sockfd, pdu_t *pdu)
 * This function receives msg from wire, destined for client.
 * It encapsulates the msg into pdu and gives pdu to App.
 * </doc>
 */
int read_record(int sockfd, pdu_t *pdu){
    XDR xdrs ;
    int res;
    
    xdrs.x_op = XDR_DECODE;

    char      msgbuf[MAXMSGSIZE];
    ssize_t     msglen;
    struct sockaddr_in  sin;
    socklen_t   alen = (socklen_t)sizeof(sin);

    //memset(msgbuf, 0, sizeof(msgbuf));
    msglen = recvfrom(sockfd, msgbuf, sizeof(msgbuf), 0,
                     (struct sockaddr *)&sin, &alen);

    char s[INET6_ADDRSTRLEN];

    inet_ntop(sin.sin_family, &(((struct sockaddr_in *) &sin)->sin_addr) , s, sizeof(s));
    //PRINT("listener: got packet from %s\n", s);

    xdrmem_create(&xdrs, msgbuf, (unsigned int)msglen, XDR_DECODE);
    
    res = process_comm_struct(&xdrs,&(pdu->msg));

    /*encapsulation for msg. pdu will be passed to App.*/
    pdu->peer_addr.sa_family = ((struct sockaddr *)&sin)->sa_family;
    strcpy(pdu->peer_addr.sa_data, ((struct sockaddr *)&sin)->sa_data);

    xdr_destroy(&xdrs);
    return pdu->msg.id;
}

bool xdr_echo_req(XDR* xdrs, echo_req_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
    }

    return (xdr_string(xdrs, &(m->group_name), max_gname_len));
}

bool xdr_echo_resp(XDR* xdrs, echo_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->status = 0;
    }

    return ((xdr_u_int(xdrs, &(m->status))));
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
            xdr_array(xdrs, (char **)&(m->group_ids), &(m->num_groups), max_groups,
                             (sizeof(string_t)),
                             (xdrproc_t )xdr_gname_string));
}


bool process_leave_resp(XDR* xdrs, leave_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_ids = NULL;
        m->cause = 0;
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
    int uint_cause = xdr_u_int(xdrs, &(m->cause));
    return uint_res && gid_res && uint_cause;
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
    int uint_cause = xdr_u_int(xdrs, &(m->cause));
    int uint_res = xdr_u_int(xdrs, &(m->grp_port));
    return short_res && ushort_res && ulong_res && string_res && uint_cause && uint_res;
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
        {TASK_RESPONSE, (xdrproc_t)process_leave_resp},
        { __dontcare__, NULL }
    };
    int enum_res = xdr_enum(xdrs, (enum_t *)(&(m->id)));
    int union_res =  xdr_union(xdrs,(enum_t *)(&(m->id)), (char *)&m->idv, comm_discrim, NULL);

    return enum_res && union_res;
}
inline unsigned int get_size(rsp_type_t type){
/*
  switch(type){
    case TYPE_CHAR: return sizeof(char);
    case TYPE_INT: return sizeof(int);
    case TYPE_LONG: return sizeof(long);
}*/ 
  return sizeof(int);
}
void update_task_rsp(comm_struct_t* m, rsp_type_t r_type, result_t *resp, unsigned int client_id ){
    int iter;
    result_t * temp = malloc(sizeof(result_t));
    temp->size=resp->size;
    temp->value=malloc(get_size(r_type)*resp->size);
    for(iter = 0; iter < resp->size; iter++){
       temp->value[iter]=resp->value[iter];
    }
    unsigned int * num_clients = &(m->idv.task_rsp.num_clients);
    m->idv.task_rsp.result[*num_clients] = temp;
    m->idv.task_rsp.client_ids[*num_clients] = client_id; 
    *num_clients++;
}
void populate_task_rsp(comm_struct_t* m, rsp_type_t r_type, result_t *resp, unsigned int client_id ){
    memset(&(m->idv),0,sizeof(task_rsp_t));
    m->id=TASK_RESPONSE;
    m->idv.task_rsp.type = r_type;
    m->idv.task_rsp.result = (result_t **)malloc(sizeof(result_t*)); 
    m->idv.task_rsp.client_ids = (unsigned int *)malloc(sizeof(unsigned int *)); 
    update_task_rsp(m,r_type,resp, client_id);
}



