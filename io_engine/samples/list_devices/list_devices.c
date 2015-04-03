#include <stdio.h>

#include "../../include/ps.h"

int main()
{
	int num_devices;
	struct ps_device devices[MAX_DEVICES];

	int i;

	num_devices = ps_list_devices(devices);
	if (num_devices == -1) {
		perror("ps_list_devices");
		return 1;
	}

	printf("found %d device(s).\n", num_devices);

	for (i = 0; i < num_devices; i++) {
		struct ps_device *dev = &devices[i];
		char *t = (char *)&dev->ip_addr;

		printf("%d: %s ",
				dev->ifindex, 
				dev->name);
		
		printf("(%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX)  ", 
				dev->dev_addr[0],
				dev->dev_addr[1],
				dev->dev_addr[2],
				dev->dev_addr[3],
				dev->dev_addr[4],
				dev->dev_addr[5]);

		printf("%u.%u.%u.%u  ", t[0], t[1], t[2], t[3]);

		printf("%d RX, %d TX queues\n", 
				dev->num_rx_queues,
				dev->num_tx_queues);
	}

	return 0;
}
