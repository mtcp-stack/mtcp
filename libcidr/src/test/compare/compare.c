/*
 * Compare two CIDR blocks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcidr.h>

char *pname;
void usage(void);

int
main(int argc, char *argv[])
{
	char *ifirst, *isecond;
	CIDR *first, *second;
	char *sfirst, *ssecond;
	int cflags;

	cflags=CIDR_NOFLAGS;
	pname = *argv;

	if(argc!=3)
		usage();

	/* Now let's compare */
	ifirst = argv[1];
	isecond = argv[2];

	if(ifirst==NULL || strlen(ifirst)==0
	   || isecond==NULL || strlen(isecond)==0)
	{
		printf("Error: Can't get cidr-block's from your input!\n");
		usage();
	}

	/* Parse 'em both */
	first = cidr_from_str(ifirst);
	second = cidr_from_str(isecond);

	if(first==NULL)
	{
		printf("Error: Can't parse cidr-block '%s'\n", ifirst);
		usage();
	}
	if(second==NULL)
	{
		printf("Error: Can't parse cidr-block '%s'\n", isecond);
		usage();
	}

	sfirst = cidr_to_str(first, cflags);
	ssecond = cidr_to_str(second, cflags);


	/*
	 * OK, now we've got 'em.  Start some comparisons.
	 * Note that none of the following is an _error_; they're all
	 * answers.
	 */
#define PROTOSTR(x) (((x)->proto==CIDR_IPV4)?"IPv4":"IPv6")
#define DONE \
		{ \
			free(sfirst); free(ssecond); \
			cidr_free(first); cidr_free(second); \
			exit(0); \
		}
	
	/* Are they even the same address family? */
	if(first->proto != second->proto)
	{
		printf("Blocks are different address families:\n"
		       "  - '%s' is %s\n"
		       "  - '%s' is %s\n",
		       sfirst, PROTOSTR(first),
		       ssecond, PROTOSTR(second));
		DONE;
	}

	/* First inside second? */
	if(cidr_contains(second, first)==0)
	{
		printf("%s block '%s' is wholly contained within '%s'\n",
				PROTOSTR(first), sfirst, ssecond);
		DONE;
	}

	/* Second inside first? */
	if(cidr_contains(first, second)==0)
	{
		printf("%s block '%s' is wholly contained within '%s'\n",
				PROTOSTR(first), ssecond, sfirst);
		DONE;
	}

	/* Otherwise, they're totally unrelated */
	printf("%s blocks '%s' and '%s' don't intersect.\n",
			PROTOSTR(first), ssecond, sfirst);
	DONE;

	/* NOTREACHED */
}


void
usage(void)
{
	printf("Usage: %s cidr-block cidr-block\n\n"
	       "       Compares the two given blocks, and tells you what\n"
	       "       their relationship is to each other (same, one inside\n"
	       "       the other, non-overlapping, etc).\n\n",
	       pname);
	exit(1);
}
