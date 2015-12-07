#include "comm_primitives.h"
#include "print.h"
#define MAXMSGSIZE 100000

const unsigned int max_groups = 1000;
const unsigned int max_client_in_group = 255;
const unsigned int max_task_count = 100000;
const unsigned int max_data_size = 10000000;
const unsigned int max_gname_len = 25; //includes nul termination
const unsigned int echo_req_len = 256; //includes nul termination
const unsigned int echo_resp_len = 21; //includes nul termination

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

void populate_join_req(comm_struct_t* m, char** grnames, int num_groups, unsigned int capability){
    m->id=join_request;
    int iter;
    m->idv.join_req.num_groups = num_groups; 

    m->idv.join_req.capability = capability;
//    m->idv.join_req.group_ids = (string_t*) malloc (sizeof(string_t) * num_groups);
    m->idv.join_req.group_ids = MALLOC_STR_IE(num_groups);
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
    m->idv.leave_req.group_ids = MALLOC_STR_IE(num_groups);

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
    m->idv.leave_rsp.group_ids = MALLOC_STR_IE(num_groups);
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

    msglen = xdr_sizeof((xdrproc_t)process_comm_struct, (void *) m);

    /* msglen will ensure only pdu of req. length goes on wire, not whole MAXMSGSIZE btyes.*/
    xdrmem_create(&xdrs, msgbuf, (unsigned int)msglen, XDR_ENCODE);

    res = process_comm_struct(&xdrs, &(pdu->msg));

    if (!res) {
        PRINT("Error in sending msg.");
    }

    xdr_destroy(&xdrs);
    sendto(sockfd, msgbuf, msglen, MSG_DONTWAIT,
                     (struct sockaddr *) destAddr, sizeof(*destAddr));

}

/* <doc>
 * int read_record(int sockfd, pdu_t *pdu)
 * This function receives msg from wire, destined for client.
 * It encapsulates the msg into pdu and gives pdu to App.
 * </doc>
 */
int read_record(int sockfd, pdu_t *pdu){
    XDR xdrs;
    int res;
    pdu->msg.id = unknown_msg; /*initializing msg type here*/
 
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

bool process_moderator_update_req(XDR* xdrs, moderator_update_req_t* m){

    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
        m->client_ids = NULL;
    }

    int uint_res1 = (xdr_u_int(xdrs, &(m->moderator_id)));
    int uint_res2 = (xdr_u_int(xdrs, &(m->moderator_port)));
    int uint_res3   = (xdr_u_int(xdrs, &(m->client_id_count)));
    int str_res =  xdr_string(xdrs, &(m->group_name), max_gname_len);
    int uarray_res2 = xdr_array(xdrs, (char **)&(m->client_ids), &(m->client_id_count), max_client_in_group,
                               (sizeof(unsigned int)),
                               (xdrproc_t )xdr_u_int);
    return (uint_res1 && uint_res2 && uint_res3 && str_res && uarray_res2);
}

bool process_moderator_notify_req(XDR* xdrs, moderator_notify_req_t* m){

    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
    }

    int uint_res1 = (xdr_u_int(xdrs, &(m->moderator_id)));
    int uint_res2 = (xdr_u_int(xdrs, &(m->moderator_port)));
    int uint_res3 = (xdr_int(xdrs, &(m->num_of_clients_in_grp)));
    int str_res =  xdr_string(xdrs, &(m->group_name), max_gname_len);

    return (uint_res1 && uint_res2 && uint_res3 && str_res);
}

bool process_moderator_notify_rsp(XDR* xdrs, moderator_notify_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
        m->client_ids = NULL;
    }

    int str_res     = xdr_string(xdrs, &(m->group_name), max_gname_len);
    int uint_res1   = (xdr_u_int(xdrs, &(m->moderator_id)));
    int uint_res2   = (xdr_u_int(xdrs, &(m->client_id_count)));
    int uarray_res2 = xdr_array(xdrs, (char **)&(m->client_ids), &(m->client_id_count), max_client_in_group,
                             (sizeof(unsigned int)),
                             (xdrproc_t )xdr_u_int);

    return (uint_res1 && uint_res2 && uarray_res2 && str_res);
}

