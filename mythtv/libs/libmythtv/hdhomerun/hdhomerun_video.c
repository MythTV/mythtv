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

struct hdhomerun_video_sock_t {
	int sock;
};

struct hdhomerun_video_sock_t *hdhomerun_video_create(void)
{
	struct hdhomerun_video_sock_t *vs = (struct hdhomerun_video_sock_t *)malloc(sizeof(struct hdhomerun_video_sock_t));
	if (!vs) {
		return NULL;
	}
	
	/* Create socket. */
	vs->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (vs->sock == -1) {
		free(vs);
		return NULL;
	}

	/* Set non blocking. */
	fcntl(vs->sock, F_SETFL, O_NONBLOCK);

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

	/* Success. */
	return vs;
}

void hdhomerun_video_destroy(struct hdhomerun_video_sock_t *vs)
{
	close(vs->sock);
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

int hdhomerun_video_recv(struct hdhomerun_video_sock_t *vs, struct hdhomerun_video_data_t *result, unsigned long timeout)
{
	int i = 0;
	char *ptr = (char *) result->buffer;
	struct timeval t;
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(vs->sock, &readfds);

	if (select(vs->sock+1, &readfds, NULL, NULL, &t) < 0) {
		return -1;
	}
	if (!FD_ISSET(vs->sock, &readfds)) {
		return 0;
	}

	result->length = 0;
	for (i = 0; i < VIDEO_DATA_DMESG_COUNT; i++)
	{
		int length = recv(vs->sock, ptr, VIDEO_DATA_DMESG_SIZE,
				  MSG_DONTWAIT);
		if (length <= 0)
			break;
		ptr += length;
		result->length += length;
	}

	if (result->length <= 0)
		return -1;

	return 1;
}
