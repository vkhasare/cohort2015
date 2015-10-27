
#define TEST_CLIENT_C
#include "../src/client.c"
#include <assert.h>

#define MATCH_STRING(str1,str2) strcmp(str1,str2)==0
static void verify_join_request(comm_struct_t *,comm_struct_t*);
static void fill_join_request_for_a_group(comm_struct_t *);
static void fill_join_request_for_two_groups(comm_struct_t *);
static void test_join_with_a_group(void);
static void test_join_with_two_groups(void);

int main(void){

    test_join_with_a_group();
   // test_join_with_two_groups();
    PRINT("done");
    return 0;
}
static void test_join_with_a_group(void){
     int sockets[2], child;
     struct sockaddr_in destAddr;
     pdu_t pdu0, pdu1;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        perror("opening stream socket pair");
        exit(1);
    }
   
   destAddr.sin_family = AF_INET;
   destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   destAddr.sin_port = htons(4442);
//   addrlen = sizeof(addr);
 
    comm_struct_t *m,*n,ex_result;
    fill_join_request_for_a_group(&ex_result);
    
    if ((child = fork()) == -1)
        perror("fork");
    else if (child) {   /* This is the client. */
        close(sockets[0]);
        m= &(pdu0.msg);
        char * gr_list[]={"G1"};
        populate_join_req(m, gr_list, 1);
        verify_join_request(m,&ex_result);
        //PRINT("write");
        write_record(sockets[1], &destAddr, &pdu0);
        close(sockets[1]);
    } else {        /* This is the server. */
        close(sockets[1]);
        //PRINT("read");
        PRINT("Read output %d", read_record(sockets[0], &pdu1));
        n=&(pdu1.msg);
        verify_join_request(n,&ex_result);
        close(sockets[0]);
    }

}
static void test_join_with_two_groups(void){
     int sockets[2], child;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        perror("opening stream socket pair");
        exit(1);
    }
   
    comm_struct_t m,n,ex_result;
    fill_join_request_for_two_groups(&ex_result);
    
    if ((child = fork()) == -1)
        perror("fork");
    else if (child) {   /* This is the client. */
        close(sockets[0]);
        m.id=join_request;
        char * gr_list[]={"G1","G2"};
        populate_join_req(&m, gr_list, 2);
        verify_join_request(&m,&ex_result);
        //PRINT("write");
        //write_record(sockets[1], &m);
        close(sockets[1]);
    } else {        /* This is the server. */
        close(sockets[1]);
        //PRINT("read");
        //read_record(sockets[0], &n);
        //verify_join_request(&n,&ex_result);
        close(sockets[0]);
    }

}
static void verify_join_request(comm_struct_t *m,comm_struct_t *n){
    PRINT("Join id %d and %d", m->id, n->id);
    assert(m->id == n->id);
    assert(m->idv.join_req.num_groups == n->idv.join_req.num_groups);
    size_t iter, num_groups = (size_t)(m->idv.join_req.num_groups);
    for( iter = 0; iter < num_groups; iter++) 
      assert(MATCH_STRING(m->idv.join_req.group_ids[iter].str , n->idv.join_req.group_ids[iter].str));
}

static void fill_join_request_for_a_group(comm_struct_t *ex_result){
    ex_result->id = 32; 
    ex_result->idv.join_req.num_groups = 1;
    ex_result->idv.join_req.group_ids = (string_t*) malloc (sizeof(string_t) * 1);
    ex_result->idv.join_req.group_ids[0].str = "G1";
}
static void fill_join_request_for_two_groups(comm_struct_t *ex_result){
    ex_result->id = 32; 
    ex_result->idv.join_req.num_groups = 2;
    ex_result->idv.join_req.group_ids = (string_t*) malloc (sizeof(string_t) * 2);
    ex_result->idv.join_req.group_ids[0].str = "G1";
    ex_result->idv.join_req.group_ids[1].str = "G2";
}
