/*
 * cidr_from_str() - Generate a CIDR structure from a string in addr/len
 * form.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h> /* I'm always stuffing debug printf's into here */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libcidr.h>

CIDR *
cidr_from_str(const char *addr)
{
	size_t _alen;
	int alen;
	CIDR *toret, *ctmp;
	const char *pfx, *buf;
	char *buf2; /* strtoul() can't use a (const char *) */
	int i, j;
	int pflen;
	unsigned long octet;
	int nocts, eocts;
	short foundpf, foundmask, nsect;

	/* There has to be *SOMETHING* to work with */
	if(addr==NULL || (_alen=strlen(addr))<1)
	{
		errno = EFAULT;
		return(NULL);
	}

	/*
	 * But not too much.  The longest possible is a fully spelled out
	 * IPv6 addr with a fully spelled out netmask (~80 char).  Let's
	 * round way the heck up to 64k.
	 */
	if(_alen > 1<<16)
	{
		errno = EFAULT;
		return(NULL);
	}
	alen = (int)_alen;

	/* And we know it can only contain a given set of chars */
	buf = addr + strspn(addr, "0123456789abcdefABCDEFxX.:/in-rpt");
	if(*buf!='\0')
	{
		errno = EINVAL;
		return(NULL);
	}

	toret = cidr_alloc();
	if(toret==NULL)
		return(NULL); /* Preserve errno */


	/* First check if we're a PTR-style string */
	/*
	 * XXX This could be folded with *pfx; they aren't used in code paths
	 * that overlap.  I'm keeping them separate just to keep my sanity
	 * though.
	 */
	buf = NULL;
	/* Handle the deprecated RFC1886 form of v6 PTR */
	if(strcasecmp(addr+alen-8, ".ip6.int")==0)
	{
		toret->proto = CIDR_IPV6;
		buf = addr+alen-8;
	}

	if(buf!=NULL || strcasecmp(addr+alen-5, ".arpa")==0)
	{
		/*
		 * Do all this processing here, instead of trying to intermix it
		 * with the rest of the formats.  This might lead to some code
		 * duplication, but it'll be easier to read.
		 */
		if(buf==NULL) /* If not set by .ip6.int above */
		{
			/* First, see what protocol it is */
			if(strncasecmp(addr+alen-9, ".ip6", 3)==0)
			{
				toret->proto = CIDR_IPV6;
				buf = addr+alen-9;
			}
			else if(strncasecmp(addr+alen-13, ".in-addr", 7)==0)
			{
				toret->proto = CIDR_IPV4;
				buf = addr+alen-13;
			}
			else
			{
				/* Unknown */
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}
		}
		/*
		 * buf now points to the period after the last (first) bit of
		 * address numbering in the PTR name.
		 */

		/*
		 * Now convert based on that protocol.  Note that we're going to
		 * be slightly asymmetrical to the way cidr_to_str() works, in
		 * how we handle the netmask.  cidr_to_str() ignores it, and
		 * treats the PTR-style output solely as host addresses.  We'll
		 * use the netmask bits to specify how much of the address is
		 * given in the PTR we get.  That is, if we get
		 * "3.2.1.in-addr.arpa", we'll set a /24 netmask on the returned
		 * result.  This way, the calling program can tell the difference
		 * between "3.2.1..." and "0.3.2.1..." if it really cares to.
		 */
		buf--; /* Step before the period */
		if(toret->proto == CIDR_IPV4)
		{
			for(i=11 ; i<=14 ; /* */)
			{
				/* If we're before the beginning, we're done */
				if(buf<addr)
					break;

				/* Step backward until we at the start of an octet */
				while(isdigit(*buf) && buf>=addr)
					buf--;

				/*
				 * Save that number (++i here to show that this octet is
				 * now set.
				 */
				octet = strtoul(buf+1, NULL, 10);
				if(octet > (unsigned long)0xff)
				{
					/* Bad octet!  No biscuit! */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}
				toret->addr[++i] = octet;


				/*
				 * Back up a step to get before the '.', and process the
				 * next [previous] octet.  If we were at the beginning of
				 * the string already, the test at the top of the loop
				 * will drop us out.
				 */
				buf--;
			}

			/* Too much? */
			if(buf>=addr)
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/*
			 * Now, what about the mask?  We set the netmask bits to
			 * describe how much information we've actually gotten, if we
			 * didn't get all 4 octets.  Because of the way .in-addr.arpa
			 * works, the mask can only fall on an octet boundary, so we
			 * don't need too many fancy tricks.  'i' is still set from
			 * the above loop to whatever the last octet we filled in is,
			 * so we don't even have to special case anything.
			 */
			for(j=0 ; j<=i ; j++)
				toret->mask[j] = 0xff;

			/* Done processing */
		}
		else if(toret->proto == CIDR_IPV6)
		{
			/*
			 * This processing happens somewhat similarly to IPV4 above,
			 * the format is simplier, and we need to be a little
			 * sneakier about the mask, since it can fall on a half-octet
			 * boundary with .ip6.arpa format.
			 */
			for(i=0 ; i<=15 ; i++)
			{
				/* If we're before the beginning, we're done */
				if(buf<addr)
					break;

				/* We better point at a number */
				if(!isxdigit(*buf))
				{
					/* Bad input */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}

				/* Save the current number */
				octet = strtoul(buf, NULL, 16);
				if(octet > (unsigned long)0xff)
				{
					/* Bad octet!  No biscuit! */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}
				toret->addr[i] = octet << 4;
				toret->mask[i] = 0xf0;

				/* If we're at the beginning of the string, we're thru */
				if(buf==addr)
				{
					/* Shift back to skip error condition at end of loop */
					buf--;
					break;
				}

				/* If we're not, stepping back should give us a period */
				if(*--buf != '.')
				{
					/* Bad input */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}

				/* Stepping back again should give us a number */
				if(!isxdigit(*--buf))
				{
					/* Bad input */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}

				/* Save that one */
				octet = strtoul(buf, NULL, 16);
				if(octet > (unsigned long)0xff)
				{
					/* Bad octet!  No biscuit! */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}
				toret->addr[i] |= octet & 0x0f;
				toret->mask[i] |= 0x0f;


				/*
				 * Step back and loop back around.  If that last step
				 * back moves us to before the beginning of the string,
				 * the condition at the top of the loop will drop us out.
				 */
				while(*--buf=='.' && buf>=addr)
					/* nothing */;
			}

			/* Too much? */
			if(buf>=addr)
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/* Mask is set in the loop for v6 */
		}
		else
		{
			/* Shouldn't happen */
			cidr_free(toret);
			errno = ENOENT; /* Bad choice of errno */
			return(NULL);
		}

		/* Return the value we built up, and we're done! */
		return(toret);

		/* NOTREACHED */
	}
	buf=NULL; /* Done */


	/*
	 * It's not a PTR form, so find the '/' prefix marker if we can.  We
	 * support both prefix length and netmasks after the /, so flag if we
	 * find a mask.
	 */
	foundpf=foundmask=0;
	for(i=alen-1 ; i>=0 ; i--)
	{
		/* Handle both possible forms of netmasks */
		if(addr[i]=='.' || addr[i]==':')
			foundmask=1;

		/* Are we at the beginning of the prefix? */
		if(addr[i]=='/')
		{
			foundpf=1;
			break;
		}
	}

	if(foundpf==0)
	{
		/* We didn't actually find a prefix, so reset the foundmask */
		foundmask=0;

		/*
		 * pfx is only used if foundpf==1, but set it to NULL here to
		 * quiet gcc down.
		 */
		pfx=NULL;
	}
	else
	{
		/* Remember where the prefix is */
		pfx = addr+i;

		if(foundmask==0)
		{
			/*
			 * If we didn't find a netmask, it may be that it's one of
			 * the v4 forms without dots.  Technically, it COULD be
			 * expressed as a single (32-bit) number that happens to be
			 * between 0 and 32 inclusive, so there's no way to be
			 * ABSOLUTELY sure when we have a prefix length and not a
			 * netmask.  But, that would be a non-contiguous netmask,
			 * which we don't attempt to support, so we can probably
			 * safely ignore that case.  So try a few things...
			 */
			/* If it's a hex or octal number, assume it's a mask */
			if(pfx[1]=='0' && tolower(pfx[2])=='x')
				foundmask=1; /* Hex */
			else if(pfx[1]=='0')
				foundmask=1; /* Oct */
			else if(isdigit(pfx[1]))
			{
				/*
				 * If we get here, it looks like a decimal number, and we
				 * know there aren't any periods or colons in it, so if
				 * it's valid, it can ONLY be a single 32-bit decimal
				 * spanning the whole 4-byte v4 address range.  If that's
				 * true, it's GOTTA be a valid number, it's GOTTA reach
				 * to the end of the strong, and it's GOTTA be at least
				 * 2**31 and less than 2**32.
				 */
				octet = strtoul(pfx+1, &buf2, 10);
				if(*buf2=='\0' && octet >= (unsigned long)(1<<31)
						&& octet <= (unsigned long)0xffffffff)
					foundmask=1; /* Valid! */

				octet=0; buf2=NULL; /* Done */
			}
		}
	}
	i=0; /* Done */


	/*
	 * Now, let's figure out what kind of address this is.  A v6 address
	 * will contain a : within the first 5 characters ('0000:'), a v4
	 * address will have a . within the first 4 ('123.'), UNLESS it's
	 * just a single number (in hex, octal, or decimal).  Anything else
	 * isn't an address we know anything about, so fail.
	 */
	if((buf = strchr(addr, ':'))!=NULL && (buf-addr)<=5)
		toret->proto = CIDR_IPV6;
	else if((buf = strchr(addr, '.'))!=NULL && (buf-addr)<=4)
		toret->proto = CIDR_IPV4;
	else
	{
		/*
		 * Special v4 forms
		 */
		if(*addr=='0' && tolower(*(addr+1))=='x')
		{
			/* Hex? */
			buf = (addr+2) + strspn(addr+2, "0123456789abcdefABCDEF");
			if(*buf=='\0' || *buf=='/')
				toret->proto = CIDR_IPV4; /* Yep */
		}
		else if(*addr=='0')
		{
			/* Oct? */
			/* (note: this also catches the [decimal] address '0' */
			buf = (addr+1) + strspn(addr+1, "01234567");
			if(*buf=='\0' || *buf=='/')
				toret->proto = CIDR_IPV4; /* Yep */
		}
		else
		{
			/* Dec? */
			buf = (addr) + strspn(addr, "0123456789");
			if(*buf=='\0' || *buf=='/')
				toret->proto = CIDR_IPV4; /* Yep */
		}

		/* Did we catch anything? */
		if(toret->proto == 0)
		{
			/* Unknown */
			cidr_free(toret);
			errno = EINVAL;
			return(NULL);
		}
	}
	buf=NULL; /* Done */


	/*
	 * So now we know what sort of address it is, we can go ahead and
	 * have a parser for either.
	 */
	if(toret->proto==CIDR_IPV4)
	{
		/*
		 * Parse a v4 address.  Now, we're being a little tricksy here,
		 * and parsing it from the end instead of from the front.
		 */

		/*
		 * First, find out how many bits we have.  We need to have 4 or
		 * less...
		 */
		buf = strchr(addr, '.');
		/* Through here, nsect counts dots */
		for(nsect=0 ; buf!=NULL && (pfx!=NULL?buf<pfx:1) ; buf=strchr(buf, '.'))
		{
			nsect++; /* One more section */
			buf++; /* Move past . */
			if(nsect>3)
			{
				/* Bad!  We can't have more than 4 sections... */
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}
		}
		buf=NULL; /* Done */
		nsect++; /* sects = dots+1 */

		/*
		 * First, initialize this so we can skip building the bits if we
		 * don't have to.
		 */
		pflen=-1;

		/*
		 * Initialize the first 12 octets of the address/mask to look
		 * like a v6-mapped address.  This is the correct info for those
		 * octets to have if/when we decide to use this v4 address as a
		 * v6 one.
		 */
		for(i=0 ; i<=9 ; i++)
			toret->addr[i] = 0;
		for(i=10 ; i<=11 ; i++)
			toret->addr[i] = 0xff;
		for(i=0 ; i<=11 ; i++)
			toret->mask[i] = 0xff;

		/*
		 * Handle the prefix/netmask.  If it's not set at all, slam it to
		 * the maximum, and put us at the end of the string to start out.
		 * Ditto if the '/' is the end of the string.
		 */
		if(foundpf==0)
		{
			pflen=32;
			i=alen-1;
		}
		else if(foundpf==1 && *(pfx+1)=='\0')
		{
			pflen=32;
			i=(int)(pfx-addr-1);
		}

		/*
		 * Or, if we found it, and it's a NETMASK, we need to parse it
		 * just like an address.  So, cheat a little and call ourself
		 * recursively, and then just count the bits in our returned
		 * address for the pflen.
		 */
		if(foundpf==1 && foundmask==1 && pflen==-1)
		{
			ctmp = cidr_from_str(pfx+1);
			if(ctmp==NULL)
			{
				/* This shouldn't happen */
				cidr_free(toret);
				return(NULL); /* Preserve errno */
			}
			/* Stick it in the mask */
			for(i=0 ; i<=11 ; i++)
				ctmp->mask[i] = 0;
			for(i=12 ; i<=15 ; i++)
				ctmp->mask[i] = ctmp->addr[i];

			/* Get our prefix length */
			pflen = cidr_get_pflen(ctmp);
			cidr_free(ctmp);
			if(pflen==-1)
			{
				/* Failed; probably non-contiguous */
				cidr_free(toret);
				return(NULL); /* Preserve errno */
			}

			/* And set us to before the '/' like below */
			i = (int)(pfx-addr-1);
		}

		/*
		 * Finally, if we did find it and it's a normal prefix length,
		 * just pull it it, parse it out, and set ourselves to the first
		 * character before the / for the address reading
		 */
		if(foundpf==1 && foundmask==0 && pflen==-1)
		{
			pflen = (int)strtol(pfx+1, NULL, 10);
			i = (int)(pfx-addr-1);
		}


		/*
		 * If pflen is set, we need to turn it into a mask for the bits.
		 * XXX pflen actually should ALWAYS be set, so we might not need
		 * to make this conditional at all...
		 */
		if(pflen>0)
		{
			/* 0 < pflen <= 32 */
			if(pflen<0 || pflen>32)
			{
				/* Always bad */
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/*
			 * Now pflen is in the 0...32 range and thus good.  Set it in
			 * the structure.  Note that memset zero'd the whole thing to
			 * start.  We ignore mask[<12] with v4 addresses normally,
			 * but they're already set to all-1 anyway, since if we ever
			 * DO care about them, that's the most appropriate thing for
			 * them to be.
			 *
			 * This is a horribly grody set of macros.  I'm only using
			 * them here to test them out before using them in the v6
			 * section, where I'll need them more due to the sheer number
			 * of clauses I'll have to get written.  Here's the straight
			 * code I had written that the macro should be writing for me
			 * now:
			 *
			 * if(pflen>24)
			 *   for(j=24 ; j<pflen ; j++)
			 *     toret->mask[15] |= 1<<(31-j);
			 * if(pflen>16)
			 *   for(j=16 ; j<pflen ; j++)
			 *     toret->mask[14] |= 1<<(23-j);
			 * if(pflen>8)
			 *   for(j=8 ; j<pflen ; j++)
			 *     toret->mask[13] |= 1<<(15-j);
			 * if(pflen>0)
			 *   for(j=0 ; j<pflen ; j++)
			 *     toret->mask[12] |= 1<<(7-j);
			 */
#define UMIN(x,y) ((x)<(y)?(x):(y))
#define MASKNUM(x) (24-((15-x)*8))
#define WRMASKSET(x) \
		if(pflen>MASKNUM(x)) \
			for(j=MASKNUM(x) ; j<UMIN(pflen,MASKNUM(x)+8) ; j++) \
				toret->mask[x] |= 1<<(MASKNUM(x)+7-j);

			WRMASKSET(15);
			WRMASKSET(14);
			WRMASKSET(13);
			WRMASKSET(12);

#undef WRMASKET
#undef MASKNUM
#undef UMIN
		} /* Normal v4 prefix */


		/*
		 * Now we have 4 octets to grab.  If any of 'em fail, or are
		 * outside the 0...255 range, bomb.
		 */
		nocts = 0;

		/* Here, i should be before the /, but we may have multiple */
		while(i>0 && addr[i]=='/')
			i--;

		for( /* i */ ; i>=0 ; i--)
		{
			/*
			 * As long as it's still a number or an 'x' (as in '0x'),
			 * keep backing up.  Could be hex, so don't just use
			 * isdigit().
			 */
			if((isxdigit(addr[i]) || tolower(addr[i])=='x') && i>0)
				continue;

			/*
			 * It's no longer a number.  So, grab the number we just
			 * moved before.
			 */
			/* Cheat for "beginning-of-string" rather than "NaN" */
			if(i==0)
				i--;
			/* Theoretically, this can be in hex/oct/dec... */
			if(addr[i+1]=='0' && tolower(addr[i+2])=='x')
				octet = strtoul(addr+i+1, &buf2, 16);
			else if(addr[i+1] == '0')
				octet = strtoul(addr+i+1, &buf2, 8);
			else
				octet = strtoul(addr+i+1, &buf2, 10);

			/* If buf isn't pointing at one of [./'\0'], it's screwed */
			if(!(*buf2=='.' || *buf2=='/' || *buf2=='\0'))
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}
			buf2=NULL; /* Done */

			/*
			 * Now, because of the way compressed IPv4 addresses work,
			 * this number CAN be greater than 255, IF it's the last bit
			 * in the address (the first bit we parse), in which case it
			 * must be no bigger than needed to fill the unaccounted-for
			 * 'slots' in the address.
			 *
			 * See
			 * <http://www.opengroup.org/onlinepubs/007908799/xns/inet_addr.html>
			 * for details.
			 */
			if( (nocts!=0 && octet>255)
			    || (nocts==0 && octet>(0xffffffff >> (8*(nsect-1)))) )
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/* Save the lower 8 bits into this octet */
			toret->addr[15-nocts++] = octet & 0xff;

			/*
			 * If this is the 'last' piece of the address (the first we
			 * process), and there are fewer than 4 pieces total, we need
			 * to extend it out into additional fields.  See above
			 * reference.
			 */
			if(nocts==1)
			{
				if(nsect<=3)
					toret->addr[15-nocts++] = (octet >> 8) & 0xff;
				if(nsect<=2)
					toret->addr[15-nocts++] = (octet >> 16) & 0xff;
				if(nsect==1)
					toret->addr[15-nocts++] = (octet >> 24) & 0xff;
			}

			/*
			 * If we've got 4 of 'em, we're actually done.  We got the
			 * prefix above, so just return direct from here.
			 */
			if(nocts==4)
				return(toret);
		}

		/*
		 * If we get here, it failed to get all 4.  That shouldn't
		 * happen, since we catch proper abbreviated forms above.
		 */
		cidr_free(toret);
		errno = EINVAL;
		return(NULL);
	}
	else if(toret->proto==CIDR_IPV6)
	{
		/*
		 * Parse a v6 address.  Like the v4, we start from the end and
		 * parse backward.  However, to handle compressed form, if we hit
		 * a ::, we drop off and start parsing from the beginning,
		 * because at the end we'll then have a hole that is what the ::
		 * is supposed to contain, which is already automagically 0 from
		 * the memset() we did earlier.  Neat!
		 *
		 * Initialize the prefix length
		 */
		pflen=-1;

		/* If no prefix was found, assume the max */
		if(foundpf==0)
		{
			pflen = 128;
			/* Stretch back to the end of the string */
			i=alen-1;
		}
		else if(foundpf==1 && *(pfx+1)=='\0')
		{
			pflen = 128;
			i=(int)(pfx-addr-1);
		}

		/*
		 * If we got a netmask, rather than a prefix length, parse it and
		 * count the bits, like we did for v4.
		 */
		if(foundpf==1 && foundmask==1 && pflen==-1)
		{
			ctmp = cidr_from_str(pfx+1);
			if(ctmp==NULL)
			{
				/* This shouldn't happen */
				cidr_free(toret);
				return(NULL); /* Preserve errno */
			}
			/* Stick it in the mask */
			for(i=0 ; i<=15 ; i++)
				ctmp->mask[i] = ctmp->addr[i];

			/* Get the prefix length */
			pflen = cidr_get_pflen(ctmp);
			cidr_free(ctmp);
			if(pflen==-1)
			{
				/* Failed; probably non-contiguous */
				cidr_free(toret);
				return(NULL); /* Preserve errno */
			}

			/* And set us to before the '/' like below */
			i = (int)(pfx-addr-1);
		}

		/* Finally, the normal prefix case */
		if(foundpf==1 && foundmask==0 && pflen==-1)
		{
			pflen = (int)strtol(pfx+1, NULL, 10);
			i = (int)(pfx-addr-1);
		}


		/*
		 * Now, if we have a pflen, turn it into a mask.
		 * XXX pflen actually should ALWAYS be set, so we might not need
		 * to make this conditional at all...
		 */
		if(pflen>0)
		{
			/* Better be 0...128 */
			if(pflen<0 || pflen>128)
			{
				/* Always bad */
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/*
			 * Now save the pflen.  See comments on the similar code up in
			 * the v4 section about the macros.
			 */
#define UMIN(x,y) ((x)<(y)?(x):(y))
#define MASKNUM(x) (120-((15-x)*8))
#define WRMASKSET(x) \
		if(pflen>MASKNUM(x)) \
			for(j=MASKNUM(x) ; j<UMIN(pflen,MASKNUM(x)+8) ; j++) \
				toret->mask[x] |= 1<<(MASKNUM(x)+7-j);

			WRMASKSET(15);
			WRMASKSET(14);
			WRMASKSET(13);
			WRMASKSET(12);
			WRMASKSET(11);
			WRMASKSET(10);
			WRMASKSET(9);
			WRMASKSET(8);
			WRMASKSET(7);
			WRMASKSET(6);
			WRMASKSET(5);
			WRMASKSET(4);
			WRMASKSET(3);
			WRMASKSET(2);
			WRMASKSET(1);
			WRMASKSET(0);

#undef WRMASKET
#undef MASKNUM
#undef UMIN
		}


		/*
		 * Now we have 16 octets to grab.  If any of 'em fail, or are
		 * outside the 0...0xff range, bomb.  However, we MAY have a
		 * v4-ish form, whether it's a formal v4 mapped/compat address,
		 * or just a v4 address written in a v6 block.  So, look for
		 * .-separated octets, but there better be exactly 4 of them
		 * before we hit a :.
		 */
		nocts = 0;

		/* Bump before / (or multiple /'s */
		while(i>0 && addr[i]=='/')
			i--;

		for( /* i */ ; i>=0 ; i--)
		{
			/*
			 * First, check the . cases, and handle them all in one
			 * place.  These can only happen at the beginning, when we
			 * have no octets yet, and if it happens at all, we need to
			 * have 4 of them.
			 */
			if(nocts==0 && addr[i]=='.')
			{
				i++; /* Shift back to after the '.' */

				for( /* i */ ; i>0 && nocts<4 ; i--)
				{
					/* This shouldn't happen except at the end */
					if(addr[i]==':' && nocts<3)
					{
						cidr_free(toret);
						errno = EINVAL;
						return(NULL);
					}

					/* If it's not a . or :, move back 1 */
					if(addr[i]!='.' && addr[i]!=':')
						continue;

					/* Should be a [decimal] octet right after here */
					octet = strtoul(addr+i+1, NULL, 10);
					/* Be sure */
					if(octet>255)
					{
						cidr_free(toret);
						errno = EINVAL;
						return(NULL);
					}

					/* Save it */
					toret->addr[15-nocts] = octet & 0xff;
					nocts++;

					/* And find the next octet */
				}

				/*
				 * At this point, 4 dotted-decimal octets should be
				 * consumed.  i has gone back one step past the : before
				 * the decimal, so addr[i+1] should be the ':' that
				 * preceeds them.  Verify.
				 */
				if(nocts!=4 || addr[i+1]!=':')
				{
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}
			}

			/*
			 * Now we've either gotten 4 octets filled in from
			 * dotted-decimal stuff, or we've filled in nothing and have
			 * no dotted decimal.
			 */


			/* As long as it's not our separator, keep moving */
			if(addr[i]!=':' && i>0)
				continue;

			/* If it's a :, and our NEXT char is a : too, flee */
			if(addr[i]==':' && addr[i+1]==':')
			{
				/*
				 * If i is 0, we're already at the beginning of the
				 * string, so we can just return; we've already filled in
				 * everything but the leading 0's, which are already
				 * zero-filled from the memory
				 */
				if(i==0)
					return(toret);

				/* Else, i!=0, and we break out */
				break;
			}

			/* If it's not a number either...   well, bad data */
			if(!isxdigit(addr[i]) && addr[i]!=':' && i>0)
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/*
			 * It's no longer a number.  So, grab the number we just
			 * moved before.
			 */
			/* Cheat for "beginning-of-string" rather than "NaN" */
			if(i==0)
				i--;
			octet = strtoul(addr+i+1, &buf2, 16);
			if(*buf2!=':' && *buf2!='/' && *buf2!='\0')
			{
				/* Got something unexpected */
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}
			buf2=NULL;

			/* Remember, this is TWO octets */
			if(octet>0xffff)
			{
				cidr_free(toret);
				errno = EINVAL;
				return(NULL);
			}

			/* Save it */
			toret->addr[15-nocts] = octet & 0xff;
			nocts++;
			toret->addr[15-nocts] = (octet>>8) & 0xff;
			nocts++;

			/* If we've got all of 'em, just return from here. */
			if(nocts==16)
				return(toret);
		}

		/*
		 * Now, if i is >=0 and we've got two :'s, jump around to the
		 * front of the string and start parsing inward.
		 */
		if(i>=0 && addr[i]==':' && addr[i+1]==':')
		{
			/* Remember how many octets we put on the end */
			eocts = nocts;

			/* Remember how far we were into the string */
			j=i;

			/* Going this way, we do things a little differently */
			i=0;
			while(i<j)
			{
				/*
				 * The first char better be a number.  If it's not, bail
				 * (a leading '::' was already handled in the loop above
				 * by just returning).
				 */
				if(i==0 && !isxdigit(addr[i]))
				{
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}

				/*
				 * We should be pointing at the beginning of a digit
				 * string now.  Translate it into an octet.
				 */
				octet = strtoul(addr+i, &buf2, 16);
				if(*buf2!=':' && *buf2!='/' && *buf2!='\0')
				{
					/* Got something unexpected */
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}
				buf2=NULL;

				/* Sanity (again, 2 octets) */
				if(octet>0xffff)
				{
					cidr_free(toret);
					errno = EINVAL;
					return(NULL);
				}

				/* Save it */
				toret->addr[nocts-eocts] = (octet>>8) & 0xff;
				nocts++;
				toret->addr[nocts-eocts] = octet & 0xff;
				nocts++;

				/*
				 * Discussion: If we're in this code block, it's because
				 * we hit a ::-compression while parsing from the end
				 * backward.  So, if we hit 15 octets here, it's an
				 * error, because with the at-least-2 that were minimized,
				 * that makes 17 total, which is too many.  So, error
				 * out.
				 */
				if(nocts==15)
				{
					cidr_free(toret);
					return(NULL);
				}

				/* Now skip around to the end of this number */
				while(isxdigit(addr[i]) && i<j)
					i++;

				/*
				 * If i==j, we're back where we started.  So we've filled
				 * in all the leading stuff, and the struct is ready to
				 * return.
				 */
				if(i==j)
					return(toret);

				/*
				 * Else, there's more to come.  We better be pointing at
				 * a ':', else die.
				 */
				if(addr[i]!=':')
				{
					cidr_free(toret);
					return(NULL);
				}

				/* Skip past : */
				i++;

				/* If we're at j now, we had a ':::', which is invalid */
				if(i==j)
				{
					cidr_free(toret);
					return(NULL);
				}

				/* Head back around */
			}
		}

		/* If we get here, it failed somewhere odd */
		cidr_free(toret);
		errno = EINVAL;
		return(NULL);
	}
	else
	{
		/* Shouldn't happen */
		cidr_free(toret);
		errno = ENOENT; /* Bad choice of errno */
		return(NULL);
	}


	/* NOTREACHED */
	errno = ENOENT;
	return(NULL);
}
