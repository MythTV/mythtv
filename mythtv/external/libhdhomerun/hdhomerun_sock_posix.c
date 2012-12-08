/*
 * hdhomerun_sock_posix.c
 *
 * Copyright © 2010 Silicondust USA Inc. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * As a special exception to the GNU Lesser General Public License,
 * you may link, statically or dynamically, an application with a
 * publicly distributed version of the Library to produce an
 * executable file containing portions of the Library, and
 * distribute that executable file under terms of your choice,
 * without any of the additional requirements listed in clause 4 of
 * the GNU Lesser General Public License.
 * 
 * By "a publicly distributed version of the Library", we mean
 * either the unmodified Library as distributed by Silicondust, or a
 * modified version of the Library that is distributed under the
 * conditions defined in the GNU Lesser General Public License.
 */

/*
 * Implementation notes:
 *
 * API specifies timeout for each operation (or zero for non-blocking).
 *
 * It is not possible to rely on the OS socket timeout as this will fail to
 * detect the command-response situation where data is sent successfully and
 * the other end chooses not to send a response (other than the TCP ack).
 *
 * The select() cannot be used with high socket numbers (typically max 1024)
 * so the code works as follows:
 * - Use non-blocking sockets to allow operation without select.
 * - Use select where safe (low socket numbers).
 * - Poll with short sleep when select cannot be used safely.
 */

#include "hdhomerun.h"

#include <net/if.h>
#include <sys/ioctl.h>

#ifndef SIOCGIFCONF
#include <sys/sockio.h>
#endif

#ifndef _SIZEOF_ADDR_IFREQ
#define _SIZEOF_ADDR_IFREQ(x) sizeof(x)
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

int hdhomerun_local_ip_info(struct hdhomerun_local_ip_info_t ip_info_list[], int max_count)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == HDHOMERUN_SOCK_INVALID) {
		return -1;
	}

	struct ifconf ifc;
	size_t ifreq_buffer_size = 1024;

	while (1) {
		ifc.ifc_len = ifreq_buffer_size;
		ifc.ifc_buf = (char *)malloc(ifreq_buffer_size);
		if (!ifc.ifc_buf) {
			close(sock);
			return -1;
		}

		memset(ifc.ifc_buf, 0, ifreq_buffer_size);

		if (ioctl(sock, SIOCGIFCONF, &ifc) != 0) {
			free(ifc.ifc_buf);
			close(sock);
			return -1;
		}

		if (ifc.ifc_len < ifreq_buffer_size) {
			break;
		}

		free(ifc.ifc_buf);
		ifreq_buffer_size += 1024;
	}

	char *ptr = ifc.ifc_buf;
	char *end = ifc.ifc_buf + ifc.ifc_len;

	int count = 0;
	while (ptr <= end) {
		struct ifreq *ifr = (struct ifreq *)ptr;
		ptr += _SIZEOF_ADDR_IFREQ(*ifr);

		/* Flags. */
		if (ioctl(sock, SIOCGIFFLAGS, ifr) != 0) {
			continue;
		}

		if ((ifr->ifr_flags & IFF_UP) == 0) {
			continue;
		}
		if ((ifr->ifr_flags & IFF_RUNNING) == 0) {
			continue;
		}

		/* Local IP address. */
		if (ioctl(sock, SIOCGIFADDR, ifr) != 0) {
			continue;
		}

		struct sockaddr_in *ip_addr_in = (struct sockaddr_in *)&(ifr->ifr_addr);
		uint32_t ip_addr = ntohl(ip_addr_in->sin_addr.s_addr);
		if (ip_addr == 0) {
			continue;
		}

		/* Subnet mask. */
		if (ioctl(sock, SIOCGIFNETMASK, ifr) != 0) {
			continue;
		}

		struct sockaddr_in *subnet_mask_in = (struct sockaddr_in *)&(ifr->ifr_addr);
		uint32_t subnet_mask = ntohl(subnet_mask_in->sin_addr.s_addr);

		/* Report. */
		if (count < max_count) {
			struct hdhomerun_local_ip_info_t *ip_info = &ip_info_list[count];
			ip_info->ip_addr = ip_addr;
			ip_info->subnet_mask = subnet_mask;
		}

		count++;
	}

	free(ifc.ifc_buf);
	close(sock);
	return count;
}

