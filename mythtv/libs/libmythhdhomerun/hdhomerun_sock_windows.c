/*
 * hdhomerun_sock_windows.c
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
 * Windows supports select() however native WSA events are used to:
 * - avoid problems with socket numbers above 1024.
 * - wait without allowing other events handlers to run (important for use
 *   with win7 WMC).
 */

#include "hdhomerun.h"
#include <windows.h>
#include <iphlpapi.h>

int hdhomerun_local_ip_info(struct hdhomerun_local_ip_info_t ip_info_list[], int max_count)
{
	PIP_ADAPTER_INFO AdapterInfo;
	ULONG AdapterInfoLength = sizeof(IP_ADAPTER_INFO) * 16;

	while (1) {
		AdapterInfo = (IP_ADAPTER_INFO *)malloc(AdapterInfoLength);
		if (!AdapterInfo) {
			return -1;
		}

		ULONG LengthNeeded = AdapterInfoLength;
		DWORD Ret = GetAdaptersInfo(AdapterInfo, &LengthNeeded);
		if (Ret == NO_ERROR) {
			break;
		}

		free(AdapterInfo);

		if (Ret != ERROR_BUFFER_OVERFLOW) {
			return -1;
		}
		if (AdapterInfoLength >= LengthNeeded) {
			return -1;
		}

		AdapterInfoLength = LengthNeeded;
	}

	int count = 0;
	PIP_ADAPTER_INFO Adapter = AdapterInfo;
	while (Adapter) {
		IP_ADDR_STRING *IPAddr = &Adapter->IpAddressList;
		while (IPAddr) {
			uint32_t ip_addr = ntohl(inet_addr(IPAddr->IpAddress.String));
			uint32_t subnet_mask = ntohl(inet_addr(IPAddr->IpMask.String));

			if (ip_addr == 0) {
				IPAddr = IPAddr->Next;
				continue;
			}

			struct hdhomerun_local_ip_info_t *ip_info = &ip_info_list[count++];
			ip_info->ip_addr = ip_addr;
			ip_info->subnet_mask = subnet_mask;

			if (count >= max_count) {
				break;
			}

			IPAddr = IPAddr->Next;
		}

		if (count >= max_count) {
			break;
		}

		Adapter = Adapter->Next;
	}

	free(AdapterInfo);
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
	unsigned long mode = 1;
	if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
		closesocket(sock);
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
	unsigned long mode = 1;
	if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
		closesocket(sock);
		return HDHOMERUN_SOCK_INVALID;
	}

	/* Success. */
	return sock;
}

void hdhomerun_sock_destroy(hdhomerun_sock_t sock)
{
	closesocket(sock);
}

int hdhomerun_sock_getlasterror(void)
{
	return WSAGetLastError();
}

