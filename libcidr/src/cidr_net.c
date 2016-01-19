/*
 * Functions to generate various networks based on a CIDR
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libcidr.h>


/* Get the CIDR's immediate supernet */
CIDR *
cidr_net_supernet(const CIDR *addr)
{
	int i, j;
	int pflen;
	CIDR *toret;

	/* Quick check */
	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	/* If it's already a /0 in its protocol, return nothing */
	pflen = cidr_get_pflen(addr);
	if(pflen==0)
	{
		errno = 0;
		return(NULL);
	}

	toret = cidr_dup(addr);
	if(toret==NULL)
		return(NULL); /* Preserve errno */

	/* Chop a bit off the netmask */
	/* This gets the last network bit */
	if(toret->proto==CIDR_IPV4)
		pflen += 96;
	pflen--;
	i = pflen / 8;
	j = 7 - (pflen % 8);

	/* Make that bit a host bit */
	(toret->mask)[i] &= ~(1<<j);

	/*
	 * Now zero out the host bits in the addr.  Do this manually instead
	 * of calling cidr_addr_network() to save some extra copies and
	 * malloc()'s and so forth.
	 */
	for(/* i */ ; i<=15 ; i++)
	{
		for(/* j */ ; j>=0 ; j--)
			(toret->addr)[i] &= ~(1<<j);
		j=7;
	}

	/* And send it back */
	return(toret);
}


/* Get the CIDR's two children */
CIDR **
cidr_net_subnets(const CIDR *addr)
{
	int i, j;
	int pflen;
	CIDR **toret;

	if(addr==NULL)
	{
		errno = EFAULT;
		return(NULL);
	}

	/* You can't split a host address! */
	pflen = cidr_get_pflen(addr);
	if(  (addr->proto==CIDR_IPV4 && pflen==32)
	  || (addr->proto==CIDR_IPV6 && pflen==128))
	{
		errno = 0;
		return(NULL);
	}

	toret = malloc(2 * sizeof(CIDR *));
	if(toret==NULL)
	{
		errno = ENOMEM;
		return(NULL);
	}

	/* Get a blank-ish slate for the first kid */
	toret[0] = cidr_addr_network(addr);
	if(toret[0]==NULL)
	{
		free(toret);
		return(NULL); /* Preserve errno */
	}

	/* Find its first host bit */
	if(toret[0]->proto==CIDR_IPV4)
		pflen += 96;
	i = pflen / 8;
	j = 7 - (pflen % 8);

	/* Make it a network bit */
	(toret[0])->mask[i] |= 1<<j;

	/* Now dup the second kid off that */
	toret[1] = cidr_dup(toret[0]);
	if(toret[1]==NULL)
	{
		cidr_free(toret[0]);
		free(toret);
		return(NULL); /* Preserve errno */
	}

	/* And set that first host bit */
	(toret[1])->addr[i] |= 1<<j;


	/* Return the pair */
	return(toret);
}
