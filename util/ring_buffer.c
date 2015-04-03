#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mtcp_api.h>

#include "debug.h"
#include "rep.h"
#include "ring_buffer.h"

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*----------------------------------------------------------------------------*/
struct ring_buffer 
{
	u_char* head; // pointer to the head
	u_char* data; // pointer to the buffered data

	int tot_size; // size of buffer
	int data_size; // length of data
	int cum_size; // length of total merged data

};
/*----------------------------------------------------------------------------*/
ring_buffer* 
InitBuffer(int size) 
{
	ring_buffer* r_buff = calloc (1, sizeof(ring_buffer));
	r_buff->head = malloc(size);
	r_buff->data = r_buff->head;
	r_buff->tot_size = size;
	return r_buff;
}
/*----------------------------------------------------------------------------*/
int GetTotSizeRBuffer(ring_buffer* r_buff) 
{
	return  r_buff->tot_size;
}
/*----------------------------------------------------------------------------*/
int GetDataSizeRBuffer(ring_buffer* r_buff) 
{
	return  r_buff->data_size;
}
/*----------------------------------------------------------------------------*/
int GetCumSizeRBuffer(ring_buffer* r_buff) 
{
	return  r_buff->cum_size;
}
/*----------------------------------------------------------------------------*/
int GetRemainBufferSize(ring_buffer *r_buff) 
{
	assert(r_buff->head <= r_buff->data);
	
	int data_offset = r_buff->data - r_buff->head;
	assert (data_offset <= r_buff->tot_size - 1);

	if (data_offset > r_buff->tot_size / 2) {
		memmove(r_buff->head, r_buff->data, r_buff->data_size);
		r_buff->data = r_buff->head;
		data_offset = 0;
		return r_buff->tot_size - r_buff->data_size;
	}

	return r_buff->tot_size -  r_buff->data_size - data_offset;
}
/*----------------------------------------------------------------------------*/
int CheckAvailableSize(ring_buffer *r_buff, int size)
{	
	int remain_size = GetRemainBufferSize(r_buff);
	if (remain_size < size)
		return FALSE;
	else 
		return TRUE;
}
/*----------------------------------------------------------------------------*/
u_char* GetDataPoint(ring_buffer* r_buff) 
{
	return r_buff->data;
}
/*----------------------------------------------------------------------------*/
u_char* GetInputPoint(ring_buffer *r_buff) 
{
	assert(r_buff->data_size <= r_buff->tot_size);
	return r_buff->data + r_buff->data_size;
}
/*----------------------------------------------------------------------------*/
int RemoveDataFromBuffer(ring_buffer* r_buff, int size) 
{
	if (size < 0)
		return -1;

	if (size > r_buff->data_size)
		return -1;
	
	r_buff->data_size -= size;
	
	if (r_buff->data_size == 0)
		r_buff->data = r_buff->head;
	else
		r_buff->data += size;

	return size;
}
/*----------------------------------------------------------------------------*/
int AddDataLen(ring_buffer *r_buff, int size)
{
	assert(r_buff->data_size + size <= r_buff->tot_size);
	r_buff->data_size += size;
	r_buff->cum_size += size;
	return r_buff->data_size;
}
/*----------------------------------------------------------------------------*/
int CopyData(ring_buffer *dest_buff, ring_buffer *src_buff, int len) 
{
	int to_cpy;
	u_char* ip;

	// getting length to copy
	to_cpy = GetRemainBufferSize(dest_buff);
	if (to_cpy <= 0)
		return 0;

	if (to_cpy > 0)
		to_cpy = MIN(to_cpy, len);
	to_cpy = MIN(GetDataSizeRBuffer(src_buff), to_cpy);

	// copy from src_buffer to dest_buffer
	ip = GetInputPoint(dest_buff);
	memcpy(ip, GetDataPoint(src_buff), to_cpy);
	AddDataLen(dest_buff, to_cpy);

	return to_cpy;
}
/*----------------------------------------------------------------------------*/
int MoveToREPData(ring_buffer *dest_buff, ring_buffer *src_buff, int len)
{
	int to_move, ret, sum = 0;
	u_char* ip;
	
	int data_size = GetDataSizeRBuffer(src_buff);
	int remain_size = GetRemainBufferSize(dest_buff);

	if (len > 0)
		data_size = MIN(data_size, len);

	while (data_size > 0 && remain_size > sizeof(rephdr)) {
		// getting length to move		
		to_move = MIN(data_size, remain_size - sizeof(rephdr));
		to_move = MIN(to_move, MAX_REP_LEN);
		if (to_move <= 0)
			return 0;
		
		remain_size -= sizeof(rephdr);
		remain_size -= to_move;
		data_size -= to_move;

		ip = GetInputPoint(dest_buff);
		
		// create rep header  
		rephdr rep;
		rep.msg_type = 0x01;
		rep.command = 0x00;
		rep.msg_len = to_move;
		memcpy(ip, &rep, sizeof(rephdr));
		ip += sizeof(rephdr);
		AddDataLen(dest_buff, sizeof(rephdr));
		
		// copy from src_buffer to dest_buffer
		memcpy(ip, GetDataPoint(src_buff), to_move);
		AddDataLen(dest_buff, to_move);
		sum += to_move;

		// remove from src_buff
		ret = RemoveDataFromBuffer(src_buff, to_move);
		assert(to_move == ret);
	}
	
	return sum;
}
/*----------------------------------------------------------------------------*/
int MoveData(ring_buffer *dest_buff, ring_buffer *src_buff, int len)
{
	int to_move, ret;
	u_char* ip;

	// getting length to move
	to_move = GetDataSizeRBuffer(src_buff);

	if (len > 0)
		to_move = MIN(to_move, len);

	to_move = MIN(GetDataSizeRBuffer(src_buff), to_move);
	if(to_move <= 0)
		return 0;

	// copy from src_buffer to dest_buffer
	ip = GetInputPoint(dest_buff);
	memcpy(ip, GetDataPoint(src_buff), to_move);
	AddDataLen(dest_buff, to_move);

	// remove from src_buff
	ret = RemoveDataFromBuffer(src_buff, to_move);
	assert(to_move == ret);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
int MtcpWriteFromBuffer(mctx_t mctx, int fid, ring_buffer *r_buff)
{
	int to_send, wr, ret;
	to_send = GetDataSizeRBuffer(r_buff);
	if (to_send <= 0)
		return 0;

	wr = mtcp_write(mctx, fid, GetDataPoint(r_buff), to_send);
	if (wr < 0) {
		TRACE_APP("RE_MAIN: Write failed. reason: %d\n", wr);
		return wr;
	}

	ret = RemoveDataFromBuffer(r_buff, wr);
	assert (wr == ret);

	return wr;
}
/*----------------------------------------------------------------------------*/
int MtcpReadFromBuffer(mctx_t mctx, int fid, ring_buffer *r_buff)
{
	int free_len, ret;
	u_char* ip;
	
	free_len = GetRemainBufferSize(r_buff);
	ip = GetInputPoint(r_buff);

	ret = mtcp_read(mctx, fid, ip, free_len);
	if (ret < 0) {
		TRACE_APP("RE_MAIN: Read failed. reason: %d\n", ret);
		return ret;
	}
	AddDataLen(r_buff, ret);

	return ret;
}
/*----------------------------------------------------------------------------*/
