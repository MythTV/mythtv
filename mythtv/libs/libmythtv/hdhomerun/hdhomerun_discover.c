/*
 * hdhomerun_discover.c
 *
 * Copyright © 2006-2007 Silicondust Engineering Ltd. <www.silicondust.com>.
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
 */

#include "hdhomerun.h"

#if defined(__CYGWIN__) || defined(__WINDOWS__)
#include <windows.h>
#include <iphlpapi.h>
#define USE_IPHLPAPI 1
#endif

#include <sys/param.h>  // Defines BSD on FreeBSD and Mac OS X

#if defined(__linux__) || defined(__APPLE__) || defined(BSD)
#include <ifaddrs.h>
#define USE_IFADDRS 1
#include <sys/select.h>
#endif

#define HDHOMERUN_DISOCVER_MAX_SOCK_COUNT 16

struct hdhomerun_discover_sock_t {
	int sock;
	uint32_t broadcast_ip;
};

struct hdhomerun_discover_t {
	struct hdhomerun_discover_sock_t socks[HDHOMERUN_DISOCVER_MAX_SOCK_COUNT];
	unsigned int sock_count;
	struct hdhomerun_pkt_t tx_pkt;
	struct hdhomerun_pkt_t rx_pkt;
};

static void hdhomerun_discover_sock_create(struct hdhomerun_discover_t *ds, uint32_t local_ip, uint32_t mask)
{
	if (ds->sock_count >= HDHOMERUN_DISOCVER_MAX_SOCK_COUNT) {
		return;
	}

	/* Create socket. */
	int sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		return;
	}

	/* Set timeouts. */
	setsocktimeout(sock, SOL_SOCKET, SO_SNDTIMEO, 1000);
	setsocktimeout(sock, SOL_SOCKET, SO_RCVTIMEO, 1000);

	/* Allow broadcast. */
	int sock_opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&sock_opt, sizeof(sock_opt));

	/* Bind socket. */
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(local_ip);
	sock_addr.sin_port = htons(0);
	if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		close(sock);
		return;
	}

	/* Write sock entry. */
	struct hdhomerun_discover_sock_t *dss = &ds->socks[ds->sock_count++];
	dss->sock = sock;
	dss->broadcast_ip = local_ip | ~mask;
}

#if defined(USE_IPHLPAPI)
static void hdhomerun_discover_sock_detect(struct hdhomerun_discover_t *ds)
{
	PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
	ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

	DWORD Ret = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
	if (Ret != NO_ERROR) {
		free(pAdapterInfo);
		if (Ret != ERROR_BUFFER_OVERFLOW) {
			return;
		}
		pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen); 
		Ret = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
		if (Ret != NO_ERROR) {
			free(pAdapterInfo);
			return;
		}
	}

	PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
	while (pAdapter) {
		IP_ADDR_STRING *pIPAddr = &pAdapter->IpAddressList;
		while (pIPAddr) {
			uint32_t local_ip = ntohl(inet_addr(pIPAddr->IpAddress.String));
			uint32_t mask = ntohl(inet_addr(pIPAddr->IpMask.String));

			if (local_ip == 0) {
				pIPAddr = pIPAddr->Next;
				continue;
			}

			hdhomerun_discover_sock_create(ds, local_ip, mask);
			pIPAddr = pIPAddr->Next;
		}

		pAdapter = pAdapter->Next;
	}

	free(pAdapterInfo);
}
#endif

#if defined(USE_IFADDRS)
static void hdhomerun_discover_sock_detect(struct hdhomerun_discover_t *ds)
{
	struct ifaddrs *ifap;
	if (getifaddrs(&ifap) < 0) {
		return;
	}

	struct ifaddrs *p = ifap;
	while (p) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)p->ifa_addr;
		struct sockaddr_in *mask_in = (struct sockaddr_in *)p->ifa_netmask;
		if (!addr_in || !mask_in) {
			p = p->ifa_next;
			continue;
		}

		uint32_t local_ip = ntohl(addr_in->sin_addr.s_addr);
		uint32_t mask = ntohl(mask_in->sin_addr.s_addr);
		if (local_ip == 0) {
			p = p->ifa_next;
			continue;
		}

		hdhomerun_discover_sock_create(ds, local_ip, mask);
		p = p->ifa_next;
	}

	freeifaddrs(ifap);
}
#endif


#if !defined(USE_IPHLPAPI) && !defined(USE_IFADDRS)
static void hdhomerun_discover_sock_detect(struct hdhomerun_discover_t *ds)
{
	/* do nothing */
}
#endif

