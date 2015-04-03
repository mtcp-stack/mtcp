#ifndef __APP_RELAYBUFFER
#define __APP_RELAYBUFFER

#include <mtcp_api.h>

#define MAX_REP_LEN (10*1024)

typedef struct ring_buffer ring_buffer;

ring_buffer* InitBuffer(int size);

int GetTotSizeRBuffer(ring_buffer* r_buff);
int GetDataSizeRBuffer(ring_buffer* r_buff);
int GetCumSizeRBuffer(ring_buffer* r_buff);
int GetRemainBufferSize(ring_buffer *r_buff);
int CheckAvailableSize(ring_buffer *r_buff, int size);

u_char* GetDataPoint(ring_buffer* r_buff);
u_char* GetInputPoint(ring_buffer *r_buff);

int RemoveDataFromBuffer(ring_buffer *r_buff, int size);
int AddDataLen(ring_buffer *r_buffer, int size);

int CopyData(ring_buffer *dest_buff, ring_buffer *src_buff, int len);
int MoveData(ring_buffer *dest_buff, ring_buffer *src_buff, int len);
int MoveToREPData(ring_buffer *dest_buff, ring_buffer *src_buff, int len);

int MtcpWriteFromBuffer(mctx_t mtcp, int fid, ring_buffer *r_buff);
int MtcpReadFromBuffer(mctx_t mtcp, int fid, ring_buffer *r_buff);

#endif
