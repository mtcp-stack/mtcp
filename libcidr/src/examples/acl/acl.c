/*
 * Demonstrate doing ACL's with libcidr
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcidr.h>


/* Max size of ACL file */
#define AFILESZ (1024*1024) /* 1 meg */
/* Max ACL rules */
#define NRULES 128


/* Globals/prototypes */
void usage(void);
static int _client_isallowed(const CIDR *, int );

char *pname;
short verbose;
struct aclrules
{
	short   allowed;
	CIDR    *cidr;
} acls[NRULES+1];


int
main(int argc, char *argv[])
{
	char *fname;
	char *aclfile;
	char *buf, *buf2;
	int afd;
	struct stat ast;
	short port;
	int goch;
	int i;
	struct sockaddr_in srv4, cl4;
	struct sockaddr_in6 srv6, cl6;
	socklen_t cllen;
	CIDR *clcidr;
	char *clstr;
	char toclnt[2048]; /* Hardcoded, but who cares */
	int sp4, sp6, clsock;
	fd_set rfd;
	int maxdesc;
	int sckopt;

	/* Initialize */
	pname = *argv;
	fname = NULL;
	port = 0;
	verbose = 0;
	sckopt = 1;
	for(i=0 ; i<=NRULES ; i++)
		acls[i].allowed=-1;

	/* Grab our args */
	while((goch=getopt(argc, argv, "f:p:v"))!=-1)
	{
		switch((char)goch)
		{
			case 'f':
				fname = strdup(optarg);
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'v':
				verbose++;
				break;
			default:
				printf("Unknown argument: '%c'\n", goch);
				usage();
				/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;


	/* These conditions may change down the road */
	if(port==0)
	{
		printf("Error: Port must be set.\n");
		usage();
	}

	if(fname==NULL)
	{
		printf("Error: ACL file must be set.\n");
		usage();
	}


	/*
	 * First, parse out the ACL file.
	 */
	afd = open(fname, O_RDONLY);
	if(afd==-1)
	{
		printf("Error: Can't open file %s: %s\n", fname, strerror(errno));
		usage();
	}
	if(fstat(afd, &ast)==-1)
	{
		printf("Error: Can't stat file %s: %s\n", fname, strerror(errno));
		usage();
	}
	if(ast.st_size >= AFILESZ) /* >= to handle \0 */
	{
		printf("Error: File %s too large; max %dkb\n", fname, AFILESZ/1024);
		exit(1);
	}

	/* Read it all in at once, 'cuz I'm lazy */
	aclfile = malloc(ast.st_size+1);
	if(aclfile==NULL)
	{
		printf("Error: malloc() failed\n");
		exit(1);
	}
	memset(aclfile, 0, ast.st_size+1);
	if(read(afd, aclfile, ast.st_size)!=ast.st_size)
	{
		printf("Error: Failed to read enough bytes.\n");
		free(aclfile);
		exit(1);
	}
	close(afd);

	/*
	 * Now parse it line by line.
	 * Format: Each line should contain first a + or - sign, followed by
	 * whitespace, followed by a CIDR block, then a newline.  That's all.
	 */
	buf = aclfile;
	i=0;
	while(1)
	{
		/* Have we overflowed? */
		if(i>= NRULES)
		{
			printf("Error: Too many rules found.\n");
			free(aclfile);
			exit(1);
		}

		/* Are we through? */
		if(*buf=='\0' || *(buf+1)=='\0')
			break;

		/* Find the newline and null it out temporarily */
		buf2 = strchr(buf, '\n');
		if(buf2!=NULL)
			*buf2 = '\0';

		/* Allowed or not? */
		if(*buf=='+')
			acls[i].allowed=1;
		else if(*buf=='-')
			acls[i].allowed=0;
		else
		{
			printf("Warning: Couldn't parse line '%s'\n", buf);
			continue;
		}

		/* Skip to the CIDR */
		buf++;
		while(isspace(*buf))
			buf++;

		/* Try parsing it */
		if((acls[i].cidr=cidr_from_str(buf))==NULL)
		{
			printf("Warning: Couldn't parse line '%s'\n", buf);
			acls[i].allowed=-1; /* "unset" */
			continue;
		}


		/* Hope to the next line if it exists, else exit */
		if(buf2==NULL)
			break;

		buf = buf2+1;
		i++;
	}
	/* Done parsing */
	free(aclfile);


	/*
	 * Now start listening for connections on the given port.  Open up
	 * both v4 and v6 ports.
	 */
	memset(&srv4, 0, sizeof(srv4));
	srv4.sin_family = AF_INET;
	srv4.sin_addr.s_addr = INADDR_ANY;
	srv4.sin_port = htons(port);
	if((sp4=socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("Error: Couldn't create v4 socket: %s\n", strerror(errno));
		exit(1);
	}
	setsockopt(sp4, SOL_SOCKET, SO_REUSEADDR, &sckopt, sizeof(sckopt));
#if defined(SO_REUSEPORT) /* May not exist */
	setsockopt(sp4, SOL_SOCKET, SO_REUSEPORT, &sckopt, sizeof(sckopt));
#endif
	if(bind(sp4, (struct sockaddr *) &srv4, sizeof(srv4)) == -1)
	{
		printf("Error: Couldn't bind v4 socket: %s\n", strerror(errno));
		exit(1);
	}
	listen(sp4, 10);

	memset(&srv6, 0, sizeof(srv6));
	srv6.sin6_family = AF_INET6;
	memcpy(&(srv6.sin6_addr.s6_addr), &in6addr_any, sizeof(in6addr_any));
	srv6.sin6_port = htons(port);
	if((sp6=socket(AF_INET6, SOCK_STREAM, 0)) == -1)
	{
		printf("Error: Couldn't create v6 socket: %s\n", strerror(errno));
		exit(1);
	}
	setsockopt(sp6, SOL_SOCKET, SO_REUSEADDR, &sckopt, sizeof(sckopt));
#if defined(SO_REUSEPORT) /* May not exist */
	setsockopt(sp6, SOL_SOCKET, SO_REUSEPORT, &sckopt, sizeof(sckopt));
#endif
	if(bind(sp6, (struct sockaddr *) &srv6, sizeof(srv6)) == -1)
	{
		printf("Error: Couldn't bind v6 socket: %s\n", strerror(errno));
		exit(1);
	}
	listen(sp6, 10);


	/* Now go into our main loop and wait for connections */
	while(1)
	{
		FD_ZERO(&rfd);
		FD_SET(sp4, &rfd);
		FD_SET(sp6, &rfd);
		if(sp6>sp4)
			maxdesc = sp6;
		else
			maxdesc = sp4;
		if(select(maxdesc+1, &rfd, NULL, NULL, NULL)<0)
		{
			printf("Error: select() failed: %s\n", strerror(errno));
			exit(1);
		}


		/* Handle pending v4 connection */
		if(FD_ISSET(sp4, &rfd))
		{
			cllen = sizeof(cl4);
			clsock = accept(sp4, (struct sockaddr *) &cl4, &cllen);
			if(clsock==-1)
			{
				if(errno==ECONNABORTED) /* Not really an error */
					goto after4;

				printf("Error: accept(sp4) failed: %s\n", strerror(errno));
				exit(1);
			}

			/* Extract their address */
			clcidr = cidr_from_inaddr(&cl4.sin_addr);

			/* Greet them, and mention locally */
			clstr = cidr_to_str(clcidr, CIDR_ONLYADDR);
			printf("**> New v4 client from %s\n", clstr);
			sprintf(toclnt, "Hi there, %s!\n", clstr);
			write(clsock, toclnt, strlen(toclnt));
			free(clstr);

			/* Now check them in the ACL */
			if(_client_isallowed(clcidr, clsock)==0)
			{
				printf("    Access PERMITTED.\n");
				sprintf(toclnt, "Your access is ACCEPTED!\nYou rock!\n\n");
				write(clsock, toclnt, strlen(toclnt));
			}
			else
			{
				printf("    Access DENIED.\n");
				sprintf(toclnt, "Your access is DENIED!\nYou suck!\n\n");
				write(clsock, toclnt, strlen(toclnt));
			}

			/* That's all.  Close 'em off and move on. */
			close(clsock);
			cidr_free(clcidr);
		}
after4:


		/* Handle pending v6 connection (big copy/paste) */
		if(FD_ISSET(sp6, &rfd))
		{
			cllen = sizeof(cl6);
			clsock = accept(sp6, (struct sockaddr *) &cl6, &cllen);
			if(clsock==-1)
			{
				if(errno==ECONNABORTED) /* Not really an error */
					goto after6;

				printf("Error: accept(sp6) failed: %s\n", strerror(errno));
				exit(1);
			}

			/* Extract their address */
			clcidr = cidr_from_in6addr(&cl6.sin6_addr);

			/* Greet them, and mention locally */
			clstr = cidr_to_str(clcidr, CIDR_ONLYADDR);
			printf("**> New v6 client from %s\n", clstr);
			sprintf(toclnt, "Hi there, %s!\n", clstr);
			write(clsock, toclnt, strlen(toclnt));
			free(clstr);

			/* Now check them in the ACL */
			if(_client_isallowed(clcidr, clsock)==0)
			{
				printf("    Access PERMITTED.\n");
				sprintf(toclnt, "Your access is ACCEPTED!\nYou rock!\n\n");
				write(clsock, toclnt, strlen(toclnt));
			}
			else
			{
				printf("    Access DENIED.\n");
				sprintf(toclnt, "Your access is DENIED!\nYou suck!\n\n");
				write(clsock, toclnt, strlen(toclnt));
			}

			/* That's all.  Close 'em off and move on. */
			close(clsock);
			cidr_free(clcidr);
		}
after6:

		/* Loop back around, forever */
		; /* Shut gcc up about the label at the end of the loop */
	}

	/* NOTREACHED */
	exit(1);
}


/* Usage */
void
usage(void)
{
	printf("Usage: %s [-v] [-f acl-file] [-p port]\n"
	       "       -f  File containing the ACL list\n"
	       "       -p  TCP port to listen on\n"
	       "       -v  Be more verbose\n"
	       "           Specified once, it shows the ACL's it checks locally\n"
	       "           Specified twice, it sends them across the socket too\n"
	       "\n", pname);
	exit(1);
}


/* Check the client against the ACL */
static int _client_isallowed(const CIDR *clcidr, int clsock)
{
	int i;
	char tmpbuf[1024]; /* Hardcoded */
	char *buf;

	/*
	 * Start checking.  Note that the ending i<NRULES condition should
	 * never be met, since we should bump out at some point.  We treat
	 * the ruleset as default deny.
	 */
	for(i=0 ; i<NRULES ; i++)
	{
		if(acls[i].allowed==-1)
		{
			if(verbose>0)
			{
				sprintf(tmpbuf, "\tDefault deny.\n");
				printf("%s", tmpbuf);
				if(verbose>1)
					write(clsock, tmpbuf, strlen(tmpbuf));
			}
			return(-1); /* Out of options, default deny */
		}

		/* Now compare */
		if(verbose>0)
		{
			buf = cidr_to_str(acls[i].cidr, CIDR_NOFLAGS);
			sprintf(tmpbuf, "\tChecking '%s'...  ", buf);
			free(buf);
		}
		if(cidr_contains(acls[i].cidr, clcidr)==0)
		{
			/* This rule matches */
			if(verbose>0)
			{
				strcat(tmpbuf, "matched!\n");
				printf("%s", tmpbuf);
				if(verbose>1)
					write(clsock, tmpbuf, strlen(tmpbuf));
			}

			/* See which it returns */
			if(acls[i].allowed==1)
				return(0); /* Accepted! */
			else
				return(-1); /* Denied! */
		}

		/* Didn't match */
		if(verbose>0)
		{
			strcat(tmpbuf, "not matched!\n");
			printf("%s", tmpbuf);
			if(verbose>1)
				write(clsock, tmpbuf, strlen(tmpbuf));
		}
	}

	/* XXX Should never get here */
	printf("*** Internal error: NOTREACHED\n");
	return(-1);
}
