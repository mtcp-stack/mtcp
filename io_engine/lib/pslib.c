#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "../include/ps.h"

int ps_list_devices(struct ps_device *devices)
{
	struct ps_handle handle;
	int ret;

	if (ps_init_handle(&handle))
		return -1;

	ret = ioctl(handle.fd, PS_IOC_LIST_DEVICES, devices);
	
	ps_close_handle(&handle);

	return ret;
}

int ps_init_handle(struct ps_handle *handle)
{
	memset(handle, 0, sizeof(struct ps_handle));

	handle->fd = open("/dev/packet_shader", O_RDWR);
	if (handle->fd == -1)
		return -1;

	return 0;
}

void ps_close_handle(struct ps_handle *handle)
{
	close(handle->fd);
	handle->fd = -1;
}

int ps_attach_rx_device(struct ps_handle *handle, struct ps_queue *queue)
{
	return ioctl(handle->fd, PS_IOC_ATTACH_RX_DEVICE, queue);
}

int ps_detach_rx_device(struct ps_handle *handle, struct ps_queue *queue)
{
	return ioctl(handle->fd, PS_IOC_DETACH_RX_DEVICE, queue);
}

int ps_alloc_chunk(struct ps_handle *handle, struct ps_chunk *chunk)
{
	memset(chunk, 0, sizeof(*chunk));

	chunk->info = (struct ps_pkt_info *)malloc(
			sizeof(struct ps_pkt_info) * MAX_CHUNK_SIZE);
	if (!chunk->info)
		return -1;

	chunk->buf = (char *)mmap(NULL, MAX_PACKET_SIZE * MAX_CHUNK_SIZE, 
			PROT_READ | PROT_WRITE, MAP_SHARED,
			handle->fd, 0);
	if ((long)chunk->buf == -1)
		return -1;
	
	return 0;
}

void ps_free_chunk(struct ps_chunk *chunk)
{
	free(chunk->info);
	munmap(chunk->buf, MAX_PACKET_SIZE * MAX_CHUNK_SIZE);

	chunk->info = NULL;
	chunk->buf = NULL;
}

int ps_alloc_chunk_buf(struct ps_handle *handle,
		int ifidx, int qidx, struct ps_chunk_buf *c_buf) 
{
	memset(c_buf, 0, sizeof(*c_buf));
	
	c_buf->info = (struct ps_pkt_info *)malloc(
			sizeof(struct ps_pkt_info) * ENTRY_CNT);
	if (!c_buf->info)
		return -1;
	
	c_buf->buf = (char *)mmap(NULL, MAX_PACKET_SIZE * ENTRY_CNT, 
			PROT_READ | PROT_WRITE, MAP_SHARED,
			handle->fd, 0);
	if ((long)c_buf->buf == -1)
		return -1;

	c_buf->lock = (pthread_mutex_t *) malloc(
			sizeof(pthread_mutex_t));

	c_buf->queue.ifindex = ifidx;
	c_buf->queue.qidx = qidx;
	c_buf->cnt = 0;
	c_buf->next_to_use = 0;
	c_buf->next_to_send = 0;
	c_buf->next_offset = 0;

	if (pthread_mutex_init(c_buf->lock, NULL)) {
		perror("pthread_mutex_init of c_buf->lock\n");
		return -1;
	}

	return 0;
}

void ps_free_chunk_buf(struct ps_chunk_buf *c_buf) 
{
	free(c_buf->info);
	munmap(c_buf->buf, MAX_PACKET_SIZE * ENTRY_CNT);

	c_buf->info = NULL;
	c_buf->buf = NULL;
}

char* ps_assign_chunk_buf(struct ps_chunk_buf *c_buf, int len) {
	
	int w_idx;
	
	if (c_buf->cnt >= ENTRY_CNT)
		return NULL;

	pthread_mutex_lock(c_buf->lock);
	
	w_idx = c_buf->next_to_use;

	c_buf->cnt++;
	c_buf->info[w_idx].len = len;
	c_buf->info[w_idx].offset = c_buf->next_offset;
	c_buf->next_offset += (len + 63) / 64 * 64;

	c_buf->next_to_use = (w_idx + 1) % ENTRY_CNT;

	if(c_buf->next_to_use == 0)
		c_buf->next_offset = 0;

	pthread_mutex_unlock(c_buf->lock);

	return c_buf->buf + c_buf->info[w_idx].offset;
}

int ps_recv_chunk(struct ps_handle *handle, struct ps_chunk *chunk)
{
	int cnt;

	cnt = ioctl(handle->fd, PS_IOC_RECV_CHUNK, chunk);
	if (cnt > 0) {
		int i;
		int ifindex = chunk->queue.ifindex;

		handle->rx_chunks[ifindex]++;
		handle->rx_packets[ifindex] += cnt;

		for (i = 0; i < cnt; i++)
			handle->rx_bytes[ifindex] += chunk->info[i].len;
	}

	return cnt;
}

int ps_recv_chunk_ifidx(struct ps_handle *handle, struct ps_chunk *chunk, int ifidx) 
{
	int cnt;

	chunk->queue.ifindex = ifidx;
	cnt = ioctl(handle->fd, PS_IOC_RECV_CHUNK_IFIDX, chunk);
	if (cnt > 0) {
		int i;
		int ifindex = chunk->queue.ifindex;

		handle->rx_chunks[ifindex]++;
		handle->rx_packets[ifindex] += cnt;

		for (i = 0; i < cnt; i++)
			handle->rx_bytes[ifindex] += chunk->info[i].len;
	}

	return cnt;
}

/* Send the given chunk to the modified driver. */
int ps_send_chunk(struct ps_handle *handle, struct ps_chunk *chunk)
{
	int cnt;

	cnt = ioctl(handle->fd, PS_IOC_SEND_CHUNK, chunk);
	if (cnt > 0) {
		int i;
		int ifindex = chunk->queue.ifindex;

		handle->tx_chunks[ifindex]++;
		handle->tx_packets[ifindex] += cnt;

		for (i = 0; i < cnt; i++)
			handle->tx_bytes[ifindex] += chunk->info[i].len;
	}

	return cnt;
}

/* Send the given chunk to the modified driver. */
int ps_send_chunk_buf(struct ps_handle *handle, struct ps_chunk_buf *c_buf)
{
	int cnt;

	if(c_buf->cnt <= 0) 
		return 0;

	pthread_mutex_lock(c_buf->lock);

	cnt = ioctl(handle->fd, PS_IOC_SEND_CHUNK_BUF, c_buf);
	if (cnt > 0) {
		int i;
		int ifindex = c_buf->queue.ifindex;

		handle->tx_chunks[ifindex]++;
		handle->tx_packets[ifindex] += cnt;

		for (i = 0; i < cnt; i++)
			handle->tx_bytes[ifindex] += c_buf->info[i].len;
		
		c_buf->cnt -= cnt;
		c_buf->next_to_send = (c_buf->next_to_send + cnt) % ENTRY_CNT;

	}

	pthread_mutex_unlock(c_buf->lock);

	return cnt;
}

int ps_select(struct ps_handle *handle, struct ps_event * event)
{
	return ioctl(handle->fd, PS_IOC_SELECT, event);
}

/* Get the remain number of tx_entry in a tx_ring */
int ps_get_txentry(struct ps_handle *handle, struct ps_queue *queue)
{
	return ioctl(handle->fd, PS_IOC_GET_TXENTRY, queue);
}

int ps_slowpath_packet(struct ps_handle *handle, struct ps_packet *packet)
{
	return ioctl(handle->fd, PS_IOC_SLOWPATH_PACKET, packet);
}