uint32_t hdhomerun_sock_getsockname_addr(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	int sockaddr_size = sizeof(sock_addr);

	if (getsockname(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohl(sock_addr.sin_addr.s_addr);
}

uint16_t hdhomerun_sock_getsockname_port(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	int sockaddr_size = sizeof(sock_addr);

	if (getsockname(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return 0;
	}

	return ntohs(sock_addr.sin_port);
}

uint32_t hdhomerun_sock_getpeername_addr(hdhomerun_sock_t sock)
{
	struct sockaddr_in sock_addr;
	int sockaddr_size = sizeof(sock_addr);

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

bool_t hdhomerun_sock_connect(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, uint64_t timeout)
{
	WSAEVENT wsa_event = WSACreateEvent();
	if (wsa_event == WSA_INVALID_EVENT) {
		return FALSE;
	}

	if (WSAEventSelect(sock, wsa_event, FD_CONNECT) == SOCKET_ERROR) {
		WSACloseEvent(wsa_event);
		return FALSE;
	}

	/* Connect (non-blocking). */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(remote_addr);
	sock_addr.sin_port = htons(remote_port);

	if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			WSACloseEvent(wsa_event);
			return FALSE;
		}
	}

	/* Wait for connect to complete (both success and failure will signal). */
	DWORD ret = WaitForSingleObjectEx(wsa_event, (DWORD)timeout, FALSE);
	WSACloseEvent(wsa_event);

	if (ret != WAIT_OBJECT_0) {
		return FALSE;
	}

	/* Detect success/failure. */
	int sockaddr_size = sizeof(sock_addr);
	if (getpeername(sock, (struct sockaddr *)&sock_addr, &sockaddr_size) != 0) {
		return FALSE;
	}

	return TRUE;
}

static bool_t hdhomerun_sock_wait_for_event(hdhomerun_sock_t sock, long event_type, uint64_t stop_time)
{
	uint64_t current_time = getcurrenttime();
	if (current_time >= stop_time) {
		return FALSE;
	}

	WSAEVENT wsa_event = WSACreateEvent();
	if (wsa_event == WSA_INVALID_EVENT) {
		return FALSE;
	}

	if (WSAEventSelect(sock, wsa_event, event_type) == SOCKET_ERROR) {
		WSACloseEvent(wsa_event);
		return FALSE;
	}

	DWORD ret = WaitForSingleObjectEx(wsa_event, (DWORD)(stop_time - current_time), FALSE);
	WSACloseEvent(wsa_event);

	if (ret != WAIT_OBJECT_0) {
		return FALSE;
	}

	return TRUE;
}

bool_t hdhomerun_sock_send(hdhomerun_sock_t sock, const void *data, size_t length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;
	const uint8_t *ptr = (uint8_t *)data;

	while (1) {
		int ret = send(sock, (char *)ptr, (int)length, 0);
		if (ret >= (int)length) {
			return TRUE;
		}

		if (ret > 0) {
			ptr += ret;
			length -= ret;
		}

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			return FALSE;
		}

		if (!hdhomerun_sock_wait_for_event(sock, FD_WRITE | FD_CLOSE, stop_time)) {
			return FALSE;
		}
	}
}

bool_t hdhomerun_sock_sendto(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, const void *data, size_t length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;
	const uint8_t *ptr = (uint8_t *)data;

	while (1) {
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = htonl(remote_addr);
		sock_addr.sin_port = htons(remote_port);

		int ret = sendto(sock, (char *)ptr, (int)length, 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
		if (ret >= (int)length) {
			return TRUE;
		}

		if (ret > 0) {
			ptr += ret;
			length -= ret;
		}

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			return FALSE;
		}

		if (!hdhomerun_sock_wait_for_event(sock, FD_WRITE | FD_CLOSE, stop_time)) {
			return FALSE;
		}
	}
}

bool_t hdhomerun_sock_recv(hdhomerun_sock_t sock, void *data, size_t *length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;

	while (1) {
		int ret = recv(sock, (char *)data, (int)(*length), 0);
		if (ret > 0) {
			*length = ret;
			return TRUE;
		}

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			return FALSE;
		}

		if (!hdhomerun_sock_wait_for_event(sock, FD_READ | FD_CLOSE, stop_time)) {
			return FALSE;
		}
	}
}

bool_t hdhomerun_sock_recvfrom(hdhomerun_sock_t sock, uint32_t *remote_addr, uint16_t *remote_port, void *data, size_t *length, uint64_t timeout)
{
	uint64_t stop_time = getcurrenttime() + timeout;

	while (1) {
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(sock_addr));
		int sockaddr_size = sizeof(sock_addr);

		int ret = recvfrom(sock, (char *)data, (int)(*length), 0, (struct sockaddr *)&sock_addr, &sockaddr_size);
		if (ret > 0) {
			*remote_addr = ntohl(sock_addr.sin_addr.s_addr);
			*remote_port = ntohs(sock_addr.sin_port);
			*length = ret;
			return TRUE;
		}

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			return FALSE;
		}

		if (!hdhomerun_sock_wait_for_event(sock, FD_READ | FD_CLOSE, stop_time)) {
			return FALSE;
		}
	}
}
