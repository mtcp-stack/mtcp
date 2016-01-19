/*
 * cidr_get - Get and return various semi-raw bits of info
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libcidr.h>


/* Get the prefix length */
int
cidr_get_pflen(const CIDR *block)
{
	int i, j;
	int foundnmh;
	int pflen;

	if(block==NULL)
	{
		errno = EFAULT;
		return(-1);
	}

	/* Where do we start? */
	if(block->proto==CIDR_IPV4)
		i=12;
	else if(block->proto==CIDR_IPV6)
		i=0;
	else
	{
		errno = ENOENT; /* Bad errno */
		return(-1); /* Unknown */
	}

	/*
	 * We're intentionally not supporting non-contiguous netmasks.  So,
	 * if we find one, bomb out.
	 */
	foundnmh=0;
	pflen=0;
	for(/* i */ ; i<=15 ; i++)
	{
		for(j=7 ; j>=0 ; j--)
		{
			if((block->mask)[i] & (1<<j))
			{
				/*
				 * This is a network bit (1).  If we've already seen a
				 * host bit (0), we need to bomb.
				 */
				if(foundnmh==1)
				{
					errno = EINVAL;
					return(-1);
				}

				pflen++;
			}
			else
				foundnmh=1; /* A host bit */
		}
	}

	/* If we get here, return the length */
	return(pflen);
}


/* Get the address bits */
uint8_t *
cidr_get_addr(const CIDR *addr)
{
	uint8_t *toret;

	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	toret = malloc(16*sizeof(uint8_t));
	if(toret==NULL)
	{
		errno = ENOMEM;
		return(NULL);
	}

	/* Copy 'em in */
	memcpy(toret, addr->addr, sizeof(addr->addr));

	return(toret);
}


/* Get the netmask bits */
uint8_t *
cidr_get_mask(const CIDR *addr)
{
	uint8_t *toret;

	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	toret = malloc(16*sizeof(uint8_t));
	if(toret==NULL)
	{
		errno = ENOMEM;
		return(NULL);
	}

	/* Copy 'em in */
	memcpy(toret, addr->mask, sizeof(addr->mask));

	return(toret);
}


/* Get the protocol */
int
cidr_get_proto(const CIDR *addr)
{

	if(addr==NULL)
	{
		errno = EFAULT;
		return(-1);
	}

	return(addr->proto);
}
