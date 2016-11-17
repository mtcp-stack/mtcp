#ifndef __MTCP_PIPE_H_
#define __MTCP_PIPE_H_

#include <mtcp_api.h>

int 
PipeRead(mctx_t mctx, int pipeid, char *buf, int len);

int 
PipeWrite(mctx_t mctx, int pipeid, const char *buf, int len);

int 
RaisePendingPipeEvents(mctx_t mctx, int epid, int pipeid);

int 
PipeClose(mctx_t mctx, int pipeid);

#endif /* __MTCP_PIPE_H_ */
