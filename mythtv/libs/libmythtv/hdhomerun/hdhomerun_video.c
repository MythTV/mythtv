/*
 * hdhomerun_video.c
 *
 * Copyright © 2006 Silicondust Engineering Ltd. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "hdhomerun_os.h"
#include "hdhomerun_pkt.h"
#include "hdhomerun_video.h"
#include <pthread.h>

struct hdhomerun_video_sock_t {
	unsigned char *buffer;
	unsigned long buffer_size;
	volatile unsigned long head;
	volatile unsigned long tail;
	unsigned long advance;
	volatile int running;
	volatile int terminate;
	pthread_t thread;
	int sock;
};

static void *hdhomerun_video_thread(void *arg);

struct hdhomerun_video_sock_t *hdhomerun_video_create(unsigned long buffer_size)
{
	/* Buffer size. */
	buffer_size = (buffer_size / VIDEO_DATA_PACKET_SIZE) * VIDEO_DATA_PACKET_SIZE;
	if (buffer_size == 0) {
		return NULL;
	}
	buffer_size += VIDEO_DATA_PACKET_SIZE;

	/* Create object. */
	struct hdhomerun_video_sock_t *vs = (struct hdhomerun_video_sock_t *)calloc(1, sizeof(struct hdhomerun_video_sock_t));
	if (!vs) {
		return NULL;
	}

	/* Create buffer. */
	vs->buffer_size = buffer_size;
	vs->buffer = (unsigned char *)malloc(buffer_size);
	if (!vs->buffer) {
		free(vs);
		return NULL;
	}
	
	/* Create socket. */
	vs->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (vs->sock == -1) {
		free(vs->buffer);
		free(vs);
		return NULL;
	}

	/* Set non blocking. */
	fcntl(vs->sock, F_SETFL, O_NONBLOCK);

	/* Set buffer size. */
	unsigned long rx_size = 1024 * 1024;
	setsockopt(vs->sock, SOL_SOCKET, SO_RCVBUF, &rx_size, sizeof(rx_size));

	/* Bind socket. */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = htons(0);
	if (bind(vs->sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		hdhomerun_video_destroy(vs);
		return NULL;
	}

	/* Start thread. */
	if (pthread_create(&vs->thread, NULL, &hdhomerun_video_thread, vs) != 0) {
		hdhomerun_video_destroy(vs);
		return NULL;
	}
	vs->running = 1;

	/* Success. */
	return vs;
}

void hdhomerun_video_destroy(struct hdhomerun_video_sock_t *vs)
{
	if (vs->running) {
		vs->terminate = 1;
		pthread_join(vs->thread, NULL);
	}
	close(vs->sock);
	free(vs->buffer);
	free(vs);
}

unsigned short hdhomerun_video_get_local_port(struct hdhomerun_video_sock_t *vs)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);
	if (getsockname(vs->sock, (struct sockaddr*)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}
	return ntohs(sock_addr.sin_port);
}

int hdhomerun_video_get_state(struct hdhomerun_video_sock_t *vs)
{
	if (vs->terminate) {
		return 0;
	}
	return 1;
}

static void *hdhomerun_video_thread(void *arg)
{
	struct hdhomerun_video_sock_t *vs = (struct hdhomerun_video_sock_t *)arg;

	while (!vs->terminate) {
		struct timeval t;
		t.tv_sec = 1;
		t.tv_usec = 0;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(vs->sock, &readfds);

		if (select(vs->sock+1, &readfds, NULL, NULL, &t) < 0) {
			vs->terminate = 1;
			return NULL;
		}
		if (!FD_ISSET(vs->sock, &readfds)) {
			continue;
		}

		while (1) {
			unsigned long head = vs->head;

			/* Receive. */
			int length = recv(vs->sock, vs->buffer + head, VIDEO_DATA_PACKET_SIZE, MSG_DONTWAIT);
			if (length != VIDEO_DATA_PACKET_SIZE) {
				if (length > 0) {
					/* Data received but not valid - ignore. */
					continue;
				}
				if (errno == EAGAIN) {
					/* Wait for more data. */
					break;
				}
				vs->terminate = 1;
				return NULL;
			}

			/* Calculate new head. */
			head += length;
			if (head >= vs->buffer_size) {
				head -= vs->buffer_size;
			}

			/* Check for buffer overflow. */
			if (head == vs->tail) {
				continue;
			}

			/* Atomic update. */
			vs->head = head;
		}
	}

	return NULL;
}

static void hdhomerun_copy_and_advance_tail(struct hdhomerun_video_sock_t *vs, unsigned char *buffer, unsigned long size)
{
	unsigned long tail = vs->tail;
	memcpy(buffer, vs->buffer + tail, size);

	tail += size;
	if (tail >= vs->buffer_size) {
		tail -= vs->buffer_size;
	}

	/* Atomic update. */
	vs->tail = tail;
}

unsigned long hdhomerun_video_recv_memcpy(struct hdhomerun_video_sock_t *vs, unsigned char *buffer, unsigned long size)
{
	unsigned long head = vs->head;
	unsigned long tail = vs->tail;

	if (head == tail) {
		return 0;
	}

	size = (size / VIDEO_DATA_PACKET_SIZE) * VIDEO_DATA_PACKET_SIZE;

	/* Straight memcpy case. */
	if (head > tail) {
		unsigned long avail = head - tail;
		if (size > avail) {
			size = avail;
		}
		hdhomerun_copy_and_advance_tail(vs, buffer, size);
		return size;
	}

	/* Memcpy with wrap around check. */
	unsigned long avail = vs->buffer_size - tail;
	if (avail >= size) {
		hdhomerun_copy_and_advance_tail(vs, buffer, size);
		return size;
	}

	/* Memcpy with wrap around. */
	hdhomerun_copy_and_advance_tail(vs, buffer, avail);
	return avail + hdhomerun_video_recv_memcpy(vs, buffer + avail, size - avail);
}

unsigned char *hdhomerun_video_recv_inplace(struct hdhomerun_video_sock_t *vs, unsigned long max_size, unsigned long *pactual_size)
{
	unsigned long head = vs->head;
	unsigned long tail = vs->tail;

	if (vs->advance > 0) {
		tail += vs->advance;
		if (tail >= vs->buffer_size) {
			tail -= vs->buffer_size;
		}
	
		/* Atomic update. */
		vs->tail = tail;
	}

	if (head == tail) {
		vs->advance = 0;
		*pactual_size = 0;
		return NULL;
	}

	unsigned long size = (max_size / VIDEO_DATA_PACKET_SIZE) * VIDEO_DATA_PACKET_SIZE;

	unsigned long avail;
	if (head > tail) {
		avail = head - tail;
	} else {
		avail = vs->buffer_size - tail;
	}
	if (size > avail) {
		size = avail;
	}
	vs->advance = size;
	*pactual_size = size;
	return vs->buffer + tail;
}

void hdhomerun_video_flush(struct hdhomerun_video_sock_t *vs)
{
	/* Atomic update of tail. */
	vs->tail = vs->head;
}

