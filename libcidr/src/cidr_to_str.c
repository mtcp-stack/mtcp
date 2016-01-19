/*
 * cidr_to_str() - Generate a textual representation of the given CIDR
 * subnet.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcidr.h>

char *
cidr_to_str(const CIDR *block, int flags)
{
	int i;
	int zst, zcur, zlen, zmax;
	short pflen;
	short lzer; /* Last zero */
	char *toret;
	char tmpbuf[128]; /* We shouldn't need more than ~5 anywhere */
	CIDR *nmtmp;
	char *nmstr;
	int nmflags;
	uint8_t moct;
	uint16_t v6sect;

	/* Just in case */
	if( (block==NULL) || (block->proto==CIDR_NOPROTO) )
	{
		errno = EINVAL;
		return(NULL);
	}

	/*
	 * Sanity: If we have both ONLYADDR and ONLYPFLEN, we really don't
	 * have anything to *DO*...
	 */
	if((flags & CIDR_ONLYADDR) && (flags & CIDR_ONLYPFLEN))
	{
		errno = EINVAL;
		return(NULL);
	}

	/*
	 * Now, in any case, there's a maximum length for any address, which
	 * is the completely expanded form of a v6-{mapped,compat} address
	 * with a netmask instead of a prefix.  That's 8 pieces of 4
	 * characters each (32), separated by :'s (+7=39), plus the slash
	 * (+1=40), plus another separated-8*4 (+39=79), plus the trailing
	 * null (+1=80).  We'll just allocate 128 for kicks.
	 *
	 * I'm not, at this time anyway, going to try and allocate only and
	 * exactly as much as we need for any given address.  Whether
	 * consumers of the library can count on this behavior...  well, I
	 * haven't decided yet.  Lemme alone.
	 */
	toret = malloc(128);
	if(toret==NULL)
	{
		errno = ENOMEM;
		return(NULL);
	}
	memset(toret, 0, 128);

	/*
	 * If it's a v4 address, we mask off everything but the last 4
	 * octets, and just proceed from there.
	 */
	if( (block->proto==CIDR_IPV4 && !(flags & CIDR_FORCEV6))
	   || (flags & CIDR_FORCEV4) )
	{
		/* First off, creating the in-addr.arpa form is special */
		if(flags & CIDR_REVERSE)
		{
			/*
			 * Build the d.c.b.a.in-addr.arpa form.  Note that we ignore
			 * flags like CIDR_VERBOSE and the like here, since they lead
			 * to non-valid reverse paths (or at least, paths that no DNS
			 * implementation will look for).  So it pretty much always
			 * looks exactly the same.  Also, we don't mess with dealing
			 * with netmaks or anything here; we just assume it's a
			 * host address, and treat it as such.
			 */

			sprintf(toret, "%d.%d.%d.%d.in-addr.arpa",
					block->addr[15], block->addr[14],
					block->addr[13], block->addr[12]);
			return(toret);
		}

		/* Are we bothering to show the address? */
		if(!(flags & CIDR_ONLYPFLEN))
		{
			/* If we're USEV6'ing, add whatever prefixes we need */
			if(flags & CIDR_USEV6)
			{
				if(flags & CIDR_NOCOMPACT)
				{
					if(flags & CIDR_VERBOSE)
						strcat(toret, "0000:0000:0000:0000:0000:");
					else
						strcat(toret, "0:0:0:0:0:");
				}
				else
					strcat(toret, "::");

				if(flags & CIDR_USEV4COMPAT)
				{
					if(flags & CIDR_NOCOMPACT)
					{
						if(flags & CIDR_VERBOSE)
							strcat(toret, "0000:");
						else
							strcat(toret, "0:");
					}
				}
				else
					strcat(toret, "ffff:");
			} /* USEV6 */

			/* Now, slap on the v4 address */
			for(i=12 ; i<=15 ; i++)
			{
				sprintf(tmpbuf, "%u", (block->addr)[i]);
				strcat(toret, tmpbuf);
				if(i<15)
					strcat(toret, ".");
			}
		} /* ! ONLYPFLEN */

		/* Are we bothering to show the pf/mask? */
		if(!(flags & CIDR_ONLYADDR))
		{
			/*
			 * And the prefix/netmask.  Don't show the '/' if we're only
			 * showing the pflen/mask.
			 */
			if(!(flags & CIDR_ONLYPFLEN))
				strcat(toret, "/");

			/* Which are we showing? */
			if(flags & CIDR_NETMASK)
			{
				/*
				 * In this case, we can just print out like the address
				 * above.
				 */
				for(i=12 ; i<=15 ; i++)
				{
					moct = (block->mask)[i];
					if(flags & CIDR_WILDCARD)
						moct = ~(moct);
					sprintf(tmpbuf, "%u", moct);
					strcat(toret, tmpbuf);
					if(i<15)
						strcat(toret, ".");
				}
			}
			else
			{
				/*
		 	 	 * For this, iterate over each octet,
		 	 	 * then each bit within the octet.
		 	 	 */
				pflen = cidr_get_pflen(block);
				if(pflen==-1)
				{
					free(toret);
					return(NULL); /* Preserve errno */
				}
				/* Special handling for forced modes */
				if(block->proto==CIDR_IPV6 && (flags & CIDR_FORCEV4))
					pflen -= 96;

				sprintf(tmpbuf, "%u",
						(flags & CIDR_USEV6) ? pflen+96 : pflen);

				strcat(toret, tmpbuf);
			}
		} /* ! ONLYADDR */

		/* That's it for a v4 address, in any of our forms */
	}
	else if( (block->proto==CIDR_IPV6 && !(flags & CIDR_FORCEV4))
	        || (flags & CIDR_FORCEV6) )
	{
		/* First off, creating the .ip6.arpa form is special */
		if(flags & CIDR_REVERSE)
		{
			/*
			 * Build the ...ip6.arpa form.  See notes in the CIDR_REVERSE
			 * section of PROTO_IPV4 above for various notes.
			 */
			sprintf(toret, "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
					"%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
					"%x.%x.%x.%x.%x.ip6.arpa",
					block->addr[15] & 0x0f, block->addr[15] >> 4,
					block->addr[14] & 0x0f, block->addr[14] >> 4,
					block->addr[13] & 0x0f, block->addr[13] >> 4,
					block->addr[12] & 0x0f, block->addr[12] >> 4,
					block->addr[11] & 0x0f, block->addr[11] >> 4,
					block->addr[10] & 0x0f, block->addr[10] >> 4,
					block->addr[9]  & 0x0f, block->addr[9]  >> 4,
					block->addr[8]  & 0x0f, block->addr[8]  >> 4,
					block->addr[7]  & 0x0f, block->addr[7]  >> 4,
					block->addr[6]  & 0x0f, block->addr[6]  >> 4,
					block->addr[5]  & 0x0f, block->addr[5]  >> 4,
					block->addr[4]  & 0x0f, block->addr[4]  >> 4,
					block->addr[3]  & 0x0f, block->addr[3]  >> 4,
					block->addr[2]  & 0x0f, block->addr[2]  >> 4,
					block->addr[1]  & 0x0f, block->addr[1]  >> 4,
					block->addr[0]  & 0x0f, block->addr[0]  >> 4);
			return(toret);
		}
		/* Are we showing the address part? */
		if(!(flags & CIDR_ONLYPFLEN))
		{
			/* It's a simple, boring, normal v6 address */

		 	/* First, find the longest string of 0's, if there is one */
		 	zst = zcur = -1;
		 	zlen = zmax = 0;
		 	for(i=0 ; i<=15 ; i+=2)
		 	{
		 		if(block->addr[i]==0 && block->addr[i+1]==0)
		 		{
		 			/* This section is zero */
		 			if(zcur!=-1)
		 			{
		 				/* We're already in a block of 0's */
		 				zlen++;
		 			}
		 			else
		 			{
		 				/* Starting a new block */
		 				zcur = i;
		 				zlen = 1;
		 			}
		 		}
		 		else
		 		{
		 			/* This section is non-zero */
		 			if(zcur!=-1)
		 			{
		 				/*
		 				 * We were in 0's.  See if we set a new record,
		 				 * and if we did, note it and move on.
		 				 */
		 				if(zlen > zmax)
		 				{
		 					zst = zcur;
		 					zmax = zlen;
		 				}

		 				/* We're out of 0's, so reset start */
		 				zcur = -1;
		 			}
		 		}
		 	}

		 	/*
		 	 * If zcur is !=-1, we were in 0's when the loop ended.  Redo
		 	 * the "if we have a record, update" logic.
		 	 */
		 	if(zcur!=-1 && zlen>zmax)
		 	{
		 		zst = zcur;
		 		zmax = zlen;
		 	}


			/*
			 * Now, what makes it HARD is the options we have.  To make
			 * some things simpler, we'll take two octets at a time for
			 * our run through.
		 	 */
			lzer = 0;
			for(i=0 ; i<=15 ; i+=2)
			{
				/*
				 * Start with a cheat; if this begins our already-found
				 * longest block of 0's, and we're not NOCOMPACT'ing,
				 * stick in a ::, increment past them, and keep on
				 * playing.
				 */
				if(i==zst && !(flags & CIDR_NOCOMPACT))
				{
					strcat(toret, "::");
					i += (zmax*2)-2;
					lzer = 1;
					continue;
				}

				/*
			 	 * First, if we're not the first set, we may need a :
			 	 * before us.  If we're not compacting, we always want
			 	 * it.  If we ARE compacting, we want it unless the
			 	 * previous octet was a 0 that we're minimizing.
			 	 */
				if(i!=0 && ((flags & CIDR_NOCOMPACT) || lzer==0))
					strcat(toret, ":");
				lzer = 0; /* Reset */

				/*
			 	 * From here on, we no longer have to worry about
			 	 * CIDR_NOCOMPACT.
			 	 */

				/* Combine the pair of octets into one number */
				v6sect = 0;
				v6sect |= (block->addr)[i] << 8;
				v6sect |= (block->addr)[i+1];

				/*
				 * If we're being VERBOSE, use leading 0's.  Otherwise,
				 * only use as many digits as we need.
				 */
				if(flags & CIDR_VERBOSE)
					sprintf(tmpbuf, "%.4x", v6sect);
				else
					sprintf(tmpbuf, "%x", v6sect);
				strcat(toret, tmpbuf);

				/* And loop back around to the next 2-octet set */
			} /* for(each 16-bit set) */
		} /* ! ONLYPFLEN */

		/* Prefix/netmask */
		if(!(flags & CIDR_ONLYADDR))
		{
			/* Only show the / if we're not showing just the prefix */
			if(!(flags & CIDR_ONLYPFLEN))
				strcat(toret, "/");

			if(flags & CIDR_NETMASK)
			{
				/*
			 	 * We already wrote how to build the whole v6 form, so
			 	 * just call ourselves recurively for this.
			 	 */
				nmtmp = cidr_alloc();
				if(nmtmp==NULL)
				{
					free(toret);
					return(NULL); /* Preserve errno */
				}
				nmtmp->proto = block->proto;
				for(i=0 ; i<=15 ; i++)
					if(flags & CIDR_WILDCARD)
						nmtmp->addr[i] = ~(block->mask[i]);
					else
						nmtmp->addr[i] = block->mask[i];

				/*
				 * Strip flags:
				 * - CIDR_NETMASK would make us recurse forever.
				 * - CIDR_ONLYPFLEN would not show the address bit, which
				 *   is the part we want here.
				 * Add flag CIDR_ONLYADDR because that's the bit we care
				 * about.
				 */
				nmflags = flags;
				nmflags &= ~(CIDR_NETMASK) & ~(CIDR_ONLYPFLEN);
				nmflags |= CIDR_ONLYADDR;
				nmstr = cidr_to_str(nmtmp, nmflags);
				cidr_free(nmtmp);
				if(nmstr==NULL)
				{
					free(toret);
					return(NULL); /* Preserve errno */
				}

				/* No need to strip the prefix, it doesn't have it */

				/* Just add it on */
				strcat(toret, nmstr);
				free(nmstr);
			}
			else
			{
				/* Just figure the and show prefix length */
				pflen = cidr_get_pflen(block);
				if(pflen==-1)
				{
					free(toret);
					return(NULL); /* Preserve errno */
				}
				/* Special handling for forced modes */
				if(block->proto==CIDR_IPV4 && (flags & CIDR_FORCEV6))
					pflen += 96;

				sprintf(tmpbuf, "%u", pflen);
				strcat(toret, tmpbuf);
			}
		} /* ! ONLYADDR */
	}
	else
	{
		/* Well, *I* dunno what the fuck it is */
		free(toret);
		errno = ENOENT; /* Bad choice of errno */
		return(NULL);
	}

	/* Give back the string */
	return(toret);
}
