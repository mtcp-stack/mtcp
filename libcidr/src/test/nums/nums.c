/*
 * Show number of addresses/hosts in a subnet
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
	CIDR *tcidr;
	char *tstr;
	const char *naddr, *nhost;

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
			naddr = cidr_numaddr(tcidr);
			nhost = cidr_numhost(tcidr);

			printf("Address:   '%s'\n"
			       "    Hosts:       '%s'\n"
			       "    Total Addrs: '%s'\n", tstr, nhost, naddr);

			free(tstr);
			cidr_free(tcidr);
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
