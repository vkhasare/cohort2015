#include "common.h"

void display_mapping(grname_ip_mapping_t * mapping, uint32_t count)
{
  uint32_t i;
  char buf[512];
  char remoteIP[INET_ADDRSTRLEN];

  for(i = 0;i < count; i++)
  {
    inet_ntop(AF_INET, &(mapping[i].grp_ip), remoteIP, INET_ADDRSTRLEN);
    sprintf(buf,"Group name: %s \t\tIP addr: %s", mapping[i].grname,remoteIP);
    PRINT(buf);
  }
}

int IS_SERVER(int oper)
{
  if (SERVER_MODE == oper)
     return 1;

  return 0;
}

int IS_CLIENT(int oper)
{
  if (CLIENT_MODE == oper)
     return 1;

  return 0;
}

int create_and_bind(char *machine_addr, char *machine_port, int oper_mode)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    int yes=1;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
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

        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (IS_SERVER(oper_mode))
        {
           if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
           {
              continue;
           }
        }
        else if (IS_CLIENT(oper_mode))
        {
           if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1)
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

//    DEBUG("Socket is created.");

    return sfd;
}


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


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*) sa)->sin6_addr);
    }
}

void display_server_clis()
{
  PRINT("show groups                        --  Displays list of groups");
  PRINT("show group info <group_name|all>   --  Displays group - client association");
  PRINT("enable msg group <group_name>      --  Enables display of messages for a specific group.");
  PRINT("no msg group <group_name>          --  Disables display of messages for a specific group.");
  PRINT("cls                                --  Clears the screen");
}


