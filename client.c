#include "common.h"
#include "SLL/client_ll.h"
#define TIMEOUT_SECS 5

#define lo "127.0.0.1"
int cfd;
struct keep_alive{
unsigned count;
char group_name[10][10];
};

struct keep_alive active_group;

void sendPeriodicMsg(int signal)
{
    int numbytes;
    char msg[] ="I am Alive";
    char send_msg[512];
    int i=active_group.count;
    while(i>0){
    i--;
    sprintf(send_msg, "%s:%s\r\n",active_group.group_name[i],msg);
     
    PRINT("Sending periodic Request.");
    if ((numbytes = send(cfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending msg.");
    }
    }
    alarm(TIMEOUT_SECS);
}
bool handle_join_response(client_information_t** client_info, char *grp_name, char *grp_ip_address)
{
   char display[30];
   struct sockaddr_in group_ip;

   inet_pton(AF_INET, grp_ip_address, &(group_ip));

   int fd_id= multicast_join(lo,group_ip); 
   //sprintf(display,"Listening to group %s ip address %s\n", grp_name, grp_ip_address);
   sprintf(display,"Listening to group %s\n", grp_name);
   PRINT(display);
   if(fd_id > 0){ 
   ADD_CLIENT_IN_LL(client_info,grp_name,group_ip,fd_id); 
   }
}
void sendPeriodicMsg_XDR(int signal)
{
    my_struct_t m;
    XDR xdrs;
    FILE* fp = fdopen(cfd, "wb");
    
    populate_my_struct(&m);
    
    xdrs.x_op = XDR_ENCODE;
    xdrrec_create(&xdrs,0,0,fp,rdata,wdata);

    PRINT("Sending XDR local struct.");
    if (!process_my_struct(&m, &xdrs))
    {
        PRINT("Error in sending msg.");
    }
    xdr_destroy(&xdrs);
    fflush(fp);

    alarm(TIMEOUT_SECS);
}

int is_gname_already_present(char *grp_name){
   if(active_group.count == 0) return 0;  
   int i=0 , count=active_group.count;
   while(i<count){
     if(strcmp(grp_name,active_group.group_name[i])==0) return i+1;
     i++;
   }
   return 0;
}
void insert_gname(char *gname){
   strcpy(active_group.group_name[active_group.count], gname);
   active_group.count++;
}

/* Function to start periodic timer for sending messages to server*/
void startKeepAlive(char * gname)
{
    if(is_gname_already_present(gname)==0){
      insert_gname(gname);
    
    if(active_group.count==1){
    struct sigaction myaction;
    myaction.sa_handler = sendPeriodicMsg;
    sigfillset(&myaction.sa_mask);
    myaction.sa_flags = 0;

    if (sigaction(SIGALRM, &myaction, 0) < 0)
    {
        perror("\nError in sigaction.");
        exit(0);
    }
    
    alarm(TIMEOUT_SECS);
    }
    }
}



/* Function to stop periodic timer */
void stopKeepAlive()
{
    int i=0;

    while(i < active_group.count){
      strcpy(active_group.group_name[i] ,"IV");
      i++;
    }

    active_group.count =0;
    alarm(0);
}

void display_client_clis()
{
   PRINT("show client groups                           --  displays list of groups joined by client");
   PRINT("enable keepalive group <group_name>          --  Sends periodic messages to Server");
   PRINT("disable keepalive                            --  Stops periodic messages to Server");
   PRINT("join group <name>                            --  Joins a new group");
   PRINT("cls                                          --  Clears the screen");
}

void display_client_groups(client_information_t **client_info)
{
  display_mcast_client_node(client_info);
}


void decode_join_response(char *buf,  client_information_t **client_info)
{
    char *grp_ip, *grp_name;
//char *ptr=buf_copy;
    char *token;

    char *ptr = strtok_r(buf,":",&token);
//    char *ptr11 = buf;
//      char *buf_copy,*token1;
//      strcpy(buf_copy,ptr11);
      
      //PRINT(ptr);
//      msglen -= (strlen(ptr11)+1);
      char *ptr1 = strtok_r(NULL,",",&token);
      grp_name = ptr1;
      grp_ip=token;
      //PRINT(grp_name);
      //PRINT(grp_ip);

      handle_join_response(client_info, grp_name, grp_ip);


}

/* Function for handling received data on Client Socket */

void client_socket_data(client_information_t **client_info, int fd)
{
    char buf[512];
    int read_count;
    char length[10];
    char buf_copy[100];
    read_count = read(fd, buf, sizeof(buf));
    int len = 0;
    while(len < read_count){
       strcpy(buf_copy, buf+len);
       int slen = strlen(buf_copy);
       //PRINT(buf_copy};
       if(strncmp(buf_copy,"JOIN RESPONSE:",14 )==0){
          //PRINT(buf_copy);
        sprintf(length,"%d\n", read_count);
          //PRINT(length);
          decode_join_response(buf_copy,  client_info);
       }
       else
       {
          PRINT(buf);
       }
       slen++;
       len +=slen;
    }
}

/* Function for handling input from STDIN */

void client_stdin_data(client_information_t **client_info, int fd)
{
    char read_buffer[100];
    //char read_buffer_copy[100];
    int cnt=0;

    cnt=read(fd, read_buffer, 99);
    read_buffer[cnt-1] = '\0';

    if (0 == strncmp(read_buffer,"show help",9))
    {
       display_client_clis();
    }
    else if (strncmp(read_buffer,"show client groups",22) == 0)
    {
       display_client_groups(client_info);
    }
    else if (strncmp(read_buffer,"enable keepalive group ",23) == 0)
    {
       startKeepAlive(read_buffer+23);
    }
    else if (strncmp(read_buffer,"disable keepalive",18) == 0)
    {
       stopKeepAlive();
    }
    else if (strncmp(read_buffer,"join group ",11) == 0)
    {
    
       if (IS_GROUP_IN_CLIENT_LL(client_info,read_buffer+11))
       {
          char buf[100];
          sprintf(buf,"Error: Client is already member of group %s.",read_buffer+11);
          PRINT(buf);
       }
       else
       {
          join_msg(cfd,read_buffer+11);
       }
    }
    else if (0 == strcmp(read_buffer,"cls\0"))
    {
       system("clear");
    }
    else
    {
      if (cnt != 1 && read_buffer[0] != '\n')
        PRINT("Error: Unrecognized Command.\n");
    }
}

int main(int argc, char * argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char group_name[50];
    struct addrinfo hints, *servinfo, *p;
    int count = 0;

    active_group.count ==0;
    client_information_t *client_info = NULL;
 
    int event_count, index; 
    int /*cfd,*/ status;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;    
   
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char remoteIP[INET6_ADDRSTRLEN];
    char *addr,*port;

    allocate_client_info(&client_info);

    if (argc != 4)
    {
      printf("Usage: %s <client_IP> <client_port> <group_name>\n", argv[0]);
//      exit(1);
//      Temporary code
      argv[1] = "127.0.0.1";
      argv[2] = "3490";
      argv[3] = "G1";
    }

    addr = argv[1];
    port = argv[2];
    strcpy(group_name,argv[3]);

    cfd = create_and_bind(addr, port, CLIENT_MODE);

    //ADD_CLIENT_IN_LL(&client_info,group_name);

    if (cfd == -1)
    {
        perror("\nError while creating and binding the socket.");
        exit(0);
    }

    status = make_socket_non_blocking(cfd);

    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    PRINT("..WELCOME TO CLIENT..");
    PRINT("\r   <Use \"show help\" to see all supported clis.>\n");

    PRINT_PROMPT("[client] ");

    efd = epoll_create(MAXEVENTS);

    if (efd == -1)
    {
        perror("\nError while creating the epoll.");
        exit(0);
    }

    event.data.fd = cfd;
    event.events = EPOLLIN|EPOLLET;

    status = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event);

    if ( status == -1)
    {
        perror("\nError while adding FD to epoll event.");
        exit(0);
    }

    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN|EPOLLET;

    status = epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &event);

    if ( status == -1)
    {
        perror("\nError while adding STDIN FD to epoll event.");
        exit(0);
    }

    events = calloc(MAXEVENTS, sizeof(event));
   
    char * gname=strtok(group_name,",");
    while(gname!=NULL){
      join_msg(cfd,gname); 
      gname=strtok(NULL,",");
      //PRINT("Sending join message");
    }
    while (1) {
        event_count = epoll_wait(efd, events, MAXEVENTS, -1);

        for (index = 0; index < event_count; index++) {
            /* Code Block for receiving data on Client Socket */
            if ((cfd == events[index].data.fd) && (events[index].events & EPOLLIN))
            {
                client_socket_data(&client_info, events[index].data.fd);
            }
            /* Code Block for handling input from STDIN */
            else if (STDIN_FILENO == events[index].data.fd)
            {
                client_stdin_data(&client_info, events[index].data.fd);

                PRINT_PROMPT("[client] ");
            }
        }
    }
    
    close(sockfd);
    
    return 0;
}
