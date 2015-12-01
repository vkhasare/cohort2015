#include "common.h"

/*Flag for debug mode*/
bool debug_mode = FALSE;

char* enum_to_str(msg_cause cause)
{
   switch(cause)
   {
      case ACCEPTED:
                    return "ACCEPTED";
      case REJECTED:
                    return "REJECTED";
      default:
                    return "UNKNOWN";
   }
}

msg_cause str_to_enum(char *str)
{
    if (!strcmp(str,"ACCEPTED"))
        return ACCEPTED;
    else if (!strcmp(str,"REJECTED"))
        return REJECTED;
    else
        return UNKNOWN;

}

/* <doc>
 * char* get_my_ip(const char * device)
 * Gives IP for a particular interface
 *
 * </doc>
 */
void get_my_ip(const char * device, struct sockaddr *addr)
{
    int fd;
    struct ifreq ifr;

    fd = socket (AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET; 
    strncpy (ifr.ifr_name, device, IFNAMSIZ-1);

    ioctl (fd, SIOCGIFADDR, &ifr);

    close (fd);

    (*addr).sa_family = ifr.ifr_addr.sa_family;
    memcpy((*addr).sa_data, ifr.ifr_addr.sa_data,sizeof((*addr).sa_data));
}


/* <doc>
 * int IS_SERVER(int oper)
 * Checks if operational mode type
 * is Server
 * </doc>
 */
int IS_SERVER(int oper)
{
  if (SERVER_MODE == oper)
     return 1;

  return 0;
}

/* <doc>
 * int IS_CLIENT(int oper)
 * Checks if operational mode type
 * is Client
 * </doc>
 */
int IS_CLIENT(int oper)
{
  if (CLIENT_MODE == oper)
     return 1;

  return 0;
}

/* <doc>
 * int create_and_bind(char *machine_addr, char *machine_port, int oper_mode)
 * Create the UDP socket and binds it to specified address and port
 *
 * </doc>
 */
int create_and_bind(char *machine_addr, char *machine_port, int oper_mode)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    int yes=1;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(machine_addr, machine_port, &hints, &result) != 0)
    {
        PRINT("Failure in getaddrinfo");
        return -1;
    }

    for (rp = result; rp != NULL; rp=rp->ai_next)
    {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
        {
            continue;
        }

        /*Reuse the socket*/
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        /*Keeping separate bind blocks for server and client for future changes*/
        if (IS_SERVER(oper_mode))
        {
           if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
           {
              continue;
           }
        }
        else if (IS_CLIENT(oper_mode))
        {
           if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
           {
              continue;
           }
        }
        break;
    }

    if (!rp)
    {
      perror("\nFailure in binding socket.");
      exit(1);
    }

    freeaddrinfo(result);

    return sfd;
}

/* <doc>
 * int make_socket_non_blocking (int sfd)
 * This function accepts fd as input and
 * makes it non-blocking using fcntl
 * system call.
 *
 * </doc>
 */
int make_socket_non_blocking (int sfd)
{
    int flags;

    flags = fcntl(sfd, F_GETFL,0);

    if (flags == -1) {
        perror("Error while fcntl F_GETFL");
        return -1;
    }

    fcntl(sfd, F_SETOWN, getpid());
    if (fcntl(sfd, F_SETFL,flags|O_NONBLOCK) == -1) {
        perror("Error while fcntl F_SETFL");
        return -1;
    }

//  DEBUG("Socket is non-blocking now.");

  return 1;
}


char* get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (char *) &(((struct sockaddr_in*) sa)->sin_addr);
    } else {
        return (char *) &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
}

void initialize_echo_request(echo_req_t *echo_req)
{
    echo_req->group_name  = NULL;
    echo_req->num_clients = 0;
    echo_req->client_ids  = NULL;
}

/* <doc>
 * int send_echo_request(unsigned int sockfd, struct sockaddr *addr, char *grp_name)
 * Sends echo request on mentioned sockfd to the addr passed.
 * grp_name is filled in echo req pdu.
 *
 * </doc>
 */

int send_echo_request(const int sockfd, struct sockaddr *addr, char *grp_name)
{
    pdu_t pdu;
    char ipaddr[INET6_ADDRSTRLEN];
    
    comm_struct_t *req = &(pdu.msg);
    req->id = echo_req;
    
    echo_req_t *echo_request = &(req->idv.echo_req);
    initialize_echo_request(echo_request);

    /*Create the echo request pdu*/
    echo_request->group_name = grp_name;
    write_record(sockfd, addr, &pdu);

    inet_ntop(AF_INET, get_in_addr(addr), ipaddr, INET6_ADDRSTRLEN);
    PRINT("[Echo_Request: GRP - %s] Echo Request sent to %s", echo_request->group_name, ipaddr);

    return 0;
}


/* <doc>
 * inline int calc_key(struct sockaddr *addr)
 * calc_key returns the key based on IP address.
 *
 * </doc>
 */
inline unsigned int
calc_key(struct sockaddr *addr)
{
  if (addr && addr->sa_family == AF_INET)
    return (((struct sockaddr_in*) addr)->sin_addr.s_addr);

  return 0;
}

void display_server_clis()
{
  PRINT("show groups                                          --  Displays list of groups");
  PRINT("show group info <group_name|all>                     --  Displays group - client association");
  PRINT("task <task_type> group <group_name> file <filename>  --  Assigns a specific task to the specified Group. filename is optional. Default Value: task_set/prime_set1.txt");
  PRINT("server backup <backup_server_ip>                     --  Configures IP of secondary server");
  PRINT("switch                                               --  Switches to secondary server");
  PRINT("enable debug                                         --  Enables the debug mode");
  PRINT("disable debug                                        --  Disables the debug mode");
  PRINT("cls                                                  --  Clears the screen");
}

void start_oneshot_timer(timer_t *timer_id, uint8_t interval, uint32_t sigval)
{
    struct itimerspec its;
    struct sigevent sev;
    
    its.it_value.tv_sec = interval;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = sigval;
    sev.sigev_value.sival_ptr = timer_id;
    
    if (timer_create(CLOCKID, &sev, timer_id) == -1)
        errExit("timer_create");

    if (timer_settime(*timer_id, 0, &its, NULL) == -1)
         errExit("timer_settime");
}

void start_recurring_timer(timer_t *timer_id, uint8_t interval, uint32_t sigval)
{
    struct itimerspec its;
    struct sigevent sev;
    
    //For now, using simplified timer structure. First timeout and subsequent
    //timeouts are at the granularity of seconds and are same. This can be
    //modified to operate at nanosecond granularity though.
    uint8_t r_interval = interval;
    
    //its.it_value.tv_sec = interval / 1000000000;
    //its.it_value.tv_nsec = interval % 1000000000;
    
    its.it_value.tv_sec = interval;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = sigval;
    sev.sigev_value.sival_ptr = timer_id;
    
    if (timer_create(CLOCKID, &sev, timer_id) == -1)
        errExit("timer_create");

    if (timer_settime(*timer_id, 0, &its, NULL) == -1)
         errExit("timer_settime");
}

/* <doc>
 * unsigned int generate_random_capability(void)
 * This function returns a random capability between 1-3 for a client
 * </doc>
 */
unsigned int generate_random_capability(void)
{
  unsigned int number = 1;
  
  srand(time(NULL));
  number = (rand() % 3) + 1;

  return number;
}

