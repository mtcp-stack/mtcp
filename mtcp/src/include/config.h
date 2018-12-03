#ifndef CONFIG_H
#define CONFIG_H

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

/* fetch mask from prefix */
uint32_t 
MaskFromPrefix(int prefix);

void
ParseMACAddress(unsigned char *haddr, char *haddr_str);

int 
ParseIPAddress(uint32_t *ip_addr, char *ip_str);

#endif /* CONFIG_H */
