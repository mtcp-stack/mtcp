#define _GNU_SOURCE             
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <limits.h>

#include "netlib.h"

/*----------------------------------------------------------------------------*/
int 
GetNumCPUCores(void)
{
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
int 
AffinitizeThreadToCore(int core)
{
	cpu_set_t *cmask;
	int n, ret;
				    
    n = sysconf(_SC_NPROCESSORS_ONLN);

	if (core < 0 || core >= n) {
		fprintf(stderr, "%d: invalid CPU number.\n", core);
		return -1;
	}   

	cmask = CPU_ALLOC(n);
	if (cmask == NULL) {
		fprintf(stderr, "%d: uexpected cmask.\n", n);
		return -1;
	}

	CPU_ZERO_S(n, cmask);
	CPU_SET_S(core, n, cmask);

	ret = sched_setaffinity(0, n, cmask);

	CPU_FREE(cmask);
	return ret;
}
/*----------------------------------------------------------------------------*/
int 
CreateServerSocket(int port, int isNonBlocking)
{
	int s;
	struct sockaddr_in addr;
	struct linger doLinger;
	int doReuse = 1;

	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		fprintf(stderr, "socket() failed, errno=%d msg=%s\n",
				errno, strerror(errno));
		return(-1);
	}

	/* don't linger on close */
	doLinger.l_onoff = doLinger.l_linger = 0;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, 
				   &doLinger, sizeof(doLinger)) == -1) {
		close(s);
		return(-1);
	}

	/* reuse addresses */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, 
				   &doReuse, sizeof(doReuse)) == -1) {
		close(s);
		return(-1);
	}

	/* make the listening socket nonblocking */
	if (isNonBlocking) {
		if (fcntl(s, F_SETFL, O_NDELAY) < 0) {
			fprintf(stderr, "fcntl() failed, errno=%d msg=%s\n",
					errno, strerror(errno));
			close(s);
			return(-1);
		}
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind() failed, errno=%d msg=%s\n", 
				errno, strerror(errno));
		close(s);
		return(-1);
	}

	if (listen(s, 1024) < 0) {
		close(s);
		return(-1);
	}

	return(s);
}
/*-------------------------------------------------------------------------*/
int
CreateConnectionSocket(in_addr_t netAddr, int portNum, int nonBlocking)
{
  struct sockaddr_in saddr;
  int fd;
  struct linger doLinger;
  int doReuse = 1;

  if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      fprintf(stderr, "failed creating socket - %d\n", errno);
	  return(-1);
  }

  /* don't linger on close */
  doLinger.l_onoff = doLinger.l_linger = 0;
  if (setsockopt(fd, SOL_SOCKET, SO_LINGER, 
				 &doLinger, sizeof(doLinger)) == -1) {
	  close(fd);
	  return(-1);
  }
  
  /* reuse addresses */
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
				 &doReuse, sizeof(doReuse)) == -1) {
	  close(fd);
	  return(-1);
  }

  if (nonBlocking) {
    if (fcntl(fd, F_SETFL, O_NDELAY) < 0) {
		fprintf(stderr, "failed fcntl'ing socket - %d\n", errno);
		close(fd);
		return(-1);
    }
  }

  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = netAddr;
  saddr.sin_port = htons(portNum);
  
  if (connect(fd, (struct sockaddr *) &saddr, 
			  sizeof(struct sockaddr_in)) < 0) {
    if (errno == EINPROGRESS)
		return(fd);
	fprintf(stderr, "failed connecting socket addr=%s port %d - errno %d\n", 
			inet_ntoa(saddr.sin_addr), portNum, errno);
    close(fd);
    return(-1);
  }

  return(fd);
}
/*----------------------------------------------------------------------------*/
void
ParseOptions(int argc, const char** argv, struct Options* ops)
{
	int i, j;

	for (i = 1; i < argc; i++) {
		for (j = 0; ops[j].op_name; j++) {
			if (strcmp(ops[j].op_name, argv[i]) == 0) {
				if (i + 1 >= argc) {
					fprintf(stderr, "no value provided for %s option\n",
							argv[i]);
					exit(-1);
				}
				*(ops[j].op_varptr) = (char *)argv[++i];
				break;
			}
		}
		if (ops[j].op_name == NULL) {
			fprintf(stderr, "option %s is not supported\n", argv[i]);
			exit(-1);
		}
	}
}
/*----------------------------------------------------------------------------*/
void 
PrintOptions(const struct Options* ops, int printVal)
{
	int i;

	if (printVal) {
		/* for printing option values */
		printf("The value for each option is as follows:\n");
	} else {
		/* for explaining the options */
		printf("Here is the list of allowable options:\n");
	}
	for (i = 0; ops[i].op_name; i++) {
		printf("%s: %s\n", 
			   ops[i].op_name, printVal? *ops[i].op_varptr:ops[i].op_comment);
	}
}
/*----------------------------------------------------------------------------*/
char *
GetHeaderString(const char *buf, const char* header, int hdrsize)
{
#define SKIP_SPACE(x) while ((*(x)) && isspace((*(x)))) (x)++;
	char *temp = strstr(buf, header);
	
	if (temp) {
		temp += hdrsize;
		SKIP_SPACE(temp);
		if (*temp)
			return (temp);
	}
	return (NULL);
}
/*----------------------------------------------------------------------------*/
int
GetHeaderLong(const char* buf, const char* header, int hdrsize, long int *val)
{
	long int temp_val;
	char *temp;

	if ((temp = GetHeaderString(buf, header, hdrsize)) != NULL) {
		temp_val = strtol(temp, NULL, 10);
		if (errno != ERANGE && errno != EINVAL) {
			*val = temp_val;
			return (TRUE);
		}
	}
	return (FALSE);
}
/*----------------------------------------------------------------------------*/
int
mystrtol(const char *nptr, int base)
{
	int rval;
	char *endptr;

	errno = 0;
	rval = strtol(nptr, &endptr, 10);
	/* check for strtol errors */
	if ((errno == ERANGE && (rval == LONG_MAX ||
				 rval == LONG_MIN))
	    || (errno != 0 && rval == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	if (endptr == nptr) {
		fprintf(stderr, "Parsing strtol error!\n");
		exit(EXIT_FAILURE);
	}

	return rval;
}
/*----------------------------------------------------------------------------*/
