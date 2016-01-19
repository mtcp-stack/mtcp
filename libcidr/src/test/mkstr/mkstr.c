/*
 * Show some examples of translating to/from CIDR format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcidr.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

char *pname;
void usage(void);

int
main(int argc, char *argv[])
{
	CIDR *tcidr;
	char *tstr;
	int cflags, goch;
	short regr=0;

	cflags=CIDR_NOFLAGS;
	pname = *argv;
	while((goch=getopt(argc, argv, "ev6cmapwrf:_"))!=-1)
	{
		switch((char)goch)
		{
			case 'e':
				cflags |= CIDR_NOCOMPACT;
				break;
			case 'v':
				cflags |= CIDR_VERBOSE;
				break;
			case '6':
				cflags |= CIDR_USEV6;
				break;
			case 'c':
				cflags |= (CIDR_USEV6 | CIDR_USEV4COMPAT);
				break;
			case 'm':
				cflags |= CIDR_NETMASK;
				break;
			case 'a':
				cflags |= CIDR_ONLYADDR;
				break;
			case 'p':
				cflags |= CIDR_ONLYPFLEN;
				break;
			case 'w':
				cflags |= CIDR_WILDCARD;
				break;
			case 'r':
				cflags |= CIDR_REVERSE;
				break;
			case 'f':
				if(strcmp(optarg, "4")==0)
					cflags |= CIDR_FORCEV4;
				else if(strcmp(optarg, "6")==0)
					cflags |= CIDR_FORCEV6;
				else
				{
					printf("Error: -f needs an argument.\n");
					usage();
				}
				break;
			case '_':
				regr=1; /* Hidden arg for regression test mode */
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
		tstr = NULL;
		tcidr = cidr_from_str(*argv);
		if(tcidr==NULL)
			if(regr==1)
				printf("'%s' -> 'FROMFAILED'\n", *argv);
			else
				printf("***> ERROR: From '%s', got NULL!!\n", *argv);
		else
		{
			tstr = cidr_to_str(tcidr, cflags);
			if(tstr==NULL)
				if(regr==1)
					printf("'%s' -> 'TOFAILED'\n", *argv);
				else
					printf("***> ERROR: From '%s', got tcidr, got "
							"str NULL!!\n", *argv);
			else
				if(regr==1)
					printf("'%s' -> '%s'\n", *argv, tstr);
				else
					printf("From '%s', got str '%s'.\n", *argv, tstr);

			cidr_free(tcidr);
			free(tstr);
		}

		argv++;
	}

	exit(0);
}


void
usage(void)
{
	/*
	 * Split into two to pacify C89-spec'd gcc:
	 * mkstr.c:124: warning: string length `736' is greater than the
	 *        length `509' ISO C89 compilers are required to support
	 */
	printf("Usage: %s -[ev6cmwap] [-f [4|6]] address [...]\n\n"
	       "       -e  Expand zeros instead of ::'ing [v6]\n"
	       "       -v  Show leading 0's in octets [v6]\n"
	       "       -f  Force parsing of address as v4 or v6 [v4/v6]\n"
	       "           (depending on the arg to -f)\n"
	       "       -6  Use v6-mapped form for addresses [v4]\n"
	       "       -c  Use v6-compat form for addresses [v4]\n"
	       "           (implies -6)\n", pname);
	printf("       -m  Show netmask instead of prefix length [v4/v6]\n"
	       "       -w  Show wildcard mask instead of netmask [v4/v6]\n"
	       "           (meaningless without -m)\n"
	       "       -a  Show only the address, not the prefix [v4/v6]\n"
	       "       -p  Show only the prefix length, not the address [v4/v6]\n"
	       "           (or show netmask when combined with -m)\n"
	       "       -r  Show .{in-addr,ip6}.arpa PTR forms [v4/v6]\n"
	       "\n");
	exit(1);
}
