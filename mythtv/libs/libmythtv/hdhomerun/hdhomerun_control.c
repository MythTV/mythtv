/*
 * hdhomerun_control.c
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
#include "hdhomerun_control.h"

struct hdhomerun_control_sock_t {
	int sock;
	unsigned char rx_buffer[1024];
	unsigned char *rx_pos;
	unsigned char *rx_end;
};

struct hdhomerun_control_sock_t *hdhomerun_control_create(unsigned long ip_addr, unsigned long timeout)
{
	struct hdhomerun_control_sock_t *cs = (struct hdhomerun_control_sock_t *)malloc(sizeof(struct hdhomerun_control_sock_t));
	if (!cs) {
		return NULL;
	}
	
	/* Create socket. */
	cs->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (cs->sock == -1) {
		free(cs);
		return NULL;
	}

	/* Set non blocking. */
	fcntl(cs->sock, F_SETFL, O_NONBLOCK);

	/* Initiate connection. */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(ip_addr);
	sock_addr.sin_port = htons(HDHOMERUN_CONTROL_TCP_PORT);
	if (connect(cs->sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		if ((errno != EINPROGRESS) && (errno != EAGAIN)) {
			hdhomerun_control_destroy(cs);
			return NULL;
		}
	}

	/* Wait for connect to succeed. */
	struct timeval t;
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;

	fd_set writefds;
	fd_set exceptfds;
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	FD_SET(cs->sock, &writefds);
	FD_SET(cs->sock, &exceptfds);

	if (select(cs->sock+1, NULL, &writefds, &exceptfds, &t) < 0) {
		hdhomerun_control_destroy(cs);
		return NULL;
	}
	if (!FD_ISSET(cs->sock, &writefds)) {
		hdhomerun_control_destroy(cs);
		return NULL;
	}

	/* Success. */
	cs->rx_pos = cs->rx_buffer;
	cs->rx_end = cs->rx_buffer + sizeof(cs->rx_buffer);
	return cs;
}

void hdhomerun_control_destroy(struct hdhomerun_control_sock_t *cs)
{
	close(cs->sock);
	free(cs);
}

unsigned long hdhomerun_control_get_local_addr(struct hdhomerun_control_sock_t *cs)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);
	if (getsockname(cs->sock, (struct sockaddr*)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}
	return ntohl(sock_addr.sin_addr.s_addr);
}

int hdhomerun_control_send(struct hdhomerun_control_sock_t *cs, unsigned char *start, unsigned char *end)
{
	int length = end - start;
	if (send(cs->sock, (char *)start, length, 0) != length) {
		if ((errno != EINPROGRESS) && (errno != EAGAIN)) {
			return -1;
		}
	}
	return 0;
}

int hdhomerun_control_send_get_request(struct hdhomerun_control_sock_t *cs, const char *name)
{
	unsigned char buffer[1024];
	unsigned char *ptr = buffer;
	hdhomerun_write_get_request(&ptr, name);
	return hdhomerun_control_send(cs, buffer, ptr);
}

int hdhomerun_control_send_set_request(struct hdhomerun_control_sock_t *cs, const char *name, const char *value)
{
	unsigned char buffer[1024];
	unsigned char *ptr = buffer;
	hdhomerun_write_set_request(&ptr, name, value);
	return hdhomerun_control_send(cs, buffer, ptr);
}

static int hdhomerun_control_recv_sock(struct hdhomerun_control_sock_t *cs, unsigned long timeout)
{
	struct timeval t;
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(cs->sock, &readfds);

	if (select(cs->sock+1, &readfds, NULL, NULL, &t) < 0) {
		return -1;
	}
	if (!FD_ISSET(cs->sock, &readfds)) {
		return 0;
	}

	int rx_length = recv(cs->sock, (char *)cs->rx_pos, cs->rx_end - cs->rx_pos, 0);
	if (rx_length <= 0) {
		return -1;
	}
	cs->rx_pos += rx_length;
	return 1;
}

static int hdhomerun_control_recv_process(struct hdhomerun_control_sock_t *cs, struct hdhomerun_control_data_t *result)
{
	int rx_length = cs->rx_pos - cs->rx_buffer;
	if (rx_length < HDHOMERUN_MIN_PEEK_LENGTH) {
		return 0;
	}
	int length = hdhomerun_peek_packet_length(cs->rx_buffer);
	if (rx_length < length) {
		return 0;
	}

	memcpy(result->buffer, cs->rx_buffer, length);
	if (rx_length > length) {
		memcpy(cs->rx_buffer, cs->rx_buffer + length, rx_length - length);
	}
	cs->rx_pos = cs->rx_buffer + (rx_length - length);

	result->ptr = result->buffer;
	result->end = result->buffer + length;
	result->type = hdhomerun_process_packet(&result->ptr, &result->end);
	if (result->type < 0) {
		return -1;
	}

	return 1;
}

int hdhomerun_control_recv(struct hdhomerun_control_sock_t *cs, struct hdhomerun_control_data_t *result, unsigned long timeout)
{
	int ret = hdhomerun_control_recv_process(cs, result);
	if (ret == 0) {
		ret = hdhomerun_control_recv_sock(cs, timeout);
		if (ret == 1) {
			ret = hdhomerun_control_recv_process(cs, result);
		}
	}
	return ret;
}