static struct hdhomerun_discover_t *hdhomerun_discover_create(void)
{
	struct hdhomerun_discover_t *ds = (struct hdhomerun_discover_t *)calloc(1, sizeof(struct hdhomerun_discover_t));
	if (!ds) {
		return NULL;
	}

	/* Detect & create sockets. */
	hdhomerun_discover_sock_detect(ds);
	if (ds->sock_count == 0) {
		hdhomerun_discover_sock_create(ds, 0, 0);
		if (ds->sock_count == 0) {
			free(ds);
			return NULL;
		}
	}

	/* Success. */
	return ds;
}

static void hdhomerun_discover_destroy(struct hdhomerun_discover_t *ds)
{
	unsigned int i;
	for (i = 0; i < ds->sock_count; i++) {
		struct hdhomerun_discover_sock_t *dss = &ds->socks[i];
		close(dss->sock);
	}

	free(ds);
}

static bool_t hdhomerun_discover_send_internal(struct hdhomerun_discover_t *ds, struct hdhomerun_discover_sock_t *dss, uint32_t target_ip, uint32_t device_type, uint32_t device_id)
{
	if (target_ip == 0) {
		target_ip = dss->broadcast_ip;
	}

	struct hdhomerun_pkt_t *tx_pkt = &ds->tx_pkt;
	hdhomerun_pkt_reset(tx_pkt);

	hdhomerun_pkt_write_u8(tx_pkt, HDHOMERUN_TAG_DEVICE_TYPE);
	hdhomerun_pkt_write_var_length(tx_pkt, 4);
	hdhomerun_pkt_write_u32(tx_pkt, device_type);
	hdhomerun_pkt_write_u8(tx_pkt, HDHOMERUN_TAG_DEVICE_ID);
	hdhomerun_pkt_write_var_length(tx_pkt, 4);
	hdhomerun_pkt_write_u32(tx_pkt, device_id);
	hdhomerun_pkt_seal_frame(tx_pkt, HDHOMERUN_TYPE_DISCOVER_REQ);

	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(target_ip);
	sock_addr.sin_port = htons(HDHOMERUN_DISCOVER_UDP_PORT);

	int length = (int)(tx_pkt->end - tx_pkt->start);
	if (sendto(dss->sock, (char *)tx_pkt->start, length, 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != length) {
		return FALSE;
	}

	return TRUE;
}

static bool_t hdhomerun_discover_send(struct hdhomerun_discover_t *ds, uint32_t target_ip, uint32_t device_type, uint32_t device_id)
{
	bool_t result = FALSE;

	unsigned int i;
	for (i = 0; i < ds->sock_count; i++) {
		struct hdhomerun_discover_sock_t *dss = &ds->socks[i];
		result |= hdhomerun_discover_send_internal(ds, dss, target_ip, device_type, device_id);
	}

	return result;
}

static int hdhomerun_discover_recv_internal(struct hdhomerun_discover_t *ds, struct hdhomerun_discover_sock_t *dss, struct hdhomerun_discover_device_t *result)
{
	struct hdhomerun_pkt_t *rx_pkt = &ds->rx_pkt;
	hdhomerun_pkt_reset(rx_pkt);

	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	socklen_t sockaddr_size = sizeof(sock_addr);

	int rx_length = recvfrom(dss->sock, (char *)rx_pkt->end, (int)(rx_pkt->limit - rx_pkt->end), 0, (struct sockaddr *)&sock_addr, &sockaddr_size);
	if (rx_length <= 0) {
		/* Don't return error - windows machine on VPN can sometimes cause a sock error here but otherwise works. */
		return 0;
	}
	rx_pkt->end += rx_length;

	uint16_t type;
	if (hdhomerun_pkt_open_frame(rx_pkt, &type) <= 0) {
		return 0;
	}
	if (type != HDHOMERUN_TYPE_DISCOVER_RPY) {
		return 0;
	}

	result->ip_addr = ntohl(sock_addr.sin_addr.s_addr);
	result->device_type = 0;
	result->device_id = 0;

	while (1) {
		uint8_t tag;
		size_t len;
		uint8_t *next = hdhomerun_pkt_read_tlv(rx_pkt, &tag, &len);
		if (!next) {
			break;
		}

		switch (tag) {
		case HDHOMERUN_TAG_DEVICE_TYPE:
			if (len != 4) {
				break;
			}
			result->device_type = hdhomerun_pkt_read_u32(rx_pkt);
			break;

		case HDHOMERUN_TAG_DEVICE_ID:
			if (len != 4) {
				break;
			}
			result->device_id = hdhomerun_pkt_read_u32(rx_pkt);
			break;

		default:
			break;
		}

		rx_pkt->pos = next;
	}

	return 1;
}

static int hdhomerun_discover_recv(struct hdhomerun_discover_t *ds, struct hdhomerun_discover_device_t *result)
{
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 250000;

	fd_set readfds;
	FD_ZERO(&readfds);
	int max_sock = -1;

	unsigned int i;
	for (i = 0; i < ds->sock_count; i++) {
		struct hdhomerun_discover_sock_t *dss = &ds->socks[i];
		FD_SET(dss->sock, &readfds);
		if (dss->sock > max_sock) {
			max_sock = dss->sock;
		}
	}

	if (select(max_sock+1, &readfds, NULL, NULL, &t) < 0) {
		return -1;
	}

	for (i = 0; i < ds->sock_count; i++) {
		struct hdhomerun_discover_sock_t *dss = &ds->socks[i];
		if (!FD_ISSET(dss->sock, &readfds)) {
			continue;
		}

		if (hdhomerun_discover_recv_internal(ds, dss, result) <= 0) {
			continue;
		}

		return 1;
	}

	return 0;
}

static struct hdhomerun_discover_device_t *hdhomerun_discover_find_in_list(struct hdhomerun_discover_device_t result_list[], int count, uint32_t ip_addr)
{
	int index;
	for (index = 0; index < count; index++) {
		struct hdhomerun_discover_device_t *result = &result_list[index];
		if (result->ip_addr == ip_addr) {
			return result;
		}
	}

	return NULL;
}

static int hdhomerun_discover_find_devices_internal(struct hdhomerun_discover_t *ds, uint32_t target_ip, uint32_t device_type, uint32_t device_id, struct hdhomerun_discover_device_t result_list[], int max_count)
{
	int count = 0;
	int attempt;
	for (attempt = 0; attempt < 4; attempt++) {
		if (!hdhomerun_discover_send(ds, target_ip, device_type, device_id)) {
			return -1;
		}

		uint64_t timeout = getcurrenttime() + 250;
		while (getcurrenttime() < timeout) {
			struct hdhomerun_discover_device_t *result = &result_list[count];

			int ret = hdhomerun_discover_recv(ds, result);
			if (ret < 0) {
				return -1;
			}
			if (ret == 0) {
				continue;
			}

			/* Filter. */
			if (device_type != HDHOMERUN_DEVICE_TYPE_WILDCARD) {
				if (device_type != result->device_type) {
					continue;
				}
			}
			if (device_id != HDHOMERUN_DEVICE_ID_WILDCARD) {
				if (device_id != result->device_id) {
					continue;
				}
			}

			/* Ensure not already in list. */
			if (hdhomerun_discover_find_in_list(result_list, count, result->ip_addr)) {
				continue;
			}

			/* Add to list. */
			count++;
			if (count >= max_count) {
				return count;
			}
		}
	}

	return count;
}

int hdhomerun_discover_find_devices_custom(uint32_t target_ip, uint32_t device_type, uint32_t device_id, struct hdhomerun_discover_device_t result_list[], int max_count)
{
	struct hdhomerun_discover_t *ds = hdhomerun_discover_create();
	if (!ds) {
		return -1;
	}

	int ret = hdhomerun_discover_find_devices_internal(ds, target_ip, device_type, device_id, result_list, max_count);

	hdhomerun_discover_destroy(ds);
	return ret;
}

bool_t hdhomerun_discover_validate_device_id(uint32_t device_id)
{
	static uint32_t lookup_table[16] = {0xA, 0x5, 0xF, 0x6, 0x7, 0xC, 0x1, 0xB, 0x9, 0x2, 0x8, 0xD, 0x4, 0x3, 0xE, 0x0};

	uint32_t checksum = 0;

	checksum ^= lookup_table[(device_id >> 28) & 0x0F];
	checksum ^= (device_id >> 24) & 0x0F;
	checksum ^= lookup_table[(device_id >> 20) & 0x0F];
	checksum ^= (device_id >> 16) & 0x0F;
	checksum ^= lookup_table[(device_id >> 12) & 0x0F];
	checksum ^= (device_id >> 8) & 0x0F;
	checksum ^= lookup_table[(device_id >> 4) & 0x0F];
	checksum ^= (device_id >> 0) & 0x0F;

	return (checksum == 0);
}

