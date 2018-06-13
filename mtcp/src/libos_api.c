#include <sys/queue.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>

#include "mtcp.h"
#include "mtcp_api.h"
#include "tcp_in.h"
#include "tcp_stream.h"
#include "tcp_out.h"
#include "ip_out.h"
#include "eventpoll.h"
#include "pipe.h"
#include "fhash.h"
#include "addr_pool.h"
#include "rss.h"
#include "config.h"
#include "debug.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))


/******************************************************************************/
// libos_mtcp implementation
/******************************************************************************/

// network functions
int libos_mtcp_queue(int domain, int type, int protocol){
	printf("@@@@@@@@@JINGLIU:libos_mtcp_queue@@@@@@@@@\n");
    return 0;
}
int libos_mtcp_listen(int qd, int backlog){
    return 0;
}
int libos_mtcp_bind(int qd, struct sockaddr *saddr, socklen_t size){
    return 0;
}
int libos_mtcp_accept(int qd, struct sockaddr *saddr, socklen_t *size){
    return 0;
}
int libos_mtcp_connect(int qd, struct sockaddr *saddr, socklen_t size){
    return 0;
}
int libos_mtcp_close(int qd){
    return 0;
}

// other functions
int libos_mtcp_push(int qd, zeus_sgarray *sga){
    // if return 0, then already complete
    return 0;
}
int libos_mtcp_pop(int qd, zeus_sgarray *sga){
    //if return 0, then already ready and in sga
    return 0;
}
ssize_t libost_mtpc_wait(int *qts, size_t num_qts){
    return 0;
}
ssize_t libos_mtcp_wait_all(int *qts, size_t num_qts){
    // identical to a push, followed by a wait on the returned qtoken
    return 0;
}
ssize_t libos_mtcp_blocking_push(int qd, zeus_sgarray *sga){
    // identical to a pop, followed by a wait on the returned qtoken
    return 0;
}
ssize_t libos_mtcp_blocking_pop(int qd, zeus_sgarray *sga){
    return 0;
}

/******************************************************************************/
/******************************************************************************/

