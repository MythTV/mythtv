/*
 * hdhomerun_discover.c
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
#include "hdhomerun_discover.h"

struct hdhomerun_discover_sock_t {
	int sock;
};

struct hdhomerun_discover_sock_t *hdhomerun_discover_create(void)
{
	struct hdhomerun_discover_sock_t *ds = (struct hdhomerun_discover_sock_t *)malloc(sizeof(struct hdhomerun_discover_sock_t));
	if (!ds) {
		return NULL;
	}
	
	/* Create socket. */
	ds->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ds->sock == -1) {
		free(ds);
		return NULL;
	}

	/* Set non blocking. */
	fcntl(ds->sock, F_SETFL, O_NONBLOCK);

	/* Allow broadcast. */
	int sock_opt = 1;
	setsockopt(ds->sock, SOL_SOCKET, SO_BROADCAST, (char *)&sock_opt, sizeof(sock_opt));

	/* Bind socket. */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = htons(0);
	if (bind(ds->sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		hdhomerun_discover_destroy(ds);
		return NULL;
	}

	/* Success. */
	return ds;
}

void hdhomerun_discover_destroy(struct hdhomerun_discover_sock_t *ds)
{
	close(ds->sock);
	free(ds);
}

int hdhomerun_discover_send(struct hdhomerun_discover_sock_t *ds, unsigned long device_type, unsigned long device_id)
{
	unsigned char buffer[1024];
	unsigned char *ptr = buffer;
	hdhomerun_write_discover_request(&ptr, device_type, device_id);

	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	sock_addr.sin_port = htons(HDHOMERUN_DISCOVER_UDP_PORT);

	int length = ptr - buffer;
	if (sendto(ds->sock, (char *)buffer, length, 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != length) {
		if ((errno != EINPROGRESS) && (errno != EAGAIN)) {
			return -1;
		}
	}

	return 0;
}

int hdhomerun_discover_recv(struct hdhomerun_discover_sock_t *ds, struct hdhomerun_discover_device_t *result, unsigned long timeout)
{
	struct timeval t;
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(ds->sock, &readfds);

	if (select(ds->sock+1, &readfds, NULL, NULL, &t) < 0) {
		return -1;
	}
	if (!FD_ISSET(ds->sock, &readfds)) {
		return 0;
	}

	unsigned char buffer[1024];
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);
	int rx_length = recvfrom(ds->sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&sock_addr, &sockaddr_size);
	if (rx_length <= 0) {
		return -1;
	}
	if (rx_length < HDHOMERUN_MIN_PEEK_LENGTH) {
		return 0;
	}
	int length = hdhomerun_peek_packet_length(buffer);
	if (rx_length < length) {
		return 0;
	}

	unsigned char *ptr = buffer;
	unsigned char *end = buffer + length;
	int type = hdhomerun_process_packet(&ptr, &end);
	if (type != HDHOMERUN_TYPE_DISCOVER_RPY) {
		return 0;
	}

	result->ip_addr = ntohl(sock_addr.sin_addr.s_addr);
	result->device_type = 0;
	result->device_id = 0;
	while (1) {
		unsigned char tag;
		unsigned char len;
		unsigned char *value;
		if (hdhomerun_read_tlv(&ptr, end, &tag, &len, &value) < 0) {
			break;
		}

		switch (tag) {
		case HDHOMERUN_TAG_DEVICE_TYPE:
			if (len != 4) {
				break;
			}
			result->device_type = hdhomerun_read_u32(&value);
			break;
		case HDHOMERUN_TAG_DEVICE_ID:
			if (len != 4) {
				break;
			}
			result->device_id = hdhomerun_read_u32(&value);
			break;
		default:
			break;
		}
	}

	return 1;
}
