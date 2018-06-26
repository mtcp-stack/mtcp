#ifndef MTCP_PIPE_H
#define MTCP_PIPE_H

#include <mtcp_api.h>

int 
PipeRead(mctx_t mctx, int pipeid, char *buf, int len);

int 
PipeWrite(mctx_t mctx, int pipeid, const char *buf, int len);

int 
RaisePendingPipeEvents(mctx_t mctx, int epid, int pipeid);

int 
PipeClose(mctx_t mctx, int pipeid);

#endif /* MTCP_PIPE_H */
