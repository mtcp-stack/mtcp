/*
 * Show some numbers
 */

#include <errno.h>
#include <string.h>

#include <libcidr.h>
#include <libcidr_pow2_p.h>


/* Number of total addresses in a given prefix length */
const char *
cidr_numaddr_pflen(int pflen)
{

	if(pflen<0 || pflen>128)
	{
		errno = EINVAL;
		return(NULL);
	}
	return(__cidr_pow2[128-pflen]);
}


/* Addresses in a CIDR block */
const char *
cidr_numaddr(const CIDR *addr)
{
	int pflen;

	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	pflen = cidr_get_pflen(addr);
	if(addr->proto==CIDR_IPV4)
		pflen += 96;

	return(cidr_numaddr_pflen(pflen));
}


/* Hosts in a prefix length */
const char *
cidr_numhost_pflen(int pflen)
{

	if(pflen<0 || pflen>128)
	{
		errno = EINVAL;
		return(NULL);
	}
	return(__cidr_pow2m2[128-pflen]);
}


/* Addresses in a CIDR block */
const char *
cidr_numhost(const CIDR *addr)
{
	int pflen;

	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	pflen = cidr_get_pflen(addr);
	if(addr->proto==CIDR_IPV4)
		pflen += 96;

	return(cidr_numhost_pflen(pflen));
}
