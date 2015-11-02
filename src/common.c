#include "common.h"

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


void* get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
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
  PRINT("show groups                        --  Displays list of groups");
  PRINT("show group info <group_name|all>   --  Displays group - client association");
  PRINT("enable msg group <group_name>      --  Enables display of messages for a specific group.");
  PRINT("no msg group <group_name>          --  Disables display of messages for a specific group.");
  PRINT("send msg <group_name>              --  Sends a multicast message to the specified Group");
  PRINT("task <task_type> group <group_name>--  Assigns a specific task to the specified Group");
  PRINT("cls                                --  Clears the screen");
}

