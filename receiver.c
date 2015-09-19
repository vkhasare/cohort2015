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
#include "ip_address_picking.c" 
//#include "SLL/client_ll.h"
struct sockaddr_in localSock;
struct ip_mreq group;
//int sd;
int datalen;
char databuf[1024];
#define MATCH_NAME(name1, name2)  (strncmp(name1, name2,2)==0) 
bool send_join_response(int infd, grname_ip_mapping_t * map);


/*int main(int argc, char *argv[])
{
char my_address[16];
get_my_ip_address(my_address, "eth0");
multicast_join(my_address, "226.1.1.1");
}*/

bool join_group(int infd, char * group_name, grname_ip_mapping_t * mapping, int num_groups){
   //void * client_info = get_client_info(infd);  
   int index=0;
   for(index=0;index<num_groups;index++){
     if(MATCH_NAME(group_name, mapping[index].grname)){
        //PRINT(group_name);
        //PRINT(mapping[index].grname);
        if(send_join_response(infd,(mapping+index) )){
          //update_client_group_mapping(client_info, mapping, index); 
        }
        break;
     }
      
    }
}

bool send_join_response(int infd, grname_ip_mapping_t* map){
    int numbytes;
    char display_string[100];
    char send_msg[512];
    char remoteIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(map->grp_ip), remoteIP, INET_ADDRSTRLEN);

    sprintf(send_msg,"JOIN RESPONSE:%s,%s\r\n", map->grname, remoteIP);
    if ((numbytes = send(infd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending join rsp.");
        return FALSE;
    }
    return TRUE; 
}



bool join_msg(int cfd, char * group_name)
{
    int numbytes;
    char display_string[100];
    char msg[] ="JOIN";
    char send_msg[512];
    sprintf(send_msg,"JOIN:%s\r\n",group_name);
    sprintf(display_string,"Sending Join message for group %s\n",group_name );
    PRINT(display_string);
    if ((numbytes = send(cfd,send_msg,(strlen(send_msg) + 1),0)) < 0)
    {
        PRINT("Error in sending join  msg.");
        return FALSE;
    }
    return TRUE;
}

int multicast_join(char * my_ip_address, char *group_ip_address){
int status=0;
int sd;
char display[100];
/* Create a datagram socket on which to receive. */
sd = socket(AF_INET, SOCK_DGRAM, 0);
if(sd < 0)
{
perror("Opening datagram socket error");
exit(1);
}
//else
//PRINT("Opening datagram socket....OK.\n");
 
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
//else
//PRINT("Setting SO_REUSEADDR...OK.\n");
}
 
/* Bind to the proper port number with the IP address */
/* specified as INADDR_ANY. */
memset((char *) &localSock, 0, sizeof(localSock));
localSock.sin_family = AF_INET;
localSock.sin_port = htons(4321);
localSock.sin_addr.s_addr = INADDR_ANY;
if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock)))
{
perror("Binding datagram socket error");
close(sd);
exit(1);
}
//else
//PRINT("Binding datagram socket...OK.\n");
 
/* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
/* interface. Note that this IP_ADD_MEMBERSHIP option must be */
/* called for each local interface over which the multicast */
/* datagrams are to be received. */

group.imr_multiaddr.s_addr = inet_addr(group_ip_address);
group.imr_interface.s_addr = inet_addr(my_ip_address);
if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
{
sprintf(display,"New group ip address=%s & source ip address=%s",group_ip_address,my_ip_address);
PRINT(display);
perror("Adding multicast group error");
close(sd);
exit(1);
}
//else
//PRINT("Adding multicast group...OK.\n");
 
    status=make_socket_non_blocking(sd);
    if (status == -1)
    {
        perror("\nError while making the socket non-blocking.");
        exit(0);
    }

/* Read from the socket. 
datalen = sizeof(databuf);
if(read(sd, databuf, datalen) < 0)
{
perror("Reading datagram message error");
close(sd);
exit(1);
}
else
{
printf("Reading datagram message...OK.\n");
printf("The message from multicast server is: \"%s\"\n", databuf);
}*/

return sd;
}
