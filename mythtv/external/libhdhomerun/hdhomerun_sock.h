/*
 * hdhomerun_sock.h
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
#ifdef __cplusplus
extern "C" {
#endif

struct hdhomerun_local_ip_info_t {
	uint32_t ip_addr;
	uint32_t subnet_mask;
};

extern LIBTYPE int hdhomerun_local_ip_info(struct hdhomerun_local_ip_info_t ip_info_list[], int max_count);

#define HDHOMERUN_SOCK_INVALID -1

typedef int hdhomerun_sock_t;

extern LIBTYPE hdhomerun_sock_t hdhomerun_sock_create_udp(void);
extern LIBTYPE hdhomerun_sock_t hdhomerun_sock_create_tcp(void);
extern LIBTYPE void hdhomerun_sock_destroy(hdhomerun_sock_t sock);

extern LIBTYPE int hdhomerun_sock_getlasterror(void);
extern LIBTYPE uint32_t hdhomerun_sock_getsockname_addr(hdhomerun_sock_t sock);
extern LIBTYPE uint16_t hdhomerun_sock_getsockname_port(hdhomerun_sock_t sock);
extern LIBTYPE uint32_t hdhomerun_sock_getpeername_addr(hdhomerun_sock_t sock);
extern LIBTYPE uint32_t hdhomerun_sock_getaddrinfo_addr(hdhomerun_sock_t sock, const char *name);

extern LIBTYPE bool_t hdhomerun_sock_join_multicast_group(hdhomerun_sock_t sock, uint32_t multicast_ip, uint32_t local_ip);
extern LIBTYPE bool_t hdhomerun_sock_leave_multicast_group(hdhomerun_sock_t sock, uint32_t multicast_ip, uint32_t local_ip);

extern LIBTYPE bool_t hdhomerun_sock_bind(hdhomerun_sock_t sock, uint32_t local_addr, uint16_t local_port, bool_t allow_reuse);
extern LIBTYPE bool_t hdhomerun_sock_connect(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, uint64_t timeout);

extern LIBTYPE bool_t hdhomerun_sock_send(hdhomerun_sock_t sock, const void *data, size_t length, uint64_t timeout);
extern LIBTYPE bool_t hdhomerun_sock_sendto(hdhomerun_sock_t sock, uint32_t remote_addr, uint16_t remote_port, const void *data, size_t length, uint64_t timeout);

extern LIBTYPE bool_t hdhomerun_sock_recv(hdhomerun_sock_t sock, void *data, size_t *length, uint64_t timeout);
extern LIBTYPE bool_t hdhomerun_sock_recvfrom(hdhomerun_sock_t sock, uint32_t *remote_addr, uint16_t *remote_port, void *data, size_t *length, uint64_t timeout);

#ifdef __cplusplus
}
#endif
