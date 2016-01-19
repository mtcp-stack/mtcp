/*
 * Gen network/broadcast addresses for a given block
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcidr.h>

char *pname;
void usage(void);

int
main(int argc, char *argv[])
{
	CIDR *tcidr, *net, *bc;
	char *tstr, *netstr, *bcstr;

	pname = *argv++;
	argc--;

	if(argc==0)
		usage();

	/* All the rest of the args are addresses to run */
	while(*argv!=NULL)
	{
		tstr = NULL;
		tcidr = cidr_from_str(*argv);
		if(tcidr==NULL)
			printf("***> ERROR: Couldn't parse '%s'!\n", *argv);
		else
		{
			tstr = cidr_to_str(tcidr, CIDR_NOFLAGS);
			bc = cidr_addr_broadcast(tcidr);
			bcstr = cidr_to_str(bc, CIDR_ONLYADDR);
			net = cidr_addr_network(tcidr);
			netstr = cidr_to_str(net, CIDR_ONLYADDR);

			printf("Address:   '%s'\n"
			       "      Net: '%s'\n"
			       "       Bc: '%s'\n", tstr, netstr, bcstr);

			free(tstr);
			free(bcstr);
			free(netstr);
			cidr_free(tcidr);
			cidr_free(bc);
			cidr_free(net);
		}

		argv++;
	}

	exit(0);
}


void
usage(void)
{
	printf("Usage: %s address [...]\n\n", pname);
	exit(1);
}
