/*
 * Show all the subnets of a given CIDR
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcidr.h>

char *pname;
void showkids(const CIDR *, short depth);
void usage(void);

int
main(int argc, char *argv[])
{
	CIDR *tcidr;
	char *tstr;
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
			/* XXX Sanity */
			if(tcidr->proto==CIDR_IPV4 && cidr_get_pflen(tcidr)<24)
			{
				printf("***> Error: No v4 prefixes shorter than /24\n");
				argv++;
				continue;
			}
			else if(tcidr->proto==CIDR_IPV6 && cidr_get_pflen(tcidr)<120)
			{
				printf("***> Error: No v6 prefixes shorter than /120\n");
				argv++;
				continue;
			}
			tstr = cidr_to_str(tcidr, cflags);
			printf("Subdividing %s:\n", tstr);
			free(tstr);

			showkids(tcidr, 1);
		}

		argv++;
	}

	exit(0);
}


void
showkids(const CIDR *cur, short depth)
{
	int i;
	CIDR **kids;
	char *net;

	/* Get the kids */
	kids = cidr_net_subnets(cur);
	if(kids==NULL)
		return;
	
	/* Show 'em and recurse to their kids */
	for(i=0 ; i<=1 ; i++)
	{
#define PADSTR "........................................"
		net = cidr_to_str(kids[i], CIDR_NOFLAGS);
		printf("%*.*s %s\n", depth-1, depth-1,
				PADSTR, net);
		free(net);
		showkids(kids[i], depth+1);
		cidr_free(kids[i]);
	}
	free(kids);

	return;
}


void
usage(void)
{
	printf("Usage: %s address [...]\n\n", pname);
	exit(1);
}