bool xdr_echo_req(XDR* xdrs, echo_req_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
        m->client_ids = NULL;
    }

    int str_res    = xdr_string(xdrs, &(m->group_name), max_gname_len);
    int uint_res   = xdr_u_int(xdrs, &(m->num_clients));
    int uarray_res = xdr_array(xdrs, (char **)&(m->client_ids), &(m->num_clients), max_client_in_group,
                              (sizeof(unsigned int)),
                              (xdrproc_t )xdr_u_int);
    return str_res && uint_res && uarray_res;
}

bool xdr_echo_resp(XDR* xdrs, echo_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->status = 0;
        m->group_name = NULL;
    }

    int uint_res = xdr_u_int(xdrs, &(m->status));
    int str_res =  xdr_string(xdrs, &(m->group_name), max_gname_len);

    return uint_res && str_res;
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
                             (xdrproc_t )xdr_gname_string) &&
            xdr_u_int(xdrs, &(m->capability)));
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
    if(xdrs->x_op == XDR_DECODE){
        m->group_ips = NULL;
    }
    
/*  return (xdr_u_int(xdrs, &(m->num_groups)) &&
            xdr_array(xdrs, (char**) &(m->group_ips),
            &(m->num_groups), max_groups, sizeof(l_saddr_in_t),(xdrproc_t)xdr_l_saddr_in));*/
    int int_res = xdr_u_int(xdrs, &(m->num_groups));
    int arr_res = xdr_array(xdrs, (char**) &(m->group_ips),
            &(m->num_groups), max_groups, sizeof(l_saddr_in_t),(xdrproc_t )xdr_l_saddr_in);
    return int_res && arr_res;
}

bool process_perform_task_req(XDR* xdrs, perform_task_req_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
        m->client_ids = NULL;
        m->task_set = NULL;
    }

    return (xdr_u_int(xdrs, &(m->client_id_count)) &&
            xdr_u_int(xdrs, &(m->task_id)) &&
            xdr_string(xdrs, &(m->group_name), max_gname_len) &&
            xdr_array(xdrs, (char**)&(m->client_ids), &(m->client_id_count), max_client_in_group,
                             (sizeof(unsigned int)),
                             (xdrproc_t )xdr_u_int) &&
            xdr_u_int(xdrs, &(m->task_count)) &&
            xdr_array(xdrs, (char**)&(m->task_set), &(m->task_count), max_task_count,
                             (sizeof(long)),
                             (xdrproc_t )xdr_long) &&
            xdr_int(xdrs, &(m->task_type)));
}

bool xdr_result_t(XDR* xdrs, result_t* m){
    if(xdrs->x_op == XDR_DECODE){
       m->value = NULL;
    }
    
/*  return (xdr_short(xdrs, &(m->sin_family)) &&
            xdr_u_short(xdrs, &(m->sin_port)) &&
            xdr_u_long(xdrs, &(m->s_addr)) &&
            xdr_string(xdrs, &(m->group_name), max_gname_len));*/
    bool uint_res = xdr_u_int(xdrs, &(m->size));
    bool arr_res = xdr_array(xdrs, (char**) &(m->value),
            &(m->size), max_data_size, sizeof(int),(xdrproc_t )xdr_int);
 
    return uint_res && arr_res;
}
bool process_task_resp(XDR* xdrs, task_rsp_t* m){
    if(xdrs->x_op == XDR_DECODE){
        m->group_name = NULL;
        m->client_ids = NULL;
        m->result = NULL;
    }

    bool string_res = xdr_string(xdrs, &(m->group_name), max_gname_len);
    bool task_id_res = xdr_u_int(xdrs, &(m->task_id));
    bool typ_res = xdr_enum(xdrs, (enum_t *)(&(m->type)));
    bool num_client_res = xdr_u_int(xdrs, &(m->num_clients));
    bool client_id_res = xdr_array(xdrs, (char**)&(m->client_ids), &(m->num_clients), max_client_in_group,
                             (sizeof(unsigned int)),(xdrproc_t )xdr_u_int);
    bool arr_res = xdr_array(xdrs, (char**) &(m->result),
            &(m->num_clients), max_client_in_group, sizeof(result_t),(xdrproc_t )xdr_result_t);

    return string_res && task_id_res && typ_res  && num_client_res && client_id_res && arr_res;
}