hdhomerun_sock_t hdhomerun_sock_create_udp(void)
{
	/* Create socket. */
	hdhomerun_sock_t sock = (hdhomerun_sock_t)socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		return HDHOMERUN_SOCK_INVALID;
	}

	/* Set non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
		close(sock);
		return HDHOMERUN_SOCK_INVALID;
	}

	/* Allow broadcast. */
	int sock_opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&sock_opt, sizeof(sock_opt));

	/* Success. */
	return sock;
}

hdhomerun_sock_t hdhomerun_sock_create_tcp(void)
{
	/* Create socket. */
	hdhomerun_sock_t sock = (hdhomerun_sock_t)socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		return HDHOMERUN_SOCK_INVALID;
	}

	/* Set non-blocking */
	if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
		close(sock);
		return HDHOMERUN_SOCK_INVALID;
	}

	/* Success. */
	return sock;
}

void hdhomerun_sock_destroy(hdhomerun_sock_t sock)
{
	close(sock);
}

int hdhomerun_sock_getlasterror(void)
{
	return errno;
}

uint32_t hdhomerun_sock_getsockname_addr(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);

	if (getsockname(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohl(sock_addr.sin_addr.s_addr);
}

uint16_t hdhomerun_sock_getsockname_port(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);

	if (getsockname(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohs(sock_addr.sin_port);
}

uint32_t hdhomerun_sock_getpeername_addr(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);

	if (getpeername(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohl(sock_addr.sin_addr.s_addr);
}

uint32_t hdhomerun_sock_getaddrinfo_addr(hdhomerun_sock_t sock, const char *name)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *sock_info;
	if (getaddrinfo(name, "", &hints, &sock_info) != 0) {
		return 0;
	}

	struct sockaddr_in *sock_addr = (struct sockaddr_in *)sock_info->ai_addr;
	uint32_t addr = ntohl(sock_addr->sin_addr.s_addr);

	freeaddrinfo(sock_info);
	return addr;
}

bool_t hdhomerun_sock_join_multicast_group(hdhomerun_sock_t sock, uint32_t multicast_ip, uint32_t local_ip)
{
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	imr.imr_multiaddr.s_addr  = htonl(multicast_ip);
	imr.imr_interface.s_addr  = htonl(local_ip);

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(imr)) != 0) {
		return FALSE;
	}

	return TRUE;
}

bool_t hdhomerun_sock_leave_multicast_group(hdhomerun_sock_t sock, uint32_t multicast_ip, uint32_t local_ip)
{
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	imr.imr_multiaddr.s_addr  = htonl(multicast_ip);
	imr.imr_interface.s_addr  = htonl(local_ip);

	if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&imr, sizeof(imr)) != 0) {
		return FALSE;
	}

	return TRUE;
}

bool_t hdhomerun_sock_bind(hdhomerun_sock_t sock, uint32_t local_addr, uint16_t local_port, bool_t allow_reuse)
{
	int sock_opt = allow_reuse;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sock_opt, sizeof(sock_opt));

	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(local_addr);
	sock_addr.sin_port = htons(local_port);

	if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		return FALSE;
	}

	return TRUE;
}

static bool_t hdhomerun_sock_wait_for_read_event(hdhomerun_sock_t sock, uint64_t stop_time)
{
	uint64_t current_time = getcurrenttime();
	if (current_time >= stop_time) {
		return FALSE;
	}

	if (sock < FD_SETSIZE) {
		uint64_t timeout = stop_time - current_time;
		struct timeval t;
		t.tv_sec = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		fd_set errorfds;
		FD_ZERO(&errorfds);
		FD_SET(sock, &errorfds);

		if (select(sock + 1, &readfds, NULL, &errorfds, &t) <= 0) {
			return FALSE;
		}
		if (!FD_ISSET(sock, &readfds)) {
			return FALSE;
		}
	} else {
		uint64_t delay = stop_time - current_time;
		if (delay > 5) {
			delay = 5;
		}

		msleep_approx(delay);
	}

	return TRUE;
}

