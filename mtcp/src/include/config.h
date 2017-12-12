#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "ps.h"

extern int num_cpus;
extern int num_queues;
extern int num_devices;

extern int num_devices_attached;
extern int devices_attached[MAX_DEVICES];

int 
LoadConfiguration(const char *fname);

/* set configurations from the setted 
   interface information */
int
SetInterfaceInfo();

/* set configurations from the files */
int 
SetRoutingTable();

int 
LoadARPTable();

/* print setted configuration */
void 
PrintConfiguration();

void 
PrintInterfaceInfo();

void 
PrintRoutingTable();

/* set socket modes */
int
SetSocketMode(int8_t socket_mode);

/* fetch mask from prefix */
uint32_t 
MaskFromPrefix(int prefix);

void
ParseMACAddress(unsigned char *haddr, char *haddr_str);

int 
ParseIPAddress(uint32_t *ip_addr, char *ip_str);

#endif /* __CONFIG_H_ */
