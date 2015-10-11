/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"

#define MATCH_NAME(name1, name2)  (strncmp(name1, name2,2)==0)

struct sockaddr_in localSock;
struct ip_mreq group;
int datalen;
char databuf[1024];

/*
bool send_join_response(int infd, grname_ip_mapping_t * map);

bool join_group(int infd, char * group_name, grname_ip_mapping_t * mapping, int num_groups){
   int index=0;
   for(index=0;index<num_groups;index++){
     if(MATCH_NAME(group_name, mapping[index].grname))
     {
        if(send_join_response(infd,(mapping+index) ))
        {
          return 1;
        }
        break;
     }
    }
 return 0;
}

bool send_join_response(int infd, grname_ip_mapping_t* map){
    int numbytes;
    char display_string[100];
    char send_msg[512];
    char remoteIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(map->grp_ip), remoteIP, INET_ADDRSTRLEN);

    sprintf(send_msg,"JOIN RESPONSE:%s,%s", map->grname, remoteIP);
    if ((numbytes = send(infd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending join rsp.");
        return false;
    }
    return true; 
}



bool join_msg(int cfd, char * group_name)
{
    int numbytes;
    char display_string[100];
    char msg[] ="JOIN";
    char send_msg[512];
    sprintf(send_msg,"JOIN:%s",group_name);
    //sprintf(display_string,"Sending Join message for group %s\n",group_name );
    //PRINT(display_string);

    if ((numbytes = send(cfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending join  msg.");
        return false;
    }
    return true;
}
*/
int multicast_join(struct sockaddr_in group_ip, unsigned int port)
{
  int status=0;
  int sd;
  char group_ip_address[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &(group_ip), group_ip_address, INET_ADDRSTRLEN);

  /* Create a datagram socket on which to receive. */
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
    perror("Opening datagram socket error");
    exit(1);
  }
 
  /* Enable SO_REUSEADDR to allow multiple instances of this */
  /* application to receive copies of the multicast datagrams. */
  {
    int reuse = 1;
    status =setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    if(status < 0) {
      perror("Setting SO_REUSEADDR error");
      close(sd);
      exit(1);
    }
  }
 
/* Bind to the proper port number with the IP address */
/* specified as INADDR_ANY. */
  memset((char *) &localSock, 0, sizeof(localSock));
  localSock.sin_family = AF_INET;
  localSock.sin_port = htons(port);
  localSock.sin_addr.s_addr = INADDR_ANY;

  if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock)))
  {
    perror("Binding datagram socket error");
    close(sd);
    exit(1);
  }
 
/* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
/* interface. Note that this IP_ADD_MEMBERSHIP option must be */
/* called for each local interface over which the multicast */
/* datagrams are to be received. */
  group.imr_multiaddr.s_addr = inet_addr(group_ip_address);
  group.imr_interface.s_addr = INADDR_ANY;

  if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
  {
    PRINT("New group ip address=%s ",group_ip_address);
    perror("Adding multicast group error");
    close(sd);
    exit(1);
  }
 
    status=make_socket_non_blocking(sd);
    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

    return sd;
}

bool multicast_leave(int mcast_fd, struct sockaddr_in group_ip)
{
  struct ip_mreq group;
  char group_ip_address[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &(group_ip), group_ip_address, INET_ADDRSTRLEN);

  group.imr_multiaddr.s_addr = inet_addr(group_ip_address);
  group.imr_interface.s_addr = INADDR_ANY;

  if(setsockopt(mcast_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
  {
    PRINT("Group Ip Address = %s & mcast_fd = %d",group_ip_address, mcast_fd);
    perror("Leaving multicast group error");
    exit(1);
  }

  close(mcast_fd);
  return TRUE;
}