bool process_comm_struct(XDR* xdrs, void* msg, ...){
    comm_struct_t* m = (comm_struct_t*) msg;
    struct xdr_discrim comm_discrim[] = {
        {join_request,          (xdrproc_t)process_join_req},
        {join_response,         (xdrproc_t)process_join_resp},
        {echo_req,              (xdrproc_t)xdr_echo_req},
        {echo_response,         (xdrproc_t)xdr_echo_resp},
        {leave_request,         (xdrproc_t)process_leave_req},
        {leave_response,        (xdrproc_t)process_leave_resp},
        {moderator_notify_req,  (xdrproc_t)process_moderator_notify_req},
        {moderator_notify_rsp,  (xdrproc_t)process_moderator_notify_rsp},
        {moderator_update_req,  (xdrproc_t)process_moderator_update_req},
        {perform_task_req,      (xdrproc_t)process_perform_task_req},
        {task_response,         (xdrproc_t)process_task_resp},
        { __dontcare__,         NULL }
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
    unsigned int * num_clients = &(m->idv.task_rsp.num_clients);
    result_t * temp =&(m->idv.task_rsp.result[*num_clients]);
    temp->size=resp->size;
    temp->value=malloc(sizeof(int)*resp->size);
    for(iter = 0; iter < resp->size; iter++){
       temp->value[iter]=resp->value[iter];
    }
    m->idv.task_rsp.client_ids[*num_clients] = client_id; 
    (*num_clients)++;
}

void populate_task_rsp(comm_struct_t* m, unsigned int task_id,char* group_name, rsp_type_t r_type, result_t *resp, unsigned int client_id ){
    memset(&(m->idv),0,sizeof(task_rsp_t));
    m->id=task_response;
    m->idv.task_rsp.type = r_type;
    m->idv.task_rsp.task_id = task_id;
    m->idv.task_rsp.group_name = malloc(sizeof(char)*strlen(group_name));
    strcpy(m->idv.task_rsp.group_name, group_name); 
    m->idv.task_rsp.task_id = task_id;
    m->idv.task_rsp.result = (result_t *)malloc(sizeof(result_t)); 
    m->idv.task_rsp.client_ids = (unsigned int *)malloc(sizeof(unsigned int *)); 
    update_task_rsp(m,r_type,resp, client_id);
}

void * populate_moderator_task_rsp( uint8_t num_clients, task_rsp_t *resp, unsigned int client_id ){
    pdu_t *rsp_pdu= malloc(sizeof(pdu_t)); 
    comm_struct_t* m = &(rsp_pdu->msg);
    memset(&(m->idv),0,sizeof(task_rsp_t));
    m->id=task_response;
    task_rsp_t * task_rsp = &(m->idv.task_rsp);
    task_rsp->type = resp->type;
    task_rsp->task_id = resp->task_id;
    task_rsp->group_name = malloc(sizeof(char)*strlen(resp->group_name));
    strcpy(task_rsp->group_name, resp->group_name); 
    task_rsp->result = (result_t *)(l_saddr_in_t *) calloc (num_clients, sizeof(result_t));
    task_rsp->client_ids = (unsigned int *)calloc(num_clients, sizeof(unsigned int *));
    update_task_rsp(m, task_rsp->type, resp->result, client_id);
    return rsp_pdu;
}

