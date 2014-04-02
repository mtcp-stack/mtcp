#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "ps.h"

int num_cpus;
int num_queues;
int num_devices;
struct ps_device devices[MAX_DEVICES];

int num_devices_attached;
int devices_attached[MAX_DEVICES];

int 
LoadConfiguration(char *fname);

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

#endif /* __CONFIG_H_ */
