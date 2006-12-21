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
#include "hdhomerun_discover.h"
#include "hdhomerun_control.h"

struct hdhomerun_control_sock_t {
	uint32_t device_id;
	uint32_t device_ip;
	int sock;
	uint8_t buffer[16384];
};

struct hdhomerun_control_sock_t *hdhomerun_control_create(uint32_t device_id, uint32_t device_ip)
{
	struct hdhomerun_control_sock_t *cs = (struct hdhomerun_control_sock_t *)malloc(sizeof(struct hdhomerun_control_sock_t));
	if (!cs) {
		return NULL;
	}
	
	cs->device_id = device_id;
	cs->device_ip = device_ip;
	cs->sock = -1;

	return cs;
}

void hdhomerun_control_destroy(struct hdhomerun_control_sock_t *cs)
{
	if (cs->sock != -1) {
		close(cs->sock);
	}
	free(cs);
}

static void hdhomerun_control_close_sock(struct hdhomerun_control_sock_t *cs)
{
	close(cs->sock);
	cs->sock = -1;
}

static bool_t hdhomerun_control_connect_sock(struct hdhomerun_control_sock_t *cs)
{
	if (cs->sock != -1) {
		return TRUE;
	}

	/* Find ip address. */
	uint32_t device_ip = cs->device_ip;
	if (device_ip == 0) {
		struct hdhomerun_discover_device_t result;
		if (hdhomerun_discover_find_device(cs->device_id, &result) <= 0) {
			return FALSE;
		}
		device_ip = result.ip_addr;
	}

	/* Create socket. */
	cs->sock = (int)socket(AF_INET, SOCK_STREAM, 0);
	if (cs->sock == -1) {
		return FALSE;
	}

	/* Set timeouts. */
	setsocktimeout(cs->sock, SOL_SOCKET, SO_SNDTIMEO, 1000);
	setsocktimeout(cs->sock, SOL_SOCKET, SO_RCVTIMEO, 1000);

	/* Initiate connection. */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(device_ip);
	sock_addr.sin_port = htons(HDHOMERUN_CONTROL_TCP_PORT);
	if (connect(cs->sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		hdhomerun_control_close_sock(cs);
		return FALSE;
	}

	/* Success. */
	return TRUE;
}

uint32_t hdhomerun_control_get_local_addr(struct hdhomerun_control_sock_t *cs)
{
	if (!hdhomerun_control_connect_sock(cs)) {
		return 0;
	}

	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);
	if (getsockname(cs->sock, (struct sockaddr*)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohl(sock_addr.sin_addr.s_addr);
}

static int hdhomerun_control_send(struct hdhomerun_control_sock_t *cs, uint8_t *start, uint8_t *end)
{
	int length = (int)(end - start);
	if (send(cs->sock, (char *)start, (int)length, 0) != length) {
		return -1;
	}

	return length;
}

static int hdhomerun_control_recv_sock(struct hdhomerun_control_sock_t *cs, uint8_t *buffer, uint8_t *limit)
{
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 250000;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(cs->sock, &readfds);

	if (select(cs->sock+1, &readfds, NULL, NULL, &t) < 0) {
		return -1;
	}

	if (!FD_ISSET(cs->sock, &readfds)) {
		return 0;
	}

	int length = recv(cs->sock, (char *)buffer, (int)(limit - buffer), 0);
	if (length <= 0) {
		return -1;
	}

	return length;
}

static int hdhomerun_control_recv(struct hdhomerun_control_sock_t *cs, uint8_t *buffer, uint8_t *limit)
{
	uint64_t timeout = getcurrenttime() + 1000;
	uint8_t *ptr = buffer;

	while (getcurrenttime() < timeout) {
		int length = hdhomerun_control_recv_sock(cs, ptr, limit);
		if (length < 0) {
			return -1;
		}
		if (length == 0) {
			continue;
		}
		ptr += length;

		if (buffer + HDHOMERUN_MIN_PEEK_LENGTH > limit) {
			continue;
		}

		length = (int)hdhomerun_peek_packet_length(buffer);
		if (buffer + length > limit) {
			continue;
		}

		return length;
	}

	return -1;
}

static int hdhomerun_control_get_set(struct hdhomerun_control_sock_t *cs, const char *name, const char *value, char **pvalue, char **perror)
{
	/* Send request. */
	uint8_t *ptr = cs->buffer;
	hdhomerun_write_get_set_request(&ptr, name, value);
	if (hdhomerun_control_send(cs, cs->buffer, ptr) < 0) {
		return -1;
	}

	/* Receive response. */
	int length = hdhomerun_control_recv(cs, cs->buffer, cs->buffer + sizeof(cs->buffer));
	if (length <= 0) {
		return -1;
	}

	/* Parse response. */
	ptr = cs->buffer;
	uint8_t *end = ptr + length;
	int type = hdhomerun_process_packet(&ptr, &end);
	if (type < 0) {
		return -1;
	}
	if (type != HDHOMERUN_TYPE_GETSET_RPY) {
		return -1;
	}

	while (ptr < end) {
		uint8_t tag;
		size_t len;
		uint8_t *val;
		if (hdhomerun_read_tlv(&ptr, end, &tag, &len, &val) < 0) {
			break;
		}
		switch (tag) {
		case HDHOMERUN_TAG_GETSET_VALUE:
			if (pvalue) {
				*pvalue = (char *)val;
				val[len] = 0;
			}
			if (perror) {
				*perror = NULL;
			}
			return 1;

		case HDHOMERUN_TAG_ERROR_MESSAGE:
			if (pvalue) {
				*pvalue = NULL;
			}
			if (perror) {
				*perror = (char *)val;
				val[len] = 0;
			}
			return 0;
		}
	}

	return -1;
}

int hdhomerun_control_get(struct hdhomerun_control_sock_t *cs, const char *name, char **pvalue, char **perror)
{
	if (!hdhomerun_control_connect_sock(cs)) {
		return -1;
	}

	int ret = hdhomerun_control_get_set(cs, name, NULL, pvalue, perror);
	if (ret < 0) {
		hdhomerun_control_close_sock(cs);
		return -1;
	}

	return ret;
}

int hdhomerun_control_set(struct hdhomerun_control_sock_t *cs, const char *name, const char *value, char **pvalue, char **perror)
{
	if (!hdhomerun_control_connect_sock(cs)) {
		return -1;
	}

	int ret = hdhomerun_control_get_set(cs, name, value, pvalue, perror);
	if (ret < 0) {
		hdhomerun_control_close_sock(cs);
		return -1;
	}

	return ret;
}

int hdhomerun_control_upgrade(struct hdhomerun_control_sock_t *cs, FILE *upgrade_file)
{
	if (!hdhomerun_control_connect_sock(cs)) {
		return -1;
	}

	uint32_t sequence = 0;
	uint8_t *ptr;

	while (1) {
		uint8_t data[256];
		size_t length = fread(data, 1, 256, upgrade_file);
		if (length == 0) {
			break;
		}

		ptr = cs->buffer;
		hdhomerun_write_upgrade_request(&ptr, sequence, data, length);
		if (hdhomerun_control_send(cs, cs->buffer, ptr) < 0) {
			hdhomerun_control_close_sock(cs);
			return -1;
		}

		sequence += (uint32_t)length;
	}

	if (sequence == 0) {
		/* No data in file. Error, but no need to close connection. */
		return 0;
	}

	ptr = cs->buffer;
	hdhomerun_write_upgrade_request(&ptr, 0xFFFFFFFF, NULL, 0);
	if (hdhomerun_control_send(cs, cs->buffer, ptr) < 0) {
		hdhomerun_control_close_sock(cs);
		return -1;
	}

	return 1;
}
