#pragma once
#include<string.h>
#include<network.h>
#define T_IPv4ADDRESS 1 //Ipv4 address
#define T_NAMESERVER 2 //Nameserver
#define T_CNAME 5 // canonical name
#define T_SOA 6 /* start of authority zone */
#define T_DOMAINNAMEPTR 12 /* domain name pointer */
#define T_MAILSERVER 15 //Mail server

extern char dns_servers[10][100];
extern int dns_server_count;
//Function Prototypes
sockaddr_in pm_gethostbyname(unsigned char* hostname, int qtype);
void pm_changetoDnsNameFormat(unsigned char*, unsigned char*);
unsigned char* pm_readName(unsigned char*, unsigned char*, int*);
void pm_getDnsServers(const char* configFilePath);