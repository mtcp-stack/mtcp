/*
 * Implement cidrcalc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcidr.h>

/* Width for the line defs */
#define DWID 9

/* Gen up an octet in binary */
#define OCTET_BIN(oct) \
			{ \
				memset(boct, 0, 9); \
				for(obi = 7 ; obi>=0 ; obi--) \
					if( ((oct >> obi) & 1) == 1) \
						boct[7-obi] = '1'; \
					else \
						boct[7-obi] = '0'; \
			}

/* Show binary form of something */
#define SHOWBIN(arr, pname) \
			{ \
				printf("%*s:", DWID, "Bin" pname); \
				if(proto==CIDR_IPV4) \
				{ \
					/* Show v4 inline */ \
					for(i=12 ; i<=15 ; i++) \
					{ \
						OCTET_BIN(arr[i]) \
						printf(" %s", boct); \
					} \
 	 	 	 	 	\
					/* Now skip to the same starting point */ \
					printf("\n%*s ", DWID, ""); \
 	 	 	 	 	\
					/* And show the decimal octets below */ \
					for(i=12 ; i<=15 ; i++) \
						printf(" %5d%3s", arr[i], ""); \
					printf("\n"); \
				} \
				else if(proto==CIDR_IPV6) \
				{ \
					/* v6 needs to span multiple lines */ \
					for(i=0 ; i<=3 ; i++) \
					{ \
						/* 4 octets in binary */ \
						for(j=i*4 ; j<=(i*4)+3 ; j++) \
						{ \
							OCTET_BIN(arr[j]) \
							printf(" %s", boct); \
						} \
						\
						/* Those 4 octets in hex */ \
						printf("\n%*s ", DWID, ""); \
						for(j=i*4 ; j<=i*4+3 ; j++) \
							printf("    %.2x   ", arr[j]); \
						\
						/* Prep for next round */ \
						if(i<3) \
							printf("\n%*s ", DWID, ""); \
						else \
							printf("\n"); \
					} \
				} \
			}

/* Globals/prototypes */
char *pname;
void usage(void);

int
main(int argc, char *argv[])
{
	CIDR *addr, *addr2, *addr3, **kids;
	char *astr, *astr2;
	char boct[9];
	int obi;
	int i, j;
	const char *cstr;
	int goch;
	short proto;
	short showbin, showss;
	uint8_t *bits;

	pname = *argv;
	showbin = showss = 0;

	while((goch=getopt(argc, argv, "bs"))!=-1)
	{
		switch((char)goch)
		{
			case 'b':
				showbin = 1;
				break;
			case 's':
				showss = 1;
				break;
			default:
				printf("Unknown argument: '%c'\n", goch);
				usage();
				/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if(argc==0)
		usage();

	/* All the rest of the args are addresses to run */
	while(*argv!=NULL)
	{
		astr = NULL;
		addr = cidr_from_str(*argv);
		if(addr ==NULL)
			printf("***> ERROR: Couldn't parse address '%s'.\n\n", *argv);
		else
		{
			/* Start putting out the pieces */
			proto = cidr_get_proto(addr);

			/* Address */
			astr = cidr_to_str(addr, CIDR_ONLYADDR);
			printf("%*s: %s\n", DWID, "Address", astr);
			free(astr);


			/* Check if it's v4-mapped */
			if(proto==CIDR_IPV6 && cidr_is_v4mapped(addr)==0)
			{
				astr = cidr_to_str(addr,
						CIDR_ONLYADDR | CIDR_FORCEV4 | CIDR_USEV6);
				printf("%*s: %s\n", DWID, "v4-mapped", astr);
				free(astr);
			}



			/* Show the full 'expanded' address form */
			if(proto==CIDR_IPV6)
			{
				astr = cidr_to_str(addr,
						CIDR_VERBOSE | CIDR_NOCOMPACT | CIDR_ONLYADDR);
				printf("%*s: %s\n", DWID, "Expanded", astr);
				free(astr);
			}


			/* Netmask */
			astr = cidr_to_str(addr, CIDR_ONLYPFLEN);
			astr2 = cidr_to_str(addr, CIDR_ONLYPFLEN | CIDR_NETMASK);
			printf("%*s: %s (/%s)\n", DWID, "Netmask", astr2, astr);
			free(astr);
			free(astr2);


			/* Show binary forms? */
			if(showbin==1)
			{
				bits = cidr_get_addr(addr);
				SHOWBIN(bits, "Addr")
				free(bits);
				bits = cidr_get_mask(addr);
				SHOWBIN(bits, "Mask")
				free(bits);
			}


			/* Wildcard mask */
			astr = cidr_to_str(addr,
					CIDR_ONLYPFLEN | CIDR_NETMASK | CIDR_WILDCARD);
			/* Spaced to match above */
			printf("%*s: %s\n", DWID, "Wildcard", astr);
			free(astr);


			/* Network and broadcast */
			addr2 = cidr_addr_network(addr);
			astr = cidr_to_str(addr2, CIDR_NOFLAGS);
			printf("%*s: %s\n", DWID, "Network", astr);
			free(astr);
			cidr_free(addr2);

			addr2 = cidr_addr_broadcast(addr);
			astr = cidr_to_str(addr2, CIDR_ONLYADDR);
			printf("%*s: %s\n", DWID, "Broadcast", astr);
			free(astr);
			cidr_free(addr2);


			/* Range of hosts */
			addr2 = cidr_addr_hostmin(addr);
			astr = cidr_to_str(addr2, CIDR_ONLYADDR);
			addr3 = cidr_addr_hostmax(addr);
			astr2 = cidr_to_str(addr3, CIDR_ONLYADDR);
			printf("%*s: %s - %s\n", DWID, "Hosts", astr, astr2);
			free(astr);
			free(astr2);
			cidr_free(addr2);
			cidr_free(addr3);


			/* Num of hosts */
			cstr = cidr_numhost(addr);
			printf("%*s: %s\n", DWID, "NumHosts", cstr);
			/* Don't free cstr */


			/* Super/subs? */
			if(showss==1)
			{
				/* Parent network */
				addr2 = cidr_net_supernet(addr);
				if(addr2!=NULL)
				{
					astr = cidr_to_str(addr2, CIDR_NOFLAGS);
					printf("%*s: %s\n", DWID, "Supernet", astr);
					free(astr);
					cidr_free(addr2);
				}
				else
					printf("%*s: (none)\n", DWID, "Supernet");

				
				/* Children networks */
				kids = cidr_net_subnets(addr);
				if(kids!=NULL)
				{
					astr = cidr_to_str(kids[0], CIDR_NOFLAGS);
					astr2 = cidr_to_str(kids[1], CIDR_NOFLAGS);
					printf("%*s: %s\n%*s  %s\n", DWID, "Subnets", astr,
							DWID, "", astr2);
					free(astr);
					free(astr2);
					cidr_free(kids[0]);
					cidr_free(kids[1]);
					free(kids);
				}
				else
					printf("%*s: (none)\n", DWID, "Subnets");
			}


			/* That's it for this address */
			cidr_free(addr);
			printf("\n");
		}

		argv++;
	}

	exit(0);
}


void
usage(void)
{
	printf("Usage: %s [-bs] address [...]\n"
	       "       -b  Show binary expansions\n"
	       "       -s  Show super and subnets\n"
	       "\nUsing libcidr %s\n", pname, cidr_version());
	exit(1);
}
