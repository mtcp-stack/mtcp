#ifndef _NET_LIB_H_
#define _NET_LIB_H_

#include <netinet/in.h>

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef VERIFY
#define VERIFY(x) if (!(x)) {fprintf(stderr, "error: FILE:%s LINE:%d FUNC: %s", __FILE__, __LINE__, __FUNCTION__); assert(0);}
#endif

#ifndef FREE
#define FREE(x) if (x) {free(x); (x) = NULL;}
#endif

int GetNumCPUCores(void);
int AffinitizeThreadToCore(int core);
int CreateServerSocket(int port, int isNonBlocking);
int CreateConnectionSocket(in_addr_t addr, int port, int isNonBlocking);
int mystrtol(const char *nptr, int base);

/* processing options */
struct Options {
	char  *op_name;
	char **op_varptr;
	char  *op_comment;
} Options;
void ParseOptions(int argc, const char** argv, struct Options* ops);
void PrintOptions(const struct Options* ops, int printVal);


/* HTTP header processing */
char *GetHeaderString(const char *buf, const char* header, int hdrsize);
int GetHeaderLong(const char* buf, const char* header, int hdrsize, long int *val);

#endif
