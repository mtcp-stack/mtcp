/*
 * Misc pieces
 */

#include <libcidr.h>


static const char *__libcidr_version = CIDR_VERSION_STR;

/* Library version info */
const char *
cidr_version(void)
{

	return(__libcidr_version);
}


/* Is a CIDR a v4-mapped IPv6 address? */
int
cidr_is_v4mapped(const CIDR *addr)
{
	int i;

	if(addr->proto != CIDR_IPV6)
		return(-1);

	/* First 10 octets should be 0 */
	for(i=0 ; i<=9 ; i++)
		if(addr->addr[i] != 0)
			return(-1);

	/* Next 2 should be 0xff */
	for(i=10 ; i<=11 ; i++)
		if(addr->addr[i] != 0xff)
			return(-1);

	/* Then it is */
	return(0);
}
