/*
 * Test inaddr-related functions
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
	CIDR *cad, *ctmp;
	char *cstr, *ctstr;
	struct in_addr iaddr;
	struct in6_addr i6addr;
#define PSLEN 256
	char pstr[PSLEN];
	int proto;

	pname = *argv++;
	argc--;

	if(argc==0)
		usage();

	/* All the rest of the args are addresses to run */
	while(*argv!=NULL)
	{
		/* First, CIDR -> inaddr */
		proto = 0;
		cad = cidr_from_str(*argv);
		if(cad==NULL)
			printf("***> ERROR: Couldn't parse '%s'!\n", *argv);
		else
		{
			proto = cad->proto;
			if(proto==CIDR_IPV4)
			{
				/* Try the in_addr */
				if(cidr_to_inaddr(cad, &iaddr)==NULL)
					printf("cidr_to_inaddr() failed\n");
				else
				{
					/* Translate back using system function */
					if(inet_ntop(AF_INET, &iaddr, pstr, PSLEN)==NULL)
						printf("inet_ntop(AF_INET) failed\n");
					else
					{
						/* Show */
						cstr = cidr_to_str(cad, CIDR_ONLYADDR);
						printf("CIDR '%s' -> in_addr '%s'\n",
								cstr, pstr);
						free(cstr);
					}

					/* Make sure we get back what we gave */
					ctmp = cidr_from_inaddr(&iaddr);
					if(cidr_equals(ctmp, cad)!=0)
					{
						cstr = cidr_to_str(cad, CIDR_NOFLAGS);
						ctstr = cidr_to_str(ctmp, CIDR_NOFLAGS);
						printf("Warning: Gave '%s', got back '%s'\n",
								cstr, ctstr);
						free(cstr);
						free(ctstr);
					}
					cidr_free(ctmp);
				}
			}

			/* Now (whether v4 or v6) try in6_addr */
			if(cidr_to_in6addr(cad, &i6addr)==NULL)
				printf("cidr_to_in6addr() failed\n");
			else
			{
				/* Translate back using system function */
				if(inet_ntop(AF_INET6, &i6addr, pstr, PSLEN)==NULL)
					printf("inet_ntop(AF_INET6) failed\n");
				else
				{
					/* Show */
					cstr = cidr_to_str(cad, CIDR_ONLYADDR);
					printf("CIDR '%s' -> in6_addr '%s'\n",
							cstr, pstr);
					free(cstr);

					/*
					 * Make sure we get back what we gave, but skip
					 * warning if we're a v4 address (since it will come
					 * back as a v6, so the proto's won't be equal).
					 */
					ctmp = cidr_from_in6addr(&i6addr);
					if(cidr_equals(ctmp, cad)!=0 && cad->proto==CIDR_IPV6)
					{
						cstr = cidr_to_str(cad, CIDR_NOFLAGS);
						ctstr = cidr_to_str(ctmp, CIDR_NOFLAGS);
						printf("Warning: Gave '%s', got back '%s'\n",
								cstr, ctstr);
						free(cstr);
						free(ctstr);
					}
					cidr_free(ctmp);
				}
			}
			cidr_free(cad);
		}


		/* Now, go the other way around */
		/* Don't bother trying if we couldn't parse it */
		if(proto==CIDR_IPV4)
		{
			if(inet_pton(AF_INET, *argv, &iaddr)!=1)
				printf("inet_pton(AF_INET, '%s') failed\n", *argv);
			else
			{
				/* Translate back */
				cad = cidr_from_inaddr(&iaddr);
				if(cad==NULL)
					printf("cidr_from_inaddr() failed\n");
				else
				{
					cstr = cidr_to_str(cad, CIDR_ONLYADDR);
					printf("in_addr '%s' -> CIDR '%s'\n",
							*argv, cstr);
					free(cstr);
					cidr_free(cad);
				}
			}
		}
		/*
		 * Note: inet_pton(AF_INET6) doesn't seem to handle a V4 address
		 * in normal dotted-quad form.  How bizarre.  Well, only test the
		 * _from_in6addr in v6-world.
		 */
		if(proto==CIDR_IPV6)
		{
			if(inet_pton(AF_INET6, *argv, &i6addr)!=1)
				printf("inet_pton(AF_INET6, '%s') failed\n", *argv);
			else
			{
				/* Translate back */
				cad = cidr_from_in6addr(&i6addr);
				if(cad==NULL)
					printf("cidr_from_in5addr() failed\n");
				else
				{
					cstr = cidr_to_str(cad, CIDR_ONLYADDR);
					printf("in6_addr '%s' -> CIDR '%s'\n",
							*argv, cstr);
					free(cstr);
					cidr_free(cad);
				}
			}
		}


		/* Move on */
		printf("\n");
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