static bool_t hdhomerun_sock_wait_for_write_event(hdhomerun_sock_t sock, uint64_t stop_time)
{
	uint64_t current_time = getcurrenttime();
	if (current_time >= stop_time) {
		return FALSE;
	}

	if (sock < FD_SETSIZE) {
		uint64_t timeout = stop_time - current_time;
		struct timeval t;
		t.tv_sec = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;

		fd_set writefds;
		FD_ZERO(&writefds);
		FD_SET(sock, &writefds);

		fd_set errorfds;
		FD_ZERO(&errorfds);
		FD_SET(sock, &errorfds);

		if (select(sock + 1, NULL, &writefds, &errorfds, &t) <= 0) {
			return FALSE;
		}
		if (!FD_ISSET(sock, &writefds)) {
			return FALSE;
		}
	} else {
		uint64_t delay = stop_time - current_time;
		if (delay > 5) {
			delay = 5;
		}

		msleep_approx(delay);
	}

	return TRUE;
}

bool_t hdhomerun_sock_connect(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, uint64_t timeout)
{
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(remote_addr);
	sock_addr.sin_port = htons(remote_port);

	if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
			return FALSE;
		}
	}

	uint64_t stop_time = getcurrenttime() + timeout;

	while (1) {
		if (send(sock, NULL, 0, MSG_NOSIGNAL) == 0) {
			return TRUE;
		}

		if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS) && (errno != ENOTCONN)) {
			return FALSE;
		}

		if (!hdhomerun_sock_wait_for_write_event(sock, stop_time)) {
			return FALSE;
		}
	}
}

bool_t hdhomerun_sock_send(hdhomerun_sock_t sock, const void *data, size_t length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;
	const uint8_t *ptr = (const uint8_t *)data;

	while (1) {
		int ret = send(sock, ptr, length, MSG_NOSIGNAL);
		if (ret <= 0) {
			if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
				return FALSE;
			}
			if (!hdhomerun_sock_wait_for_write_event(sock, stop_time)) {
				return FALSE;
			}
			continue;
		}

		if (ret < (int)length) {
			ptr += ret;
			length -= ret;
			continue;
		}

		return TRUE;
	}
}

bool_t hdhomerun_sock_sendto(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, const void *data, size_t length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;
	const uint8_t *ptr = (const uint8_t *)data;

	while (1) {
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = htonl(remote_addr);
		sock_addr.sin_port = htons(remote_port);

		int ret = sendto(sock, ptr, length, 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
		if (ret <= 0) {
			if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
				return FALSE;
			}
			if (!hdhomerun_sock_wait_for_write_event(sock, stop_time)) {
				return FALSE;
			}
			continue;
		}

		if (ret < (int)length) {
			ptr += ret;
			length -= ret;
			continue;
		}

		return TRUE;
	}
}

bool_t hdhomerun_sock_recv(hdhomerun_sock_t sock, void *data, size_t *length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;

	while (1) {
		int ret = recv(sock, data, *length, 0);
		if (ret < 0) {
			if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
				return FALSE;
			}
			if (!hdhomerun_sock_wait_for_read_event(sock, stop_time)) {
				return FALSE;
			}
			continue;
		}

		if (ret == 0) {
			return FALSE;
		}

		*length = ret;
		return TRUE;
	}
}

bool_t hdhomerun_sock_recvfrom(hdhomerun_sock_t sock, uint32_t *remote_addr, uint16_t *remote_port, void *data, size_t *length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;

	while (1) {
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(sock_addr));
		socklen_t sockaddr_size = sizeof(sock_addr);

		int ret = recvfrom(sock, data, *length, 0, (struct sockaddr *)&sock_addr, &sockaddr_size);
		if (ret < 0) {
			if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
				return FALSE;
			}
			if (!hdhomerun_sock_wait_for_read_event(sock, stop_time)) {
				return FALSE;
			}
			continue;
		}

		if (ret == 0) {
			return FALSE;
		}

		*remote_addr = ntohl(sock_addr.sin_addr.s_addr);
		*remote_port = ntohs(sock_addr.sin_port);
		*length = ret;
		return TRUE;
	}
}
