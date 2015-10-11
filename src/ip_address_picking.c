#include<stdio.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>

void get_my_ip_address(char * ip_address, char * interface_name){
if (interface_name == NULL){
 interface_name="eth0";
}
struct ifaddrs *addrs, *tmp;
getifaddrs(&addrs);
tmp = addrs;

while (tmp) 
{
    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET && !strcmp(tmp->ifa_name,interface_name))
    {
        struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
        printf("%s: %s\n", tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
        strcpy (ip_address,inet_ntoa(pAddr->sin_addr));
        break;
    }

    tmp = tmp->ifa_next;
}
freeifaddrs(addrs);
}

/*int main(void){
  char ip_address[16];
  get_my_ip_address(ip_address,NULL);
  printf("My ip address is %s \n", ip_address);
return 0;
}*/
