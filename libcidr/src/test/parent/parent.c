/*
 * Show all the parent nets of a given CIDR
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
	CIDR *tcidr, *parent;
	char *tstr, *pstr;
	int cflags;

	cflags=CIDR_NOFLAGS;
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
			printf("***> ERROR: Can't parse '%s'!\n", *argv);
		else
		{
			/* Print the first line */
			parent = cidr_addr_network(tcidr);
			tstr = cidr_to_str(tcidr, cflags);
			pstr = cidr_to_str(parent, cflags);
			printf("%s is in the network %s\n", tstr, pstr);
			free(pstr);
			free(tstr);
			cidr_free(tcidr);

			tcidr = parent;
			while((parent = cidr_net_supernet(tcidr)))
			{
				pstr = cidr_to_str(parent, cflags);
				printf("\t which is a subnet of %s\n", pstr);
				free(pstr);

				cidr_free(tcidr);
				tcidr = parent;
			}
			free(tcidr);
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
