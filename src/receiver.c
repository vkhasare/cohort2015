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

//struct sockaddr_in localSock;
//struct ip_mreq group;
//int datalen;
//char databuf[1024];

/* <doc>
 * int multicast_join(struct sockaddr_in group_ip, unsigned int port)
 * Client joins the multicast group by giving the system call to kernel.
 * This function takes parameter as
 *  - multicast group address
 *  - multicast group port
 *
 * </doc>
 */
int multicast_join(struct sockaddr_in group_ip, unsigned int port)
{
  int status=0;
  int sd;
  char group_ip_address[INET6_ADDRSTRLEN];
  struct sockaddr_in localSock;
  struct ip_mreq group;

  inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(group_ip)), group_ip_address, INET6_ADDRSTRLEN);

  /* Create a datagram socket on which to receive. */
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
    perror("Opening datagram socket error");
    exit(1);
  }
 
  /* Enable SO_REUSEADDR to allow multiple instances of this
   * application to receive copies of the multicast datagrams.
   */
  {
    int reuse = 1;
    status =setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    if(status < 0) {
      perror("Setting SO_REUSEADDR error");
      close(sd);
      exit(1);
    }
  }
 
  /* Bind to the proper port number with the IP address
   * specified as INADDR_ANY.
   */
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
 
  /* Join the multicast group 226.1.1.1 on the local 203.106.93.94
   * interface. Note that this IP_ADD_MEMBERSHIP option must be
   * called for each local interface over which the multicast
   * datagrams are to be received.
   */
  group.imr_multiaddr.s_addr = inet_addr(group_ip_address);
  group.imr_interface.s_addr = INADDR_ANY;

  if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
  {
    PRINT("New group ip address=%s ",group_ip_address);
    perror("Adding multicast group error");
    close(sd);
    exit(1);
  }

    /* PRINT("listening on %s",group_ip_address); */

   status=make_socket_non_blocking(sd);
   if (status == -1)
   {
       perror("\nError while making the socket non-blocking.");
       exit(0);
   }

   return sd;
}

/* <doc>
 * bool multicast_leave(int mcast_fd, struct sockaddr_in group_ip)
 * Client leaves the multicast group address. This function takes
 * parameter as
 *  - multicast group address
 *  - Multicast FD on which client is bound
 *
 * </doc>
 */
bool multicast_leave(int mcast_fd, struct sockaddr_in group_ip)
{
  struct ip_mreq group;
  char group_ip_address[INET6_ADDRSTRLEN];

  //inet_ntop(AF_INET, &(group_ip), group_ip_address, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&(group_ip)), group_ip_address, INET6_ADDRSTRLEN);

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


